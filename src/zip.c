/*** RST
zip
===

.. lua:module:: zip

.. code-block:: lua

    local zip = require 'zip'

The :lua:mod:`zip` module provides basic read access to zip files.

.. important::
    ZIP64 files are not yet supported along with any compression methods other
    than DEFLATE.

Functions
---------

*/

#include "zip.h"
#include "lua-manager.h"
#include "logging/logger.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <lauxlib.h>
#include <windows.h>
#include <zlib.h>

typedef struct zip_cdfh_t {
    uint16_t version_by;
    uint16_t version_extract;
    uint16_t gp_bits;
    uint16_t compression_method;
    uint16_t file_mod_time;
    uint16_t file_mod_date;
    uint32_t file_crc;
    uint32_t file_compressed_size;
    uint32_t file_uncompressed_size;
    uint16_t file_name_len;
    uint16_t extra_fld_len;
    uint16_t comment_len;
    uint16_t disk_num;
    uint16_t int_attrs;
    uint32_t ext_attrs;
    uint32_t file_offset;
    char *file_name;
} zip_cdfh_t;

typedef struct zip_file_header_t {
    zip_cdfh_t cdfh;
    struct zip_file_header_t *next;
} zip_file_header_t;

typedef struct zip_t {
    FILE *fstream;
    zip_file_header_t *files;

    int ref_count;
} zip_t;

int zip_lua_open_module(lua_State *L);

void zip_lua_init() {
    lua_manager_add_module_opener("zip", &zip_lua_open_module);
}

zip_t *zip_open(const char *path);
void zip_free(zip_t *zip);

void zip_ref(zip_t *zip);
void zip_unref(zip_t *zip);

int zip_find_central_directory(FILE *zip, uint32_t *cd_offset, uint32_t *cd_size);
int zip_read_central_directory_file_header(FILE *zip, zip_cdfh_t *cdfh);

zip_t *zip_open(const char *path) {
    logger_t *log = logger_get("zip");    
    FILE *f = fopen(path, "rb");

    if (!f) {
        logger_error(log, "Couldn't open %s.", path);
        return NULL;
    }

    uint32_t cd_offset = 0;
    uint32_t cd_size = 0;
    if (zip_find_central_directory(f, &cd_offset, &cd_size)) {
        logger_error(log, "%s : Couldn't find central directory (this probably isn't a zip file).", path);
        fclose(f);
        return NULL;
    }

    zip_t *zip = egoverlay_calloc(1, sizeof(zip_t));
    zip->fstream = f;

    // start reading the central directory file entries
    if (fseek(f, cd_offset, SEEK_SET)) {
        fclose(f);
        logger_error(log, "%s : Couldn't seek to central directory file entries.", path);
        egoverlay_free(zip);
        return NULL;
    }

    size_t cd_read_size = 0;
    zip_file_header_t *prev = NULL;
    while (cd_read_size < cd_size) {
        zip_file_header_t *fh = egoverlay_calloc(1, sizeof(zip_file_header_t));

        size_t cdfh_size = zip_read_central_directory_file_header(f, &fh->cdfh);
        if (cdfh_size<0) {
            fclose(f);
            logger_error(log, "%s : Couldn't read central directory file header.", path);
            egoverlay_free(fh);
            zip_free(zip);
            return NULL;
        }
        cd_read_size += cdfh_size;

        _strlwr_s(fh->cdfh.file_name, fh->cdfh.file_name_len + 1);

        if (prev==NULL) zip->files = fh;
        else prev->next = fh;
        prev = fh;
    }

    zip_ref(zip);

    // logger_debug(log, "Created 0x%x (%s)", zip, path);

    return zip;
}

void zip_free(zip_t *zip) {
    // logger_t *log = logger_get("zip");
    // logger_debug(log, "Freeing 0x%x", zip);

    zip_file_header_t *h = zip->files;
    zip_file_header_t *next = NULL;
    while (h) {
        next = h->next;
        egoverlay_free(h->cdfh.file_name);
        egoverlay_free(h);
        h = next;
    }
    fclose(zip->fstream);
    egoverlay_free(zip);
}

void zip_ref(zip_t *zip) {
    zip->ref_count++;
}

void zip_unref(zip_t *zip) {
    zip->ref_count--;
    if (zip->ref_count==0) zip_free(zip);
}

void lua_pushzip(lua_State *L, zip_t *zip);


int zip_lua_open(lua_State *L);

int zip_lua_del(lua_State *L);

int zip_lua_files(lua_State *L);
int zip_lua_file_content(lua_State *L);

int zip_lua_open_module(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, &zip_lua_open);
    lua_setfield(L, -2, "open");

    //luaL_setfuncs(L, zip_funcs, 0);

    return 1;
}

char *read_string(FILE *zip, size_t len) {
    char *str = egoverlay_malloc(sizeof(char) * (len + 1));
    _fread_nolock(str, sizeof(char), len, zip);
    str[len] = 0;

    return str;
}

uint32_t read_uint32(FILE *zip) {
    uint32_t val = 0;
    _fread_nolock(&val, sizeof(uint32_t), 1, zip);

    return val;
}

uint16_t read_uint16(FILE *zip) {
    uint16_t val = 0;
    _fread_nolock(&val, sizeof(uint16_t), 1, zip);
    return val;
}

int zip_find_central_directory(FILE *zip, uint32_t *cd_offset, uint32_t *cd_size) {
    long eocd_start = -22;

    fseek(zip, 0, SEEK_END);
    size_t file_size = ftell(zip);

    // start looking for the end of central directory record
    fseek(zip, eocd_start, SEEK_END);

    // start at 22 bytes from the end and look for the EOCD signature
    uint32_t eocd_sig = 0;
    while ((eocd_start * -1) < (file_size -22) && (eocd_sig=read_uint32(zip))!=0x06054b50) {
        eocd_start--;
        fseek(zip, eocd_start - 4, SEEK_END); // -4 because we read 4 bytes for the sigature each time
    }

    if ((eocd_start * -1) >= (file_size - 22)) {
        // couldn't find EOCD, probably not a zip file. or it's corrupted.
        fclose(zip);
        return -1;
    }

    // these should be 0, unless someone tries to use part of an archive originally stored on floppy disks
    uint16_t cd_disk_num = read_uint16(zip);
    UNUSED_PARAM(cd_disk_num);
    
    uint16_t cd_start_disk = read_uint16(zip);
    uint16_t cd_disk_records = read_uint16(zip);


    uint16_t cd_records = read_uint16(zip);
    *cd_size = read_uint32(zip);
    *cd_offset = read_uint32(zip);

    if (cd_start_disk==0xFFFF || cd_disk_records==0xFFFF || cd_records==0xFFFFFFFF || *cd_size==0xFFFFFFFF || *cd_offset==0xFFFFFFFF) {
        abort();
    }

    return 0;
}

int zip_read_central_directory_file_header(FILE *zip, zip_cdfh_t *cdfh) {
    int cd_read_size = 0;
    uint32_t cdfh_sig = read_uint32(zip); cd_read_size += 4;

    if (cdfh_sig!=0x02014b50) {
        return -1;
    }

    cdfh->version_by             = read_uint16(zip); cd_read_size += 2;
    cdfh->version_extract        = read_uint16(zip); cd_read_size += 2;
    cdfh->gp_bits                = read_uint16(zip); cd_read_size += 2;
    cdfh->compression_method     = read_uint16(zip); cd_read_size += 2;
    cdfh->file_mod_time          = read_uint16(zip); cd_read_size += 2;
    cdfh->file_mod_date          = read_uint16(zip); cd_read_size += 2;
    cdfh->file_crc               = read_uint32(zip); cd_read_size += 4;
    cdfh->file_compressed_size   = read_uint32(zip); cd_read_size += 4;
    cdfh->file_uncompressed_size = read_uint32(zip); cd_read_size += 4;
    cdfh->file_name_len          = read_uint16(zip); cd_read_size += 2;
    cdfh->extra_fld_len          = read_uint16(zip); cd_read_size += 2;
    cdfh->comment_len            = read_uint16(zip); cd_read_size += 2;
    cdfh->disk_num               = read_uint16(zip); cd_read_size += 2;
    cdfh->int_attrs              = read_uint16(zip); cd_read_size += 2;
    cdfh->ext_attrs              = read_uint32(zip); cd_read_size += 4;
    cdfh->file_offset            = read_uint32(zip); cd_read_size += 4;

    cdfh->file_name = read_string(zip, cdfh->file_name_len);
    cd_read_size += cdfh->file_name_len;

    _fseek_nolock(zip, cdfh->extra_fld_len, SEEK_CUR); // extra field
    cd_read_size += cdfh->extra_fld_len;
    _fseek_nolock(zip, cdfh->comment_len, SEEK_CUR); // comment
    cd_read_size += cdfh->comment_len;

    return cd_read_size;
}

/*** RST
.. lua:function:: open(path)

    Load a zip file into a :lua:class:`zip`.

    :param string path:
    :rtype: zip

    .. versionhistory::
        :0.0.1: Added
*/
int zip_lua_open(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    zip_t *zip = zip_open(path);

    if (!zip) return 0;

    lua_pushzip(L, zip);
    zip_unref(zip);

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: zip
*/
static luaL_Reg zip_funcs[] = {
    "__gc",         &zip_lua_del,
    "files",        &zip_lua_files,
    "file_content", &zip_lua_file_content,
    NULL, NULL
};

void lua_pushzip(lua_State *L, zip_t *zip) {
    zip_ref(zip);
    zip_t **pzip = (zip_t**)lua_newuserdata(L, sizeof(zip_t*));
    *pzip = zip;

    if (luaL_newmetatable(L, "ZipMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, zip_funcs, 0);
    }
    lua_setmetatable(L, -2);
}

#define LUA_CHECK_ZIP(L, ind) *(zip_t**)luaL_checkudata(L, ind, "ZipMetaTable")

int zip_lua_del(lua_State *L) {
    zip_t *zip = LUA_CHECK_ZIP(L, 1);

    zip_unref(zip);

    return 0;
}

/*** RST
    .. lua:method:: files()

        Return a list of files in this zip. Directory names will not be returned
        as a separate string, but all files inside of directories will.

        :rtype: table

        .. versionhistory::
            :0.0.1: Added
*/
int zip_lua_files(lua_State *L) {
    zip_t *zip = LUA_CHECK_ZIP(L, 1);

    lua_newtable(L);

    int ti = 1;
    for (zip_file_header_t *fh=zip->files;fh;fh=fh->next) {
        if (!(fh->cdfh.ext_attrs & FILE_ATTRIBUTE_DIRECTORY)) { // this is a file
            lua_pushstring(L, fh->cdfh.file_name);
            lua_seti(L, -2, ti++);
        }
    }

    return 1;
}

/*** RST
    .. lua:method:: content(path)

        Return the uncompressed content for a file in this zip. ``path`` should
        be a file path returned by :lua:meth:`files`.

        If ``path`` does not exist in this zip, this function returns ``nil``

        :param string path:
        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int zip_lua_file_content(lua_State *L) {
    zip_t *zip = LUA_CHECK_ZIP(L, 1);

    const char *file_path_orig = luaL_checkstring(L, 2);
    char *file_path = _strdup(file_path_orig);
    _strlwr_s(file_path, strlen(file_path)+1);

    zip_cdfh_t *cdfh = NULL;
    for (zip_file_header_t *fh=zip->files;fh;fh=fh->next) {
        if (strcmp(file_path, fh->cdfh.file_name)==0) {
            cdfh = &fh->cdfh;
            break;
        }
    }

    egoverlay_free(file_path);

    if (!cdfh) return 0;

    _fseek_nolock(zip->fstream, cdfh->file_offset, SEEK_SET);
    uint32_t file_hdr_sig = read_uint32(zip->fstream);
    if (file_hdr_sig!=0x04034b50) {
        return luaL_error(L, "Error reading local file header.");
    }

    _fseek_nolock(zip->fstream, 4, SEEK_CUR); 

    uint16_t compression = read_uint16(zip->fstream);

    _fseek_nolock(zip->fstream, 16, SEEK_CUR);
    uint16_t file_name_len = read_uint16(zip->fstream);
    uint16_t extra_len = read_uint16(zip->fstream);
    _fseek_nolock(zip->fstream, file_name_len + extra_len, SEEK_CUR);

    if (compression!=8 && compression!=0) {
        return luaL_error(L, "unsupported compression: %d", compression);
    }

    if (compression==0) { // no compression, just stored
        uint8_t *data = egoverlay_calloc(cdfh->file_compressed_size, sizeof(uint8_t));
        _fread_nolock(data, sizeof(uint8_t), cdfh->file_compressed_size, zip->fstream);

        lua_pushlstring(L, (char*)data, cdfh->file_compressed_size);
    
        egoverlay_free(data);
        return 1;
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = 0;

    // windowbits -15 = raw compression (no zlib headers)
    if (inflateInit2(&strm, -15)!=Z_OK) {
        return luaL_error(L, "Couldn't initialize zlib.");
    }

    uint8_t *compressed_data = egoverlay_calloc(cdfh->file_compressed_size, sizeof(uint8_t));
    _fread_nolock(compressed_data, sizeof(uint8_t), cdfh->file_compressed_size, zip->fstream);
    uint8_t *uncompressed_data = egoverlay_calloc(cdfh->file_uncompressed_size, sizeof(uint8_t));

    strm.avail_out = cdfh->file_uncompressed_size;
    strm.next_out = uncompressed_data;
    strm.avail_in = cdfh->file_compressed_size;
    strm.next_in = compressed_data;

    int ret = inflate(&strm, Z_FINISH); // uncompress in a single run
    if (ret != Z_STREAM_END) {
        egoverlay_free(compressed_data);
        egoverlay_free(uncompressed_data);
        return luaL_error(L, "expected stream end.");
    }

    // TODO: CRC checking
    lua_pushlstring(L, (char*)uncompressed_data, cdfh->file_uncompressed_size);
    inflateEnd(&strm);
    
    egoverlay_free(compressed_data);
    egoverlay_free(uncompressed_data);

    return 1;
}

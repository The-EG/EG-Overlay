#include "app.h"
#include "utils.h"
#include "logging/logger.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>

char *load_file(const char *path, size_t *length) {
    logger_t *log = logger_get("load_file");

    FILE *f = fopen(path, "rb");

    if (f==NULL) {
        logger_error(log, "Can't load file %s, does not exist.", path);
        *length = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);

    //logger_debug(log, "Read %d bytes from %s.", *length, path);

    char *data = egoverlay_calloc(*length, sizeof(char));
    fseek(f, 0, SEEK_SET);
    fread(data, sizeof(char), *length, f);

    fclose(f);

    return data;
}

// http://www.cse.yorku.ca/~oz/hash.html
uint32_t djb2_hash_string(const char *string) {
    uint32_t hash = 5381;
    
    int c;
    while ((c = *string++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

void error_and_exit(const char *title, const char *msg_format, ...) {
    size_t msglen;
    char *msgbuf;

    va_list args;
    va_start(args, msg_format);
    msglen = vsnprintf(NULL, 0, msg_format, args) + 1;
    va_end(args);

    // now allocate it and do the actual format
    msgbuf = egoverlay_calloc(msglen, sizeof(char));
    
    va_start(args, msg_format);
    vsnprintf(msgbuf, msglen, msg_format, args);
    va_end(args);

    MessageBox(NULL, msgbuf, title, MB_OK | MB_ICONERROR);
    egoverlay_free(msgbuf);
    exit(-1);
}

char *wchar_to_char(const wchar_t *wstr) {
    int strsize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);

    if (strsize==0) return NULL;

    char *str = egoverlay_calloc(strsize, sizeof(char));
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, strsize, NULL, NULL);

    return str;
}

wchar_t *char_to_wchar(const char *str) {
    int wstrsize = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

    if (wstrsize==0) return NULL;

    wchar_t *wstr = egoverlay_calloc(wstrsize, sizeof(wchar_t));

    MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wstrsize);

    return wstr;
}

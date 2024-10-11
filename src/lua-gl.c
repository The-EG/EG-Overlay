#include "lua-gl.h"
#include "lua-manager.h"
#include <stb_image.h>
#include <glad/gl.h>
#include <string.h>
#include <lauxlib.h>
#include "logging/logger.h"
#include "gl.h"
#include "ui/ui.h"
#include "utils.h"

typedef struct {
    gl_shader_program_t *sprite_program;
    GLuint sprite_vert_shader;
    GLuint sprite_frag_shader;
    
    mat4f_t *view;
    mat4f_t *proj;
    int in_frame;
} overlay_3d_t;

static overlay_3d_t *overlay_3d = NULL;

int overlay_3d_lua_open_module(lua_State *L);

void overlay_3d_init() {
    overlay_3d = egoverlay_calloc(1, sizeof(overlay_3d_t));

    overlay_3d->sprite_program = gl_shader_program_new();
    gl_shader_program_attach_shader_file(
        overlay_3d->sprite_program,
        "shaders/sprite-collection.vert",
        GL_VERTEX_SHADER
    );
    gl_shader_program_attach_shader_file(
        overlay_3d->sprite_program,
        "shaders/sprite-collection.frag",
        GL_FRAGMENT_SHADER
    );
    gl_shader_program_link(overlay_3d->sprite_program);

    lua_manager_add_module_opener("eg-overlay-3d", &overlay_3d_lua_open_module);
}

void overlay_3d_cleanup() {
    gl_shader_program_free(overlay_3d->sprite_program);

    egoverlay_free(overlay_3d);
}

void overlay_3d_begin_frame(mat4f_t *view, mat4f_t *proj) {
    overlay_3d->view = view;
    overlay_3d->proj = proj;
    overlay_3d->in_frame = 1;
}

void overlay_3d_end_frame() {
    overlay_3d->view = NULL;
    overlay_3d->proj = NULL;
    overlay_3d->in_frame = 0;
}

/*** RST
eg-overlay-3d
=============

.. lua:module:: eg-overlay-3d

.. code-block:: lua

    local overlay3d = require 'eg-overlay-3d'

The :lua:mod:`eg-overlay-3d` module contains functions and classes that can be used
to draw in the 3D scene, below the UI. 

.. important::
    Objects here can be created during :overlay:event:`startup` or :overlay:event:`update`
    events, but drawing can only occur during :overlay:event:`draw-3d`.

Functions
---------
*/

int texture_lua_new(lua_State *L);
int texture_lua_del(lua_State *L);

int sprite_collection_lua_new(lua_State *L);
int sprite_collection_lua_del(lua_State *L);
int sprite_collection_lua_updatelocations(lua_State *L);
int sprite_collection_lua_draw(lua_State *L);

luaL_Reg overlay_3d_mod_funcs[] = {
    "spritecollection", &sprite_collection_lua_new,
    "texture"         , &texture_lua_new,
    NULL, NULL
};

int overlay_3d_lua_open_module(lua_State *L) {
    lua_newtable(L);

    luaL_setfuncs(L, overlay_3d_mod_funcs, 0);

    return 1;
}

typedef struct {
    GLuint texture;
    float xy_ratio;
    float max_u;
    float max_v;
} texture_t;

typedef struct {
    GLuint vao;
    GLuint vbo;

    size_t count;
    
    float size;
    ui_color_t color;

    texture_t *texture;
    int texture_cbi;
} sprite_collection_t;

typedef struct {
    float x;
    float y;
    float z;

    float size_ratio;
    
    float r;
    float g;
    float b;
    float a;

    uint32_t flags;

    mat4f_t rotation;
} sprite_collection_location_t;



#define SPRITE_COLLECTION_MT "EGOverlaySpriteCollection"

#define lua_checkspritecollection(L, ind) (sprite_collection_t*)luaL_checkudata(L, ind, SPRITE_COLLECTION_MT)

#define TEXTURE_MT "EGOverlayTexture"
#define lua_checktexture(L, ind) (texture_t*)luaL_checkudata(L, ind, TEXTURE_MT)

luaL_Reg sprite_collection_funcs[] = {
    "__gc"           , &sprite_collection_lua_del,
    "updatelocations", &sprite_collection_lua_updatelocations,
    "draw"           , &sprite_collection_lua_draw,
    NULL, NULL
};

luaL_Reg texture_funcs[] = {
    "__gc", &texture_lua_del,
    NULL, NULL
};

/*** RST
.. lua:function:: spritecollection(texture)

    Create a new :lua:class:`o3dspritecollection` object with the given texture.

    :param o3dtexture texture:

    :rtype: o3dspritecollection

    .. versionhistory::
        :0.1.0: Added
*/
int sprite_collection_lua_new(lua_State *L) {
    sprite_collection_t *sprite = lua_newuserdata(L, sizeof(sprite_collection_t));
    memset(sprite, 0, sizeof(sprite_collection_t));

    sprite->texture = lua_checktexture(L, 1);

    lua_pushvalue(L, 1);
    sprite->texture_cbi = luaL_ref(L, LUA_REGISTRYINDEX);

    sprite->color = 0xFFFFFFFF;

    glGenVertexArrays(1, &sprite->vao);
    glGenBuffers(1, &sprite->vbo);

    glBindVertexArray(sprite->vao);
    glBindBuffer(GL_ARRAY_BUFFER, sprite->vbo);

    GLsizei loc_size = sizeof(sprite_collection_location_t);
    
    // location 0 = vec3 position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, loc_size, (void*)0);
    glVertexAttribDivisor(0, 1);

    // location 1 = float size_ratio
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, loc_size, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(1, 1);

    // location 2 = vec4 color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, loc_size, (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(2, 1);

    // location 3 = uint flags
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, loc_size, (void*)(8 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    // location4 = mat4 rotation
    // rotation matrix. this is sent as 4 vec4s
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, loc_size, (void*)(8 * sizeof(float) + sizeof(uint32_t)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, loc_size, (void*)(12 * sizeof(float) + sizeof(uint32_t)));
    glVertexAttribDivisor(5, 1);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, loc_size, (void*)(16 * sizeof(float) + sizeof(uint32_t)));
    glVertexAttribDivisor(6, 1);
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, loc_size, (void*)(20 * sizeof(float) + sizeof(uint32_t)));
    glVertexAttribDivisor(7, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);    
    
    glBindVertexArray(0);

    sprite->size = 40;

    if (luaL_newmetatable(L, SPRITE_COLLECTION_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, sprite_collection_funcs, 0);
    }
    lua_setmetatable(L, -2);

    return 1;
}

int sprite_collection_lua_del(lua_State *L) {
    sprite_collection_t *sprite = lua_checkspritecollection(L, 1);

    glDeleteBuffers(1, &sprite->vbo);
    glDeleteVertexArrays(1, &sprite->vao);
    luaL_unref(L, LUA_REGISTRYINDEX, sprite->texture_cbi);

    return 0;
}

/*** RST
.. lua:function:: texture(texturedata)

    Create a new :lua:class:`o3dtexture`.

    :param string texturedata: The binary data for the texture. This should be
        an image supported by stb_image.
    :rtype: o3dtexture

    .. versionhistory::
        :0.1.0: Added
*/
int texture_lua_new(lua_State *L) {
    texture_t *tex = lua_newuserdata(L, sizeof(texture_t));
    size_t texdatalen = 0;
    const uint8_t *texdata = (const uint8_t*)luaL_checklstring(L, 1, &texdatalen);

    if (texdatalen==0) {
        return luaL_error(L, "Texture data expected, got nil.");
    }

    memset(tex, 0, sizeof(texture_t));

    int texw = 0;
    int texh = 0;
    int texc = 0;

    uint8_t *pixels = stbi_load_from_memory(texdata, (int)texdatalen, &texw, &texh, &texc, 4);

    if (pixels==NULL) {
        return luaL_error(L, "Could not read image data.");
    }

    // work out how large the texture needs to be. we need a square image for
    // our texture, but the input may not be square. 
    int32_t texsize = 16;
    while (texsize < texw || texsize < texh) texsize <<= 1;

    // dont' allow massive textures
    if (texsize > 4096) {
        return luaL_error(L, "image too large.");
    }

    // this is how much wider the sprite will be than it is tall
    // 1.0 for a square
    tex->xy_ratio = (float)texw / (float)texh;

    // how much of the square texture isn't used
    tex->max_u = (float)texw / (float)texsize;
    tex->max_v = (float)texh / (float)texsize;

    // create the pixels for the square texture and copy the image data into it
    // leaving space on either the right or bottom needed to make it square
    uint8_t *tex_pixels = egoverlay_calloc((texsize * texsize * 4), sizeof(uint8_t));
    for (size_t r=0;r<texh;r++) {
        size_t tex_row_offset = r * texsize * 4;
        size_t img_row_offset = r * texw    * 4;
        memcpy(tex_pixels + tex_row_offset, pixels + img_row_offset, texw * 4);
    }

    glGenTextures(1, &tex->texture);
    glBindTexture(GL_TEXTURE_2D, tex->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texsize, texsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    egoverlay_free(tex_pixels);
    stbi_image_free(pixels);

    if (luaL_newmetatable(L, TEXTURE_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, texture_funcs, 0);
    }
    lua_setmetatable(L, -2);

    return 1;
}

int texture_lua_del(lua_State *L) {
    texture_t *tex = lua_checktexture(L, 1);   
    glDeleteTextures(1, &tex->texture);

    return 0;
}


/*** RST
Classes
-------

.. lua:class:: o3dtexture

    A texture that can be used with other :lua:mod:`eg-overlay-3d` elements.

.. lua:class:: o3dspritecollection

    A sprite collection can be used to efficiently draw instances of the same
    texture at multiple locations, all facing the camera. This is useful for
    showing billboards or markers that all share the same options except for
    location.

    .. code-block:: lua
        :caption: Example

        local overlay = require 'eg-overlay'
        local o3d = require 'overlay-3d'

        local texture = io.open('textures/eg-overlay-32x32.png', 'rb')

        local texdata = texture:read('a')

        local tex = o3d.texture(texdata)

        local sprite = o3d.spritecollection(tex)

        sprite:updatelocations({
            {x=0, y=0, z= 0},
            {x=50, y=1.5, z=0, sizeratio=0.5},
            {x=-30, y=0, z=100*12}
        })

        local function drawsprites()
            sprite:draw() 
        end

        overlay.add_event_handler('draw-3d', drawsprites)
*/

/*** RST
    .. lua:method:: updatelocations([locations])

        Update the locations this sprite is drawn at with location specific
        attributes.

        The locations are *set* to the given locations, any previous locations
        are discarded.

        ``locations`` must be a sequence of tables, one per location.

        Each location table can have the following fields:

        ========== ============================================================
        Field      Description
        ========== ============================================================
        x          The location x coordinate. In map units. Default ``0``.
        y          The location y coordinate. In map units. Default ``0``.
        z          The location z coordinate. In map units. Default ``0``.
        sizeratio  A size factor controlling how large/small this location will
                   be drawn compared to the size set on this spritecollection.
        color      Tint color and opacity, see :ref:`colors`.
        billboard  A boolean indicating if the texture should always face the
                   camera. Default: ``true``
        centerfade A boolean indicating if the sprite is displayed more
                   transparent near the center of the screen. Default: ``true``
        rotation   A sequence of 3 numbers indicating the rotation to be
                   applied to the sprite along the X, Y, and Z axes in that
                   order. Only applicable if ``billboard`` is ``false``.
        ========== ============================================================

        :param table locations: (Optional) If omitted, the locations for this
            :lua:class:`o3dspritecollection` are cleared.

        .. versionhistory::
            :0.1.0: Added
*/
int sprite_collection_lua_updatelocations(lua_State *L) {
    sprite_collection_t *sprite = lua_checkspritecollection(L, 1);

    if (lua_gettop(L)==1) {
        glBindBuffer(GL_ARRAY_BUFFER, sprite->vbo);
        glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        sprite->count = 0;

        return 0;
    }

    if (!lua_istable(L, 2)) {
        return luaL_error(L, "updateloctions(locations): locations must be a "
                             "sequence of location details.");
    }

    size_t len = luaL_len(L, 2);
    if (len==0) return luaL_error(L, "updateloctions(locations): locations is empty.");

    sprite_collection_location_t *locs = egoverlay_calloc(len, sizeof(sprite_collection_location_t));

    lua_pushnil(L);
    int li = 0;
    while (lua_next(L, 2) != 0) {
        locs[li].size_ratio = 1.f;
        locs[li].r = 1.f;
        locs[li].g = 1.f;
        locs[li].b = 1.f;
        locs[li].a = 1.f;
        locs[li].flags = 0x01 | (0x01 << 1); // billboard & centerfade

        mat4f_identity(&locs[li].rotation);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const char *key = lua_tostring(L, -2);

            if (strcmp(key, "x")==0) {
                if (lua_type(L, -1)!=LUA_TNUMBER) return luaL_error(L, "x must be a number.");
                locs[li].x = (float)lua_tonumber(L, -1);
            } else if (strcmp(key, "y")==0) {
                if (lua_type(L, -1)!=LUA_TNUMBER) return luaL_error(L, "y must be a number.");
                locs[li].y = (float)lua_tonumber(L, -1);
            } else if (strcmp(key, "z")==0) {
                if (lua_type(L, -1)!=LUA_TNUMBER) return luaL_error(L, "z must be a number.");
                locs[li].z = (float)lua_tonumber(L, -1);
            } else if (strcmp(key, "sizeratio")==0) {
                if (lua_type(L, -1)!=LUA_TNUMBER) return luaL_error(L, "sizeratio must be a number.");
                locs[li].size_ratio = (float)lua_tonumber(L, -1);
            } else if (strcmp(key, "billboard")==0) {
                if (lua_type(L, -1)!=LUA_TBOOLEAN) return luaL_error(L, "billboard must be a boolean.");
                int billboard = lua_toboolean(L, -1);
                locs[li].flags = (locs[li].flags & ~0x1) | billboard;
            } else if (strcmp(key, "color")==0) {
                if (lua_type(L, -1)!=LUA_TNUMBER || !lua_isinteger(L, -1)) {
                    return luaL_error(L, "color must be an integer.");
                }
                ui_color_t color = (ui_color_t)lua_tointeger(L, -1);
                locs[li].r = UI_COLOR_R(color);
                locs[li].g = UI_COLOR_G(color);
                locs[li].b = UI_COLOR_B(color);
                locs[li].a = UI_COLOR_A(color);
            } else if (strcmp(key, "centerfade")==0) {
                if (lua_type(L, -1)!=LUA_TBOOLEAN) return luaL_error(L, "centerfade must be a boolean.");
                int centerfade = (lua_toboolean(L, -1) << 1);
                locs[li].flags = (locs[li].flags & ~(0x01 << 1)) | centerfade;
            } else if (strcmp(key, "rotation")==0) {
                if (lua_type(L, -1)!=LUA_TTABLE || luaL_len(L, -1)!=3) {
                    return luaL_error(L, "rotation must be a sequence of 3 numbers.");
                }
                float x = 0.0;
                float y = 0.0;
                float z = 0.0;

                lua_geti(L, -1, 1);
                x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_geti(L, -1, 2);
                y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_geti(L, -1, 3);
                z = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);
                mat4f_t xr = {0};
                mat4f_t yr = {0};
                mat4f_t zr = {0};

                mat4f_t zy = {0};
                
                mat4f_rotatex(&xr, deg2rad(x));
                mat4f_rotatey(&yr, deg2rad(y));
                mat4f_rotatez(&zr, deg2rad(z));

                mat4f_mult_mat4f(&zr, &yr, &zy);
                mat4f_mult_mat4f(&zy, &xr, &locs[li].rotation);
            } else {
                return luaL_error(L, "Unknown sprite location attribute: %s", key);
            }

            lua_pop(L, 1);
        }

        li++;

        lua_pop(L, 1);
    }


    glBindBuffer(GL_ARRAY_BUFFER, sprite->vbo);
    glBufferData(GL_ARRAY_BUFFER, len * sizeof(sprite_collection_location_t), locs, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sprite->count = len;
    egoverlay_free(locs);

    return 0;
}

/*** RST
    .. lua:method:: draw()

        Draw this sprite at the positions previously set with :lua:meth:`updatelocations`.

        .. important::
            This method can only be called during :overlay:event:`draw-3d`. Attempts
            to call it at any other time will result in an error.

        .. versionhistory::
            :0.1.0: Added
*/
int sprite_collection_lua_draw(lua_State *L) {
    sprite_collection_t *sprite = lua_checkspritecollection(L, 1);

    if (!overlay_3d->in_frame) return luaL_error(L, "Not in a frame.");

    if (sprite->count==0) return 0;

    int viewport[4] = {0};
    glGetIntegerv(GL_VIEWPORT, viewport);

    glDisable(GL_CULL_FACE);

    gl_shader_program_use(overlay_3d->sprite_program);

    glUniform1f(0, sprite->size);    
    glUniform1f(1, sprite->texture->xy_ratio);
    glUniform1f(2, sprite->texture->max_u);
    glUniform1f(3, sprite->texture->max_v);
    glUniformMatrix4fv(4, 1, GL_FALSE, (GLfloat*)overlay_3d->view);
    glUniformMatrix4fv(5, 1, GL_FALSE, (GLfloat*)overlay_3d->proj);
    glUniform1f(6, (float)viewport[2]);
    glUniform1f(7, (float)viewport[3]);

    glBindVertexArray(sprite->vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sprite->texture->texture);    

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)sprite->count);

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindVertexArray(0);

    glUseProgram(0);

    glEnable(GL_CULL_FACE);

    return 0;
}


#include <windows.h>
#include <math.h>
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
#include "logging/logger.h"
#include "mumble-link.h"
#include "app.h"
#include "assert.h"

typedef struct {
    gl_shader_program_t *sprite_array_program;
    gl_shader_program_t *trail_program;

    mat4f_t *view;
    mat4f_t *proj;
    mat4f_t map_proj;
    mat4f_t map_view;

    vec3f_t player_pos;

    vec3f_t mouse_ray;
    vec3f_t camera;
    int in_frame;
    int mouse_in_overlay;
    int mapfullscreen;

    int minimapleft;
    int minimapbottom;
    int minimapwidth;
    int minimapheight;

    float mapcoordsleft;
    float mapcoordsright;
    float mapcoordstop;
    float mapcoordsbottom;
} overlay_3d_t;

static overlay_3d_t *overlay_3d = NULL;

int overlay_3d_lua_open_module(lua_State *L);

gl_shader_source_list_t sprite_array_srcs[] = {
    "shaders/sprite-array.vert", GL_VERTEX_SHADER,
    "shaders/sprite-array.frag", GL_FRAGMENT_SHADER,
    NULL                       , 0
};

gl_shader_source_list_t trail_srcs[] = {
    "shaders/trail.vert", GL_VERTEX_SHADER,
    "shaders/trail.frag", GL_FRAGMENT_SHADER,
    NULL                , 0
};

void overlay_3d_init() {
    overlay_3d = egoverlay_calloc(1, sizeof(overlay_3d_t));

    gl_add_shader_include("shaders/3dcommon.glsl", "/3dcommon.glsl");

    overlay_3d->sprite_array_program = gl_shader_program_new_with_sources(sprite_array_srcs);
    overlay_3d->trail_program        = gl_shader_program_new_with_sources(trail_srcs       );

    lua_manager_add_module_opener("eg-overlay-3d", &overlay_3d_lua_open_module);
}

void overlay_3d_cleanup() {
    gl_shader_program_free(overlay_3d->sprite_array_program);
    gl_shader_program_free(overlay_3d->trail_program);
    gl_del_shader_include("/3dcommon.glsl");

    egoverlay_free(overlay_3d);
}

void overlay_3d_calc_mouse_ray() {
    int mx = 0;
    int my = 0;
    int fbw = 0;
    int fbh = 0;

    app_get_mouse_coords(&mx, &my);
    app_get_framebuffer_size(&fbw, &fbh);

    if (mx < 0 || mx > fbw || my < 0 || my > fbh) {
        overlay_3d->mouse_in_overlay = 0;
        overlay_3d->mouse_ray.x = 0.f;
        overlay_3d->mouse_ray.y = 0.f;
        overlay_3d->mouse_ray.z = 0.f;
    }

    overlay_3d->mouse_in_overlay = 1;

    // https://antongerdelan.net/opengl/raycasting.html
    // this differs a bit since we are using D3D style LH coordinates

    vec4f_t ray_clip = {
        (2.f * mx) / fbw - 1.f,
        1.f - (2.f * my) / fbh,
        1.f,
        1.f
    };

    mat4f_t proj_inv = {0};
    mat4f_inverse(overlay_3d->proj, &proj_inv);

    vec4f_t ray_eye = {0};
    mat4f_mult_vec4f(&proj_inv, &ray_clip, &ray_eye);
    ray_eye.z = 1.0;
    ray_eye.w = 0.0;

    mat4f_t view_inv = {0};
    mat4f_inverse(overlay_3d->view, &view_inv);

    vec4f_t ray_world = {0};
    mat4f_mult_vec4f(&view_inv, &ray_eye, &ray_world);

    vec3f_t rayw3 = { ray_world.x, ray_world.y, ray_world.z };
    vec3f_normalize(&rayw3, &overlay_3d->mouse_ray);
}

void overlay_3d_begin_frame(mat4f_t *view, mat4f_t *proj) {
    overlay_3d->view = view;
    overlay_3d->proj = proj;
    overlay_3d->in_frame = 1;

    overlay_3d_calc_mouse_ray();
    mumble_link_camera_position(
        &overlay_3d->camera.x,
        &overlay_3d->camera.y,
        &overlay_3d->camera.z
    );

    mumble_link_avatar_position(
        &overlay_3d->player_pos.x,
        &overlay_3d->player_pos.y,
        &overlay_3d->player_pos.z
    );

    overlay_3d->camera.x *= 39.3701f;
    overlay_3d->camera.y *= 39.3701f;
    overlay_3d->camera.z *= 39.3701f;

    overlay_3d->player_pos.x *= 39.3701f;
    overlay_3d->player_pos.y *= 39.3701f;
    overlay_3d->player_pos.z *= 39.3701f;

    float mapcenterx = 0.f;
    float mapcentery = 0.f;

    mumble_link_map_center(&mapcenterx, &mapcentery);

    float mapscale = mumble_link_map_scale();
    uint32_t uistate = mumble_link_ui_state();

    overlay_3d->mapfullscreen = uistate & MUMBLE_LINK_UI_STATE_MAP_OPEN;

    int fbw = 0;
    int fbh = 0;
    app_get_framebuffer_size(&fbw, &fbh);

    uint16_t mapw = 0;
    uint16_t maph = 0;
    if (overlay_3d->mapfullscreen) {
        mapw = (uint16_t)fbw;
        maph = (uint16_t)fbh;
    } else {
        mumble_link_map_size(&mapw, &maph);
        float minimapscale = 1.f;
       
        switch(mumble_link_ui_size()) {
        case MUMBLE_LINK_UI_SIZE_SMALL:
            overlay_3d->minimapbottom = 33;
            minimapscale = 0.9f;
            break;
        case MUMBLE_LINK_UI_SIZE_NORMAL:
            overlay_3d->minimapbottom = 35;
            break;
        case MUMBLE_LINK_UI_SIZE_LARGE:
            overlay_3d->minimapbottom = 42;
            minimapscale = 1.11f;
            break;
        case MUMBLE_LINK_UI_SIZE_LARGER:
            overlay_3d->minimapbottom = 45;
            minimapscale = 1.225f;
            break;
        default:
            overlay_3d->minimapbottom = 35;
            break;
        }

        overlay_3d->minimapleft = (int)((float)fbw - ((float)mapw * minimapscale));
        overlay_3d->minimapwidth = (int)((float)mapw * minimapscale);
        overlay_3d->minimapheight = (int)((float)maph * minimapscale);

        if (uistate & MUMBLE_LINK_UI_STATE_COMPASS_TOP_RIGHT) {
            overlay_3d->minimapbottom = fbh - overlay_3d->minimapheight;
        }

    }

    float mapxsize = (float)mapw * mapscale;
    float mapysize = (float)maph * mapscale;

    float mapleft = -(mapxsize / 2.f);
    float mapright = mapxsize / 2.f;
    float maptop = -mapysize / 2.f;
    float mapbottom = (mapysize / 2.f);

    overlay_3d->mapcoordsleft   = mapcenterx - (mapxsize / 2.f);
    overlay_3d->mapcoordsright  = mapcenterx + (mapxsize / 2.f);
    overlay_3d->mapcoordstop    = mapcentery - (mapysize / 2.f);
    overlay_3d->mapcoordsbottom = mapcentery + (mapysize / 2.f);

    mat4f_ortho(&overlay_3d->map_proj, mapleft, mapright, maptop, mapbottom, -1.f, 1.f);

    // build the view matrix (this is a translation to the map center and a rotate
    // if the minimap is rotated
    mat4f_t view_translate = {0};
    mat4f_t view_rotate = {0};
    mat4f_translate(&view_translate, -mapcenterx, -mapcentery, 0.f);

    if (!overlay_3d->mapfullscreen && uistate & MUMBLE_LINK_UI_STATE_COMPASS_ROTATE) {
        float rotate = mumble_link_map_rotation();

        mat4f_rotatez(&view_rotate, rotate);
    } else {
        mat4f_identity(&view_rotate);
    }

    mat4f_mult_mat4f(&view_translate, &view_rotate, &overlay_3d->map_view);
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

The :lua:mod:`eg-overlay-3d` module contains functions and classes that can be
used to draw in the 3D scene, below the overlay UI. 

.. important::

    Objects here can be created during :overlay:event:`startup`,
    :overlay:event:`update`, or other events, but drawing can only occur during
    :overlay:event:`draw-3d`.

Most of the rendering details are abstracted away so all that is needed is an
image to use for a texture, and coordinates for display, either in the 3D scene
(the 'world') or on the map or minimap.

.. note::

    This module will render objects anytime their ``draw`` method is run during
    an :overlay:event:`draw-3d` event, regardless of the UI state or MumbleLink
    data.

    Module authors should track these states and only draw when appropriate.

Textures
--------

Textures are managed by the :lua:class:`texture map <o3dtexturemap>` class.
A map can hold multiple textures that can be shared by multiple classes that use
them. Module authors are encouraged to create a single map use it for all 3D
rendering with the module.

Dimensions
~~~~~~~~~~

This module does not enforce any restrictions on texture dimensions when
loading the data, however internally all textures are stored in square
textures with dimensions that are a power of 2.

This should not affect sprites because the original dimensions of the image are
stored and used when rendering them, but trails may not be rendered as expected
if a non-square and/or non-power of 2 image is used.

Supported Formats
~~~~~~~~~~~~~~~~~

EG-Overlay uses `stb_image <https://github.com/nothings/stb/blob/master/stb_image.h>`_
to load texture image data, so any format it supports can be used.

All textures are loaded as 4 channel RGBA images.

3D Object Types
---------------

Sprites or icons can be drawn using a :lua:class:`o3dspritelist`. Trails, paths,
or walls can be drawn using the :lua:class:`o3dtraillist`. Both use textures
from a :lua:class:`o3dtexturemap`.

World vs Map
------------

Both :lua:class:`sprites <o3dspritelist>` and :lua:class:`trails <o3dtraillist>`
can be drawn either in the 3D world or on the 2D (mini)map. This module will
automatically handle projection and viewport settings for either.

The difference is specified when creating the list (see :lua:func:`spritelist`
and :lua:func:`traillist`).

When a trail or sprite is created as a world object, its coordinates are assumed
to be in GW2 map coordinates, with the X axis for west (negative) and east 
(positive), Y axis for altitude or down (negative) and up (positive), and Z
axis for south (negative) and north (positive).

.. important::
    
    GW2 map coordinates are in inches, while the GW2 MumbleLink data is returned
    in meters. The :lua:mod:`eg-overlay-3d` module assumes GW2 map coordinates
    are in inches, consistent with the game.

When a trail or sprite is created as a map object, its coordinates are assumed
to be in GW2 continent coordinates, with the X axis for west (negative) and east
(positive), Y axis for south (negative) and north (positive), and Z set to 0.
When specifying coordinates for map objects, it is possible to just specify X
and Y.

Examples
--------

.. code-block:: lua

        local o3d =require 'eg-overlay-3d'

        local textures = o3d.texturemap()

        local sprites = o3d.spritelist(textures)

        -- load texture image data from somewhere
        local texturedata = '...'

        -- add a texture
        textures:add('Texture Name', texturedata)

        -- attributes for a sprite
        local attrs = {
            x = 10.1, y = 20, z = -50, -- location
            color = 0xFF00FFFF,
            size = 120,
            tags = { -- arbitrary info
                id = '1234',
                group = 'foo'
            }
        }

        -- add the sprite
        sprites:add('Texture Name', attrs)

        -- update it later
        sprites:update({ group = 'foo' }, { color = 0x00FFFFFF })

        -- eventually draw, sometime during a draw-3d event
        sprites:draw()

        -- trails are similar
        
        -- add some other texture for a trail
        textures:add('Some other texture', trailtexturedata)

        local trails = o3d.traillist(textures)

        -- trails are a path drawn along points
        local trailpoints = {
            {0, 0, 0},
            {10, 10, 10},
            {20, 20, 20}
        }

        local trailattrs = {
            points = trailpoints,
            color = 0xFFFFFFFF,
            size = 60,
            wall = false,
            tags = {
                id = '5678',
                group = 'bar'
            }
        }

        traillist:add('Some other texture', trailattrs)

        -- draw it eventually
        traillist:draw()


Functions
---------
*/

int overlay_3d_lua_mouse_points_at(lua_State *L);
int overlay_3d_lua_mouse_pointer_map_coords(lua_State *L);

int texture_map_lua_new(lua_State *L);
int texture_map_lua_del(lua_State *L);
int texture_map_lua_clear(lua_State *L);
int texture_map_lua_add(lua_State *L);
int texture_map_lua_has(lua_State *L);

int sprite_list_lua_new(lua_State *L);
int sprite_list_lua_del(lua_State *L);
int sprite_list_lua_clear(lua_State *L);
int sprite_list_lua_add_sprite(lua_State *L);
int sprite_list_lua_update_sprites(lua_State *L);
int sprite_list_lua_remove_sprites(lua_State *L);
int sprite_list_lua_draw(lua_State *L);

int trail_list_lua_new(lua_State *L);
int trail_list_lua_del(lua_State *L);
int trail_list_lua_clear(lua_State *L);
int trail_list_lua_add(lua_State *L);
int trail_list_lua_remove(lua_State *L);
int trail_list_lua_draw(lua_State *L);
int trail_list_lua_update(lua_State *L);

luaL_Reg overlay_3d_mod_funcs[] = {
    "mousepointsat"        , &overlay_3d_lua_mouse_points_at,
    "mousepointermapcoords", &overlay_3d_lua_mouse_pointer_map_coords,
    "texturemap"           , &texture_map_lua_new,
    "spritelist"           , &sprite_list_lua_new,
    "traillist"            , &trail_list_lua_new,
    NULL                   , NULL
};

int overlay_3d_lua_open_module(lua_State *L) {
    lua_newtable(L);

    luaL_setfuncs(L, overlay_3d_mod_funcs, 0);

    return 1;
}

typedef struct {
    GLsizei size;
    float max_u;
    float max_v;
    float xy_ratio;
    GLuint texture;
} texture_map_texture_t;

typedef struct {
    size_t hash_map_size;
    size_t texture_count;

    char **keys;
    texture_map_texture_t **texture_info;
} texture_map_t;

typedef struct {
    struct {
        float x;
        float y;
        float z;
    } position;

    float max_u;
    float max_v;
    float xy_ratio;

    float size;

    float fade_near;
    float fade_far;

    struct {
        float r;
        float g;
        float b;
        float a;
    } color;

    uint32_t flags;

    mat4f_t rotation;
} sprite_list_sprite_t;

typedef struct {
    GLuint vao;
    GLuint vbo;

    size_t vbo_size;

    size_t texture_count;
    char **texture_keys;

    size_t *sprite_counts;
    size_t total_sprite_count;
    
    sprite_list_sprite_t **sprites;
    int **tags;

    texture_map_t *texture_map;
    int texture_map_luaref;

    int map;

    int vbo_update;
} sprite_list_t;

typedef struct {
    vec3f_t position;

    float u;
    float v;
} trail_coordinate_t;

typedef struct trail_list_trail_t {
    size_t point_count;
    vec3f_t *points;

    size_t coord_count;

    float fade_near;
    float fade_far;

    struct {
        float r;
        float g;
        float b;
        float a;
    } color;

    float size;
    int wall;

    int tags;
} trail_list_trail_t;

typedef struct {
    GLuint vao;
    GLuint vbo;

    size_t vbo_size;
    int vbo_update;

    size_t texture_count;
    char **texture_keys;

    size_t *trail_counts;
    size_t total_trail_count;

    int map;

    trail_list_trail_t **trails;

    texture_map_t *texture_map;
    int texture_map_luaref;
} trail_list_t;

/*** RST
.. lua:function:: mousepointsat(x, y, z, radius)
    
    Determine if the mouse pointer is positioned near the given position.

    This is determined by calculating a ray pointing from the camera through
    the mouse pointer and calculating if that ray would intersect a sphere
    with the given radius around the position.

    Coordinates and radius is in map units (inches).

    :param number x:
    :param number y:
    :param number z:
    :param number radius:
    :rtype: boolean

    .. versionhistory::
        :0.1.0: Added
*/
int overlay_3d_lua_mouse_points_at(lua_State *L) {
    if (!overlay_3d->in_frame) {
        return luaL_error(L, "Not in a frame.");
    }

    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float z = (float)luaL_checknumber(L, 3);
    float r = (float)luaL_checknumber(L, 4);
    
    // more from https://antongerdelan.net/opengl/raycasting.html

    // solving for t = -b +/- sqrt(b^2 - c)

    // Origin (camera) - C (the center point of the test)
    vec3f_t omc = {
        overlay_3d->camera.x - x,
        overlay_3d->camera.y - y,
        overlay_3d->camera.z - z
    };

    float b = vec3f_dot_vec3f(&overlay_3d->mouse_ray, &omc);
    float c = vec3f_dot_vec3f(&omc, &omc) - powf(r, 2);

    float dsqr = powf(b, 2) - c;

    if (dsqr < 0) {
        // a miss
        lua_pushboolean(L, 0);
        return 1;
    }

    if (dsqr == 0) {
        // a special case, an exact intersection on the bounding radius
        float t = -b; // sqrt(0) = 0
        if (t > 0) {
            lua_pushboolean(L, 1);
            return 1;
        }
    } else {
        // the mouse is pointed somewhere inside the bounding radius, check both
        // solutions
        float t1 = -b + sqrtf(dsqr);
        float t2 = -b - sqrtf(dsqr);
        if (t1 > 0 || t2 > 0) {
            lua_pushboolean(L, 1);
            return 1;
        }
    }

    // if we get to this point it's a miss

    lua_pushboolean(L, 0);
    return 1;
}

/*** RST
.. lua:function:: mousepointermapcoords()

    Return the continent coordinates of the mouse pointer. If the mouse is not
    currently over the (mini)map ``nil`` is returned instead.

    :rtype: number

    .. code-block:: lua
        :caption: Example

        local o3d = require 'eg-overlay-3d'

        local cx,cy = o3d.mousepointermapcoords()
    
    .. versionhistory::
        :0.1.0: Added
*/
int overlay_3d_lua_mouse_pointer_map_coords(lua_State *L) {
    int mx = 0;
    int my = 0;
    int fbw = 0;
    int fbh = 0;

    app_get_mouse_coords(&mx, &my);
    app_get_framebuffer_size(&fbw, &fbh);

    if (mx > fbw || my > fbh) return 0;

    float mousexf = 0.f;
    float mouseyf = 0.f;

    if (!overlay_3d->mapfullscreen) {
        if (! (mx >= overlay_3d->minimapleft &&
               my >= (fbh - overlay_3d->minimapbottom - overlay_3d->minimapheight) &&
               my <= (fbh - overlay_3d->minimapbottom))
        ) {
            // mouse is not within minimap
            return 0;
        }

        mousexf = ((float)mx - overlay_3d->minimapleft) / overlay_3d->minimapwidth;
        mouseyf = ((float)my - ((float)fbh - overlay_3d->minimapbottom - overlay_3d->minimapheight)) / overlay_3d->minimapheight;
    } else {
        mousexf = (float)mx / (float)fbw;
        mouseyf = (float)my / (float)fbh;
    }

    float mapcontwidth = overlay_3d->mapcoordsright - overlay_3d->mapcoordsleft;
    float mapcontheight = overlay_3d->mapcoordsbottom - overlay_3d->mapcoordstop;
    float mousecontx = overlay_3d->mapcoordsleft + (mousexf * mapcontwidth);
    float mouseconty = overlay_3d->mapcoordstop + (mouseyf * mapcontheight);
    lua_pushnumber(L, mousecontx);
    lua_pushnumber(L, mouseconty);

    return 2;
}

#define TEXTURE_MAP_MT "EGOverlayTextureMap"
#define lua_checktexturemap(L, ind) (texture_map_t*)luaL_checkudata(L, ind, TEXTURE_MAP_MT)

#define SPRITE_LIST_MT "EGOverlaySpriteList"
#define lua_checkspritelist(L, ind) (sprite_list_t*)luaL_checkudata(L, ind, SPRITE_LIST_MT)

#define TRAIL_LIST_MT "EGOverlayTrailList"
#define lua_checktraillist(L, ind) (trail_list_t*)luaL_checkudata(L, ind, TRAIL_LIST_MT)

luaL_Reg texture_map_funcs[] = {
    "__gc" , &texture_map_lua_del,
    "clear", &texture_map_lua_clear,
    "add"  , &texture_map_lua_add,
    "has"  , &texture_map_lua_has,
    NULL   , NULL
};

luaL_Reg sprite_list_funcs[] = {
    "__gc"  , &sprite_list_lua_del,
    "clear" , &sprite_list_lua_clear,
    "add"   , &sprite_list_lua_add_sprite,
    "update", &sprite_list_lua_update_sprites,
    "remove", &sprite_list_lua_remove_sprites,
    "draw"  , &sprite_list_lua_draw,
    NULL    , NULL
};

luaL_Reg trail_list_funcs[] = {
    "__gc"   , &trail_list_lua_del,
    "clear"  , &trail_list_lua_clear,
    "add"    , &trail_list_lua_add,
    "remove" , &trail_list_lua_remove,
    "draw"   , &trail_list_lua_draw,
    "update" , &trail_list_lua_update,
    NULL     , NULL
};

/*** RST
.. lua:function:: texturemap()

    Create a new :lua:class:`o3dtexturemap` object.

    :rtype: o3dtexturemap

    .. versionhistory::
        :0.1.0: Added
*/
int texture_map_lua_new(lua_State *L) {
    texture_map_t *map = lua_newuserdata(L, sizeof(texture_map_t));
    memset(map, 0, sizeof(texture_map_t));

    map->hash_map_size = 8;

    map->keys         = egoverlay_calloc(map->hash_map_size, sizeof(char*));
    map->texture_info = egoverlay_calloc(map->hash_map_size, sizeof(texture_map_texture_t*));
    
    if (luaL_newmetatable(L, TEXTURE_MAP_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, texture_map_funcs, 0);
    }
    lua_setmetatable(L, -2);

    return 1;
}

int texture_map_lua_del(lua_State *L) {
    texture_map_t *map = lua_checktexturemap(L, 1);

    for (size_t k=0;k<map->hash_map_size;k++) {
        if (map->keys[k]==NULL) continue;

        glDeleteTextures(1, &map->texture_info[k]->texture);
        egoverlay_free(map->texture_info[k]);
        egoverlay_free(map->keys[k]);
    }

    egoverlay_free(map->texture_info);
    egoverlay_free(map->keys);

    return 0;
}

/*** RST
.. lua:function:: spritelist(texturemap[, position])

    Create a new :lua:class:`o3dspritelist` object.

    :param o3dtexturemap texturemap:
    :param string position: (Optional) How the sprites in this list will be
        positioned. See below, default ``'world'``.
    :rtype: o3dspritelist

    **Position Values**

    =========== ================================================================
    Value       Description
    =========== ================================================================
    ``'world'`` Sprites are drawn within the 3D world, coordinates must be in
                map coordinates.
    ``'map'``   Sprites are dawn on the (mini)map, coordinates must be in
                continent coordinates.
    =========== ================================================================

    .. versionhistory::
        :0.1.0: Added
*/
int sprite_list_lua_new(lua_State *L) {
    texture_map_t *texmap = lua_checktexturemap(L, 1);

    int map = 0;
    if (lua_gettop(L)>=2) {
        const char *position = lua_tostring(L, 2);

        if (strcmp(position, "world")==0) map = 0;
        else if (strcmp(position, "map")==0) map = 1;
        else return luaL_error(L, "position must be either 'world' or 'map'");
    }

    sprite_list_t *list = lua_newuserdata(L, sizeof(sprite_list_t));
    memset(list, 0, sizeof(sprite_list_t));

    if (luaL_newmetatable(L, SPRITE_LIST_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, sprite_list_funcs, 0);
    }
    lua_setmetatable(L, -2);

    list->map = map;

    list->texture_map = texmap;
    lua_pushvalue(L, 1);
    list->texture_map_luaref = luaL_ref(L, LUA_REGISTRYINDEX);

    glGenVertexArrays(1, &list->vao);
    glGenBuffers(1, &list->vbo);

    glBindVertexArray(list->vao);
    glBindBuffer(GL_ARRAY_BUFFER, list->vbo);

    // VBO contents, shader inputs
    VERT_ATTRIB_VEC3 (0, 1, sprite_list_sprite_t, position );
    VERT_ATTRIB_FLOAT(1, 1, sprite_list_sprite_t, max_u    );
    VERT_ATTRIB_FLOAT(2, 1, sprite_list_sprite_t, max_v    );
    VERT_ATTRIB_FLOAT(3, 1, sprite_list_sprite_t, xy_ratio );
    VERT_ATTRIB_FLOAT(4, 1, sprite_list_sprite_t, size     );
    VERT_ATTRIB_FLOAT(5, 1, sprite_list_sprite_t, fade_near);
    VERT_ATTRIB_FLOAT(6, 1, sprite_list_sprite_t, fade_far );
    VERT_ATTRIB_VEC4 (7, 1, sprite_list_sprite_t, color    );
    VERT_ATTRIB_UINT (8, 1, sprite_list_sprite_t, flags    );
    VERT_ATTRIB_MAT4 (9, 1, sprite_list_sprite_t, rotation );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    return 1;
}

int sprite_list_lua_del(lua_State *L) {
    sprite_list_t *list = lua_checkspritelist(L, 1);

    glDeleteBuffers(1, &list->vbo);
    glDeleteVertexArrays(1, &list->vao);

    for (size_t t=0;t<list->texture_count;t++) {
        if (list->sprite_counts[t]) {
            for (size_t s=0;s<list->sprite_counts[t];s++) {
                if (list->tags[t][s]>=0) luaL_unref(L, LUA_REGISTRYINDEX, list->tags[t][s]);
            }

            egoverlay_free(list->sprites[t]);
            egoverlay_free(list->tags[t]);
        }
        egoverlay_free(list->texture_keys[t]);
    }

    if (list->texture_count) {
        egoverlay_free(list->texture_keys);
        egoverlay_free(list->sprite_counts);
        egoverlay_free(list->sprites);
        egoverlay_free(list->tags);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, list->texture_map_luaref);
    
    return 0;
}

/*** RST
.. lua:function:: traillist(texturemap[, position])

    Create a new :lua:class:`o3dtraillist` object.

    :param texturemap texturemap:
    :param string position: (Optional) How the trails in this list will be
        positioned. See below, default ``'world'``.
    :rtype: o3dspritelist

    **Position Values**

    =========== ================================================================
    Value       Description
    =========== ================================================================
    ``'world'`` Trails are drawn within the 3D world, coordinates must be in
                map coordinates.
    ``'map'``   Trails are dawn on the (mini)map, coordinates must be in
                continent coordinates.
    =========== ================================================================

    .. versionhistory::
        :0.1.0: Added
*/
int trail_list_lua_new(lua_State *L) {
    texture_map_t *texture_map = lua_checktexturemap(L, 1);

    int map = 0;
    if (lua_gettop(L)>=2) {
        const char *position = lua_tostring(L, 2);

        if (strcmp(position, "world")==0) map = 0;
        else if (strcmp(position, "map")==0) map = 1;
        else return luaL_error(L, "position must be either 'world' or 'map'");
    }

    trail_list_t *list = lua_newuserdata(L, sizeof(trail_list_t));
    memset(list, 0, sizeof(trail_list_t));

    lua_pushvalue(L, 1);
    list->texture_map_luaref = luaL_ref(L, LUA_REGISTRYINDEX);
    list->texture_map = texture_map;
    list->map = map;

    glGenVertexArrays(1, &list->vao);
    glGenBuffers(1, &list->vbo);

    glBindVertexArray(list->vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, list->vbo);

    VERT_ATTRIB_VEC3(0, 0, trail_coordinate_t, position);
    VERT_ATTRIB_VEC2(1, 0, trail_coordinate_t, u       );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (luaL_newmetatable(L, TRAIL_LIST_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, trail_list_funcs, 0);
    }
    lua_setmetatable(L, -2);

    return 1;
}

int trail_list_lua_del(lua_State *L) {
    trail_list_t *list = lua_checktraillist(L, 1);

    glDeleteBuffers(1, &list->vbo);
    glDeleteVertexArrays(1, &list->vao);

    for (size_t t=0;t<list->texture_count;t++) {
        egoverlay_free(list->texture_keys[t]);

        for (size_t trail=0;trail<list->trail_counts[t];trail++) {
            if (list->trails[t][trail].tags>=0) luaL_unref(L, LUA_REGISTRYINDEX, list->trails[t][trail].tags);
            if (list->trails[t][trail].points) egoverlay_free(list->trails[t][trail].points);
        }
        egoverlay_free(list->trails[t]);
    }
    if (list->texture_count) {
        egoverlay_free(list->texture_keys);
        egoverlay_free(list->trails);
        egoverlay_free(list->trail_counts);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, list->texture_map_luaref);

    return 0;
}

/*** RST
Classes
-------
*/

/*** RST
.. lua:class:: o3dtexturemap

    A texture map holds a list of textures that other objects in this module
    use when being displayed. 
*/

/*** RST
    .. lua:method:: clear()

        Remove all textures from this map and reset it to an initial state.

        .. danger::

            If sprite lists or other objects are still referencing textures
            within this map after this method is called their draws will not
            function properly.

        .. versionhistory::
            :0.1.0: Added
        
*/
int texture_map_lua_clear(lua_State *L) {
    texture_map_t *map = lua_checktexturemap(L, 1);

    for (size_t k=0;k<map->hash_map_size;k++) {
        if (map->keys[k]==NULL) continue;

        glDeleteTextures(1, &map->texture_info[k]->texture);
        egoverlay_free(map->texture_info[k]);
        map->texture_info[k] = NULL;
        egoverlay_free(map->keys[k]);
        map->keys[k] = NULL;
    }

    map->texture_count = 0;

    return 0;
}

texture_map_texture_t *texture_map_get(texture_map_t *map, const char *name) {
    if (!map->keys) return NULL;
    if (!name) return NULL;

    uint32_t hash = djb2_hash_string(name);
    size_t ind = hash % map->hash_map_size;

    if (map->keys[ind]==NULL) return NULL;

    while (map->keys[ind] && strcmp(name, map->keys[ind])!=0) {
        ind++;
        // this is a special case, name is a collision but it isn't in the map
        // yet
        if (ind==hash % map->hash_map_size) return NULL;

        if (ind>=map->hash_map_size) {
            // edge case, hash % map->hash_map_size == 0
            if (hash % map->hash_map_size == 0) return NULL;
            ind = 0;
        }
    }

    return map->texture_info[ind];
}

void texture_map_resize_hash_map(texture_map_t *map) {
    size_t newsize = map->hash_map_size + 8;

    char **newkeys = egoverlay_calloc(newsize, sizeof(char*));
    texture_map_texture_t **newinfos = egoverlay_calloc(newsize, sizeof(texture_map_texture_t*));

    for (size_t oldind=0;oldind<map->hash_map_size;oldind++) {
        if (map->keys[oldind]==NULL) continue; //shouldn't happen

        uint32_t hash = djb2_hash_string(map->keys[oldind]);
        size_t newind = hash % newsize;

        while (newkeys[newind]) {
            newind++;
            if (newind>=newsize) newind = 0;
        }
        newkeys[newind] = map->keys[oldind];
        newinfos[newind] = map->texture_info[oldind];
    }

    egoverlay_free(map->keys);
    egoverlay_free(map->texture_info);
    map->keys          = newkeys;
    map->texture_info  = newinfos;
    map->hash_map_size = newsize;
}

/*** RST
    .. lua:method:: add(name, data[, options])

        Add a texture.

        :param string name: The name of the texture, this will be used to
            reference it later when adding data to sprite lists and other
            objects.
        :param string data: The texture data.
        :param table options: (Optional) A table of texture options. See below.


        **Options**

        +-----------+----------------------------------------------------------------------------------------+
        | Field     | Description                                                                            |
        +===========+========================================================================================+
        | mipmaps   | A boolean indicating if mipmaps should be generated. Default ``true``.                 |
        +-----------+----------------------------------------------------------------------------------------+
        | minfilter | The filtering method used when the texture is displayed at a lower resolution. One of  |
        |           |                                                                                        |           
        |           | * ``nearest``                                                                          |
        |           | * ``linear``                                                                           |
        |           | * ``nearest-mipmap-nearest``                                                           |
        |           | * ``linear-mipmap-nearest``                                                            |
        |           | * ``nearest-mipmap-linear``                                                            |
        |           | * ``linear-mipmap-linear``                                                             |
        |           |                                                                                        |
        |           | Default: ``nearest`` if ``mipmaps`` is ``false`` or ``linear-mipmap-linear`` if        |
        |           | ``mipmaps`` is ``true``.                                                               |
        +-----------+----------------------------------------------------------------------------------------+
        | magfilter | The filtering method used when the texture is displayed at a higher resolution. One of |
        |           |                                                                                        |
        |           | * ``nearest``                                                                          |
        |           | * ``linear``                                                                           |
        |           |                                                                                        |
        |           | Default: ``linear``                                                                    |
        +-----------+----------------------------------------------------------------------------------------+
        | maxaniso  | The amount of anisotropic to use. Default: the maximum supported on the system,        |
        |           | probably ``16.0``                                                                      |
        +-----------+----------------------------------------------------------------------------------------+

        .. admonition:: Implementation Detail

            EG-Overlay uses `stb_image <https://github.com/nothings/stb/blob/master/stb_image.h>`_
            to load ``data``, so any format it supports can be used.

            All textures are loaded as 4 channel RGBA images.

        .. versionhistory::
            :0.1.0: Added
*/
int texture_map_lua_add(lua_State *L) {
    texture_map_t *map = lua_checktexturemap(L, 1);
    size_t namelen = 0;
    const char *name = luaL_checklstring(L, 2, &namelen);
    size_t datalen = 0;
    const uint8_t *data = (const uint8_t*)luaL_checklstring(L, 3, &datalen);

    if (texture_map_get(map, name)) return luaL_error(L, "duplicate texture name: %s", name);

    float max_aniso = 0.f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);

    GLint minfilter = GL_LINEAR_MIPMAP_LINEAR;
    GLint magfilter = GL_LINEAR;
    int mipmaps = 1;

    if (lua_gettop(L)>=4) {
        luaL_checktype(L, 4, LUA_TTABLE);
        if (lua_getfield(L, 4, "mipmaps")!=LUA_TNIL) mipmaps = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (!mipmaps) minfilter = GL_NEAREST;

        if (lua_getfield(L, 4, "minfilter")==LUA_TSTRING) {
            const char *minfilterstr = lua_tostring(L, -1);
            if      (strcmp(minfilterstr, "nearest"               )==0) minfilter = GL_NEAREST;
            else if (strcmp(minfilterstr, "linear"                )==0) minfilter = GL_LINEAR;
            else if (strcmp(minfilterstr, "nearest-mipmap-nearest")==0) minfilter = GL_NEAREST_MIPMAP_NEAREST;
            else if (strcmp(minfilterstr, "linear-mipmap-nearest" )==0) minfilter = GL_LINEAR_MIPMAP_NEAREST;
            else if (strcmp(minfilterstr, "nearest-mipmap-linear" )==0) minfilter = GL_NEAREST_MIPMAP_LINEAR;
            else if (strcmp(minfilterstr, "linear-mipmap-linear"  )==0) minfilter = GL_LINEAR_MIPMAP_LINEAR;
            else return luaL_error(L, "unknown minfilter: %s", minfilterstr);
        }
        lua_pop(L, 1);

        if (lua_getfield(L, 4, "magfilter")==LUA_TSTRING) {
            const char *magfilterstr = lua_tostring(L, -1);
            if      (strcmp(magfilterstr, "nearest")==0) magfilter = GL_NEAREST;
            else if (strcmp(magfilterstr, "linear" )==0) magfilter = GL_LINEAR;
            else return luaL_error(L,"unknown magfilter: %s", magfilterstr);
        }
        lua_pop(L, 1);

        if (lua_getfield(L, 4, "maxaniso")==LUA_TNUMBER) max_aniso = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    int channels = 0;
    int width = 0;
    int height = 0;
    uint8_t *pixels = stbi_load_from_memory(data, (int)datalen, &width, &height, &channels, 4);

    if (!pixels) return luaL_error(L, "failed to load texture data");

    if (map->texture_count==map->hash_map_size) texture_map_resize_hash_map(map);

    uint32_t hash = djb2_hash_string(name);
    int ind = hash % map->hash_map_size;

    while (map->keys[ind]) {
        ind++;
        if (ind>=map->hash_map_size) ind = 0;
    }

    map->keys[ind] = egoverlay_calloc(namelen+1, sizeof(char));
    memcpy(map->keys[ind], name, namelen);

    texture_map_texture_t *tex = egoverlay_calloc(1, sizeof(texture_map_texture_t));
    map->texture_info[ind] = tex;

    // calculate how large a square texture needs to be:
    // what's the smallest power of 2 that is greater than or equal to the
    // width and height?
    int32_t req_size = 16;
    while (req_size < width || req_size < height) req_size <<= 1;

    tex->xy_ratio = (float)width / (float)height;
    tex->max_u = (float)width / (float)req_size;
    tex->max_v = (float)height / (float)req_size;

    GLsizei mipmaplevels = 1;

    if (mipmaps) mipmaplevels += (GLsizei)floorf(log2f((float)req_size));

    glGenTextures(1, &tex->texture);

    glBindTexture(GL_TEXTURE_2D, tex->texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilter);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

    glTexStorage2D(GL_TEXTURE_2D, mipmaplevels, GL_RGBA8, req_size, req_size);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    map->texture_count++;

    stbi_image_free(pixels);

    return 0;
}

/*** RST
    .. lua:method:: has(name)

        Returns ``true`` if this map has a texture named ``name``.

        :param string name:

        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added
*/
int texture_map_lua_has(lua_State *L) {
    texture_map_t *map = lua_checktexturemap(L, 1);
    const char *name = luaL_checkstring(L, 2);

    uint32_t hash = djb2_hash_string(name);
    size_t ind = hash % map->hash_map_size;

    if (map->keys[ind]==NULL) {
        lua_pushboolean(L, 0);
        return 1;
    }

    while (strcmp(map->keys[ind], name)!=0) {
        ind++;
        if (ind >= map->hash_map_size) ind = 0;
        if (map->keys[ind]==NULL || ind==hash%map->hash_map_size) {
            lua_pushboolean(L, 0);
            return 1;
        }
    }

    lua_pushboolean(L, 1);
    return 1;
}

/*** RST

.. lua:class:: o3dspritelist
    
    A sprite list efficiently draws multiple sprites or icons within the 3D
    scene, each with their own attributes and texture. Sprite lists use
    textures from a :lua:class:`o3dtexturemap`.

    A sprite list stores all sprite information in one place and can render
    all sprites in a single call, making it much more efficient to draw larger
    numbers of sprites than drawing each one individually.

    In addition to the attributes used to display them, each sprite can also
    have arbitrary data stored in 'tags.' This can be used to remove or update
    individual or groups of sprites later.
*/

/** RST
    .. lua:method:: clear()

        Reset this sprite list to an initial state.

        .. versionhistory::
            :0.1.0: Added
*/
int sprite_list_lua_clear(lua_State *L) {
    sprite_list_t *list = lua_checkspritelist(L, 1);

    for (size_t t=0;t<list->texture_count;t++) {
        for (size_t s=0;s<list->sprite_counts[t];s++) {
            if (list->tags[t][s]>=0) luaL_unref(L, LUA_REGISTRYINDEX, list->tags[t][s]);
        }

        egoverlay_free(list->tags[t]);
        egoverlay_free(list->sprites[t]);
        egoverlay_free(list->texture_keys[t]);
    }

    egoverlay_free(list->tags);
    egoverlay_free(list->sprites);
    egoverlay_free(list->texture_keys);
    egoverlay_free(list->sprite_counts);

    list->texture_count = 0;
    list->tags          = NULL;
    list->sprites       = NULL;
    list->texture_keys  = NULL;
    list->sprite_counts = NULL;

    return 0;
}

void sprite_list_sprite_update(sprite_list_sprite_t *sprite, lua_State *L, int ind) {
    if (lua_getfield(L, ind, "x")!=LUA_TNIL) sprite->position.x = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "y")!=LUA_TNIL) sprite->position.y = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "z")!=LUA_TNIL) sprite->position.z = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "size")!=LUA_TNIL) sprite->size = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "fadenear")!=LUA_TNIL) sprite->fade_near = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "fadefar")!=LUA_TNIL) sprite->fade_far = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "billboard")!=LUA_TNIL) {
        int billboard = lua_toboolean(L, -1);
        sprite->flags = (sprite->flags & ~0x01) | billboard;
    }
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "color")!=LUA_TNIL) {
        ui_color_t color = (ui_color_t)lua_tointeger(L, -1);
        sprite->color.r = UI_COLOR_R(color);
        sprite->color.g = UI_COLOR_G(color);
        sprite->color.g = UI_COLOR_G(color);
        sprite->color.a = UI_COLOR_A(color);
    }
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "rotation")!=LUA_TNIL) {
        mat4f_identity(&sprite->rotation);
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        lua_geti(L, -1, 1); x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_geti(L, -1, 2); y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_geti(L, -1, 3); z = (float)lua_tonumber(L, -1); lua_pop(L, 1);

        mat4f_t xr = {0};
        mat4f_t yr = {0};
        mat4f_t zr = {0};

        mat4f_t zy = {0};

        mat4f_rotatex(&xr, deg2rad(x));
        mat4f_rotatey(&yr, deg2rad(y));
        mat4f_rotatez(&zr, deg2rad(z));

        mat4f_mult_mat4f(&zr, &yr, &zy);
        mat4f_mult_mat4f(&zy, &xr, &sprite->rotation);
    }
    lua_pop(L, 1); 
}

/*** RST
    .. lua:method:: add(texture, attributes)

        Add a sprite to this list. ``attributes`` must a table with the
        following fields:

        ========= ==============================================================
        Field     Description
        ========= ==============================================================
        x         The sprite's x coordinate, in map units, default ``0.0``.
        y         The sprite's y coordinate, in map units, default ``0.0``.
        z         The sprite's z coordinate, in map units, default ``0.0``.
        tags      A table of attributes that can be used with 
                  :lua:meth:`update` and :lua:meth:`remove`.
                  *Note:* the table is referenced directly, not copied.
        size      The sprite's size, in map units, default ``80``.
        color     Tint color and opacity, see :ref:`colors`, default
                  ``0xFFFFFFFF``.
        billboard A boolean indicating if the sprite should always face the
                  camera, default ``true``.
        rotation  A sequence of 3 numbers indicating the rotation to be applied
                  to the sprite along the X, Y, and Z axes, in that order. Only
                  applicable if ``billboard`` is ``false``.
        fadenear  A number that indicates how far away from the player a sprite
                  begins to fade to transparent.
        fadefar   A number that indicates how far away from the player a sprite
                  will become completely transparent.
        ========= ==============================================================

        :param string texture: The name of the texture, see :lua:meth:`o3dtexturemap.add`.
        :param table attributes: See above
        
        .. versionhistory::
            :0.1.0: Added
*/
int sprite_list_lua_add_sprite(lua_State *L) {
    sprite_list_t *list = lua_checkspritelist(L, 1);
    const char *texname = luaL_checkstring(L, 2);

    luaL_checktype(L, 3, LUA_TTABLE);

    texture_map_texture_t *tex = texture_map_get(list->texture_map, texname);

    if (tex==NULL) return luaL_error(L, "invalid texture name: %s", texname);

    int texture = -1;
    for (size_t t=0;t<list->texture_count;t++) {
        if (strcmp(list->texture_keys[t], texname)==0) {
            texture = (int)t;
            break;
        }
    }

    if (texture < 0) {
        list->texture_count++;
        list->texture_keys  = egoverlay_realloc(list->texture_keys , list->texture_count * sizeof(char*));
        list->sprite_counts = egoverlay_realloc(list->sprite_counts, list->texture_count * sizeof(size_t));
        list->sprites       = egoverlay_realloc(list->sprites      , list->texture_count * sizeof(sprite_list_sprite_t*));
        list->tags          = egoverlay_realloc(list->tags         , list->texture_count * sizeof(int*));

        texture = (int)list->texture_count - 1;

        list->texture_keys[texture] = egoverlay_calloc(strlen(texname)+1, sizeof(char));
        memcpy(list->texture_keys[texture], texname, strlen(texname));

        list->sprite_counts[texture] = 0;
        list->sprites[texture] = NULL;
        list->tags[texture] = NULL;
    }

    list->total_sprite_count++;
    list->sprite_counts[texture]++;
    list->sprites[texture] = egoverlay_realloc(
        list->sprites[texture],
        list->sprite_counts[texture] * sizeof(sprite_list_sprite_t)
    );
    list->tags[texture] = egoverlay_realloc(list->tags[texture], list->sprite_counts[texture] * sizeof(int));

    size_t spritei = list->sprite_counts[texture] - 1;
    sprite_list_sprite_t *s = list->sprites[texture] + spritei;

    s->max_u    = tex->max_u;
    s->max_v    = tex->max_v;
    s->xy_ratio = tex->xy_ratio;

    s->position.x = 0.f;
    s->position.y = 0.f;
    s->position.z = 0.f;

    list->tags[texture][spritei] = -1;
    s->size = 80.f;

    s->fade_near = -1.f;
    s->fade_far  = -1.f;

    s->color.r = 1.f;
    s->color.g = 1.f;
    s->color.b = 1.f;
    s->color.a = 1.f;

    s->flags = 0x01; // billboard

    if (lua_getfield(L, 3, "tags")!=LUA_TNIL) list->tags[texture][spritei] = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    sprite_list_sprite_update(s, L, 3);

    list->vbo_update = 1;

    return 0;
}

int tags_match(int tagsind, lua_State *L, int tags_ind) {
    lua_pushnil(L);
    while (lua_next(L, tags_ind)) {
        const char *tagkey = lua_tostring(L, -2);

        if (lua_getfield(L, tagsind, tagkey)==LUA_TNIL || lua_compare(L, -1, -2, LUA_OPEQ)==0) {
            // not a match

            // pop the current key, value, and the value from the sprites tag
            // table, which is nil
            lua_pop(L, 3);
            return 0; 
        }
        lua_pop(L, 2); // pop the sprite's tag value and input value, leaving key
    }

    return 1;
}

/*** RST
    .. lua:method:: update(tags, attributes)

        Update the sprites that have matching tags.

        An empty tags table matches all sprites. A sprite must match all tag
        values given, if a sprite does not have a value for a tag it will not
        match.

        Returns the number of sprites updated.

        :param table tags:
        :param table attributes:
        :rtype: number

        .. versionhistory::
            :0.1.0: Added

*/
int sprite_list_lua_update_sprites(lua_State *L) {
    sprite_list_t *list = lua_checkspritelist(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);

    int nupdated = 0;

    for (size_t t=0;t<list->texture_count;t++) {
        for (size_t s=0;s<list->sprite_counts[t];s++) {
            if (list->tags[t][s]<0) continue;

            lua_geti(L, LUA_REGISTRYINDEX, list->tags[t][s]);
            int tagsind = lua_gettop(L);
            if (tags_match(tagsind, L, 2)) {
                sprite_list_sprite_update(list->sprites[t] + s, L, 3);
                nupdated++;
            }
            lua_pop(L, 1);
        }
    }

    if (nupdated) {
        list->vbo_update = 1;
    }

    lua_pushinteger(L, nupdated);

    return 1;
}

/*** RST
    .. lua:method:: remove(tags)

        Remove sprites that have matching tags.

        An empty tags table matches all sprites. A sprite must match all tag
        values given, if a sprite does not have a value for a tag it will not
        match.

        :param table tags:
        :returns: The number of sprites removed.
        :rtype: number

        .. versionhistory::
            :0.1.0: Added
*/
int sprite_list_lua_remove_sprites(lua_State *L) {
    sprite_list_t *list = lua_checkspritelist(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);

    int nremoved = 0;

    for (size_t t=0;t<list->texture_count;t++) {
        int snrem = 0;
        for (size_t s=0;s<list->sprite_counts[t];s++) {
            if (list->tags[t][s]<0) continue;

            lua_geti(L, LUA_REGISTRYINDEX, list->tags[t][s]);
            int tagsind = lua_gettop(L);
            
            if (tags_match(tagsind, L, 2)) {
                luaL_unref(L, LUA_REGISTRYINDEX, list->tags[t][s]);
                for (size_t sm=s+1;sm<list->sprite_counts[t];sm++) {
                    sprite_list_sprite_t *a = list->sprites[t] + sm - 1;
                    sprite_list_sprite_t *b = list->sprites[t] + sm;

                    memcpy(a, b, sizeof(sprite_list_sprite_t));
                    list->tags[t][sm-1] = list->tags[t][sm];
                }
                list->sprite_counts[t]--;
                list->total_sprite_count--;
                s--;
                snrem++;
            }
            lua_pop(L, 1);
        }

        if (snrem) {
            nremoved += snrem;
            list->sprites[t] = egoverlay_realloc(
                list->sprites[t],
                list->sprite_counts[t] * sizeof(sprite_list_sprite_t)
            );
            list->tags[t] = egoverlay_realloc(list->tags[t], list->sprite_counts[t] * sizeof(int));
        }
    }

    if (nremoved) {
        list->vbo_update = 1;
   }

    lua_pushinteger(L, nremoved);

    return 1;
}

void sprite_list_update_vbo(sprite_list_t *list) {
    size_t new_vbo_size = 0;
    for (size_t t=0;t<list->texture_count;t++) {
        new_vbo_size += (sizeof(sprite_list_sprite_t) * list->sprite_counts[t]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, list->vbo);

    if (list->vbo_size < new_vbo_size) {
        glBufferData(GL_ARRAY_BUFFER, new_vbo_size, NULL, GL_DYNAMIC_DRAW);
        list->vbo_size = new_vbo_size;
    }

    size_t offset = 0;
    for (size_t t=0;t<list->texture_count;t++) {
        size_t tvbo_size = sizeof(sprite_list_sprite_t) * list->sprite_counts[t];
        if (tvbo_size==0) continue;
        glBufferSubData(GL_ARRAY_BUFFER, offset, tvbo_size, list->sprites[t]);
        offset += tvbo_size;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    list->vbo_update = 0;
}

/*** RST
    .. lua:method:: draw()

        Draw all sprites in this list.

        :param boolean map: If ``true`` draw sprites on the map or minimap.
            Otherwise draw sprites in the 3D world.

        .. important::
        
            This method can only be called during :overlay:event:`draw-3d`.
            Attempts to call it at any other time will result in an error.

        .. versionhistory::
            :0.1.0: Added
*/
int sprite_list_lua_draw(lua_State *L) {
    sprite_list_t *list = lua_checkspritelist(L, 1);

    if (!overlay_3d->in_frame) return luaL_error(L, "draw can only be called during update-3d");

    if (list->total_sprite_count==0) return 0;

    if (list->vbo_update) sprite_list_update_vbo(list);

    glDisable(GL_CULL_FACE);
    
    gl_shader_program_use(overlay_3d->sprite_array_program);

    int oldvp[4] = {0};

    if (list->map) {
        glUniformMatrix4fv(0, 1, GL_FALSE, (GLfloat*)&overlay_3d->map_view);
        glUniformMatrix4fv(1, 1, GL_FALSE, (GLfloat*)&overlay_3d->map_proj);

        if (!overlay_3d->mapfullscreen) {
            int fbw, fbh;
            app_get_framebuffer_size(&fbw, &fbh);

            glGetIntegerv(GL_VIEWPORT, oldvp);
            glViewport(
                overlay_3d->minimapleft,
                overlay_3d->minimapbottom,
                overlay_3d->minimapwidth,
                overlay_3d->minimapheight
            );
        }
    } else {
        glUniformMatrix4fv(0, 1, GL_FALSE, (GLfloat*)overlay_3d->view);
        glUniformMatrix4fv(1, 1, GL_FALSE, (GLfloat*)overlay_3d->proj);
    }

    glUniform3fv(2, 1, (GLfloat*)&overlay_3d->player_pos);
    glUniform1ui(3, (GLuint)list->map);
    glUniform3fv(4, 1, (GLfloat*)&overlay_3d->camera);

    glUniform1f(5, (GLfloat)overlay_3d->minimapleft  );
    glUniform1f(6, (GLfloat)overlay_3d->minimapbottom);
    glUniform1f(7, (GLfloat)overlay_3d->minimapheight);

    glBindVertexArray(list->vao);

    glActiveTexture(GL_TEXTURE0);
    
    GLint inst = 0;
    for (size_t t=0;t<list->texture_count;t++) {
        texture_map_texture_t *tex = texture_map_get(list->texture_map, list->texture_keys[t]);

        if (!tex) {
            logger_t *log = logger_get("lua-gl");
            logger_error(log, "invalid texture key: %s", list->texture_keys[t]);
        } else {
            glBindTexture(GL_TEXTURE_2D, tex->texture);
        }

        glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)list->sprite_counts[t], inst);
        inst += (GLint)list->sprite_counts[t];
    }

    if (list->map && !overlay_3d->mapfullscreen) {
        glViewport(oldvp[0], oldvp[1], oldvp[2], oldvp[3]);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindVertexArray(0);

    glUseProgram(0);

    glEnable(GL_CULL_FACE);

    return 0;
}

/*** RST
.. lua:class:: o3dtraillist

    A trail list displays multiple paths as flat 2D textures drawn along
    specified points.
*/

/*** RST
    .. lua:method:: clear()

        Clear this trail list and set it back to an initial state.

    .. note::

        This will not clear the texture list that this trail list references.

    .. versionhistory::
        :0.1.0: Added
*/
int trail_list_lua_clear(lua_State *L) {
    trail_list_t *list = lua_checktraillist(L, 1);

    for (size_t t=0;t<list->texture_count;t++) {
        egoverlay_free(list->texture_keys[t]);

        for (size_t trail=0;trail<list->trail_counts[t];trail++) {
            if (list->trails[t][trail].tags>=0) luaL_unref(L, LUA_REGISTRYINDEX, list->trails[t][trail].tags);
            if (list->trails[t][trail].points) egoverlay_free(list->trails[t][trail].points);
        }
        egoverlay_free(list->trails[t]);
    }
    if (list->texture_count) {
        egoverlay_free(list->texture_keys);
        egoverlay_free(list->trails);
        egoverlay_free(list->trail_counts);

        list->texture_keys = NULL;
        list->trails = NULL;
        list->trail_counts = NULL;
    }
    list->texture_count = 0;

    return 0;
}

void trail_list_trail_update(trail_list_t *list, trail_list_trail_t *trail, lua_State *L, int ind) {
    if (lua_getfield(L, ind, "fadenear")!=LUA_TNIL) trail->fade_near = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "fadefar")!=LUA_TNIL) trail->fade_far = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "color")!=LUA_TNIL) {
        ui_color_t color = (ui_color_t)lua_tointeger(L, -1);
        trail->color.r = UI_COLOR_R(color);
        trail->color.g = UI_COLOR_G(color);
        trail->color.b = UI_COLOR_B(color);
        trail->color.a = UI_COLOR_A(color);
    }
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "size")!=LUA_TNIL) {
        trail->size = (float)lua_tonumber(L, -1);
        list->vbo_update = 1;
    }
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "wall")!=LUA_TNIL) {
        trail->wall = lua_toboolean(L, -1);
        list->vbo_update = 1;
    }
    lua_pop(L, 1);

    if (lua_getfield(L, ind, "points")!=LUA_TNIL) {
        int pointsind = lua_gettop(L);
        size_t c = luaL_len(L, pointsind);
        if (c < 2) luaL_error(L, "trails must have at least 2 points.");

        if (trail->points) egoverlay_free(trail->points);

        trail->point_count = c;
        trail->points = egoverlay_calloc(trail->point_count, sizeof(vec3f_t));

        for (size_t i=1;i<=trail->point_count;i++) {
            lua_geti(L, pointsind, i); // the sequence of x,y,z
            int pind = lua_gettop(L);
            lua_geti(L, pind, 1);
            lua_geti(L, pind, 2);
            lua_geti(L, pind, 3);
            trail->points[i-1].x = (float)lua_tonumber(L, -3);
            trail->points[i-1].y = (float)lua_tonumber(L, -2);
            trail->points[i-1].z = (float)lua_tonumber(L, -1);
            lua_pop(L, 4);
        }
        list->vbo_update = 1;
    }
    lua_pop(L, 1);
}

/*** RST
    .. lua:method:: addtrail(texturename, attributes)

        Create a new trail.

        ``attributes`` must be a table with the following fields:

        ======== ===============================================================
        Field    Description
        ======== ===============================================================
        points   A sequence of sequences, trail points. ie. { {1,1,1}, {2,2,2} }
        tags     A table of attributes that can be used other methods of this
                 list to update or remove trails with matching tags.
                 *Note:* the table is referenced directly, not copied.
        fadenear A number that indicates how far away from the player a trail
                 begins to fade to transparent.
        fadefar  A number that indicates how far away from the player a trail
                 will become completely transparent.
        ======== ===============================================================

        :param string texturename: The name of a texture in the texture list
            this trail list references.
        :param table attributes: See above.

        .. versionhistory::
            :0.1.0: Added
*/
int trail_list_lua_add(lua_State *L) {
    trail_list_t *list = lua_checktraillist(L, 1);

    const char *texname = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    if (lua_getfield(L, 3,"points")!=LUA_TTABLE) return luaL_error(L, "points must be a table");
    lua_pop(L, 1);

    texture_map_texture_t *tex = texture_map_get(list->texture_map, texname);

    if (!tex) return luaL_error(L, "invalid texture name: %s", texname);

    int texture = -1;
    for (size_t t=0;t<list->texture_count;t++) {
        if (strcmp(texname, list->texture_keys[t])==0) {
            texture = (int)t;
            break;
        }
    }

    if (texture<0) {
        texture = (int)list->texture_count;
        list->texture_count++;
        list->texture_keys = egoverlay_realloc(list->texture_keys, list->texture_count * sizeof(char*));
        list->texture_keys[texture] = egoverlay_calloc(strlen(texname)+1, sizeof(char));
        memcpy(list->texture_keys[texture], texname, strlen(texname));

        list->trail_counts = egoverlay_realloc(list->trail_counts, list->texture_count * sizeof(size_t));
        list->trails = egoverlay_realloc(list->trails, list->texture_count * sizeof(trail_list_trail_t**));
        list->trail_counts[texture] = 0;
        list->trails[texture] = NULL;
    }

    list->trail_counts[texture]++;
    list->trails[texture] = egoverlay_realloc(list->trails[texture], list->trail_counts[texture] * sizeof(trail_list_trail_t));

    trail_list_trail_t *trail = list->trails[texture] + list->trail_counts[texture] - 1;

    trail->coord_count = 0;
    trail->point_count = 0;
    trail->points      = NULL;
    trail->fade_near   = -1.f;
    trail->fade_far    = -1.f;
    trail->color.r     = 1.f;
    trail->color.g     = 1.f;
    trail->color.b     = 1.f;
    trail->color.a     = 1.f;
    trail->tags        = -1;

    if (lua_getfield(L, 3, "tags")!=LUA_TNIL) trail->tags = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    trail_list_trail_update(list, trail, L, 3);

    list->vbo_update = 1;

    return 0;
}

/*** RST
    .. lua:method:: remove(tags)

        Remove trails that have matching tags.

        :param table tags:
        :returns: The number of trails removed.
        :rtype: number

        .. versionhistory::
            :0.1.0: Added
*/
int trail_list_lua_remove(lua_State *L) {
    trail_list_t *list = lua_checktraillist(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);

    int nremoved = 0;

    for (size_t tex=0;tex<list->texture_count;tex++) {
        int tremoved = 0;
        for (size_t trail=0;trail<list->trail_counts[tex];trail++) {
            if (list->trails[tex][trail].tags<0) continue;

            lua_geti(L, LUA_REGISTRYINDEX, list->trails[tex][trail].tags);
            int tagsind = lua_gettop(L);

            if (tags_match(tagsind, L, 2)) {
                luaL_unref(L, LUA_REGISTRYINDEX, list->trails[tex][trail].tags);
                if (list->trails[tex][trail].points) egoverlay_free(list->trails[tex][trail].points);

                for (size_t tm=trail+1;tm<list->trail_counts[tex];tm++) {
                    trail_list_trail_t *a = &list->trails[tex][tm-1];
                    trail_list_trail_t *b = &list->trails[tex][tm];
                    memcpy(a, b, sizeof(trail_list_trail_t));
                }
                trail--;
                tremoved++;
                list->trail_counts[tex]--;
            }
            lua_pop(L, 1);
        }
        if (tremoved) {
            list->trails[tex] = egoverlay_realloc(list->trails[tex], list->trail_counts[tex] * sizeof(trail_list_trail_t));
        }
        nremoved += tremoved;
    }

    if (nremoved) list->vbo_update = 1;

    lua_pushinteger(L, nremoved);
    return 1;
}

trail_coordinate_t *trail_list_trail_calc_coords(trail_list_t *list, trail_list_trail_t *trail) {
    trail_coordinate_t *coords = NULL;

    trail->coord_count = ((trail->point_count - 1) * 2) + 2;
    coords = egoverlay_calloc(trail->coord_count, sizeof(trail_coordinate_t));
    
    // in the 3D world, up is Y positive
    vec3f_t up = { 0.f, 1.f, 0.f };
    if (trail->wall && list->map) {
        // unless it's a wall, then up is to the east
        up.x = 1.f;
        up.y = 0.f;
        up.z = 0.f;
    } else if (list->map) {
        // or on the map, up is Z
        up.x = 0.f;
        up.y = 0.f;
        up.z = 1.f;
    }

    size_t coord_ind = 0;

    for (size_t i=0; i<trail->point_count-1; i++) {
        // each segment is made up of 2 points, p1 and p2
        vec3f_t *p1 = &trail->points[i];
        vec3f_t *p2 = &trail->points[i+1];

        /* 
           We calculate a,b,c,d based on the points p1 and p2

           c<----p2----->d
                 ^
                 |
                 |
                 |
                 |
                 |
           a<----p1----->b

           Our 6 coords are the vertices of 2 triangles:
            - b, a, d
            - d, a, c

           The most efficient way of storing and drawing these triangles will
           be a triangle strip. Each segment will use 4 vertices: b, a, d, c
           which will be drawn in b, a, d and d, a, c order.

           The next segment will use d, c, e, f and so on.
        */
        
        // forward is a vector from p1 to p2
        vec3f_t forward = {0};
        vec3f_sub_vec3f(p2, p1, &forward);

        vec3f_t forwardn = {0};
        vec3f_normalize(&forward, &forwardn);
        
        // side is a vector perpendicular to forward and up
        vec3f_t side = {0};
        vec3f_crossproduct(&up, &forwardn, &side);

        vec3f_t siden = {0};
        vec3f_normalize(&side, &siden);

        // side becomes our vector to b and d
        // and the opposite direction is to a and c
        vec3f_mult_f(&siden, trail->size / 2.f, &side);

        // TODO: adjust points where segments meet at angles

        // if this is the first segment then calculate a and b, otherwise
        // and c and d from the previous segment will become a and b
        if (i==0) {
            // b
            vec3f_add_vec3f(p1, &side, &coords[coord_ind].position);
            coords[coord_ind].u = 1.f;
            coords[coord_ind].v = 0.f;
            coord_ind++;

            // a
            vec3f_sub_vec3f(p1, &side, &coords[coord_ind].position);
            coords[coord_ind].u = 0.f;
            coords[coord_ind].v = 0.f;
            coord_ind++;
        }

        float section_len = vec3f_length(&forward);
        
        // If the segment is too long fading won't be calculated properly
        // so insert extra points along forward.
        // I'm not exactly sure what's going on, but I suspect it's something to
        // do with how the z/depth isn't linear and the calculated fade distance
        // isn't being interpolated properly when the end vertex is beyond a
        // certain Z distance away. In any case, breaking long segments up into
        // multiple smaller ones makes fading with fade_near/fade_far work much
        // better on very long segments.
        if (section_len > 5000.f && !list->map) {
            int extrapoints = (int)floorf(section_len / 5000.f);
            trail->coord_count += extrapoints * 2;
            coords = egoverlay_realloc(coords, trail->coord_count * sizeof(trail_coordinate_t));

            for (int ep=0; ep < extrapoints; ep++) {
                float len = 5000.f * (ep + 1);
                vec3f_t fp = {0}; // vector from p1 to this extra point
                vec3f_t p = {0};  // the extra point

                vec3f_mult_f(&forwardn, len, &fp);
                vec3f_add_vec3f(p1, &fp, &p);

                float epv = - ( 5000.f / trail->size ) + coords[coord_ind-1].v;

                vec3f_add_vec3f(&p, &side, &coords[coord_ind].position);
                coords[coord_ind].u = 1.f;
                coords[coord_ind].v = epv;
                coord_ind++;

                vec3f_sub_vec3f(&p, &side, &coords[coord_ind].position);
                coords[coord_ind].u = 0.f;
                coords[coord_ind].v = epv;
                coord_ind++;

                section_len -= 5000.f;
            }
        }
        

        float section_frac = section_len / trail->size;

        float p2v = section_frac + coords[coord_ind-1].v;
        
        // d
        vec3f_add_vec3f(p2, &side, &coords[coord_ind].position);
        coords[coord_ind].u = 1.f;
        coords[coord_ind].v = p2v;
        coord_ind++;

        // c
        vec3f_sub_vec3f(p2, &side, &coords[coord_ind].position);
        coords[coord_ind].u = 0.f;
        coords[coord_ind].v = p2v;
        coord_ind++;
    }

    return coords;
}

void trail_list_update_vbo(trail_list_t *list) {
    glBindBuffer(GL_ARRAY_BUFFER, list->vbo);

    trail_coordinate_t ***coords = egoverlay_calloc(list->texture_count, sizeof(trail_coordinate_t**));

    size_t newsize = 0;
    for (size_t tex=0;tex<list->texture_count;tex++) {
        coords[tex] = egoverlay_calloc(list->trail_counts[tex], sizeof(trail_coordinate_t*));

        for (size_t trail=0;trail<list->trail_counts[tex];trail++) {
            coords[tex][trail] = trail_list_trail_calc_coords(list, &list->trails[tex][trail]);

            newsize += list->trails[tex][trail].coord_count * sizeof(trail_coordinate_t);
        }
    }
    
    if (newsize > list->vbo_size) {
        list->vbo_size = newsize;
        glBufferData(GL_ARRAY_BUFFER, list->vbo_size * sizeof(trail_coordinate_t), NULL, GL_STATIC_DRAW);
    }

    size_t offset = 0;
    for (size_t tex=0;tex<list->texture_count;tex++) {
        for (size_t trail=0;trail<list->trail_counts[tex];trail++) {
            glBufferSubData(
                GL_ARRAY_BUFFER,
                offset,
                list->trails[tex][trail].coord_count * sizeof(trail_coordinate_t),
                coords[tex][trail]
            );
            offset += list->trails[tex][trail].coord_count * sizeof(trail_coordinate_t);
            egoverlay_free(coords[tex][trail]);
        }
        egoverlay_free(coords[tex]);
    }

    egoverlay_free(coords);

    glBindBuffer(GL_ARRAY_BUFFER, 0);    

    list->vbo_update = 0;
}

/*** RST
    .. lua:method:: draw()
    
        Draw all of the trails in this list.

        .. important::
            This method can only be called during :overlay:event:`draw-3d`. Attempts
            to call it at any other time will result in an error.

        .. versionhistory::
            :0.1.0: Added
*/
int trail_list_lua_draw(lua_State *L) {
    trail_list_t *list = lua_checktraillist(L, 1);

    if (!overlay_3d->in_frame) return luaL_error(L, "not in a frame");

    if (list->vbo_update) trail_list_update_vbo(list);
    
    glDisable(GL_CULL_FACE);

    gl_shader_program_use(overlay_3d->trail_program);

    glActiveTexture(GL_TEXTURE0);

    // set general uniforms

    int oldvp[4] = {0};

    if (list->map) {
        glUniformMatrix4fv(0, 1, GL_FALSE, (GLfloat*)&overlay_3d->map_view);
        glUniformMatrix4fv(1, 1, GL_FALSE, (GLfloat*)&overlay_3d->map_proj);

        if (!overlay_3d->mapfullscreen) {
            int fbw, fbh;
            app_get_framebuffer_size(&fbw, &fbh);

            glGetIntegerv(GL_VIEWPORT, oldvp);
            glViewport(
                overlay_3d->minimapleft,
                overlay_3d->minimapbottom,
                overlay_3d->minimapwidth,
                overlay_3d->minimapheight
            );
        }
    } else {
        glUniformMatrix4fv(0, 1, GL_FALSE, (GLfloat*)overlay_3d->view);
        glUniformMatrix4fv(1, 1, GL_FALSE, (GLfloat*)overlay_3d->proj);
    }

    glUniform1f(5, (GLfloat)overlay_3d->minimapleft);
    glUniform1f(6, (GLfloat)overlay_3d->minimapbottom);
    glUniform1f(7, (GLfloat)overlay_3d->minimapheight);
    glUniform1ui(8, (GLuint)list->map);
    glUniform3fv(9, 1, (GLfloat*)&overlay_3d->player_pos);
    glUniform3fv(10, 1, (GLfloat*)&overlay_3d->camera);

    glBindVertexArray(list->vao);

    size_t first = 0;
    for (size_t tex=0;tex<list->texture_count;tex++) {
        if (list->trail_counts[tex]==0) continue;

        texture_map_texture_t *gltex = texture_map_get(list->texture_map, list->texture_keys[tex]);

        glBindTexture(GL_TEXTURE_2D, gltex->texture);

        for (size_t trail=0;trail<list->trail_counts[tex];trail++) {
            glUniform1f(2, list->trails[tex][trail].fade_near);
            glUniform1f(3, list->trails[tex][trail].fade_far);
            glUniform4fv(4, 1, (GLfloat*)&list->trails[tex][trail].color);

            glDrawArrays(GL_TRIANGLE_STRIP, (GLint)first, (GLsizei)list->trails[tex][trail].coord_count);
            first += list->trails[tex][trail].coord_count;
        }
    }

    if (list->map && !overlay_3d->mapfullscreen) {
        glViewport(oldvp[0], oldvp[1], oldvp[2], oldvp[3]);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindVertexArray(0);

    glUseProgram(0);

    glEnable(GL_CULL_FACE);

    return 0;
}

/*** RST
    .. lua:method:: update(tags, attributes)

        Update attributes of trails with matching tags.

        :param table tags:
        :param table attributes:

        :returns: The number of trails updated
        :rtype: integer

        .. versionhistory::
            :0.1.0: Added
*/
int trail_list_lua_update(lua_State *L) {
    trail_list_t *list = lua_checktraillist(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);

    int nupdated = 0;

    for (size_t tex=0;tex<list->texture_count;tex++) {
        int tupdated = 0;
        for (size_t trail=0;trail<list->trail_counts[tex];trail++) {
            if (list->trails[tex][trail].tags<0) continue;

            lua_geti(L, LUA_REGISTRYINDEX, list->trails[tex][trail].tags);
            int tagsind = lua_gettop(L);

            if (tags_match(tagsind, L, 2)) {
                trail_list_trail_update(list, &list->trails[tex][trail], L, 3);
                tupdated++;
            }
            lua_pop(L, 1);
        }
        nupdated += tupdated;
    }

    if (nupdated) list->vbo_update = 1;

    lua_pushinteger(L, nupdated);
    return 1;
}


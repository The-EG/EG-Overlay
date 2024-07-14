subdir('logging')

egoverlay_sources = files(
    'app.c',
    'lamath.c',
    'main.c',
    'gl.c',
    
    'ui/ui.c',
    'ui/rect.c',
    'ui/font.c',
    'ui/text.c',
    'ui/polyline.c',
    'ui/window.c',
    'ui/image.c',
    'ui/button.c',
    'ui/menu.c',
    'ui/box.c',
    'ui/scrollview.c',
    'ui/text-entry.c',
    'ui/separator.c',

    'utils.c',

    'lua-manager.c',

    'mumble-link.c',

    'web-request.c',

    'settings.c',

    'xml.c',

    'zip.c',

    'lua-sqlite.c',
    'lua-json.c',

    '../contrib/glad/src/gl.c',
    '../contrib/sqlite-amalgamation-3460000/sqlite3.c'
) + logging_sources

subdir('shaders')
subdir('textures')
subdir('fonts')
subdir('res')
subdir('lua')

executable(
    'eg-overlay',
    egoverlay_sources + icon_res + shaders + lua_files,    
    dependencies: [glfw_lib, winmm_lib, freetype2_dep, curl_dep, curl_link_libs, libxml2_dep, zlib_dep, lua_dep, jansson_dep],
    link_with: [],
    install: true,
    c_args: [
        '-DSQLITE_DQS=0',
        '-DSQLITE_THREADSAFE=0', #sqlite only used from the single Lua thread
        #'-DSQLITE_DEFAULT_MEMSTATUS=0',
        '-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1',
        '-DSQLITE_LIKE_DOESNT_MATCH_BLOBS',
        '-DSQLIET_MAX_EXPR_DEPTH=0',
        '-DSQLITE_OMIT_UTF16',
        #'-DSQLITE_OMIT_DECLTYPE',
        '-DSQLITE_OMIT_DEPRECATED',
        '-DSQLITE_OMIT_PROGRESS_CALLBACK',
        '-DSQLITE_OMIT_SHARED_CACHE',
        '-DSQLITE_USE_ALLOCA',
        '-DSQLITE_OMIT_AUTOINIT',
        '-DSQLITE_STRICT_SUBTYPE=1',

        '-DSQLITE_ENABLE_COLUMN_METADATA',
        '-DSQLITE_ENABLE_MATH_FUNCTIONS',
        '-DSQLITE_ENABLE_MEMORY_MANAGEMENT',
        '-DSQLITE_ENABLE_NULL_TRIM'
    ],
    include_directories: [stb_inc, glad_inc, include_directories('../contrib/sqlite-amalgamation-3460000/', is_system: true)],
    win_subsystem: 'windows',
    override_options: ['warning_level=3', 'c_std=c11']
)

executable('hashstr', 'hashstr.c')
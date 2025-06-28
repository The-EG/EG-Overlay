project('sqlite', 'c', version: '3.46')

sqlite_lib = static_library('sqlite-3.46',
    'sqlite3.c',
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
        '-DSQLITE_WIN32_MALLOC',
        '-DSQLITE_ENABLE_COLUMN_METADATA',
        '-DSQLITE_ENABLE_MATH_FUNCTIONS',
        '-DSQLITE_ENABLE_MEMORY_MANAGEMENT',
        '-DSQLITE_ENABLE_NULL_TRIM'
    ]
)

sqlite_dep = declare_dependency(link_with: sqlite_lib)

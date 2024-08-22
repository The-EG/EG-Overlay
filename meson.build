project('eg-overlay', 'c', version: '0.1.0-pre', default_options: ['c_std=c17'])

version_data = configuration_data()
version_data.set('version_str', meson.project_version())
version_data.set('rc_version_num', '0,1,0,0')

cc = meson.get_compiler('c')
fs = import('fs')

cmake = import('cmake')

cmake_buildtypes = {
    'debug': {'CMAKE_BUILD_TYPE':'Debug'},
    'debugoptimized': {'CMAKE_BUILD_TYPE':'RelWithDebInfo'},
    'minsize': {'CMAKE_BUILD_TYPE':'MinSizeRel'},
    'release': {'CMAKE_BUILD_TYPE':'Release'}
}

generic_cmake_opts = cmake.subproject_options()
curl_opts = cmake.subproject_options()

generic_cmake_opts.add_cmake_defines(cmake_buildtypes[get_option('buildtype')])
curl_opts.add_cmake_defines(cmake_buildtypes[get_option('buildtype')])

glfw_proj = cmake.subproject('glfw', options: generic_cmake_opts)
glfw_lib = glfw_proj.dependency('glfw')

stb_inc = include_directories('contrib/stb', is_system: true)
glad_inc = include_directories('contrib/glad/include', is_system: true)

winmm_lib = cc.find_library('winmm')

zlib_dep = dependency('zlib')

freetype2_dep = dependency('freetype2')


curl_opts.add_cmake_defines({
    'BUILD_CURL_EXE': 'OFF',
    'BUILD_SHARED_LIBS': 'OFF',
    'HTTP_ONLY': 'ON',
    'CURL_USE_SCHANNEL': 'ON'
})

curl_proj = cmake.subproject('curl',options: curl_opts)
# these aren't included with the static lib dependency
curl_link_libs = [
    cc.find_library('crypt32'),
    cc.find_library('ws2_32')
]
curl_dep = curl_proj.dependency('libcurl_static')

libxml2_dep = dependency('libxml-2.0', default_options: ['default_library=static', 'warning_level=0', 'python=false', 'readline=false', 'threads=enabled'])

lua_proj = subproject('lua-5.4.6')
lua_dep = lua_proj.get_variable('lua_dep')

jansson_proj = cmake.subproject('jansson-2.14', options:generic_cmake_opts)
jansson_dep = jansson_proj.dependency('jansson')

subdir('src')

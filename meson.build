project('eg-overlay', 'c', 'cpp', version: '0.1.0-dev', default_options: ['c_std=c17'])

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
jansson_opts = cmake.subproject_options()

jansson_opts.add_cmake_defines(cmake_buildtypes[get_option('buildtype')])
jansson_opts.add_cmake_defines({'JANSSON_BUILD_DOCS': 'OFF'})
generic_cmake_opts.add_cmake_defines(cmake_buildtypes[get_option('buildtype')])

zlib_dep = dependency('zlib')

freetype2_dep = dependency('freetype2')

libxml2_dep = dependency('libxml-2.0', 
  default_options: [
    'default_library=static',
    'warning_level=0',
    'python=false',
    'readline=false',
    'threads=disabled',
    'catalog=false',
    'debuging=false',
    'html=false',
    'ipv6=false',
    'modules=disabled',
    'push=false',
    'readline=false',
    'zlib=disabled',
    'sax1=false',
    'history=false',
    'iconv=disabled',
    'icu=disabled',
    'lzma=disabled',
    'pattern=false',
    'regexps=false',
    'schemas=false',
    'schematron=false',
    'xptr=false',
    'xpath=false',
    'xinclude=false',
    'valid=false',
    'c14n=false',
    'iso8859x=false',
    'output=false',
    ]
)

lua_proj = subproject('lua-5.4.7')
lua_dep = lua_proj.get_variable('lua_dep')

jansson_proj = cmake.subproject('jansson-2.14', options:jansson_opts)
jansson_dep = jansson_proj.dependency('jansson')

winmm_lib = cc.find_library('winmm')
rpcrt_lib = cc.find_library('rpcrt4')

crypt32_dep = cc.find_library('Crypt32')
wininet_dep = cc.find_library('WinInet')
shlwapi_dep = cc.find_library('shlwapi')

d3d12_dep = cc.find_library('D3D12')
dxguid_dep = cc.find_library('DXGUID')
dxgi_dep = cc.find_library('DXGI')
dcomp_dep = cc.find_library('DComp')
winmm_dep = cc.find_library('WinMM')
wincodecs_dep = cc.find_library('WindowsCodecs')

dxc = find_program('dxc')

subdir('src')

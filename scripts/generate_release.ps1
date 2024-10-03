$ErrorActionPreference = "Stop"

New-Item -Path . -Name release -ItemType Directory -Force | Out-Null

Push-Location -Path release

& meson setup builddir .. --wipe --buildtype release

& meson compile -C builddir

New-Item -Path . -Name "EG-Overlay"         -ItemType Directory -Force | Out-Null
New-Item -Path . -Name "EG-Overlay\lib"     -ItemType Directory -Force | Out-Null
New-Item -Path . -Name "EG-Overlay\include" -ItemType Directory -Force | Out-Null

Copy-Item -Path "builddir\src\eg-overlay.exe" -Destination "EG-Overlay"                  -Force
Copy-Item -Path "builddir\src\lua"            -Destination "EG-Overlay\lua"     -Recurse -Force
Copy-Item -Path "builddir\src\fonts"          -Destination "EG-Overlay\fonts"   -Recurse -Force
Copy-Item -Path "builddir\src\shaders"        -Destination "EG-Overlay\shaders" -Recurse -Force

Copy-Item -Path "builddir\subprojects\zlib-1.3\z.dll" -Destination "EG-Overlay" -Force

Copy-Item -Path "builddir\subprojects\lua-5.4.7\lua-5.4.7.dll" -Destination "EG-Overlay"         -Force
Copy-Item -Path "builddir\subprojects\lua-5.4.7\lua-5.4.7.lib" -Destination "EG-Overlay\lib"     -Force
Copy-Item -Path "..\subprojects\lua-5.4.7\src\lua.h"           -Destination "EG-Overlay\include" -Force
Copy-Item -Path "..\subprojects\lua-5.4.7\src\luaconf.h"       -Destination "EG-Overlay\include" -Force
Copy-Item -Path "..\subprojects\lua-5.4.7\src\lauxlib.h"       -Destination "EG-Overlay\include" -Force

Copy-Item -Path "builddir\subprojects\libpng-1.6.40\png16-16.dll" -Destination "EG-Overlay" -Force

Copy-Item -Path "builddir\subprojects\freetype-2.13.2\freetype-6.dll" -Destination "EG-Overlay" -Force

Copy-Item -Path "..\data" -Destination "EG-Overlay\data" -Recurse -Force

Pop-Location

# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT
$workspace = $PSScriptRoot | Split-Path -Parent

$luapath = $workspace + "\builddir\subprojects\lua-5.4.7"
$zlibpath = $workspace + "\builddir\subprojects\zlib-1.3"
$ftpath = $workspace + "\builddir\subprojects\freetype-2.13.2"
$pngpath = $workspace + "\builddir\subprojects\libpng-1.6.40"
#$xmlpath = $workspace + "\builddir\subprojects\libxml2-2.13.1"

$paths = $luapath , $zlibpath, $ftpath, $pngpath#, $xmlpath

$oldpath = $env:Path

foreach ($p in $paths) {
    if ($env:Path -split ";" -notcontains "$p") {
        $env:Path += ";" + $p
    }
}

$builddir = $workspace + "\builddir"

$workingdir = $builddir + "\src\"
$srcpath = $workspace + "\src\"

$exe = $workingdir + "eg-overlay.exe"

$dbginit = ".lines;l+t;l+s;l+o"

$terminal_cls = "CASCADIA_HOSTING_WINDOW_CLASS"

$extra_args = ""

if ($args[0]) {
    switch ($args[0]) {
        "terminal" { $extra_args = @("--target-win-class", $terminal_cls) }
        "notepad" { $extra_args = @("--target-win-class", "Notepad") }
        "no-hooks" { $extra_args = @("--no-input-hooks") }
        "terminal-no-hooks" { $extra_args = @("--target-win-class", $terminal_cls, "--no-input-hooks") }
        "notepad-no-hooks" { $extra_args = @("--target-win-class", "Notepad", "--no-input-hooks") }
        "script" { $extra_args = @("--lua-script", $args[1]) }
        Default {
            Write-Error ("Unknown debug target '" +$args[0] + "'. Valid targets: terminal, no-hooks, terminal-no-hooks")
            return
        }
    }
}

#& meson compile -C builddir

#if ($LastExitCode -ne 0) {
#    Write-Error "Build failed, aborting debug."
#    return
#}

$windbgscript = $builddir + "\eg-overlay.windbg"

echo ".lines"                >  $windbgscript
echo "l+t"                   >> $windbgscript
echo "l+s"                   >> $windbgscript
echo ".sympath+ $workingdir" >> $windbgscript
echo ".srcpath+ $srcpath"    >> $windbgscript

# Put this in a try because if we use ctrl-c to break within the debugger
# PowerShell will also break and we'll be left in the $workingdir and contaminated
# path
try {
    Push-Location $workingdir

    & windbgx `
        -G `
        -c "$<$windbgscript" `
        "$exe" $extra_args "--debug" &
} finally {
    Pop-Location
    $env:Path = $oldpath
}


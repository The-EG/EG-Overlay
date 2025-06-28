# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT
$extra_args = ""

if ($args[0]) {
    switch ($args[0]) {
        "debug" { $extra_args = "" }
        "release" { $extra_args = @("--debug", "--optimization=3", "-Db_ndebug=true") }
        Default {
            Write-Error ("Unknown build type '" + $args[0] + ". Valid build types: debug, release")
            return
        }
    }
}

& meson setup builddir $extra_args


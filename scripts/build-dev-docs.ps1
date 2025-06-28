# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT
Write-Host "Generating Rust docs in builddir\doc..."
& ninja --quiet -C builddir rustdoc

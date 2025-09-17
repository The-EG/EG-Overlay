# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

fonts = {
    '../../contrib/fonts/Inter/Inter-VariableFont_opsz,wght.ttf': 'Inter.ttf',
    '../../contrib/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf': 'Inter-Italic.ttf',
    '../../contrib/fonts/CascadiaCode-2404.23/ttf/CascadiaCode.ttf': 'CascadiaCode.ttf',
    '../../contrib/fonts/CascadiaCode-2404.23/ttf/CascadiaMono.ttf': 'CascadiaMono.ttf',
    '../../contrib/fonts/google-material-design-icons/variablefont/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf': 'MaterialSymbolsOutlined.ttf',
    '../../contrib/fonts/google-material-design-icons/variablefont/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].codepoints': 'MaterialSymbolsOutlined.codepoints',
}

foreach i, o : fonts
    configure_file(
        input: i,
        output: o,
        copy: true
    )
endforeach

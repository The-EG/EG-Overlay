fonts = {
    '../../contrib/fonts/Inter/Inter-VariableFont_slnt,wght.ttf': 'Inter.ttf',
    '../../contrib/fonts/CascadiaCode-2404.23/ttf/CascadiaCode.ttf': 'CascadiaCode.ttf',
    '../../contrib/fonts/CascadiaCode-2404.23/ttf/CascadiaMono.ttf': 'CascadiaMono.ttf',
}

foreach i, o : fonts
    configure_file(
        input: i,
        output: o,
        copy: true
    )
endforeach

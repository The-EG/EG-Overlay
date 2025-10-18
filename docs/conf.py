# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information
import os
import sys

project = 'EG-Overlay'
copyright = '2025, Taylor Talkington'
author = 'Taylor Talkington'

version = '0.3.0'
release = '0.3.0-dev'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

sys.path.append(os.path.abspath('./_ext'))

extensions = [
    'sphinxcontrib.mermaid',
    'eg_overlay'
]

templates_path = ['_templates']
exclude_patterns = [
    'docs/_build',
    '**/Thumbs.db',
    '**/.DS_Store',
    'docs/_ext',
    'docs/_include',
]
include_patterns = [
    'docs/**',
    'src/**/lua.rs',
    'src/lua_sqlite3.rs',
    'src/lua_path.rs',
    '*.rst',
    'src/**/*.rst',

    'src/lua/mumble-link-events.lua',
    'src/lua/overlay-menu.lua',
    'src/lua/overlay-stats.lua',
    'src/lua/utils.lua',
    'src/lua/console.lua',
    'src/lua/dialogs.lua',
    'src/lua/mumble-link-info.lua',

    'src/lua/gw2/init.lua',
    'src/lua/gw2/data.lua',

    'src/lua/markers/data.lua',
    'src/lua/markers/package.lua',
]

#root_doc = 'docs/index'

nitpicky = True



#toc_object_entries_show_parents = 'all'

toc_object_entries = True

github_repo = "https://github.com/The-EG/EG-Overlay"

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output


html_title = f'{project} {release}'
html_theme = 'sphinx_book_theme'
html_theme_options = {
    "show_toc_level": 3,
    "repository_url": github_repo,
    "use_download_button": False,
    "use_source_button": True,
    "icon_links": [
        {"name": "GitHub", "url": github_repo, "icon": "fa-brands fa-github", "type": "fontawesome"}
    ]
}
html_copy_source = False
html_static_path = ['_static']
html_css_files = [
    'css/egoverlay.css'
]

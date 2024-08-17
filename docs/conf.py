# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information
import os
import sys

project = 'EG-Overlay'
copyright = '2024, Taylor Talkington'
author = 'Taylor Talkington'

version = '0.0.1'
release = '0.0.1'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

sys.path.append(os.path.abspath('./_ext'))

extensions = [
    'sphinxcontrib.mermaid',
    'eg_overlay'
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', '_ext']
include_patterns = ['docs/**', 'src/**', '*.rst']

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
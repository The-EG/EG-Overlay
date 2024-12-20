from sphinx.application import Sphinx
from sphinx.util.typing import ExtensionMetadata

from .directives import VersionHistoryDirective, LuaTableFieldsDirective, SettingsValuesDirective
from .domain import OverlayDomain

from .parsers import LuaCommentParser, CCommentParser

from .luadomain import LuaDomain

def setup(app: Sphinx) -> ExtensionMetadata:
    app.add_directive('versionhistory', VersionHistoryDirective)
    app.add_directive('luatablefields', LuaTableFieldsDirective)
    app.add_directive('settingsvalues', SettingsValuesDirective)
    
    app.add_domain(OverlayDomain)

    app.add_domain(LuaDomain)

    app.add_source_suffix('.lua', 'luarstcomments')
    app.add_source_parser(LuaCommentParser)

    app.add_source_suffix('.c', 'crstcomments')
    app.add_source_parser(CCommentParser)

    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True
    }
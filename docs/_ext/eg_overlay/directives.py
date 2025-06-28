# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT
from docutils import nodes

from sphinx.util.docutils import SphinxDirective

import re

def make_entry(text):
    entry = nodes.entry()
    p = nodes.paragraph(text, text)
    entry += p

    return entry

OPTPAT = re.compile(r':(?P<name>[^:]+): (?P<value>.*)')

class OptionTableDirective(SphinxDirective):
    has_content = True
    name_header = 'Name'
    value_header = 'Value'
    caption = 'Options'

    def run(self) -> list[nodes.Node]:
        tbl = nodes.table()
        tgroup = nodes.tgroup(cols=2)
        tgroup += nodes.colspec()
        tgroup += nodes.colspec()
        tbl += tgroup

        thead = nodes.thead()
        tgroup += thead
        throw = nodes.row()
        thead += throw
        
        throw += make_entry(self.name_header)
        throw += make_entry(self.value_header)

        tbody = nodes.tbody()
        tgroup += tbody

        for line in self.content:
            m = OPTPAT.match(line)
            if m is None: continue
            row = nodes.row()
            row += make_entry(m.group('name'))
            row += make_entry(m.group('value'))
            tbody += row

        n = []

        if self.caption:
            hdr = nodes.paragraph()
            hdr += nodes.strong(self.caption, self.caption)
            n.append(hdr)
        n.append(tbl)

        return n
    
class VersionHistoryDirective(OptionTableDirective):
    name_header = 'Version'
    value_header = 'Notes'
    caption = 'Version History'

class LuaTableFieldsDirective(OptionTableDirective):
    name_header = 'Field'
    value_header = 'Description'
    caption = None

class SettingsValuesDirective(OptionTableDirective):
    name_header = 'Key'
    value_header = 'Description'
    caption = None

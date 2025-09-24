# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

#from collections import defaultdict
#from typing import Iterable
from docutils import nodes
from docutils.parsers.rst import Directive, directives
from sphinx import addnodes
#from sphinx.builders import Builder
from sphinx.directives import ObjectDescription
from sphinx.domains import Domain, Index, IndexEntry, ObjType
from sphinx.domains.std import ConfigurationValue
#from sphinx.environment import BuildEnvironment
from sphinx.util.nodes import make_refnode
from sphinx.roles import XRefRole

class ModuleDirective(Directive):
    """
    Directive to mark a new overlay module. This doesn't output anything itself,
    but it does organize things like settings values.
    """

    has_content = False
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {}

    def run(self):
        env = self.state.document.settings.env
        modname = self.arguments[0].strip()
        if modname == 'None':
            env.ref_context.pop('overlay:module', None)
        else:
            env.ref_context['overlay:module'] = modname
        return []

class ModuleSettingDirective(ConfigurationValue):
    has_content = True
    required_arguments = 1

    def handle_signature(self, sig, signode):
        signode += addnodes.desc_name(text=sig)
        signode['settingname'] = sig
        modname = self.options.get(
            'module', self.env.ref_context.get('overlay:module'))
        signode['modname'] = modname
        return sig

    def add_target_and_index(self, name, sig, signode):
        modname = signode.get('modname', 'none')
        anchor = f'overlay-modsetting-{modname}-{sig}'
        signode['ids'].append(anchor)
        _name = f'overlay.modsetting.{modname}.{sig}'
        objs = self.env.domaindata['overlay']['objects']
        objs.append((_name, sig, 'modsetting', self.env.docname, anchor, 0))

    def _object_hierarchy_parts(self, sig_node):
        return (sig_node['modname'], sig_node['settingname'])

    def _toc_entry_name(self, sig_node):
        return sig_node['settingname']

class ModuleSettingIndex(Index):
    name = 'modsettingindex'
    localname = 'Module Settings Index'
    shortname = 'module settings'

    def generate(self, docnames=None):
        content = {}
        items = ((name, dispname, typ, docname, anchor)
                 for name, dispname, typ, docname, anchor, prio
                 in self.domain.get_objects() if typ=='modsetting')
        items = sorted(items, key=lambda item: item[0])
        for name, dispname, typ, docname, anchor in items:
            lis = content.setdefault(dispname[0].lower(), [])
            lis.append((
                dispname, 0, docname,
                anchor,
                docname, '', typ
            ))
        re = [(k,v) for k,v in sorted(content.items())]
        return (re, True)

class DBDirective(Directive):
    """
    Directive to mark description of a new Database.
    """

    has_content = False
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {}

    def run(self):
        env = self.state.document.settings.env
        dbname = self.arguments[0].strip()
        if dbname == 'None':
            env.ref_context.pop('overlay:database', None)
        else:
            env.ref_context['overlay:database'] = dbname
        return []

class DBTableDirective(ObjectDescription):
    has_content = True
    required_arguments = 1

    def handle_signature(self, sig, signode):
        signode += addnodes.desc_name(text=sig)
        signode['tablename'] = sig
        dbname = self.options.get(
            'database', self.env.ref_context.get('overlay:database'))
        signode['dbname'] = dbname
        return sig

    def add_target_and_index(self, name, sig, signode):
        dbname = signode.get('dbname', 'none')
        anchor = f'overlay-dbtable-{dbname}-{sig}'
        signode['ids'].append(anchor)
        _name = f'overlay.dbtable.{dbname}.{sig}'
        objs = self.env.domaindata['overlay']['objects']
        objs.append((_name, sig, 'dbtable', self.env.docname, anchor, 0))

    def _object_hierarchy_parts(self, sig_node):
        return (sig_node['dbname'], sig_node['tablename'])

    def _toc_entry_name(self, sig_node):
        return sig_node['tablename']

class DBTableIndex(Index):
    name = 'dbtableindex'
    localname = 'Database Table Index'
    shortname = 'db tables'

    def generate(self, docnames=None):
        content = {}
        items = ((name, dispname, typ, docname, anchor)
                 for name, dispname, typ, docname, anchor, prio
                 in self.domain.get_objects() if typ=='dbtable')
        items = sorted(items, key=lambda item: item[0])
        for name, dispname, typ, docname, anchor in items:
            lis = content.setdefault(dispname[0].lower(), [])
            lis.append((
                dispname, 0, docname,
                anchor,
                docname, name.split('.')[-2], typ
            ))
        re = [(k,v) for k,v in sorted(content.items())]
        return (re, True)

class EventDirective(ObjectDescription):
    has_content = True
    required_arguments = 1

    def handle_signature(self, sig, signode):
        signode += addnodes.desc_name(text=sig)
        signode['eventname'] = sig
        return sig

    def add_target_and_index(self, name, sig, signode):
        anchor = f'overlay-event-{sig}'
        signode['ids'].append(anchor)
        _name = f'overlay.event.{sig}'
        objs = self.env.domaindata['overlay']['objects']
        objs.append((_name, sig, 'event', self.env.docname, anchor, 0))

    def _object_hierarchy_parts(self, sig_node):
        return tuple(sig_node['eventname'])

    def _toc_entry_name(self, sig_node):
        return sig_node['eventname']

class EventIndex(Index):
    name = 'eventindex'
    localname = 'Events Index'
    shortname = 'events'

    def generate(self, docnames=None):
        content = {}
        items = ((name, dispname, typ, docname, anchor)
                 for name, dispname, typ, docname, anchor, prio
                 in self.domain.get_objects() if typ=='event')
        items = sorted(items, key=lambda item: item[0])
        for name, dispname, typ, docname, anchor in items:
            lis = content.setdefault(dispname[0].lower(), [])
            lis.append((
                dispname, 0, docname,
                anchor,
                docname, '', typ
            ))
        re = [(k,v) for k,v in sorted(content.items())]
        return (re, True)

class OverlayDomain(Domain):
    name = 'overlay'
    label = 'EG-Overlay'

    object_types = {
        'event': ObjType('event', 'event', 'obj'),
        'dbtable': ObjType('dbtable','dbtable','obj'),
        'database': ObjType('database', 'database', 'obj'),
        'module': ObjType('module', 'module', 'obj'),
    }

    roles = {
        'event': XRefRole(),
        'dbtable': XRefRole(),
        'modsetting': XRefRole(),
    }

    indices = {
        EventIndex,
        DBTableIndex,
        ModuleSettingIndex,
    }

    directives = {
        'event': EventDirective,
        'dbtable': DBTableDirective,
        'database': DBDirective,
        'module': ModuleDirective,
        'modsetting': ModuleSettingDirective,
    }

    initial_data = {
        'objects': []
    }

    def get_full_qualified_name(self, node):
        return f'overlay.{type(node).__name__}.{node.arguments[0]}'

    def get_objects(self):
        for obj in self.data['objects']: yield(obj)

    def resolve_xref(self, env, fromdocname, builder, typ, target, node, contnode):
        if typ == 'dbtable':
            targparts = target.split('.')
            targname = targparts[-1]
            dbname = '.'.join(targparts[:-1])

            matches = [(docname, anchor)
                       for name, sig, otyp, docname, anchor, prio
                       in self.get_objects() if otyp==typ and name.split('.')[-2]==dbname and name.split('.')[-1]==targname]
        else:
            matches = [(docname, anchor)
                        for name, sig, otyp, docname, anchor, prio
                        in self.get_objects() if sig == target and otyp == typ]

        if len(matches) > 0:
            todocname = matches[0][0]
            targ = matches[0][1]

            return make_refnode(builder, fromdocname, todocname, targ, contnode, targ)
        else:
            return None

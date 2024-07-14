from sphinx.parsers import RSTParser
from sphinx.util import logging
import re

class CommentParser(RSTParser):

    def parse(self, inputstring, document):
        rstlines = []
        
        for block in self.block_pattern.findall(inputstring):
            for line in block.splitlines():
                rstlines.append(line)
            rstlines.append('')

        if len(rstlines)>0: 
            rststr = '\n'.join(rstlines) + '\n'
            super().parse(rststr, document)
            

class LuaCommentParser(CommentParser):
    supported = ('luarstcomments',)
    block_pattern = re.compile(r'^--\[\[ RST\r?\n(?P<content>.*?)\r?\n^\]\]--$', re.M | re.S)

class CCommentParser(CommentParser):
    supported = ('crstcomments',)
    block_pattern = re.compile(r'^/\*\*\* RST\r?\n(?P<content>.*?)\r?\n^\*/$', re.M | re.S)
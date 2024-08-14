# create index placeholders
"Events Index" >  .\overlay-eventindex.rst
"============" >> .\overlay-eventindex.rst

"Lua Module Index" >  .\lua-modindex.rst
"================" >> .\lua-modindex.rst

"" > .\genindex.rst

# build docs
sphinx-build -v -n -b html . docs\_build -c docs
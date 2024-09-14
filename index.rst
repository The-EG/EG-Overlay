EG-Overlay
==========

EG-Overlay is yet another overlay for GuildWars 2. It is designed to be light
weight and 'out of the way' as possible from both a resources and module author
point of view. The core is written in C and everything else is Lua. Modules can
be written purely in Lua, or Lua C modules can also be used.

User Guide
----------

.. toctree::
    :caption: User Guide
    :hidden:

    docs/install
    docs/running
    docs/configuration

- :doc:`docs/install`
- :doc:`docs/running`
- :doc:`docs/configuration`

.. toctree::
    :caption: Bundled Modules
    :hidden:

    /src/lua/main-menu
    /src/lua/console
    /src/lua/map-buddy
    /src/lua/overlay-stats
    /src/lua/psna-tracker
    /src/lua/mumble-link-info

Development
-----------

EG-Overlay provides a Lua environment for customization and extension. Modules
are Lua modules/packages that are loaded into this environment and use the API
detailed below.

.. toctree::
    :maxdepth: 3
    :caption: Development
    :hidden:

    docs/development/compiling
    docs/development/lua-api
    docs/development/lua-threads

- :doc:`docs/development/compiling`
- :doc:`docs/development/lua-api`
- :doc:`docs/development/lua-threads`
        

.. toctree::
    :caption: Index
    :hidden:

    General <genindex>
    Events <overlay-eventindex>
    Lua Modules <lua-modindex>

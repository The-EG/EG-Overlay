.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

EG-Overlay
==========

EG-Overlay is yet another overlay for GuildWars 2. It is designed to be
lightweight and 'out of the way' as possible from both a resources and module
author point of view. The core is written in Rust and everything else is Lua.
Modules can be written purely in Lua, or Lua C/Rust modules can also be used.

User Guide
----------

.. toctree::
    :caption: User Guide
    :hidden:

    docs/faq
    docs/install
    docs/running
    docs/configuration

- :doc:`docs/faq`
- :doc:`docs/install`
- :doc:`docs/running`
- :doc:`docs/configuration`

.. toctree::
    :caption: Bundled Modules
    :hidden:

    /src/lua/overlay-menu
    /src/lua/console
    /src/lua/mumble-link-info
    /src/lua/markers/markers
    /src/lua/overlay-stats


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


EG-Overlay
==========

EG-Overlay is yet another overlay for GuildWars 2. It is designed to be light weight and 'out of the way' as possible from both a resources and module author point of view. The core is written in C and everything else is Lua. Modules can be written purely in Lua, or Lua C modules can also be used.

User Guide
----------

.. toctree::
    :caption: User Guide
    :hidden:

    docs/running
    docs/configuration

- :doc:`docs/running`
- :doc:`docs/configuration`

.. toctree::
    :caption: Bundled Modules
    :hidden:

    /src/lua/console

Development
-----------

EG-Overlay provides a Lua environment for customization and extension. Modules are Lua modules/packages that are loaded into this environment and use the API detailed below.

.. toctree::
    :maxdepth: 3
    :caption: Development
    :hidden:

    docs/development/lua-api
    docs/development/lua-threads

- :doc:`docs/development/lua-api`
- :doc:`docs/development/lua-threads`
        

.. toctree::
    :caption: Index
    :hidden:

    General <genindex>
    Events <overlay-eventindex>
    Lua Modules <lua-modindex>


Development TODOs
-----------------

- [X] Settings/Configuration Store
- [ ] UI Styling System
- [ ] UI Widgets
    - [ ] Text
        - [x] Fonts and basic text
        - [ ] Rich text
        - [ ] Markdown text
        - [x] Solid/filled rectangle
    - [ ] Arbitrary lines
        - [ ] Straight lines with square corners
        - [ ] More corner/end options
    - [ ] Windows
        - [x] Basic functionality
        - [x] Auto sizing
        - [x] Resizing
        - [x] Moving
        - [ ] Minimizing
        - [ ] Closing
        - [X] Link to settings store (size/position)
    - [ ] Layout containers
        - [x] Box
        - [ ] Grid
        - [ ] Fixed (arbitrary placement)
        - [X] Scroll view
    - [X] Buttons
    - [X] Menus
    - [X] Input text box
    - [ ] Radio buttons
    - [X] Check box
    - [ ] Combo box
    - [ ] Collapsing group/header

Lua API
=======

EG-Overlay does not define any globals, unlike many other programs that interface via Lua. Instead certain functionality is exposed via embedded Lua C modules, and others through bundled Lua files. This allows for customization and substitution without rebuilding the core program.

EG-Overlay Modules
------------------

.. toctree::
    :maxdepth: 1

    /src/lua-manager
    /src/lua/gw2/init
    /src/lua-json
    /src/lua/logger
    /src/lua/mumble-link-events
    /src/mumble-link
    /src/settings
    /src/web-request
    /src/lua/db
    /src/lua-sqlite
    /src/xml
    /src/zip
    /src/ui/ui
    /src/lua/ui-helpers
    /src/lua-gl
    /src/lua/utils


Lua Types
---------

Various Lua types are referenced throughout this documentation. Each module documentation contains any specific types/classes and Lua provides the standard types below:

.. lua:data:: nil

    :lua:class:`nil` is most often used to represent the absence of a value. The only thing that equates to :lua:class:`nil` is :lua:class:`nil`, although :lua:class:`nil` will also evaluate to ``false``.

.. lua:data:: boolean

    :lua:class:`boolean` has two values, ``true`` and ``false``.

.. lua:alias:: integer = number
.. lua:alias:: float = number

.. lua:data:: number

    :lua:class:`number` can hold both integer and floating-point numbers, however the two are not interchangeable in many instances. Module documentation will specify :lua:class:`integer` when only an integer is appropriate.

    .. warning::
        Supplying a real (floating-point) value in place of an integer may result in a Lua error.

.. lua:data:: string

    :lua:class:`string` can hold character or binary data as bytes, including nulls.

.. lua:data:: function

    Any Lua function.

.. lua:data:: table

    A Lua object or sequence. Modules may expect specific table structures or fields and many will return data using tables.

.. lua:alias:: sequence = table

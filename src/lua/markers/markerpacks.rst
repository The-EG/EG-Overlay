.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

EG-Overlay Markerpack Format
============================

EG-Overlay's marker module houses data in SQLite databases. This is a departure
from the set of XML files that have become standard with other overlays, however
the change is intentional.

Why SQLite?
-----------

Some markerpack developers have created packs that are much larger than the
original author of the XML pack format likely intended. This creates challenges,
forcing overlays to either load an entire pack into memory, suffer undesirable
performance when changing maps or refreshing marker data, or performing one time
optimizations of the data.

Using SQLite databases fall into that last category, and while it isn't as
transparent to users as optimizing XML, it is the most straightforward way to
achieve desirable performance while sticking to EG-Overlay's core vision:
being light weight and out of the way.

SQLite databases *will* take up more hard disk space than a zip compressed
'TaCO' pack, but they are still a single file and the additional disk usage is
much less than the RAM usage it is being traded for.

SQLite Database Structure
-------------------------

Each markerpack is stored in a single SQLite database. For simplicity, the
files should still have the ``.db`` file extension, but there is nothing that
enforces this.

The :lua:mod:`markers.package` module implements the raw data access to the
database.

.. overlay:database:: markerpackdb


Tables
~~~~~~

.. |categorytbl| replace:: :overlay:dbtable:`categories <markerpackdb.categories>`

.. |markertbl| replace:: :overlay:dbtable:`markers <markerpackdb.markers>`

.. |trailtbl| replace:: :overlay:dbtable:`trails <markerpackdb.trails>`

.. overlay:dbtable:: markerpack

    The main table of a markerpack. This contains general information about the
    markerpack and is also used to validate that this database is a markerpack
    when it is loaded.

    **Columns**

    ======= ======= ==========================================================
    Name    Type    Description
    ======= ======= ==========================================================
    version INTEGER The markerpack database version. Currently, the only valid
                    value is ``1``.
    ======= ======= ==========================================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: categories

    Categories are a grouping of markers, trails, and other
    categories.

    Each category has a ``typeid``. This is the ``Name`` attribute from the TaCO
    XML format.

    The ``typeid`` of a category is hierarchical and will contain the full
    'path' of the category. This means that if a top level category with a
    ``typeid`` of 'foo' contains a category 'bar', the second category's
    ``typeid`` will be ``'foo.bar'``.

    **Columns**

    ====== ======= =============================================================
    Name   Type    Description
    ====== ======= =============================================================
    typeid TEXT    **Primary Key**. The typeid of the category.
    parent TEXT    **Foreign Key**. The typeid of this category's parent.
                   ``NULL`` if this category has no parent.
    active BOOL    ``TRUE`` if this category is active, meaning it should be
                   displayed.
    seq    INTEGER A number indicating what order categories should be shown
                   in a UI.
    ====== ======= =============================================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: categoryprops

    Category properties may affect the behavior of the category, all ancestors
    (children, etc.) and any contained markers and trails.

    See :ref:`properties`.

    **Columns**

    ======== ======= ===========================================================
    Name     Type    Description
    ======== ======= ===========================================================
    id       INTEGER **Primary Key**. Internal ID.
    category TEXT    **Foreign Key**. The category ``typeid``.
    proprety TEXT    The property name, ie. ``behavior`` or ``alpha``.
    value    ANY     The property value. This can be any type.
    ======== ======= ===========================================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: markers

    A marker is a location to be displayed within the GW2 scene.

    **Columns**

    ===== ======= =================================================================
    Name  Type    Description
    ===== ======= =================================================================
    id    INTEGER **Primary Key**. Internal ID.
    type  TEXT    **Foreign Key**. The |categorytbl|.typeid this marker belongs to.
    mapid INTEGER The MapID this marker is displayed on.
    ===== ======= =================================================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: markerprops

    Marker properties. Marker properties affect how a marker is shown and other
    behavior. If a marker does not define a property, the value defined on the
    |categorytbl| it belongs to or any parents will take effect instead.

    See :ref:`properties`.

    **Columns**

    ======== ======= ================================
    Name     Type    Description
    ======== ======= ================================
    id       INTEGER **Primary Key**. Internal ID.
    marker   INTEGER **Foreign Key**. |markertbl|.id.
    property TEXT    The property name.
    value    ANY     The property value.
    ======== ======= ================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: trails

    Trails are linear markers showing a route between multiple points.

    **Columns**

    ===== ======= ================================================================
    Name  Type    Description
    ===== ======= ================================================================
    id    INTEGER **Primary Key**. Internal ID.
    type  TEXT    **Foreign Key**. The |categorytbl|.typeid this trail belongs to.
    mapid INTEGER The MapID this marker is displayed on.
    ===== ======= ================================================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: trailprops

    Trail properties. Trail properties affect how a marker is shown and other
    behavior. If a trail does not define a property, the value defined on the
    |categorytbl| it belongs to or any parents will take effect instead.

    See :ref:`properties`.

    **Columns**

    ======== ======= ================================
    Name     Type    Description
    ======== ======= ================================
    id       INTEGER **Primary Key**. Internal ID.
    trail    INTEGER **Foreign Key**. |trailtbl|.id.
    property TEXT    The property name.
    value    ANY     The property value.
    ======== ======= ================================

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: trailcoords

    Trail points.

    **Columns**

    ===== ======= ==============================================================
    Name  Type    Description
    ===== ======= ==============================================================
    id    INTEGER **Primary Key**. Internal ID.
    seq   INTEGER The sequence or order this point occurs at in the trail.
    trail INTEGER **Foreign Key**. |trailtbl|.id.
    x     REAL    X map coordinate, in **meters**.
    Y     REAL    Y map coordinate, in **meters**.
    Z     REAL    Z map coordinate, in **meters**.
    ===== ======= ==============================================================

    .. note::

        Even though GW2 uses inches for map coordinates, markers data is still
        stored in meters, which is what the MumbleLink info reports. This also
        keeps the coordinates consistent with other marker formats.

    .. versionhistory::
        :0.1.0: Added

.. overlay:dbtable:: datafiles

    Data files are binary data that accompanies a markerpack. This is generally
    image files used as textures for markers or trails, but could be any binary
    data.

    **Columns**

    ==== ==== ==================================================================
    Name Type Description
    ==== ==== ==================================================================
    path TEXT **Primary Key**. A relative path identifying the 'file.'
    data BLOB The binary Data
    ==== ==== ==================================================================

    .. versionhistory::
        :0.1.0: Added

.. _properties:

Properties
----------

|categorytbl|, |markertbl|, and |trailtbl| can all have arbitrary properties
assigned to them. The properties documented below are recognized by the
markers module. Other properties can be stored for use by
marker authors or other EG-Overlay modules.

.. important::

    Property names are stored in lower case and the :lua:mod:`markers.package`
    operates on properties in a case insensitive manner. In other words, a
    property named ``name`` is considered the same as ``Name`` or ``NAME``.

displayname
~~~~~~~~~~~~

The name displayed within the UI for a category. This name may also be displayed
on tooltips for markers.

:type: string

.. versionhistory::
    :0.1.0: Added

isseparator
~~~~~~~~~~~

If this value is present and ``1``, the category will be treated as a separator
or header and used for display only. This means users will not be able to
interact with it and any markers or trails assigned to it will be ignored.

:type: integer

.. versionhistory::
    :0.1.0: Added

color
~~~~~

The color tinting of the marker or trail.

:type: string
:default: #FFFFFF

The color value is expected to be a string containing a 24bit integer RGB
color in hexadecimal format preceded by a ``#``. This is the format commonly
used in CSS. Example: ``#FF0000`` for red.

.. versionhistory::
    :0.1.0: Added

alpha
~~~~~

The transparency of a marker or trail. From ``0.0`` to ``1.0``

:type: float
:default: 1.0

.. note::

    This is the *minimum* transparency a marker or trail will be displayed.
    Markers and trails may be displayed more transparent based on other
    settings.

.. versionhistory::
    :0.1.0: Added

xpos
~~~~

X map position, in meters.

:type: float
:default: 0.0

.. versionhistory::
    :0.1.0: Added

.. _ypos:

ypos
~~~~

Y map position, in meters.

:type: float
:default: 0.0

.. versionhistory::
    :0.1.0: Added

zpos
~~~~

Z map position, in meters.

:type: float
:default: 0.0

.. versionhistory::
    :0.1.0: Added

heightoffset
~~~~~~~~~~~~

The distance above or below the given :ref:`ypos` that the marker will
be shown, in meters. This is used to offset the marker so that it does not
cover the item it is marking.

:type: float
:default: 1.5

.. versionhistory::
    :0.1.0: Added

iconsize
~~~~~~~~

A ratio controlling how large the marker icon is drawn. By default, each
marker is displayed 80 inches wide (in map units). An ``iconsize`` of 2
causes the marker to be drawn 160 inches wide.

:type: float
:default: 1.0

.. versionhistory::
    :0.1.0: Added

iconfile
~~~~~~~~

The path identifying the :overlay:dbtable:`datafile <markerpackdb.datafiles>`
that contains the icon that will be displayed for the marker.

:type: string

.. versionhistory::
    :0.1.0: Added

texture
~~~~~~~

The path identifying the :overlay:dbtable:`datafile <markerpackdb.datafiles>`
that contains the texture that will be used to display the trail.

:type: string

.. versionhistory::
    :0.1.0: Added

.. _fadenear:

fadenear
~~~~~~~~

The distance from the player where a marker or trail will begin to fade to
transparent. This value is ignored, and the marker/trail will never fade if it
is below 0.

:type: float
:default: -1.0

.. versionhistory::
    :0.1.0: Added

fadefar
~~~~~~~

The distance from the player where a marker/trail will become completely
transparent. If :ref:`fadenear` is ``-1.0`` or if this value is less than
:ref:`fadenear` it will be ignored and markers/trails will never fade.

:type: float
:default: -1.0

.. versionhistory::
    :0.1.0: Added

minimapvisibility / mapvisibility
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``minimapvisibility`` and ``mapvisibility`` control if a marker/trail is
displayed on the (mini)map.

.. important::

    If either value is ``1`` the marker/trail is displayed on both the minimap
    and map. EG-Overlay does not distinguish between the two.

:type: integer
:default: 1

.. versionhistory::
    :0.1.0: Added

ingamevisibility
~~~~~~~~~~~~~~~~

This value controls if a marker/trail is visible within the 3d game scene.

:type: integer
:default: 1

.. versionhistory::
    :0.1.0: Added

mapdisplaysize
~~~~~~~~~~~~~~

The size a marker should be displayed on the (mini)map, in continent units.
Because this is in continent units, it will be scaled with the map as the player
zooms in/out.

:type: float
:default: 20

.. versionhistory::
    :0.1.0: Added

trailscale
~~~~~~~~~~

A ratio controlling how wide a trail is displayed. A value of ``1.0`` results in
trails drawn 40 inches wide (map units), while ``2.0`` would result in a trail
80 inches wide.

:type: float
:default: 1.0

.. versionhistory::
    :0.1.0: Added

iswall
~~~~~~

Controls how the trail is rendered. Normally trails are drawn so that the
texture is facing up/down. If this value is ``1`` the trail will be drawn
vertically instead.

:type: integer

.. versionhistory::
    :0.1.0: Added


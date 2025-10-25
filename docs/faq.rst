.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

Frequently Asked Questions
==========================

| :ref:`faq-1`
| :ref:`faq-2`
| :ref:`faq-3`
| :ref:`faq-4`
| :ref:`faq-5`
| :ref:`faq-6`
| :ref:`faq-7`

.. _faq-1:

1. What is all this? I just want markers and trails.
----------------------------------------------------

See the :ref:`Markers Quick Start Guide <markers-quick-start>`.

.. _faq-2:

2. How do I install EG-Overlay?
-------------------------------

See :doc:`install`.

.. _faq-3:

3. How do I update to a new version?
------------------------------------

Just download the new version as described in :doc:`install`. You can copy the
new files on top of the existing folder.

.. _faq-4:

4. How do I backup my settings and data?
----------------------------------------

Settings and data are stored within the ``settings`` and ``data`` folders
respectively within the EG-Overlay folder.

You can copy these folders to back up your existing settings and/or data.

.. note::

   Not all modules will store or reference data in these locations. Most notably
   the :overlay:module:`markers` module will load marker packages from any
   location you specify.

   Additionally, any additional Lua modules you have loaded will be in the
   ``Lua`` folder.

.. _faq-5:

5. EG-Overlay is running but I don't see it.
--------------------------------------------

First, verify that Guild Wars 2 is in ``Windowed Fullscreen`` and not
``Fullscreen`` mode. This is on the 'Graphics' tab of the Options screen in game
(press ``F11``).

By default the overlay won't display anything. Each module can have their own
methods of displaying content, but typically they will have an item in the
:overlay:module:`overlay-menu`.

The menu can be shown by pressing ``ctrl-shift-e`` by default.

If you know overlay windows should be visible but aren't, or the menu isn't
showing, try focusing another application, i.e. alt-tab or click on a different
window, and then focus the Guild Wars 2 window again.

.. _faq-6:

6. The overlay is shifted and the bottom is cut off
---------------------------------------------------

The GW2 window does not start off at the proper size when it is first opened,
and if the overlay first sees it in this state it will think the game area is
too small.

Just focus another window, i.e. alt-tab or click on a different window, and then
focus the Guild Wars 2 window again.

This will cause the overlay to recalculate the window size.

.. _faq-7:

7. The key I press on my keyboard doesn't match the key EG-Overlay sees.
------------------------------------------------------------------------

Submit an issue requesting to add your keyboard layout.

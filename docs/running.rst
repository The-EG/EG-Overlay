.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

Running EG-Overlay
==================

.. program:: eg-overlay

EG-Overlay can be started by just running the main program, ``eg-overlay.exe``.
Most users won't need to do anything additional.

Command Line Options
--------------------

.. option:: --debug

    Enable debug logging.

    .. versionhistory::
        :0.3.0: Added

.. option:: --target-win-class <window class name>

    Set the window class that is used to determine the target window. This is
    normally a GW2 window, but it can be set to something else for debugging
    purposes.

    .. versionhistory::
        :0.3.0: Added

.. option:: --script <script path>

    Run a Lua script instead of starting the overlay.

    .. versionhistory::
        :0.3.0: Added

.. option:: --lua-path <path>

    Add the given directory to the Lua module search paths.

    .. versionhistory::
        :0.3.0: Added


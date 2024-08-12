Running EG-Overlay
==================

.. program:: eg-overlay

EG-Overlay can be started by just running the main program, ``eg-overlay.exe``. Most users won't need to do anything additional.

Command Line Options
--------------------

.. option:: --target-win-class <window class name>

    Set the window class that is used to determine the target window. This is normally a GW2 window, but it can be set to something else for debugging purposes.

.. option:: --no-input-hooks

    Do not install input hooks. This is only intended for debugging.

    .. warning::
        The overlay will not function properly with this option, as no mouse or keyboard input will be possible. This is strictly for debugging purposes, as debugging the main thread isn't possible with the hooks installed.

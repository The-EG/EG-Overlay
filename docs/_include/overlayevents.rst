.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

Events
------

.. overlay:event:: startup

    The startup event is sent once before the start of the render thread.
    Modules can use this event to initialize or load data.

    .. versionhistory::
        :0.3.0: Added

.. overlay:event:: update

    Sent once per frame before any drawing has occurred.

    .. versionhistory::
        :0.3.0: Added

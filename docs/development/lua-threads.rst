.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

Lua Thread and Coroutines
=========================

The Lua environment is single threaded and is run on the same thread as the
rendering engine. This means that any call into Lua that blocks or is long
running *can* negatively impact the overlay's frame rate.

However, this does not mean there are no options for concurrency or that long
running tasks *must* negatively impact FPS. Instead, Lua offers coroutines.

Coroutines allow a long running task to yield, or suspend execution to allow
either other Lua functions or the rendering thread to continue.

.. danger::

    While event handlers and callbacks are run as coroutines, the initial run of
    ``autload.lua`` is not. Modules should not have any long running or blocking
    tasks occur during load/definition.

    If modules do need to do such tasks at startup, they should add an event
    handler for the :overlay:event:`startup` event.

Simple Coroutine Setup
----------------------

An event handler that is performing a long running task, especially one with
tight loops, can simply ``coroutine.yield()`` occasionally. This will stop
execution at each ``yield`` and allow other events to continue. After all other
events have either completed or yielded, the event will resume after the
``yield`` until it yields again or completes normally.

Advanced Usage: Lua-side coroutines
-----------------------------------

Coroutines can also be setup directly within Lua itself. This can be used to
monitor some other Lua function that uses coroutines or similar situations.

.. note::

    The function must still ``coroutine.yield`` itself or it will block the
    Lua/render thread. In most cases the function should probably ``yield``
    every time the inner coroutine does.


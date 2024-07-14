Lua Thread and Coroutines
=========================

The Lua environment is single threaded and is run on the same thread as the renderer. This means that any call into Lua that blocks or is long running *can* negatively impact the overlay's framerate.

However, this does not mean there are no options for concurrency or that long running tasks *must* negatively impact FPS. Instead, Lua offers coroutines. Coroutines allow a long running task to yield, or suspend execution to allow either other Lua functions or the rendering thread to continue.

.. danger::
    While event handlers and callbacks are run as coroutines, the initial run of ``autload.lua`` is not. Modules should not have any long running or blocking tasks occur during load/definition. If modules do need to do such tasks at startup, they should add an event handler for the :overlay:event:`startup` event.

The flow of the Lua/render thread is as follows:

.. mermaid::

    flowchart TD
        start[Render Thread Start] --> startup('startup' event queued<br>&<br>event handlers processed);
        startup                    --> beginLoop(Begin Render Loop);
        beginLoop                  --> checkFG(Check Foreground Window);
        checkFG                    --> runIntEvents(Run Internal Lua Events);
        runIntEvents               --> resumeCo(Resume Pending Lua Coroutines);
        resumeCo                   --> queueUpdate(Queue 'update' event);
        queueUpdate                --> runLuaEvents(Run Lua Events);
        runLuaEvents               --> isShown{Is overlay visible?};

        isShown -->|No| coroutinesPending{Coroutines still pending?};

        coroutinesPending -->|Yes| checkFG;
        coroutinesPending -->|No| sleep(Sleep 100 ms) --> checkFG;

        isShown -->|Yes| clearFB(Clear Framebuffer);
        clearFB          --> draw3D(Draw 3D Scene);
        draw3D           --> drawUI(Draw UI);
        drawUI           --> swapBuffers(Swap Buffers);
        swapBuffers      --> isFrameTimeUnder{Is frame time under 33ms?};

        isFrameTimeUnder -->|No| isOverlayClosing{Application closing?};

        isFrameTimeUnder -->|Yes| coroutinesPending2{Coroutines Still pending?};

        coroutinesPending2 --> |No| sleep2("Sleep until frame time >= 33ms (30 FPS)") --> isOverlayClosing;
        coroutinesPending2 --> |Yes| resumeCoroutines2(Resume Pending Lua Coroutines);

        resumeCoroutines2 --> isFrameTimeUnder;

        isOverlayClosing -->|No| checkFG;

        isOverlayClosing -->|Yes| endLoop(End Render Loop);

        endLoop --> shutdown('shutdown' event queued<br>&<br>event handlers processed);
        shutdown --> endThread[Render Thread End];

Simple Coroutine Setup
----------------------

An event handler that is performing a long running task, especially one with tight loops, can simply ``coroutine.yield()`` occasionally. This will stop execution at each ``yield`` and allow other events to continue. After all other events have either completed or yielded, the event will resume after the ``yield`` until it yields again or completes normally.

Advanced Usage: Lua-side coroutines
-----------------------------------

Coroutines can also be setup directly within Lua itself. This can be used to monitor some other Lua function that uses coroutines or similar situations. Note, the function must still ``coroutine.yield`` itself or it will block the Lua/render thread. In most cases the function should probably ``yield`` everytime the inner coroutine does.

An example, with the Pathing module; ``pathing.load_zip`` yields after loading each zipped file, returning the number of files processed and the total number of files. This can be used to monitor the progress:

.. code:: lua

    local pathing = require 'pathing'
    local logger = require 'logger'
    local overlay = require 'eg-overlay'

    local co_test = {}

    co_test.log = logger.get("coroutine-test")

    function co_test.startup()

        local start_time = overlay.time()
        local run_load_zip = coroutine.create(pathing.load_zip)

        while coroutine.status(run_load_zip)~='dead' do
            local ok, f, total = coroutine.resume(run_load_zip, "D:\\Downloads\\tw_ALL_IN_ONE.taco")
            if not ok then
                coroutine.close(run_load_zip)
                error(f)
                return
            end
            if f then 
                co_test.log:info("Loading pack, %.1f%% complete.", (f/total)*100.0)
                coroutine.yield()
            else
                co_test.log:info("Pack loaded.")
            end
        end
        
        coroutine.close(run_load_zip)

        local end_time = overlay.time()
        co_test.log:info("Loading took %.04f seconds, %d POIs, %d trails", end_time - start_time, #pathing.pois, #pathing.trails)
    end


    overlay.add_event_handler('startup', co_test.startup)

    return co_test

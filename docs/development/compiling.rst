Building EG-Overlay From Source
================================

Most users should use pre-built binaries, but EG-Overlay can be built from
source for development or debugging purposes.

Visual Studio
-------------

EG-Overlay is built using the MSVC compiler. The most straightforward way to
install a functional compiler toolchain is to install Microsoft Visual Studio.

.. note::
    The Visual Studio IDE itself does not need to be used to build EG-Overlay,
    but installing it is the easiest way to get all needed components installed.
    In fact, the author of EG-Overlay uses Neovim and command line tools for
    development.

    It may be possible to use Build Tools for Visual Studio instead, but that is
    outside the scope of this documentation.

Install Visual Studio 2022 Community from
`<https://visualstudio.microsoft.com/downloads/>`_.

During installation select the 'Desktop development with C++' workload.

CMake 
-----

A few of the libraries that EG-Overlay depends on are built with CMake. It can
be installed with Visual Studio.

After Visual Studio is installed, open the Visual Studio Installer from the
start menu. Select 'Modify' on the Visual Studio Community 2022 installation.

Go to the 'Individual Components' tab, type ``cmake`` into the search box and
verify that 'C++ CMake tools for Windows' is checked.

Click 'Close' and wait for any changes to complete, and then close the installer
window.

.. warning::
    It is also possible to install CMake independently of Visual Studio, however
    the method detailed here ensures that CMake can find the MSVC compiler paths
    properly.

Meson and Ninja
---------------

EG-Overlay uses the Meson Build system and Ninja. Install both by using the
`Meson and Ninja MSI Installer <https://mesonbuild.com/Getting-meson.html
#installing-meson-and-ninja-with-the-msi-installer>`_.

Sources
-------

Download the EG-Overlay sources from `<https://github.com/The-EG/EG-Overlay>`_.
Sources for a particular release can be downloaded or the latest from
`<https://github.com/The-EG/EG-Overlay/archive/refs/heads/main.zip>`_.

Alternatively, git can be used to clone the repository:

.. code-block:: powershell

    PS > git clone https://github.com/The-EG/EG-Overlay.git


Command Line Build
------------------

EG-Overlay can be built from the command line using either CMD or PowerShell.

1. Open either ``Developer Command Prompt for VS 2022`` or
   ``Developer PowerShell For VS 2022`` from the start menu.
2. Navigate to the sources downloaded above.
3. Run ``meson setup builddir .``
4. Run ``meson compile -C builddir``

   .. note::
        By default a debug build is configured. You can request a release build
        using the command line argument ``--buildtype release``. For example,
        ``meson setup builddir . --buildtype release``.

At this point, EG-Overlay is in the ``builddir/src`` folder.

.. _vs-code:

Visual Studio Code
------------------

Meson integrates lightly with Visual Studio Code. 

First, install the Meson extension in Visual Studio Code. Then open the folder
containing the sources downloaded previously.

To configure the build, open the command pallet (``ctrl-shift-p``) and select
'Meson: Reconfigure'. Type 'Meson' in the search box if that option is not
visible.

After configuration, build by opening the command pallet again and selecting
'Meson: Build' and then 'Build All Targets'.

Debugging
---------

CDB
~~~

CDB is Microsoft's command line debugger. It is quite powerful, but has a steep
learning curve; however it is useful for 'light' debugging.

CDB can be installed as part of 'Debugging Tools for Windows' in the Windows SDK.
See 'As a standalone toolset' at `<https://learn.microsoft.com/en-us/windows-
hardware/drivers/debugger/debugger-download-tools>`_ for details.

The included ``scripts\debug.ps1`` will start CDB as long as it is installed in
the default location.

``scripts\debug.ps1`` can be given arguments that control how EG-Overlay is run:

- ``terminal`` : Overlay Windows Terminal instead of GuildWars 2
- ``notepad`` : Overlay Notepad instead of GuildWars 2
- ``no-hooks`` : Do not install mouse/keyboard hooks
- ``terminal-no-hooks`` : combine ``terminal`` and ``no-hooks``
- ``notepad-no-hooks`` : combine ``notepad`` and ``no-hooks``
- ``script <scriptpath.lua>`` : run the lua script at ``<scriptpath.lua>``

For example, to run EG-Overlay in the debugger and overlay Notepad instead of
GW2: ``PS > .\scripts\debug.ps1 notepad``

WinDbgX
~~~~~~~

WinDbg is a friendly UI built on top of CDB. All of the commands from CDB work
inside of WinDbg, however most tasks can also be performed through the UI as
well. WinDbgx is an updated version that is available in the Windows App Store.

WinDbgX can be installed directly from the Windows App Store (it is called
WinDbg) at `<https://apps.microsoft.com/detail/9pgjgd53tn86?launch=true
&mode=mini&hl=en-us&gl=US>`_.

The script ``scripts\start-windbgx.ps1`` will start a debugging session in
WinDbgx and takes the same options as ``scripts\debug.ps1`` described above.


Debugging in VSCode
~~~~~~~~~~~~~~~~~~~

The EG-Overlay sources come with the following VSCode debug configurations:

* Debug: run EG-Overlay normally, but in debug mode.
* Debug (Terminal Window): run EG-Overlay with an option that causes it to
  overlay the Windows Terminal instead of GW2. This is useful for debugging
  without running the game.
* Update static database: Run ``scripts/updatestaticdb.lua`` instead of running
  the full overlay.

These debug configurations can be selected on the 'Run and Debug' tab of VSCode
and the current configuration can be run by pressing F5.

While debugging in VSCode, the log will also be output to the debug log.

Dependencies
------------

All dependencies are either bundled with the source code or automatically
handled by Meson. They are listed below for reference only.

* FreeType 2.13.2
* zlib 1.3
* libxml 2.13.1
* Lua 5.4.6
* Jansson 2.14
* sqlite 3.46
* Inter font
* CascadiaCode font (also includes CascadiaMono)

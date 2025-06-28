.. EG-Overlay
.. Copyright (c) 2025 Taylor Talkington
.. SPDX-License-Identifier: MIT

Building EG-Overlay From Source
================================

Most users should use pre-built binaries, but EG-Overlay can be built from
source for development or debugging purposes.

Visual Studio
-------------

While EG-Overlay is written in Rust, it still requires the MSVC compiler. The most
straightforward way to install a functional compiler toolchain with the required
dependencies is to install Microsoft Visual Studio.

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

Rust
----

Rust can be installed with rustup from `<https://www.rust-lang.org/tools/install>`_.

Default options should be sufficient.

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

EG-Overlay can be built from the command line using PowerShell.

1. Open ``Developer PowerShell For VS 2022`` from the start menu.
2. Navigate to the sources downloaded above.
3. Run ``.\scripts\meson-setup.ps1``
4. Run ``meson compile -C builddir``

   .. note::

        By default a debug build is configured.

        You can request a release build by adding ``release`` to step #3 above.

        For example ``.\scripts\meson-setup.ps1 release``.

At this point, EG-Overlay is in the ``builddir/src`` folder.

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


Dependencies
------------

All dependencies are either bundled with the source code or automatically
handled by Meson. They are listed below for reference only.

* windows 0.61 (Rust)
* serde (Rust)
* serde_json (Rust)
* xml 0.8 (Rust)
* FreeType 2.13.2
* zlib 1.3
* Lua 5.4.7
* sqlite 3.46
* Inter font
* CascadiaCode font (also includes CascadiaMono)
* Google Material Design Icons font

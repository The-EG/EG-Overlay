-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
dialogs
=======

.. code:: lua

    local dialogs = require 'dialogs'

.. lua:module:: dialogs

The :lua:mod:`dialogs` module provides standard UI dialogs.

Classes
-------

]]--

local shell = require 'shell'
local ui = require 'ui'

local M = {}

--[[ RST
.. lua:class:: FileDialog

    The :lua:class:`FileDialog` class provides a window that allows users to
    select a file or folder for opening or saving.

    .. seealso::

        See the :lua:meth:`new` method for creating a new :lua:class:`FileDialog`.

    .. code-block:: lua
        :caption: Example

        local dialogs = require 'dialogs'

        local of = dialogs.FileDialog.new('open-file')

        -- specify a function to be called when the user confirms
        of.confirmcallback = function(path)
            -- do something with the file at path
        end

        -- only show TacO marker packs
        of.filefilters = {'.taco'}

        of:show()

    .. lua:attribute:: confirmcallback: function

        A function that will be called whenever the confirmation button ('Open'
        or 'Save') is pressed by the user.

        This function will be called with a single argument: the full path of
        the selected file/folder.

        .. versionhistory::
            :0.3.0: Added

    .. lua:attribute:: filefilters: sequence

        A list of file extensions to display within the dialog.

        .. note::

            The extension should include the leading ``.``.

        .. important::

            To show all files, set this attribute to ``nil``. An empty table
            will not show any files.

        .. versionhistory::
            :0.3.0: Added


]]--
M.FileDialog = {}
M.FileDialog.__index = M.FileDialog
M.FileDialog._itemicons = {
    ['other']           = ui.iconcodepoint('unknown_document'),
    ['drive-removable'] = ui.iconcodepoint('security_key'),
    ['drive-fixed']     = ui.iconcodepoint('hard_drive'),
    ['drive-cdrom']     = ui.iconcodepoint('album'),
    ['drive-ramdisk']   = ui.iconcodepoint('memory_alt'),
    ['link-other']      = ui.iconcodepoint('unknown_document'),
    ['link-network']    = ui.iconcodepoint('smb_share'),
    ['link-local']      = ui.iconcodepoint('article_shortcut'),
    ['folder']          = ui.iconcodepoint('folder'),
    ['file']            = ui.iconcodepoint('draft'),
}
M.FileDialog._itemiconcolors = {
    ['other']           = 0x9e9e9edd,
    ['drive-removable'] = 0x757575ff,
    ['drive-fixed']     = 0x757575ff,
    ['drive-cdrom']     = 0x757575ff,
    ['drive-ramdisk']   = 0x757575ff,
    ['link-other']      = 0x7596c7ff,
    ['link-network']    = 0x7596c7ff,
    ['link-local']      = 0x7596c7ff,
    ['folder']          = 0xc7ac75ff,
    ['file']            = 0xb5bf9dff,
}

function M.FileDialog.navbtn(icon)
    local btn = ui.button()
    local box = ui.box('vertical')
    local lbl = ui.text(ui.iconcodepoint(icon), ui.color('text'), ui.fonts.icon:tosizeperc(1.25))

    box:paddingleft(5)
    box:paddingright(5)
    box:paddingtop(5)
    box:paddingbottom(5)
    box:pushback(lbl, 'middle', false)
    btn:child(box)

    return btn
end

--[[ RST
    .. lua:function:: new(mode)

        Create a new :lua:class:`FileDialog`.

        :param string mode: The dialog mode. See below.
        :rtype: FileDialog

        **Dialog Mode**
        ``mode`` controls how the :lua:class:`FileDialog` will behave and must
        be one of the following values:

        - ``'open-file'``
        - ``'save-file'``

        .. warning::

            ``'save-file'`` dialog will allow users to specify existing files.
            Module authors should confirm that the user wishes to overwrite existing
            files if they are chosen.

        .. versionhistory::
            :0.3.0: Added
]]--
function M.FileDialog.new(mode)
    if mode ~= 'open-file' and mode ~= 'save-file' then
        error("mode must be open-file or save-file.", 1)
    end

    local of = {}

    of.filefilters = nil

    local title = 'File Dialog'
    local confirmlbl = 'Confirm'

    if mode == 'open-file' then
        title = 'Open File'
        confirmlbl = 'Open'
    elseif mode == 'save-file' then
        title = 'Save File'
        confirmlbl = 'Save'
    end

    of.mode = mode
    of.win = ui.window(title)
    of.outerbox = ui.box('vertical')
    of.navbtnbox = ui.box('horizontal')
    of.itemscroll = ui.scrollview()
    of.itembox = ui.box('vertical')
    of.buttonbox = ui.box('horizontal')

    of.confirmbtn = {
        btn = ui.button(),
        box = ui.box('vertical'),
        txt = ui.text(confirmlbl, ui.color('text'), ui.fonts.regular),
    }
    of.confirmbtn.box:paddingleft(10)
    of.confirmbtn.box:paddingright(10)
    of.confirmbtn.box:paddingtop(5)
    of.confirmbtn.box:paddingbottom(5)
    of.confirmbtn.btn:child(of.confirmbtn.box)
    of.confirmbtn.box:pushback(of.confirmbtn.txt, 'middle', false)

    of.cancelbtn = {
        btn = ui.button(),
        box = ui.box('vertical'),
        txt = ui.text('Cancel', ui.color('text'), ui.fonts.regular),
    }
    of.cancelbtn.box:paddingleft(10)
    of.cancelbtn.box:paddingright(10)
    of.cancelbtn.box:paddingtop(5)
    of.cancelbtn.box:paddingbottom(5)
    of.cancelbtn.btn:child(of.cancelbtn.box)
    of.cancelbtn.box:pushback(of.cancelbtn.txt, 'middle', false)

    of.desktopbtn = M.FileDialog.navbtn('desktop_landscape')
    of.thispcbtn = M.FileDialog.navbtn('computer')
    of.previousbtn = M.FileDialog.navbtn('arrow_upward')

    of.pathtxt = ui.text('(path)', ui.color('text'), ui.fonts.regular:tosizeperc(1.25))

    of.selectedtxt = ui.entry(ui.fonts.regular)

    if mode ~= 'save-file' then
        of.selectedtxt:readonly(true)
    end

    of.win:resizable(true)
    of.win:x(50)
    of.win:y(50)
    of.win:width(400)
    of.win:height(300)
    of.win:child(of.outerbox)

    of.outerbox:spacing(2)
    of.outerbox:paddingleft(5)
    of.outerbox:paddingright(5)
    of.outerbox:paddingtop(5)
    of.outerbox:paddingbottom(5)

    of.outerbox:pushback(of.navbtnbox, 'start', false)
    of.outerbox:pushback(of.pathtxt, 'start', false)
    of.outerbox:pushback(ui.separator('horizontal'), 'fill', false)
    of.outerbox:pushback(of.itemscroll, 'fill', true)
    of.outerbox:pushback(ui.separator('horizontal'), 'fill', false)
    of.outerbox:pushback(of.selectedtxt, 'fill', false)
    of.outerbox:pushback(of.buttonbox, 'end', false)

    of.navbtnbox:spacing(5)
    of.navbtnbox:pushback(of.desktopbtn, 'middle', false)
    of.navbtnbox:pushback(of.thispcbtn, 'middle', false)
    of.navbtnbox:pushback(of.previousbtn, 'middle', false)

    of.itembox:paddingtop(2)
    of.itembox:paddingright(2)
    of.itembox:paddingbottom(2)
    of.itembox:spacing(2)
    of.itemscroll:child(of.itembox)

    of.buttonbox:spacing(5)
    of.buttonbox:pushback(of.confirmbtn.btn, 'middle', false)
    of.buttonbox:pushback(of.cancelbtn.btn, 'middle', false)

    of._selected = nil

    setmetatable(of, M.FileDialog)

    of.confirmbtn.btn:addeventhandler(function() of:onconfirm() end, 'click-left')
    of.cancelbtn.btn:addeventhandler(function() of:oncancel() end, 'click-left')
    of.desktopbtn:addeventhandler(function() of:gotodesktop() end, 'click-left')
    of.thispcbtn:addeventhandler(function() of:gotothispc() end, 'click-left')
    of.previousbtn:addeventhandler(function() of:gotoparent() end, 'click-left')
    of:gotothispc()

    return of
end

--[[ RST
    .. lua:method:: gotodesktop()

        Navigate this file dialog to the 'Desktop' virtual folder.

        .. versionhistory::
            :0.3.0: Added
]]--
function M.FileDialog:gotodesktop()
    self:gotoitem(shell.getshellitem('desktop'))
end

--[[ RST
    .. lua:method:: gotothispc()

        Navigate this file dialog to the 'This PC' virtual folder.

        .. versionhistory::
            :0.3.0: Added
]]--
function M.FileDialog:gotothispc()
    self:gotoitem(shell.getshellitem('this-pc'))
end

function M.FileDialog:gotoparent()
    local p = self.shellitem:parent()

    if p then
        self:gotoitem(p)
    end
end

function M.FileDialog:itembtn(item)
    local btn = ui.button()
    local box = ui.box('horizontal')
    local iconcodepoint = self._itemicons[item:type()]
    local iconcolor = self._itemiconcolors[item:type()]

    local icon = ui.text(iconcodepoint, iconcolor, ui.fonts.icon:tosizeperc(1.5))
    local label = ui.text(item:displayname(), ui.color('text'), ui.fonts.regular)

    box:spacing(5)
    box:paddingleft(10)
    box:paddingright(10)
    box:paddingtop(2)
    box:paddingbottom(2)

    btn:child(box)
    box:pushback(icon, 'middle', false)
    box:pushback(label, 'middle', false)

    btn:borderwidth(0)
    btn:bgcolor(0x00000000)

    return btn
end

--[[ RST
    .. lua:method:: gotopath(path)

        Navigate this file dialog to the given path.

        :param string path: A full path to a folder.

        .. versionhistory::
            :0.3.0: Added
]]--
function M.FileDialog:gotopath(path)
    local pathitem = shell.getshellitem('path', path)

    if pathitem then self:gotoitem(pathitem) end
end

function M.FileDialog:gotoitem(newitem)
    self.shellitem = newitem

    local itype = newitem:type()

    local itempath = newitem:path()
    if itempath and newitem:filetype() then
        self.pathtxt:text(itempath)
    else
        self.pathtxt:text(newitem:displayname())
    end

    self.selectedtxt:text('')
    self._selected = nil
    self.itembox:clear()
    self.itemscroll:scrolly(0.0)

    coroutine.yield()

    self.itembtns = {}

    local children = self.shellitem:children()

    if self.filefilters then
        local filtered = {}
        for i, c in ipairs(children) do
            local filetype = c:filetype()
            local include = false

            if not filetype or filetype == 'Directory' then include = true
            else
                for i, f in ipairs(self.filefilters) do
                    if f==filetype then
                        include = true
                        goto endfilters
                    end
                end
                ::endfilters::
            end

            if include then
                table.insert(filtered, c)
            end
        end

        children = filtered
    end


    table.sort(children, function(a, b)
        local atype = a:type()
        local btype = b:type()

        local aisfolder = atype == 'folder' or atype == 'other' or atype:find('drive-')==1
        local bisfolder = btype == 'folder' or btype == 'other' or btype:find('drive-')==1

        if aisfolder and not bisfolder then
            return true
        end
        if bisfolder and not aisfolder then
            return false
        end

        return a:name():lower() < b:name():lower()
    end)

    for i, c in ipairs(children) do
        local cbtn = self:itembtn(c)

        self.itembox:pushback(cbtn, 'fill', false)

        if c:type()~='file' then
            cbtn:addeventhandler(function()
                self:gotoitem(c)
            end, 'click-left')
        end

        if (self.mode == 'open-file' or self.mode == 'save-file') and c:type() =='file' then
            cbtn:addeventhandler(function()
                self:selectitem(c)
            end, 'click-left')
        end

        self.itembtns[c:displayname()] = cbtn
    end
end

function M.FileDialog:selectitem(item)
    for p, btn in pairs(self.itembtns) do
        btn:bgcolor(0x00000000)
    end

    self._selected = item:path()
    self.selectedtxt:text(item:displayname())

    self.itembtns[item:displayname()]:bgcolor(ui.color('buttonBG'))
end

--[[ RST
    .. lua:method:: show()

        Show this file dialog.

        The dialog will be shown until the user presses either the confirmation
        or cancel buttons.

        On confirmation, the :lua:attr:`confirmcallback <dialogs.FileDialog.confirmcallback>`
        function will be called if set.

        .. versionhistory::
            :0.3.0: Added
]]--
function M.FileDialog:show()
    self._selected = nil
    self.selectedtxt:text('')
    self.win:show()
end

function M.FileDialog:onconfirm()
    if self.mode == 'open-file' and not self._selected then return end

    if self.mode == 'save-file' then
        if self.shellitem:type() == 'other' then return end

        if self.selectedtxt:text() == '' then return end
    end

    self.win:hide()
    if self.confirmcallback then
        if self.mode == 'open-file' then self.confirmcallback(self._selected) end

        if self.mode == 'save-file' then
            local folder = self.shellitem:path()

            if self.shellitem:type() == 'folder' then folder = folder .. '\\' end

            self.confirmcallback(folder .. self.selectedtxt:text())
        end
    end
end

function M.FileDialog:oncancel()
    self.win:hide()
end

--[[ RST
.. lua:class:: MessageDialog

    A simple dialog that displays a message and optional icon with an 'OK' button.

    .. seealso::

        See the :lua:meth:`new` method for creating a new :lua:class:`MessageDialog`

    .. code-block:: lua
        :caption: Example

        local dialogs = require 'dialogs'

        local d = dialogs.MessageDialog.new('Test Dialog','This is a test!','feedback')

        d:show()
]]--
M.MessageDialog = {}
M.MessageDialog.__index = M.MessageDialog

--[[ RST
    .. lua:method:: new(title, message[, icon])

        :param string title: The dialog title.
        :param string message: A message to display. This can include newline
            (``'\n'``) characters to display multiple lines of text.
        :param string icon: (Optional) An icon name. If absent, no icon is displayed.
        :rtype: MessageDialog

        .. versionhistory::
            :0.3.0: Added
]]--
function M.MessageDialog.new(title, message, icon)
    local d = {}

    d.window = ui.window(title)
    d.box = ui.box('vertical')
    d.grid = ui.grid(2, 2)

    d.window:child(d.box)
    d.box:pushback(d.grid, 'fill', false)
    d.box:paddingleft(10)
    d.box:paddingright(10)
    d.box:paddingbottom(5)
    d.box:paddingtop(5)

    d.grid:colspacing(10)
    d.grid:rowspacing(5)

    if icon then
        local iconcp = ui.iconcodepoint(icon)
        if iconcp then
            d.icon = ui.text(iconcp, ui.color('text'), ui.fonts.icon:tosizeperc(2.5))
            d.grid:attach(d.icon, 1, 1, 1, 1, 'middle', 'middle')
        end
    end

    d.message = ui.text(message, ui.color('text'), ui.fonts.regular)
    d.grid:attach(d.message, 1, 2, 1, 1, 'start', 'middle')

    d.okbtn = ui.button()
    local box = ui.box('vertical')
    d.okbtn:child(box)
    box:paddingleft(10)
    box:paddingright(10)
    box:paddingtop(5)
    box:paddingbottom(5)

    box:pushback(ui.text('Ok', ui.color('text'), ui.fonts.regular), 'middle', false)

    d.grid:attach(d.okbtn, 2, 1, 1, 2, 'middle', 'middle')

    setmetatable(d, M.MessageDialog)

    d.okbtn:addeventhandler(function() d:onok() end, 'click-left')

    return d
end

function M.MessageDialog:onok()
    self._okclicked = true
    self.window:hide()
end

--[[ RST
    .. lua:method:: show([wait])

        :param boolean wait: (Optional) If ``true``, this method will not return
            until the OK button is pressed and the dialog is closed.

        .. versionhistory::
            :0.3.0: Added
]]--
function M.MessageDialog:show(wait)
    self.window:updatesize()

    local win_width = self.window:width()
    local win_height = self.window:height()

    local ow, oh = ui.overlaysize()

    local wx = math.floor((ow / 2.0) - (win_width / 2.0))
    local wy = math.floor((oh / 2.0) - (win_height / 2.0))

    self.window:position(wx, wy)

    self._okclicked = false
    self.window:show()

    if wait then
        while not self._okclicked do coroutine.yield() end
    end
end

return M

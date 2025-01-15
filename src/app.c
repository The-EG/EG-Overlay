#include <windows.h>
#include <windowsx.h>
#include <jansson.h>

#include "eg-overlay.h"
#include "githash.h"

#include "app.h"
#include "logging/logger.h"
#include "logging/console-sink.h"
#include "logging/file-sink.h"
#include "logging/event-sink.h"
#include "logging/dbg-sink.h"
#include <stdlib.h>

#include "dx.h"

#include "ui/ui.h"
#include "ui/menu.h"
#include "ui/text.h"
#include "ui/rect.h"

#include "settings.h"

#include "res/icons.h"
#include "lua-manager.h"

#include "web-request.h"
#include "mumble-link.h"
#include "zip.h"
#include "utils.h"

#include "xml.h"

#include "lua-sqlite.h"
#include "lua-json.h"
#include "lua-dx.h"

#include <time.h>
#include <stdio.h>
#include <sys/timeb.h>

#define OVERLAY_WIN_CLASS "EG-Overlay Window"

#define WM_SYSTRAYEVENT (WM_APP + 1)
#define WM_SYSTRAYQUIT  (WM_APP + 2)
#define WM_SYSTRAYLOG   (WM_APP + 3)
#define WM_SYSTRAYDOCS  (WM_APP + 4)
#define WM_APPEXIT      (WM_APP + 5)

typedef struct {
    logger_t *log;

    HINSTANCE inst;
    HWND win_hwnd;
    HWND target_hwnd;
    HMENU sys_tray_menu;

    int running;
    int visible;

    settings_t *settings;

    const char *target_win_class;

    const char *runscript;

    uint64_t app_start_time;

    int mouse_ldrag_target;
    int mouse_rdrag_target;
} app_t;

app_t *app = NULL;

void app_run_script();

LRESULT CALLBACK winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        break;
    case WM_APPEXIT:
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        dx_resize(app->win_hwnd);
        break;
    case WM_SYSTRAYEVENT:
        if (LOWORD(lParam)==WM_CONTEXTMENU) {
            // if the window isn't foreground the menu will not close if the user clicks out of it
            // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu
            SetForegroundWindow(app->win_hwnd); 
            int systraycmd = TrackPopupMenu(
                app->sys_tray_menu,
                TPM_RETURNCMD,
                GET_X_LPARAM(wParam),
                GET_Y_LPARAM(wParam),
                0,
                app->win_hwnd,
                NULL
            );
            switch(systraycmd) {
            case WM_SYSTRAYQUIT:
                logger_debug(app->log,"Quit selected.");
                DestroyWindow(hWnd);
                break;
            case WM_SYSTRAYLOG:
                logger_debug(app->log, "Opening log file...");
                ShellExecute(NULL, "open", "eg-overlay.log", NULL, NULL, SW_SHOWNORMAL);
                break;
            case WM_SYSTRAYDOCS:
                logger_debug(app->log, "Opening documentation...");
                ShellExecute(NULL, "open", "https://the-eg.github.io/EG-Overlay", NULL, NULL, SW_SHOWNORMAL);
                break;
            default:
                logger_error(app->log, "Unknown system tray menu command: %d", systraycmd);
            }

            PostMessage(app->win_hwnd, WM_NULL, 0, 0);
        }
        break;
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

void app_register_win_class() {
    WNDCLASSEX wc = {0};

    wc.lpfnWndProc = &winproc;
    wc.hInstance = app->inst;
    wc.lpszClassName = OVERLAY_WIN_CLASS;
    wc.cbWndExtra = 0;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.cbSize = sizeof(WNDCLASSEX);

    if (RegisterClassEx(&wc)==0) {
        logger_error(app->log, "Failed to register window class.");
        exit(-1);
    }
}

void app_create_window() {
    app->win_hwnd = CreateWindowEx(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        OVERLAY_WIN_CLASS,
        "EG-Overlay",
        WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        NULL,
        NULL,
        app->inst,
        NULL
    );

    if (!app->win_hwnd) {
        logger_error(app->log, "Couldn't create window.");
        exit(-1);
    }
}

char *numbers[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

char *letters[] = {
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"
};

LRESULT CALLBACK keyboard_hook_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    HWND fg_win = GetForegroundWindow();
    if (
        nCode < 0 ||
        !IsWindowVisible(app->win_hwnd) ||
        fg_win!=app->target_hwnd
    ) return CallNextHookEx(NULL, nCode, wParam, lParam);

    // For some reason GetKeyboardState doesn't always return an updated result.
    // Calling GetKeyState first flushes the state. WTF Windows?    
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⣤⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⠀⢀⣄⡀⠀⠀⢀⣴⣾⣿⣿⣿⣿⣿⣷⣦⣄⡀⠀⠀⣤⣟⠛⠋⠙⢷⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡴⠿⣫⠟⠛⢋⣧⣤⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣼⡯⣉⠉⠓⢦⣀⠉⢢⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⡴⠋⠀⣠⠖⠉⢋⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠑⢄⠀⠈⢳⡀⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⢿⠏⠀⡴⠊⠁⢀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢿⢦⣀⢳⠀⠀⢣⠸⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⡞⢀⠞⠀⠀⣴⣿⡿⡿⢿⣿⡟⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣄⣀⠀⠉⠃⠀⠀⠳⢽⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⠏⣠⠋⠀⢀⡴⠛⠁⢀⣨⣾⡿⣇⠀⠛⠛⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢲⡀⠀⠀⠀⠀⠹⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠏⠁⠀⠀⠀⠴⠋⠀⢀⣴⣿⣿⠟⠀⠈⣰⣦⣀⢀⣈⣿⠟⡿⢿⣿⠏⣈⣟⢻⣿⣿⣿⡀⠳⡄⠀⠀⠀⠀⠙⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠏⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⠃⠀⢀⡄⠉⣉⠿⠛⠧⣞⣀⣧⡜⢛⣛⣻⣏⣀⢻⣿⣿⠃⠣⡇⠀⠀⠀⠀⠀⠘⢆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠏⠀⠀⠀⠀⠀⠀⢠⣾⣿⣿⣿⡏⠀⠀⠎⠀⠽⢟⣻⣟⡆⣸⠆⠸⡄⢻⣛⣛⠃⠘⢸⣿⡿⠀⠀⠸⡀⠀⠀⠀⠀⠀⠈⠳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⣠⠏⠀⠀⠀⠀⠀⠀⢀⡞⢹⣿⣿⣿⡇⠀⠀⠀⠀⠀⠈⢁⣠⢤⡁⠀⠀⢈⢦⡀⠀⠀⠀⢸⣿⠇⠀⠀⠀⠳⣄⠀⠀⠀⠀⠀⠀⠙⢦⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⣠⠊⠁⠀⠀⠀⠀⠀⠀⢀⡞⠀⠈⣿⠙⠛⣇⠀⠀⠀⠀⠀⠀⠉⢀⡼⠇⠀⠀⠘⢦⡁⠀⠀⠀⢸⣟⠀⠀⠀⠀⠀⠀⠙⢦⠀⠀⠀⠀⠀⠀⠱⣄⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⡴⣯⡟⠁⠀⠀⠀⠀⠀⢀⡤⠖⠋⠀⠀⠀⠸⡎⠿⠏⠀⠀⠀⠀⠀⠀⠀⠚⠓⠚⠣⠴⠚⠛⣃⠀⠀⠀⣺⡏⠀⠀⠀⠀⠀⠀⠀⠈⢣⠀⠀⠀⠀⠀⠀⠈⠳⠀⠀⠀⠀⠀
    // ⠀⠀⠘⠀⢈⡏⠀⠀⠀⠀⠀⢀⡞⠀⠀⠀⠀⠀⠀⠀⢻⣄⣠⡄⠀⠀⠀⠀⠀⠀⣴⠋⣠⣤⣤⣄⠀⠘⠆⠀⠀⡟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⢣⣀⣀⣀⣀⣤⣤⣤⠬⠖⠀⠀⠀
    // ⠤⠀⠀⠀⢸⡀⠀⠀⠀⠀⢠⠎⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⢿⡇⠀⠀⠀⠀⠀⠀⠁⠾⣗⠓⠒⢚⡗⠀⠀⠀⡼⠙⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠱
    // ⠀⠀⠀⠀⠀⠉⠓⠒⠲⢰⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⡎⢣⡀⠀⠀⠀⠀⠀⠀⠀⠀⢉⣉⠁⠀⠀⠀⣼⠁⠀⠈⠳⣄⢀⡴⠒⠦⣄⣀⣀⣀⣽⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣧⣄⣀⡤⠶⠤⣴⠒⠒⠒⠋⠁⢸⠁⠀⠙⢦⡀⠀⠀⠀⠀⠀⠘⠉⠈⠙⠂⢀⠞⢸⡆⠀⠀⠀⡼⠉⠀⠀⠀⠀⠀⠁⠀⠘⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠈⠙⠂⠀⠀⠀⢻⡀⠀⠀⠀⠈⠻⡒⠦⠤⢄⣀⣀⠀⣀⠴⠋⢀⡼⢧⠀⠀⣸⠁⠀⠀⠀⠀⠸⡀⠀⠀⠀⢳⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⡀⠀⠀⠀⢧⡀⠀⠀⠀⠀⣉⣀⣀⠴⢫⠇⠀⠙⠲⠧⡄⠀⠀⠀⠀⠀⢣⠀⠀⠀⠈⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⢀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠲⢤⣀⣸⡉⠉⠉⠉⠉⠉⠀⠀⠀⡼⠀⠀⠀⠀⠀⡇⠀⠀⠀⠀⠀⠘⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠇⠀⠀⠀⠀⠀⠀⠀⢠⡇⠀⠀⠀⠀⢰⠇⠀⠀⠀⠀⠀⠀⢹⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    uint8_t keystate[256];
    GetKeyState(0);
    if (!GetKeyboardState(keystate)) {
        logger_error(app->log, "Couldn't get keyboard state.");
    }

    KBDLLHOOKSTRUCT *keydata = (KBDLLHOOKSTRUCT*)lParam;

    ui_keyboard_event_t event = {0};
    event.down = wParam==WM_KEYDOWN || wParam==WM_SYSKEYDOWN ? 1 : 0;
    event.vk_key = keydata->vkCode;
    event.alt   = keystate[VK_MENU   ] & 0x80;
    event.shift = keystate[VK_SHIFT  ] & 0x80;
    event.ctrl  = keystate[VK_CONTROL] & 0x80;
    event.caps  = keystate[VK_CAPITAL] & 0x01;
    
    ToAscii(keydata->vkCode, keydata->scanCode, keystate, (LPWORD)event.ascii, 0); 

    if (ui_process_keyboard_event(&event)) return 1;

    char *keystr = "unknown";

    switch (event.vk_key) {
    case VK_BACK      : keystr = "backspace"    ; break;
    case VK_TAB       : keystr = "tab"          ; break;
    case VK_CLEAR     : keystr = "clear"        ; break;
    case VK_RETURN    : keystr = "return"       ; break;
    case VK_SHIFT     : keystr = "shift"        ; break;
    case VK_CONTROL   : keystr = "ctrl"         ; break;
    case VK_MENU      : keystr = "alt"          ; break;
    case VK_PAUSE     : keystr = "pause"        ; break;
    case VK_CAPITAL   : keystr = "capslock"     ; break;
    case VK_ESCAPE    : keystr = "escape"       ; break;
    case VK_SPACE     : keystr = "space"        ; break;
    case VK_PRIOR     : keystr = "pageup"       ; break;
    case VK_NEXT      : keystr = "pagedown"     ; break;
    case VK_END       : keystr = "end"          ; break;
    case VK_HOME      : keystr = "home"         ; break;
    case VK_LEFT      : keystr = "left"         ; break;
    case VK_UP        : keystr = "up"           ; break;
    case VK_RIGHT     : keystr = "right"        ; break;
    case VK_DOWN      : keystr = "down"         ; break;
    case VK_SELECT    : keystr = "select"       ; break;
    case VK_PRINT     : keystr = "print"        ; break;
    case VK_EXECUTE   : keystr = "execute"      ; break;
    case VK_SNAPSHOT  : keystr = "printscreen"  ; break;
    case VK_INSERT    : keystr = "insert"       ; break;
    case VK_DELETE    : keystr = "delete"       ; break;
    case VK_HELP      : keystr = "help"         ; break;
    case VK_LWIN      : keystr = "left-windows" ; break;
    case VK_RWIN      : keystr = "right-windows"; break;
    case VK_APPS      : keystr = "applications" ; break;
    case VK_ADD       : keystr = "plus"         ; break;
    case VK_SEPARATOR : keystr = "separator"    ; break;
    case VK_NUMPAD0   : keystr = "numpad0"      ; break;
    case VK_NUMPAD1   : keystr = "numpad1"      ; break;
    case VK_NUMPAD2   : keystr = "numpad2"      ; break;
    case VK_NUMPAD3   : keystr = "numpad3"      ; break;
    case VK_NUMPAD4   : keystr = "numpad4"      ; break;
    case VK_NUMPAD5   : keystr = "numpad5"      ; break;
    case VK_NUMPAD6   : keystr = "numpad6"      ; break;
    case VK_NUMPAD7   : keystr = "numpad7"      ; break;
    case VK_NUMPAD8   : keystr = "numpad8"      ; break;
    case VK_NUMPAD9   : keystr = "numpad9"      ; break;
    case VK_MULTIPLY  : keystr = "multiply"     ; break;
    case VK_SUBTRACT  : keystr = "subtract"     ; break;
    case VK_DECIMAL   : keystr = "decimal"      ; break;
    case VK_DIVIDE    : keystr = "divide"       ; break;
    case VK_F1        : keystr = "f1"           ; break;
    case VK_F2        : keystr = "f2"           ; break;
    case VK_F3        : keystr = "f3"           ; break;
    case VK_F4        : keystr = "f4"           ; break;
    case VK_F5        : keystr = "f5"           ; break;
    case VK_F6        : keystr = "f6"           ; break;
    case VK_F7        : keystr = "f7"           ; break;
    case VK_F8        : keystr = "f8"           ; break;
    case VK_F9        : keystr = "f9"           ; break;
    case VK_F10       : keystr = "f10"          ; break;
    case VK_F11       : keystr = "f11"          ; break;
    case VK_F12       : keystr = "f12"          ; break;
    case VK_F13       : keystr = "f13"          ; break;
    case VK_F14       : keystr = "f14"          ; break;
    case VK_F15       : keystr = "f15"          ; break;
    case VK_F16       : keystr = "f16"          ; break;
    case VK_F17       : keystr = "f17"          ; break;
    case VK_F18       : keystr = "f18"          ; break;
    case VK_F19       : keystr = "f19"          ; break;
    case VK_F20       : keystr = "f20"          ; break;
    case VK_F21       : keystr = "f21"          ; break;
    case VK_F22       : keystr = "f22"          ; break;
    case VK_F23       : keystr = "f23"          ; break;
    case VK_F24       : keystr = "f24"          ; break;
    case VK_NUMLOCK   : keystr = "numlock"      ; break;
    case VK_SCROLL    : keystr = "scrolllock"   ; break;
    case VK_LSHIFT    : keystr = "left-shift"   ; break;
    case VK_RSHIFT    : keystr = "right-shift"  ; break;
    case VK_LCONTROL  : keystr = "left-ctrl"    ; break;
    case VK_RCONTROL  : keystr = "right-ctrl"   ; break;
    case VK_LMENU     : keystr = "left-alt"     ; break;
    case VK_RMENU     : keystr = "right-alt"    ; break;
    case VK_OEM_1     : keystr = "semicolon"    ; break;
    case VK_OEM_PLUS  : keystr = "plus"         ; break;
    case VK_OEM_COMMA : keystr = "comma"        ; break;
    case VK_OEM_MINUS : keystr = "minus"        ; break;
    case VK_OEM_PERIOD: keystr = "period"       ; break;
    case VK_OEM_2     : keystr = "forwardslash" ; break;
    case VK_OEM_3     : keystr = "backtick"     ; break;
    case VK_OEM_4     : keystr = "leftbracket"  ; break;
    case VK_OEM_5     : keystr = "backslash"    ; break;
    case VK_OEM_6     : keystr = "rightbracket" ; break;
    case VK_OEM_7     : keystr = "quote"        ; break;
    default:
        if (event.vk_key>='0' && event.vk_key<='9') {
            keystr = numbers[event.vk_key - '0'];
        } else if (event.vk_key>='A' && event.vk_key<='Z') {
            keystr = letters[event.vk_key - 'A'];
        }
    }

    json_t *json = json_sprintf(
        "%s%s%s%s",
        event.alt   && event.vk_key!=VK_LMENU    && event.vk_key!=VK_RMENU    ? "alt-"   : "",
        event.shift && event.vk_key!=VK_LSHIFT   && event.vk_key!=VK_RSHIFT   ? "shift-" : "",
        event.ctrl  && event.vk_key!=VK_LCONTROL && event.vk_key!=VK_RCONTROL ? "ctrl-"  : "",
        keystr
    );
    lua_manager_queue_event(event.down ? "key-down" : "key-up", json);
    json_decref(json);

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK mouse_hook_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (wParam==WM_LBUTTONUP) app->mouse_ldrag_target = 0;
    if (wParam==WM_RBUTTONUP) app->mouse_rdrag_target = 0;

    HWND fg_win = GetForegroundWindow();
    if (
        nCode < 0 ||
        !IsWindowVisible(app->win_hwnd) ||
        fg_win!=app->target_hwnd
    ) return CallNextHookEx(NULL, nCode, wParam, lParam);

    if (app->mouse_ldrag_target || app->mouse_rdrag_target) return CallNextHookEx(NULL, nCode, wParam, lParam);

    MSLLHOOKSTRUCT *msll = (MSLLHOOKSTRUCT*)lParam;
    
    POINT p = { msll->pt.x, msll->pt.y };
    ScreenToClient(app->win_hwnd, &p);

    ui_mouse_event_t me;
    me.x = p.x;
    me.y = p.y;

    if (
        wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
        wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN
    ) {
        me.event = UI_MOUSE_EVENT_TYPE_BTN_DOWN;
    } else if (
        wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP ||
        wParam == WM_MBUTTONUP || wParam == WM_XBUTTONUP
    ) {
        me.event = UI_MOUSE_EVENT_TYPE_BTN_UP;
    } else if (wParam == WM_MOUSEMOVE) {
        me.event = UI_MOUSE_EVENT_TYPE_MOVE;
    } else if (wParam == WM_MOUSEWHEEL) {
        me.event = UI_MOUSE_EVENT_TYPE_WHEEL;
        me.value = ((int16_t)HIWORD(msll->mouseData)) / WHEEL_DELTA;
    } else if (wParam == WM_MOUSEHWHEEL) {
        me.event = UI_MOUSE_EVENT_TYPE_HWHEEL;
        me.value = ((int16_t)HIWORD(msll->mouseData)) / WHEEL_DELTA;
    } else {
        logger_warn(app->log, "unknown event type: 0x%x", wParam);
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    if (wParam==WM_LBUTTONDOWN || wParam==WM_LBUTTONUP) {
        me.button = UI_MOUSE_EVENT_BUTTON_LEFT;
    } else if (wParam==WM_RBUTTONDOWN || wParam==WM_RBUTTONUP) {
        me.button = UI_MOUSE_EVENT_BUTTON_RIGHT;
    } else if (wParam==WM_MBUTTONDOWN || wParam==WM_MBUTTONUP) {
        me.button = UI_MOUSE_EVENT_BUTTON_MIDDLE;
    } else if (wParam==WM_XBUTTONDOWN || wParam==WM_XBUTTONUP) {
        if (HIWORD(msll->mouseData)==XBUTTON1) me.button = UI_MOUSE_EVENT_BUTTON_X1;
        else me.button = UI_MOUSE_EVENT_BUTTON_X2;
    }
    
    if (ui_process_mouse_event(&me)) {
        if (wParam==WM_MOUSEMOVE || wParam==WM_LBUTTONUP || wParam==WM_RBUTTONUP) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
        return 1;
    } else {
        // if this was a mouse button down event and we didn't consume it
        // then we don't want to process mouse events until the corresponding up
        // event comes through, otherwise we could be getting events during a
        // drag/mouse look.
        // this has to be tracked for all buttons separately (currently just
        // left and right)
        if (wParam==WM_LBUTTONDOWN) app->mouse_ldrag_target = 1;
        if (wParam==WM_RBUTTONDOWN) app->mouse_rdrag_target = 1;
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
}

void app_init(HINSTANCE hinst, int argc, char **argv) {
    logger_init();

    FILETIME create_time = {0};
    FILETIME exit_time = {0};
    FILETIME kernel_time = {0};
    FILETIME user_time = {0};

    GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time);

    ULARGE_INTEGER uli_create = {0};
    uli_create.HighPart = create_time.dwHighDateTime;
    uli_create.LowPart = create_time.dwLowDateTime;

    logger_t *log = logger_new("overlay");
    logger_set_level(log, LOGGER_LEVEL_INFO);

    log_sink_t *file_sink = log_file_sink_new("eg-overlay.log");
    log_sink_t *lua_sink = log_event_sink_new();
    logger_add_sink(log, file_sink);
    logger_add_sink(log, lua_sink);

    // attach additional log output
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // First, if the overlay was launched from a console window of some sort
        // just use that console's standard error.
        // At this point EG-Overlay is attached to the console, so if it is
        // closed, our process exits too.
        log_sink_t *con_sink = log_console_sink_new();
        logger_add_sink(log, con_sink);
    } else if (IsDebuggerPresent()) {
        // If no console exists, see if we are in a debugging session, and if
        // so, output there. We only do this if not already attached to a
        // console because doing both would mean duplicate output on a console
        // debugger.
        log_sink_t *dbg_sink = log_dbg_sink_new();
        logger_add_sink(log, dbg_sink);
    }

    logger_set_default(log);

    logger_info(log, "============================================================");
    logger_info(log, "EG-Overlay startup");
    logger_info(log, "Version " VERSION_STR);
    logger_info(log, "Git Commit: " GITHASHSTR);
    logger_info(log, "------------------------------------------------------------");

    app = egoverlay_calloc(1, sizeof(app_t));
    app->target_win_class = "ArenaNet_Gr_Window_Class";
    app->log = log;
    app->app_start_time = uli_create.QuadPart;
    logger_debug(app->log, "init");

    int no_input_hooks = 0;

    for (int a=1;a<argc;a++) {
        if (strcmp(argv[a], "--debug")==0) {
            logger_set_level(log, LOGGER_LEVEL_DEBUG);
            logger_debug(log, "Debug logging enabled.");
        } else if (strcmp(argv[a], "--target-win-class")==0) {
            if (a + 1 == argc) {
                MessageBox(
                    NULL,
                    "--target-win-class argument requires a string.",
                    "Command Line Argument Error",
                    MB_OK | MB_ICONERROR
                );
                exit(-1);
            } else {
                app->target_win_class = argv[++a];
                logger_warn(app->log, "Target window class overriden: %s", app->target_win_class);
            }
        } else if (strcmp(argv[a], "--help")==0) {
            MessageBox(NULL, 
                "--help\n"
                "--target-win-class (window class name)\n"
                "--no-input-hooks",
                "EG-Overlay Command Line Options", MB_OK | MB_ICONINFORMATION);
                exit(0);
        } else if (strcmp(argv[a], "--no-input-hooks")==0) {
            logger_warn(app->log, "Input hooks DISABLED.");
            no_input_hooks = 1;
        } else if (strcmp(argv[a], "--lua-script")==0) {
            if (a + 1 == argc) {
                MessageBox(
                    NULL,
                    "--lua-script argument requires a path.",
                    "Command Line Argument Error",
                    MB_OK |MB_ICONERROR
                );
                exit(-1);
            } else {
                app->runscript = argv[++a];
                logger_warn(app->log, "--lua-script mode, running %s", app->runscript);
            }
        }
    }

    app->settings = settings_new("eg-overlay");
    settings_set_default_double(app->settings, "overlay.frameTargetTime", 32.0);

    if (app->runscript) {
        return;
    }

    if (CoInitializeEx(NULL, COINIT_MULTITHREADED)!=S_OK) {
        logger_error(app->log, "Couldn't initialize COM.");
        exit(-1);
    }

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    app->inst = hinst;
    app_register_win_class();
    app_create_window();

    dx_init(app->win_hwnd);

    if (!no_input_hooks) {
        SetWindowsHookEx(WH_MOUSE_LL, &mouse_hook_proc, NULL, 0);
        SetWindowsHookEx(WH_KEYBOARD_LL, &keyboard_hook_proc, NULL, 0);
    }

    app->sys_tray_menu = CreatePopupMenu();
    AppendMenu(app->sys_tray_menu, MF_GRAYED | MF_STRING , 0             , "EG-Overlay " VERSION_STR);
    AppendMenu(app->sys_tray_menu, MF_SEPARATOR          , 0             , NULL                     );
    AppendMenu(app->sys_tray_menu, MF_ENABLED | MF_STRING, WM_SYSTRAYDOCS, "Open documentation"     );
    AppendMenu(app->sys_tray_menu, MF_ENABLED | MF_STRING, WM_SYSTRAYLOG , "Open log file"          );
    AppendMenu(app->sys_tray_menu, MF_SEPARATOR          , 0             , NULL                     );
    AppendMenu(app->sys_tray_menu, MF_ENABLED | MF_STRING, WM_SYSTRAYQUIT, "Quit"                   );
}

void app_cleanup() {
    logger_debug(app->log, "cleanup");

    if (!app->runscript) {
        DestroyMenu(app->sys_tray_menu);
        dx_cleanup();
    }
    
    settings_unref(app->settings);

    logger_info(app->log, "------------------------------------------------------------");
    logger_info(app->log, "EG-Overlay shutdown");
    logger_info(app->log, "============================================================");

    logger_free(app->log);
    logger_cleanup();

    egoverlay_free(app);
}

static DWORD WINAPI app_render_thread(LPVOID lpParam) {
    (void)lpParam; // unused

    logger_debug(app->log, "begin render thread.");

    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof(TIMECAPS));

    logger_debug(app->log, "Setting timer resolution to %d ms.", tc.wPeriodMin);

    // set the timer resolution to the minimum possible, hopefully 1ms
    // this allows Sleep to be much more accurate
    timeBeginPeriod(tc.wPeriodMin);

    mat4f_t sceneproj = {0};
    mat4f_t sceneview = {0};

    // run startup events
    lua_manager_queue_event("startup", NULL);
    lua_manager_run_event_queue();

    logger_info(app->log, "Starting render loop.");

    double frame_begin;
    double frame_end;
    double frame_target;

    settings_get_double(app->settings, "overlay.frameTargetTime", &frame_target);        
    
    vec3f_t avatar = {0,0,0};
    vec3f_t camera = {0,0,0};
    vec3f_t camera_front = {0, 0, 0};
    vec3f_t up = {0.f, 1.f, 0.f};

    float fov = 0.f;

    while (app->running) {
        if (!app->visible) {
            lua_manager_run_events();
            int havecoroutines = lua_manager_resume_coroutines();
            lua_manager_queue_event("update", NULL);
            lua_manager_run_event_queue();

            while (havecoroutines) havecoroutines = lua_manager_resume_coroutines();

            Sleep(25);
            continue;
        }

        frame_begin = app_get_uptime() / 10000.0 / 1000.0;

        fov = mumble_link_fov();

        lua_manager_run_events();
        lua_manager_resume_coroutines();
        lua_manager_queue_event("update", NULL);
        lua_manager_run_event_queue();

        dx_start_frame();

        if (fov!=0.0) {
            uint32_t width;
            uint32_t height;
            dx_get_render_target_size(&width, &height);
            mumble_link_avatar_position(&avatar.x, &avatar.y, &avatar.z);
            mumble_link_camera_position(&camera.x, &camera.y, &camera.z);
            mumble_link_camera_front(&camera_front.x, &camera_front.y, &camera_front.z);
            
            mat4f_perpsective_lh(&sceneproj, fov, (float)width/(float)height, 1.f, 25000.f);

            avatar.x *= 39.3701f;
            avatar.y *= 39.3701f;
            avatar.z *= 39.3701f;
            camera.x *= 39.3701f;
            camera.y *= 39.3701f;
            camera.z *= 39.3701f;

            mat4f_camera_facing(&sceneview, &camera, &camera_front, &up);

            overlay_3d_begin_frame(&sceneview, &sceneproj);
            lua_manager_run_event("draw-3d", NULL);
            overlay_3d_end_frame();
        }

        ui_draw(dx_get_ortho_proj());
        dx_end_frame();

        frame_end = app_get_uptime() / 10000.0 / 1000.0;

        long frame_time = (long)((frame_end - frame_begin) * 1000);
        long sleep_time = (long)frame_target - frame_time;

        // if we have extra time after rendering the frame, run the Lua
        // coroutines again if there are any pending and keep running them until
        // they are either done or we run out of time before the next frame
        while (sleep_time > 0 && lua_manager_resume_coroutines()) {
            frame_end = app_get_uptime() / 10000.0 / 1000.0;
            frame_time = (long)((frame_end - frame_begin) * 1000);
            sleep_time = (long)frame_target - frame_time;
        }

        // if we still have extra time, sleep
        if (sleep_time > 0) {
            Sleep(sleep_time);
        }
    }

    logger_debug(app->log, "Waiting for all Lua coroutines to finish.");
    // run shutdown events and wait for all coroutines to finish
    lua_manager_queue_event("shutdown", NULL);
    lua_manager_run_event_queue();
    while (lua_manager_resume_coroutines());

    timeEndPeriod(tc.wPeriodMin);

    logger_debug(app->log, "end render thread.");

    return 0;
}

static DWORD WINAPI app_fgwincheck_thread(LPVOID lpParam) {
    UNUSED_PARAM(lpParam);
    logger_debug(app->log, "begin foreground window checker thread...");

    HWND lastwin = NULL;
    char *fg_cls = egoverlay_calloc(513, sizeof(char));
    char *target_cls = egoverlay_calloc(513, sizeof(char));

    RECT *target_rect = egoverlay_calloc(1, sizeof(RECT));
    POINT *target_pos = egoverlay_calloc(1, sizeof(POINT));

    HWND fg_win = NULL;

    while (app->running) {
        fg_win = GetForegroundWindow();

        if (fg_win && fg_win!=lastwin) {
            memset(fg_cls, 0, 513);
            GetClassName(fg_win, fg_cls, 512);

            if (strcmp(fg_cls, app->target_win_class)==0) {
                logger_debug(app->log, "Target window reactivated, showing overlay. (%s)", fg_cls);
                ShowWindow(app->win_hwnd, SW_SHOWNA);
                app->visible = 1;
                app->target_hwnd = fg_win;

            }

            if (fg_win==app->target_hwnd) {
                SetWindowPos(app->win_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                SetWindowPos(app->win_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            } else {
                SetWindowPos(app->win_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
        } else {
            memset(target_cls, 0, 513);     
            if (
                app->target_hwnd &&
                (GetClassName(app->target_hwnd, target_cls, 512)==0 ||
                 strcmp(target_cls, app->target_win_class)!=0)
            ) {
                logger_debug(app->log, "Target window disappeared, hiding overlay.");
                ShowWindow(app->win_hwnd, SW_HIDE);
                app->visible = 0;
                app->target_hwnd = NULL;                
            } else if (fg_win==app->target_hwnd) {
                GetClientRect(app->target_hwnd, target_rect);
                target_pos->x = target_rect->left;
                target_pos->y = target_rect->top;
                ClientToScreen(app->target_hwnd, target_pos);

                SetWindowPos(app->win_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                SetWindowPos(
                    app->win_hwnd,
                    HWND_NOTOPMOST,
                    target_pos->x,
                    target_pos->y,
                    target_rect->right - target_rect->left,
                    target_rect->bottom - target_rect->top,
                    SWP_NOACTIVATE
                );
            }

        }
        lastwin = fg_win;

        Sleep(100);
    }

    egoverlay_free(fg_cls);
    egoverlay_free(target_cls);
    egoverlay_free(target_rect);
    egoverlay_free(target_pos);

    logger_debug(app->log, "end foreground window checker thread.");

    return 0;
}

int app_run() {
    if (app->runscript) {
        app_run_script();
        return 0;
    }

    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(nid);
    nid.hWnd = app->win_hwnd;
    nid.uID = 0x01;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_SYSTRAYEVENT;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.hIcon = LoadIcon(app->inst, MAKEINTRESOURCE(IDI_EGOVERLAY_16x16));
    memcpy(nid.szTip, "EG-Overlay " VERSION_STR, strlen("EG-Overlay " VERSION_STR));
    
    Shell_NotifyIcon(NIM_ADD, &nid);
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    // init lua first, others may register module openers
    lua_manager_init();
    settings_lua_init();
    zip_lua_init();
    xml_lua_init();
    json_lua_init();
    web_request_init();
    ui_init();
    overlay_3d_init();
    mumble_link_init();
    lua_sqlite_init();

    lua_manager_run_file("lua/autoload.lua");

    app->running = 1;

    logger_debug(app->log, "Starting render thread...");
    DWORD render_thread_id = 0;
    HANDLE render_thread = CreateThread(NULL, 0, &app_render_thread, NULL, 0, &render_thread_id);

    logger_debug(app->log, "Starting foreground window checker thread...");
    DWORD fgwin_thread_id = 0;
    HANDLE fgwin_thread = CreateThread(NULL, 0, &app_fgwincheck_thread, NULL, 0, &fgwin_thread_id);

    if (render_thread==NULL) {
        logger_error(app->log, "Failed to create render thread.");
        return -1;
    }

    MSG msg = {0};
    while (msg.message!=WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        #ifdef _DEBUG
        dx_process_debug_messages();
        #endif
        Sleep(1);
    }

    app->running = 0;

    logger_debug(app->log, "Waiting for threads to end...");
    
    WaitForSingleObject(render_thread, INFINITE);
    WaitForSingleObject(fgwin_thread, INFINITE);
    CloseHandle(render_thread);
    CloseHandle(fgwin_thread);

    ui_clear_top_level_elements();
    lua_manager_cleanup();
    mumble_link_cleanup();
    overlay_3d_cleanup();
    ui_cleanup();
    web_request_cleanup();
    xml_cleanup();

    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyIcon(nid.hIcon);

    return 0;
}

settings_t *app_get_settings() {
    return app->settings;
}

void app_setclipboard_text(const char *text) {
    if (!OpenClipboard(app->win_hwnd)) {
        logger_error(app->log, "Couldn't open clipboard.");
        return;
    }

    if (!EmptyClipboard()) {
        logger_error(app->log, "Couldn't empty clipboard.");
        CloseClipboard();
        return;
    }

    HGLOBAL glbltext = GlobalAlloc(GMEM_MOVEABLE, strlen(text)+1);
    char *textcpy = GlobalLock(glbltext);
    memcpy(textcpy, text, strlen(text)+1);
    GlobalUnlock(textcpy);

    if (!SetClipboardData(CF_TEXT, glbltext)) {
        logger_error(app->log, "Couldn't set clipboard text: %d", GetLastError());
    }
    CloseClipboard();

    logger_debug(app->log, "Set clipboard text to: %s", text);

    return;
}

char *app_getclipboard_text() {
    if (!OpenClipboard(app->win_hwnd)) {
        logger_error(app->log, "Couldn't open clipboard.");
        return NULL;
    }

    HGLOBAL glbltext = GetClipboardData(CF_TEXT);

    if (glbltext==NULL) {
        CloseClipboard();
        logger_debug(app->log, "Couldn't get text from clipboard.");
        return NULL;
    }

    char *textcpy = GlobalLock(glbltext);
    char *text = egoverlay_calloc(strlen(textcpy)+1, sizeof(char));
    memcpy(text, textcpy, strlen(textcpy));
    GlobalUnlock(textcpy);

    CloseClipboard();

    logger_debug(app->log, "Got text from clipboard: %s", text);
    return text;
}

void app_exit() {
    PostMessage(app->win_hwnd, WM_APPEXIT, 0, 0);
}

void app_run_script() {

    // everything but UI
    lua_manager_init();
    settings_lua_init();
    zip_lua_init();
    xml_lua_init();
    json_lua_init();
    web_request_init();
    mumble_link_init();
    lua_sqlite_init();

    lua_manager_run_file(app->runscript);

    lua_manager_cleanup();
    mumble_link_cleanup();
    web_request_cleanup();
    xml_cleanup();
}

void app_get_mouse_coords(int *x, int *y) {
    POINT mouse = {0};

    if (!GetCursorPos(&mouse)) return;
    if (!ScreenToClient(app->win_hwnd, &mouse)) return;

    *x = mouse.x;
    *y = mouse.y;
}

uint64_t app_get_uptime() {
    FILETIME ft_now;
    SYSTEMTIME st_now;
    GetSystemTime(&st_now);
    SystemTimeToFileTime(&st_now, &ft_now);

    ULARGE_INTEGER uli_now = {0};
    uli_now.HighPart = ft_now.dwHighDateTime;
    uli_now.LowPart = ft_now.dwLowDateTime;

    return uli_now.QuadPart - app->app_start_time;
}

#include <windows.h>
#include <windowsx.h>
#include <glad/gl.h>
#include <glfw/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>

#include "eg-overlay.h"
#include "githash.h"

#include "app.h"
#include "logging/logger.h"
#include "logging/console-sink.h"
#include "logging/file-sink.h"
#include "logging/event-sink.h"
#include "logging/dbg-sink.h"
#include <stdlib.h>

#include <stb_image.h>

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
#include "lua-gl.h"

#include <time.h>
#include <stdio.h>
#include <sys/timeb.h>

#define WM_SYSTRAYEVENT (WM_APP + 1)
#define WM_SYSTRAYQUIT  (WM_APP + 2)
#define WM_SYSTRAYLOG   (WM_APP + 3)
#define WM_SYSTRAYDOCS  (WM_APP + 4)

typedef struct {
    logger_t *log;

    GLFWwindow *win;

    HWND message_win;
    HINSTANCE inst;
    HWND win_hwnd;
    HWND target_hwnd;
    HMENU sys_tray_menu;

    settings_t *settings;

    const char *target_win_class;

    const char *runscript;
} app_t;

static app_t *app = NULL;

static void app_run_script();

static LRESULT CALLBACK winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SYSTRAYEVENT:
        if (LOWORD(lParam)==WM_CONTEXTMENU) {
            // if the window isn't foreground the menu will not close if the user clicks out of it
            // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu
            SetForegroundWindow(app->message_win); 
            int systraycmd = TrackPopupMenu(
                app->sys_tray_menu,
                TPM_RETURNCMD,
                GET_X_LPARAM(wParam),
                GET_Y_LPARAM(wParam),
                0,
                app->message_win,
                NULL
            );
            switch(systraycmd) {
            case WM_SYSTRAYQUIT:
                logger_debug(app->log,"Quit selected.");
                glfwSetWindowShouldClose(app->win, GLFW_TRUE);
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

            PostMessage(app->message_win, WM_NULL, 0, 0);
        }
        break;
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

static void register_win_class() {
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));

    wc.lpfnWndProc = &winproc;
    wc.hInstance = app->inst;
    wc.lpszClassName = "EG-Overlay Message Window Class";
    wc.cbWndExtra = 0;
    wc.cbSize = sizeof(WNDCLASSEX);

    RegisterClassEx(&wc);
}

LRESULT CALLBACK keyboard_hook_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    HWND fg_win = GetForegroundWindow();
    if (
        nCode < 0 ||
        glfwGetWindowAttrib(app->win, GLFW_VISIBLE)==GLFW_FALSE ||
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

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK mouse_hook_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    HWND fg_win = GetForegroundWindow();
    if (
        nCode < 0 ||
        glfwGetWindowAttrib(app->win, GLFW_VISIBLE)==GLFW_FALSE ||
        fg_win!=app->target_hwnd
    ) return CallNextHookEx(NULL, nCode, wParam, lParam);

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
    } else if (wParam==WM_RBUTTONDOWN || wParam==WM_LBUTTONUP) {
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
    } else return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void APIENTRY gl_debug_output(
    GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei length,
    const char *message,
    const void *userp
) {
    UNUSED_PARAM(userp);
    UNUSED_PARAM(length);

    // filter out messages we don't care about
    if (
        id == 131185 || // buffer will use VIDEO memory as source
        id == 131218    // shader in program XX is being recompiled based on GL state
    ) return;

    char *srcstr = "";
    char *typestr = "";
    char *severitystr = "";

    switch (source) {
    case GL_DEBUG_SOURCE_API:             srcstr = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   srcstr = "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: srcstr = "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     srcstr = "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     srcstr = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           srcstr = "Other"; break;
    default:                              srcstr = "Unknown"; break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               typestr = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typestr = "Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typestr = "Undefined Behaviour"; break; 
    case GL_DEBUG_TYPE_PORTABILITY:         typestr = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         typestr = "Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              typestr = "Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          typestr = "Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           typestr = "Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               typestr = "Other"; break;
    }
    
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         severitystr = "high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       severitystr = "medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          severitystr = "low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: severitystr = "notification"; break;
    }

    logger_debug(app->log, "OpenGL message: (%d) %s - %s - %s: %s", id, srcstr, typestr, severitystr, message);
}

void app_init(HINSTANCE hinst, int argc, char **argv) {
    logger_init();

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

    glfwInit();

    if (app->runscript) {
        return;
    }

    app->inst = hinst;

    register_win_class();

    app->message_win = CreateWindowEx(
        0,
        "EG-Overlay Message Window Class",
        "EG-Overlay Message Window", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);   

    
    #ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    #endif

    glfwWindowHint(GLFW_VISIBLE                , GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW          , GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR  , 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR  , 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE         , GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER           , GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED              , GLFW_FALSE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH      , GLFW_TRUE);

    app->win = glfwCreateWindow(1200, 800, "EG-Overlay", NULL, NULL);
    app->win_hwnd = glfwGetWin32Window(app->win);

    // don't show the window on the task bar
    DWORD ws_ex_style = (DWORD)GetWindowLongPtr(app->win_hwnd, GWL_EXSTYLE);
    ws_ex_style = (ws_ex_style & ~WS_EX_APPWINDOW) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    SetWindowLongPtr(app->win_hwnd, GWL_EXSTYLE, ws_ex_style);

    glfwMakeContextCurrent(app->win);

    gladLoadGL(glfwGetProcAddress);

    int context_flags;
    glGetIntegerv(GL_CONTEXT_FLAGS,&context_flags);

    // setup OpenGL debug logging
    if (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(&gl_debug_output, NULL);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

        logger_debug(app->log, "OpenGL debug logging enabled.");
    }

    logger_info(app->log, "OpenGL Initialized");
    logger_info(app->log, "------------------------------------------------------------");
    logger_info(app->log, "Version       %s", (char*)glGetString(GL_VERSION));
    logger_info(app->log, "Renderer:     %s", (char*)glGetString(GL_RENDERER));
    logger_info(app->log, "Vendor:       %s", (char*)glGetString(GL_VENDOR));
    logger_info(app->log, "GLSL Version: %s", (char*)glGetString(GL_SHADING_LANGUAGE_VERSION));


    
    logger_info(app->log, "Checking extensions:");
    int have_shading_include = 0;
    int i = 0;
    GLint extn = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extn);
    for (i=0; i<extn; i++) {
        const GLubyte *ext = glGetStringi(GL_EXTENSIONS, i);
        if (strcmp((const char*)ext,"GL_ARB_shading_language_include")==0) {
            logger_info(app->log, "  GL_ARB_shading_language_include supported");
            have_shading_include = 1;
        }
   }

    if (!have_shading_include) {
        logger_error(app->log,"  GL_ARB_shading_language_include not supported.");
        abort();
    }    

    logger_info(app->log, "------------------------------------------------------------");

    glfwSwapInterval(0); // no V-sync


    // alpha blending, but we'll do premultiplied RGB in our fragment shaders
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        
    glEnable(GL_DEPTH_TEST);

    // GW2 is a DirectX application, so we'll be using left-handed rendering
    // instead of right-handed. Switch some defaults around to compensate.
    glDepthFunc(GL_GEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    
    // let the render thread grab the context when it starts up
    glfwMakeContextCurrent(NULL);

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

    // OpenGL textures expect data to be from the bottom up
    stbi_set_flip_vertically_on_load(1);
}

void app_cleanup() {
    logger_debug(app->log, "cleanup");

    if (!app->runscript) {
        DestroyWindow(app->message_win);
        glfwDestroyWindow(app->win);
        
        DestroyMenu(app->sys_tray_menu);
    }
    glfwTerminate();
    
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
    glfwMakeContextCurrent(app->win);

    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof(TIMECAPS));

    logger_debug(app->log, "Setting timer resolution to %d ms.", tc.wPeriodMin);

    // set the timer resolution to the minimum possible, hopefully 1ms
    // this allows Sleep to be much more accurate
    timeBeginPeriod(tc.wPeriodMin);

    mat4f_t proj = {0};
    mat4f_t sceneproj = {0};
    mat4f_t sceneview = {0};

    // run startup events
    lua_manager_queue_event("startup", NULL);
    lua_manager_run_event_queue();

    logger_info(app->log, "Starting render loop.");

    double frame_begin;
    double frame_end;
    double frame_target;
    
    vec3f_t avatar = {0,0,0};
    vec3f_t camera = {0,0,0};
    vec3f_t camera_front = {0, 0, 0};
    vec3f_t up = {0.f, 1.f, 0.f};

    float fov = 0.f;

    glClearColor(0.0f, 0.0f, 0.0f, 0.f);
    glClearDepth(-1.f); // left-handed
    glClearStencil(1);
    //glStencilMask(0x0);

    while (!glfwWindowShouldClose(app->win)) {
        settings_get_double(app->settings, "overlay.frameTargetTime", &frame_target);        
        frame_begin = glfwGetTime();

        int width;
        int height;
        app_get_framebuffer_size(&width, &height);
        mat4f_ortho(&proj, 0.f, (float)width, 0.f, (float)height,-1.f, 1.f);        

        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);        

        lua_manager_run_events();
        int have_coroutines = lua_manager_resume_coroutines();
        lua_manager_queue_event("update", NULL);
        lua_manager_run_event_queue();

        if (glfwGetWindowAttrib(app->win, GLFW_VISIBLE)==GLFW_FALSE) {
            if (!have_coroutines) Sleep(100);
            continue;
        }
    
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        fov = mumble_link_fov();
        if (fov!=0.0) {

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

        glDisable(GL_DEPTH_TEST);
        ui_draw(&proj);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(app->win);

        frame_end = glfwGetTime();

        long frame_time = (long)((frame_end - frame_begin) * 1000);
        long sleep_time = (long)frame_target - frame_time;

        // if we have extra time after rendering the frame, run the Lua
        // coroutines again if there are any pending and keep running them until
        // they are either done or we run out of time before the next frame
        while (sleep_time > 0 && lua_manager_resume_coroutines()) {
            frame_end = glfwGetTime();
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

    glfwMakeContextCurrent(NULL);
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

    while (!glfwWindowShouldClose(app->win)) {
        fg_win = GetForegroundWindow();

        if (fg_win && fg_win!=lastwin) {
            memset(fg_cls, 0, 513);
            GetClassName(fg_win, fg_cls, 512);

            if (strcmp(fg_cls, app->target_win_class)==0) {
                logger_debug(app->log, "Target window reactivated, showing overlay. (%s)", fg_cls);
                glfwShowWindow(app->win);
                app->target_hwnd = fg_win;

            }

            if (fg_win==app->target_hwnd) {
                SetWindowPos(app->win_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                SetWindowPos(app->win_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            } else {
                //logger_debug(app->log, "Overlay -> not topmost");
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
                glfwHideWindow(app->win);
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
                    target_rect->bottom - target_rect->top - 1,
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
    nid.hWnd = app->message_win;
    nid.uID = 0x01;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_SYSTRAYEVENT;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.hIcon = LoadIcon(app->inst, MAKEINTRESOURCE(IDI_EGOVERLAY_16x16));
    memcpy(nid.szTip, "EG-Overlay " VERSION_STR, strlen("EG-Overlay " VERSION_STR));
    
    Shell_NotifyIcon(NIM_ADD, &nid);
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    glfwMakeContextCurrent(app->win);
    
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
    glfwMakeContextCurrent(NULL);

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

    // poll events in a tight loop
    // since we are using hooks any delays here can negatively impact input for the entire system!
    while (!glfwWindowShouldClose(app->win)) {
        glfwPollEvents();

        Sleep(1);
    }

    logger_debug(app->log, "Waiting for threads to end...");
    
    WaitForSingleObject(render_thread, INFINITE);
    WaitForSingleObject(fgwin_thread, INFINITE);
    CloseHandle(render_thread);
    CloseHandle(fgwin_thread);

    glfwMakeContextCurrent(app->win);
    ui_clear_top_level_elements();
    lua_manager_cleanup();
    mumble_link_cleanup();
    overlay_3d_cleanup();
    ui_cleanup();
    web_request_cleanup();
    xml_cleanup();
    glfwMakeContextCurrent(NULL);

    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyIcon(nid.hIcon);

    return 0;
}

void app_get_framebuffer_size(int *width, int *height) {
    glfwGetFramebufferSize(app->win, width, height);
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
    glfwSetWindowShouldClose(app->win, GLFW_TRUE);
}

static void app_run_script() {

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

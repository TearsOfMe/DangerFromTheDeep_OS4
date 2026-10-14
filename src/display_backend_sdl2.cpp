/*
 * Danger from the Deep - Backend de display SDL2 (ventana, GL, eventos)
 */

#include "display_backend.h"
#include <SDL.h>
#include <algorithm>
#include <list>
#include <stdexcept>

static void sdl_throw(const char* msg) {
    const char* err = SDL_GetError();
    std::string full = msg;
    if (err && err[0]) {
        full += ": ";
        full += err;
    }
    throw std::runtime_error(full);
}

// --- Inicialización ---
static bool s_has_gl_context = false;

bool display_has_gl_context() {
    return s_has_gl_context && (SDL_GL_GetCurrentContext() != nullptr);
}

void display_init_video() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        sdl_throw("video init failed");
}

void display_quit_video() {
    s_has_gl_context = false;
    SDL_Quit();
}

void display_get_version(int* major, int* minor, int* patch) {
    SDL_version v;
    SDL_GetVersion(&v);
    *major = v.major;
    *minor = v.minor;
    *patch = v.patch;
}

void display_get_available_resolutions(int display_index, std::list<vector2i>& out) {
    out.clear();
    int n = SDL_GetNumDisplayModes(display_index);
    if (n > 0) {
        SDL_DisplayMode dm;
        for (int i = 0; i < n; ++i) {
            if (SDL_GetDisplayMode(display_index, i, &dm) == 0) {
                vector2i v(dm.w, dm.h);
                if (std::find(out.begin(), out.end(), v) == out.end()) {
                    out.push_back(v);
                }
            }
        }
    }
    // Ensure common standard resolutions are always present
    static const vector2i standard_modes[] = {
        vector2i(800, 600),
        vector2i(1024, 768),
        vector2i(1280, 720),
        vector2i(1280, 1024),
        vector2i(1920, 1080)
    };
    for (size_t i = 0; i < sizeof(standard_modes) / sizeof(standard_modes[0]); ++i) {
        if (std::find(out.begin(), out.end(), standard_modes[i]) == out.end()) {
            out.push_back(standard_modes[i]);
        }
    }
}

void dftd_startup_log(const char *fmt, ...);

void display_create_window(const char* caption, int w, int h, bool fullscreen,
                          bool multisample, int multisample_level, int vsync,
                          void** out_window, void** out_glcontext) {
    dftd_startup_log("Creating SDL window: %dx%d (fullscreen=%d)...", w, h, fullscreen ? 1 : 0);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    Uint32 fs_mode = 0;
    if (fullscreen) {
        SDL_DisplayMode desktop_mode;
        if (SDL_GetDesktopDisplayMode(0, &desktop_mode) == 0 && w == desktop_mode.w && h == desktop_mode.h) {
            fs_mode = SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else {
            fs_mode = SDL_WINDOW_FULLSCREEN;
        }
        flags |= fs_mode;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#ifdef __amigaos4__
    // GL4ES / Mesa requires OpenGL 2.1 context hints
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
    if (multisample) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample_level);
    }

    SDL_Window* win = SDL_CreateWindow(caption,
                                      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                      w, h, flags);
    if (!win) {
        dftd_startup_log("SDL_CreateWindow failed: %s", SDL_GetError());
        std::cout << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        sdl_throw("Video mode set failed");
    }

    if (fullscreen && fs_mode == SDL_WINDOW_FULLSCREEN) {
        SDL_DisplayMode target_mode;
        SDL_zero(target_mode);
        target_mode.w = w;
        target_mode.h = h;
        SDL_DisplayMode closest;
        if (SDL_GetClosestDisplayMode(0, &target_mode, &closest) != NULL) {
            SDL_SetWindowDisplayMode(win, &closest);
        } else {
            SDL_SetWindowDisplayMode(win, &target_mode);
        }
    }

    dftd_startup_log("SDL window created successfully. Creating OpenGL context...");
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        dftd_startup_log("SDL_GL_CreateContext failed (%s), retrying with default attributes...", SDL_GetError());
        SDL_GL_ResetAttributes();
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
        ctx = SDL_GL_CreateContext(win);
    }
    if (!ctx) {
        dftd_startup_log("SDL_GL_CreateContext failed completely: %s", SDL_GetError());
        std::cout << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(win);
        sdl_throw("Couldn't create OpenGL context");
    }

    dftd_startup_log("OpenGL context created successfully.");
    s_has_gl_context = true;

    int ret_vsync = SDL_GL_SetSwapInterval(vsync);
    dftd_startup_log("SDL_GL_SetSwapInterval(%d) returned: %d (error: %s)", vsync, ret_vsync, SDL_GetError());

    *out_window = win;
    *out_glcontext = ctx;
}

void display_destroy_window(void* window, void* glcontext) {
    s_has_gl_context = false;
    if (glcontext)
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(glcontext));
    if (window)
        SDL_DestroyWindow(static_cast<SDL_Window*>(window));
}

void display_set_window_size(void* window, int w, int h) {
    if (!window) return;
    SDL_SetWindowSize(static_cast<SDL_Window*>(window), w, h);
    SDL_SetWindowPosition(static_cast<SDL_Window*>(window), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void display_get_window_size(void* window, int* w, int* h) {
    if (!window) return;
    SDL_Window* win = static_cast<SDL_Window*>(window);
    SDL_PumpEvents();
    Uint32 flags = SDL_GetWindowFlags(win);
    if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        SDL_DisplayMode mode;
        if (SDL_GetWindowDisplayMode(win, &mode) == 0 && mode.w > 0 && mode.h > 0) {
            *w = mode.w;
            *h = mode.h;
            return;
        }
    }
    SDL_GL_GetDrawableSize(win, w, h);
    if (*w <= 0 || *h <= 0) {
        SDL_GetWindowSize(win, w, h);
    }
}

void display_set_mode(void* window, int w, int h, bool fullscreen) {
    if (!window) return;
    SDL_Window* win = static_cast<SDL_Window*>(window);
    dftd_startup_log("display_set_mode: %dx%d (fullscreen=%d)...", w, h, fullscreen ? 1 : 0);

    if (fullscreen) {
        SDL_DisplayMode desktop_mode;
        int has_desktop = SDL_GetDesktopDisplayMode(0, &desktop_mode);
        bool is_desktop = (has_desktop == 0 && w == desktop_mode.w && h == desktop_mode.h);
        Uint32 target_flags = is_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;

        SDL_DisplayMode target_mode;
        SDL_zero(target_mode);
        target_mode.w = w;
        target_mode.h = h;
        target_mode.format = (has_desktop == 0) ? desktop_mode.format : 0;

        SDL_DisplayMode closest;
        if (SDL_GetClosestDisplayMode(0, &target_mode, &closest) != NULL) {
            dftd_startup_log("Setting window display mode: %dx%d (format=0x%08x)...", closest.w, closest.h, closest.format);
            SDL_SetWindowDisplayMode(win, &closest);
        } else {
            dftd_startup_log("Setting window display mode: %dx%d...", target_mode.w, target_mode.h);
            SDL_SetWindowDisplayMode(win, &target_mode);
        }

        // Toggle through windowed to cleanly switch video mode/dimensions
        SDL_SetWindowFullscreen(win, 0);
        SDL_SetWindowSize(win, w, h);
        int ret = SDL_SetWindowFullscreen(win, target_flags);
        dftd_startup_log("SDL_SetWindowFullscreen(0x%08x) result: %d", target_flags, ret);
    } else {
        SDL_SetWindowFullscreen(win, 0);
        SDL_SetWindowSize(win, w, h);
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_PumpEvents();
}

void display_set_fullscreen(void* window, bool fullscreen) {
    if (!window) return;
    int w = 0, h = 0;
    display_get_window_size(window, &w, &h);
    display_set_mode(window, w, h, fullscreen);
}

uint32_t display_get_window_id(void* window) {
    return SDL_GetWindowID(static_cast<SDL_Window*>(window));
}

void display_swap_buffers(void* window) {
    SDL_GL_SwapWindow(static_cast<SDL_Window*>(window));
}

void display_set_swap_interval(int interval) {
    int ret = SDL_GL_SetSwapInterval(interval);
    dftd_startup_log("display_set_swap_interval(%d) returned: %d (error: %s)", interval, ret, SDL_GetError());
}

const char* display_get_error() {
    return SDL_GetError();
}

uint32_t display_get_ticks() {
    return SDL_GetTicks();
}

void display_delay(uint32_t ms) {
    SDL_Delay(ms);
}

void display_setup_events_and_cursor() {
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_ShowCursor(SDL_ENABLE);
}

namespace {

game_event sdl_to_game(const SDL_Event& e) {
    game_event ge;
    switch (e.type) {
    case SDL_QUIT:
        ge.type = event_type::QUIT;
        break;
    case SDL_KEYDOWN:
        ge.type = event_type::KEY_DOWN;
        ge.keysym.sym = static_cast<key_code>(e.key.keysym.sym);
        ge.keysym.scancode = e.key.keysym.scancode;
        ge.keysym.mod = static_cast<key_mod>(e.key.keysym.mod);
        break;
    case SDL_KEYUP:
        ge.type = event_type::KEY_UP;
        ge.keysym.sym = static_cast<key_code>(e.key.keysym.sym);
        ge.keysym.scancode = e.key.keysym.scancode;
        ge.keysym.mod = static_cast<key_mod>(e.key.keysym.mod);
        break;
    case SDL_MOUSEMOTION:
        ge.type = event_type::MOUSE_MOTION;
        ge.motion_x = e.motion.x;
        ge.motion_y = e.motion.y;
        ge.motion_xrel = e.motion.xrel;
        ge.motion_yrel = e.motion.yrel;
        ge.motion_state = e.motion.state;
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == 4) { // SDL_BUTTON_WHEELUP (button 4)
            ge.type = event_type::MOUSE_WHEEL;
            ge.button_button = 4;
            ge.button_x = e.button.x;
            ge.button_y = e.button.y;
            ge.wheel_x = 0.0f;
            ge.wheel_y = 1.0f;
        } else if (e.button.button == 5) { // SDL_BUTTON_WHEELDOWN (button 5)
            ge.type = event_type::MOUSE_WHEEL;
            ge.button_button = 5;
            ge.button_x = e.button.x;
            ge.button_y = e.button.y;
            ge.wheel_x = 0.0f;
            ge.wheel_y = -1.0f;
        } else {
            ge.type = event_type::MOUSE_BUTTON_DOWN;
            ge.button_button = e.button.button;
            ge.button_x = e.button.x;
            ge.button_y = e.button.y;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (e.button.button == 4 || e.button.button == 5) {
            ge.type = event_type::NONE;
        } else {
            ge.type = event_type::MOUSE_BUTTON_UP;
            ge.button_button = e.button.button;
            ge.button_x = e.button.x;
            ge.button_y = e.button.y;
        }
        break;
    case SDL_MOUSEWHEEL:
        ge.type = event_type::MOUSE_WHEEL;
        ge.wheel_x = static_cast<float>(e.wheel.x);
        ge.wheel_y = static_cast<float>(e.wheel.y);
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (ge.wheel_y == 0.0f && e.wheel.preciseY != 0.0f) {
            ge.wheel_y = e.wheel.preciseY;
        }
        if (ge.wheel_x == 0.0f && e.wheel.preciseX != 0.0f) {
            ge.wheel_x = e.wheel.preciseX;
        }
#endif
#if defined(SDL_MOUSEWHEEL_FLIPPED)
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            ge.wheel_x = -ge.wheel_x;
            ge.wheel_y = -ge.wheel_y;
        }
#endif
        if (ge.wheel_y > 0.0f) {
            ge.button_button = 4;
        } else if (ge.wheel_y < 0.0f) {
            ge.button_button = 5;
        }
        break;
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
            ge.type = event_type::QUIT;
            break;
        }
        ge.type = event_type::WINDOW_EVENT;
        ge.window_id = e.window.windowID;
        ge.window_data1 = e.window.data1;
        ge.window_data2 = e.window.data2;
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            ge.window_event = window_event_type::FOCUS_LOST;
        else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            ge.window_event = window_event_type::FOCUS_GAINED;
        else if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            ge.window_event = window_event_type::SIZE_CHANGED;
        break;
    default:
        ge.type = event_type::NONE;
        break;
    }
    return ge;
}

} // namespace

std::list<game_event> poll_display_events() {
    std::list<game_event> events;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT || (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)) {
            dftd_startup_log("poll_display_events: SDL_PollEvent returned QUIT or WINDOW_CLOSE");
        }
        game_event ge = sdl_to_game(e);
        if (ge.type != event_type::NONE)
            events.push_back(ge);
    }
    return events;
}

const char* get_key_name(key_code k) {
    const char* s = SDL_GetKeyName(static_cast<SDL_Keycode>(k));
    return s ? s : "";
}

key_mod display_get_mod_state() {
    return static_cast<key_mod>(SDL_GetModState());
}

bool display_is_key_down(key_code k) {
    int numkeys = 0;
    const Uint8* state = SDL_GetKeyboardState(&numkeys);
    SDL_Scancode sc = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(k));
    return state && static_cast<int>(sc) < numkeys && state[sc];
}

void display_save_bmp_rgb(const char* path, const uint8_t* rgb, int w, int h) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rmask = 0xff000000, gmask = 0x00ff0000, bmask = 0x0000ff00;
#else
    Uint32 rmask = 0x000000ff, gmask = 0x0000ff00, bmask = 0x00ff0000;
#endif
    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(
        const_cast<uint8_t*>(rgb), w, h, 24, w * 3, rmask, gmask, bmask, 0);
    if (surf) {
        SDL_SaveBMP(surf, path);
        SDL_FreeSurface(surf);
    }
}

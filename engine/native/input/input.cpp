module;

#include <SDL3/SDL.h>

module input;

import core.stdtypes;

namespace draco::input {
// Static globals
static bool g_keys[256]{};
static bool g_mouse_captured;
static f32 g_mouse_dx = 0;
static f32 g_mouse_dy = 0;

static Key map_sdl_key(SDL_Keycode k) {
	switch (k) {
	case SDLK_W:      return Key::W;
	case SDLK_A:      return Key::A;
	case SDLK_S:      return Key::S;
	case SDLK_D:      return Key::D;
	case SDLK_SPACE:  return Key::Space;
	case SDLK_LCTRL:  return Key::Ctrl;
	case SDLK_LSHIFT: return Key::Shift;
	case SDLK_ESCAPE: return Key::Escape;
	default:          return Key::Invalid;
	}
}

void begin_frame() {
	g_mouse_dx = 0;
	g_mouse_dy = 0;
}

void end_frame() { }

void set_key(Key key, bool down) {
	if (key == Key::Invalid) { return; }

	g_keys[(u16)key] = down;
}

bool is_down(Key key) {
	return g_keys[(u16)key];
}

void process_event(SDL_Event const &e) {
	switch (e.type) {
	case SDL_EVENT_KEY_DOWN:     set_key(map_sdl_key(e.key.key), true); break;

	case SDL_EVENT_KEY_UP:       set_key(map_sdl_key(e.key.key), false); break;

	case SDL_EVENT_MOUSE_MOTION: set_mouse_delta((f32)e.motion.xrel, (f32)e.motion.yrel); break;
	}
}

void set_mouse_captured(SDL_Window *window, bool enabled) {
	g_mouse_captured = enabled;

	SDL_SetWindowRelativeMouseMode(window, enabled);
	SDL_SetWindowMouseGrab(window, enabled);
}

bool is_mouse_captured() {
	return g_mouse_captured;
}

void set_mouse_delta(f32 dx, f32 dy) {
	g_mouse_dx += dx;
	g_mouse_dy += dy;
}

f32 get_mouse_dx() {
	return g_mouse_dx;
}

f32 get_mouse_dy() {
	return g_mouse_dy;
}
} // namespace draco::input

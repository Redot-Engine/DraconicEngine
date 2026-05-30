module;

#include <cstdint>
#include <SDL3/SDL.h>

export module input;

import core.stdtypes;

export namespace draco::input {
enum class Key : u16 {
	// TODO: A small set of keys should be okay for now but this needs to be updated later as per our needs
	W,
	A,
	S,
	D,
	Space,
	Ctrl,
	Shift,
	Escape,
	Invalid
};

// Note: This isn't the same as RHI
void begin_frame();
void end_frame();

void set_key(Key key, bool down);
bool is_down(Key key);

void process_event(SDL_Event const &e);

void set_mouse_captured(SDL_Window *window, bool enabled);
bool is_mouse_captured();

void set_mouse_delta(f32 dx, f32 dy);

f32 get_mouse_dx();
f32 get_mouse_dy();
} // namespace draco::input

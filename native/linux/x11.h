#pragma once
#include <thread>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

namespace priv_os_x11 {
	struct Frame {
		xcb_window_t window;
		xcb_window_t frame;
		Frame(xcb_window_t window, xcb_window_t frame) : window(window), frame(frame) {}
	};

	extern xcb_connection_t* connection;
	extern xcb_window_t rootWindow;
	extern xcb_ewmh_connection_t ewmhConnection;

	/**
	 * Ensure that we have connection to X11
	 */
	void ensureConnection();

	xcb_atom_t getAtom(const char* name);
}

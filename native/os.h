/**
 * This header file defines the set of platform-specific functions required by alt1
 * 
 * There is a corresponding source file and reference in binding.gyp for each platform
 */

#pragma once

#include "util.h"
#ifdef OS_WIN
#include "os_win.h"
#elif OS_LINUX
#include "os_x11_linux.h"
#elif OS_MAC
#include "os_mac.h"
#else
#error Platform not supported
#endif

#ifndef DEFAULT_OSRAWWINDOW
#define DEFAULT_OSRAWWINDOW 0
#endif


struct CaptureRect {
	JSRectangle rect;
	void* data;
	size_t size;
	CaptureRect(void* data, size_t size, JSRectangle rect) :rect(rect), data(data), size(size) {}
};


//TODO parameter type of objectwrap
struct OSWindow {
	OSRawWindow handle = DEFAULT_OSRAWWINDOW;
public:
	OSWindow() = default;
	OSWindow(OSRawWindow hnd) : handle(hnd) {}
	// The bounderies of the window including title bar and borders in screen coordinates
	JSRectangle GetBounds();
	// The boundaries of the client area of the window, without any title bar of borders
	JSRectangle GetClientBounds();
	// Is the handle valid and does the window still exist
	bool IsValid();
	// Gets the text in the window title bar
	string GetTitle();
	// Convert the internal window handle to a javascript Bigint
	Napi::Value ToJS(Napi::Env env);
	// Check if the value is of correct type and convert it to an OSWindow
	static OSWindow FromJsValue(const Napi::Value jsval);

	bool operator==(const OSWindow& other) const;
	bool operator<(const OSWindow& other) const;
};

/**
 * Reparent the 'wnd' to any other window 'parent', parent can belong to any process.
 * This makes 'wnd' always show up in front of 'parent'
 * Revert any reparenting if 'parent' is 0 
 * 
 * On linux a window can only be reparented before it is shown for the first time
 */
void OSSetWindowParent(OSWindow wnd, OSWindow parent);

//TODO: Make this function parametrized so it no longer contains constants referencing RuneScape
/**
 * Gets a list of matching rs windows
 */
std::vector<OSWindow> OSGetRsHandles();

/**
 * capture the target wnd into each of the rects struct, each of the subrectangles should be captured
 * from the same frame
 */
void OSCaptureMulti(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env);

/**
 * Get the currently active window on the desktop
 */
OSWindow OSGetActiveWindow();

/**
 * Returns true when the left/main mouse button is down, even in another process and regardless of message pump state
 */
bool OSGetMouseState();


enum class WindowEventType { Move, Close, Show, Click };
const std::map<std::string, WindowEventType> windowEventTypes = {
	{"move",WindowEventType::Move},
	{"close",WindowEventType::Close},
	{"show",WindowEventType::Show},
	{"click",WindowEventType::Click}
};

/**
 * Listen for window events in windows owned by another process or the desktop.
 * @param wnd the window to listen for or the null window to listen for all windows/the desktop
 */
void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb);

/**
 * Remove an event listener, wnd, type and cb must match
 */
void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb);

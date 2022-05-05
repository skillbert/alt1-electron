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
	OSRawWindow hwnd = DEFAULT_OSRAWWINDOW;
public:
	OSWindow() = default;
	OSWindow(OSRawWindow wnd) : hwnd(wnd) {}
	void SetBounds(JSRectangle bounds);
	int GetPid();
	JSRectangle GetBounds();
	JSRectangle GetClientBounds();
	bool IsValid();
	string GetTitle();
	Napi::Value ToJS(Napi::Env env);

	static OSWindow FromJsValue(const Napi::Value jsval);

	bool operator==(const OSWindow& other) const;
	bool operator<(const OSWindow& other) const;
};

OSWindow OSFindMainWindow(unsigned long process_id);
void OSSetWindowParent(OSWindow wnd, OSWindow parent);
std::vector<OSWindow> OSGetRsHandles();
void OSCaptureMulti(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env);
OSWindow OSGetActiveWindow();


enum WindowDragPhase { Start, Moving, End };

enum class WindowEventType { Move, Close, Show, Click };
const std::map<std::string, WindowEventType> windowEventTypes = {
	{"move",WindowEventType::Move},
	{"close",WindowEventType::Close},
	{"show",WindowEventType::Show},
	{"click",WindowEventType::Click}
};

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb);

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb);

#pragma once

#include "util.h"


struct CaptureRect {
	JSRectangle rect;
	void* data;
	size_t size;
	CaptureRect(void* data, size_t size, JSRectangle rect) :rect(rect), data(data), size(size) {}
};


//TODO parameter type of objectwrap
struct OSWindow {
	OSRawWindow hwnd = 0;
public:
	OSWindow() = default;
	OSWindow(OSRawWindow wnd) :hwnd(wnd) {}
	void SetBounds(JSRectangle bounds);
	int GetPid();
	JSRectangle GetBounds();
	JSRectangle GetClientBounds();
	bool IsValid();
	string GetTitle();
	Napi::Value ToJS(Napi::Env env);

	static OSWindow OSWindow::FromJsValue(const Napi::Value jsval);

	bool operator==(const OSWindow& other)const = default;
	bool operator<(const OSWindow& other) const { return this->hwnd < other.hwnd; }
};

vector<uint32_t> OSGetProcessesByName(std::wstring name, uint32_t parentpid);

OSWindow OSFindMainWindow(unsigned long process_id);

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h);
void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h);
void OSCaptureDesktopMulti(vector<CaptureRect> rects);
void OSCaptureWindowMulti(vector<CaptureRect> rects);
string OSGetProcessName(int pid);


enum WindowDragPhase { Start, Moving, End };

enum class WindowEventType { Move, Close,Show };
const std::map<std::string, WindowEventType> windowEventTypes = {
	{"move",WindowEventType::Move},
	{"close",WindowEventType::Close},
	{"show",WindowEventType::Show}
};

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb);

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb);
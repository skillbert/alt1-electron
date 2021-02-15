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


enum WindowDragPhase { Start, Moving, End };

enum class WindowEventType { Move, Close };
const std::map<std::string, WindowEventType> windowEventTypes = {
	{"move",WindowEventType::Move},
	{"close",WindowEventType::Close}
};

//TODO find out why i can't put functionreference's in std::list
struct FunctionRefWrap {
	Napi::FunctionReference ref;
	FunctionRefWrap(Napi::Function fn) {
		ref = Napi::Persistent(fn);
	}
};

struct TrackedEvent {
	OSWindow wnd;
	//TODO make OS-agnotic type
	void* eventhandle;
	std::vector<Napi::FunctionReference> listeners;
	void add(Napi::Function cb) {
		listeners.push_back(Napi::Persistent(cb));
	}
	bool remove(Napi::Function cb) {
		for (auto it = listeners.begin(); it != listeners.end(); it++) {
			//TODO figure out proper way to compare these
			if (*it == Napi::Persistent(cb)) {
				listeners.erase(it);
				break;
			}
		}
		return listeners.size() == 0;
	}
	TrackedEvent(OSWindow wnd, WindowEventType type);
	~TrackedEvent();
	bool operator==(const TrackedEvent& other)const = default;
	//delete copy-assign
	TrackedEvent(const TrackedEvent&) = delete;
	TrackedEvent& operator=(const TrackedEvent& other) = delete;
	//allow move assign
	TrackedEvent(TrackedEvent&& other) noexcept { listeners = std::move(other.listeners); };
	TrackedEvent& operator=(TrackedEvent&& other) noexcept { listeners = std::move(other.listeners); }
};

//TODO maybe change to per process
std::map<OSWindow, std::unordered_map<WindowEventType, TrackedEvent>> windowHandlers;

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	auto wndevents = &windowHandlers[wnd];
	auto handlerpos = wndevents->find(type);
	if (handlerpos == wndevents->end()) {
		wndevents->emplace(std::make_pair(type, TrackedEvent(wnd, type)));
	}
	auto handler = &wndevents->at(type);
	handler->add(cb);
}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	auto wndpos = windowHandlers.find(wnd);
	if (wndpos == windowHandlers.end()) { return; }
	auto wndevents = &wndpos->second;
	auto handlerpos = wndevents->find(type);
	if (handlerpos == wndevents->end()) { return; }
	auto handler = &handlerpos->second;
	if (handler->remove(cb)) {
		wndevents->erase(handlerpos);
		if (wndevents->size() == 0) {
			windowHandlers.erase(wndpos);
		}
	}
}

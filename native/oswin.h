#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
typedef HWND OSRawWindow;
#include "os.h"
#include "openglcapture/defs.h"

/*
* Currently using the Ansi version of windows api's as v8 expects utf8, this will work for ascii but will garble anything outside ascii
*/

void OSWindow::SetBounds(JSRectangle bounds) {
	SetWindowPos(hwnd, NULL, bounds.x, bounds.y, bounds.width, bounds.height, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

int OSWindow::GetPid() {
	uint32_t pid = 0;
	GetWindowThreadProcessId(this->hwnd, (DWORD*)&pid);
	return pid;
}
JSRectangle OSWindow::GetBounds() {
	RECT rect;
	if (!GetWindowRect(hwnd, &rect)) { return JSRectangle(0, 0, 0, 0); }
	return JSRectangle(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
JSRectangle OSWindow::GetClientBounds() {
	RECT rect;
	GetClientRect(hwnd, &rect);
	//this rect to point cast is actually specified in winapi docs and has special behavior
	MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)&rect, 2);
	return JSRectangle(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
bool OSWindow::IsValid() {
	if (!hwnd) { return false; }
	return IsWindow(hwnd);
}
string OSWindow::GetTitle() {
	int len = GetWindowTextLengthA(hwnd);
	if (len == 0) { return string(""); }
	vector<char> buf(len + 1);
	GetWindowTextA(hwnd, &buf[0], len + 1);
	return string(&buf[0]);
}
OSWindow OSWindow::FromJsValue(const Napi::Value jsval) {
	auto handle = jsval.As<Napi::BigInt>();
	bool lossless;
	auto handleint = handle.Uint64Value(&lossless);
	if (!lossless) { Napi::RangeError::New(jsval.Env(), "Invalid handle").ThrowAsJavaScriptException(); }
	return OSWindow((HWND)handleint);
}

vector<uint32_t> OSGetProcessesByName(std::string name, uint32_t parentpid)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process = {};
	process.dwSize = sizeof(process);

	//Walk through all processes
	bool first = true;
	vector<uint32_t> res;
	while (first ? Process32First(snapshot, &process) : Process32Next(snapshot, &process))
	{
		first = false;
		if (std::string(process.szExeFile) == name && (parentpid == 0 || parentpid == process.th32ParentProcessID))
		{
			res.push_back(process.th32ProcessID);
		}
	}
	CloseHandle(snapshot);
	return res;
}

string OSGetProcessName(uint32_t pid) {
	char buffer[MAX_PATH] = { 0 };
	DWORD len = MAX_PATH;
	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	QueryFullProcessImageNameA((HMODULE)hProc, 0, buffer, &len);
	CloseHandle(hProc);
	string exename = string(buffer);
	auto i = exename.find_last_of('\\');
	if (i != string::npos) { exename = exename.substr(i + 1); }
	return exename;
}

struct WinFindMainWindow_data
{
	unsigned long process_id;
	HWND window_handle;
};

BOOL WinIsMainWindow(HWND handle)
{
	return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK WinFindMainWindow_callback(HWND handle, LPARAM lParam)
{
	WinFindMainWindow_data& data = *(WinFindMainWindow_data*)lParam;
	unsigned long process_id = 0;
	GetWindowThreadProcessId(handle, &process_id);
	if (data.process_id != process_id || !WinIsMainWindow(handle))
		return TRUE;
	data.window_handle = handle;
	return FALSE;
}

OSWindow OSFindMainWindow(uint32_t process_id)
{
	WinFindMainWindow_data data;
	data.process_id = process_id;
	data.window_handle = 0;
	EnumWindows(WinFindMainWindow_callback, (LPARAM)&data);
	return OSWindow(data.window_handle);
}

void OSCaptureDesktopMulti(vector<CaptureRect> rects) {
	for (auto const& capt : rects) {
		OSCaptureDesktop(capt.data, capt.size, capt.rect.x, capt.rect.y, capt.rect.width, capt.rect.height);
	}
}

void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
	for (auto const& capt : rects) {
		OSCaptureWindow(capt.data, capt.size, wnd, capt.rect.x, capt.rect.y, capt.rect.width, capt.rect.height);
	}
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
	//show behind parent, then show parent behind self (no way to show in front in winapi)
	if (parent.hwnd != 0) {
		SetWindowPos(wnd.hwnd, parent.hwnd, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		SetWindowPos(parent.hwnd, wnd.hwnd, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
	//TODO there was a reason to do this instead of using the nicely named SetParent window, find out why
	SetWindowLongPtr(wnd.hwnd, GWLP_HWNDPARENT, (uint64_t)parent.hwnd);
}

//TODO this is stupid
void flipBGRAtoRGBA(void* data, size_t len) {
	unsigned char* index = (unsigned char*)data;
	unsigned char* end = index + len;
	for (; index < end; index += 4) {
		unsigned char tmp = index[0];
		//TODO profile this, does the compiler do fancy simd swizzles if we self assign 1 and 3 as well?
		index[0] = index[2];
		index[2] = tmp;
	}
}

void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h) {
	HDC hdc = GetDC(wnd.hwnd);
	HDC hDest = CreateCompatibleDC(hdc);
	HBITMAP hbDesktop = CreateCompatibleBitmap(hdc, w, h);
	//TODO hbDekstop can be null (in alt1 code at least)
	HGDIOBJ old = SelectObject(hDest, hbDesktop);

	//copy desktop to bitmap
	BitBlt(hDest, 0, 0, w, h, hdc, x, y, SRCCOPY);

	BITMAPINFOHEADER bmi = { 0 };
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biPlanes = 1;
	bmi.biBitCount = 32;
	bmi.biWidth = w;
	bmi.biHeight = -h;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = 0;

	//TODO safeguard buffer overflow somehow
	GetDIBits(hdc, hbDesktop, 0, h, target, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
	flipBGRAtoRGBA(target, maxlength);

	//release everything
	SelectObject(hDest, old);
	ReleaseDC(NULL, hdc);
	DeleteObject(hbDesktop);
	DeleteDC(hDest);
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	return Napi::BigInt::New(env, (uint64_t)hwnd);
}


void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h)
{
	return OSCaptureWindow(target, maxlength, NULL, x, y, w, h);
}

void HookProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);

enum class WindowsEventGroup { System, Object };
struct WindowsEventHook {
	HWND hwnd;
	WindowsEventGroup group;
	HWINEVENTHOOK eventhandle;
	WindowsEventHook(HWND hwnd, WindowsEventGroup group) :hwnd(hwnd), group(group) {
		DWORD pid = 0;
		if (hwnd) {
			GetWindowThreadProcessId(hwnd, &pid);
		}
		switch (group) {
		case WindowsEventGroup::Object:
			eventhandle = SetWinEventHook(0x0000, 0x0030, 0, HookProc, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
			break;
		case WindowsEventGroup::System:
			eventhandle = SetWinEventHook(0x8000, 0x800B, 0, HookProc, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
			break;
		default:
			assert(false);
		}
	}
	~WindowsEventHook() {
		UnhookWinEvent(eventhandle);
	}
	static std::shared_ptr<WindowsEventHook> GetHook(HWND hwnd, WindowsEventGroup group);
};

struct TrackedEvent {
	OSWindow wnd;
	WindowEventType type;
	Napi::FunctionReference callback;
	std::vector<std::shared_ptr<WindowsEventHook>> hooks;
	TrackedEvent(OSWindow wnd, WindowEventType type, Napi::Function cb);
	~TrackedEvent();
	//allow move assign
	TrackedEvent(TrackedEvent&& other) noexcept { 
		*this = std::move(other);
	};
	TrackedEvent& operator=(TrackedEvent&& other) noexcept {
		this->hooks = other.hooks;
		this->type = other.type;
		this->wnd = other.wnd;
		callback = std::move(other.callback);
		return *this;
	}
	//delete copy-assign
	TrackedEvent(const TrackedEvent&) = delete;
	TrackedEvent& operator=(const TrackedEvent& other) = delete;
};


std::vector<TrackedEvent> windowHandlers;

std::shared_ptr<WindowsEventHook> WindowsEventHook::GetHook(HWND hwnd, WindowsEventGroup group) {
	for (auto& handler : windowHandlers) {
		for (auto& hook : handler.hooks) {
			if (hook->hwnd == hwnd && hook->group == group) {
				return hook;
			}
		}
	}
	return std::make_shared<WindowsEventHook>(hwnd, group);
}

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	auto ev = TrackedEvent(wnd, type, cb);
	windowHandlers.push_back(std::move(ev));
}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	const TrackedEvent* ev = nullptr;
	for (auto it = windowHandlers.begin(); it != windowHandlers.end(); it++) {
		if (it->type == type && it->wnd == wnd && it->callback == Napi::Persistent(cb)) {
			windowHandlers.erase(it);
			break;
		}
	}
}

void HookProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
	OSWindow wnd(hwnd);

	vector<Napi::Value> args;
	switch (event) {
	case EVENT_OBJECT_STATECHANGE:
	case EVENT_OBJECT_DESTROY:
		for (const auto& h : windowHandlers) {
			if (hwnd == h.wnd.hwnd && h.type == WindowEventType::Close) {
				auto env = h.callback.Env();
				Napi::HandleScope scope(env);
				//TODO more intelligent way to deal with js land callback errors?
				try { h.callback.MakeCallback(env.Global(), {}); }
				catch (...) {}
			}
		}
		break;
	case EVENT_SYSTEM_MOVESIZEEND:
	case EVENT_SYSTEM_MOVESIZESTART:
	case EVENT_OBJECT_LOCATIONCHANGE: {
		JSRectangle bounds = wnd.GetBounds();
		const char* phase = (event == EVENT_SYSTEM_MOVESIZEEND ? "end" : event == EVENT_SYSTEM_MOVESIZESTART ? "start" : "moving");
		for (const auto& h : windowHandlers) {
			if (hwnd == h.wnd.hwnd && h.type == WindowEventType::Move) {
				auto env = h.callback.Env();
				Napi::HandleScope scope(env);
				try { h.callback.MakeCallback(env.Global(), { bounds.ToJs(env),Napi::String::New(env, phase) }); }
				catch (...) {}
			}
		}
		break; }
	case EVENT_OBJECT_UNCLOAKED:
	case EVENT_SYSTEM_FOREGROUND:
	case EVENT_OBJECT_SHOW: {
		for (const auto& h : windowHandlers) {
			if ((h.wnd.hwnd == 0 || hwnd == h.wnd.hwnd) && h.type == WindowEventType::Show) {
				auto env = h.callback.Env();
				Napi::HandleScope scope(env);
				try { h.callback.MakeCallback(env.Global(), { Napi::BigInt::New(env,(uint64_t)hwnd),Napi::Number::New(env,event) }); }
				catch (...) {}
			}
		}
		break; }
	}
}

TrackedEvent::TrackedEvent(OSWindow wnd, WindowEventType type,Napi::Function cb) {
	this->wnd = wnd;
	this->type = type;
	this->callback = Napi::Persistent(cb);
	this->callback.SuppressDestruct();
	DWORD pid;
	//TODO error handling
	GetWindowThreadProcessId(wnd.hwnd, &pid);
	switch (type) {
	case WindowEventType::Move:
		this->hooks = { 
			WindowsEventHook::GetHook(wnd.hwnd,WindowsEventGroup::Object),
			WindowsEventHook::GetHook(wnd.hwnd,WindowsEventGroup::System)
		};
		break;
	case WindowEventType::Close:
		this->hooks = {
			WindowsEventHook::GetHook(wnd.hwnd,WindowsEventGroup::Object)
		};
		break;
	case WindowEventType::Show:
		//TODO don't need both here
		this->hooks = {
			WindowsEventHook::GetHook(wnd.hwnd,WindowsEventGroup::Object),
			WindowsEventHook::GetHook(wnd.hwnd,WindowsEventGroup::System),
		};
		break;
	default:
		assert(false);
	}
}
TrackedEvent::~TrackedEvent() {
	this->callback.Reset();
}
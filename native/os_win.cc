
#include "os.h"
#include <TlHelp32.h>
#include "../libs/Alt1Native.h"

/*
* Currently using the Ansi version of windows api's as v8 expects utf8, this will work for ascii but will garble anything outside ascii
*/

OSWindow OSGetActiveWindow() {
	return OSWindow(GetForegroundWindow());
}

void OSWindow::SetBounds(JSRectangle bounds) {
	SetWindowPos(this->handle, NULL, bounds.x, bounds.y, bounds.width, bounds.height, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	return Napi::BigInt::New(env, (uint64_t)this->handle);
}

JSRectangle OSWindow::GetBounds() {
	RECT rect;
	if (!GetWindowRect(this->handle, &rect)) { return JSRectangle(0, 0, 0, 0); }
	return JSRectangle(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
JSRectangle OSWindow::GetClientBounds() {
	RECT rect;
	GetClientRect(this->handle, &rect);
	//this rect to point cast is actually specified in winapi docs and has special behavior
	MapWindowPoints(this->handle, HWND_DESKTOP, (LPPOINT)&rect, 2);
	return JSRectangle(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
bool OSWindow::IsValid() {
	if (!this->handle) { return false; }
	return IsWindow(this->handle);
}
string OSWindow::GetTitle() {
	int len = GetWindowTextLengthA(this->handle);
	if (len == 0) { return string(""); }
	vector<char> buf(len + 1);
	GetWindowTextA(this->handle, &buf[0], len + 1);
	return string(&buf[0]);
}
OSWindow OSWindow::FromJsValue(const Napi::Value jsval) {
	auto handle = jsval.As<Napi::BigInt>();
	bool lossless;
	auto handleint = handle.Uint64Value(&lossless);
	if (!lossless) { throw Napi::RangeError::New(jsval.Env(), "Invalid handle"); }
	return OSWindow((HWND)handleint);
}

bool OSWindow::operator==(const OSWindow& other) const {
	return this->handle == other.handle;
}

bool OSWindow::operator<(const OSWindow& other) const {
	return this->handle < other.handle;
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

OSWindow OSFindMainWindow(unsigned long process_id)
{
	WinFindMainWindow_data data;
	data.process_id = process_id;
	data.window_handle = 0;
	EnumWindows(WinFindMainWindow_callback, (LPARAM)&data);
	return OSWindow(data.window_handle);
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
	//show behind parent, then show parent behind self (no way to show in front in winapi)
	if (parent.handle != 0) {
		SetWindowPos(wnd.handle, parent.handle, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		SetWindowPos(parent.handle, wnd.handle, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
	//TODO there was a reason to do this instead of using the nicely named SetParent window, find out why
	SetWindowLongPtr(wnd.handle, GWLP_HWNDPARENT, (uint64_t)parent.handle);
}

std::vector<OSWindow> OSGetRsHandles() {
	std::vector<OSWindow> out;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process = {};
	process.dwSize = sizeof(process);

	//Walk through all processes
	if (Process32First(snapshot, &process)) {
		do {
			if (std::string(process.szExeFile) == "rs2client.exe") {
				WinFindMainWindow_data data;
				data.process_id = process.th32ProcessID;
				data.window_handle = 0;
				EnumWindows(WinFindMainWindow_callback, (LPARAM)&data);
				out.push_back(OSWindow(data.window_handle));
			}
		} while (Process32Next(snapshot, &process));
	}
	CloseHandle(snapshot);
	return out;
}

void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h) {
	HDC hdc = GetDC(wnd.handle);
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
	//TODO i don't think this was necessary in c# alt1, check if this can be skipped
	//TODO perf merge these two loops
	fillImageOpaque(target, maxlength);

	//release everything
	SelectObject(hDest, old);
	ReleaseDC(NULL, hdc);
	DeleteObject(hbDesktop);
	DeleteDC(hDest);
}

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h)
{
	return OSCaptureWindow(target, maxlength, NULL, x, y, w, h);
}

void OSCaptureOpenGLMulti(OSWindow wnd, vector<CaptureRect> rects, Napi::Env env) {
	//TODO cache this handle!!!
	auto handle = Alt1Native::HookProcess(wnd.handle);
	vector<JSRectangle> rawrects(rects.size());
	for (int i = 0; i < rects.size(); i++) {
		rawrects[i] = rects[i].rect;
	}
	auto pixeldata = Alt1Native::CaptureMultiple(handle, &rawrects[0], rawrects.size());
	if (!pixeldata) {
		char errtext[200] = { 0 };
		int len = Alt1Native::GetDebug(errtext, sizeof(errtext) - 1);
		throw Napi::Error::New(env, string() + "Failed to capture, native error: " + errtext);
	}
	//TODO get rid of copy somehow? (src memory is shared ipc memory so not trivial)
	size_t offset = 0;
	for (int i = 0; i < rects.size(); i++) {
		//TODO use correct pixel format in injectdll so this flip isnt needed
		//copy and flip in one pass
		flipBGRAtoRGBA(rects[i].data, pixeldata + offset, rects[i].size);
		offset += (size_t)rawrects[i].width * rawrects[i].height * 4;
	}
}

void OSCaptureMulti(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env) {
	switch (mode) {
	case CaptureMode::Desktop: {
		//TODO double check and document desktop 0 special case
		auto offset = wnd.GetClientBounds();
		auto mapped = vector<CaptureRect>(rects);
		for (auto& capt : mapped) {
			OSCaptureDesktop(capt.data, capt.size, capt.rect.x + offset.x, capt.rect.y + offset.y, capt.rect.width, capt.rect.height);
		}
		break;
	}
	case CaptureMode::Window:
		for (auto const& capt : rects) {
			OSCaptureWindow(capt.data, capt.size, wnd, capt.rect.x, capt.rect.y, capt.rect.width, capt.rect.height);
		}
		break;
	case CaptureMode::OpenGL: {
		OSCaptureOpenGLMulti(wnd, rects, env);
		break;
	}
	default:
		throw Napi::RangeError::New(env, "Capture mode not supported");
	}
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
			eventhandle = SetWinEventHook(0x8000, 0x800B, 0, HookProc, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
			break;
		case WindowsEventGroup::System:
			eventhandle = SetWinEventHook(0x0000, 0x0030, 0, HookProc, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
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
	int lastCalledId = 0;//used to determine if this callback was called already
	Napi::FunctionReference callback;
	std::vector<std::shared_ptr<WindowsEventHook>> hooks;
	TrackedEvent(OSWindow wnd, WindowEventType type, Napi::Function cb);
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

//used to determine which callbacks we called already
int hookprocCount = 0;

//need this weird function to deal with the possibility of the list being modified from the js callback
//return true from cond if the handler matches and should be called
template<typename F,typename COND>
void iterateHandlers(COND cond, F tracker) {
	hookprocCount++;
	bool changed;
	do {
		changed = false;
		for (auto it = windowHandlers.begin(); it != windowHandlers.end(); it++) {
			if (it->lastCalledId != hookprocCount && cond(*it)) {
				it->lastCalledId = hookprocCount;
				changed = true;
				tracker(*it);
				break;
			}
		}
	} while (changed);
}

void HookProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
	OSWindow wnd(hwnd);

	vector<Napi::Value> args;
	switch (event) {
	case EVENT_OBJECT_STATECHANGE:
	case EVENT_OBJECT_DESTROY:
		iterateHandlers(
			[&](const TrackedEvent& h) {return hwnd == h.wnd.handle && h.type == WindowEventType::Close; },
			[&](const TrackedEvent& h) {
				auto env = h.callback.Env();
				Napi::HandleScope scope(env);
				//TODO more intelligent way to deal with js land callback errors?
				try { h.callback.MakeCallback(env.Global(), {}); }
				catch (...) {}
			});
		break;
	case EVENT_SYSTEM_CAPTURESTART: {
		auto windowhwnd = GetAncestor(hwnd, GA_ROOT);
		iterateHandlers(
			[&](const TrackedEvent& h) {return windowhwnd == h.wnd.handle && h.type == WindowEventType::Click; },
			[&](const TrackedEvent& h) {
				auto env = h.callback.Env();
				Napi::HandleScope scope(env);
				try { h.callback.MakeCallback(env.Global(), {}); }
				catch (...) {}
			});
		break; }
	case EVENT_SYSTEM_MOVESIZESTART:
	case EVENT_SYSTEM_MOVESIZEEND:
	case EVENT_OBJECT_LOCATIONCHANGE: {
		JSRectangle bounds = wnd.GetBounds();
		const char* phase = (event == EVENT_SYSTEM_MOVESIZEEND ? "end" : event == EVENT_SYSTEM_MOVESIZESTART ? "start" : "moving");
		iterateHandlers(
			[&](const TrackedEvent& h) {return hwnd == h.wnd.handle && h.type == WindowEventType::Move; },
			[&](const TrackedEvent& h) {
				auto env = h.callback.Env();
				Napi::HandleScope scope(env);
				try { h.callback.MakeCallback(env.Global(), { bounds.ToJs(env),Napi::String::New(env, phase) }); }
				catch (...) {}
			});
		break; }
	case EVENT_OBJECT_CREATE: {
		if (hwnd != 0) {
			wchar_t className[32];
			char name[32];
			if (!GetClassNameW(hwnd, className, 32)) {
				if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)wname, -1, (LPSTR)&name, sizeof name, NULL, NULL) != 0) {
					if (strcmp(name, "RuneScape") == 0) {
						// The new object is in fact a RuneScape window
						DWORD pid = 0;
						GetWindowThreadProcessId(hwnd, &pid);
						iterateHandlers(
							[&](const TrackedEvent& h) {return (h.wnd.handle == 0 || hwnd == h.wnd.handle) && h.type == WindowEventType::Show; },
							[&](const TrackedEvent& h) {
								auto env = h.callback.Env();
								Napi::HandleScope scope(env);
								try { h.callback.MakeCallback(env.Global(), { Napi::BigInt::New(env,(uint64_t)hwnd),Napi::Number::New(env,event) }); }
								catch (...) {}
							});
						break; }
					}
				}
			}
			
		}
	}
}

TrackedEvent::TrackedEvent(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	this->wnd = wnd;
	this->type = type;
	this->callback = Napi::Persistent(cb);
	this->callback.SuppressDestruct();
	DWORD pid;
	//TODO error handling
	GetWindowThreadProcessId(wnd.handle, &pid);
	switch (type) {
	case WindowEventType::Click:
		this->hooks = {
			WindowsEventHook::GetHook(wnd.handle,WindowsEventGroup::System)
		};
		break;
	case WindowEventType::Move:
		this->hooks = {
			WindowsEventHook::GetHook(wnd.handle,WindowsEventGroup::Object),
			WindowsEventHook::GetHook(wnd.handle,WindowsEventGroup::System)
		};
		break;
	case WindowEventType::Close:
		this->hooks = {
			WindowsEventHook::GetHook(wnd.handle,WindowsEventGroup::Object)
		};
		break;
	case WindowEventType::Show:
		//TODO don't need both here, currently spamming basically all event under all windows (hwnd=0)
		this->hooks = {
			WindowsEventHook::GetHook(wnd.handle,WindowsEventGroup::Object),
			WindowsEventHook::GetHook(wnd.handle,WindowsEventGroup::System),
		};
		break;
	default:
		assert(false);
	}
}

#pragma once

/*
* Currently using the Ansi version of windows api's as v8 expects utf8, this will work for ascii but will garble anything outside ascii
*/

#include "os.h"

JSRectangle OSGetWindowBounds(OSWindow wnd) {
	RECT rect;
	if (!GetWindowRect(wnd.GetHwnd(), &rect)) {
		return MakeJSRect(0, 0, 0, 0);
	}
	return MakeJSRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

string OSGetWindowTitle(OSWindow wnd) {
	int len = GetWindowTextLengthA(wnd.GetHwnd());
	if (len == 0) { return string(""); }
	vector<char> buf(len+1);
	GetWindowTextA(wnd.GetHwnd(), &buf[0], len + 1);
	return string(&buf[0]);
}

bool OSIsWindow(OSWindow wnd) {
	return IsWindow(wnd.GetHwnd());
}

void OSSetWindowBounds(OSWindow wnd, JSRectangle bounds) {
	SetWindowPos(wnd.GetHwnd(), NULL, bounds.x, bounds.y, bounds.width, bounds.height, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

vector<DWORD> OSGetProcessesByName(std::wstring name, DWORD parentpid)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process = {};
	process.dwSize = sizeof(process);

	//Walk through all processes
	bool first = true;
	vector<DWORD> res;
	while (first ? Process32First(snapshot, &process) : Process32Next(snapshot, &process))
	{
		first = false;
		if (std::wstring(process.szExeFile) == name && (parentpid == 0 || parentpid == process.th32ParentProcessID))
		{
			res.push_back(process.th32ProcessID);
		}
	}
	CloseHandle(snapshot);
	return res;
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

HWND OSFindMainWindow(unsigned long process_id)
{
	WinFindMainWindow_data data;
	data.process_id = process_id;
	data.window_handle = 0;
	EnumWindows(WinFindMainWindow_callback, (LPARAM)&data);
	return data.window_handle;
}

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h)
{
	HDC hdc = GetDC(NULL);
	HDC hDest = CreateCompatibleDC(hdc);
	HBITMAP hbDesktop = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(hDest, hbDesktop);

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

	//TODO safeguard maxlength somehow
	GetDIBits(hdc, hbDesktop, 0, h, target, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

	//release everything
	ReleaseDC(NULL, hdc);
	DeleteObject(hbDesktop);
	DeleteDC(hDest);
}

struct WinWindowTracker:public OSWindowTracker {
private:
	WindowListener* cb;
	OSWindow wnd;
	vector<HWINEVENTHOOK> hooks;
public:
	WinWindowTracker(OSWindow wnd, WindowListener* cb);
	~WinWindowTracker();
	void HookProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);
};

std::map<HWINEVENTHOOK, WinWindowTracker*> eventmap;

void CALLBACK HookProcRelay(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
	auto target = eventmap.find(hWinEventHook);
	assert(target != eventmap.end());
	target->second->HookProc(hWinEventHook, event, hwnd, idObject, idChild, idEventThread, dwmsEventTime);
}

WinWindowTracker::WinWindowTracker(OSWindow wnd, WindowListener* cb) :cb(cb) {
	this->wnd = wnd;
	DWORD pid;
	GetWindowThreadProcessId(wnd.GetHwnd(), &pid);
	auto hook = SetWinEventHook(0x0000, 0x0030, 0, HookProcRelay, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	auto hook2 = SetWinEventHook(0x8000, 0x800B, 0, HookProcRelay, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	eventmap.insert_or_assign(hook, this);
	eventmap.insert_or_assign(hook2, this);
	hooks.push_back(hook);
	hooks.push_back(hook2);
}

WinWindowTracker::~WinWindowTracker() {
	for (auto it = hooks.begin(); it != hooks.end(); it++) {
		//TODO weird cast, not completely sure if winapi is being weird here or its me
		UnhookWindowsHookEx((HHOOK)*it);
		eventmap.erase(*it);
	}
}

void WinWindowTracker::HookProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
	if (hwnd == wnd.GetHwnd()) {
		if (event == EVENT_OBJECT_STATECHANGE || event == EVENT_OBJECT_DESTROY) {
			//TODO in c# code there is a bunch more logic for this one
			cb->OnWindowClose();
		}

		if (event == EVENT_SYSTEM_MOVESIZEEND) {
			cb->OnWindowMove(OSGetWindowBounds(wnd), WindowDragPhase::End);
		}
		if (event == EVENT_SYSTEM_MOVESIZESTART) {
			cb->OnWindowMove(OSGetWindowBounds(wnd), WindowDragPhase::Start);
		}
		if (event == EVENT_OBJECT_LOCATIONCHANGE) {
			cb->OnWindowMove(OSGetWindowBounds(wnd), WindowDragPhase::Moving);
		}
	}
}

class WinOsWindow :public OSWindow2 {
private:
	HWND hwnd;
public:
	WinOsWindow(HWND hwnd) {
		this->hwnd = hwnd;
	}
	void SetBounds(JSRectangle bounds) {
		SetWindowPos(hwnd, NULL, bounds.x, bounds.y, bounds.width, bounds.height, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	}
	virtual JSRectangle GetBounds() {
		RECT rect;
		if (!GetWindowRect(hwnd, &rect)) { return MakeJSRect(0, 0, 0, 0); }
		return MakeJSRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
	}
	virtual bool IsValid() {
		return IsWindow(hwnd);
	}
	string GetTitle() {
		return "";
	}
	static bool FromJS(const v8::Local<v8::Value> jsval, OSWindow2* out) {
		if (!jsval->IsArrayBufferView()) { return false; }
		auto buf = jsval.As<v8::ArrayBufferView>();
		if (buf->ByteLength() != sizeof(OSWindow)) { return false; }
		byte* ptr = (byte*)buf->Buffer()->GetContents().Data();
		ptr += buf->ByteOffset();
		//TODO double check bitness situation here (on 32bit as well?)
		*out = WinOsWindow(*(HWND*)ptr);
	}

};

std::unique_ptr<OSWindowTracker> OSNewWindowTracker(OSWindow wnd, WindowListener* cb) {
	return std::unique_ptr<OSWindowTracker>(new WinWindowTracker(wnd, cb));
}
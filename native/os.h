#pragma once

#include "util.h"


vector<DWORD> OSGetProcessesByName(std::wstring name, DWORD parentpid);

HWND OSFindMainWindow(unsigned long process_id);

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h);

bool OSIsWindow(OSWindow wnd);

JSRectangle OSGetWindowBounds(OSWindow wnd);

string OSGetWindowTitle(OSWindow wnd);

void OSSetWindowBounds(OSWindow wnd, JSRectangle bounds);

enum WindowDragPhase { Start, Moving, End };

 class WindowListener {
 public:
	 virtual void OnWindowMove(JSRectangle bounds, WindowDragPhase phase) = 0;
	 virtual void OnWindowClose() = 0;
};

struct OSWindowTracker {};

std::unique_ptr<OSWindowTracker> OSNewWindowTracker(OSWindow wnd, WindowListener* cb);

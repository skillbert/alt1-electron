#pragma once

#include "util.h"


vector<DWORD> OSGetProcessesByName(std::wstring name, DWORD parentpid);

OSWindow OSFindMainWindow(unsigned long process_id);

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h);

struct CaptureRect {
	JSRectangle rect;
	void* data;
	size_t size;
};

enum WindowDragPhase { Start, Moving, End };

 class WindowListener {
 public:
	 virtual void OnWindowMove(JSRectangle bounds, WindowDragPhase phase) = 0;
	 virtual void OnWindowClose() = 0;
};

struct OSWindowTracker {};

std::unique_ptr<OSWindowTracker> OSNewWindowTracker(OSWindow wnd, WindowListener* cb);

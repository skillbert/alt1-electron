#pragma once
#ifdef __OBJC__
#include <Cocoa/Cocoa.h>
#endif

#define DEFAULT_OSRAWWINDOW {.winid=0}

typedef union NativeView {
	uintptr_t winid;
#ifdef __OBJC__
	NSView* wnd;
#else
	void* wnd;
#endif
} NativeView;

typedef NativeView OSRawWindow;

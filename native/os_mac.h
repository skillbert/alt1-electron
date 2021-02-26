#pragma once
#include "stdint.h"
#ifdef __OBJC__
#include <Cocoa/Cocoa.h>
#endif

#define DEFAULT_OSRAWWINDOW {.winid=0}
#ifdef __OBJC__
typedef NSView view_t;
#else
typedef void view_t;
#endif


typedef union NativeView {
	uintptr_t winid;
	view_t* wnd;
} NativeView;

typedef NativeView OSRawWindow;

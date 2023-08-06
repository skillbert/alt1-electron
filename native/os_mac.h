#pragma once
#include "stdint.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
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
	view_t *view;
    bool operator==(const NativeView& other) const;
} NativeView;

typedef NativeView OSRawWindow;

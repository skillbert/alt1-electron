#pragma once
#include "stdint.h"
#ifdef __OBJC__
#include <Cocoa/Cocoa.h>
#endif
#include <CoreGraphics/CGWindow.h>

#ifdef __OBJC__
typedef NSView view_t;
#else
typedef void view_t;
#endif

#define MAGIC_WINID 0xC0A1
#define DEFAULT_OSRAWWINDOW {0,0}

typedef struct magicWinId {
	uint64_t magicNo; // = MAGIC_WINID
	CGWindowID id;
} MagicWinId;


typedef union Window {
	MagicWinId cg;
	view_t* wnd;
} Window;

typedef Window OSRawWindow;

#pragma once

#include "../os.h"

class NSOSWindow : public OSWindow {
	NSView* view;
public:
	NSOSWindow(NSView* wnd) : OSWindow {{.view = wnd}}, view(wnd) {}
};

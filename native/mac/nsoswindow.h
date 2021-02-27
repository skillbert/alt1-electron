#pragma once

#include "macbase.h"

class NSOSWindow : public MacOSWindow {
	NSView* view;
public:
	NSOSWindow(NSView* wnd) : OSWindow {{.view = wnd}}, view(wnd) {}
	void SetBounds(JSRectangle bounds);
	JSRectangle GetBounds();
	JSRectangle GetClientBounds();
	int GetPid();
	bool IsValid();
	std::string GetTitle();
};

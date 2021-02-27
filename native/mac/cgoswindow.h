#pragma once

#include "macbase.h"

class CGOSWindow : public MacOSWindow {
	CGWindowID winid;
public:
	CGOSWindow(CGWindowID winid) : OSWindow {{.cg = {.magicNo = MAGIC_WINID, .id = winid}}}, winid(winid) {}
	void SetBounds(JSRectangle bounds);
	JSRectangle GetBounds();
	JSRectangle GetClientBounds();
	int GetPid();
	bool IsValid();
	std::string GetTitle();
	CGWindowID cgWindowID() { return this->winid; }
private:
	NSDictionary* getInfo();
};

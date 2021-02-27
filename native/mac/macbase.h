#pragma once

#include "../os.h"


class MacOSWindow : public OSWindow {
public:
	virtual CGWindowID cgWindowID();
}

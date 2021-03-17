#pragma once
#include <functional>
#include <thread>
#include "x11.h"

class X11EventListener {
public:
	virtual void onEvent(const xcb_generic_event_t*) = 0;
	virtual bool operator==(const X11EventListener&) = 0;
};

namespace priv_os_x11 {
	extern std::thread event_thread;
	
	void startThread();
	void registerEventHandler(const uint8_t opcode, std::unique_ptr<X11EventListener> listener);
	void unregisterEventHandler(const uint8_t opcode, const X11EventListener& listener);
}

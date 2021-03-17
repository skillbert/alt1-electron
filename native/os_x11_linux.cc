#include <unistd.h>
#include <memory>
#include <functional>
#include <napi.h>
#include <proc/readproc.h>
#include <xcb/composite.h>
#include "os.h"
#include "linux/x11.h"
#include "linux/x11_event.h"
#include "linux/shm.h"

using namespace priv_os_x11;

void OSInit() {
	priv_os_x11::connect();
	priv_os_x11::startThread();
}

void OSWindow::SetBounds(JSRectangle bounds) {
	xcb_configure_window_value_list_t config = {
		.x = bounds.x,
		.y = bounds.y,
		.width = (uint32_t) bounds.width,
		.height = (uint32_t) bounds.height,
		0, 0, 0
	};

	xcb_configure_window_aux(connection, this->hwnd, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &config);
}

JSRectangle OSWindow::GetBounds() {
	xcb_query_tree_cookie_t cookie = xcb_query_tree_unchecked(connection, this->hwnd);
	std::unique_ptr<xcb_query_tree_reply_t, decltype(&free)> reply { xcb_query_tree_reply(connection, cookie, NULL), &free };
	if (!reply) {
		return JSRectangle();
	}

	if (reply->parent == reply->root) {
		// not reparented to wm - probably has no border
		return this->GetClientBounds();
	}

	return OSWindow(reply->parent).GetClientBounds();
}

JSRectangle OSWindow::GetClientBounds() {
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(connection, this->hwnd);
	std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> reply { xcb_get_geometry_reply(connection, cookie, NULL), &free };
	if (!reply) { 
		return JSRectangle();
	}

	return JSRectangle(reply->x, reply->y, reply->width, reply->height);
}

int OSWindow::GetPid() {
	xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_pid(&ewmhConnection, this->hwnd);
	uint32_t pid;
	if (xcb_ewmh_get_wm_pid_reply(&ewmhConnection, cookie, &pid, NULL) == 0) {
		return 0;
	}

	return int(pid);
}

bool OSWindow::IsValid() {
	if (!hwnd) {
		return false;
	}

	xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(connection, this->hwnd);
	std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> reply { xcb_get_geometry_reply(connection, cookie, NULL), &free };
	return !!reply;
}

std::string OSWindow::GetTitle() {
	xcb_get_property_cookie_t cookie = xcb_get_property_unchecked(connection, 0, this->hwnd, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 100);
	std::unique_ptr<xcb_get_property_reply_t, decltype(&free)> reply { xcb_get_property_reply(connection, cookie, NULL), &free };
	if (!reply) {
		return std::string();
	}

	char* title = reinterpret_cast<char*>(xcb_get_property_value(reply.get()));
	int length = xcb_get_property_value_length(reply.get());

	return std::string(title, length);
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	return Napi::BigInt::New(env, (uint64_t) this->hwnd);
}

bool OSWindow::operator==(const OSWindow& other) const {
	return this->hwnd == other.hwnd;
}

bool OSWindow::operator<(const OSWindow& other) const {
	return this->hwnd < other.hwnd;
}

OSWindow OSWindow::FromJsValue(const Napi::Value jsval) {
	auto handle = jsval.As<Napi::BigInt>();
	bool lossless;
	xcb_window_t handleint = handle.Uint64Value(&lossless);
	if (!lossless) {
		Napi::RangeError::New(jsval.Env(), "Invalid handle").ThrowAsJavaScriptException();
	}
	return OSWindow(handleint);
}

std::vector<uint32_t> OSGetProcessesByName(std::string name, uint32_t parentpid) {
	std::vector<uint32_t> out;
	
	uid_t uidlist[1] = {getuid()};
	std::unique_ptr<PROCTAB, decltype(&closeproc)> proctab = { openproc(PROC_FILLSTAT | PROC_UID, uidlist, 1), &closeproc };
	
	proc_t data = {};
	while (readproc(proctab.get(), &data) != NULL) {
		if (parentpid != 0 && parentpid != (uint32_t) data.ppid) {
			continue;
		}
		if (name.compare(data.cmd) != 0) {
			continue;
		}
		out.push_back(data.tgid);
	}
	
	return out;
}

OSWindow OSFindMainWindow(unsigned long process_id) {
	std::vector<xcb_window_t> windows = findWindowsWithPid((pid_t) process_id);

	if (windows.size() == 0){
		return OSWindow(0);
	}

	return OSWindow(windows[0]);
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
	// This does not work:
	// 1. It show up on screenshot
	// 2. It break setbounds somehow
	// 3. Resizing doesn't work as it don't use setbounds
	// xcb_reparent_window(connection, wnd.hwnd, parent.hwnd, 0, 0);
}

//TODO obsolete?
void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h) {
	XShmCapture acquirer(connection, rootWindow);
	acquirer.copy(reinterpret_cast<char*>(target), maxlength, x, y, w, h);
}

//TODO obsolete?
void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h) {
	xcb_composite_redirect_window(connection, wnd.hwnd, XCB_COMPOSITE_REDIRECT_AUTOMATIC);
	xcb_pixmap_t pixId = xcb_generate_id(connection);
	xcb_composite_name_window_pixmap(connection, wnd.hwnd, pixId);

	XShmCapture acquirer(connection, pixId);
	acquirer.copy(reinterpret_cast<char*>(target), maxlength, x, y, w, h);

	xcb_free_pixmap(connection, pixId);
}

void OSCaptureDesktopMulti(OSWindow wnd, vector<CaptureRect> rects) {
	XShmCapture acquirer(connection, rootWindow);
	//TODO double check and document desktop 0 special case
	auto offset = wnd.GetClientBounds();

	for (CaptureRect &rect : rects) {
		acquirer.copy(reinterpret_cast<char*>(rect.data), rect.size, rect.rect.x + offset.x, rect.rect.y + offset.y, rect.rect.width, rect.rect.height);
	}
}

void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
	xcb_composite_redirect_window(connection, wnd.hwnd, XCB_COMPOSITE_REDIRECT_AUTOMATIC);
	xcb_pixmap_t pixId = xcb_generate_id(connection);
	xcb_composite_name_window_pixmap(connection, wnd.hwnd, pixId);

	XShmCapture acquirer(connection, pixId);

	for (CaptureRect &rect : rects) {
		acquirer.copy(reinterpret_cast<char*>(rect.data), rect.size, rect.rect.x, rect.rect.y, rect.rect.width, rect.rect.height);
	}

	xcb_free_pixmap(connection, pixId);
}

void OSCaptureMulti(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env) {
	switch (mode) {
	case CaptureMode::Desktop: {
		OSCaptureDesktopMulti(wnd, rects);
		break;
	}
	case CaptureMode::Window:
		OSCaptureWindowMulti(wnd, rects);
		break;
	default:
		throw Napi::RangeError::New(env, "Capture mode not supported");
	}
}

std::string OSGetProcessName(int pid) {
	pid_t pidlist[2] = {pid, 0};
	std::unique_ptr<PROCTAB, decltype(&closeproc)> proctab = { openproc(PROC_FILLSTAT | PROC_PID, pidlist), &closeproc };

	proc_t data = {};
	if (readproc(proctab.get(), &data) == NULL) {
		return std::string();
	}

	return std::string(data.cmd);
}

OSWindow OSGetActiveWindow() {
	xcb_get_property_cookie_t cookie = xcb_ewmh_get_active_window(&ewmhConnection, 0);
	xcb_window_t window;
	if (xcb_ewmh_get_active_window_reply(&ewmhConnection, cookie, &window, NULL) == 0) {
		return OSWindow(0);
	}

	return OSWindow(window);
}

uint8_t opcode_event(WindowEventType type) {
	switch (type) {
	case WindowEventType::Move:
		return XCB_CONFIGURE_NOTIFY;
	case WindowEventType::Close:
		return XCB_DESTROY_NOTIFY;
	// case WindowEventType::Show:
	// 	break;
	case WindowEventType::Click:
		return XCB_BUTTON_PRESS;
	default:
		return 0;
	}
}

class Listener : public X11EventListener {
public:
	OSWindow wnd;
	WindowEventType type;
	Napi::ThreadSafeFunction callback;
	Napi::FunctionReference fref;
	
	Listener(OSWindow wnd, WindowEventType type, Napi::Function cb) : wnd(wnd), type(type) {
		this->callback = Napi::ThreadSafeFunction::New(cb.Env(), cb, "listener", 0, 2);
		this->fref = Napi::Persistent(cb);
		this->fref.SuppressDestruct();
	};

	Listener(Listener& other) {
		this->wnd = other.wnd;
		this->type = other.type;
		this->callback = other.callback;
		this->fref = Napi::Persistent(other.fref.Value());
	};
	
	void onEvent(const xcb_generic_event_t* event);
	
	bool operator==(const X11EventListener& other) {
		const Listener& listener = reinterpret_cast<const Listener&>(other);
		return listener.wnd == this->wnd && listener.type == this->type && listener.fref == this->fref;
	}
};

void Listener::onEvent(const xcb_generic_event_t* event) {
	// XXX: This function is called without checking window
	try {
		switch (this->type) {
			case WindowEventType::Move: 
				{
					// (bounds: Rectangle, phase: "start" | "moving" | "end")
					auto real_event = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
					if (real_event->window != this->wnd.hwnd) {
						return;
					}
					this->callback.NonBlockingCall([this, real_event](Napi::Env env, Napi::Function jsCallback) {
						jsCallback({
							JSRectangle(real_event->x, real_event->y, real_event->width, real_event->height).ToJs(env),
							Napi::String::New(env, "end")
						});
					});
					break;
				}
			case WindowEventType::Close:
				{
					// ()
					auto real_event = reinterpret_cast<const xcb_destroy_notify_event_t*>(event);
					if (real_event->window != this->wnd.hwnd) {
						return;
					}
					this->callback.NonBlockingCall();
					break;
				}
			case WindowEventType::Show:
				{
					// (wnd: BigInt, event: number)
					// auto real_event = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
					// if (real_event->window != this->wnd.hwnd) {
					// 	return;
					// }
					this->callback.NonBlockingCall([this, event](Napi::Env env, Napi::Function jsCallback) {
						jsCallback({
							this->wnd.ToJS(env),
							Napi::Number::New(env, event->full_sequence)
						});
					});
					break;
				}
			case WindowEventType::Click:
				{
					// ()
					auto real_event = reinterpret_cast<const xcb_button_press_event_t*>(event);
					if (real_event->event != this->wnd.hwnd) {
						return;
					}
					this->callback.NonBlockingCall();
					break;
				}
		}
	} catch (...) {}
}

std::vector<std::reference_wrapper<std::unique_ptr<Listener>>> listeners;

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	uint8_t opcode = opcode_event(type);
	if (opcode == 0) {
		return;
	}
	
	xcb_change_window_attributes_value_list_t values = {};
	values.event_mask = XCB_EVENT_MASK_STRUCTURE_NOTIFY; // XCB_EVENT_MASK_BUTTON_PRESS
	xcb_change_window_attributes_aux(connection, wnd.hwnd, XCB_CW_EVENT_MASK, &values);

	std::unique_ptr<Listener> listener = std::make_unique<Listener>(wnd, type, cb);
	listeners.push_back(std::ref(listener));
	priv_os_x11::registerEventHandler(opcode, std::move(listener));
}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {
	uint8_t opcode = opcode_event(type);
	if (opcode == 0) {
		return;
	}
	auto persistCb = Napi::Persistent(cb);

	for (auto it = listeners.begin(); it != listeners.end();) {
		std::unique_ptr<Listener>& ref = it->get();
		if (ref == NULL) {
			listeners.erase(it);
			break;
		}
		if (ref->type == type && ref->wnd == wnd && ref->fref == persistCb) {
			priv_os_x11::unregisterEventHandler(opcode, *ref);
			listeners.erase(it);
			break;
		}
		it++;
	}
}

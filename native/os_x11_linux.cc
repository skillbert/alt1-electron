#include <unistd.h>
#include <memory>
#include <iostream>
#include <napi.h>
#include <proc/readproc.h>
#include <xcb/composite.h>
#include <algorithm>
#include <thread>
#include <mutex>
#include "os.h"
#include "linux/x11.h"
#include "linux/shm.h"

using namespace priv_os_x11;

struct TrackedEvent {
	xcb_window_t window;
	WindowEventType type;
	Napi::ThreadSafeFunction callback;
	Napi::FunctionReference callbackRef;
	TrackedEvent(xcb_window_t window, WindowEventType type, Napi::Function callback) :
		window(window),
		type(type),
		callback(Napi::ThreadSafeFunction::New(callback.Env(), callback, "event", 1, 1, [](Napi::Env) {})),
		callbackRef(Napi::Persistent(callback)) {}
};

std::thread windowThread;
xcb_window_t windowThreadId;
bool windowThreadExists = false;
std::vector<TrackedEvent> trackedEvents;
size_t rsDepth = 0;

std::mutex eventMutex; // Locks the trackedEvents vector
std::mutex windowThreadMutex; // Locks windowThread and windowThreadId. Should NEVER be locked from inside the window thread
std::mutex rsDepthMutex; // Locks the rsDepth variable

void WindowThread();
void StartWindowThread();

void OSWindow::SetBounds(JSRectangle bounds) {
	ensureConnection();
	if (bounds.width > 0 && bounds.height > 0) {
		int32_t config[] = {
			bounds.x,
			bounds.y,
			bounds.width,
			bounds.height,
		};
		xcb_configure_window(connection, this->handle, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &config);
	} else {
		int32_t config[] = {
			bounds.x,
			bounds.y,
		};
		xcb_configure_window(connection, this->handle, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &config);
	}
	xcb_flush(connection);
}

JSRectangle OSWindow::GetBounds() {
	return GetClientBounds();
}

JSRectangle OSWindow::GetClientBounds() {
	ensureConnection();
	xcb_generic_error_t *error = NULL;
	xcb_get_geometry_cookie_t gcookie = xcb_get_geometry(connection, this->handle);
	xcb_get_geometry_reply_t* geometry = xcb_get_geometry_reply(connection, gcookie, &error);
	if (error != NULL) {
		free(error);
		return JSRectangle();
	}
	error = NULL;
	xcb_translate_coordinates_cookie_t tcookie = xcb_translate_coordinates(connection, this->handle, rootWindow, geometry->x, geometry->y);
	xcb_translate_coordinates_reply_t* translation = xcb_translate_coordinates_reply(connection, tcookie, &error);
	if (error != NULL) {
		free(error);
		free(geometry);
		return JSRectangle();
	}
	auto x = translation->dst_x;
    auto y = translation->dst_y;
    auto w = geometry->width;
    auto h = geometry->height;
	free(geometry);
	free(translation);
	return JSRectangle(x, y, w, h);
}

bool OSWindow::IsValid() {
	if (!this->handle) {
		return false;
	}

	ensureConnection();
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(connection, this->handle);
	std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> reply { xcb_get_geometry_reply(connection, cookie, NULL), &free };
	return !!reply;
}

std::string OSWindow::GetTitle() {
	ensureConnection();
	xcb_get_property_cookie_t cookie = xcb_get_property_unchecked(connection, 0, this->handle, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 100);
	std::unique_ptr<xcb_get_property_reply_t, decltype(&free)> reply { xcb_get_property_reply(connection, cookie, NULL), &free };
	if (!reply) {
		return std::string();
	}

	char* title = reinterpret_cast<char*>(xcb_get_property_value(reply.get()));
	int length = xcb_get_property_value_length(reply.get());

	return std::string(title, length);
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	return Napi::BigInt::New(env, (uint64_t) this->handle);
}

bool OSWindow::operator==(const OSWindow& other) const {
	return this->handle == other.handle;
}

bool OSWindow::operator<(const OSWindow& other) const {
	return this->handle < other.handle;
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

bool IsRsWindow(const xcb_window_t window) {
	const uint32_t long_length = 64; // Any length higher than 2x+3 of the longest string we may match is fine
	// Check window class (WM_CLASS property); this is set by the application controlling the window
	xcb_get_property_cookie_t cookieProp = xcb_get_property(connection, 0, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, long_length);
	xcb_get_property_reply_t* replyProp = xcb_get_property_reply(connection, cookieProp, NULL);
	if (replyProp != NULL) {
		auto len = xcb_get_property_value_length(replyProp);
		// if len == long_length then that means we didn't read the whole property, so discard.
		if (len > 0 && (uint32_t)len < long_length) {
			char buffer[long_length] = { 0 };
			memcpy(buffer, xcb_get_property_value(replyProp), len);
			// first is instance name, then class name - both null terminated. we want class name.
			const char* classname = buffer + strlen(buffer) + 1;
			if (strcmp(classname, "RuneScape") == 0 || strcmp(classname, "steam_app_1343400") == 0) {
				free(replyProp);
				return true;
			}
		}
	}
	free(replyProp);
	return false;
}

void GetRsHandlesRecursively(const xcb_window_t window, std::vector<OSWindow>* out, unsigned int depth = 0) {
	xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
	xcb_query_tree_reply_t* reply = xcb_query_tree_reply(connection, cookie, NULL);
	if (reply == NULL) {
		return;
	}

	xcb_window_t* children = xcb_query_tree_children(reply);

	for (auto i = 0; i < xcb_query_tree_children_length(reply); i++) {
		xcb_window_t child = children[i];
		if (IsRsWindow(child)) {
			rsDepthMutex.lock();
			// Only take this if it's one of the deepest instances found so far
			if (depth > rsDepth) {
				out->clear();
				out->push_back(child);
				rsDepth = depth;
			} else if (depth == rsDepth) {
				out->push_back(child);
			}
			rsDepthMutex.unlock();
		}
		
		GetRsHandlesRecursively(child, out, depth + 1);
	}

	free(reply);
}

std::vector<OSWindow> OSGetRsHandles() {
	ensureConnection();
	std::vector<OSWindow> out;
	GetRsHandlesRecursively(rootWindow, &out);
	return out;
}

void OSSetWindowParent(OSWindow window, OSWindow parent) {
	ensureConnection();

	// If the parent handle is 0, we're supposed to detach, not attach
	if (parent.handle != 0) {
		xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window.handle, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1, &parent.handle);
	} else {
		xcb_delete_property(connection, window.handle, XCB_ATOM_WM_TRANSIENT_FOR);
		xcb_flush(connection);
	}
}

void OSCaptureMulti(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env) {
	// Ignore capture mode, XComposite will always work
	ensureConnection();
	xcb_composite_redirect_window(connection, wnd.handle, XCB_COMPOSITE_REDIRECT_AUTOMATIC);
	xcb_pixmap_t pixId = xcb_generate_id(connection);
	xcb_composite_name_window_pixmap(connection, wnd.handle, pixId);

	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(connection, pixId);
	xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(connection, cookie, NULL);
	if (!reply) {
		xcb_free_pixmap(connection, pixId);
		return;
	}

	XShmCapture acquirer(connection, pixId);

	for (CaptureRect &rect : rects) {
		acquirer.copy(reinterpret_cast<char*>(rect.data), rect.size, rect.rect.x, rect.rect.y, rect.rect.width, rect.rect.height);
	}

	free(reply);
	xcb_free_pixmap(connection, pixId);
}

OSWindow OSGetActiveWindow() {
	xcb_get_property_cookie_t cookie = xcb_ewmh_get_active_window(&ewmhConnection, 0);
	xcb_window_t window;
	if (xcb_ewmh_get_active_window_reply(&ewmhConnection, cookie, &window, NULL) == 0) {
		return OSWindow(0);
	}

	return OSWindow(window);
}
	

void OSNewWindowListener(OSWindow window, WindowEventType type, Napi::Function callback) {
	auto event = TrackedEvent(window.handle, type, callback);
	eventMutex.lock();

	// If this is a new window, request all its events from X server
	if (window.handle != 0 && std::find_if(trackedEvents.begin(), trackedEvents.end(), [window](TrackedEvent& e) {return e.window == window.handle;}) == trackedEvents.end()) {
		const uint32_t values[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };
		xcb_change_window_attributes(connection, window.handle, XCB_CW_EVENT_MASK, values);
	}

	// Add the event
	trackedEvents.push_back(std::move(event));
	eventMutex.unlock();

	// Start a window thread if there wasn't already one running
	StartWindowThread();
}

void OSRemoveWindowListener(OSWindow window, WindowEventType type, Napi::Function callback) {
	eventMutex.lock();

	// Remove any matching events
	trackedEvents.erase(
		std::remove_if(
			trackedEvents.begin(),
			trackedEvents.end(),
			[window, type, callback](TrackedEvent& e){
				if ((e.window == window.handle) && (e.type == type) && (Napi::Persistent(callback) == e.callbackRef)) {
					e.callback.Release();
					return true;
				}
				return false;
			}
		),
		trackedEvents.end()
	);

	// If there are no more tracked events for this window, request X server to stop sending any events about it
	if (window.handle != 0 && std::find_if(trackedEvents.begin(), trackedEvents.end(), [window](TrackedEvent& e) {return e.window == window.handle;}) == trackedEvents.end()) {
		const uint32_t values[] = { XCB_NONE };
		xcb_change_window_attributes(connection, window.handle, XCB_CW_EVENT_MASK, values);
	}
	bool wait = trackedEvents.size() == 0;
	eventMutex.unlock();

	// If the window thread has nothing left to do, send it a wakeup, then wait for it to exit
	if (wait) {
		xcb_expose_event_t event;
		event.response_type = XCB_EXPOSE;
		event.count = 0;
		event.window = windowThreadId;
		xcb_send_event(connection, false, windowThreadId, XCB_EVENT_MASK_EXPOSURE, (const char*)(&event));
		xcb_flush(connection);
		windowThread.join();
	}
}

bool WindowThreadShouldRun() {
	eventMutex.lock();
	bool anyEvents = trackedEvents.size() != 0;
	eventMutex.unlock();
	return anyEvents;
}

void StartWindowThread() {
	// Only start if there isn't already a window thread running
	windowThreadMutex.lock();
	if (!windowThreadExists) {
		windowThreadExists = true;
		windowThread = std::thread(WindowThread);

		windowThreadId = xcb_generate_id(connection);
		std::cout << "native: starting window thread: id " << windowThreadId << std::endl;
		const uint32_t values[] = { XCB_EVENT_MASK_EXPOSURE };
		xcb_create_window(connection, XCB_COPY_FROM_PARENT, windowThreadId, rootWindow, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);
		xcb_flush(connection);
	}
	windowThreadMutex.unlock();
}

// Should only be called from the window thread.
// Called when a window's state has changed such that it may have become eligible for tracking.
void HandleNewWindow(const xcb_window_t window, xcb_window_t parent) {
	if (IsRsWindow(window)) {
		size_t depth = 0;
		while (parent != rootWindow) {
			xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, parent);
			xcb_query_tree_reply_t* reply = xcb_query_tree_reply(connection, cookie, NULL);
			if (reply == NULL) {
				return;
			}
			parent = reply->parent;
			depth += 1;
		}
		rsDepthMutex.lock();
		if (depth >= rsDepth) {
			rsDepth = depth;
			eventMutex.lock();
			for (auto ev = trackedEvents.begin(); ev != trackedEvents.end(); ev++) {
				if (ev->type == WindowEventType::Show && ev->window == 0) {
					ev->callback.BlockingCall([window](Napi::Env env, Napi::Function jsCallback) {
						jsCallback.Call({Napi::BigInt::New(env, (uint64_t)window), Napi::Number::New(env, XCB_CREATE_NOTIFY)});
					});
				}
			}
			eventMutex.unlock();
		}
		rsDepthMutex.unlock();
	}
}

void WindowThread() {
	// Request substructure events for root window
	const uint32_t rootValues[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY };
    xcb_change_window_attributes(connection, rootWindow, XCB_CW_EVENT_MASK, rootValues);

	xcb_generic_event_t* event;
	while (WindowThreadShouldRun()) {
		event = xcb_wait_for_event(connection);
		if (event) {
			auto type = event->response_type & ~0x80;
			switch (type) {
				case 0: {
					xcb_generic_error_t* error = (xcb_generic_error_t*)event;
					std::cout << "native: error: code " << (int)error->error_code << "; " << (int)error->major_code << "." << (int)error->minor_code << std::endl;
					break;
				}
				case XCB_CONFIGURE_NOTIFY: {
					xcb_configure_notify_event_t* configure = (xcb_configure_notify_event_t*)event;

					eventMutex.lock();
					for (auto ev = trackedEvents.begin(); ev != trackedEvents.end(); ev++) {
						if (ev->type == WindowEventType::Move && ev->window == configure->window) {
							JSRectangle bounds = JSRectangle(configure->x, configure->y, configure->width, configure->height);
							ev->callback.BlockingCall([bounds](Napi::Env env, Napi::Function jsCallback) {
								jsCallback.Call({bounds.ToJs(env), Napi::String::New(env, "end")});
							});
						}
					}
					eventMutex.unlock();
					break;
				}
				case XCB_CREATE_NOTIFY: {
					xcb_create_notify_event_t* create = (xcb_create_notify_event_t*)event;
					HandleNewWindow(create->window, create->parent);
					break;
				}
				case XCB_REPARENT_NOTIFY: {
					xcb_reparent_notify_event_t* reparent = (xcb_reparent_notify_event_t*)event;
					HandleNewWindow(reparent->window, reparent->parent);
					break;
				}
				case XCB_EXPOSE: {
					// Not an important event, but we use XCB_EXPOSE to wake up the window thread spontaneously,
					// so it's important to catch it here
					break;
				}
				default: {
					//std::cout << "native: got event type " << type << std::endl;
					break;
				}
			}
			free(event);
		} else {
			std::cout << "native: window thread encountered an error" << std::endl;
			break;
		}
	}
	windowThreadMutex.lock();
	xcb_destroy_window(connection, windowThreadId);
	windowThreadExists = false;
	std::cout << "native: window thread exiting" << std::endl;
	windowThreadMutex.unlock();
}

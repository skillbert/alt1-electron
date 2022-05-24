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

struct Frame {
	xcb_window_t window;
	xcb_window_t frame;
	Frame(xcb_window_t window, xcb_window_t frame) : window(window), frame(frame) {}
};

struct TrackedEvent {
	xcb_window_t window;
	WindowEventType type;
	Napi::ThreadSafeFunction callback;
	Napi::FunctionReference callbackRef;
	TrackedEvent(xcb_window_t window, WindowEventType type, Napi::Function callback) :
		window(window),
		type(type),
		callback(Napi::ThreadSafeFunction::New(callback.Env(), callback, "event", 0, 1, [](Napi::Env) {})),
		callbackRef(Napi::Persistent(callback)) {}
};

std::vector<Frame> frames;
std::thread windowThread;
xcb_window_t windowThreadId;
bool windowThreadExists = false;
std::vector<TrackedEvent> trackedEvents;

std::mutex eventMutex;
std::mutex frameMutex;
std::mutex windowThreadMutex;

void WindowThread();
void StartWindowThread();

xcb_window_t* GetFrame(xcb_window_t win) {
	auto result = std::find_if(frames.begin(), frames.end(), [&win](Frame w){return w.window == win;});
	return result == frames.end() ? nullptr : &(result->frame);
}

void OSWindow::SetBounds(JSRectangle bounds) {
	ensureConnection();
	frameMutex.lock();
	auto frame = GetFrame(this->handle);
	if (frame) {
		if (bounds.width > 0 && bounds.height > 0) {
			int32_t config[] = {
				bounds.x,
				bounds.y,
				bounds.width,
				bounds.height,
			};
			xcb_configure_window(connection, *frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &config);
			int32_t childConfig[] = { 0, 0, bounds.width, bounds.height };
			xcb_configure_window(connection, this->handle, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &childConfig);
		} else {
			int32_t config[] = {
				bounds.x,
				bounds.y,
			};
			xcb_configure_window(connection, *frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &config);
		}
		xcb_flush(connection);
	}
	frameMutex.unlock();
}

JSRectangle OSWindow::GetBounds() {
	ensureConnection();
	frameMutex.lock();
	auto frame = GetFrame(this->handle);
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(connection, frame ? *frame : this->handle);
	frameMutex.unlock();
	xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(connection, cookie, NULL);
	if (!reply) {
		return JSRectangle();
	}
	auto result = JSRectangle(reply->x, reply->y, reply->width, reply->height);
	free(reply);
	return result;
}

JSRectangle OSWindow::GetClientBounds() {
	ensureConnection();
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(connection, this->handle);
	xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(connection, cookie, NULL);
	if (!reply) {
		return JSRectangle();
	}

	auto x = reply->x;
	auto y = reply->y;
	auto width = reply->width;
	auto height = reply->height;
	auto window = this->handle;
	while (true) {
		xcb_query_tree_cookie_t cookieTree = xcb_query_tree(connection, window);
		xcb_query_tree_reply_t* replyTree = xcb_query_tree_reply(connection, cookieTree, NULL);
		if (replyTree == NULL || replyTree->parent == reply->root) {
			break;
		}
		window = replyTree->parent;
		free(reply);
		free(replyTree);

		cookie = xcb_get_geometry(connection, window);
		reply = xcb_get_geometry_reply(connection, cookie, NULL);
		if (reply == NULL) {
			break;
		}

		x += reply->x;
		y += reply->y;
	}
	free(reply);
	return JSRectangle(x, y, width, height);
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

void GetRsHandlesRecursively(const xcb_window_t window, std::vector<OSWindow>* out, unsigned int* deepest, unsigned int depth = 0) {
	const uint32_t long_length = 4096;
	xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
	xcb_query_tree_reply_t* reply = xcb_query_tree_reply(connection, cookie, NULL);
	if (reply == NULL) {
		return;
	}

	xcb_window_t* children = xcb_query_tree_children(reply);

	for (auto i = 0; i < xcb_query_tree_children_length(reply); i++) {
		// First, check WM_CLASS for either "RuneScape" or "steam_app_1343400"
		xcb_window_t child = children[i];
		xcb_get_property_cookie_t cookieProp = xcb_get_property(connection, 0, child, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, long_length);
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
					// Now, only take this if it's one of the deepest instances found so far
					if (depth > *deepest) {
						out->clear();
						out->push_back(child);
						*deepest = depth;
					} else if (depth == *deepest) {
						out->push_back(child);
					}
				}
			}
		}
		free(replyProp);
		GetRsHandlesRecursively(child, out, deepest, depth + 1);
	}

	free(reply);
}

std::vector<OSWindow> OSGetRsHandles() {
	ensureConnection();
	std::vector<OSWindow> out;
	unsigned int deepest = 0;
	GetRsHandlesRecursively(rootWindow, &out, &deepest);
	return out;
}

void OSSetWindowParent(OSWindow window, OSWindow parent) {
	ensureConnection();

	// If the parent handle is 0, we're supposed to detach, not attach
	if (parent.handle != 0) {
		// Query the tree and geometry of the game window
		xcb_query_tree_cookie_t cookieTree = xcb_query_tree(connection, parent.handle);
		xcb_query_tree_reply_t* tree = xcb_query_tree_reply(connection, cookieTree, NULL);
		xcb_get_geometry_cookie_t cookieGeometry = xcb_get_geometry(connection, window.handle);
		xcb_get_geometry_reply_t* geometry = xcb_get_geometry_reply(connection, cookieGeometry, NULL);

		if (tree && tree->parent != tree->root) {
			// Generate an ID and track it
			xcb_window_t id = xcb_generate_id(connection);
			frameMutex.lock();
			frames.push_back(Frame(window.handle, id));
			frameMutex.unlock();

			// Set OverrideRedirect on the electron window, and request events
			const uint32_t evalues[] = { 1 };
			xcb_change_window_attributes(connection, window.handle, XCB_CW_OVERRIDE_REDIRECT, evalues);

			// Create a new window, parented to the game's parent, with the game view's geometry and OverrideRedirect
			const uint32_t fvalues[] = { 1, 33554431 };
			xcb_create_window(connection, XCB_COPY_FROM_PARENT, id, tree->parent, 0, 0, geometry->width, geometry->height,
								0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, fvalues);

			// Map our new frame
			xcb_map_window(connection, id);

			// Parent the electron window to our frame
			xcb_reparent_window(connection, window.handle, id, 0, 0);

			std::cout << "native: created frame " << id << " for electron window " << window.handle << std::endl;

			// Start an event-handling thread if there isn't one already running
			StartWindowThread();
		}

		free(tree);
		free(geometry);
	} else {
		frameMutex.lock();
		auto frame = GetFrame(window.handle);
		if (frame) {
			xcb_reparent_window(connection, window.handle, rootWindow, 0, 0);
			std::cout << "native: destroying frame " << *frame << std::endl;
			xcb_destroy_window(connection, *frame);
			xcb_flush(connection);
		}
		frames.erase(
			std::remove_if(frames.begin(), frames.end(), [&window](Frame w){return w.window == window.handle;}),
			frames.end()
		);

		eventMutex.lock();
		bool wait = frames.size() == 0 && trackedEvents.size() == 0;
		eventMutex.unlock();
		frameMutex.unlock();

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
}

void OSCaptureDesktopMulti(OSWindow wnd, vector<CaptureRect> rects) {
	ensureConnection();
	XShmCapture acquirer(connection, rootWindow);
	//TODO double check and document desktop 0 special case
	auto offset = wnd.GetClientBounds();

	for (CaptureRect &rect : rects) {
		acquirer.copy(reinterpret_cast<char*>(rect.data), rect.size, rect.rect.x + offset.x, rect.rect.y + offset.y, rect.rect.width, rect.rect.height);
	}
}

void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
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
	if (std::find_if(trackedEvents.begin(), trackedEvents.end(), [window](TrackedEvent& e) {return e.window == window.handle;}) == trackedEvents.end()) {
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
	if (std::find_if(trackedEvents.begin(), trackedEvents.end(), [window](TrackedEvent& e) {return e.window == window.handle;}) == trackedEvents.end()) {
		const uint32_t values[] = { XCB_NONE };
		xcb_change_window_attributes(connection, window.handle, XCB_CW_EVENT_MASK, values);
	}
	frameMutex.lock();
	bool wait = frames.size() == 0 && trackedEvents.size() == 0;
	eventMutex.unlock();
	frameMutex.unlock();

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
	frameMutex.lock();
	eventMutex.lock();
	bool run = (frames.size() != 0 || trackedEvents.size() != 0);
	frameMutex.unlock();
	eventMutex.unlock();
	return run;
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

void WindowThread() {
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
							JSRectangle bounds = JSRectangle(0, 0, configure->width, configure->height);
							ev->callback.BlockingCall(&bounds, [](Napi::Env env, Napi::Function jsCallback, JSRectangle* jsBounds) {
								jsCallback.Call({jsBounds->ToJs(env), Napi::String::New(env, "end")});
							});
						}
					}
					eventMutex.unlock();
					break;
				}
				case XCB_CLIENT_MESSAGE: {
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

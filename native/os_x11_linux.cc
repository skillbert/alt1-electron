#include <unistd.h>
#include <memory>
#include <iostream>
#include <napi.h>
#include <proc/readproc.h>
#include <xcb/composite.h>
#include <xcb/record.h>
#include <xcb/shape.h>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
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
		callback(Napi::ThreadSafeFunction::New(callback.Env(), callback, "event", 0, 1, [](Napi::Env) {})),
		callbackRef(Napi::Persistent(callback)) {}
};

std::thread windowThread;
std::thread recordThread;
bool windowThreadExists = false;
std::vector<TrackedEvent> trackedEvents;
size_t rsDepth = 0;

std::mutex eventMutex; // Locks the trackedEvents vector
std::mutex windowThreadMutex; // Locks windowThread. Should NEVER be locked from inside the window thread
std::mutex rsDepthMutex; // Locks the rsDepth variable

void WindowThread();
void RecordThread();
void StartWindowThread();

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
	ensureConnection();
	constexpr uint32_t long_length = 64; // Any length higher than 2x+3 of the longest string we may match is fine
	// Check window class (WM_CLASS property); this is set by the application controlling the window
	// Also check WM_TRANSIENT_FOR is not set, this will be set on things like popups
	xcb_get_property_cookie_t cookieProp = xcb_get_property(connection, 0, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, long_length);
	xcb_get_property_cookie_t cookieTransient = xcb_get_property(connection, 0, window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 0, long_length);
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
				auto replyTransient = xcb_get_property_reply(connection, cookieTransient, NULL);
				if (replyTransient && xcb_get_property_value_length(replyTransient) == 0) {
					free(replyProp);
					return true;
				}
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


// Only for use from the window thread
struct CondPair {
	std::condition_variable condvar;
	std::mutex mutex;
	bool done;
	CondPair(): done(false) {}
	CondPair(CondPair& other): done(other.done) {}
	CondPair(CondPair&& other): done(other.done) {}
};
std::vector<CondPair> condvars;
template<typename F, typename COND>
void IterateEvents(COND cond, F callback) {
	eventMutex.lock();
	for (auto it = trackedEvents.begin(); it != trackedEvents.end(); it++) {
		if (cond(*it)) {
			condvars.push_back(CondPair());
			CondPair* pair = &*condvars.end();
			it->callback.BlockingCall([callback, &pair](Napi::Env env, Napi::Function jsCallback) {
				callback(env, jsCallback);
				std::unique_lock<std::mutex> lock(pair->mutex);
				pair->done = true;
				pair->condvar.notify_all();
			});
		}
	}
	eventMutex.unlock();
	for(auto it = condvars.begin(); it != condvars.end(); it++) {
		std::unique_lock<std::mutex> lock(it->mutex);
		while(!it->done) it->condvar.wait(lock);
	}
	condvars.clear();
}

void OSSetWindowShape(OSWindow window, std::vector<JSRectangle> rects, uint8_t op) {
	OSUnsetWindowShape(window);
	std::vector<xcb_rectangle_t> xrects;
	xrects.reserve(rects.size());
	for(size_t i = 0; i < rects.size(); i += 1) {
		xcb_rectangle_t rect;
		rect.x = rects[i].x;
		rect.y = rects[i].y;
		rect.width = rects[i].width;
		rect.height = rects[i].height;
		xrects.push_back(rect);
	}
	uint8_t ordering = 0;
	if (xrects.size() < 2) ordering = 3;
	xcb_shape_rectangles(connection, op, 0, ordering, window.handle, 0, 0, xrects.size(), xrects.data());
	xcb_flush(connection);
}

void OSUnsetWindowShape(OSWindow window) {
	ensureConnection();
	xcb_shape_mask(connection, 0, 0, window.handle, 0, 0, 0);
}


void OSNewWindowListener(OSWindow window, WindowEventType type, Napi::Function callback) {
	auto event = TrackedEvent(window.handle, type, callback);

	// If this is a new window, request all its events from X server
	eventMutex.lock();
	if (window.handle != 0 && std::find_if(trackedEvents.begin(), trackedEvents.end(), [window](TrackedEvent& e) {return e.window == window.handle;}) == trackedEvents.end()) {
		constexpr uint32_t values[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };
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

	// If there are no more tracked events for this window, request X server to stop sending any events about it
	if (window.handle != 0 && std::find_if(trackedEvents.begin(), trackedEvents.end(), [window](TrackedEvent& e) {return e.window == window.handle;}) == trackedEvents.end()) {
		constexpr uint32_t values[] = { XCB_NONE };
		xcb_change_window_attributes_checked(connection, window.handle, XCB_CW_EVENT_MASK, values);
	}

	bool wait = trackedEvents.size() != 0;

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

	wait &= trackedEvents.size() == 0;
	eventMutex.unlock();

	// If the window thread has nothing left to do, send it a wakeup, then wait for it to exit
	if (wait) {
		xcb_disconnect(connection);
		xcb_flush(connection);
		windowThread.join();
		recordThread.join();
		connection = NULL;
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
		recordThread = std::thread(RecordThread);
	}
	windowThreadMutex.unlock();
}

// Should only be called from the window thread.
// Called when a window's state has changed such that it may have become eligible for tracking.
void HandleNewWindow(const xcb_window_t window, xcb_window_t parent) {
	bool untrack = true;
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
			free(reply);
		}
		rsDepthMutex.lock();
		if (depth >= rsDepth) {
			untrack = false;
			rsDepth = depth;
			rsDepthMutex.unlock();
			IterateEvents(
				[](const TrackedEvent& e){return e.type == WindowEventType::Show && e.window == 0;},
				[window](Napi::Env env, Napi::Function callback){callback.Call({Napi::BigInt::New(env, (uint64_t)window), Napi::Number::New(env, XCB_CREATE_NOTIFY)});}
			);
		} else {
			rsDepthMutex.unlock();
		}
		
	}

	if (untrack) {
		IterateEvents(
			[window](const TrackedEvent& e){return e.type == WindowEventType::Close && e.window == window;},
			[](Napi::Env env, Napi::Function callback){callback.Call({});}
		);
	}
}

void WindowThread() {
	// Request substructure events for root window
	constexpr uint32_t rootValues[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY };
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
					xcb_window_t window = configure->window;
					JSRectangle bounds = JSRectangle(configure->x, configure->y, configure->width, configure->height);
					IterateEvents(
						[window](const TrackedEvent& e){return e.type == WindowEventType::Move && e.window == window;},
						[bounds](Napi::Env env, Napi::Function callback){callback.Call({bounds.ToJs(env), Napi::String::New(env, "end")});}
					);
					break;
				}
				case XCB_CREATE_NOTIFY: {
					xcb_create_notify_event_t* create = (xcb_create_notify_event_t*)event;
					if (!create->override_redirect) {
						HandleNewWindow(create->window, create->parent);
					}
					break;
				}
				case XCB_DESTROY_NOTIFY: {
					xcb_destroy_notify_event_t* destroy = (xcb_destroy_notify_event_t*)event;
					xcb_window_t window = destroy->window;
					IterateEvents(
						[window](const TrackedEvent& e){return e.type == WindowEventType::Close && e.window == window;},
						[](Napi::Env env, Napi::Function callback){callback.Call({});}
					);
					break;
				}
				case XCB_REPARENT_NOTIFY: {
					xcb_reparent_notify_event_t* reparent = (xcb_reparent_notify_event_t*)event;
					if(!reparent->override_redirect) {
						HandleNewWindow(reparent->window, reparent->parent);
					}
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
			// Fatal error - probably because the application is stopping and we need to return now
			break;
		}
	}

	windowThreadExists = false;
	std::cout << "native: window thread exiting" << std::endl;
}

void HitTestRecursively(xcb_window_t window, int16_t x, int16_t y, int16_t offset_x, int16_t offset_y, xcb_window_t& out_window) {
	xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
	xcb_query_tree_reply_t* reply = xcb_query_tree_reply(connection, cookie, NULL);
	if (reply == NULL) {
		return;
	}

	xcb_window_t* children = xcb_query_tree_children(reply);
	xcb_generic_error_t *error;

	for (auto i = 0; i < xcb_query_tree_children_length(reply); i++) {
		xcb_window_t child = children[i];

		error = NULL;
		xcb_get_window_attributes_cookie_t acookie = xcb_get_window_attributes(connection, child);
		xcb_get_window_attributes_reply_t* attributes = xcb_get_window_attributes_reply(connection, acookie, &error);
		if(error != NULL) {
			free(error);
			continue;
		}
		auto map_state = attributes->map_state;
		free(attributes);
		if (map_state != XCB_MAP_STATE_VIEWABLE) {
			continue;
		}

		error = NULL;
		xcb_get_geometry_cookie_t gcookie = xcb_get_geometry(connection, child);
		xcb_get_geometry_reply_t* geometry = xcb_get_geometry_reply(connection, gcookie, &error);
		if (error != NULL) {
			free(error);
			continue;
		}
		int16_t gx = geometry->x + offset_x;
		int16_t gy = geometry->y + offset_y;
		auto gw = geometry->width;
		auto gh = geometry->height;
		free(geometry);

		bool hit = true;
		xcb_shape_get_rectangles_cookie_t rcookie[3] = { // 0=ShapeBounding, 1=ShapeClip, 2=ShapeInput
			xcb_shape_get_rectangles(connection, child, 0),
			xcb_shape_get_rectangles(connection, child, 1),
			xcb_shape_get_rectangles(connection, child, 2),
		};
        xcb_shape_get_rectangles_reply_t* rectangles[3] = {
			xcb_shape_get_rectangles_reply(connection, rcookie[0], NULL),
			xcb_shape_get_rectangles_reply(connection, rcookie[1], NULL),
			xcb_shape_get_rectangles_reply(connection, rcookie[2], NULL),
		};
        if (rectangles[0] && rectangles[1] && rectangles[2]) {
			for(auto j = 0; j < 3; j += 1) {
				bool hit_shape = false;
				auto rect_count = xcb_shape_get_rectangles_rectangles_length(rectangles[j]);
				xcb_rectangle_t* rects = xcb_shape_get_rectangles_rectangles(rectangles[j]);
				for (auto k = 0; k < rect_count; k += 1) {
					xcb_rectangle_t rect = rects[k];
					hit_shape |= (x >= (rect.x + gx) && x < (rect.x + rect.width + gx) && y >= (rect.y + gy) && y < (rect.y + rect.height + gy));
				}
				hit &= hit_shape;
			}
		} else {
            hit = (x >= gx && x < (gx + gw) && y >= gy && y < (gy + gh));
        }
		free(rectangles[0]);
		free(rectangles[1]);
		free(rectangles[2]);

		if (hit) {
			out_window = child;
			HitTestRecursively(child, x, y, gx, gy, out_window);
		}
	}

	free(reply);
}

// To be called from Record thread. Recursively finds the topmost window which passes hit test at given root coordinates
xcb_window_t HitTest(int16_t x, int16_t y) {
	xcb_window_t out = rootWindow;
	HitTestRecursively(rootWindow, x, y, 0, 0, out);
	return out;
}

void RecordThread() {
	// Second event thread for using the X Record API, which we need to receive mouse button events
	const xcb_query_extension_reply_t* ext = xcb_get_extension_data(connection, &xcb_record_id);
	if (!ext) {
		std::cerr << "native: X record extension is not supported; some features will not work" << std::endl;
		return;
	}

	xcb_record_query_version_reply_t *version_reply = xcb_record_query_version_reply(connection, xcb_record_query_version(connection, XCB_RECORD_MAJOR_VERSION, XCB_RECORD_MINOR_VERSION), NULL);
	if (version_reply) {
		std::cout << "native: X record extension version: " << version_reply->major_version << "." << version_reply->minor_version << std::endl;
		free(version_reply);
	} else {
		std::cout << "native: X record extension version is unknown" << std::endl;
	}

	auto id = xcb_generate_id(connection);
	xcb_record_range_t range;
	memset(&range, 0, sizeof(xcb_record_range_t));
	range.device_events.first = XCB_BUTTON_PRESS;
	range.device_events.last = XCB_BUTTON_RELEASE;
	xcb_record_client_spec_t client_spec = XCB_RECORD_CS_ALL_CLIENTS;
	xcb_void_cookie_t cookie = xcb_record_create_context_checked(connection, id, 0, 1, 1, &client_spec, &range);
	xcb_generic_error_t* error = xcb_request_check(connection, cookie);
	if (error) {
		std::cout << "native: couldn't setup X record: xcb_record_create_context_checked returned " << (int)error->error_code << "; some features will not work" << std::endl;
		free(error);
		return;
	}

	auto rec_connection = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(rec_connection)) {
		std::cout << "native: couldn't start record thread connection; some features will not work" << std::endl;
		return;
	}

	// xcb-record event loop
	xcb_record_enable_context_cookie_t cookie2 = xcb_record_enable_context(rec_connection, id);
	while (WindowThreadShouldRun()) {
		xcb_record_enable_context_reply_t* reply = xcb_record_enable_context_reply(rec_connection, cookie2, NULL);
		if (!reply) {
			std::cout << "native: error in xcb_record_enable_context_reply" << std::endl;
			continue;
		}
		if (reply->client_swapped) {
			std::cout << "native: unsupported setting client_swapped; please report this error" << std::endl;
			break;
		}

		// 0 is XRecordFromServer; we also receive 4 (XRecordStartOfData) at the start of execution, and
		// 5 (XRecordEndOfData) when we invalidate the main connection, which works as this thread's end-wakeup
		if (reply->category == 0) {
			uint8_t* data = xcb_record_enable_context_data(reply);
			int data_len = xcb_record_enable_context_data_length(reply);
			if (data_len == sizeof(xcb_button_press_event_t)) {
				xcb_generic_event_t* ev = (xcb_generic_event_t*)data;
				switch (ev->response_type) {
					case XCB_BUTTON_PRESS: {
						xcb_button_press_event_t* event = (xcb_button_press_event_t*)ev;
						auto button = event->detail;
						if (button >= 1 && button <= 3) {
							int16_t click_x = event->root_x;
							int16_t click_y = event->root_y;
							xcb_window_t hit = HitTest(click_x, click_y);
							IterateEvents(
								[hit](const TrackedEvent& e){return e.type == WindowEventType::Click && e.window == hit;},
								[](Napi::Env env, Napi::Function callback){callback.Call({});}
							);
						}
						break;
					}
					case XCB_BUTTON_RELEASE: {
						// Mouse button released - may be useful in future?
						break;
					}
				}
			}
		}
		free(reply);
	}

	xcb_record_disable_context(rec_connection, id);
	xcb_record_free_context(rec_connection, id);
	xcb_flush(rec_connection);
	xcb_disconnect(rec_connection);
	std::cout << "native: record thread exiting" << std::endl;
}

#include <unistd.h>
#include <memory>
#include <iostream>
#include <napi.h>
#include <proc/readproc.h>
#include <xcb/composite.h>
#include "os.h"
#include "linux/x11.h"
#include "linux/shm.h"

using namespace priv_os_x11;

void OSWindow::SetBounds(JSRectangle bounds) {
	ensureConnection();
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
	ensureConnection();
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
	ensureConnection();
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(connection, this->hwnd);
	std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> reply { xcb_get_geometry_reply(connection, cookie, NULL), &free };
	if (!reply) { 
		return JSRectangle();
	}

	return JSRectangle(reply->x, reply->y, reply->width, reply->height);
}

int OSWindow::GetPid() {
	ensureConnection();
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

	ensureConnection();
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(connection, this->hwnd);
	std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> reply { xcb_get_geometry_reply(connection, cookie, NULL), &free };
	return !!reply;
}

std::string OSWindow::GetTitle() {
	ensureConnection();
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
	ensureConnection();
	std::vector<xcb_window_t> windows = findWindowsWithPid((pid_t) process_id);

	if (windows.size() == 0){
		return OSWindow(0);
	}

	return OSWindow(windows[0]);
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
	ensureConnection();
	// This does not work:
	// 1. It show up on screenshot
	// 2. It break setbounds somehow
	// 3. Resizing doesn't work as it don't use setbounds
	// xcb_reparent_window(connection, wnd.hwnd, parent.hwnd, 0, 0);
}

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h) {
	ensureConnection();
	XShmCapture acquirer(connection, rootWindow);
	acquirer.copy(reinterpret_cast<char*>(target), maxlength, x, y, w, h);
}

void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h) {
	ensureConnection();
	xcb_composite_redirect_window(connection, wnd.hwnd, XCB_COMPOSITE_REDIRECT_AUTOMATIC);
	xcb_pixmap_t pixId = xcb_generate_id(connection);
	xcb_composite_name_window_pixmap(connection, wnd.hwnd, pixId);

	XShmCapture acquirer(connection, pixId);
	acquirer.copy(reinterpret_cast<char*>(target), maxlength, x, y, w, h);

	xcb_free_pixmap(connection, pixId);
}

void OSCaptureDesktopMulti(vector<CaptureRect> rects) {
	ensureConnection();
	XShmCapture acquirer(connection, rootWindow);

	for (CaptureRect &rect : rects) {
		acquirer.copy(reinterpret_cast<char*>(rect.data), rect.size, rect.rect.x, rect.rect.y, rect.rect.width, rect.rect.height);
	}
}
void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
	ensureConnection();
	xcb_composite_redirect_window(connection, wnd.hwnd, XCB_COMPOSITE_REDIRECT_AUTOMATIC);
	xcb_pixmap_t pixId = xcb_generate_id(connection);
	xcb_composite_name_window_pixmap(connection, wnd.hwnd, pixId);

	XShmCapture acquirer(connection, pixId);

	for (CaptureRect &rect : rects) {
		acquirer.copy(reinterpret_cast<char*>(rect.data), rect.size, rect.rect.x, rect.rect.y, rect.rect.width, rect.rect.height);
	}

	xcb_free_pixmap(connection, pixId);
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

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {

}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {

}

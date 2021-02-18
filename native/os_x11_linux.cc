#include <unistd.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <iostream> // cout
#include <napi.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <proc/readproc.h>
#include "os.h"

namespace priv_os_x11 {
	xcb_connection_t* connection;
	xcb_window_t rootWindow;
	xcb_ewmh_connection_t ewmhConnection;

	std::mutex conn_mtx;
	std::map<std::string, xcb_atom_t> atoms;
	std::shared_mutex atoms_mtx;

	/**
	 * Ensure that we have connection to X11
	 */
	void ensureConnection() {
		if (connection != NULL) {
			return;
		}

		conn_mtx.lock();
		// check again with lock
		if (connection != NULL) {
			conn_mtx.unlock();
			return;
		}

		connection = xcb_connect(NULL, NULL);
		if (xcb_connection_has_error(connection)) {
			conn_mtx.unlock();
			throw new std::runtime_error("Cannot initiate xcb connection");
		}
	
		xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
		if (!screen) {
			conn_mtx.unlock();
			throw new std::runtime_error("Cannot iterate screens");
		}
		rootWindow = screen->root;
		if (xcb_ewmh_init_atoms_replies(&ewmhConnection, xcb_ewmh_init_atoms(connection, &ewmhConnection), NULL) == 0) {
			conn_mtx.unlock();
			throw new std::runtime_error("Cannot prepare ewmh atoms");
		}

		conn_mtx.unlock();
	}

	xcb_atom_t getAtom(const char* name){ // FIXME: Unused?
		std::string nameStr = std::string(name);

		atoms_mtx.lock_shared();
		if (atoms.find(nameStr) != atoms.end()) {
			xcb_atom_t out = atoms[nameStr];
			atoms_mtx.unlock_shared();
			return out;
		}

		atoms_mtx.unlock_shared();
		ensureConnection();
		atoms_mtx.lock();
		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, true, strlen(name), name);
		std::unique_ptr<xcb_intern_atom_reply_t, decltype(&free)> reply { xcb_intern_atom_reply(connection, cookie, NULL), &free };
		if (!reply) {
			atoms_mtx.unlock();
			throw std::runtime_error("fail to get atom");
		}

		atoms[nameStr] = reply->atom;
		atoms_mtx.unlock();

		return reply->atom;
	}
}

using namespace priv_os_x11;

void OSWindow::SetBounds(JSRectangle bounds) {
	// TODO
}

JSRectangle OSWindow::GetBounds() {
	// TODO: Add window decorator size
	return this->GetClientBounds();
}

JSRectangle OSWindow::GetClientBounds() {
	// TODO
	return JSRectangle();
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
	// TODO
	return true;
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

	// FIXME: Move this to ts
	if (name == "rs2client.exe") {
		name = "rs2client";
	}
	
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
	// TODO
	std::cout << "OSFindMainWindow" << std::endl;
	return OSWindow();
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
	// TODO
}

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h) {
	// TODO
}

void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h) {
	// TODO
}

void OSCaptureDesktopMulti(vector<CaptureRect> rects) {
	// TODO
}
void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
	// TODO
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

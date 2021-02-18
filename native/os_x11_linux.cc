#include <memory>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <napi.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
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

	xcb_atom_t getAtom(const char* name){
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

int OSWindow::GetPid() {
	ensureConnection();
	xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_pid(&ewmhConnection, this->hwnd);
	uint32_t pid;
	if (xcb_ewmh_get_wm_pid_reply(&ewmhConnection, cookie, &pid, NULL) == 0) {
		return 0;
	}

	return int(pid);
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

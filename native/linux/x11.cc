#include <cstring>
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include "x11.h"

using namespace std;

namespace priv_os_x11 {
	xcb_connection_t* connection;
	xcb_window_t rootWindow;
	xcb_ewmh_connection_t ewmhConnection;

	std::mutex conn_mtx;
	std::map<std::string, xcb_atom_t> atoms;
	std::shared_mutex atoms_mtx;

	void ensureConnection() {
		if (connection != NULL) {
			return;
		}

		std::lock_guard<std::mutex> lock(conn_mtx);
		// check again with lock
		if (connection != NULL) {
			return;
		}

		connection = xcb_connect(NULL, NULL);
		if (xcb_connection_has_error(connection)) {
			throw new std::runtime_error("Cannot initiate xcb connection");
		}
	
		xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
		if (!screen) {
			throw new std::runtime_error("Cannot iterate screens");
		}
		rootWindow = screen->root;
		if (xcb_ewmh_init_atoms_replies(&ewmhConnection, xcb_ewmh_init_atoms(connection, &ewmhConnection), NULL) == 0) {
			throw new std::runtime_error("Cannot prepare ewmh atoms");
		}
	}

	xcb_atom_t getAtom(const char* name) { // FIXME: Unused?
		std::string nameStr = std::string(name);

		std::shared_lock<std::shared_mutex> slock(atoms_mtx);
		if (atoms.find(nameStr) != atoms.end()) {
			xcb_atom_t out = atoms[nameStr];
			return out;
		}

		slock.unlock();
		ensureConnection();
		
		std::lock_guard<std::shared_mutex> lock(atoms_mtx);
		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, true, strlen(name), name);
		std::unique_ptr<xcb_intern_atom_reply_t, decltype(&free)> reply { xcb_intern_atom_reply(connection, cookie, NULL), &free };
		if (!reply) {
			throw std::runtime_error("fail to get atom");
		}

		atoms[nameStr] = reply->atom;

		return reply->atom;
	}
}

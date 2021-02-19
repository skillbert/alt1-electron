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

	xcb_atom_t getAtom(const char* name) { // FIXME: Unused?
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

	std::vector<xcb_window_t> findWindowsWithPid(const pid_t pid) {
		ensureConnection();
		
		return findWindowsWithPid(pid, rootWindow);
	}

	std::vector<xcb_window_t> findWindowsWithPid(const pid_t pid, const xcb_window_t root) {
		// XXX: This does not check for connection validity - it assume that you must have connection
		// to get that window ID anyway
		std::vector<xcb_window_t> out;

		xcb_get_property_cookie_t pidCookie = xcb_ewmh_get_wm_pid(&ewmhConnection, root);
		xcb_query_tree_cookie_t queryCookie = xcb_query_tree_unchecked(connection, root);
		std::unique_ptr<xcb_query_tree_reply_t, decltype(&free)> queryReply { xcb_query_tree_reply(connection, queryCookie, NULL), &free };
		if (!queryReply) {
			return out;
		}
		uint32_t rootPid;
		if (xcb_ewmh_get_wm_pid_reply(&ewmhConnection, pidCookie, &rootPid, NULL) != 0) {
			if (pid == (pid_t) rootPid) {
				out.push_back(root);
				// don't recurse as everything gonna be owned by this
				return out;
			}
		}

		// recurse and merge
		xcb_window_t* children = xcb_query_tree_children(queryReply.get());
		size_t len = xcb_query_tree_children_length(queryReply.get());
		for (size_t i = 0; i < len; i++) {
			std::vector<xcb_window_t> result = findWindowsWithPid(pid, children[i]);
			out.reserve(out.size() + result.size());
			out.insert(out.end(), result.begin(), result.end());
		}

		return out;
	}
}

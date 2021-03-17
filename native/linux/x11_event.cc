#include <iostream>
#include <vector>
#include <map>
#include <shared_mutex>
#include "x11_event.h"

// #define WITH_ERROR

#ifdef WITH_ERROR
#include <xcb/xcb_errors.h>
#endif

namespace priv_os_x11 {
	std::map<uint8_t, std::vector<std::unique_ptr<X11EventListener>>> listeners;
	std::shared_mutex listeners_mtx;
	std::thread event_thread;
#ifdef WITH_ERROR
	xcb_errors_context_t* error_context;
#endif

	void fireEvent(const uint8_t opcode, const xcb_generic_event_t* event) {
		std::shared_lock<std::shared_mutex> lock(listeners_mtx);
		auto result = listeners.find(opcode);
		if (result == listeners.end()) {
			return;
		}

		std::vector<std::unique_ptr<X11EventListener>>& list = result->second;
		for (std::unique_ptr<X11EventListener>& item : list) {
			item->onEvent(event);
		}
	}

	void worker() {
		xcb_generic_event_t *event;
		while ((event = xcb_wait_for_event(connection))) {
			if (event->response_type == 0) {
#ifdef WITH_ERROR
				xcb_generic_error_t* error = reinterpret_cast<xcb_generic_error_t*>(event);
				const char *extension;
				const char *errorStr = xcb_errors_get_name_for_error(error_context, error->error_code, &extension);
				if (error == NULL) {
					std::cout << "UNKNOWN ERROR in X11 event loop!!" << std::endl;
				} else if (extension != NULL) {
					std::cout << "ERROR in X11 event loop!! Ext " << extension << " " << errorStr << std::endl;
				} else {
					std::cout << "ERROR in X11 event loop!! " << errorStr << std::endl;
				}
#endif
				continue;
			}

			fireEvent(event->response_type & ~0x80, event);

			free(event);
			std::this_thread::yield();
		}
		std::cout << "x11 event thread ended" << std::endl;
	}

	void startThread() {
		if (connection == NULL) {
			throw new std::runtime_error("Cannot start thread without xcb connection");
		}
#ifdef WITH_ERROR
		if (xcb_errors_context_new(connection, &error_context) != 0) {
			throw new std::runtime_error("Fail to init error context");
		}
#endif

		event_thread = std::thread(worker);
	}

	void registerEventHandler(const uint8_t opcode, std::unique_ptr<X11EventListener> listener) {
		std::unique_lock<std::shared_mutex> lock(listeners_mtx);
		listeners.try_emplace(opcode);
		listeners[opcode].push_back(std::move(listener));
	}
	
	void unregisterEventHandler(const uint8_t opcode, const X11EventListener& listener) {
		std::unique_lock<std::shared_mutex> lock(listeners_mtx);
		auto result = listeners.find(opcode);
		if (result == listeners.end()) {
			return;
		}

		std::vector<std::unique_ptr<X11EventListener>>& list = result->second;
		for (auto item = list.begin(); item != list.end();) {
			if (**item == listener) {
				item = list.erase(item);
			} else {
				++item;
			}
		}
	}

}

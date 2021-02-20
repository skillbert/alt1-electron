#pragma once
#include <xcb/xcb.h>
#include <xcb/shm.h>

namespace priv_os_x11 {
	class XShmCapture {
		xcb_connection_t* connection;
	public:
		XShmCapture(xcb_connection_t* c, xcb_drawable_t d);
		~XShmCapture();

		void copy(char* target, size_t maxLength, int x, int y, int w, int h);
	
	private:
		xcb_drawable_t drawable;
		int shmId;
		char* shm;
		xcb_shm_seg_t shmSeg;
		char name[100];
		std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> geometry;
	};
}

#include <assert.h>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#include "shm.h"

namespace priv_os_x11 {
	XShmCapture::XShmCapture(xcb_connection_t* c, xcb_drawable_t d) : connection(c), drawable(d), geometry(NULL, &free) {
		xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(c, d);
		geometry = std::unique_ptr<xcb_get_geometry_reply_t, decltype(&free)> { xcb_get_geometry_reply(c, cookie, NULL), &free };
		if (!geometry) {
			throw new std::runtime_error("Unable to get image size");
		}

		this->shmId = shmget(IPC_PRIVATE, geometry->width * geometry->height * 4, IPC_CREAT | 0600);
		if (this->shmId == -1) {
			throw new std::runtime_error("Fail to allocate SHM");
		}
		this->shm = reinterpret_cast<char*>(shmat(this->shmId, NULL, SHM_RDONLY));
		if (this->shm == (char *) -1) {
			throw new std::runtime_error("Cannot attach to SHM");
		}

		this->shmSeg = reinterpret_cast<xcb_shm_seg_t>(xcb_generate_id(c));
		xcb_shm_attach(c, this->shmSeg, this->shmId, 0);
		
		xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(c, d, 0, 0, geometry->width, geometry->height, 0xFFFFFF, XCB_IMAGE_FORMAT_Z_PIXMAP, this->shmSeg, 0);
		std::unique_ptr<xcb_shm_get_image_reply_t, decltype(&free)> getImageReply { xcb_shm_get_image_reply(c, imageCookie, NULL), &free };
		if (!getImageReply) {
			throw new std::runtime_error("Fail to fetch image");
		}
	}

	XShmCapture::~XShmCapture() {
		shmdt(this->shm);
		xcb_shm_detach(this->connection, this->shmSeg);
		shmctl(this->shmId, IPC_RMID, NULL);
	}

	void XShmCapture::copy(char* target, size_t maxLength, int x, int y, int w, int h) {
		size_t expectedSize = w * h * 4;
		if (expectedSize > maxLength) {
			throw new std::invalid_argument("Insufficient buffer size");
		}

		size_t targetPos = 0;
		for (int row = y; row < y + h; row++) {
			for (int col = x; col < x + w; col++) {
				if (col < this->geometry->width && row < this->geometry->height) {
					int pos = ((row * this->geometry->width) + col) * 4;
					target[targetPos++] = this->shm[pos + 2];
					target[targetPos++] = this->shm[pos + 1];
					target[targetPos++] = this->shm[pos];
					target[targetPos++] = 0xFF; // alpha
				} else {
					targetPos += 4;
				}
			}
		}
		assert(targetPos <= expectedSize);
	}
}

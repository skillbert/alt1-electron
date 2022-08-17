#include <cstring>
#include <iostream>
#include <string>
#include "os.h"
#include "libproc.h"

std::vector<OSWindow> OSGetRsHandles() {
	std::vector<OSWindow> out;
	
	// TODO: Placeholder to allow build. Fill this in.

	return out;
}

JSRectangle OSWindow::GetBounds() {
	NSWindow* window = [this->handle.wnd window];
	NSRect frame = [window frame];
	int y = [[NSScreen mainScreen] frame].size.height - frame.origin.y - frame.size.height;
	return JSRectangle(frame.origin.x, y, frame.size.width, frame.size.height);
}

JSRectangle OSWindow::GetClientBounds() {
	return JSRectangle();
}

bool OSWindow::IsValid() {
	if (this->handle.winid == 0) {
		return false;
	}

	return true;
}

std::string OSWindow::GetTitle() {
	return "";
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	return Napi::BigInt::New(env, (uint64_t) this->handle.winid);
}

bool OSWindow::operator==(const OSWindow& other) const {
	return memcmp(&this->handle, &other.handle, sizeof(this->handle)) == 0;
}

bool OSWindow::operator<(const OSWindow& other) const {
	return memcmp(&this->handle, &other.handle, sizeof(this->handle)) < 0;
}

OSWindow OSWindow::FromJsValue(const Napi::Value jsval) {
	auto handle = jsval.As<Napi::BigInt>();
	bool lossless;
	uint64_t handleint = handle.Uint64Value(&lossless);
	if (!lossless) {
		Napi::RangeError::New(jsval.Env(), "Invalid handle").ThrowAsJavaScriptException();
	}
	return OSWindow(OSRawWindow{.wnd = (NSView*) handleint});
}

std::string OSGetProcessName(int pid) {
	char namebuf[255];
	if (proc_name(pid, namebuf, sizeof(namebuf)) == -1) {
		throw new std::runtime_error("Unable to get process name");
	}

	return std::string(namebuf);
}

std::vector<uint32_t> OSGetProcessesByName(std::string name, uint32_t parentpid) {
	std::vector<uint32_t> out;
	std::unique_ptr<pid_t[]> buf;
	int no_proc;
	if (parentpid == 0) {
		no_proc = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);  
		if (no_proc == -1) {
			throw new std::runtime_error("Unable to get pids");
		}
		buf = std::unique_ptr<pid_t[]> { new pid_t[no_proc/sizeof(pid_t)] };
 
		no_proc = proc_listpids(PROC_ALL_PIDS, 0, buf.get(), no_proc);
		if (no_proc == -1) {
			throw new std::runtime_error("Unable to get pids");
		}
	} else {
		// this path is not tested
		no_proc = proc_listchildpids(parentpid, NULL, 0);
		if (no_proc == -1) {
			throw new std::runtime_error("Unable to get pids");
		}
		buf = std::unique_ptr<pid_t[]> { new pid_t[no_proc/sizeof(pid_t)] };
		
		no_proc = proc_listchildpids(parentpid, buf.get(), no_proc);
		if (no_proc == -1) {
			throw new std::runtime_error("Unable to get pids");
		}
	}
	no_proc /= sizeof(pid_t);
	
	for (int i = 0; i < no_proc; i++) {
		if (OSGetProcessName(buf[i]) == name) {
			out.push_back(buf[i]);
		}
	}
	
	return out;
}

OSWindow OSFindMainWindow(unsigned long process_id) {
	return OSWindow(DEFAULT_OSRAWWINDOW);
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
}

void OSCaptureDesktopMulti(OSWindow wnd, vector<CaptureRect> rects) {
	
}
void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
	
}


void OSCaptureMulti(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env){
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

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {

}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {

}


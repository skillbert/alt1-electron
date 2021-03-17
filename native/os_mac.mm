#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include "os.h"
#include "libproc.h"

bool isCgWindowId(Window wnd) {
	return wnd.cg.magicNo == MAGIC_WINID;
}

void OSWindow::SetBounds(JSRectangle bounds) {
	if (isCgWindowId(this->hwnd)) {
		std::cout << "SetBounds called on cgwindowid" << std::endl;
		return;
	}
	
	assert(bounds.width > 0);
	assert(bounds.height > 0);
	
	NSWindow* window = [this->hwnd.wnd window];
	if (window == NULL) {
		return;
	}
	
	int y = [[NSScreen mainScreen] frame].size.height - bounds.y - bounds.height;
	NSRect rect = NSMakeRect(bounds.x, y, bounds.width, bounds.height);
	[window setFrame:rect display:YES];
}

JSRectangle OSWindow::GetBounds() {
	if (isCgWindowId(this->hwnd)) {
		std::cout << "GetBounds called on cgwindowid" << std::endl;
		return JSRectangle();
	}

	NSWindow* window = [this->hwnd.wnd window];
	NSRect frame = [window frame];
	int y = [[NSScreen mainScreen] frame].size.height - frame.origin.y - frame.size.height;
	return JSRectangle(frame.origin.x, y, frame.size.width, frame.size.height);
}

JSRectangle OSWindow::GetClientBounds() {
	return this->GetBounds();
}

int OSWindow::GetPid() {
	if (isCgWindowId(this->hwnd)) {
		return 0;
	}

	// if it's NSView then it's always our own
	return getpid();
}

bool OSWindow::IsValid() {
	if (this->hwnd.wnd == NULL) {
		return false;
	}

	return true;
}

std::string OSWindow::GetTitle() {
	if (isCgWindowId(this->hwnd)) {
		std::cout << "GetTitle called on cgwindowid" << std::endl;
		return "";
	}

	NSWindow* window = [this->hwnd.wnd window];
	return std::string([[window title] UTF8String]);
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	size_t size = sizeof(OSRawWindow) / sizeof(uint64_t);
	return Napi::BigInt::New(env, 0, size, (uint64_t*) &this->hwnd);
}

OSWindow OSWindow::FromJsValue(const Napi::Value jsval) {
	auto handle = jsval.As<Napi::BigInt>();

	OSRawWindow buf = DEFAULT_OSRAWWINDOW;
	int sign;
	size_t bufSize = sizeof(OSRawWindow) / sizeof(uint64_t);
	handle.ToWords(&sign, &bufSize, (uint64_t*) &buf);

	return OSWindow(buf);
}

bool OSWindow::operator==(const OSWindow& other) const {
	return memcmp(&this->hwnd, &other.hwnd, sizeof(this->hwnd)) == 0;
}

bool OSWindow::operator<(const OSWindow& other) const {
	return memcmp(&this->hwnd, &other.hwnd, sizeof(this->hwnd)) < 0;
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

void OSInit() {}

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

std::string OSGetProcessName(int pid) {
	char namebuf[255];
	if (proc_name(pid, namebuf, sizeof(namebuf)) == -1) {
		throw new std::runtime_error("Unable to get process name");
	}

	return std::string(namebuf);
}

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {

}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function cb) {

}


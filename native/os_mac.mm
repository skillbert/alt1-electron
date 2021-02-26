#include <cstring>
#include <iostream>
#include <string>
#include "os.h"
#include "libproc.h"

void OSWindow::SetBounds(JSRectangle bounds) {
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
	NSWindow* window = [this->hwnd.wnd window];
	NSRect frame = [window frame];
	int y = [[NSScreen mainScreen] frame].size.height - frame.origin.y - frame.size.height;
	return JSRectangle(frame.origin.x, y, frame.size.width, frame.size.height);
}

JSRectangle OSWindow::GetClientBounds() {
	return JSRectangle();
}

int OSWindow::GetPid() {
	return 0;
}

bool OSWindow::IsValid() {
	if (this->hwnd.winid == 0) {
		return false;
	}

	return true;
}

std::string OSWindow::GetTitle() {
	return "";
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	return Napi::BigInt::New(env, (uint64_t) this->hwnd.winid);
}

bool OSWindow::operator==(const OSWindow& other) const {
	return memcmp(&this->hwnd, &other.hwnd, sizeof(this->hwnd)) == 0;
}

bool OSWindow::operator<(const OSWindow& other) const {
	return memcmp(&this->hwnd, &other.hwnd, sizeof(this->hwnd)) < 0;
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

	if (name == "rs2client.exe") {
		name = "rs2client"; // TODO: Move this to ts
	}
	
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

void OSCaptureDesktop(void* target, size_t maxlength, int x, int y, int w, int h) {
	
}

void OSCaptureWindow(void* target, size_t maxlength, OSWindow wnd, int x, int y, int w, int h) {
	
}

void OSCaptureDesktopMulti(vector<CaptureRect> rects) {
	
}
void OSCaptureWindowMulti(OSWindow wnd, vector<CaptureRect> rects) {
	
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


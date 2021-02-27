#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include "os.h"
#include "libproc.h"
#include "mac/nswindow.h"

bool isCgWindowId(Window wnd) {
	return wnd.cg.magicNo == MAGIC_WINID;
}

void OSWindow::SetBounds(JSRectangle bounds) {
	std::cout << "setbound base call !!" << std::endl;
}

JSRectangle OSWindow::GetBounds() {
	std::cout << "getbound base call !!" << std::endl;
	return JSRectangle();
}

JSRectangle OSWindow::GetClientBounds() {
	return this->GetBounds();
}

int OSWindow::GetPid() {
	std::cout << "getpid base call !!" << std::endl;
	return 0;
}

bool OSWindow::IsValid() {
	return false;
}

std::string OSWindow::GetTitle() {
	std::cout << "gettitle base call !!" << std::endl;
	return "";
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
	size_t size = sizeof(OSRawWindow) / sizeof(uint64_t);
	return Napi::BigInt::New(env, 0, size, (uint64_t*) &this->hwnd);
}

std::unique_ptr<OSWindow> OSWindow::FromJsValue(const Napi::Value jsval) {
	auto handle = jsval.As<Napi::BigInt>();

	OSRawWindow buf = DEFAULT_OSRAWWINDOW;
	int sign;
	size_t bufSize = sizeof(OSRawWindow) / sizeof(uint64_t);
	handle.ToWords(&sign, &bufSize, (uint64_t*) &buf);

	if (buf.cg.magicNo == MAGIC_WINID) {
		std::cout << "fromjs returning cgoswindow" << std::endl;
		return std::make_unique<OSWindow>(buf);
	} else {
		std::cout << "fromjs returning nsoswindow" << std::endl;
		return std::make_unique<NSOSWindow>(buf.view);
	}
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


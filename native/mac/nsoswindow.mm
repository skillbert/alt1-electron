#include "nsoswindow.h"

void NSOSWindow::SetBounds(JSRectangle bounds) {
	assert(bounds.width > 0);
	assert(bounds.height > 0);
	
	NSWindow* window = [this->view window];
	if (window == NULL) {
		return;
	}
	
	int y = [[NSScreen mainScreen] frame].size.height - bounds.y - bounds.height;
	NSRect rect = NSMakeRect(bounds.x, y, bounds.width, bounds.height);
	[window setFrame:rect display:YES];
}

JSRectangle NSOSWindow::GetBounds() {
	NSWindow* window = [this->view window];
	NSRect frame = [window frame];
	int y = [[NSScreen mainScreen] frame].size.height - frame.origin.y - frame.size.height;
	return JSRectangle(frame.origin.x, y, frame.size.width, frame.size.height);
}

JSRectangle NSOSWindow::GetClientBounds() {
	return this->GetBounds();
}

int NSOSWindow::GetPid() {
	return getpid();
}

bool NSOSWindow::IsValid() {
	return this->view != NULL && [this->view window] != nil;
}

std::string NSOSWindow::GetTitle() {
	NSWindow* window = [this->view window];
	return std::string([[window title] UTF8String]);
}

CGWindowID NSOSWindow::cgWindowID() {
	NSWindow* window = [this->view window];
	return [window windowNumber];
}

#include "cgoswindow.h"

void CGOSWindow::SetBounds(JSRectangle bounds) {
}

JSRectangle CGOSWindow::GetBounds() {
	NSDictionary* bound = this->getInfo()[(id) kCGWindowBounds];
	if (bound == nil) {
		return JSRectangle(0, 0, 0, 0);
	}

	CGRect rect;
	if (CGRectMakeWithDictionaryRepresentation((CFDictionaryRef) bound, &rect) == NO) {
		return JSRectangle(0, 0, 0, 0);
	}
	return JSRectangle(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}

JSRectangle CGOSWindow::GetClientBounds() {
	return this->GetBounds();
}

int CGOSWindow::GetPid() {
	NSNumber* number = this->getInfo()[(id) kCGWindowOwnerPID];
	return [number intValue];
}

bool CGOSWindow::IsValid() {
	if (this->winid == 0) {
		return false;
	}
	
	return this->getInfo() != nil;
}

std::string CGOSWindow::GetTitle() {
	NSString* name = this->getInfo()[(id) kCGWindowName];
	return std::string([name UTF8String]);
}

NSDictionary* CGOSWindow::getInfo() {
	NSArray* data = CFBridgingRelease(CGWindowListCopyWindowInfo(kCGWindowListOptionAll, this->winid));
	if (data == nil) {
		return nil;
	}

	for (NSDictionary *info in data) {
		NSNumber* windowNumber = info[(id) kCGWindowNumber];
		if ([windowNumber unsignedIntValue] == this->winid) {
			return info; // does this increase arc?
		}
	}
	return nil;
}

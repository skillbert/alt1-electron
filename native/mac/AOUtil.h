//
//  AOUtil.h
//  addon
//
//  Created by user on 8/19/22.
//

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import "AONSWindowDelegate.h"
#include "stdint.h"
#include "../os.h"

#define MAX_DISPLAY_COUNT 10
#define MENU_BAR_HEIGHT 24
#define TITLE_BAR_HEIGHT 28

NS_ASSUME_NONNULL_BEGIN
#define array_count(a) (sizeof((a)) / sizeof(*(a)))

@interface AOUtil : NSObject
+ (BOOL)isRsWindowActive;
+ (BOOL) macOSGetMouseState;
+ (void) macOSNewWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback;
+ (void) macOSRemoveWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback;

+ (pid_t)focusedPid;
+ (pid_t) pidForWindow:(uintptr_t) winid;

+ (CFDictionaryRef) findWindow:(uintptr_t) winid;

+ (CGWindowID)appFocusedWindow:(pid_t)pid;
+ (CGRect) appBounds:(pid_t) pid;
+ (NSString *) appTitle:(pid_t) pid;
+ (CGFloat) findScalingFactor: (CGDirectDisplayID) displayId;
+ (CGDirectDisplayID) findScreenForRect: (CGRect) bounds;

+ (void) OSCaptureWindowMulti:(OSWindow)wnd withRects:(vector<CaptureRect>)rects;
+ (void) captureImageFile:(CGImageRef) imageRef withInfo: (NSString*) str withBounds: (CGRect) rect;
+ (BOOL) CGImageResizeGetBytesByScale:(CGImageRef) image withScale: (CGFloat) scale andData: (void *)theData;
+ (void) interceptDelegate:(NSWindow *)window;
+ (BOOL) updateNotifications:(BOOL) add forObserver: (AXObserverRef) obs withAppRef: (AXUIElementRef) appRef withReferenceObj: (nullable void *)refcon withNotifications: (CFStringRef) notification, ...;
+ (void) macOSSetParent:(OSWindow) parent forWindow: (OSWindow) wnd;
@end

NS_ASSUME_NONNULL_END
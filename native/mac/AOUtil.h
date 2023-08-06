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
#import <ApplicationServices/ApplicationServices.h>
#import "AONSWindowDelegate.h"
#import "AOTrackedEvent.h"
#include <unordered_map>
#include "stdint.h"
#include "../os.h"

#define MAX_DISPLAY_COUNT 10
#define MENU_BAR_HEIGHT 24
#define TITLE_BAR_HEIGHT 28
#define array_count(a) (sizeof((a)) / sizeof(*(a)))

using namespace std;

NS_ASSUME_NONNULL_BEGIN

@interface AOUtil : NSObject
+ (BOOL) shouldBeOnTop;
+ (BOOL) isFullScreen:(CGRect) bounds;
+ (BOOL) macOSGetMouseState;
+ (void) macOSNewWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback;
+ (void) macOSRemoveWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback;
+ (void) macOSSetParent:(OSWindow) parent forWindow: (OSWindow) wnd;
+ (void) updateWindow:(NSWindow*) window;

+ (pid_t) focusedPid;
+ (pid_t) pidForWindow:(uintptr_t) winid;
+ (CFDictionaryRef) findWindow:(uintptr_t) winid;

+ (CGWindowID) appMainWindow:(pid_t)pid;
+ (CGWindowID) appFocusedWindow:(pid_t)pid;
+ (CGRect) appBounds:(pid_t) pid;
+ (NSString *) appTitle:(pid_t) pid;

+ (CGFloat) findScalingFactor: (CGDirectDisplayID) displayId;
+ (CGDirectDisplayID) findScreenForRect: (CGRect) bounds;

+ (void) capture:(OSWindow)wnd withRects:(vector<CaptureRect>)rects;
+ (void) captureImageFile:(CGImageRef) imageRef withFilename: (NSString*)filename;
+ (CGImageRef) redrawImage:(CGImageRef) image;
+ (BOOL) drawImage:(CGImageRef) image ontoBuffer: (void *)theData withScale: (CGFloat) scale;
+ (void) interceptDelegate:(NSWindow *)window;
+ (BOOL) updateNotifications:(BOOL) add forObserver: (AXObserverRef) obs withAppRef: (AXUIElementRef) appRef withReferenceObj: (nullable void *)refcon withNotifications: (CFStringRef) notification, ...;
@end

NS_ASSUME_NONNULL_END

//
//  AOUtil.m
//  addon
//
//  Created by user on 8/19/22.
//

#import "AOUtil.h"

@interface AOUtil()
+ (void (^)(void)) createEventBlocks;
+ (void) accessibilityChanged:(NSNotification*)note;
+ (void) handleRsDidLaunch: (pid_t) pid;
+ (void) handleRsDidTerminate: (pid_t) pid;
+ (void) handleAXActivate;
+ (void) handleAXDeactivate;
+ (void) handleAXMiniaturize;
+ (void) handleAXDeminiaturize;
+ (void) handleAXMove: (pid_t) pid;
+ (void) handleAXResize: (pid_t) pid;
+ (void) handleClick:(NSEvent*)event;
@end

extern "C" AXError _AXUIElementGetWindow(AXUIElementRef, CGWindowID *out) __attribute__((weak_import));

const NSMutableDictionary<NSNumber*, NSNumber*> *winIdPidMap = [NSMutableDictionary new];
const NSMutableDictionary<NSNumber*, id> *pidAppRefMap = [NSMutableDictionary new];
const NSMutableDictionary<NSNumber*, id> *winIdObsMap = [NSMutableDictionary new];
const NSMutableDictionary<NSNumber*, NSNumber*> *trackedWindows = [NSMutableDictionary new];
const NSMutableDictionary<NSNumber*, NSView*> *trackedViews = [NSMutableDictionary new];

static inline bool ax_privilege() {
    const void *keys[] = {kAXTrustedCheckOptionPrompt};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(nullptr, keys, values, array_count(keys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    bool result = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    return result;
}

static bool str_eq(CFStringRef theString, CFStringRef theOtherString) {
    return CFStringCompare(theString, theOtherString, 0) == kCFCompareEqualTo;
}

// Detect if the app's windows are on the active space or not
static BOOL areWeOnActiveSpaceNative() {
    BOOL isOnActiveSpace = NO;
    for (NSWindow *window in [[NSApplication sharedApplication] orderedWindows]) {
        isOnActiveSpace = [window isOnActiveSpace];
        if (isOnActiveSpace) {
            break;
        }
    }
    return isOnActiveSpace;
}

static void ax_callback(AXObserverRef observer, AXUIElementRef element, CFStringRef notification, void *refcon) {
    NSLog(@"==%@==", notification);
    pid_t pid;
    AXError err = AXUIElementGetPid(element, &pid);
    if (err != kAXErrorSuccess) {
        NSLog(@"no pid: %@", @(err));
        return;
    }
    if (str_eq(notification, kAXUIElementDestroyedNotification)) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [AOUtil handleRsDidTerminate:pid];
        });
    } else if (str_eq(notification, kAXApplicationActivatedNotification)) {
        [AOUtil handleAXActivate];
    } else if (str_eq(notification, kAXApplicationDeactivatedNotification)) {
        [AOUtil handleAXDeactivate];
    } else if (str_eq(notification, kAXWindowMiniaturizedNotification)) {
        [AOUtil handleAXMiniaturize];
    } else if (str_eq(notification, kAXWindowDeminiaturizedNotification)) {
        [AOUtil handleAXDeminiaturize];
    } else if (str_eq(notification, kAXWindowMovedNotification)) {
        [AOUtil handleAXMove: pid];
    } else if (str_eq(notification, kAXWindowResizedNotification)) {
        [AOUtil handleAXResize:pid];
    }
}

typedef void (^blockType)(void);

static pid_t _rsPid;
static pid_t _frontPid;
static bool leftMouseDown = false;
static bool rightMouseDown = false;

@implementation AOUtil
+(void) initialize {
    dispatch_once_t once;
    dispatch_once(&once, [AOUtil createEventBlocks]);
}

+ (void (^)(void)) createEventBlocks {
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(accessibilityChanged:) name:@"com.apple.accessibility.api" object:nil];

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidActivateApplicationNotification
                                                                    object:NULL
                                                                     queue:NULL
                                                                usingBlock:^(NSNotification *note) {
        NSDictionary *userInfo = [note userInfo];
        NSRunningApplication *frontApp = [userInfo objectForKey:NSWorkspaceApplicationKey];
        _frontPid = [frontApp processIdentifier];
        if(_rsPid == -1 && [[frontApp localizedName] caseInsensitiveCompare:@"rs2client"]) {
            _rsPid = _frontPid;
        }
        NSLog(@"FrontApp: %d %@", _frontPid, [frontApp localizedName]);
    }];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidTerminateApplicationNotification
                                                                    object:NULL
                                                                     queue:NULL
                                                                usingBlock:^(NSNotification *note) {
        NSDictionary *userInfo = [note userInfo];
        NSRunningApplication *termApp = [userInfo objectForKey:NSWorkspaceApplicationKey];
        if([[termApp localizedName] caseInsensitiveCompare:@"rs2client"]) {
            [AOUtil handleRsDidTerminate:[termApp processIdentifier]];
            NSLog(@"RS Terminated: %d %@", [termApp processIdentifier], [termApp localizedName]);
        }
    }];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                                                    object:NULL
                                                                     queue:NULL
                                                                usingBlock:^(NSNotification *note) {
        NSDictionary *userInfo = [note userInfo];
        NSRunningApplication *app = [userInfo objectForKey:NSWorkspaceApplicationKey];
        if([[app localizedName] caseInsensitiveCompare:@"rs2client"]) {
            [AOUtil handleRsDidLaunch:[app processIdentifier]];
            NSLog(@"RS Launched: %d %@", [app processIdentifier], [app localizedName]);
        }
    }];

    return ^{
        auto block = ^(NSEvent *evt) {
            if (evt.type == NSEventTypeLeftMouseDown) {
                leftMouseDown = true;
            } else if (evt.type == NSEventTypeLeftMouseUp) {
                leftMouseDown = false;
                [AOUtil handleClick:evt];
            } else if (evt.type == NSEventTypeRightMouseDown) {
                rightMouseDown = true;
            } else if (evt.type == NSEventTypeRightMouseUp) {
                rightMouseDown = false;
                [AOUtil handleClick:evt];
            }
        };
        auto localblock = ^NSEvent *(NSEvent *evt) {
            if (evt.type == NSEventTypeLeftMouseDown) {
                leftMouseDown = true;
            } else if (evt.type == NSEventTypeLeftMouseUp) {
                leftMouseDown = false;
            }
            return evt;
        };
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskAppKitDefined handler:block];
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskAppKitDefined handler:localblock];
    };
}

+ (void) accessibilityChanged:(NSNotification *)note {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if(!ax_privilege()) {
            NSLog(@"Still no accessibility permission. This won't work.");
        }
    });
}

+ (void) handleRsDidLaunch:(pid_t)pid {
    CGWindowID windowId = [AOUtil appFocusedWindow:pid];
    [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
        return e.type == WindowEventType::Show && e.window == 0;
    } andCallback:[windowId](Napi::Env env, Napi::Function callback) {
        NSLog(@"mac: Notified alt1 of new RS instance [%d]", windowId);
        callback.Call({Napi::BigInt::New(env, (uint64_t)windowId), Napi::Number::New(env, 2)});
    }];
}

+ (void) handleRsDidTerminate:(pid_t)pid {
    NSNumber *pidRef = [NSNumber numberWithInt:pid];
    NSRunningApplication *application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if(![application isTerminated]) {
        CGWindowID nwindowId = [AOUtil appFocusedWindow:pid];
        if(nwindowId != kCGNullWindowID) {
            if([trackedWindows objectForKey:pidRef] != nil) {
                NSNumber *rsWinIdRef = trackedWindows[pidRef];
                CGWindowID rsWinId = [rsWinIdRef unsignedIntValue];
                NSLog(@"It appears the RS Window ID did not really change... from %@ ==> %@", @(rsWinId), @(nwindowId));
                if(rsWinId == nwindowId) {
                    BOOL hasSwitchedToFullScreenApp = !areWeOnActiveSpaceNative();
                    if(hasSwitchedToFullScreenApp) {
                        NSArray<NSView*> *allViews = [trackedViews allValues];
                        for(NSView *cview in allViews) {
                            NSLog(@"View:%@", cview);
                            NSWindow *window = [cview window];
                            [AOUtil updateWindow:window];
                        }
                    }
                    return;
                }
            } else {
                NSLog(@"It appears the RS Window ID Changed from %@ ==> %@", @"UNKNOWN", @(nwindowId));
            }
        } else {
            NSLog(@"It appears the RS Window Closed");
        }
    } else {
        NSLog(@"It appears the RS Window Closed");
    }
    if([trackedWindows objectForKey:pidRef] != nil) {
        NSNumber *rsWinIdRef = trackedWindows[pidRef];
        CGWindowID rsWinId = [rsWinIdRef unsignedIntValue];
        NSLog(@"Closing %u", rsWinId);
        [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
            return e.type == WindowEventType::Close && e.window == rsWinId;
        } andCallback:[](Napi::Env env, Napi::Function callback) {
            callback.Call({});
        }];
        [trackedWindows removeObjectForKey:pidRef];
        [trackedViews removeAllObjects];
        [rsWinIdRef release];
    }
    [pidRef release];
}

+ (void) handleAXActivate {
    NSArray<NSView*> *allViews = [trackedViews allValues];
    for(NSView *cview in allViews) {
        NSWindow *window = [cview window];
        [AOUtil updateWindow:window];
    }
}

+ (void) handleAXDeactivate {
    NSArray<NSView*> *allViews = [trackedViews allValues];
    for(NSView *cview in allViews) {
        NSWindow *window = [cview window];
        [AOUtil updateWindow:window];
    }
}

+ (void) handleAXMiniaturize {
    NSArray<NSView*> *allViews = [trackedViews allValues];
    for(NSView *cview in allViews) {
        NSLog(@"View:%@", cview);
        NSWindow *window = [cview window];
        [window setIsMiniaturized:true];
    }

}

+ (void) handleAXDeminiaturize {
    NSArray<NSView*> *allViews = [trackedViews allValues];
    for(NSView *cview in allViews) {
        NSLog(@"View:%@", cview);
        NSWindow *window = [cview window];
        [window setIsMiniaturized:false];
    }
}

+ (void) handleAXMove:(pid_t) pid {
    CGRect winRect = [AOUtil appBounds:pid];
    NSLog(@"RS: %d (%0.0f,%0.0f) [%0.0fx%0.0f]", pid, winRect.origin.x, winRect.origin.y, winRect.size.width, winRect.size.height);
    __block CGWindowID rsWinId = [AOUtil appFocusedWindow:pid];
    JSRectangle bounds = JSRectangle(static_cast<int>(winRect.origin.x), static_cast<int>(winRect.origin.y), static_cast<int>(winRect.size.width), static_cast<int>(winRect.size.height));
    [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
        return e.type == WindowEventType::Move && e.window == rsWinId;
    } andCallback:[bounds](Napi::Env env, Napi::Function callback) {
        NSLog(@"MOVE: bounds=[%d,%d %dx%d]", bounds.x, bounds.y, bounds.width, bounds.height);
        callback.Call({bounds.ToJs(env), Napi::String::New(env, "end")});
    }];

}

+ (void) handleAXResize:(pid_t) pid {
    CGRect winRect = [AOUtil appBounds:pid];
    NSLog(@"RS: %d (%0.0f,%0.0f) [%0.0fx%0.0f]", pid, winRect.origin.x, winRect.origin.y, winRect.size.width, winRect.size.height);
    __block CGWindowID rsWinId = [AOUtil appFocusedWindow:pid];
    JSRectangle bounds = JSRectangle(static_cast<int>(winRect.origin.x), static_cast<int>(winRect.origin.y), static_cast<int>(winRect.size.width), static_cast<int>(winRect.size.height));
    [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
        return e.type == WindowEventType::Move && e.window == rsWinId;
    } andCallback:[bounds](Napi::Env env, Napi::Function callback) {
        NSLog(@"RESIZE: bounds=[%d,%d %dx%d]", bounds.x, bounds.y, bounds.width, bounds.height);
        callback.Call({bounds.ToJs(env), Napi::String::New(env, "end")});
    }];
}
+ (void) handleClick:(NSEvent*)event {
    if([event type] == NSEventTypeLeftMouseUp || [event type] == NSEventTypeRightMouseUp) {
        __block CGWindowID evtWindowId = [@([event windowNumber]) unsignedIntValue];
        [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
            return e.type == WindowEventType::Click && e.window == evtWindowId;
        } andCallback:[](Napi::Env env, Napi::Function callback){callback.Call({});}];
    }
}

+ (BOOL) macOSGetMouseState {
    return leftMouseDown;
}

+ (void) macOSNewWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback {
    NSLog(@"PID: %d", [[NSRunningApplication currentApplication] processIdentifier]);
    if(!ax_privilege()) {
        NSLog(@"alt1 will not work without accessibility permissions");
        exit(1);
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
    CGRequestScreenCaptureAccess();
#endif
    NSLog(@"macOSNewWindowListener: %u %d", window, type);
    [AOTrackedEvent push: window andType:type andCallback:callback];
}

+ (void) macOSRemoveWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback {
    [AOTrackedEvent remove:window andType:type andCallback:callback];
}

+ (void) macOSSetParent:(OSWindow) parent forWindow: (OSWindow) wnd {
    NSView *view = wnd.handle.view;
    [AOUtil interceptDelegate:[view window]];
    NSInteger winnum = [[view window] windowNumber];
    NSNumber *winIdRef = @(winnum);
    if (parent.handle.winid != 0) {
        _rsPid = [AOUtil pidForWindow: parent.handle.winid];
        NSLog(@"RSPID: %d, RSWIN: %@", _rsPid, @(parent.handle.winid));
        NSNumber *pidRef = @(_rsPid);
        NSNumber *pwinIdRef = @(parent.handle.winid);
        trackedWindows[pidRef] = pwinIdRef;
    }
    NSLog(@"macOSSetWindowParent: %lu => %lu", winnum, parent.handle.winid);
    NSLog(@"%@ %@ movable, %@ resizable", winIdRef, [view mouseDownCanMoveWindow] ? @"IS" : @"IS NOT", [[view window] isResizable] ? @"IS" : @"IS NOT");
    AXUIElementRef appRef = NULL;
    if (parent.handle.winid == 0) {
        NSNumber *pidRef = winIdPidMap[winIdRef];
        AXObserverRef obs;
        if([winIdObsMap objectForKey:winIdRef] != nil) {
            obs = (AXObserverRef) winIdObsMap[winIdRef];
        } else {
            NSLog(@"Unable to find observer for %@", winIdRef);
            return;
        }
        if ([pidAppRefMap objectForKey:pidRef] != nil) {
            appRef = (AXUIElementRef) pidAppRefMap[pidRef];
        } else {
            NSLog(@"Unable to find appref for %@", pidRef);
            return;
        }
        if ([winIdObsMap objectForKey:winIdRef] != nil) {
            [winIdObsMap removeObjectForKey:winIdRef];
            if ([[winIdObsMap allValues] containsObject:(id)obs]) {
                if ([AOUtil updateNotifications:false forObserver:obs withAppRef:appRef withReferenceObj:NULL withNotifications:kAXApplicationShownNotification, kAXApplicationHiddenNotification, kAXApplicationActivatedNotification,
                     kAXApplicationDeactivatedNotification, kAXWindowMiniaturizedNotification, kAXWindowDeminiaturizedNotification, kAXWindowMovedNotification,
                     kAXWindowResizedNotification, kAXUIElementDestroyedNotification, NULL]) {
                    [trackedViews removeObjectForKey:@(winnum)];
                    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);
                    NSLog(@"Notifications removed for %@ because no more window ids exist for it", pidRef);
                }
            }
        }
    } else {
        pid_t pid = [AOUtil pidForWindow: parent.handle.winid];
        NSNumber *pidRef = @(pid);
        winIdPidMap[winIdRef] = pidRef;
        if ([pidAppRefMap objectForKey:pidRef] != nil) {
            appRef = (AXUIElementRef) pidAppRefMap[pidRef];;
        } else {
            appRef = AXUIElementCreateApplication(pid);
        }
        pidAppRefMap[pidRef] = (id)appRef;
        AXError err;
        AXObserverRef obs;
        err = AXObserverCreate(pid, (AXObserverCallback) & ax_callback, &obs);
        if (err != kAXErrorSuccess) {
            NSLog(@"Error creating observer: %d", err);
            return;
        } else {
            NSLog(@"Setting observer for %@: %@", winIdRef, [NSValue valueWithPointer:obs]);
            winIdObsMap[winIdRef] = (id)obs;
        }
        
        if ([AOUtil updateNotifications:true forObserver:obs withAppRef:appRef withReferenceObj:nullptr withNotifications:kAXApplicationShownNotification, kAXApplicationHiddenNotification, kAXApplicationActivatedNotification,
             kAXApplicationDeactivatedNotification, kAXWindowMiniaturizedNotification, kAXWindowDeminiaturizedNotification, kAXWindowMovedNotification,
             kAXWindowResizedNotification, kAXUIElementDestroyedNotification, NULL]) {
            [trackedViews setObject: view forKey: @(winnum)];

            CFRunLoopAddSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);
        }
    }
}

+ (void) updateWindow:(NSWindow*) window {
    if ([AOUtil isRsWindowActive]) {
        [window setLevel:NSScreenSaverWindowLevel];
        [window makeKeyAndOrderFront:nil];
    } else {
        [window setLevel:NSNormalWindowLevel];
    }
}

+ (BOOL) isRsWindowActive {
    pid_t aoPid = [[NSRunningApplication currentApplication] processIdentifier];
    
//    NSLog(@"F: %d, C: %d, one: %@ two: %@", _frontPid, aoPid, _frontPid == aoPid ? @"YES" : @"NO", _frontPid == _rsPid ? @"YES" : @"NO");
    return _frontPid == aoPid || _frontPid == _rsPid;
}

+ (BOOL) isFullScreen:(CGRect) bounds {
    CGDirectDisplayID *displays = (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);
    uint32_t count = 0;
    CGError err = CGGetActiveDisplayList(MAX_DISPLAY_COUNT, displays, &count);
    if (err != kCGErrorSuccess) {
        printf("error: %d\n", err);
        return false;
    }
    err = CGGetDisplaysWithRect(bounds, MAX_DISPLAY_COUNT, displays, &count);
    if (err != kCGErrorSuccess) {
        printf("error: %d\n", err);
        return false;
    }
    if (count > 1) {
        if (displays) {
            free(displays);
        }
        return false;
    }
    CGRect screenBounds = CGDisplayBounds(displays[0]);
    free(displays);
    return (bounds.origin.x == screenBounds.origin.x && bounds.origin.y == screenBounds.origin.y && bounds.size.width == screenBounds.size.width && bounds.size.height == screenBounds.size.height);
}

+ (pid_t) focusedPid {
    NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
    return app.processIdentifier;
}

+ (pid_t) pidForWindow:(uintptr_t) winid {
    pid_t pid;
    CGWindowID windowId = (CGWindowID) winid;
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, windowId);
    if (CFArrayGetCount(windowList) == 0) {
        return -1;
    }
    CFDictionaryRef entry = (CFDictionaryRef) CFArrayGetValueAtIndex(windowList, 0);
    CFNumberRef pidRef = (CFNumberRef)CFDictionaryGetValue(entry, kCGWindowOwnerPID);
    if (CFNumberGetValue(pidRef, kCFNumberIntType, &pid)) {
        return pid;
    }
    return pid;
}

+ (CFDictionaryRef) findWindow:(uintptr_t) winid {
    CGWindowID windowId = (CGWindowID) winid;
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, windowId);
    if (CFArrayGetCount(windowList) == 0) {
        CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
        NSArray<NSDictionary*>* dwindowList = (NSArray<NSDictionary*>*)windowList;
        for(NSDictionary* cdict in dwindowList) {
            if((NSNumber*)cdict[(NSString*)kCGWindowNumber] != nil) {
                NSNumber *winRef = (NSNumber*)cdict[(NSString*)kCGWindowNumber];
                if([winRef unsignedIntValue] == windowId) {
                    return (CFDictionaryRef)cdict;
                }
            }
        }
        return nullptr;
    }
    CFDictionaryRef entry = (CFDictionaryRef) CFArrayGetValueAtIndex(windowList, 0);
    return entry;
}


+ (CGWindowID) appFocusedWindow:(pid_t) pid {
    AXUIElementRef frontMostApp;
    AXUIElementRef frontMostWindow;

    // Get the frontMostApp
    frontMostApp = AXUIElementCreateApplication(pid);

    // Copy window attributes
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXFocusedWindowAttribute, (CFTypeRef*)&frontMostWindow);
    if (err != kAXErrorSuccess) {
        NSLog(@"no focused window %@ %d", [NSValue valueWithPointer:frontMostApp], err);
        CFRelease(frontMostApp);
        return kCGNullWindowID;
    }
    CGWindowID windowId;
    err = _AXUIElementGetWindow(frontMostWindow, &windowId);
    if (err != kAXErrorSuccess) {
        NSLog(@"no window id %d", err);
        if (frontMostApp != nil) {
            CFRelease(frontMostApp);
        }
        return kCGNullWindowID;
    }
    return windowId;
}

+ (CGRect) appBounds:(pid_t) pid {
    AXValueRef temp;
    CGSize windowSize;
    CGPoint windowPosition;
    AXUIElementRef frontMostApp;
    AXUIElementRef frontMostWindow;

    // Get the frontMostApp
    frontMostApp = AXUIElementCreateApplication(pid);

    // Copy window attributes
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXFocusedWindowAttribute, (CFTypeRef*)&frontMostWindow);
    if (err != kAXErrorSuccess) {
        NSLog(@"no focused window %@ %d", [NSValue valueWithPointer:frontMostApp], err);
        CFRelease(frontMostApp);
        return CGRectNull;
    }
    err = AXUIElementCopyAttributeValue(frontMostWindow, kAXSizeAttribute, (CFTypeRef *)&temp);
    if (err != kAXErrorSuccess) {
        NSLog(@"no window size %d", err);
        if (frontMostApp != nil) {
            CFRelease(frontMostApp);
        }
        return CGRectNull;
    }
    AXValueGetValue(temp, kAXValueTypeCGSize, &windowSize);
    CFRelease(temp);
    err = AXUIElementCopyAttributeValue(frontMostWindow, kAXPositionAttribute, (CFTypeRef*)&temp);
    if (err != kAXErrorSuccess) {
        if (frontMostWindow != nil) {
            CFRelease(frontMostWindow);
        }
        if (frontMostApp != nil) {
            CFRelease(frontMostApp);
        }
        NSLog(@"no window position %d", err);
        return CGRectNull;
    }
    AXValueGetValue(temp, kAXValueTypeCGPoint, &windowPosition);
    CFRelease(temp);
    if (frontMostWindow != nil) {
        CFRelease(frontMostWindow);
    }
    if (frontMostApp != nil) {
        CFRelease(frontMostApp);
    }
    return CGRectMake(windowPosition.x, windowPosition.y, windowSize.width, windowSize.height);
}

+ (NSString *) appTitle:(pid_t) pid {
    AXUIElementRef frontMostApp;
    AXUIElementRef frontMostWindow;
    CFStringRef frontMostWindowTitle;

    // Get the frontMostApp
    frontMostApp = AXUIElementCreateApplication(pid);

    // Copy window attributes
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXFocusedWindowAttribute, (CFTypeRef*)&frontMostWindow);
    if (err != kAXErrorSuccess) {
        NSLog(@"no focused window %@ %d", [NSValue valueWithPointer:frontMostApp], err);
        CFRelease(frontMostApp);
        return @"";
    }
    err = AXUIElementCopyAttributeValue(frontMostWindow, kAXTitleAttribute, (CFTypeRef*)&frontMostWindowTitle);
    if (err != kAXErrorSuccess) {
        NSLog(@"no window size %d", err);
        if (frontMostApp != nil) {
            CFRelease(frontMostApp);
        }
        return @"";
    }
    NSLog(@"App title: %d %@", pid, frontMostWindowTitle);
    return (NSString *)frontMostWindowTitle;
}


+ (CGFloat) findScalingFactor: (CGDirectDisplayID) displayId {
    NSScreen *screen = [NSScreen mainScreen];
    NSArray *screens = [NSScreen screens];
    for (NSScreen *iscreen in screens) {
        NSDictionary *desc = iscreen.deviceDescription;
        CGDirectDisplayID idisplayID = [desc[@"NSScreenNumber"] unsignedIntValue];
        if (displayId == idisplayID) {
            screen = iscreen;
            break;
        }
    }
    return [screen backingScaleFactor];
}

+ (CGDirectDisplayID) findScreenForRect: (CGRect) bounds {
    CGDirectDisplayID *displays = (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);
    uint32_t count = 0;
    CGError err = CGGetActiveDisplayList(MAX_DISPLAY_COUNT, displays, &count);
    if (err != kCGErrorSuccess) {
        printf("error: %d\n", err);
        return 0;
    }
    err = CGGetDisplaysWithRect(bounds, 1, displays, &count);
    if (err != kCGErrorSuccess) {
        printf("error: %d\n", err);
        return 0;
    }
    CGDirectDisplayID id = displays[0];
    free(displays);
    return id;
}

+(void) OSCaptureWindowMulti:(OSWindow) wnd withRects: (vector <CaptureRect>) rects {
    CFDictionaryRef windowInfo = [AOUtil findWindow: wnd.handle.winid];
    if (windowInfo == nullptr) {
        printf("window nil - something changed!\n");
        return;
    }
    CGRect screenBounds;
    CGRectMakeWithDictionaryRepresentation((CFDictionaryRef) CFDictionaryGetValue(windowInfo, kCGWindowBounds), &screenBounds);
//    BOOL isFs = [AOUtil isFullScreen: screenBounds];
    CGWindowID windowId = static_cast<CGWindowID>(wnd.handle.winid);
    CGImageRef scaledImageRef = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowId, kCGWindowImageNominalResolution | kCGWindowImageBoundsIgnoreFraming);
//    [AOUtil captureImageFile:scaledImageRef withInfo:@"fullimage" withBounds:screenBounds];

    for (vector<CaptureRect>::iterator it = rects.begin(); it != rects.end(); ++it) {
        CGRect iscreenBounds = CGRectMake((CGFloat) it->rect.x, (CGFloat) it->rect.y, (CGFloat) it->rect.width, (CGFloat) it->rect.height);
        CGImageRef _imageRef = CGImageCreateWithImageInRect(scaledImageRef, iscreenBounds);
        CGImageRef imageRef = [AOUtil CGImageResize:_imageRef toSize:iscreenBounds.size];
        CGImageRelease(_imageRef);
//        [AOUtil captureImageFile:imageRef withInfo:@"clip" withBounds:iscreenBounds];
//        NSLog(@"C: (%d,%d) [%dx%d] ?? (%zu vs %d)", it->rect.x, it->rect.y, it->rect.width, it->rect.height, it->size, 4 * it->rect.width * it->rect.height);

        if (![AOUtil CGImageResizeGetBytesByScale: imageRef withScale:1.0 andData:it->data]) {
            fprintf(stderr, "error: could not copy image data\n");
        }
        CGImageRelease(imageRef);
    }
    CGImageRelease(scaledImageRef);
}


+ (CGImageRef) CGImageResize:(CGImageRef) image toSize: (CGSize) maxSize {
    // calculate size
//    CGFloat ratio = MAX(CGImageGetWidth(image) / maxSize.width, CGImageGetHeight(image) / maxSize.height);
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    // resize
    CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, width * 4, colorspace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorspace);
    if (context == NULL) return nil;
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    // draw image to context (resizing it)
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    // extract resulting image from context
    CGImageRef imgRef = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    return imgRef;
}

+ (BOOL) CGImageResizeGetBytesByScale:(CGImageRef) image withScale: (CGFloat) scale andData: (void *)theData {
    // calculate size
    size_t width = CGImageGetWidth(image) / static_cast<size_t>(scale);
    size_t height = CGImageGetHeight(image) / static_cast<size_t>(scale);
    // resize
    CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(theData, width, height, 8, width * 4, colorspace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorspace);
    if (context == NULL) return false;
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    // draw image to context (resizing it)
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    // extract resulting image from context
    CGContextRelease(context);
    return true;
}

+ (void) captureImageFile:(CGImageRef) imageRef withInfo: (NSString*) str withBounds: (CGRect) rect {
    NSString *format = @"/Users/user/Desktop/ss/%@_%@_window_%0.0f_%0.0f_%0.0fx%0.0f.png";
    NSString *name = [NSString stringWithFormat:format, @"rsclient", str, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height];
    bool b = [[NSFileManager defaultManager] fileExistsAtPath:name];
    if(b) {
        return;
    }
    CFURLRef fileURL = static_cast<CFURLRef>([NSURL fileURLWithPath:name]);
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(fileURL, CFSTR("public.png"), 1, nullptr);
    CGImageDestinationAddImage(destination, imageRef, nullptr);
    CGImageDestinationFinalize(destination);
    CFRelease(destination);
}

+ (void) interceptDelegate:(NSWindow *)window {
    if ([window delegate] == nil || [[[window delegate] class] isEqualTo:[AONSWindowDelegate class]] ) {
        return;
    }
    [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary];
    id<NSWindowDelegate> dd = [window delegate];
    AONSWindowDelegate *d = [[AONSWindowDelegate alloc] initWithDelegate:dd];
    [window setDelegate:d];
    NSLog(@"New Delegate %@", d);
    NSLog(@"Old Delegate %@", dd);
}

+ (BOOL) updateNotifications:(BOOL) add forObserver: (AXObserverRef) obs withAppRef: (AXUIElementRef) appRef withReferenceObj: (nullable void *)refcon withNotifications: (CFStringRef) notification, ... {
    va_list args;
    va_start(args, notification);
    AXError err;
    CFStringRef current = notification;
    while (current != NULL) {
        if (add) {
            err = AXObserverAddNotification(obs, appRef, current, refcon);
        } else {
            err = AXObserverRemoveNotification(obs, appRef, current);
        }
        if (err != kAXErrorSuccess) {
            NSLog(@"error %@ notification %@: %d", (add ? @"adding" : @"removing"), current, err);
            va_end(args);
            return false;
        } else {
            NSLog(@"%@ %@ notification!", (add ? @"added" : @"removed"), current);
        }
        current = va_arg(args, CFStringRef);
    }
    va_end(args);
    return true;
}

@end

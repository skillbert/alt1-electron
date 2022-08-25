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
+ (void) handleAXDestroyed: (pid_t) pid;
+ (void) handleRsDidLaunch: (pid_t) pid;
+ (void) handleRsDidTerminate: (pid_t) pid;
+ (void) handleAXActivate: (pid_t) pid;
+ (void) handleAXDeactivate: (pid_t) pid;
+ (void) handleAXMiniaturize: (pid_t) pid;
+ (void) handleAXDeminiaturize: (pid_t) pid;
+ (void) handleAXMove: (pid_t) pid;
+ (void) handleAXResize: (pid_t) pid;
+ (void) handleClick:(NSEvent*)event;
+ (NSLock*)pidLock;
+ (void)lockPidLock;
+ (void)unlockPidLock;
@end

extern "C" AXError _AXUIElementGetWindow(AXUIElementRef, CGWindowID *out) __attribute__((weak_import));

const NSMutableDictionary<NSNumber*, NSNumber*> *trackedParents = [NSMutableDictionary new];
const NSMutableDictionary<NSNumber*, NSNumber*> *trackedWindows = [NSMutableDictionary new];
const NSMutableDictionary<NSNumber*, NSMutableArray<NSView*>*> *trackedViews = [NSMutableDictionary new];

static unordered_map<pid_t, pair<AXUIElementRef, AXObserverRef>> observers;

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
    pid_t pid;
    AXError err = AXUIElementGetPid(element, &pid);
    if (err != kAXErrorSuccess) {
        NSLog(@"no pid: %@", @(err));
        return;
    }
    CGWindowID elementWindowId;
    _AXUIElementGetWindow(element, &elementWindowId);
    NSLog(@"==%@==: %@ [%@]", notification, @(pid), [NSValue valueWithPointer: element]);
//    CFArrayRef cnames;
//    if(AXUIElementCopyAttributeNames(element, &cnames) != kAXErrorSuccess) {
//        NSLog(@"error copy attribute names %u", pid);
//        return;
//    }
//    CFArrayRef cvalues;
//    if(AXUIElementCopyMultipleAttributeValues(element, cnames, 0, &cvalues) == kAXErrorSuccess) {
//        NSArray* values = (NSArray*)cvalues;
//        NSArray* names = (NSArray*)cnames;
//        for(NSString* name in names) {
//            NSLog(@"%@: %@", name, (id)[values objectAtIndex:[names indexOfObject:name]]);
//        }
//    }
//
    if (str_eq(notification, kAXUIElementDestroyedNotification)) {
        [AOUtil handleAXDestroyed:pid];
//        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
//        });
    } else if (str_eq(notification, kAXApplicationActivatedNotification)) {
        [AOUtil handleAXActivate:pid];
    } else if (str_eq(notification, kAXApplicationDeactivatedNotification)) {
        [AOUtil handleAXDeactivate:pid];
    } else if (str_eq(notification, kAXWindowMiniaturizedNotification)) {
        [AOUtil handleAXMiniaturize:pid];
    } else if (str_eq(notification, kAXWindowDeminiaturizedNotification)) {
        [AOUtil handleAXDeminiaturize:pid];
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


+ (NSLock*) pidLock {
    static NSLock* _pidLock = nil;
    if(_pidLock == nil) {
        _pidLock = [NSLock new];
    }
    return _pidLock;
}

+ (void) lockPidLock {
    [[AOUtil pidLock] lock];
}

+ (void) unlockPidLock {
    [[AOUtil pidLock] unlock];
}

+ (void (^)(void)) createEventBlocks {
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(accessibilityChanged:) name:@"com.apple.accessibility.api" object:nil];

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidActivateApplicationNotification
                                                                    object:NULL
                                                                     queue:NULL
                                                                usingBlock:^(NSNotification *note) {
        [AOUtil lockPidLock];
        NSLog(@"**%@**", NSWorkspaceDidActivateApplicationNotification);
        NSDictionary *userInfo = [note userInfo];
        NSRunningApplication *frontApp = [userInfo objectForKey:NSWorkspaceApplicationKey];
        _frontPid = [frontApp processIdentifier];
        if(_rsPid == 0 && [[frontApp localizedName] isEqualToString:@"rs2client"]) {
            _rsPid = _frontPid;
            [AOUtil handleRsDidLaunch:_rsPid];
            NSLog(@"RS Launched (Previously): %d %@", [frontApp processIdentifier], [frontApp localizedName]);
        }
        NSLog(@"FrontApp: %d %@", _frontPid, [frontApp localizedName]);
        [AOUtil unlockPidLock];
    }];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidTerminateApplicationNotification
                                                                    object:NULL
                                                                     queue:NULL
                                                                usingBlock:^(NSNotification *note) {
        NSLog(@"**%@**", NSWorkspaceDidTerminateApplicationNotification);
        [AOUtil lockPidLock];
        NSDictionary *userInfo = [note userInfo];
        NSRunningApplication *termApp = [userInfo objectForKey:NSWorkspaceApplicationKey];
        if([[termApp localizedName] isEqualToString:@"rs2client"]) {
            [AOUtil handleRsDidTerminate:[termApp processIdentifier]];
            NSLog(@"RS Terminated: %d %@", [termApp processIdentifier], [termApp localizedName]);
        }
        [AOUtil unlockPidLock];
    }];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                                                    object:NULL
                                                                     queue:NULL
                                                                usingBlock:^(NSNotification *note) {
        NSLog(@"**%@**", NSWorkspaceDidLaunchApplicationNotification);
        [AOUtil lockPidLock];
        NSDictionary *userInfo = [note userInfo];
        NSRunningApplication *app = [userInfo objectForKey:NSWorkspaceApplicationKey];
        if([[app localizedName] caseInsensitiveCompare:@"rs2client"]) {
            [AOUtil handleRsDidLaunch:[app processIdentifier]];
            NSLog(@"RS Launched: %d %@", [app processIdentifier], [app localizedName]);
        }
        [AOUtil unlockPidLock];
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
+ (void) handleAXDestroyed:(pid_t)pid {
    NSNumber *pidRef = [NSNumber numberWithInt:pid];
    NSRunningApplication *application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if(![application isTerminated]) {
        CGWindowID nwindowId = [AOUtil appFocusedWindow:pid];
        if(nwindowId != kCGNullWindowID) {
            if([trackedWindows objectForKey:pidRef] != nil) {
                CGWindowID rsWinId = [trackedWindows[pidRef] unsignedIntValue];
                NSLog(@"It appears the RS Window ID did not really change... from %@ ==> %@", @(rsWinId), @(nwindowId));
                if(rsWinId == nwindowId) {
                    BOOL hasSwitchedToFullScreenApp = !areWeOnActiveSpaceNative();
                    if(hasSwitchedToFullScreenApp) {
                        NSArray<NSView*> *allViews = trackedViews[pidRef];
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
        NSLog(@"It appears the RS Application Terminated");
    }
    if([trackedWindows objectForKey:pidRef] != nil) {
        CGWindowID rsWinId = [trackedWindows[pidRef] unsignedIntValue];
        NSLog(@"Closing %u", rsWinId);
        [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
            return e.type == WindowEventType::Close && e.window == rsWinId;
        } andCallback:[](Napi::Env env, Napi::Function callback) {
            callback.Call({});
        }];
        [trackedWindows removeObjectForKey:pidRef];
        [trackedViews removeObjectForKey:pidRef];
        [AOUtil handleRsDidTerminate:pid];
    }
    [pidRef release];
}

+ (void) handleRsDidLaunch:(pid_t)pid {
    if(observers.find(pid) == observers.end()) {
        AXUIElementRef appRef = AXUIElementCreateApplication(pid);
        AXObserverRef obs;
        AXError err = AXObserverCreate(pid, (AXObserverCallback) & ax_callback, &obs);
        if(err == kAXErrorSuccess) {
            observers[pid] = pair<AXUIElementRef, AXObserverRef>(appRef, obs);
        }
        if ([AOUtil updateNotifications:true forObserver:obs withAppRef:appRef withReferenceObj:nullptr withNotifications:kAXApplicationShownNotification, kAXApplicationHiddenNotification, kAXApplicationActivatedNotification,
             kAXApplicationDeactivatedNotification, kAXWindowMiniaturizedNotification, kAXWindowDeminiaturizedNotification, kAXWindowMovedNotification,
             kAXWindowResizedNotification, kAXUIElementDestroyedNotification, NULL]) {

            CFRunLoopAddSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);
            NSNumber* pidRef = @(pid);
            NSLog(@"Notifications added for %@ because it was launched", pidRef);
            CGWindowID windowId = [AOUtil appFocusedWindow:pid];
            if(trackedWindows[pidRef] == nil) {
                [AOTrackedEvent IterateEvents:^BOOL(AOTrackedEvent* e) {
                    return e.type == WindowEventType::Show && e.window == 0;
                } andCallback:[windowId, pid](Napi::Env env, Napi::Function callback) {
                    NSLog(@"mac: Notified alt1 of new RS instance winid[%d] pid[%d]", windowId, pid);
                    callback.Call({Napi::BigInt::New(env, (uint64_t)windowId), Napi::Number::New(env, 0)});
                }];
            }
        }
    } else {
        NSLog(@"Already tracking %@", @(pid));
    }
}

+ (void) handleRsDidTerminate:(pid_t)pid {
    if(observers.find(pid) != observers.end()) {
        pair<AXUIElementRef, AXObserverRef> p = observers[pid];
        AXUIElementRef appRef = p.first;
        AXObserverRef obs = p.second;
        if ([AOUtil updateNotifications:false forObserver:obs withAppRef:appRef withReferenceObj:NULL withNotifications:kAXApplicationShownNotification, kAXApplicationHiddenNotification, kAXApplicationActivatedNotification,
             kAXApplicationDeactivatedNotification, kAXWindowMiniaturizedNotification, kAXWindowDeminiaturizedNotification, kAXWindowMovedNotification,
             kAXWindowResizedNotification, kAXUIElementDestroyedNotification, NULL]) {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);
            NSLog(@"Notifications removed for %@ because it was terminated", @(pid));
        }
        observers.erase(pid);
        _rsPid = 0;
    }
}

+ (void) handleAXActivate: (pid_t) pid {
    NSArray<NSView*> *allViews = trackedViews[@(pid)];
    for(NSView *cview in allViews) {
        NSWindow *window = [cview window];
        [AOUtil updateWindow:window];
    }
}

+ (void) handleAXDeactivate: (pid_t) pid {
    NSArray<NSView*> *allViews = trackedViews[@(pid)];
    for(NSView *cview in allViews) {
        NSWindow *window = [cview window];
        [AOUtil updateWindow:window];
    }
}

+ (void) handleAXMiniaturize: (pid_t) pid {
    NSArray<NSView*> *allViews = trackedViews[@(pid)];
    for(NSView *cview in allViews) {
        NSLog(@"View:%@", cview);
        NSWindow *window = [cview window];
        [window setIsMiniaturized:true];
    }

}

+ (void) handleAXDeminiaturize: (pid_t) pid {
    NSArray<NSView*> *allViews = trackedViews[@(pid)];
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
    pid_t pid =  [AOUtil pidForWindow:window];
    NSLog(@"AOPID: %@ Window: %@ Pid: %@", @([[NSRunningApplication currentApplication] processIdentifier]), @(window), @(pid));
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
    if (parent.handle.winid == 0) {
        NSLog(@"macOSSetWindowParent:remove: %lu => %lu", winnum, parent.handle.winid);
        NSNumber *pidRef = trackedParents[winIdRef];
        if(trackedViews[pidRef] != nil) {
            NSMutableArray* views = trackedViews[pidRef];
            [views removeObject:view];
        }
        [trackedParents removeObjectForKey:winIdRef];
    } else {
        NSLog(@"macOSSetWindowParent:add: %lu => %lu", winnum, parent.handle.winid);
        pid_t pid = [AOUtil pidForWindow: parent.handle.winid];
        if(_rsPid == 0) {
            _rsPid = pid;
        }
        NSNumber *pidRef = @(pid);
        NSNumber *pwinIdRef = @(parent.handle.winid);
        trackedWindows[pidRef] = pwinIdRef;

        if(trackedViews[pidRef] != nil) {
            [trackedViews[pidRef] addObject:view];
        } else {
            NSMutableArray* views = [NSMutableArray arrayWithObject:view];
            trackedViews[pidRef] = views;
        }
        trackedParents[winIdRef] = pidRef;
    }
}

+ (void) updateWindow:(NSWindow*) window {
    [window invalidateShadow];
    if ([AOUtil shouldBeOnTop]) {
        [window setLevel:NSScreenSaverWindowLevel];
        [window makeKeyAndOrderFront:nil];
    } else {
        [window setLevel:NSNormalWindowLevel];
    }
}

+ (BOOL) shouldBeOnTop {
    return _frontPid == [[NSRunningApplication currentApplication] processIdentifier] || _frontPid == _rsPid;
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
    AXUIElementRef frontMostSys = AXUIElementCreateSystemWide();
    AXUIElementRef frontMostApp;
    if(kAXErrorSuccess == AXUIElementCopyAttributeValue(frontMostSys, kAXFocusedApplicationAttribute, (CFTypeRef*)&frontMostApp)) {
        pid_t pid;
        if(kAXErrorSuccess == AXUIElementGetPid(frontMostApp, &pid)) {
            return pid;
        }
    }
    
    return 0;
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

+ (CGWindowID) appMainWindow:(pid_t) pid {
    AXUIElementRef frontMostApp;
    AXUIElementRef frontMostWindow;

    // Get the frontMostApp
    frontMostApp = AXUIElementCreateApplication(pid);

    // Copy window attributes
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXMainWindowAttribute, (CFTypeRef*)&frontMostWindow);
    if (err != kAXErrorSuccess) {
        NSLog(@"no main window %@ %d", [NSValue valueWithPointer:frontMostApp], err);
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

+(void) capture:(OSWindow) wnd withRects: (vector <CaptureRect>) rects {
    CFDictionaryRef windowInfo = [AOUtil findWindow: wnd.handle.winid];
    if (windowInfo == nullptr) {
        printf("window nil - something changed!\n");
        return;
    }
    CGRect screenBounds;
    CGRectMakeWithDictionaryRepresentation((CFDictionaryRef) CFDictionaryGetValue(windowInfo, kCGWindowBounds), &screenBounds);
    CGWindowID windowId = static_cast<CGWindowID>(wnd.handle.winid);
    CGImageRef scaledImageRef = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowId, kCGWindowImageNominalResolution | kCGWindowImageBoundsIgnoreFraming);

    for (vector<CaptureRect>::iterator it = rects.begin(); it != rects.end(); ++it) {
        CGRect iscreenBounds = CGRectMake((CGFloat) it->rect.x, (CGFloat) it->rect.y, (CGFloat) it->rect.width, (CGFloat) it->rect.height);
        CGImageRef imageRef = [AOUtil redrawImage:CGImageCreateWithImageInRect(scaledImageRef, iscreenBounds)];

        if (![AOUtil drawImage: imageRef ontoBuffer:it->data withScale:1.0 ]) {
            fprintf(stderr, "error: could not copy image data\n");
        }
        CGImageRelease(imageRef);
    }
    CGImageRelease(scaledImageRef);
}

/*
 Redraws the input image returning the result and releasing the original image.
 */
+ (CGImageRef) redrawImage:(CGImageRef) image {
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    // redraw with sRGB
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
    CGImageRelease(image);
    return imgRef;
}

+ (BOOL) drawImage:(CGImageRef) image ontoBuffer: (void *)theData withScale: (CGFloat) scale {
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

+ (void) captureImageFile:(CGImageRef) imageRef withFilename: (NSString*)filename {
    bool b = [[NSFileManager defaultManager] fileExistsAtPath:filename];
    if(b) {
        return;
    }
    CFURLRef fileURL = static_cast<CFURLRef>([NSURL fileURLWithPath:filename]);
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
        if (err == kAXErrorSuccess) {
            NSLog(@"%@ %@ notification!", (add ? @"added" : @"removed"), current);
        } else {
//            NSLog(@"error %@ notification %@: %d", (add ? @"adding" : @"removing"), current, err);
//            va_end(args);
//            return false;
        }
        current = va_arg(args, CFStringRef);
    }
    va_end(args);
    return true;
}

@end

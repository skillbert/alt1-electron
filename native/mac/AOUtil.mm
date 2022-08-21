//
//  AOUtil.m
//  addon
//
//  Created by user on 8/19/22.
//

#import "AOUtil.h"

extern "C" AXError _AXUIElementGetWindow(AXUIElementRef, CGWindowID *out) __attribute__((weak_import));

bool mouseDown = false;
id localMonitor, globalMonitor;
std::mutex delegateMutex;
bool delegateSet = false;

static void updateWindowLevel(NSWindow *window) {
    if ([AOUtil isRsWindowActive]) {
        [window setLevel:NSScreenSaverWindowLevel];
        NSLog(@"Top: %@ %@", window, [window delegate]);
    } else {
        [window setLevel:NSNormalWindowLevel];
        NSLog(@"Normal: %@ %@", window, [window delegate]);
    }
}

static void createMonitors() {
    auto block = ^(NSEvent *evt) {
        if (evt.type == NSEventTypeLeftMouseDown) {
            mouseDown = true;
        } else if (evt.type == NSEventTypeLeftMouseUp) {
            mouseDown = false;
        }
    };
    auto localblock = ^NSEvent *(NSEvent *evt) {
        if (evt.type == NSEventTypeLeftMouseDown) {
            mouseDown = true;
        } else if (evt.type == NSEventTypeLeftMouseUp) {
            mouseDown = false;
        }
        return evt;
    };
    globalMonitor = [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskAppKitDefined handler:block];
    localMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskAppKitDefined handler:localblock];
}
typedef struct TrackedEvent {
    CGWindowID window;
    WindowEventType type;
    Napi::ThreadSafeFunction callback;
    Napi::FunctionReference callbackRef;

    TrackedEvent(CGWindowID window, WindowEventType type, Napi::Function callback) : window(window), type(type),
                                                                                     callback(Napi::ThreadSafeFunction::New(callback.Env(), callback, "event", 0, 1, [](Napi::Env) {})),
                                                                                     callbackRef(Napi::Persistent(callback)) {}
} TrackedEvent;

std::vector <TrackedEvent> trackedEvents;
std::mutex eventMutex; // Locks the trackedEvents vector

template<typename F, typename COND>
static void IterateEvents(COND cond, F callback) {
    dispatch_async(dispatch_get_main_queue(), ^{
        eventMutex.lock();
        for (auto it = trackedEvents.begin(); it != trackedEvents.end(); it++) {
            if (cond(*it)) {
                it->callback.BlockingCall([callback](Napi::Env env, Napi::Function jsCallback) {
                    callback(env, jsCallback);
                });
            }
        }
        eventMutex.unlock();
    });
}

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

static void ax_callback(AXObserverRef observer, AXUIElementRef element, CFStringRef notification, void *refcon) {
    NSLog(@"==%@==", notification);
    pid_t pid;
    NSView *view = ((NSView *) refcon);
    NSWindow *window = [view window];
    AXError err = AXUIElementGetPid(element, &pid);
    if (err != kAXErrorSuccess) {
        NSLog(@"no pid: %@", @(err));
        return;
    }
    CGRect winRect = [AOUtil appBounds:pid];
    NSLog(@"RS: %d (%0.0f,%0.0f) [%0.0fx%0.0f]", pid, winRect.origin.x, winRect.origin.y, winRect.size.width, winRect.size.height);
    if (str_eq(notification, kAXApplicationActivatedNotification)) {
        updateWindowLevel(window);
    } else if (str_eq(notification, kAXApplicationDeactivatedNotification)) {
        updateWindowLevel(window);
    } else if (str_eq(notification, kAXWindowMiniaturizedNotification)) {
        [window setIsVisible:false];
    } else if (str_eq(notification, kAXWindowDeminiaturizedNotification)) {
        [window setIsVisible:true];
        updateWindowLevel(window);
    } else if (str_eq(notification, kAXWindowMovedNotification)) {
        CGWindowID rsWinId = [AOUtil appFocusedWindow:pid];
        JSRectangle bounds = JSRectangle(static_cast<int>(winRect.origin.x), static_cast<int>(winRect.origin.y), static_cast<int>(winRect.size.width), static_cast<int>(winRect.size.height));
        IterateEvents([rsWinId](const TrackedEvent &e) {
            return e.type == WindowEventType::Move && e.window == rsWinId;
        }, [bounds](Napi::Env env, Napi::Function callback) {
            callback.Call({bounds.ToJs(env), Napi::String::New(env, "end")});
        });
    } else if (str_eq(notification, kAXWindowResizedNotification)) {
        CGWindowID rsWinId = [AOUtil appFocusedWindow:pid];
        JSRectangle bounds = JSRectangle(static_cast<int>(winRect.origin.x), static_cast<int>(winRect.origin.y), static_cast<int>(winRect.size.width), static_cast<int>(winRect.size.height));
        IterateEvents([rsWinId](const TrackedEvent &e) {
            return e.type == WindowEventType::Move && e.window == rsWinId;
        }, [bounds](Napi::Env env, Napi::Function callback) {
            callback.Call({bounds.ToJs(env), Napi::String::New(env, "end")});
        });
    }
}
const CFMutableDictionaryRef winIdPidMap = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
const CFMutableDictionaryRef pidAppRefMap = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
const CFMutableDictionaryRef winIdObsMap = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
const CFMutableDictionaryRef trackedWindows = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

@implementation AOUtil
+(void) initialize {
    dispatch_once_t once;
    dispatch_once(&once, ^{
        createMonitors();
    });
}

+(BOOL) macOSGetMouseState {
    return mouseDown;
}

+ (void) macOSNewWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback {
    NSLog(@"PID: %d", [[NSRunningApplication currentApplication] processIdentifier]);
    ax_privilege();
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
    CGRequestScreenCaptureAccess();
#endif
    NSLog(@"macOSNewWindowListener: %u %d", window, type);
    auto event = TrackedEvent(window, type, callback);
    eventMutex.lock();
    trackedEvents.push_back(std::move(event));
    eventMutex.unlock();
}

+ (void) macOSRemoveWindowListener:(CGWindowID) window type: (WindowEventType) type callback: (Napi::Function) callback {
    NSLog(@"macOSRemoveWindowListener: %d", window);
    eventMutex.lock();
    trackedEvents.erase(std::remove_if(trackedEvents.begin(), trackedEvents.end(), [window, type, callback](TrackedEvent &e) {
        if ((e.window == window) && (e.type == type) && (Napi::Persistent(callback) == e.callbackRef)) {
            e.callback.Release();
            return true;
        }
        return false;
    }), trackedEvents.end());
    eventMutex.unlock();
}
+(BOOL) isRsWindowActive {
   NSRunningApplication *frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
   NSRunningApplication *currentApp = [NSRunningApplication currentApplication];
   NSString *title = [[frontApp localizedName] lowercaseString];
   NSLog(@"F: %d, C: %d, t: %@, one: %@ two: %@", [frontApp processIdentifier], [currentApp processIdentifier], title, [currentApp isEqualTo:frontApp] ? @"YES" : @"NO",
         [title isCaseInsensitiveLike:@"rs2client"] ? @"YES" : @"NO");
   return [currentApp isEqualTo:frontApp] || [title isCaseInsensitiveLike:@"rs2client"];
}

+(pid_t) focusedPid {
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
        return NULL;
    }
    CFDictionaryRef entry = (CFDictionaryRef) CFArrayGetValueAtIndex(windowList, 0);
    return entry;
}


+(CGWindowID) appFocusedWindow:(pid_t) pid {
    AXUIElementRef frontMostApp;
    AXUIElementRef frontMostWindow;

    // Get the frontMostApp
    frontMostApp = AXUIElementCreateApplication(pid);

    // Copy window attributes
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXFocusedWindowAttribute, (CFTypeRef *)&frontMostWindow);
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
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXFocusedWindowAttribute, (CFTypeRef *)&frontMostWindow);
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
    err = AXUIElementCopyAttributeValue(frontMostWindow, kAXPositionAttribute, (CFTypeRef *)&temp);
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
    AXError err = AXUIElementCopyAttributeValue(frontMostApp, kAXFocusedWindowAttribute, (CFTypeRef *)&frontMostWindow);
    if (err != kAXErrorSuccess) {
        NSLog(@"no focused window %@ %d", [NSValue valueWithPointer:frontMostApp], err);
        CFRelease(frontMostApp);
        return @"";
    }
    err = AXUIElementCopyAttributeValue(frontMostWindow, kAXTitleAttribute, (CFTypeRef *)&frontMostWindowTitle);
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
    CGFloat scale = [AOUtil findScalingFactor:[AOUtil findScreenForRect:screenBounds]];
    CGWindowID windowId = static_cast<CGWindowID>(wnd.handle.winid);
    CGImageRef scaledImageRef = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowId, kCGWindowImageBestResolution);
    for (vector<CaptureRect>::iterator it = rects.begin(); it != rects.end(); ++it) {
        CGRect iscreenBounds = CGRectMake((CGFloat) it->rect.x * scale, (CGFloat) it->rect.y * scale, (CGFloat) it->rect.width * scale, (CGFloat) it->rect.height * scale);
        CGImageRef imageRef = CGImageCreateWithImageInRect(scaledImageRef, iscreenBounds);
        NSLog(@"C: (%d,%d) [%dx%d] ?? (%zu vs %d)", it->rect.x, it->rect.y, it->rect.width, it->rect.height, it->size, 4 * it->rect.width * it->rect.height);

        if (![AOUtil CGImageResizeGetBytesByScale: imageRef withScale:scale andData:it->data]) {
            fprintf(stderr, "error: could not copy image data\n");
        }
        CGImageRelease(imageRef);
    }
    CGImageRelease(scaledImageRef);
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

+ (BOOL) CGImageResizeGetBytesByScale:(CGImageRef) image withScale: (CGFloat) scale andData: (void *)theData {
    // calculate size
    size_t width = CGImageGetWidth(image) / static_cast<size_t>(scale);
    size_t height = CGImageGetHeight(image) / static_cast<size_t>(scale);
    // resize
    CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(theData, width, height, 8, width * 4, colorspace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorspace);
    if (context == NULL) return false;
    // draw image to context (resizing it)
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    // extract resulting image from context
    CGContextRelease(context);
    return true;
}

+ (void) interceptDelegate:(NSWindow *)window {
    if ([window delegate] == nil || [[[window delegate] class] isEqualTo:[AONSWindowDelegate class]] ) {
        return;
    }
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

+ (void) macOSSetParent:(OSWindow) parent forWindow: (OSWindow) wnd {
    NSView *view = wnd.handle.view;
    [AOUtil interceptDelegate:[view window]];
    NSInteger winnum = [[view window] windowNumber];
    CFNumberRef winIdRef = CFNumberCreate(NULL, kCFNumberLongType, &winnum);
    if (parent.handle.winid != 0) {
        pid_t rsPid = [AOUtil pidForWindow: parent.handle.winid];
        NSLog(@"RSPID: %d", rsPid);
    }
    NSLog(@"macOSSetWindowParent: %lu => %lu", winnum, parent.handle.winid);
    NSLog(@"%@ %@ movable, %@ resizable", winIdRef, [view mouseDownCanMoveWindow] ? @"IS" : @"IS NOT", [[view window] isResizable] ? @"IS" : @"IS NOT");
    AXUIElementRef appRef = NULL;
    if (parent.handle.winid == 0) {
        CFNumberRef pidRef = (CFNumberRef) CFDictionaryGetValue(winIdPidMap, winIdRef);
        AXObserverRef obs;
        if (CFDictionaryContainsKey(winIdObsMap, winIdRef)) {
            obs = (AXObserverRef) CFDictionaryGetValue(winIdObsMap, winIdRef);
        } else {
            NSLog(@"Unable to find observer for %@", winIdRef);
            return;
        }
        if (CFDictionaryContainsKey(pidAppRefMap, pidRef)) {
            appRef = (AXUIElementRef) CFDictionaryGetValue(pidAppRefMap, pidRef);
        } else {
            NSLog(@"Unable to find appref for %@", pidRef);
            return;
        }
        if (CFDictionaryContainsKey(winIdObsMap, winIdRef)) {
            CFDictionaryRemoveValue(winIdObsMap, winIdRef);
            if (!CFDictionaryContainsValue(winIdObsMap, obs)) {
                
                if ([AOUtil updateNotifications:false forObserver:obs withAppRef:appRef withReferenceObj:NULL withNotifications:kAXApplicationShownNotification, kAXApplicationHiddenNotification, kAXApplicationActivatedNotification,
                     kAXApplicationDeactivatedNotification, kAXWindowMiniaturizedNotification, kAXWindowDeminiaturizedNotification, kAXWindowMovedNotification,
                     kAXWindowResizedNotification, NULL]) {
                    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);
                    NSLog(@"Notifications removed for %@ because no more window ids exist for it", pidRef);
                }
            }
        }
    } else {
        pid_t pid = [AOUtil pidForWindow: parent.handle.winid];
        CFNumberRef pidRef = CFNumberCreate(NULL, kCFNumberIntType, &pid);
        CFDictionarySetValue(winIdPidMap, winIdRef, pidRef);
        if (CFDictionaryContainsKey(pidAppRefMap, pidRef)) {
            appRef = (AXUIElementRef) CFDictionaryGetValue(pidAppRefMap, pidRef);
        } else {
            appRef = AXUIElementCreateApplication(pid);
        }
        CFDictionarySetValue(pidAppRefMap, pidRef, appRef);
        AXError err;
        AXObserverRef obs;
        err = AXObserverCreate(pid, (AXObserverCallback) & ax_callback, &obs);
        if (err != kAXErrorSuccess) {
            NSLog(@"Error creating observer: %d", err);
            return;
        } else {
            NSLog(@"Setting observer for %@: %@", winIdRef, obs);
            CFDictionarySetValue(winIdObsMap, winIdRef, obs);
        }
        
        if ([AOUtil updateNotifications:true forObserver:obs withAppRef:appRef withReferenceObj:wnd.handle.view withNotifications:kAXApplicationShownNotification, kAXApplicationHiddenNotification, kAXApplicationActivatedNotification,
             kAXApplicationDeactivatedNotification, kAXWindowMiniaturizedNotification, kAXWindowDeminiaturizedNotification, kAXWindowMovedNotification,
             kAXWindowResizedNotification, NULL]) {
            CFRunLoopAddSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);
        }
    }
}


@end

#import <cstring>
#import <iostream>
#import <vector>
#import "os.h"
#import "mac/AOUtil.h"

typedef struct filterData {
    vector <OSWindow> wins;
    CGWindowID windowId;
    CFStringRef windowName;
    CFStringRef prefix;
} filterData;

JSRectangle OSWindow::GetBounds() {
    CFDictionaryRef windowInfo = [AOUtil findWindow: this->handle.winid];
    

    CGRect screenBounds;
    if(windowInfo == nullptr) {
        screenBounds = [[NSScreen screens][0] frame];
    } else {
        CGRectMakeWithDictionaryRepresentation((CFDictionaryRef) CFDictionaryGetValue(windowInfo, kCGWindowBounds), &screenBounds);
    }
    JSRectangle jbounds(static_cast<int>(screenBounds.origin.x), static_cast<int>(screenBounds.origin.y), static_cast<int>(screenBounds.size.width), static_cast<int>(screenBounds.size.height));
    return jbounds;
}

JSRectangle OSWindow::GetClientBounds() {
    CFDictionaryRef windowInfo = [AOUtil findWindow: this->handle.winid];
    CGRect screenBounds;
    if(windowInfo == nullptr) {
        screenBounds = [[NSScreen screens][0] frame];
    } else {
        CGRectMakeWithDictionaryRepresentation((CFDictionaryRef) CFDictionaryGetValue(windowInfo, kCGWindowBounds), &screenBounds);
    }
    BOOL isFs = [AOUtil isFullScreen: screenBounds];
    JSRectangle jbounds(static_cast<int>(screenBounds.origin.x), static_cast<int>(screenBounds.origin.y), static_cast<int>(screenBounds.size.width), static_cast<int>(screenBounds.size.height));
    if(!isFs) {
        jbounds.y += TITLE_BAR_HEIGHT;
        jbounds.height = jbounds.height - TITLE_BAR_HEIGHT;
    }
    return jbounds;
}

float OSWindow::OSGetScale() {
    CFDictionaryRef windowInfo = [AOUtil findWindow: this->handle.winid];
    if (windowInfo == nullptr) {
        printf("window nil - something changed!\n");
        return 1.0;
    }
    CGRect screenBounds;
    CGRectMakeWithDictionaryRepresentation((CFDictionaryRef) CFDictionaryGetValue(windowInfo, kCGWindowBounds), &screenBounds);
    return static_cast<float>([AOUtil findScalingFactor:[AOUtil findScreenForRect:screenBounds]]);
}

void filterWindows(const void *inputDictionary, void *context) {
    if (context == NULL) {
        return;
    }
    CFDictionaryRef entry = (CFDictionaryRef) inputDictionary;
    filterData *data = (filterData *) context;
    if (data->windowId != 0) {
        CFNumberRef value = (CFNumberRef) CFDictionaryGetValue(entry, kCGWindowNumber);
        CGWindowID windowId;
        CFNumberGetValue(value, kCFNumberIntType, &windowId);
        if (data->windowId == windowId) {
            data->wins.push_back(OSWindow(OSRawWindow{.winid=windowId}));
        }
    } else {
        // Grab the window name, but since it's optional we need to check before we can use it.
        CFStringRef windowName = (CFStringRef) CFDictionaryGetValue(entry, kCGWindowName);
        if(CFStringGetLength(data->windowName) > 0) {
            CFIndex windowNameLen = windowName == NULL ? 0 : CFStringGetLength(windowName);
            if (windowNameLen == 0 || kCFCompareEqualTo != CFStringCompare(windowName, data->windowName, kCFCompareCaseInsensitive)) {
                return;
            }
        }
        // Grab the application name, but since it's optional we need to check before we can use it.
        CFStringRef applicationName = (CFStringRef) CFDictionaryGetValue(entry, kCGWindowOwnerName);
        if (applicationName != NULL && data->prefix != NULL) {
            bool hasPrefix = CFStringHasPrefix(applicationName, data->prefix);
            if (!hasPrefix) {
                return;
            }
        }
        CFNumberRef appPidRef = (CFNumberRef) CFDictionaryGetValue(entry, kCGWindowOwnerPID);
        pid_t pid;
        CFNumberGetValue(appPidRef, kCFNumberIntType, &pid);

        // Grab the Window Bounds, it's a dictionary in the array, but we want to display it as a string
        CFNumberRef alphaValueRef = (CFNumberRef) CFDictionaryGetValue(entry, kCGWindowAlpha);
        CGFloat alpha;
        CFNumberGetValue(alphaValueRef, kCFNumberCGFloatType, &alpha);

        // Grab the Window ID
        CFNumberRef value = (CFNumberRef) CFDictionaryGetValue(entry, kCGWindowNumber);
        CGWindowID windowId;
        CFNumberGetValue(value, kCFNumberIntType, &windowId);
        if (alpha > 0) {
            data->wins.push_back(OSWindow(OSRawWindow{.winid=windowId}));
        }
    }
}

vector <OSWindow> OSGetRsHandles() {
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);

    filterData *windowFilterData = new filterData;
    windowFilterData->wins = vector<OSWindow>();
    windowFilterData->windowId = 0;
    windowFilterData->windowName = CFSTR("runescape");
    windowFilterData->prefix = CFSTR("rs2client");

    CFArrayApplyFunction(windowList, CFRangeMake(0, CFArrayGetCount(windowList)), &filterWindows, (void *) (windowFilterData));
    return windowFilterData->wins;
}

OSWindow OSGetActiveWindow() {
    pid_t pid = [AOUtil focusedPid];
    CGWindowID windowId = [AOUtil appFocusedWindow:pid];
    return OSWindow(OSRawWindow{.winid=windowId});
}

bool OSWindow::IsValid() {
    return this->handle.winid != 0;
}

std::string OSWindow::GetTitle() {
    auto windowId = static_cast<CGWindowID>(this->handle.winid);
    NSString *title = [AOUtil appTitle:[AOUtil pidForWindow:windowId]];
    std::string stdtitle;
    if([title length] == 0) {
        return stdtitle;
    }
    stdtitle = std::string([title cStringUsingEncoding:kCFStringEncodingUTF8], [title length]);
    return stdtitle;
}

Napi::Value OSWindow::ToJS(Napi::Env env) {
    return Napi::BigInt::New(env, static_cast<uint64_t>(this->handle.winid));
}

bool OSWindow::operator==(const OSWindow &other) const {
    return memcmp(&this->handle, &other.handle, sizeof(this->handle)) == 0;
}

bool OSWindow::operator<(const OSWindow &other) const {
    return memcmp(&this->handle, &other.handle, sizeof(this->handle)) < 0;
}

OSWindow OSWindow::FromJsValue(const Napi::Value jsval) {
    auto handle = jsval.As<Napi::BigInt>();
    bool lossless;
    uint64_t handleint = handle.Uint64Value(&lossless);
    if (!lossless) {
        Napi::RangeError::New(jsval.Env(), "Invalid handle").ThrowAsJavaScriptException();
    }
    return OSWindow(OSRawWindow{.winid = handleint});
}

void OSSetWindowParent(OSWindow wnd, OSWindow parent) {
    [AOUtil macOSSetParent:parent forWindow:wnd];
}

/**
 * Defines which region of a window can be clicked
 * Implemented only on X11 Linux as a replacement for electron's setIgnoreMouseEvents()
 */
void OSSetWindowShape(__attribute__((unused)) OSWindow wnd, __attribute__((unused)) vector <JSRectangle> rects) {
    //No op on macos
}

/**
 * Returns true when the left/main mouse button is down, even in another process and regardless of message pump state
 */
bool OSGetMouseState() {
    return [AOUtil macOSGetMouseState];
}

void OSCaptureMulti(OSWindow wnd, __attribute__((unused)) CaptureMode mode, vector <CaptureRect> rects, __attribute__((unused)) Napi::Env env) {
    [AOUtil OSCaptureWindowMulti: wnd withRects:rects];
}

void OSNewWindowListener(OSWindow wnd, WindowEventType type, Napi::Function callback) {
    NSLog(@"mac: OSNewWindowListener: wnd:%lu, type:%u", wnd.handle.winid, (uint32_t) type);
    [AOUtil macOSNewWindowListener:(CGWindowID)wnd.handle.winid type:type callback:callback];
}

void OSRemoveWindowListener(OSWindow wnd, WindowEventType type, Napi::Function callback) {
    NSLog(@"mac: OSRemoveWindowListener: wnd:%lu, type:%u", wnd.handle.winid, (uint32_t) type);
    [AOUtil macOSRemoveWindowListener:(CGWindowID)wnd.handle.winid type:type callback:callback];
}

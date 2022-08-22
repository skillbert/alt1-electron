//
//  AONSWindowDelegate.m
//  addon
//
//  Created by user on 8/19/22.
//
#import "AONSWindowDelegate.h"

@implementation AONSWindowDelegate
@synthesize delegate;

- (instancetype)initWithDelegate:(id<NSWindowDelegate>)otherDelegate {
    self = [super init];
    if(self != nil) {
        self.delegate = otherDelegate;
    }
    return self;
}

- (id)forwardingTargetForSelector:(SEL)aSelector {
    return delegate;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    SEL aSelector = [invocation selector];
    NSLog(@"Forwarding %@", invocation);
    if ([delegate respondsToSelector:aSelector])
        [invocation invokeWithTarget:delegate];
    else
        [super forwardInvocation:invocation];
}

- (void)adjustLevel:(NSWindow *)window {
    NSLog(@"CollectionBehavior: %@", @([window collectionBehavior]));
    if ([AOUtil isRsWindowActive]) {
        [window setLevel:NSScreenSaverWindowLevel];
        [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary];
        [window makeKeyAndOrderFront:nil];
    } else {
        [window setLevel:NSNormalWindowLevel];
    }
}
#pragma mark NSWindowDelegate protocol

- (void)windowDidChangeOcclusionState:(NSNotification*)notification {
    [delegate windowDidChangeOcclusionState:notification];
}

// Called when the user clicks the zoom button or selects it from the Window
// menu to determine the "standard size" of the window.
- (NSRect)windowWillUseStandardFrame:(NSWindow*)window
                        defaultFrame:(NSRect)frame {

    return [delegate windowWillUseStandardFrame:window defaultFrame: frame];
}

- (void)windowDidBecomeMain:(NSNotification *)notification {
    [delegate windowDidBecomeMain:notification];
    [self adjustLevel:notification.object];
}

- (void)windowDidResignMain:(NSNotification *)notification {
    [delegate windowDidResignMain:notification];
    [self adjustLevel:notification.object];
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    [delegate windowDidBecomeKey:notification];
    [self adjustLevel:notification.object];
}

- (void)windowDidResignKey:(NSNotification*)notification {
    [delegate windowDidResignKey:notification];
    [self adjustLevel:notification.object];
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)frameSize {
    return [delegate windowWillResize:sender toSize:frameSize];
}

- (void)windowDidResize:(NSNotification*)notification {
    [delegate windowDidResize:notification];
}

- (void)windowWillMove:(NSNotification*)notification {
    [delegate windowWillMove:notification];
}

- (void)windowDidMove:(NSNotification*)notification {
    [delegate windowDidMove:notification];
}

- (void)windowWillMiniaturize:(NSNotification*)notification {
    [delegate windowWillMiniaturize:notification];
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    [delegate windowDidMiniaturize:notification];
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    [delegate windowDidDeminiaturize:notification];
}

- (BOOL)windowShouldZoom:(NSWindow*)window toFrame:(NSRect)newFrame {
    return [delegate windowShouldZoom: window toFrame:newFrame];
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
    [delegate windowDidEndLiveResize:notification];
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
    [delegate windowWillEnterFullScreen:notification];
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
    [delegate windowDidEnterFullScreen:notification];
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
    [delegate windowWillExitFullScreen:notification];
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
    [delegate windowDidExitFullScreen:notification];
}

- (void)windowWillClose:(NSNotification*)notification {
    [delegate windowWillClose:notification];
}

- (BOOL)windowShouldClose:(id)window {
    return [delegate windowShouldClose:window];
}

- (NSRect)window:(NSWindow*)window
willPositionSheet:(NSWindow*)sheet
        usingRect:(NSRect)rect {
    return [delegate window:window willPositionSheet:sheet usingRect:rect];
}

- (void)windowWillBeginSheet:(NSNotification*)notification {
    [delegate windowWillBeginSheet:notification];
}

- (void)windowDidEndSheet:(NSNotification*)notification {
    [delegate windowDidEndSheet:notification];
}

@end

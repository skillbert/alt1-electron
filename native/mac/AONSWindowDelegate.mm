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

#pragma mark NSWindowDelegate protocol

- (void)adjustLevel:(NSWindow *)window {
    if ([AOUtil isRsWindowActive]) {
        [window setLevel:NSScreenSaverWindowLevel];
    } else {
        [window setLevel:NSNormalWindowLevel];
    }
}

- (void)windowDidBecomeMain:(NSNotification *)notification {
    [delegate windowDidBecomeMain:notification];
    [self adjustLevel:notification.object];
}

- (void)windowDidResignMain:(NSNotification *)notification {
    [delegate windowDidResignMain:notification];
    [self adjustLevel:notification.object];
}


@end

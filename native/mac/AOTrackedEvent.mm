//
//  AOTrackedEvent.m
//  addon
//
//  Created by user on 8/22/22.
//

#import "AOTrackedEvent.h"
@interface AOTrackedEvent()
+ (NSLock*) eventLock;
+ (void) pushEvent: (AOTrackedEvent*)event;
+ (NSMutableSet *)events;
@end

@implementation AOTrackedEvent {
    CGWindowID window;
    WindowEventType type;
    Napi::ThreadSafeFunction callback;
    Napi::FunctionReference callbackRef;
}

#pragma mark - Private Category functions

+ (NSLock*) eventLock {
    static NSLock *_eventLock = nil;
    if (_eventLock == nil) {
        _eventLock = [NSLock new];
    }
    return _eventLock;
}

+ (NSMutableSet*) events {
    static NSMutableSet<AOTrackedEvent*> *_events = nil;
    if(_events == nil) {
        _events = [NSMutableSet new];
    }
    return _events;
}

#pragma mark - Public Category functions

+ (void) IterateEvents: (TrackedEventCondition) condition andCallback:(std::function<void(Napi::Env, Napi::Function)>) cb {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLock *lock = [AOTrackedEvent eventLock];
        @try {
            [lock tryLock];
            NSSet<AOTrackedEvent*> *events = [AOTrackedEvent events];
            for(AOTrackedEvent *event in events) {
                if(condition(event)) {
                    event->callback.BlockingCall([cb](Napi::Env env, Napi::Function jsCallback) {
                        cb(env, jsCallback);
                    });
                }
            }
        } @finally {
            [lock unlock];
        }
    });
}

+ (void) pushEvent: (AOTrackedEvent*)event {
    NSLock *lock = [AOTrackedEvent eventLock];
    [lock tryLock];
    NSMutableSet<AOTrackedEvent*> *events = [AOTrackedEvent events];
    NSLog(@"push: Events Before: %@ [%@]", @([events count]), events);
    if([events containsObject:event]) {
        NSLog(@"Unable to add %@ as it already exists!", event);
        [lock unlock];
        return;
    }
    [events addObject:event];
    NSLog(@"push: Events After: %@ [%@]", @([events count]), events);
    [lock unlock];
}

+ (void) push: (CGWindowID) window andType: (WindowEventType) type andCallback:(Napi::Function) callback {
    [AOTrackedEvent pushEvent: [[AOTrackedEvent alloc] initWith:window andType:type andCallback:callback]];
}

+ (void) remove: (CGWindowID) window andType: (WindowEventType) type andCallback:(Napi::Function) callback {
    NSLock *lock = [AOTrackedEvent eventLock];
    [lock tryLock];
    NSMutableSet<AOTrackedEvent*> *events = [AOTrackedEvent events];
    NSLog(@"remove: Events Before: %@ [%@]", @([events count]), events);
    NSArray<AOTrackedEvent*> *ievents = [events allObjects];
    for(AOTrackedEvent *event in ievents) {
        if(event->window == window && event->type == type && event->callbackRef == Napi::Persistent(callback)) {
            event->callback.Release();
            [events removeObject:event];
        }
    }
    NSLog(@"remove: Events After: %@ [%@]", @([events count]), events);
    [lock unlock];
}

- (CGWindowID) window {
    return self->window;
}

- (WindowEventType) type {
    return self->type;
}

- (NSString *) typeName {
    switch(self->type) {
        case WindowEventType::Show:
            return @"Show";
        case WindowEventType::Click:
            return @"Click";
        case WindowEventType::Close:
            return @"Close";
        case WindowEventType::Move:
            return @"Move";
    }
    return @"";
}

- (NSString *) description {
    return [self debugDescription];
}

- (NSString *) debugDescription {
    return [NSString stringWithFormat:@"Event[%@, %d]", [self typeName], self->window];
}

- (instancetype) initWith: (CGWindowID) window andType: (WindowEventType) type andCallback:(Napi::Function) callback {
    self = [super init];
    if (self != nil) {
        self->window = window;
        self->type = type;
        self->callback = Napi::ThreadSafeFunction::New(callback.Env(), callback, "event", 0, 1, [](Napi::Env) {});
        self->callbackRef = Napi::Persistent(callback);
    }
    return self;
}

- (BOOL) isEqualTo:(id)object {
    return [self isEqual:object];
}

- (BOOL) isEqual:(id)object {
    if (object == nil) {
        return NO;
    }
    if (![[object class] isEqual:[AOTrackedEvent class]]) {
        return NO;
    }
    AOTrackedEvent *other = (AOTrackedEvent*)object;
    if(other->window == self->window && other->type == self->type) {
        if(other->callbackRef == self->callbackRef) {
            NSLog(@"cb eq");
            return YES;
        }
        return NO;
    }
    if(other->window != self->window || other->type != self->type || !(other->callbackRef == self->callbackRef)) {
        return NO;
    }
    return YES;
}

@end

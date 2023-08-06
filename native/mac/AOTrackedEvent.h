//
//  AOTrackedEvent.h
//  addon
//
//  Created by user on 8/22/22.
//

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include "../os.h"


NS_ASSUME_NONNULL_BEGIN
@class AOTrackedEvent;

typedef BOOL (^TrackedEventCondition)(AOTrackedEvent*);
@interface AOTrackedEvent : NSObject {
    
}
+ (NSString *) typeName: (WindowEventType) type;
+ (void) IterateEvents:(TrackedEventCondition) condition andCallback:(std::function<void(Napi::Env, Napi::Function)>) cb;
+ (void) push: (CGWindowID) window andType: (WindowEventType) type andCallback:(Napi::Function) callback;
+ (void) remove: (CGWindowID) window andType: (WindowEventType) type andCallback:(Napi::Function) callback;
- (CGWindowID) window;
- (WindowEventType) type;
- (instancetype) initWith: (CGWindowID) window andType: (WindowEventType) type andCallback:(Napi::Function) callback;

@end

NS_ASSUME_NONNULL_END

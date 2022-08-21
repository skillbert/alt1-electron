//
//  AONSWindowDelegate.h
//  addon
//
//  Created by user on 8/19/22.
//

#import "AOUtil.h"
#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AONSWindowDelegate : NSObject <NSWindowDelegate>
@property(assign, nonatomic) id <NSWindowDelegate> delegate;
- (instancetype) initWithDelegate:(id<NSWindowDelegate>)otherDelegate;
@end

NS_ASSUME_NONNULL_END

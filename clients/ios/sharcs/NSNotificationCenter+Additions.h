//
//  NSObject+NSNotificationCenterAdditions.h
//  sharcs
//
//  Created by Martin Kleinhans on 02.02.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSNotificationCenter (Additions)
- (void) postNotificationOnMainThread:(NSNotification *) notification;
- (void) postNotificationOnMainThread:(NSNotification *) notification waitUntilDone:(BOOL) wait;

- (void) postNotificationOnMainThreadWithName:(NSString *) name object:(id) object;
- (void) postNotificationOnMainThreadWithName:(NSString *) name object:(id) object userInfo:(NSDictionary *) userInfo;
- (void) postNotificationOnMainThreadWithName:(NSString *) name object:(id) object userInfo:(NSDictionary *) userInfo waitUntilDone:(BOOL) wait;
@end

@interface NSNotificationQueue (Additions)
- (void) enqueueNotificationOnMainThread:(NSNotification *) notification postingStyle:(NSPostingStyle) postingStyle;
- (void) enqueueNotificationOnMainThread:(NSNotification *) notification postingStyle:(NSPostingStyle) postingStyle coalesceMask:(unsigned) coalesceMask forModes:(NSArray *) modes;
@end
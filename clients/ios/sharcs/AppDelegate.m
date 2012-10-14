/*
 * Copyright (c) 2012 Martin Kleinhans <mail@mkleinhans.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#import "AppDelegate.h"
#import "NSNotificationCenter+Additions.h"

#include "libsharcs.h"

BOOL g_devicesAvailable = NO;
BOOL g_profilesAvailable = NO;

int callback_feature_i(sharcs_id feature, int value);
int callback_feature_s(sharcs_id feature, const char* value);
void callback_event(int event, int v1, int v2);

int callback_feature_i(sharcs_id feature, int value) {
    @autoreleasepool {
        [[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSFeatureI" 
                                                                            object:nil 
                                                                          userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                    [NSNumber numberWithInt:value], @"value",
                                                                                    [NSNumber numberWithInt:feature], @"feature",
                                                                                    nil]];
    }
    return 0;
}

int callback_feature_s(sharcs_id feature, const char* value) {
    @autoreleasepool {
        [[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSFeatureS" 
                                                                            object:nil 
                                                                          userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                    [NSString stringWithCString:value encoding:NSASCIIStringEncoding],@"value",
                                                                                    [NSNumber numberWithInt:feature], @"feature",
                                                                                    nil]];
    }
    return 0;
}

void callback_event(int event, int v1, int v2) {
	switch(event) {
		case LIBSHARCS_EVENT_RETRIEVE:
			g_devicesAvailable = YES;
			[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSRetrieve"
																				object:nil];
			break;
		case LIBSHARCS_EVENT_PROFILES:
			g_profilesAvailable = YES;
			[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSProfiles"
																				object:nil];
			break;
		case LIBSHARCS_EVENT_PROFILE_LOAD:
			[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSProfileLoad"
																				object:[NSDictionary dictionaryWithObjectsAndKeys:
																						[NSNumber numberWithInt:v1],@"id",
																						nil]];
			break;
		case LIBSHARCS_EVENT_PROFILE_DELETE:
			[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSProfileDelete"
																				object:[NSDictionary dictionaryWithObjectsAndKeys:
																						[NSNumber numberWithInt:v1],@"id",
																						nil]];
			break;
		case LIBSHARCS_EVENT_PROFILE_SAVE:
			[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSProfileSave"
																				object:[NSDictionary dictionaryWithObjectsAndKeys:
																						[NSNumber numberWithInt:v1],@"id",
																						nil]];
			break;
	}
}

@interface AppDelegate (Private)
- (void)connect;
- (void)disconnect;
@end
    
@implementation AppDelegate (Private)

- (void)connect {
    if(!server) {
        return;
    }
    
    sharcs_init([server cStringUsingEncoding:NSASCIIStringEncoding],&callback_feature_i,&callback_feature_s,&callback_event);
    sharcs_retrieve();
	sharcs_profiles();
}

- (void)disconnect {
    sharcs_stop();
}

@end

@implementation AppDelegate 
                 
@synthesize window = _window;

+ (AppDelegate*)shared {
    return (AppDelegate*)[UIApplication sharedApplication].delegate;
}

- (BOOL) devicesAvailable {
    return g_devicesAvailable;
}

- (BOOL) profilesAvailable {
    return g_profilesAvailable;
}

- (void)selectServer:(NSString*)s {
    server = s;
    
    [self connect];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    
    UIStoryboard *storyboard;
    UIViewController *vc;
    
    storyboard = [UIStoryboard storyboardWithName:@"MainStoryboard_iPhone" bundle:[NSBundle mainBundle]];
    vc = [storyboard instantiateInitialViewController];
    
    // create and configure window
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];  
    self.window.backgroundColor = [UIColor blackColor];  
	self.window.rootViewController = vc;
	
    [self.window addSubview:vc.view];
    [self.window makeKeyAndVisible];
    
    //if(![[NSUserDefaults standardUserDefaults] valueForKey:@"server"]) {
        [self.window.rootViewController presentModalViewController:[storyboard instantiateViewControllerWithIdentifier:@"ServerList"] animated:NO];
    //} else {
    //    [self connect];
    //    [self retrieve];
    //}
    
    return YES;
}
							
- (void)applicationWillResignActive:(UIApplication *)application {
    [self disconnect];
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    [self connect];
}

- (void)applicationWillTerminate:(UIApplication *)application {
    [self disconnect];
}

@end

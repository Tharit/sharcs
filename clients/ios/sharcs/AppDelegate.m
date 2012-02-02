//
//  AppDelegate.m
//  sharcs
//
//  Created by Martin Kleinhans on 02.02.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "AppDelegate.h"
#import "NSNotificationCenter+Additions.h"

#include "libsharcs.h"

BOOL g_devicesAvailable = NO;

int callback_feature_i(sharcs_id feature, int value);
int callback_feature_s(sharcs_id feature, const char* value);
void callback_retrieve(int success);

void callback_retrieve(int success) {
    g_devicesAvailable = YES;
    @autoreleasepool {
        [[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSRetrieve" 
                                                                            object:nil];
    }
}

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
        [[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:@"SHARCSRetrieve" 
                                                                            object:nil 
                                                                          userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                    [NSString stringWithCString:value encoding:NSASCIIStringEncoding],@"value",
                                                                                    [NSNumber numberWithInt:feature], @"feature",
                                                                                    nil]];
    }
    return 0;
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
    
    sharcs_init([server cStringUsingEncoding:NSASCIIStringEncoding],&callback_feature_i,&callback_feature_s);
    sharcs_retrieve(&callback_retrieve);
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

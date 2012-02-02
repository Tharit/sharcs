//
//  AppDelegate.h
//  sharcs
//
//  Created by Martin Kleinhans on 02.02.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>{
    NSString *server;
}


@property (strong, nonatomic) UIWindow *window;
@property (nonatomic, readonly) BOOL devicesAvailable;

+ (AppDelegate*)shared;

- (void)selectServer:(NSString*)server;

@end

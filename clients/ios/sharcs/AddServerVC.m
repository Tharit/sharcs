//
//  AddServerVC.m
//  sharcs
//
//  Created by Martin Kleinhans on 02.02.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "AddServerVC.h"

@implementation AddServerVC 

@synthesize nameInput,addressInput;

- (void)apply {
    NSMutableArray *servers = [[[NSUserDefaults standardUserDefaults] valueForKey:@"servers"] mutableCopy];
    
    if(!servers) {
        servers = [[NSMutableArray alloc] initWithCapacity:1];
    }
    
    [servers addObject:[NSDictionary dictionaryWithObjectsAndKeys:nameInput.text,@"name",addressInput.text,@"address", nil]];
    
    [[NSUserDefaults standardUserDefaults] setValue:servers forKey:@"servers"];
    [[NSUserDefaults standardUserDefaults] synchronize];
    
    [self.navigationController popViewControllerAnimated:YES];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(apply)];
}

@end

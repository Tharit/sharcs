//
//  ServerListVC.m
//  sharcs
//
//  Created by Martin Kleinhans on 02.02.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "ServerListVC.h"
#import "AppDelegate.h"

#import <BlocksKit/BlocksKit.h>


@implementation ServerListVC {
    NSArray *servers;
}

#pragma mark - Table view data source

- (void)updateList {
    servers = [[NSUserDefaults standardUserDefaults] valueForKey:@"servers"];
    
    [self.tableView reloadData];
}

- (void)addServer {
    [self performSegueWithIdentifier:@"AddServer" sender:nil];
}

- (void)stopEdit {
    self.tableView.editing = NO;
    self.navigationItem.leftBarButtonItem = nil;
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemEdit target:self action:@selector(startEdit)];
}

- (void)startEdit {
    self.tableView.editing = YES;
    
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self action:@selector(addServer)];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(stopEdit)];
}

- (void)viewDidLoad {
    [self stopEdit];
    
    [[NSUserDefaults standardUserDefaults] addObserverForKeyPath:@"servers" task:^(id obj, NSDictionary *change) {
        [self updateList];
    }];
    
    [self updateList];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [servers count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *CellIdentifier = @"ServerCell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }
    
    cell.textLabel.text = [[servers objectAtIndex:indexPath.row] valueForKey:@"name"];
    
    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if(editingStyle == UITableViewCellEditingStyleDelete) {
        NSMutableArray *s = [servers mutableCopy];
        [s removeObjectAtIndex:indexPath.row];
        [[NSUserDefaults standardUserDefaults] setValue:s forKey:@"servers"];
        [[NSUserDefaults standardUserDefaults] synchronize];
    }
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [[AppDelegate shared] selectServer:[[servers objectAtIndex:indexPath.row] valueForKey:@"address"]];
    [self dismissModalViewControllerAnimated:YES];
}

@end

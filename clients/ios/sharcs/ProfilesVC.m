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

#import "ProfilesVC.h"
#import "AppDelegate.h"
#import "libsharcs.h"
#import <BlocksKit/BlocksKit.h>
#import "AddProfileVC.h"

#define TAG_LABEL 2

@interface ProfilesVC () {
	NSInteger selectedProfileId;
}
- (void)internalInit;
- (void)populate;
- (void)stopEdit;
- (void)startEdit;
- (void)addProfile;
@end

@implementation ProfilesVC

- (void)addProfile {
	selectedProfileId = 0;
    [self performSegueWithIdentifier:@"AddProfile" sender:nil];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([[segue identifier] isEqualToString:@"AddProfile"]) {
        AddProfileVC *vc = [segue destinationViewController];
        
        if(selectedProfileId) {
			[vc setProfileId:selectedProfileId];
		}
    }
}

- (void)stopEdit {
    self.tableView.editing = NO;
    self.navigationItem.leftBarButtonItem = nil;
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemEdit target:self action:@selector(startEdit)];
}

- (void)startEdit {
    self.tableView.editing = YES;
    
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self action:@selector(addProfile)];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(stopEdit)];
}

- (void)populate {
	if(!profiles) {
        profiles = [[NSMutableArray alloc] init];
    } else {
        [profiles removeAllObjects];
    }
    
    struct sharcs_profile *p;
    int i;
    
    i = 0;
    while(sharcs_enumerate_profiles(&p, i++)) {
		[profiles addObject:[NSValue valueWithPointer:p]];
	}
    
    if([self isViewLoaded]) {
		CGFloat offset = self.tableView.contentOffset.y;
	
		[self.tableView reloadData];
		self.tableView.contentOffset = CGPointMake(0,offset);
	}
}

- (void)internalInit {
	if([AppDelegate shared].profilesAvailable) {
		[self populate];
	}
	
	[[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSProfiles" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
        [self populate];
    }];
	
	[[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSProfileSave" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
		[self populate];
    }];
	
	[[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSProfileLoad" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
		// notification?!
    }];
	
	[[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSProfileDelete" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
		[self populate];
    }];
}

- (id)initWithCoder:(NSCoder *)aDecoder {
    self = [super initWithCoder:aDecoder];
    if(self) {
        [self internalInit];
    }
    return self;
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if(self) {
        [self internalInit];
    }
    return self;
}

- (id)initWithStyle:(UITableViewStyle)style {
    self = [super initWithStyle:style];
    if (self) {
        [self internalInit];
    }
    return self;
}

- (void)viewDidLoad {
	[self stopEdit];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [profiles count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    NSString *cellIdentifier = @"ProfileCell";
    struct sharcs_profile *p;
    
    p = ((NSValue*)[profiles objectAtIndex:indexPath.row]).pointerValue;
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cellIdentifier];
    }
    
    cell.tag = p->profile_id;
    
    // set label
    ((UILabel*)[cell viewWithTag:TAG_LABEL]).text = [NSString stringWithCString:p->profile_name encoding:NSASCIIStringEncoding];
    
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	struct sharcs_profile *p;
    
    p = ((NSValue*)[profiles objectAtIndex:indexPath.row]).pointerValue;
	sharcs_profile_load(p->profile_id);
	
	// remove selection
	[tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
	struct sharcs_profile *p;
    
    if(editingStyle == UITableViewCellEditingStyleDelete) {
		p = ((NSValue*)[profiles objectAtIndex:indexPath.row]).pointerValue;
		
		[profiles removeObjectAtIndex:indexPath.row];
		[tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
		
		sharcs_profile_delete(p->profile_id);
	}
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath {
	struct sharcs_profile *p;
	p = ((NSValue*)[profiles objectAtIndex:indexPath.row]).pointerValue;
	selectedProfileId = p->profile_id;
	[self performSegueWithIdentifier:@"AddProfile" sender:nil];
}
@end

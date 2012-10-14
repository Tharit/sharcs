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

#import "FeaturePickerVC.h"
#import "AppDelegate.h"
#import "libsharcs.h"

#define TAG_LABEL 1

@interface FeaturePickerVC () {
	FeaturePickerVCSelectBlock selectBlock;
	FeaturePickerVCFilterBlock filterBlock;
	NSMutableArray *devices;
}
@end

@implementation FeaturePickerVC

- (void)setTask:(FeaturePickerVCSelectBlock)task {
	selectBlock = task;
}

- (void)setFilter:(FeaturePickerVCFilterBlock)filter {
	filterBlock = filter;
}

- (void)populate {
    if(!devices) {
        devices = [[NSMutableArray alloc] init];
    } else {
        [devices removeAllObjects];
    }
    
    struct sharcs_module *m;
    struct sharcs_device *d;
    int i,j;
    
    i = 0;
    while(sharcs_enumerate_modules(&m, i++)) {
		for(j=0;j<m->module_devices_size;j++) {
			d = m->module_devices[j];
            [devices addObject:[NSValue valueWithPointer:d]];
		}
	}
    
    if([self isViewLoaded]) {
		CGFloat offset = self.tableView.contentOffset.y;
		
		[self.tableView reloadData];
		self.tableView.contentOffset = CGPointMake(0,offset);
	}
}

- (void)internalInit {
	if([AppDelegate shared].devicesAvailable) {
        [self populate];
    }
    [[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSRetrieve" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
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

#pragma mark - Table view data source

- (NSString*)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    struct sharcs_device *d;
    
    d = ((NSValue*)[devices objectAtIndex:section]).pointerValue;
    return [NSString stringWithCString:d->device_name encoding:NSASCIIStringEncoding];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return [devices count];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    struct sharcs_device *d;
    
    d = ((NSValue*)[devices objectAtIndex:section]).pointerValue;

    return d->device_features_size;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    NSString *cellIdentifier = @"FeatureCell";
    struct sharcs_device *d;
    struct sharcs_feature *f;
    
    d = ((NSValue*)[devices objectAtIndex:indexPath.section]).pointerValue;
    f = d->device_features[indexPath.row];
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cellIdentifier];
    }
		
	if(!filterBlock(f->feature_id)) {
		cell.userInteractionEnabled = NO;
		cell.textLabel.alpha = 0.43f;
	}
	
    cell.tag = f->feature_id;
    
    // set label
    cell.textLabel.text = [NSString stringWithCString:f->feature_name encoding:NSASCIIStringEncoding];
    
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
	selectBlock(cell.tag);
	[self.navigationController popViewControllerAnimated:YES];
}

@end

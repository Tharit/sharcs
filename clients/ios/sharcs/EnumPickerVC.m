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

#import "EnumPickerVC.h"

@implementation EnumPickerVC {
    struct sharcs_feature *feature;
	EnumPickerVCSelectBlock selectBlock;
	NSInteger featureValue;
}

- (void)focus {
    int index;
    if(feature->feature_type == SHARCS_FEATURE_ENUM) {
        index = feature->feature_value.v_enum.value;
    } else {
        index = feature->feature_value.v_range.value;
    }
    [self.tableView scrollToRowAtIndexPath:[NSIndexPath indexPathForRow:index inSection:0] atScrollPosition:UITableViewScrollPositionMiddle animated:NO];
}

- (void)setFeatureValue:(NSInteger)value {
	featureValue = value;
}

- (void)setFeature:(sharcs_id)feature_id task:(EnumPickerVCSelectBlock)task {
    feature = sharcs_feature(feature_id);
	switch(feature->feature_type) {
		case SHARCS_FEATURE_ENUM:
			featureValue = feature->feature_value.v_enum.value;
			break;
		case SHARCS_FEATURE_RANGE:
			featureValue = feature->feature_value.v_range.value;
			break;
	}
	
    if([self isViewLoaded]) {
        [self.tableView reloadData];
        [self focus];
    }
    
	if(task) {
		selectBlock = task;
	} else {
		selectBlock = ^(NSInteger index) {
			sharcs_set_i(feature_id, index);
		};
	}
	
    if(!feature) {
        return;
    }
    
    self.navigationItem.title = [NSString stringWithCString:feature->feature_name encoding:NSASCIIStringEncoding];
}

#pragma mark - Table view data source

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self focus];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    if(!feature) {
        return 0;
    }
    
    if(feature->feature_type == SHARCS_FEATURE_ENUM) {
        return feature->feature_value.v_enum.size;
    }
    return (feature->feature_value.v_range.end-feature->feature_value.v_range.start)+1;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *CellIdentifier = @"ValueCell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }
    
    cell.accessoryType = UITableViewCellAccessoryNone;
    
    if(feature->feature_type == SHARCS_FEATURE_ENUM) {
        cell.textLabel.text = [NSString stringWithCString:feature->feature_value.v_enum.values[indexPath.row] encoding:NSASCIIStringEncoding];
    } else {
        cell.textLabel.text = [NSString stringWithFormat:@"%d",feature->feature_value.v_range.start+indexPath.row];
	}
    
	if(indexPath.row == featureValue) {
		cell.accessoryType = UITableViewCellAccessoryCheckmark;
	}
	
    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    selectBlock(indexPath.row);
	[self.navigationController popViewControllerAnimated:YES];
}

@end

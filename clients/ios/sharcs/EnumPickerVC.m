//
//  EnumPickerVC.m
//  sharcs
//
//  Created by Martin Kleinhans on 02.02.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EnumPickerVC.h"


@implementation EnumPickerVC {
    struct sharcs_feature *feature;
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

- (void)setFeature:(sharcs_id)feature_id {
    feature = sharcs_feature(feature_id);
    if([self isViewLoaded]) {
        [self.tableView reloadData];
        [self focus];
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
        
        if(indexPath.row == feature->feature_value.v_enum.value) {
            cell.accessoryType = UITableViewCellAccessoryCheckmark;
        }
    } else {
        cell.textLabel.text = [NSString stringWithFormat:@"%d",feature->feature_value.v_range.start+indexPath.row];
        
        if(indexPath.row == feature->feature_value.v_range.value) {
            cell.accessoryType = UITableViewCellAccessoryCheckmark;
        }
    }
    
    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    sharcs_set_i(feature->feature_id, indexPath.row);
    [self.navigationController popViewControllerAnimated:YES];
}

@end

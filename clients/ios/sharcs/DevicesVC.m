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

#import "DevicesVC.h"
#import "AppDelegate.h"

#import "libsharcs.h"
#import "EnumPickerVC.h"

#import <BlocksKit/BlocksKit.h>

#define TAG_LABEL 2
#define TAG_VALUE 1
#define TAG_VALUE2 3
#define TAG_SLIDER_VALUE 5

@interface DevicesVC (Private)
- (void)internalInit;
- (void)populate;
@end

@implementation DevicesVC (Private)

- (void)internalInit {
    if([AppDelegate shared].devicesAvailable) {
        [self populate];
    }
    [[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSRetrieve" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
        [self populate];
    }];
    
    [[NSNotificationCenter defaultCenter] addObserverForName:@"SHARCSFeatureI" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*n) {
        UITableViewCell *cell;
        struct sharcs_feature *f;
        sharcs_id feature;
        int value;
        
        
        feature = [[n.userInfo valueForKey:@"feature"] intValue];
        value = [[n.userInfo valueForKey:@"value"] intValue];
        
        cell = (UITableViewCell*)[self.tableView viewWithTag:feature];
        f = sharcs_feature(feature);
        
        if(cell && f) {        
            switch(f->feature_type) {
                case SHARCS_FEATURE_SWITCH: {
                    UISwitch *switchView;
                    
                    switchView = ((UISwitch*)[cell viewWithTag:TAG_VALUE]);
                    switchView.on = value;
                    
                    if(f->feature_flags & SHARCS_FLAG_POWER) {
                        [self populate];
                    }
                    
                    break;
                }
                case SHARCS_FEATURE_RANGE: {
                    if(f->feature_flags & SHARCS_FLAG_SLIDER) {                                
                        UILabel *sliderValueView;
						UISlider *sliderView;
                        sliderView = ((UISlider*)[cell viewWithTag:TAG_VALUE]);
						sliderValueView = ((UILabel*)[cell viewWithTag:TAG_SLIDER_VALUE]);
                        if(!sliderView.tracking) {
                            if(f->feature_flags & SHARCS_FLAG_INVERSE) {
                                sliderView.value = f->feature_value.v_range.end - value;
                            } else {
                                sliderView.value = value;
                            }
							sliderValueView.text = [NSString stringWithFormat:@"%d",value];
                        }
                    } else {
                        UILabel *labelView;   
                        labelView = ((UILabel*)[cell viewWithTag:TAG_VALUE]);
                        labelView.text = [NSString stringWithFormat:@"%d",value];
                    }
                    break;
                }
                case SHARCS_FEATURE_ENUM: {
                    UILabel *labelView;   
                    labelView = ((UILabel*)[cell viewWithTag:TAG_VALUE]);
                    labelView.text = [NSString stringWithCString:f->feature_value.v_enum.values[value] encoding:NSASCIIStringEncoding];
                    break;
                }
            }
        }    
    }];
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

@end

@implementation DevicesVC 

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

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([[segue identifier] isEqualToString:@"EnumPicker"]) {
        EnumPickerVC *vc = [segue destinationViewController];
        
        // Pass any objects to the view controller here, like...
        [vc setFeature:((UIView*)sender).tag task:nil];
    }
}

#pragma mark - Table view data source

- (NSString*)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    struct sharcs_device *d;
    
    d = ((NSValue*)[devices objectAtIndex:section]).pointerValue;
    return [NSString stringWithCString:d->device_name encoding:NSASCIIStringEncoding];
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
    struct sharcs_device *d;
    struct sharcs_feature *f;
    
    d = ((NSValue*)[devices objectAtIndex:indexPath.section]).pointerValue;
    f = d->device_features[indexPath.row];
    
    if((f->feature_flags & SHARCS_FLAG_SLIDER) && f->feature_type == SHARCS_FEATURE_RANGE) {
        return 66;
    }
    return 44;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return [devices count];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    struct sharcs_device *d;
    
    d = ((NSValue*)[devices objectAtIndex:section]).pointerValue;
   // if(d->device_flags & SHARCS_FLAG_STANDBY) {
    //    return 1;
   // }
    return d->device_features_size;
}

- (void)viewDidAppear:(BOOL)animated {
	[super viewDidAppear:animated];
	[self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow animated:YES];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    NSString *cellIdentifier;
    struct sharcs_device *d;
    struct sharcs_feature *f;
    
    d = ((NSValue*)[devices objectAtIndex:indexPath.section]).pointerValue;
    /*if(d->device_flags & SHARCS_FLAG_STANDBY) {
        int i;
        for(i=0;i<d->device_features_size;i++) {
            f = d->device_features[i];
            if(f->feature_flags & SHARCS_FLAG_POWER) {
                break; 
            }
        }
    } else {*/
        f = d->device_features[indexPath.row];
    //}
    
    switch (f->feature_type) {
        case SHARCS_FEATURE_SWITCH:
            cellIdentifier = @"FeatureSwitchCell";
            break;
        case SHARCS_FEATURE_RANGE:
            if(f->feature_flags & SHARCS_FLAG_SLIDER) {
                cellIdentifier = @"FeatureSliderCell";
            } else {
                cellIdentifier = @"FeatureEnumCell";
            }
            break;
        case SHARCS_FEATURE_ENUM:
            cellIdentifier = @"FeatureEnumCell";
            break;
    }
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cellIdentifier];
    }
    
    cell.tag = f->feature_id;
    
    // set label
    ((UILabel*)[cell viewWithTag:TAG_LABEL]).text = [NSString stringWithCString:f->feature_name encoding:NSASCIIStringEncoding];
    
    if(d->device_flags & SHARCS_FLAG_STANDBY && !(f->feature_flags & SHARCS_FLAG_POWER)) {
        ((UILabel*)[cell viewWithTag:TAG_LABEL]).alpha = 0.43f;
        cell.userInteractionEnabled = NO;
    } else {
        ((UILabel*)[cell viewWithTag:TAG_LABEL]).alpha = 1.0f;
        cell.userInteractionEnabled = YES;
    }
    
    // set value view
    switch (f->feature_type) {
        case SHARCS_FEATURE_SWITCH: {
            __weak UISwitch *switchView;
            
            switchView = ((UISwitch*)[cell viewWithTag:TAG_VALUE]);
            switchView.on = f->feature_value.v_switch.state;
            [switchView removeEventHandlersForControlEvents:UIControlEventValueChanged];
            [switchView addEventHandler:^(id sender) {
                sharcs_set_i(f->feature_id, switchView.on);
            } forControlEvents:UIControlEventValueChanged];
            
            if(d->device_flags & SHARCS_FLAG_STANDBY && !(f->feature_flags & SHARCS_FLAG_POWER)) {
                switchView.enabled = NO;
            } else {
                switchView.enabled = YES;
            }
            
            break;
        }
        case SHARCS_FEATURE_RANGE: {
            if(f->feature_flags & SHARCS_FLAG_SLIDER) {

                __weak UISlider *sliderView;
				__weak UILabel *sliderValueView;
                __block int lastValue;
                
                sliderValueView = ((UILabel*)[cell viewWithTag:TAG_SLIDER_VALUE]);
                sliderView = ((UISlider*)[cell viewWithTag:TAG_VALUE]);
                sliderView.minimumValue = f->feature_value.v_range.start;
                sliderView.maximumValue = f->feature_value.v_range.end;
                
                if(f->feature_flags & SHARCS_FLAG_INVERSE) {
                    sliderView.value = f->feature_value.v_range.end-f->feature_value.v_range.value;
                } else {
                    sliderView.value = f->feature_value.v_range.value;
                }
                sliderValueView.text = [NSString stringWithFormat:@"%d",f->feature_value.v_range.value];
				
                [sliderView removeEventHandlersForControlEvents:UIControlEventValueChanged];
                [sliderView addEventHandler:^(id sender) {
                    int v = ceilf(sliderView.value);
                    
                    if(sliderView.value != v) {
                        sliderView.value = v;
                    }
                    
                    if(lastValue!=v) {
                        lastValue = v;
                        if(f->feature_flags & SHARCS_FLAG_INVERSE) {
                            v = f->feature_value.v_range.end-v;
                        }
                        sharcs_set_i(f->feature_id, v);
						sliderValueView.text = [NSString stringWithFormat:@"%d",v];
                    }
                } forControlEvents:UIControlEventValueChanged];
                
                if(d->device_flags & SHARCS_FLAG_STANDBY && !(f->feature_flags & SHARCS_FLAG_POWER)) {
                    sliderView.enabled = NO;
                } else {
                    sliderView.enabled = YES;
                }
            } else {
                UILabel *labelView;   
                
                labelView = ((UILabel*)[cell viewWithTag:TAG_VALUE]);
                labelView.text = [NSString stringWithFormat:@"%d",f->feature_value.v_range.value];
                
                if(d->device_flags & SHARCS_FLAG_STANDBY && !(f->feature_flags & SHARCS_FLAG_POWER)) {
                    labelView.alpha = 0.43f;
                } else {
                    labelView.alpha = 1.0f;
                }
            }
            
            break;
        }
        case SHARCS_FEATURE_ENUM: {
            UILabel *labelView;   
            labelView = ((UILabel*)[cell viewWithTag:TAG_VALUE]);
            labelView.text = [NSString stringWithCString:f->feature_value.v_enum.values[f->feature_value.v_enum.value] encoding:NSASCIIStringEncoding];
            
            if(d->device_flags & SHARCS_FLAG_STANDBY && !(f->feature_flags & SHARCS_FLAG_POWER)) {
                labelView.alpha = 0.43f;
            } else {
                labelView.alpha = 1.0f;
            }
            break;
        }
    }
   
    return cell;
}

@end

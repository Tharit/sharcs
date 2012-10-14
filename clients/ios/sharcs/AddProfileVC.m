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

#import "AddProfileVC.h"
#import "AppDelegate.h"
#import <BlocksKit/BlocksKit.h>
#import "libsharcs.h"
#import "EnumPickerVC.h"
#import "FeaturePickerVC.h"

#define TAG_INPUT 2
#define TAG_LABEL 2
#define TAG_VALUE 1
#define TAG_VALUE2 3
#define TAG_DEVICE 4
#define TAG_SLIDER_VALUE 5

//------------------------------------------------------------

@interface FeatureInfo : NSObject {
@public
	struct sharcs_feature *feature;
	int value;
}
@end

@implementation FeatureInfo
- (id)initWithFeature:(struct sharcs_feature *)f {
	self = [super init];
	if(self) {
		feature = f;
		switch(f->feature_type) {
			case SHARCS_FEATURE_ENUM:
				value = f->feature_value.v_enum.value;
				break;
			case SHARCS_FEATURE_SWITCH:
				value = f->feature_value.v_switch.state;
				break;
			case SHARCS_FEATURE_RANGE:
				value = f->feature_value.v_range.value;
				break;
		}
		
		// do not use uninitialized values
		if(value == SHARCS_VALUE_UNKNOWN || value == SHARCS_VALUE_ERROR) {
			value = SHARCS_VALUE_OFF;
		}
	}
	return self;
}

@end

//------------------------------------------------------------

@interface AddProfileVC () {
	NSMutableArray *features;
	NSInteger profileId;
}
@end

@implementation AddProfileVC

- (void)addFeature:(NSInteger) featureId {
	FeatureInfo *fi;
	struct sharcs_feature *f;
	
	f = sharcs_feature(featureId);
	
	fi = [[FeatureInfo alloc] initWithFeature:f];
	[features addObject:fi];
	
	[self.tableView insertRowsAtIndexPaths:[NSArray arrayWithObject:[NSIndexPath indexPathForRow:[features count]-1 inSection:1]] withRowAnimation:UITableViewRowAnimationFade];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
	if ([[segue identifier] isEqualToString:@"EnumPicker"]) {
		EnumPickerVC *vc = [segue destinationViewController];
		
		NSInteger featureId = ((UIView*)sender).tag;
		FeatureInfo *fi;
		for(fi in features) {
			if(fi->feature->feature_id == featureId) {
				break;
			}
		}
		[vc setFeature:featureId task:^(NSInteger index) {
			fi->value = index;
			
			UILabel *labelView;
			labelView = ((UILabel*)[(UIView*)sender viewWithTag:TAG_VALUE]);
			labelView.text = [NSString stringWithCString:fi->feature->feature_value.v_enum.values[index] encoding:NSASCIIStringEncoding];
		}];
	} else if([[segue identifier] isEqualToString:@"AddFeature"]) {
		FeaturePickerVC *vc = [segue destinationViewController];
		
		__weak AddProfileVC *weakSelf = self;
		[vc setTask:^(NSInteger featureId) {
			[weakSelf addFeature:featureId];
		}];
		[vc setFilter:^(NSInteger featureId) {
			for(FeatureInfo *fi in features) {
				if(fi->feature->feature_id == featureId) {
					return NO;
				}
			}
			return YES;
		}];
	}
}

- (void)internalInit {
	features = [[NSMutableArray alloc] init];
	profileId = 0; // let sharcs assign a new id by default
}

- (void)populate {
	[features removeAllObjects];
	
	if(!profileId) {
		return;
	}
	
	struct sharcs_profile *profile;
	FeatureInfo *fi;
	
	// fetch profile
	profile = sharcs_profile(profileId);
	
	for(NSInteger i=0;i<profile->profile_size;i++) {
		fi = [[FeatureInfo alloc] initWithFeature:sharcs_feature(profile->profile_features[i])];
		fi->value = profile->profile_values[i];
		[features addObject:fi];
	}
	// populate features
	[self.tableView reloadData];
}

- (void)setProfileId:(NSInteger)newProfileId {
	profileId = newProfileId;
	if([self isViewLoaded]) {
		[self populate];
	}
}

- (void)done {
	struct sharcs_profile *profile;
	FeatureInfo *fi;
	
	NSString *name;
	name = [(UITextField*)([[self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]] viewWithTag:TAG_INPUT]) text];
			
	profile = (struct sharcs_profile*)malloc(sizeof(struct sharcs_profile));
	profile->profile_id 			= profileId;
	profile->profile_name 			= [name cStringUsingEncoding:NSASCIIStringEncoding];
	profile->profile_size			= [features count];
	profile->profile_features 		= (int*)malloc(sizeof(int)*profile->profile_size);
	profile->profile_values 		= (int*)malloc(sizeof(int)*profile->profile_size);
	
	for(int j=0;j<profile->profile_size;j++) {
		fi = [features objectAtIndex:j];
		
		profile->profile_features[j] 	= fi->feature->feature_id;
		profile->profile_values[j] 		= fi->value;
	}
	
	sharcs_profile_save(profile);
	
	[self.navigationController popViewControllerAnimated:YES];
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
    [super viewDidLoad];
    
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];
	self.tableView.editing = YES;
	
	[self populate];
}

- (void)viewDidAppear:(BOOL)animated {
	[super viewDidAppear:animated];
	[self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow animated:YES];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
	return 2;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [textField resignFirstResponder];
	return YES;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	if(section==0) {
		return 1;
	}
	return [features count] + 1;
}

- (NSString*)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
	if(section==0) {
		return @"Details";
	}
	return @"Features";
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
    // special cells
	if(indexPath.section==0||indexPath.row == [features count]) {
		return 44;
	}
	
	// feature cells
	struct sharcs_feature *f;
    
    f = ((FeatureInfo*)[features objectAtIndex:indexPath.row])->feature;
    
    if((f->feature_flags & SHARCS_FLAG_SLIDER) && f->feature_type == SHARCS_FEATURE_RANGE) {
        return 88;
    }
    return 66;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
	NSString *cellIdentifier;
	FeatureInfo *fi;
	
    if(indexPath.section == 0) {
		cellIdentifier = @"NameCell";
	} else if(indexPath.section == 1) {
		if(indexPath.row == [features count]) {
			cellIdentifier = @"AddFeatureCell";
		} else {
			fi = [features objectAtIndex:indexPath.row];
		
			switch (fi->feature->feature_type) {
				case SHARCS_FEATURE_SWITCH:
					cellIdentifier = @"FeatureSwitchCell";
					break;
				case SHARCS_FEATURE_RANGE:
					if(fi->feature->feature_flags & SHARCS_FLAG_SLIDER) {
						cellIdentifier = @"FeatureSliderCell";
					} else {
						cellIdentifier = @"FeatureEnumCell";
					}
					break;
				case SHARCS_FEATURE_ENUM:
					cellIdentifier = @"FeatureEnumCell";
					break;
			}
		}
	}
	
	UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cellIdentifier];
    }
    
    if(indexPath.section == 0 && indexPath.row == 0) {
		UITextField *textField;
		textField = ((UITextField*)[cell viewWithTag:TAG_INPUT]);
		textField.delegate = self;
		
		if(profileId) {
			struct sharcs_profile *profile;
			profile = sharcs_profile(profileId);
			textField.text = [NSString stringWithCString:profile->profile_name encoding:NSASCIIStringEncoding];
		}
	}
	// feature cells
	if(indexPath.section == 1 && indexPath.row != [features count]) {
		struct sharcs_device *d;
		
		d = sharcs_device(SHARCS_ID_DEVICE(fi->feature->feature_id));
		
		cell.showsReorderControl = YES;
		cell.tag = fi->feature->feature_id;
		
		((UILabel*)[cell viewWithTag:TAG_LABEL]).text = [NSString stringWithCString:fi->feature->feature_name encoding:NSASCIIStringEncoding];
		((UILabel*)[cell viewWithTag:TAG_DEVICE]).text = [NSString stringWithCString:d->device_name encoding:NSASCIIStringEncoding];
		
		// set value view
		switch (fi->feature->feature_type) {
			case SHARCS_FEATURE_SWITCH: {
				__weak UISwitch *switchView;
				
				switchView = ((UISwitch*)[cell viewWithTag:TAG_VALUE]);
				switchView.on = fi->value;
				[switchView removeEventHandlersForControlEvents:UIControlEventValueChanged];
				[switchView addEventHandler:^(id sender) {
					fi->value = switchView.on;
				} forControlEvents:UIControlEventValueChanged];
				
				break;
			}
			case SHARCS_FEATURE_RANGE: {
				if(fi->feature->feature_flags & SHARCS_FLAG_SLIDER) {
					__weak UISlider *sliderView;
					__weak UILabel *sliderValueView;
					
					sliderValueView = ((UILabel*)[cell viewWithTag:TAG_SLIDER_VALUE]);
					sliderView = ((UISlider*)[cell viewWithTag:TAG_VALUE]);
					sliderView.minimumValue = fi->feature->feature_value.v_range.start;
					sliderView.maximumValue = fi->feature->feature_value.v_range.end;
					
					if(fi->feature->feature_flags & SHARCS_FLAG_INVERSE) {
						sliderView.value = fi->feature->feature_value.v_range.end-fi->value;
					} else {
						sliderView.value = fi->value;
					}
					sliderValueView.text = [NSString stringWithFormat:@"%d",fi->value];
					
					[sliderView removeEventHandlersForControlEvents:UIControlEventValueChanged];
					[sliderView addEventHandler:^(id sender) {
						int v = ceilf(sliderView.value);
						
						if(sliderView.value != v) {
							sliderView.value = v;
						}
						
						if(fi->feature->feature_flags & SHARCS_FLAG_INVERSE) {
                            v = fi->feature->feature_value.v_range.end-v;
                        }
						sliderValueView.text = [NSString stringWithFormat:@"%d",v];
						
						fi->value = v;
					} forControlEvents:UIControlEventValueChanged];
					
				} else {
					UILabel *labelView;
					
					labelView = ((UILabel*)[cell viewWithTag:TAG_VALUE]);
					labelView.text = [NSString stringWithFormat:@"%d",fi->value];
				}
				
				break;
			}
			case SHARCS_FEATURE_ENUM: {
				UILabel *labelView;
				labelView = ((UILabel*)[cell viewWithTag:TAG_VALUE]);
				labelView.text = [NSString stringWithCString:fi->feature->feature_value.v_enum.values[fi->feature->feature_value.v_enum.value] encoding:NSASCIIStringEncoding];
				break;
			}
		}
    }
	
    return cell;
}

- (UITableViewCellEditingStyle)tableView:(UITableView *)tableView editingStyleForRowAtIndexPath:(NSIndexPath *)indexPath {
    if(indexPath.section == 0) {
		return UITableViewCellEditingStyleNone;
	}
	if(indexPath.row == [features count]) {
		return UITableViewCellEditingStyleInsert;
	}
	return UITableViewCellEditingStyleDelete;
}

- (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath {
	return indexPath.section==1 && indexPath.row != [features count];
}

- (void)tableView:(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath *)sourceIndexPath toIndexPath:(NSIndexPath *)destinationIndexPath {
	NSValue *value = [features objectAtIndex:sourceIndexPath.row];
	[features removeObjectAtIndex:sourceIndexPath.row];
	[features insertObject:value atIndex:destinationIndexPath.row];
}

- (NSIndexPath*)tableView:(UITableView *)tableView targetIndexPathForMoveFromRowAtIndexPath:(NSIndexPath *)sourceIndexPath toProposedIndexPath:(NSIndexPath *)proposedDestinationIndexPath {
	if(proposedDestinationIndexPath.section == 0) {
		return [NSIndexPath indexPathForRow:0 inSection:1];
	}
	if(proposedDestinationIndexPath.row>=[features count]) {
		return [NSIndexPath indexPathForRow:[features count]-1 inSection:1];
	}
	return proposedDestinationIndexPath;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if(editingStyle == UITableViewCellEditingStyleDelete) {
		[features removeObjectAtIndex:indexPath.row];
		[tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
	} else if(editingStyle == UITableViewCellEditingStyleInsert) {
		[self performSegueWithIdentifier:@"AddFeature" sender:self];
	}
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	if(indexPath.section == 1 && indexPath.row == [features count]) {
		[tableView deselectRowAtIndexPath:indexPath animated:YES];
		[self performSegueWithIdentifier:@"AddFeature" sender:self];
	}
}

- (BOOL)tableView:(UITableView *)tableView shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath *)indexPath {
    return NO;
}

@end

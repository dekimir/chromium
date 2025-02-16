// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_

@class CollectionViewItem;
@class LegacySyncSwitchItem;

// Protocol to handle user actions from the Google services settings view.
@protocol GoogleServicesSettingsServiceDelegate<NSObject>

// Called when the UISwitch from the LegacySyncSwitchItem is toggled.
- (void)toggleSwitchItem:(LegacySyncSwitchItem*)switchItem
               withValue:(BOOL)value;

// Returns YES if the item can be highlighted;
- (BOOL)shouldHighlightItem:(CollectionViewItem*)item;

// Called when cell is tapped.
- (void)didSelectItem:(CollectionViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_

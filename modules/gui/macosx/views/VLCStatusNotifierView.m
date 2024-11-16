/*****************************************************************************
 * VLCStatusNotifierView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCStatusNotifierView.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryModel.h"

@interface VLCStatusNotifierView ()

@property NSUInteger remainingCount;
@property NSUInteger loadingCount;
@property (nonatomic) BOOL permanentDiscoveryMessageActive;
@property NSMutableSet<NSString *> *longNotifications;

@end

@implementation VLCStatusNotifierView

- (void)awakeFromNib
{
    self.remainingCount = 0;
    self.loadingCount = 0;
    self.permanentDiscoveryMessageActive = NO;
    self.longNotifications = NSMutableSet.set;
    self.label.stringValue = _NS("Idle");

    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
    [defaultCenter addObserver:self selector:@selector(updateStatus:) name:nil object:nil];
}

- (void)displayStartLoad
{
    if (self.loadingCount == 0) {
        [self.progressIndicator startAnimation:self];
    }
    self.loadingCount++;
}

- (void)displayFinishLoad
{
    self.loadingCount--;
    if (self.loadingCount == 0) {
        [self.progressIndicator stopAnimation:self];
    }
}

- (void)setPermanentDiscoveryMessageActive:(BOOL)permanentDiscoveryMessageActive
{
    if (_permanentDiscoveryMessageActive == permanentDiscoveryMessageActive) {
        return;
    }

    _permanentDiscoveryMessageActive = permanentDiscoveryMessageActive;
    if (permanentDiscoveryMessageActive) {
        self.remainingCount++;
        [self displayStartLoad];
    } else {
        self.remainingCount--;
        [self displayFinishLoad];
    }
}

- (void)updateStatus:(NSNotification *)notification
{
    NSString * const notificationName = notification.name;
    if (![notificationName hasPrefix:@"VLC"]) {
        return;
    }

    if ([notificationName isEqualToString:VLCLibraryModelDiscoveryStarted]) {
        [self presentTransientMessage:_NS("Discovering media…")];
    } else if ([notificationName isEqualToString:VLCLibraryModelDiscoveryProgress]) {
        self.label.stringValue = _NS("Discovering media…");
        self.permanentDiscoveryMessageActive = YES;
    } else if ([notificationName isEqualToString:VLCLibraryModelDiscoveryCompleted]) {
        self.permanentDiscoveryMessageActive = NO;
        [self presentTransientMessage:_NS("Media discovery completed")];
    } else if ([notificationName isEqualToString:VLCLibraryModelDiscoveryFailed]) {
        self.permanentDiscoveryMessageActive = NO;
        [self presentTransientMessage:_NS("Media discovery failed")];
    } else if ([notificationName containsString:VLCLongNotificationNameStartSuffix] && ![self.longNotifications containsObject:notificationName]) {
        [self.longNotifications addObject:notificationName];
        self.label.stringValue = _NS("Loading library items…");
        self.remainingCount++;
        [self displayStartLoad];
    } else if ([notificationName containsString:VLCLongNotificationNameFinishSuffix]) {
        NSString * const loadingNotification =
            [notificationName stringByReplacingOccurrencesOfString:VLCLongNotificationNameFinishSuffix withString:VLCLongNotificationNameStartSuffix];
        [self.longNotifications removeObject:loadingNotification];
        self.remainingCount--;
        [self displayFinishLoad];
        [self presentTransientMessage:_NS("Library items loaded")];
    }
}

- (void)presentTransientMessage:(NSString *)message
{
    self.label.stringValue = message;
    [self performSelector:@selector(clearTransientMessage) withObject:nil afterDelay:2.0];
    self.remainingCount++;
}

- (void)clearTransientMessage
{
    self.remainingCount--;
    if (self.remainingCount > 0) {
        return;
    }
    self.label.stringValue = _NS("Idle");
}

@end
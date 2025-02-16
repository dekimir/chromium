// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_MAC_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

// Whether to enable update and launch of app shims in tests. (Normally shims
// are never created or launched in tests). Note that update only creates
// internal shim bundles, i.e. it does not create new shims in ~/Applications.
extern bool g_app_shims_allow_update_and_launch_in_tests;

namespace web_app {

enum class LaunchShimUpdateBehavior {
  NO_UPDATE,
  UPDATE_IF_INSTALLED,
  RECREATE,
};

// Callback type for LaunchShim. If |shim_process| is valid then the
// app shim was launched.
using LaunchShimCallback = base::OnceCallback<void(base::Process shim_process)>;

// Launch the shim specified by |shortcut_info|. Update the shim prior to launch
// if requested. Return in |callback| the pid that was launched (or an invalid
// pid if none was launched).
void LaunchShim(LaunchShimUpdateBehavior update_behavior,
                LaunchShimCallback callback,
                std::unique_ptr<web_app::ShortcutInfo> shortcut_info);

std::unique_ptr<web_app::ShortcutInfo> RecordAppShimErrorAndBuildShortcutInfo(
    const base::FilePath& bundle_path);

// Return true if launching and updating app shims will fail because of the
// testing environment.
bool AppShimLaunchDisabled();

// Creates a shortcut for a web application. The shortcut is a stub app
// that simply loads the browser framework and runs the given app.
class WebAppShortcutCreator {
 public:
  // Creates a new shortcut based on information in |shortcut_info|.
  // A copy of the shortcut is placed in |app_data_dir|.
  // |chrome_bundle_id| is the CFBundleIdentifier of the Chrome browser bundle.
  // Retains the pointer |shortcut_info|; the ShortcutInfo object must outlive
  // the WebAppShortcutCreator.
  WebAppShortcutCreator(const base::FilePath& app_data_dir,
                        const ShortcutInfo* shortcut_info);

  virtual ~WebAppShortcutCreator();

  // Returns the base name for the shortcut.
  virtual base::FilePath GetShortcutBasename() const;

  // Returns a path to the Chrome Apps folder in the relevant applications
  // folder. E.g. ~/Applications or /Applications.
  virtual base::FilePath GetApplicationsDirname() const;

  // The full path to the app bundle under the relevant Applications folder.
  base::FilePath GetApplicationsShortcutPath() const;

  // Returns the paths to app bundles with the given id as found by launch
  // services, sorted by preference.
  std::vector<base::FilePath> GetAppBundlesById() const;

  // The full path to the app bundle under the profile folder.
  base::FilePath GetInternalShortcutPath() const;

  bool CreateShortcuts(ShortcutCreationReason creation_reason,
                       ShortcutLocations creation_locations);
  void DeleteShortcuts();

  // Recreate the shortcuts where they are found on disk and in the profile
  // path. If |recreate_if_needed| is true, then recreate the shortcuts if no
  // matching shortcuts are found on disk. Populate |updated_paths| with the
  // paths that were updated.
  bool UpdateShortcuts(bool recreate_if_needed,
                       std::vector<base::FilePath>* updated_paths);

  // Show the bundle we just generated in the Finder.
  virtual void RevealAppShimInFinder() const;

 protected:
  virtual std::vector<base::FilePath> GetAppBundlesByIdUnsorted() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, DeleteShortcuts);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateIcon);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateShortcuts);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest,
                           UpdateBookmarkAppShortcut);

  // Returns the bundle identifier to use for this app bundle.
  std::string GetBundleIdentifier() const;

  // Returns the bundle identifier for the internal copy of the bundle.
  std::string GetInternalBundleIdentifier() const;

  // Copies the app loader template into a temporary directory and fills in all
  // relevant information.
  bool BuildShortcut(const base::FilePath& staging_path) const;

  // Builds a shortcut and copies it to the specified app paths. Returns with
  // the number of successful copies created. If non-nullptr, populates
  // |updated_paths| with the paths that were successfully updated.
  size_t CreateShortcutsAt(const std::vector<base::FilePath>& app_paths,
                           std::vector<base::FilePath>* updated_paths) const;

  // Updates the InfoPlist.string inside |app_path| with the display name for
  // the app.
  bool UpdateDisplayName(const base::FilePath& app_path) const;

  // Updates the bundle id of the internal copy of the app shim bundle.
  bool UpdateInternalBundleIdentifier() const;

  // Updates the plist inside |app_path| with information about the app.
  bool UpdatePlist(const base::FilePath& app_path) const;

  // Updates the icon for the shortcut.
  bool UpdateIcon(const base::FilePath& app_path) const;

  // Path to the data directory for this app. For example:
  // ~/Library/Application Support/Chromium/Default/Web Applications/_crx_abc/
  base::FilePath app_data_dir_;

  // Information about the app. Owned by the caller of the constructor.
  const ShortcutInfo* info_;

  DISALLOW_COPY_AND_ASSIGN(WebAppShortcutCreator);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_MAC_H_

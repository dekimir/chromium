// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockVolumeManager} */
var volumeManager;

/** @type {!DirectoryModel} */
var directoryModel;

/** @type {!MetadataModel} */
var metadataModel;

/** @type {!FileOperationManager} */
var fileOperationManager;

/** @type {!Array} */
var directoryChangedListeners;

/** @type {!Object<string,!MockDirectoryEntry>} */
var fakeFileSystemURLEntries;

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordSmallCount: function() {},
};

/**
 * Mock Chrome APIs
 * @type {!Object}
 */
var mockChrome;

// Set up test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.getString = id => id;
  window.loadTimeData.data = {};

  // Setup mock chrome APIs.
  directoryChangedListeners = [];
  mockChrome = {
    fileManagerPrivate: {
      onDirectoryChanged: {
        addListener: (listener) => {
          directoryChangedListeners.push(listener);
        },
      },
    },
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  // Setup mock components.
  volumeManager = new MockVolumeManager();
  directoryModel = new MockDirectoryModel();
  metadataModel = /** @type {!MetadataModel} */ ({});
  fileOperationManager = /** @type {!FileOperationManager} */ ({
    addEventListener: (name, callback) => {},
  });

  // Setup fake file system URL resolver.
  fakeFileSystemURLEntries = {};
  window.webkitResolveLocalFileSystemURL = (url, callback) => {
    callback(fakeFileSystemURLEntries[url]);
  };
}

/**
 * Creates the DOM element structure of the directory tree and returns the
 * directory tree DOM element from within that structure.
 * @return {!HTMLElement}
 */
function createElements() {
  let parent = document.createElement('div');
  let tree = document.createElement('div');
  parent.appendChild(tree);
  return /** @type {!HTMLElement} */ (tree);
}

/**
 * Returns a mock MetadataModel.
 * @return {!MetadataModel}
 */
function createMockMetadataModel() {
  return /** @type {!MetadataModel} */ ({
    notifyEntriesChanged: () => {},
    // get and getCache mock a non-shared directory.
    get: (entries, labels) => {
      return Promise.resolve([{shared: false}]);
    },
    getCache: (entries, labels) => {
      return [{shared: false}];
    },
  });
}

/**
 * Returns the directory tree item labels.
 *
 * @param {!DirectoryTree} directoryTree The directory tree object.
 * @return {!Array<string>} Array of label strings.
 */
function getDirectoryTreeItemLabels(directoryTree) {
  let labels = [];
  for (let item of directoryTree.items)
    labels.push(item.label);
  return labels;
}

/**
 * Test case for typical creation of directory tree.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTree(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 3 entries.
        return driveItem.items.length == 3;
      }).then(function() {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[1].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[2].label);
      }),
      callback);
}

/**
 * Test case for creating tree with Team Drives.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Team Drives (only if there is a child team drive).
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithTeamDrive(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/a');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 4;
      }).then(function() {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(str('DRIVE_TEAM_DRIVES_LABEL'), driveItem.items[1].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[2].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[3].label);
      }),
      callback);
}

/**
 * Test case for creating tree with empty Team Drives.
 * The Team Drives subtree should be removed if the user has no team drives.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithEmptyTeamDrive(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Root entries under Drive volume is generated, Team Drives isn't
        // included because it has no child.
        // See testCreateDirectoryTreeWithTeamDrive for detail.
        return driveItem.items.length == 3;
      }).then(function() {
        var teamDrivesItemFound = false;
        for (var i = 0; i < driveItem.items.length; i++) {
          if (driveItem.items[i].label == str('DRIVE_TEAM_DRIVES_LABEL')) {
            teamDrivesItemFound = true;
            break;
          }
        }
        assertFalse(teamDrivesItemFound, 'Team Drives should NOT be generated');
      }),
      callback);
}

/**
 * Test case for creating tree with Computers.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Computers (only if there is a child computer).
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithComputers(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  fakeFileSystemURLEntries['filesystem:drive/Comuters/My Laptop'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers/My Laptop');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 4;
      }).then(function() {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(str('DRIVE_COMPUTERS_LABEL'), driveItem.items[1].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[2].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[3].label);
      }),
      callback);
}

/**
 * Test case for creating tree with empty Computers.
 * The Computers subtree should be removed if the user has no computers.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithEmptyComputers(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  const driveItem = directoryTree.items[0];

  // Ensure we do not have a "Computers" item in drive, as it does not contain
  // any children.
  reportPromise(
      waitUntil(function() {
        // Root entries under Drive volume is generated, Computers isn't
        // included because it has no child.
        // See testCreateDirectoryTreeWithComputers for detail.
        return driveItem.items.length == 3;
      }).then(function() {
        let teamDrivesItemFound = false;
        for (let i = 0; i < driveItem.items.length; i++) {
          if (driveItem.items[i].label == str('DRIVE_COMPUTERS_LABEL')) {
            teamDrivesItemFound = true;
            break;
          }
        }
        assertFalse(teamDrivesItemFound, 'Computers should NOT be generated');
      }),
      callback);
}

/**
 * Test case for creating tree with Team Drives & Computers.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Team Drives
 * - Computers
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithTeamDrivesAndComputers(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  fakeFileSystemURLEntries ['filesystem:drive/team_drives/a'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/a');
  fakeFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  fakeFileSystemURLEntries['filesystem:drive/Comuters/My Laptop'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers/My Laptop');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 5;
      }).then(function() {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(str('DRIVE_TEAM_DRIVES_LABEL'), driveItem.items[1].label);
        assertEquals(str('DRIVE_COMPUTERS_LABEL'), driveItem.items[2].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[3].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[4].label);
      }),
      callback);
}

/**
 * Test case for updateSubElementsFromList setting section-start attribute.
 *
 * 'section-start' attribute is used to display a line divider between
 * "sections" in the directory tree. This is calculated in NavigationListModel.
 */
function testUpdateSubElementsFromListSections() {
  const recentItem = null;
  const treeModel = new NavigationListModel(
      volumeManager, new MockFolderShortcutDataModel([]), recentItem,
      new MockDirectoryModel());
  const myFilesItem = treeModel.item(0);
  const driveItem = treeModel.item(1);

  assertEquals(NavigationSection.MY_FILES, myFilesItem.section);
  assertEquals(NavigationSection.CLOUD, driveItem.section);

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  directoryTree.dataModel = treeModel;

  // Coerce to DirectoryTree type and update the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.updateSubElementsFromList(false);

  // First element should not have section-start attribute, to not display a
  // division line in the first section.
  // My files:
  assertEquals(null, directoryTree.items[0].getAttribute('section-start'));

  // Drive should have section-start, because it's a new section but not the
  // first section.
  assertEquals(
      NavigationSection.CLOUD,
      directoryTree.items[1].getAttribute('section-start'));

  // Regenerate so it re-calculates the 'section-start' without creating the
  // DirectoryItem.
  directoryTree.updateSubElementsFromList(false);
  assertEquals(
      NavigationSection.CLOUD,
      directoryTree.items[1].getAttribute('section-start'));
}

/**
 * Test case for updateSubElementsFromList.
 *
 * Mounts/unmounts removable and archive volumes, and checks these volumes come
 * up to/disappear from the list correctly.
 */
function testUpdateSubElementsFromList() {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and update the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.updateSubElementsFromList(true);

  // There are 2 volumes, Drive and Downloads, at first.
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabels(directoryTree));

  // Mounts a removable volume.
  var removableVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable',
      str('REMOVABLE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(removableVolume);

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabels(directoryTree));

  // Asserts that a removable directory is added after the update.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL'),
    str('REMOVABLE_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabels(directoryTree));

  // Mounts an archive volume.
  var archiveVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive',
      str('ARCHIVE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(archiveVolume);

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL'),
    str('REMOVABLE_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabels(directoryTree));

  // Asserts that an archive directory is added before the removable directory.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
        str('ARCHIVE_DIRECTORY_LABEL'),
      ],
      getDirectoryTreeItemLabels(directoryTree));

  // Deletes an archive directory.
  volumeManager.volumeInfoList.remove('archive');

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
        str('ARCHIVE_DIRECTORY_LABEL'),
      ],
      getDirectoryTreeItemLabels(directoryTree));

  // Asserts that an archive directory is deleted.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL'),
    str('REMOVABLE_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabels(directoryTree));
}

/**
 * Test adding the first team drive for a user.
 * Team Drives subtree should be shown after the change notification is
 * delivered.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testAddFirstTeamDrive(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 3;
      })
          .then(() => {
            fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
                new MockDirectoryEntry(driveFileSystem, '/team_drives/a');
            let event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/team_drives'],
              eventType: 'changed',
            };
            for (let listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            return waitUntil(() => {
              for (var i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label ==
                    str('DRIVE_TEAM_DRIVES_LABEL')) {
                  return !driveItem.items[i].hidden;
                }
              }
              return false;
            });
          }),
      callback);
}

/**
 * Test removing the last team drive for a user.
 * Team Drives subtree should be removed after the change notification is
 * delivered.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testRemoveLastTeamDrive(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/a');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 4;
      })
          .then(() => {
            return new Promise(resolve => {
              fakeFileSystemURLEntries['filesystem:drive/team_drives/a']
                  .remove(resolve);
            });
          })
          .then(() => {
            let event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/team_drives'],
              eventType: 'changed',
            };
            for (let listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            // Wait team drive grand root to appear.
            return waitUntil(() => {
              for (var i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label ==
                    str('DRIVE_TEAM_DRIVES_LABEL')) {
                  return false;
                }
              }
              return true;
            });
          }),
      callback);
}

/**
 * Test adding the first computer for a user.
 * Computers subtree should be shown after the change notification is
 * delivered.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testAddFirstComputer(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  // No directories exist under Computers

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  let driveItem = directoryTree.items[0];

  // Test that we initially do not have a Computers item under Drive, and that
  // adding a filesystem "/Computers/a" results in the Computers item being
  // displayed under Drive.

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 3;
      })
          .then(() => {
            fakeFileSystemURLEntries['filesystem:drive/Computers/a'] =
                new MockDirectoryEntry(driveFileSystem, '/Computers/a');
            let event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/Computers'],
              eventType: 'changed',
            };
            for (let listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            return waitUntil(() => {
              for (let i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label == str('DRIVE_COMPUTERS_LABEL')) {
                  return !driveItem.items[i].hidden;
                }
              }
              return false;
            });
          }),
      callback);
}

/**
 * Test removing the last computer for a user.
 * Computerss subtree should be removed after the change notification is
 * delivered.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testRemoveLastComputer(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  fakeFileSystemURLEntries['filesystem:drive/Computers/a'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers/a');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  const driveItem = directoryTree.items[0];

  // Check that removing the local computer "a" results in the entire
  // "Computers" element being removed, as it has no children.
  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 4;
      })
          .then(() => {
            return new Promise(resolve => {
              fakeFileSystemURLEntries['filesystem:drive/Computers/a']
                  .remove(resolve);
            });
          })
          .then(() => {
            let event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/Computers'],
              eventType: 'changed',
            };
            for (let listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            // Wait team drive grand root to appear.
            return waitUntil(() => {
              for (let i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label == str('DRIVE_COMPUTERS_LABEL')) {
                  return false;
                }
              }
              return true;
            });
          }),
      callback);
}

/**
 * Test DirectoryItem.insideMyDrive property, which should return true when
 * inside My Drive and any of its sub-directories; Should return false for
 * everything else, including within Team Drive.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testInsideMyDriveAndInsideDrive(callback) {
  // Setup My Drive and Downloads and one folder inside each of them.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  fakeFileSystemURLEntries['filesystem:drive/root/folder1'] =
      new MockDirectoryEntry(driveFileSystem, '/root/folder1');
  const downloadsFileSystem = volumeManager.volumeInfoList.item(1).fileSystem;
  fakeFileSystemURLEntries['filesystem:downloads/folder1'] =
      new MockDirectoryEntry(downloadsFileSystem, '/folder1');

  // Populate the directory tree with the mock filesystem.
  var directoryTree = createElements();
  directoryTree.metadataModel = createMockMetadataModel();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  directoryTree.redraw(true);

  const driveItem = directoryTree.items[0];
  const downloadsItem = directoryTree.items[1];

  reportPromise(
      waitUntil(() => {
        // Under the drive item, there exist 3 entries. In Downloads should
        // exist 1 entry folder1.
        return driveItem.items.length === 3 && downloadsItem.items.length === 1;
      }).then(() => {
        // insideMyDrive
        assertTrue(driveItem.insideMyDrive, 'Drive root');
        assertTrue(driveItem.items[0].insideMyDrive, 'My Drive root');
        assertFalse(driveItem.items[1].insideMyDrive, 'Team Drives root');
        assertFalse(driveItem.items[2].insideMyDrive, 'Offline root');
        assertFalse(downloadsItem.insideMyDrive, 'Downloads root');
        assertFalse(downloadsItem.items[0].insideMyDrive, 'Downloads/folder1');
        // insideDrive
        assertTrue(driveItem.insideDrive, 'Drive root');
        assertTrue(driveItem.items[0].insideDrive, 'My Drive root');
        assertTrue(driveItem.items[1].insideDrive, 'Team Drives root');
        assertTrue(driveItem.items[2].insideDrive, 'Offline root');
        assertFalse(downloadsItem.insideDrive, 'Downloads root');
        assertFalse(downloadsItem.items[0].insideDrive, 'Downloads/folder1');
      }),
      callback);
}

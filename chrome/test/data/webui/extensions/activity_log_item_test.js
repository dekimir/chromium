// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-item. */
suite('ExtensionsActivityLogItemTest', function() {
  /**
   * Extension activityLogItem created before each test.
   * @type {extensions.ActivityLogItem}
   */
  let activityLogItem;
  let testVisible;

  /**
   * ApiGroup data for the activityLogItem
   * @type {extensions.ApiGroup}
   */
  let testApiGroup;

  // Initialize an extension activity log item before each test.
  setup(function() {
    PolymerTest.clearBody();
    testApiGroup = {
      apiCall: 'i18n.getUILanguage',
      count: 1,
      activityType: chrome.activityLogPrivate.ExtensionActivityFilter.API_CALL,
      countsByUrl: new Map()
    };

    activityLogItem = new extensions.ActivityLogItem();
    activityLogItem.data = testApiGroup;
    testVisible = extension_test_util.testVisible.bind(null, activityLogItem);

    document.body.appendChild(activityLogItem);
  });

  teardown(function() {
    activityLogItem.remove();
  });

  test('no page urls shown when activity has no associated page', function() {
    Polymer.dom.flush();

    testVisible('#activity-call-and-count', true);
    testVisible('#page-url-list', false);
  });

  test('count not shown when there is only 1 page url', function() {
    const countsByUrl = new Map([['google.com', 1]]);

    testApiGroup = {
      apiCall: 'Storage.getItem',
      count: 3,
      activityType:
          chrome.activityLogPrivate.ExtensionActivityFilter.DOM_ACCESS,
      countsByUrl
    };
    activityLogItem.set('data', testApiGroup);

    Polymer.dom.flush();

    testVisible('#activity-call-and-count', true);
    testVisible('#page-url-list', true);
    testVisible('.page-url-count', false);
  });

  test('count shown in descending order for multiple page urls', function() {
    const countsByUrl =
        new Map([['google.com', 5], ['chrome://extensions', 10]]);

    testApiGroup = {
      apiCall: 'Storage.getItem',
      count: 15,
      activityType:
          chrome.activityLogPrivate.ExtensionActivityFilter.DOM_ACCESS,
      countsByUrl
    };
    activityLogItem.set('data', testApiGroup);

    Polymer.dom.flush();

    testVisible('#activity-call-and-count', true);
    testVisible('#page-url-list', true);
    testVisible('.page-url-count', true);

    const pageUrls = activityLogItem.shadowRoot.querySelectorAll('.page-url');
    expectEquals(pageUrls.length, 2);

    // Test the order of the page urls and activity count for the activity
    // log item here. Apparently a space is added at the end of the innerText,
    // hence the use of .includes.
    expectTrue(pageUrls[0]
                   .querySelector('.page-url-link')
                   .innerText.includes('chrome://extensions'));
    expectEquals(pageUrls[0].querySelector('.page-url-count').innerText, '10');

    expectTrue(pageUrls[1]
                   .querySelector('.page-url-link')
                   .innerText.includes('google.com'));
    expectEquals(pageUrls[1].querySelector('.page-url-count').innerText, '5');
  });
});

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-chrome-app-permission-view',

  properties: {
    /**
     * @type {appManagement.mojom.App}
     * @private
     */
    app_: {
      type: Object,
      value: function() {
        return app_management.FakePageHandler.createApp(
            'ahfgeienlihckogmohjhadlkjgocpleb');
      },
    },
  },

  /**
   * @param {appManagement.mojom.App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },
});

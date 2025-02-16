// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

cr.define('app_management', function() {
  const AppState = {};

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.addApps = function(apps, action) {
    const newAppEntries = {};
    for (const app of action.apps) {
      newAppEntries[app.id] = app;
    }
    return Object.assign({}, apps, newAppEntries);
  };

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.changeApp = function(apps, action) {
    assert(apps[action.update.id]);

    const changedAppEntry = {};
    changedAppEntry[action.update.id] = action.update;
    return Object.assign({}, apps, changedAppEntry);
  };

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.removeApp = function(apps, action) {
    assert(apps[action.id]);

    delete apps[action.id];
    return Object.assign({}, apps);
  };

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.updateApps = function(apps, action) {
    switch (action.name) {
      case 'add-apps':
        return AppState.addApps(apps, action);
      case 'change-app':
        return AppState.changeApp(apps, action);
      case 'remove-app':
        return AppState.removeApp(apps, action);
      default:
        return apps;
    }
  };

  const CurrentPageState = {};

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {Page}
   */
  CurrentPageState.changePage = function(apps, action) {
    if (action.pageType == PageType.DETAIL && apps[action.id]) {
      return {
        pageType: PageType.DETAIL,
        selectedAppId: action.id,
      };
    } else {
      return {
        pageType: PageType.MAIN,
        selectedAppId: null,
      };
    }
  };

  /**
   * @param {AppMap} apps
   * @param {Page} currentPage
   * @param {Object} action
   * @return {Page}
   */
  CurrentPageState.updateCurrentPage = function(apps, currentPage, action) {
    switch (action.name) {
      case 'change-page':
        return CurrentPageState.changePage(apps, action);
      default:
        return currentPage;
    }
  };

  /**
   * Root reducer for the App Management page. This is called by the store in
   * response to an action, and the return value is used to update the UI.
   * @param {!AppManagementPageState} state
   * @param {Object} action
   * @return {!AppManagementPageState}
   */
  function reduceAction(state, action) {
    return {
      apps: AppState.updateApps(state.apps, action),
      currentPage: CurrentPageState.updateCurrentPage(
          state.apps, state.currentPage, action),
    };
  }

  return {
    reduceAction: reduceAction,
    AppState: AppState,
    CurrentPageState: CurrentPageState,
  };
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_

#include <stddef.h>


@protocol CRWWebViewNavigationProxy;
@class WKBackForwardListItem;

namespace web {

struct LoadCommittedDetails;
enum class NavigationInitiationType;
class NavigationItem;
class WebState;

// Delegate for NavigationManager to hand off parts of the navigation flow.
class NavigationManagerDelegate {
 public:
  virtual ~NavigationManagerDelegate() {}

  // Instructs the delegate to clear any transient content to prepare for new
  // navigation.
  virtual void ClearTransientContent() = 0;

  // Instructs the delegate to record page states (e.g. scroll position, form
  // values, whatever can be harvested) from the current page into the
  // navigation item.
  virtual void RecordPageStateInNavigationItem() = 0;

  // Informs the delegate that a go to index same-document navigation occured.
  virtual void OnGoToIndexSameDocumentNavigation(NavigationInitiationType type,
                                                 bool has_user_gesture) = 0;

  // Instructs the delegate to perform book keeping in preparation for a new
  // navigation using a different user agent type.
  virtual void WillChangeUserAgentType() = 0;

  // Instructs the delegate to load the current navigation item.
  virtual void LoadCurrentItem() = 0;

  // Instructs the delegate to load the current navigation item if the current
  // page has not loaded yet.
  virtual void LoadIfNecessary() = 0;

  // Instructs the delegate to reload.
  virtual void Reload() = 0;

  // Informs the delegate that committed navigation items have been pruned.
  virtual void OnNavigationItemsPruned(size_t pruned_item_count) = 0;

  // Informs the delegate that a navigation item has been committed.
  virtual void OnNavigationItemCommitted(
      const LoadCommittedDetails& load_details) = 0;

  // Returns the WebState associated with this delegate.
  virtual WebState* GetWebState() = 0;

  // Returns a CRWWebViewNavigationProxy protocol that can be used to access
  // navigation related functions on the main WKWebView.
  virtual id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const = 0;

  // Instructs WKWebView to navigate to the given navigation item. |wk_item| and
  // |item| must point to the same navigation item. Calling this method may
  // result in an iframe navigation.
  virtual void GoToBackForwardListItem(WKBackForwardListItem* wk_item,
                                       NavigationItem* item,
                                       NavigationInitiationType type,
                                       bool has_user_gesture) = 0;

  // Instructs the delegate to remove the underlying web view. The only use case
  // currently is to clear back-forward history in web view before restoring
  // session history.
  virtual void RemoveWebView() = 0;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_

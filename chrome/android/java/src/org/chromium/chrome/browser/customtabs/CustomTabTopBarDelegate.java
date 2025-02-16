// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

/**
 * Delegate that manages top bar area inside of {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabTopBarDelegate {
    private final ChromeActivity mActivity;
    private ViewGroup mTopBarView;
    @Nullable
    private View mTopBarContentView;
    @Nullable
    private Integer mTopBarHeight;

    @Inject
    public CustomTabTopBarDelegate(ChromeActivity activity) {
        mActivity = activity;
    }

    /**
     * Adds the top bar, if any, to the view hierarchy and updates its visibility.
     */
    public void showTopBarIfNecessary(boolean isVisible) {
        if (mTopBarContentView != null && mTopBarContentView.getParent() == null) {
            getTopBarView().addView(mTopBarContentView);
        }
        if (mTopBarContentView != null) {
            mTopBarContentView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Sets the content of the top bar.
     */
    public void setTopBarContentView(View view) {
        mTopBarContentView = view;
    }

    /**
     * Sets the height of the top bar.
     */
    public void setTopBarHeight(int height) {
        mTopBarHeight = height;
    }

    /**
     * Gets the height of the top bar, or null if it is not specified.
     */
    @Nullable
    public Integer getTopBarHeight() {
        return mTopBarHeight;
    }

    /**
     * Gets the {@link ViewGroup} of the top bar. If it has not been inflated, inflate it first.
     */
    private ViewGroup getTopBarView() {
        if (mTopBarView == null) {
            ViewStub topBarStub = ((ViewStub) mActivity.findViewById(R.id.topbar_stub));
            mTopBarView = (ViewGroup) topBarStub.inflate();
        }
        return mTopBarView;
    }
}

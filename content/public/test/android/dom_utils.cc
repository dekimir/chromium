// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "jni/DOMUtils_jni.h"
#include "ui/android/view_android.h"

using base::android::JavaParamRef;

namespace content {

// Returns the amount of the top controls height if controls are in the state
// of shrinking Blink's view size, otherwise 0.
jint JNI_DOMUtils_GetTopControlsShrinkBlinkHeight(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);

  // Android obtains the top control size via WebContentsDelegate.
  WebContentsDelegate* delegate = web_contents->GetDelegate();
  float scale = web_contents->GetNativeView()->GetDipScale();
  return delegate && delegate->DoBrowserControlsShrinkRendererSize(web_contents)
             ? delegate->GetTopControlsHeight() * scale
             : 0;
}

}  // namespace content

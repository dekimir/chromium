// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"

#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

XRViewerPose::XRViewerPose(
    XRSession* session,
    std::unique_ptr<TransformationMatrix> pose_model_matrix)
    : session_(session), pose_model_matrix_(std::move(pose_model_matrix)) {}

DOMFloat32Array* XRViewerPose::poseModelMatrix() const {
  if (!pose_model_matrix_)
    return nullptr;
  return transformationMatrixToDOMFloat32Array(*pose_model_matrix_);
}

DOMFloat32Array* XRViewerPose::getViewMatrix(XRView* view) {
  if (view->session() != session_)
    return nullptr;

  if (!pose_model_matrix_->IsInvertible())
    return nullptr;

  TransformationMatrix view_matrix(pose_model_matrix_->Inverse());

  // Transform by the negative offset, since we're operating on the inverted
  // matrix
  const FloatPoint3D& view_offset = view->offset();
  view_matrix.PostTranslate3d(-view_offset.X(), -view_offset.Y(),
                              -view_offset.Z());

  return transformationMatrixToDOMFloat32Array(view_matrix);
}

void XRViewerPose::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"

namespace features {

// Enables running draw occlusion algorithm to remove Draw Quads that are not
// shown on screen from CompositorFrame.
const base::Feature kEnableDrawOcclusion{"DrawOcclusion",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(USE_AURA) || defined(OS_MACOSX)
const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables running the display compositor as part of the viz service in the GPU
// process. This is also referred to as out-of-process display compositor
// (OOP-D).
#if defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
const base::Feature kVizDisplayCompositor{"VizDisplayCompositor",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kVizDisplayCompositor{"VizDisplayCompositor",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables running the Viz-assisted hit-test logic.
const base::Feature kEnableVizHitTestDrawQuad{"VizHitTestDrawQuad",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableVizHitTestSurfaceLayer{
    "VizHitTestSurfaceLayer", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer to record SkPicture.
const base::Feature kRecordSkPicture{"RecordSkPicture",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSurfaceSynchronizationEnabled() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return base::FeatureList::IsEnabled(kEnableSurfaceSynchronization) ||
         command_line->HasSwitch(switches::kEnableSurfaceSynchronization) ||
         base::FeatureList::IsEnabled(kVizDisplayCompositor);
}

bool IsVizHitTestingDebugEnabled() {
  return features::IsVizHitTestingEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableVizHitTestDebug);
}

bool IsVizHitTestingDrawQuadEnabled() {
  if (IsVizHitTestingSurfaceLayerEnabled())
    return false;
  return base::FeatureList::IsEnabled(kEnableVizHitTestDrawQuad) ||
         base::FeatureList::IsEnabled(kVizDisplayCompositor);
}

bool IsVizHitTestingEnabled() {
  return IsVizHitTestingDrawQuadEnabled() ||
         IsVizHitTestingSurfaceLayerEnabled();
}

bool IsVizHitTestingSurfaceLayerEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kUseVizHitTestSurfaceLayer) ||
         base::FeatureList::IsEnabled(kEnableVizHitTestSurfaceLayer);
}

bool IsDrawOcclusionEnabled() {
  return base::FeatureList::IsEnabled(kEnableDrawOcclusion);
}

bool IsUsingSkiaRenderer() {
  return base::FeatureList::IsEnabled(kUseSkiaRenderer);
}

bool IsRecordingSkPicture() {
  return IsUsingSkiaRenderer() &&
         base::FeatureList::IsEnabled(kRecordSkPicture);
}

}  // namespace features

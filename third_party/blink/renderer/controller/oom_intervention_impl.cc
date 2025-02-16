// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

// static
void OomInterventionImpl::Create(mojom::blink::OomInterventionRequest request) {
  mojo::MakeStrongBinding(std::make_unique<OomInterventionImpl>(),
                          std::move(request));
}

OomInterventionImpl::OomInterventionImpl()
    : timer_(Thread::MainThread()->GetTaskRunner(),
             this,
             &OomInterventionImpl::Check),
      delayed_report_timer_(Thread::MainThread()->GetTaskRunner(),
                            this,
                            &OomInterventionImpl::TimerFiredUMAReport) {}

OomInterventionImpl::~OomInterventionImpl() {}

void OomInterventionImpl::StartDetection(
    mojom::blink::OomInterventionHostPtr host,
    mojom::blink::DetectionArgsPtr detection_args,
    bool renderer_pause_enabled,
    bool navigate_ads_enabled) {
  host_ = std::move(host);

  // Disable intervention if we cannot get memory details of current process.
  if (CrashMemoryMetricsReporterImpl::Instance().ResetFileDiscriptors())
    return;

  detection_args_ = std::move(detection_args);
  renderer_pause_enabled_ = renderer_pause_enabled;
  navigate_ads_enabled_ = navigate_ads_enabled;

  timer_.Start(TimeDelta(), TimeDelta::FromSeconds(1), FROM_HERE);
}

OomInterventionMetrics OomInterventionImpl::GetCurrentMemoryMetrics() {
  return CrashMemoryMetricsReporterImpl::Instance().GetCurrentMemoryMetrics();
}

void OomInterventionImpl::Check(TimerBase*) {
  DCHECK(host_);

  OomInterventionMetrics current_memory = GetCurrentMemoryMetrics();
  bool oom_detected = false;

  oom_detected |= detection_args_->blink_workload_threshold > 0 &&
                  current_memory.current_blink_usage_kb * 1024 >
                      detection_args_->blink_workload_threshold;
  oom_detected |= detection_args_->private_footprint_threshold > 0 &&
                  current_memory.current_private_footprint_kb * 1024 >
                      detection_args_->private_footprint_threshold;
  oom_detected |=
      detection_args_->swap_threshold > 0 &&
      current_memory.current_swap_kb * 1024 > detection_args_->swap_threshold;
  oom_detected |= detection_args_->virtual_memory_thresold > 0 &&
                  current_memory.current_vm_size_kb * 1024 >
                      detection_args_->virtual_memory_thresold;

  // Report memory stats every second to send UMA.
  ReportMemoryStats(current_memory);

  if (oom_detected) {
    if (navigate_ads_enabled_) {
      for (const auto& page : Page::OrdinaryPages()) {
        if (page->MainFrame()->IsLocalFrame()) {
          ToLocalFrame(page->MainFrame())
              ->GetDocument()
              ->NavigateLocalAdsFrames();
        }
      }
    }

    if (renderer_pause_enabled_) {
      // The ScopedPagePauser is destroyed when the intervention is declined and
      // mojo strong binding is disconnected.
      pauser_.reset(new ScopedPagePauser);
    }
    host_->OnHighMemoryUsage();
    timer_.Stop();
    // Notify V8GCForContextDispose that page navigation gc is needed when
    // intervention runs, as it indicates that memory usage is high.
    V8GCForContextDispose::Instance().SetForcePageNavigationGC();

    // Report the memory impact of intervention after 10, 20, 30 seconds.
    metrics_at_intervention_ = current_memory;
    number_of_report_needed_ = 3;
    delayed_report_timer_.StartRepeating(TimeDelta::FromSeconds(10), FROM_HERE);
  }
}

void OomInterventionImpl::ReportMemoryStats(
    OomInterventionMetrics& current_memory) {
  UMA_HISTOGRAM_MEMORY_MB(
      "Memory.Experimental.OomIntervention.RendererBlinkUsage",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_blink_usage_kb / 1024));
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.OomIntervention."
      "RendererPrivateMemoryFootprint",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_private_footprint_kb / 1024));
  UMA_HISTOGRAM_MEMORY_MB(
      "Memory.Experimental.OomIntervention.RendererSwapFootprint",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_swap_kb / 1024));
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.OomIntervention.RendererVmSize",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_vm_size_kb / 1024));

  CrashMemoryMetricsReporterImpl::Instance().WriteIntoSharedMemory(
      current_memory);
}

void OomInterventionImpl::TimerFiredUMAReport(TimerBase*) {
  OomInterventionMetrics current_memory = GetCurrentMemoryMetrics();
  switch (number_of_report_needed_--) {
    case 3:
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter10secs",
          base::saturated_cast<base::Histogram::Sample>(
              current_memory.current_blink_usage_kb / 1024 -
              metrics_at_intervention_.current_blink_usage_kb / 1024));
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter10secs",
          base::saturated_cast<base::Histogram::Sample>(
              current_memory.current_private_footprint_kb / 1024 -
              metrics_at_intervention_.current_private_footprint_kb / 1024));
      break;
    case 2:
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter20secs",
          base::saturated_cast<base::Histogram::Sample>(
              current_memory.current_blink_usage_kb / 1024 -
              metrics_at_intervention_.current_blink_usage_kb / 1024));
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter20secs",
          base::saturated_cast<base::Histogram::Sample>(
              current_memory.current_private_footprint_kb / 1024 -
              metrics_at_intervention_.current_private_footprint_kb / 1024));
      break;
    case 1:
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter30secs",
          base::saturated_cast<base::Histogram::Sample>(
              current_memory.current_blink_usage_kb / 1024 -
              metrics_at_intervention_.current_blink_usage_kb / 1024));
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter30secs",
          base::saturated_cast<base::Histogram::Sample>(
              current_memory.current_private_footprint_kb / 1024 -
              metrics_at_intervention_.current_private_footprint_kb / 1024));
      delayed_report_timer_.Stop();
      break;
  }
}

}  // namespace blink

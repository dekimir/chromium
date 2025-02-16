// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_constants.h"
#include "url/gurl.h"

namespace previews {

PreviewsOptimizationGuide::PreviewsOptimizationGuide(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : optimization_guide_service_(optimization_guide_service),
      ui_task_runner_(ui_task_runner),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      ui_weak_ptr_factory_(this) {
  DCHECK(optimization_guide_service_);
  optimization_guide_service_->AddObserver(this);
}

PreviewsOptimizationGuide::~PreviewsOptimizationGuide() {
  optimization_guide_service_->RemoveObserver(this);
}

bool PreviewsOptimizationGuide::IsWhitelisted(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type,
    net::EffectiveConnectionType* out_ect_threshold) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!hints_)
    return false;

  *out_ect_threshold = params::GetECTThresholdForPreview(type);
  int inflation_percent = 0;
  if (!hints_->IsWhitelisted(url, type, &inflation_percent, out_ect_threshold))
    return false;

  if (inflation_percent != 0 && previews_data)
    previews_data->set_data_savings_inflation_percent(inflation_percent);

  return true;
}

bool PreviewsOptimizationGuide::IsBlacklisted(const GURL& url,
                                              PreviewsType type) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!hints_)
    return false;

  return hints_->IsBlacklisted(url, type);
}

void PreviewsOptimizationGuide::OnLoadedHint(
    const GURL& document_url,
    const optimization_guide::proto::Hint& loaded_hint) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // TODO(dougarnett): Consider UMA to capture here else drop this callback.
}

bool PreviewsOptimizationGuide::MaybeLoadOptimizationHints(const GURL& url) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!hints_)
    return false;

  return hints_->MaybeLoadOptimizationHints(
      url, base::BindOnce(&PreviewsOptimizationGuide::OnLoadedHint,
                          ui_weak_ptr_factory_.GetWeakPtr(), url));
}

bool PreviewsOptimizationGuide::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!hints_)
    return false;

  return hints_->GetResourceLoadingHints(url, out_resource_patterns_to_block);
}

void PreviewsOptimizationGuide::LogHintCacheMatch(
    const GURL& url,
    bool is_committed,
    net::EffectiveConnectionType ect) const {
  if (!hints_)
    return;

  hints_->LogHintCacheMatch(url, is_committed, ect);
}

void PreviewsOptimizationGuide::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PreviewsHints::CreateFromHintsComponent, info),
      base::BindOnce(&PreviewsOptimizationGuide::UpdateHints,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsOptimizationGuide::UpdateHints(
    std::unique_ptr<PreviewsHints> hints) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  hints_ = std::move(hints);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      kPreviewsOptimizationGuideUpdateHintsResultHistogramString,
      hints_ != NULL);
}

}  // namespace previews

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/leak_detector.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"

namespace content {

// The initial states of the DOM objects at about:blank. The four nodes are a
// Document, a HTML, a HEAD and a BODY.
//
// TODO(hajimehoshi): Now these are hard-corded. If we add a target to count
// objects like RefCounted whose initial state is diffcult to estimate, we stop
// using hard-coded values. Instead, we need to load about:blank ahead of the
// web tests actually and initialize LeakDetector by the got values.
const int kInitialNumberOfLiveAudioNodes = 0;
const int kInitialNumberOfLiveDocuments = 1;
const int kInitialNumberOfLiveNodes = 4;
const int kInitialNumberOfLiveLayoutObjects = 3;
const int kInitialNumberOfLiveResources = 0;
const int kInitialNumberOfScriptPromises = 0;
const int kInitialNumberOfLiveFrames = 1;
const int kInitialNumberOfWorkerGlobalScopes = 0;
const int kInitialNumberOfLiveResourceFetchers = 1;

// In the initial state, there are two PausableObjects (FontFaceSet created
// by HTMLDocument and PausableTimer created by DocumentLoader).
const int kInitialNumberOfLivePausableObject = 2;

// This includes not only about:blank's context but also ScriptRegexp (e.g.
// created by isValidEmailAddress in EmailInputType.cpp). The leak detector
// always creates the latter to stabilize the number of V8PerContextData
// objects.
const int kInitialNumberOfV8PerContextData = 2;

LeakDetector::LeakDetector() : weak_factory_(this) {
  previous_result_ = blink::mojom::LeakDetectionResult::New();
  previous_result_->number_of_live_audio_nodes = kInitialNumberOfLiveAudioNodes;
  previous_result_->number_of_live_documents = kInitialNumberOfLiveDocuments;
  previous_result_->number_of_live_nodes = kInitialNumberOfLiveNodes;
  previous_result_->number_of_live_layout_objects =
      kInitialNumberOfLiveLayoutObjects;
  previous_result_->number_of_live_resources = kInitialNumberOfLiveResources;
  previous_result_->number_of_live_pausable_objects =
      kInitialNumberOfLivePausableObject;
  previous_result_->number_of_live_script_promises =
      kInitialNumberOfScriptPromises;
  previous_result_->number_of_live_frames = kInitialNumberOfLiveFrames;
  previous_result_->number_of_live_v8_per_context_data =
      kInitialNumberOfV8PerContextData;
  previous_result_->number_of_worker_global_scopes =
      kInitialNumberOfWorkerGlobalScopes;
  previous_result_->number_of_live_resource_fetchers =
      kInitialNumberOfLiveResourceFetchers;
}

LeakDetector::~LeakDetector() {}

void LeakDetector::TryLeakDetection(RenderProcessHost* process,
                                    ReportCallback callback) {
  callback_ = std::move(callback);

  BindInterface(process, &leak_detector_);
  leak_detector_.set_connection_error_handler(base::BindOnce(
      &LeakDetector::OnLeakDetectorIsGone, base::Unretained(this)));
  leak_detector_->PerformLeakDetection(base::BindOnce(
      &LeakDetector::OnLeakDetectionComplete, weak_factory_.GetWeakPtr()));
}

void LeakDetector::OnLeakDetectionComplete(
    blink::mojom::LeakDetectionResultPtr result) {
  LeakDetectionReport report;
  report.leaked = false;
  base::DictionaryValue detail;

  if (previous_result_) {
    if (previous_result_->number_of_live_audio_nodes <
        result->number_of_live_audio_nodes) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_audio_nodes);
      list->AppendInteger(result->number_of_live_audio_nodes);
      detail.Set("numberOfLiveAudioNodes", std::move(list));
    }
    if (previous_result_->number_of_live_documents <
        result->number_of_live_documents) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_documents);
      list->AppendInteger(result->number_of_live_documents);
      detail.Set("numberOfLiveDocuments", std::move(list));
    }
    if (previous_result_->number_of_live_nodes < result->number_of_live_nodes) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_nodes);
      list->AppendInteger(result->number_of_live_nodes);
      detail.Set("numberOfLiveNodes", std::move(list));
    }
    if (previous_result_->number_of_live_layout_objects <
        result->number_of_live_layout_objects) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_layout_objects);
      list->AppendInteger(result->number_of_live_layout_objects);
      detail.Set("numberOfLiveLayoutObjects", std::move(list));
    }
    // Resources associated with User Agent CSS should be excluded from leak
    // detection as they are persisted through page navigation.
    if (previous_result_->number_of_live_resources <
        (result->number_of_live_resources -
         result->number_of_live_ua_css_resources)) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_resources);
      list->AppendInteger(result->number_of_live_resources);
      detail.Set("numberOfLiveResources", std::move(list));
    }
    if (previous_result_->number_of_live_pausable_objects <
        result->number_of_live_pausable_objects) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_pausable_objects);
      list->AppendInteger(result->number_of_live_pausable_objects);
      detail.Set("numberOfLivePausableObjects", std::move(list));
    }
    if (previous_result_->number_of_live_script_promises <
        result->number_of_live_script_promises) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_script_promises);
      list->AppendInteger(result->number_of_live_script_promises);
      detail.Set("numberOfLiveScriptPromises", std::move(list));
    }
    if (previous_result_->number_of_live_frames <
        result->number_of_live_frames) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_frames);
      list->AppendInteger(result->number_of_live_frames);
      detail.Set("numberOfLiveFrames", std::move(list));
    }
    if (previous_result_->number_of_live_v8_per_context_data <
        result->number_of_live_v8_per_context_data) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_v8_per_context_data);
      list->AppendInteger(result->number_of_live_v8_per_context_data);
      detail.Set("numberOfLiveV8PerContextData", std::move(list));
    }
    if (previous_result_->number_of_worker_global_scopes <
        result->number_of_worker_global_scopes) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_worker_global_scopes);
      list->AppendInteger(result->number_of_worker_global_scopes);
      detail.Set("numberOfWorkerGlobalScopes", std::move(list));
    }
    if (previous_result_->number_of_live_resource_fetchers <
        result->number_of_live_resource_fetchers) {
      auto list = std::make_unique<base::ListValue>();
      list->AppendInteger(previous_result_->number_of_live_resource_fetchers);
      list->AppendInteger(result->number_of_live_resource_fetchers);
      detail.Set("numberOfLiveResourceFetchers", std::move(list));
    }
  }

  if (!detail.empty()) {
    std::string detail_str;
    base::JSONWriter::Write(detail, &detail_str);
    report.detail = detail_str;
    report.leaked = true;
  }

  previous_result_ = std::move(result);
  leak_detector_.reset();
  std::move(callback_).Run(report);
}

void LeakDetector::OnLeakDetectorIsGone() {
  LeakDetectionReport report;
  report.leaked = false;
  std::move(callback_).Run(report);
}

}  // namespace content

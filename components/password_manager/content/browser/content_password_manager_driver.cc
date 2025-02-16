// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/form_submission_tracker_util.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/page_transition_types.h"

namespace password_manager {

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::RenderFrameHost* render_frame_host,
    PasswordManagerClient* client,
    autofill::AutofillClient* autofill_client)
    : render_frame_host_(render_frame_host),
      client_(client),
      password_generation_manager_(client, this),
      password_autofill_manager_(this, autofill_client, client),
      is_main_frame_(render_frame_host->GetParent() == nullptr),
      weak_factory_(this) {
  // For some frames |this| may be instantiated before log manager creation, so
  // here we can not send logging state to renderer process for them. For such
  // cases, after the log manager got ready later,
  // ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability() will
  // call ContentPasswordManagerDriver::SendLoggingAvailability() on |this| to
  // do it actually.
  if (client_->GetLogManager()) {
    // Do not call the virtual method SendLoggingAvailability from a constructor
    // here, inline its steps instead.
    GetPasswordAutofillAgent()->SetLoggingState(
        client_->GetLogManager()->IsLoggingActive());
  }
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() {
}

// static
ContentPasswordManagerDriver*
ContentPasswordManagerDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentPasswordManagerDriverFactory* factory =
      ContentPasswordManagerDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->GetDriverForFrame(render_frame_host) : nullptr;
}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  password_autofill_manager_.OnAddPasswordFillData(form_data);
  GetPasswordAutofillAgent()->FillPasswordForm(
      autofill::MaybeClearPasswordValues(form_data));
}

void ContentPasswordManagerDriver::AllowPasswordGenerationForForm(
    const autofill::PasswordForm& form) {
  if (GetPasswordGenerationManager()->IsGenerationEnabled(
          /*log_debug_data=*/true)) {
    GetPasswordGenerationAgent()->FormNotBlacklisted(form);
  }
}

void ContentPasswordManagerDriver::FormsEligibleForGenerationFound(
    const std::vector<autofill::PasswordFormGenerationData>& forms) {
  GetPasswordGenerationAgent()->FoundFormsEligibleForGeneration(forms);
}

void ContentPasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::NewPasswordFormGenerationData& form) {
  if (GetPasswordGenerationManager()->IsGenerationEnabled(
          /*log_debug_data=*/true)) {
    GetPasswordGenerationAgent()->FoundFormEligibleForGeneration(form);
  }
}

void ContentPasswordManagerDriver::AutofillDataReceived(
    const std::map<autofill::FormData,
                   autofill::PasswordFormFieldPredictionMap>& predictions) {
  GetPasswordAutofillAgent()->AutofillUsernameAndPasswordDataReceived(
      predictions);
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const base::string16& password) {
  GetPasswordGenerationAgent()->GeneratedPasswordAccepted(password);
}

void ContentPasswordManagerDriver::FillSuggestion(
    const base::string16& username,
    const base::string16& password) {
  GetAutofillAgent()->FillPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::FillIntoFocusedField(
    bool is_password,
    const base::string16& credential,
    base::OnceCallback<void(autofill::FillingStatus)> compeleted_callback) {
  GetPasswordAutofillAgent()->FillIntoFocusedField(
      is_password, credential, std::move(compeleted_callback));
}

void ContentPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
  GetAutofillAgent()->PreviewPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::ShowInitialPasswordAccountSuggestions(
    const autofill::PasswordFormFillData& form_data) {
  password_autofill_manager_.OnAddPasswordFillData(form_data);
  GetAutofillAgent()->ShowInitialPasswordAccountSuggestions(form_data);
}

void ContentPasswordManagerDriver::ClearPreviewedForm() {
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentPasswordManagerDriver::GeneratePassword() {
  GetPasswordGenerationAgent()->UserTriggeredGeneratePassword();
}

PasswordGenerationManager*
ContentPasswordManagerDriver::GetPasswordGenerationManager() {
  return &password_generation_manager_;
}

PasswordManager* ContentPasswordManagerDriver::GetPasswordManager() {
  return client_->GetPasswordManager();
}

PasswordAutofillManager*
ContentPasswordManagerDriver::GetPasswordAutofillManager() {
  return &password_autofill_manager_;
}

void ContentPasswordManagerDriver::SendLoggingAvailability() {
  GetPasswordAutofillAgent()->SetLoggingState(
      client_->GetLogManager()->IsLoggingActive());
}

autofill::AutofillDriver* ContentPasswordManagerDriver::GetAutofillDriver() {
  return autofill::ContentAutofillDriver::GetForRenderFrameHost(
      render_frame_host_);
}

bool ContentPasswordManagerDriver::IsMainFrame() const {
  return is_main_frame_;
}

void ContentPasswordManagerDriver::DidNavigateFrame(
    content::NavigationHandle* navigation_handle) {
  // Clear page specific data after main frame navigation.
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    NotifyDidNavigateMainFrame(navigation_handle->IsRendererInitiated(),
                               navigation_handle->GetPageTransition(),
                               navigation_handle->HasUserGesture(),
                               GetPasswordManager());
    GetPasswordAutofillManager()->DidNavigateMainFrame();
  }
}

const autofill::mojom::AutofillAgentAssociatedPtr&
ContentPasswordManagerDriver::GetAutofillAgent() {
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          render_frame_host_);
  DCHECK(autofill_driver);
  return autofill_driver->GetAutofillAgent();
}

const autofill::mojom::PasswordAutofillAgentAssociatedPtr&
ContentPasswordManagerDriver::GetPasswordAutofillAgent() {
  if (!password_autofill_agent_) {
    auto request = mojo::MakeRequest(&password_autofill_agent_);
    // Some test environments may have no remote interface support.
    if (render_frame_host_->GetRemoteAssociatedInterfaces()) {
      render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
          std::move(request));
    }
  }

  return password_autofill_agent_;
}

const autofill::mojom::PasswordGenerationAgentAssociatedPtr&
ContentPasswordManagerDriver::GetPasswordGenerationAgent() {
  if (!password_gen_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        mojo::MakeRequest(&password_gen_agent_));
  }

  return password_gen_agent_;
}

}  // namespace password_manager

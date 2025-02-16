// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace base {
class Clock;
class RepeatingTimer;
class SequencedTaskRunner;
}  // namespace base

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}

namespace policy {

class UserCloudPolicyManagerChromeOS;

// A PKS that observes an IdentityManager and mints the policy access
// token for the UserCloudPolicyManagerChromeOS. First token is fetched when the
// token service becomes ready. After that if needed a new token is fetched when
// the previous one is expected to expire. This service decouples the
// UserCloudPolicyManagerChromeOS from depending directly on the
// IdentityManager, since it is initialized much earlier.
class UserCloudPolicyTokenForwarder : public KeyedService,
                                      public CloudPolicyService::Observer {
 public:
  // Delay to token fetch retry attempt in case token fetch failed or returned
  // invalid data.
  static constexpr base::TimeDelta kFetchTokenRetryDelay =
      base::TimeDelta::FromMinutes(5);

  // The factory of this PKS depends on the factories of these two arguments,
  // so this object will be Shutdown() first and these pointers can be used
  // until that point.
  UserCloudPolicyTokenForwarder(UserCloudPolicyManagerChromeOS* manager,
                                identity::IdentityManager* identity_manager);
  ~UserCloudPolicyTokenForwarder() override;

  // KeyedService:
  void Shutdown() override;

  // CloudPolicyService::Observer:
  void OnCloudPolicyServiceInitializationCompleted() override;

  // Returns whether OAuth token fetch is currently in progress.
  bool IsTokenFetchInProgressForTesting() const;

  // Returns whether next OAuth token refresh is scheduled.
  bool IsTokenRefreshScheduledForTesting() const;

  // Returns delay to next token refresh if it is scheduled.
  base::Optional<base::TimeDelta> GetTokenRefreshDelayForTesting() const;

  // Overrides elements responsible for time progression to allow testing.
  // Affects time calculation and timer objects.
  void OverrideTimeForTesting(
      const base::Clock* clock,
      const base::TickClock* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void StartRequest();
  void OnAccessTokenFetchCompleted(GoogleServiceAuthError error,
                                   identity::AccessTokenInfo token_info);

  UserCloudPolicyManagerChromeOS* manager_;
  identity::IdentityManager* identity_manager_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Last fetched OAuth token.
  base::Optional<identity::AccessTokenInfo> oauth_token_;

  // Timer that measures time to the next OAuth token refresh. Not initialized
  // if token refresh is not scheduled.
  std::unique_ptr<base::RepeatingTimer> refresh_oauth_token_timer_;

  // Points to the base::DefaultClock by default.
  const base::Clock* clock_;

  base::WeakPtrFactory<UserCloudPolicyTokenForwarder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyTokenForwarder);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_ANDROID_LOGO_SERVICE_IMPL_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_ANDROID_LOGO_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/search_provider_logos/logo_service.h"
#include "services/identity/public/cpp/identity_manager.h"

class TemplateURLService;

namespace base {
class Clock;
}  // namespace base

namespace image_fetcher {
class ImageDecoder;
}  // namespace image_fetcher

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace search_provider_logos {

class LogoCache;
class LogoObserver;

class LogoServiceImpl : public LogoService,
                        public identity::IdentityManager::Observer {
 public:
  LogoServiceImpl(
      const base::FilePath& cache_directory,
      identity::IdentityManager* identity_manager,
      TemplateURLService* template_url_service,
      std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::RepeatingCallback<bool()> want_gray_logo_getter);

  ~LogoServiceImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Defines the server API for downloading and parsing the logo. This must be
  // called at least once before calling GetLogo().
  //
  // |logo_url| is the URL from which the logo will be downloaded. If |logo_url|
  // is different than the current logo URL, any pending callbacks will be run
  // with LogoCallbackReason::CANCELED.
  //
  // |parse_logo_response_func| is a callback that will be used to parse the
  // server's response into a EncodedLogo object. |append_queryparams_func| is a
  // callback that will return the URL from which to download the logo.
  void SetServerAPI(const GURL& logo_url,
                    const ParseLogoResponse& parse_logo_response_func,
                    const AppendQueryparamsToLogoURL& append_queryparams_func);

  // Retrieves the current search provider's logo from the local cache and/or
  // over the network, and registers the callbacks to be called when the cached
  // and/or fresh logos are available.
  //
  // At least one callback must be non-null. All non-null callbacks will be
  // invoked exactly once.
  void GetLogo(LogoObserver* observer) override;
  void GetLogo(LogoCallbacks callbacks) override;

  // Overrides the cache used to store logos.
  void SetLogoCacheForTests(std::unique_ptr<LogoCache> cache);

  // Overrides the clock used to check the time.
  void SetClockForTests(base::Clock* clock);

 private:
  // These values must stay in sync with the NewTabPageLogoDownloadOutcome enum
  // in histograms.xml. And any addtion should be treated as append-only!
  // Animated doodle is not covered by this enum.
  enum LogoDownloadOutcome {
    DOWNLOAD_OUTCOME_NEW_LOGO_SUCCESS,
    DOWNLOAD_OUTCOME_NO_LOGO_TODAY,
    DOWNLOAD_OUTCOME_DOWNLOAD_FAILED,
    DOWNLOAD_OUTCOME_PARSING_FAILED,
    DOWNLOAD_OUTCOME_DECODING_FAILED,
    DOWNLOAD_OUTCOME_LOGO_REVALIDATED,
    DOWNLOAD_OUTCOME_COUNT,
  };

  const int kDownloadOutcomeNotTracked = -1;

  // identity::IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(const identity::AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override;

  // Clear any cached logo we might have. Useful on sign-out to get rid of
  // (potentially) personalized data.
  void ClearCachedLogo();

  // Cancels the current asynchronous operation, if any, and resets all member
  // variables that change as the logo is fetched. This method also records UMA
  // histograms for for the given LogoDownloadOutcome.
  void ReturnToIdle(int outcome);

  // Called when the cached logo has been read from the cache. |cached_logo|
  // will be NULL if there wasn't a valid, up-to-date logo in the cache.
  void OnCachedLogoRead(std::unique_ptr<EncodedLogo> cached_logo);

  // Called when the cached logo has been decoded into an SkBitmap. |image| will
  // be NULL if decoding failed.
  void OnCachedLogoAvailable(std::unique_ptr<EncodedLogo> encoded_logo,
                             const SkBitmap& image);

  // Stores |logo| in the cache.
  void SetCachedLogo(std::unique_ptr<EncodedLogo> logo);

  // Updates the metadata for the logo already stored in the cache.
  void SetCachedMetadata(const LogoMetadata& metadata);

  // Starts fetching the current logo over the network.
  void FetchLogo();

  // Called when the logo has been downloaded and parsed. |logo| will be NULL
  // if the server's response was invalid.
  void OnFreshLogoParsed(bool* parsing_failed,
                         bool from_http_cache,
                         std::unique_ptr<EncodedLogo> logo);

  // Called when the fresh logo has been decoded into an SkBitmap. |image| will
  // be NULL if decoding failed.
  void OnFreshLogoAvailable(std::unique_ptr<EncodedLogo> logo,
                            bool download_failed,
                            bool parsing_failed,
                            bool from_http_cache,
                            const SkBitmap& image);

  // Invoked by |loader|.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> body);

  // Constructor arguments.
  const base::FilePath cache_directory_;
  identity::IdentityManager* const identity_manager_;
  TemplateURLService* const template_url_service_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Callback to get the type of logo to fetch. Returns whether we want a logo
  // optimized for gray backgrounds or not.
  base::RepeatingCallback<bool()> want_gray_logo_getter_;

  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  // The URL from which the logo is fetched.
  GURL logo_url_;

  // The function used to parse the logo response from the server.
  ParseLogoResponse parse_logo_response_func_;

  // The function used to include the cached logo's fingerprint and call to
  // action request in the logo URL.
  AppendQueryparamsToLogoURL append_queryparams_func_;

  // False if an asynchronous task is currently running.
  bool is_idle_;

  // The logo that's been read from the cache, or NULL if the cache is empty.
  // Meaningful only if is_cached_logo_valid_ is true; NULL otherwise.
  std::unique_ptr<Logo> cached_logo_;
  std::unique_ptr<EncodedLogo> cached_encoded_logo_;

  // Whether the value of |cached_logo_| reflects the actual cached logo.
  // This will be false if the logo hasn't been read from the cache yet.
  // |cached_logo_| may be NULL even if |is_cached_logo_valid_| is true, if no
  // logo is cached.
  bool is_cached_logo_valid_;

  // The timestamp for the last time a logo was stated to be downloaded.
  base::TimeTicks logo_download_start_time_;

  // The SimpleURLLoader currently fetching the logo. NULL when not loading.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Lists of callbacks to be invoked when logos are available. All should be
  // empty when the state is IDLE.
  std::vector<LogoCallback> on_cached_decoded_logo_;
  std::vector<EncodedLogoCallback> on_cached_encoded_logo_;
  std::vector<LogoCallback> on_fresh_decoded_logo_;
  std::vector<EncodedLogoCallback> on_fresh_encoded_logo_;

  // The SequencedTaskRunner on which the cache lives.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // The cache used to persist the logo on disk. Used only on a background
  // SequencedTaskRunner.
  std::unique_ptr<LogoCache, base::OnTaskRunnerDeleter> logo_cache_;

  // Clock used to determine current time. Can be overridden in tests.
  base::Clock* clock_ = nullptr;

  base::WeakPtrFactory<LogoServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogoServiceImpl);
};

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_ANDROID_LOGO_SERVICE_IMPL_H_

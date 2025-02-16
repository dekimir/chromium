// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#include "base/macros.h"

#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class IOSProfileSessionDurationsService;
namespace ios {
class ChromeBrowserState;
}

class IOSProfileSessionDurationsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for |browser_state|.
  static IOSProfileSessionDurationsService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static IOSProfileSessionDurationsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IOSProfileSessionDurationsServiceFactory>;

  IOSProfileSessionDurationsServiceFactory();
  ~IOSProfileSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(IOSProfileSessionDurationsServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

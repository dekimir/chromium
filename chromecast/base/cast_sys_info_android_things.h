// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_SYS_INFO_ANDROID_THINGS_H_
#define CHROMECAST_BASE_CAST_SYS_INFO_ANDROID_THINGS_H_

#include "chromecast/base/cast_sys_info_android.h"
#include "chromecast/chromecast_buildflags.h"

namespace chromecast {

class CastSysInfoAndroidThings : public CastSysInfoAndroid {
 public:
  CastSysInfoAndroidThings();
  ~CastSysInfoAndroidThings() override;

#if !BUILDFLAG(USE_ANDROID_THINGS_N)
  // CastSysInfo implementation:
  std::string GetProductName() override;
  std::string GetDeviceModel() override;
  std::string GetManufacturer() override;
  std::string GetSystemReleaseChannel() override;
#endif
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_SYS_INFO_ANDROID_THINGS_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_CAST_TEST_COMMON_H_
#define WEBRUNNER_APP_CAST_TEST_COMMON_H_

#include <fuchsia/sys/cpp/fidl.h>

#include "base/strings/string_piece.h"

namespace castrunner {

// Starts a cast component from the runner |sys_runner| with the URL |cast_url|
// and returns the service directory client channel.
zx::channel StartCastComponent(
    const base::StringPiece& cast_url,
    fuchsia::sys::RunnerPtr* sys_runner,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request);

}  // namespace castrunner

#endif  // WEBRUNNER_APP_CAST_TEST_COMMON_H_
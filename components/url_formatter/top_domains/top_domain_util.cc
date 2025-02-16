// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/top_domains/top_domain_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace url_formatter {

namespace top_domains {

namespace {

// Minimum length for a hostname to be considered for an edit distance
// comparison. Shorter domains are ignored.
const size_t kMinLengthForEditDistance = 5u;

// Returns the portion of hostname without the registry part.
// E.g. For hostname = "google.com", the registry is "com", and the return value
// will be 6 (length of "google").
size_t GetWithoutRegistryLength(const std::string& hostname) {
  const size_t registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          hostname.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if (registry_size == 0) {
    return hostname.size();
  }
  DCHECK_LE(registry_size, hostname.size() - 1);
  return hostname.size() - registry_size - 1;
}

}  // namespace

bool IsEditDistanceCandidate(const std::string& hostname) {
  return GetWithoutRegistryLength(hostname) >= kMinLengthForEditDistance;
}

}  // namespace top_domains

}  // namespace url_formatter

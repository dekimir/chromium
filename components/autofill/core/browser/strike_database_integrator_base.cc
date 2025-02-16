// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database_integrator_base.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace autofill {

namespace {
const char kKeyDeliminator[] = "__";
}  // namespace

StrikeDatabaseIntegratorBase::StrikeDatabaseIntegratorBase(
    StrikeDatabase* strike_database)
    : strike_database_(strike_database) {}

StrikeDatabaseIntegratorBase::~StrikeDatabaseIntegratorBase() {}

bool StrikeDatabaseIntegratorBase::IsMaxStrikesLimitReached(
    const std::string id) {
  return GetStrikes(id) >= GetMaxStrikesLimit();
}

int StrikeDatabaseIntegratorBase::AddStrike(const std::string id) {
  int num_strikes = strike_database_->AddStrike(GetKey(id));
  base::UmaHistogramCounts1000(
      "Autofill.StrikeDatabase.NthStrikeAdded." + GetProjectPrefix(),
      num_strikes);
  return num_strikes;
}

int StrikeDatabaseIntegratorBase::RemoveStrike(const std::string id) {
  return strike_database_->RemoveStrike(GetKey(id));
}

int StrikeDatabaseIntegratorBase::GetStrikes(const std::string id) {
  return strike_database_->GetStrikes(GetKey(id));
}

void StrikeDatabaseIntegratorBase::ClearStrikes(const std::string id) {
  strike_database_->ClearStrikes(GetKey(id));
}

void StrikeDatabaseIntegratorBase::RemoveExpiredStrikes() {
  std::vector<std::string> expired_keys;
  for (auto entry : strike_database_->strike_map_cache_) {
    if (AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds() -
            entry.second.last_update_timestamp() >
        GetExpiryTimeMicros()) {
      if (strike_database_->GetStrikes(entry.first) > 0)
        expired_keys.push_back(entry.first);
    }
  }
  for (std::string key : expired_keys)
    strike_database_->RemoveStrike(key);
}

std::string StrikeDatabaseIntegratorBase::GetKey(const std::string id) {
  return GetProjectPrefix() + kKeyDeliminator + id;
}

}  // namespace autofill

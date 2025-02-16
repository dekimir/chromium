// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database_provider.h"

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database_provider.h"

namespace leveldb_proto {

SharedProtoDatabaseProvider::SharedProtoDatabaseProvider(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::WeakPtr<ProtoDatabaseProvider> provider_weak_ptr)
    : task_runner_(task_runner),
      provider_weak_ptr_(std::move(provider_weak_ptr)) {}

SharedProtoDatabaseProvider::~SharedProtoDatabaseProvider() = default;

void SharedProtoDatabaseProvider::GetDBInstance(
    GetSharedDBInstanceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProtoDatabaseProvider::GetSharedDBInstance,
                                provider_weak_ptr_, std::move(callback),
                                std::move(callback_task_runner)));
}

}  // namespace leveldb_proto
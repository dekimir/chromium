// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_database_provider.h"

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/shared_proto_database.h"

namespace leveldb_proto {

namespace {

const char kSharedProtoDatabaseClientName[] = "SharedProtoDB";
const char kSharedProtoDatabaseDirectory[] = "shared_proto_db";

}  // namespace

ProtoDatabaseProvider::ProtoDatabaseProvider(const base::FilePath& profile_dir)
    : profile_dir_(profile_dir),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      creation_sequence_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {}

ProtoDatabaseProvider::~ProtoDatabaseProvider() = default;

// static
ProtoDatabaseProvider* ProtoDatabaseProvider::Create(
    const base::FilePath& profile_dir) {
  return new ProtoDatabaseProvider(profile_dir);
}

void ProtoDatabaseProvider::GetSharedDBInstance(
    GetSharedDBInstanceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  {
    base::AutoLock lock(get_db_lock_);
    if (!db_) {
      db_ = base::WrapRefCounted(new SharedProtoDatabase(
          kSharedProtoDatabaseClientName, profile_dir_.AppendASCII(std::string(
                                              kSharedProtoDatabaseDirectory))));
    }
  }

  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), db_));
}

}  // namespace leveldb_proto

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/resource_cache.h"

#include "base/base64url.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"

namespace policy {

namespace {

// Decodes all elements of |input| from base64url format and stores the decoded
// elements in |output|.
bool Base64UrlEncode(const std::set<std::string>& input,
                     std::set<std::string>* output) {
  output->clear();
  for (const auto& plain : input) {
    if (plain.empty()) {
      NOTREACHED();
      output->clear();
      return false;
    }

    std::string encoded;
    base::Base64UrlEncode(plain, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded);

    output->insert(encoded);
  }
  return true;
}

}  // namespace

ResourceCache::ResourceCache(
    const base::FilePath& cache_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<int64_t> max_cache_size)
    : cache_dir_(cache_dir),
      task_runner_(task_runner),
      max_cache_size_(max_cache_size) {
  // Safe to post this without a WeakPtr because this class must be destructed
  // on the same thread.
  if (max_cache_size_.has_value()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&ResourceCache::InitCurrentCacheSize,
                                          base::Unretained(this)));
  }
}

ResourceCache::~ResourceCache() {
  // No RunsTasksInCurrentSequence() check to avoid unit tests failures.
  // In unit tests the browser process instance is deleted only after test ends
  // and test task scheduler is shutted down. Therefore we need to delete some
  // components of BrowserPolicyConnector (ResourceCache and
  // CloudExternalDataManagerBase::Backend) manually when task runner doesn't
  // accept new tasks (DeleteSoon in this case). This leads to the situation
  // when this destructor is called not on |task_runner|.
}

bool ResourceCache::Store(const std::string& key,
                          const std::string& subkey,
                          const std::string& data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.Store");
  base::FilePath subkey_path;
  if (!VerifyKeyPathAndGetSubkeyPath(key, true, subkey, &subkey_path))
    return false;
  int64_t size = base::checked_cast<int64_t>(data.size());
  if (max_cache_size_.has_value() &&
      current_cache_size_ - GetCacheDirectoryOrFileSize(subkey_path) + size >
          max_cache_size_.value()) {
    LOG(ERROR) << "Data (" << key << ", " << subkey << ") with size " << size
               << " bytes doesn't fit in cache, left size: "
               << max_cache_size_.value() - current_cache_size_ << " bytes";
    return false;
  }
  return WriteCacheFile(subkey_path, data);
}

bool ResourceCache::Load(const std::string& key,
                         const std::string& subkey,
                         std::string* data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.Load");
  base::FilePath subkey_path;
  // Only read from |subkey_path| if it is not a symlink.
  if (!VerifyKeyPathAndGetSubkeyPath(key, false, subkey, &subkey_path) ||
      base::IsLink(subkey_path)) {
    return false;
  }
  data->clear();
  return base::ReadFileToString(subkey_path, data);
}

void ResourceCache::LoadAllSubkeys(
    const std::string& key,
    std::map<std::string, std::string>* contents) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.LoadAllSubkeys");
  contents->clear();
  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string encoded_subkey = path.BaseName().MaybeAsASCII();
    std::string subkey;
    std::string data;
    // Only read from |subkey_path| if it is not a symlink and its name is
    // a base64-encoded string.
    if (!base::IsLink(path) &&
        base::Base64UrlDecode(encoded_subkey,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &subkey) &&
        !subkey.empty() && base::ReadFileToString(path, &data)) {
      (*contents)[subkey].swap(data);
    }
  }
}

void ResourceCache::Delete(const std::string& key, const std::string& subkey) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.Delete");
  base::FilePath subkey_path;
  if (VerifyKeyPathAndGetSubkeyPath(key, false, subkey, &subkey_path))
    DeleteCacheFile(subkey_path, false);
  base::FilePath key_path;
  // DeleteCacheFile() does nothing if the directory given to it is not empty.
  // Hence, the call below deletes the directory representing |key| if its last
  // subkey was just removed and does nothing otherwise.
  if (VerifyKeyPath(key, false, &key_path))
    DeleteCacheFile(key_path, false);
}

void ResourceCache::Clear(const std::string& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.Clear");
  base::FilePath key_path;
  if (VerifyKeyPath(key, false, &key_path))
    DeleteCacheFile(key_path, true);
}

void ResourceCache::FilterSubkeys(const std::string& key,
                                  const SubkeyFilter& test) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.FilterSubkeys");

  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath subkey_path = enumerator.Next();
       !subkey_path.empty(); subkey_path = enumerator.Next()) {
    std::string subkey;
    // Delete files with invalid names, and files whose subkey doesn't pass the
    // filter.
    if (!base::Base64UrlDecode(subkey_path.BaseName().MaybeAsASCII(),
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &subkey) ||
        subkey.empty() || test.Run(subkey)) {
      DeleteCacheFile(subkey_path, true);
    }
  }

  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if all of its subkeys
  // were just removed and does nothing otherwise.
  DeleteCacheFile(key_path, false);
}

void ResourceCache::PurgeOtherKeys(const std::set<std::string>& keys_to_keep) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.PurgeOtherKeys");
  std::set<std::string> encoded_keys_to_keep;
  if (!Base64UrlEncode(keys_to_keep, &encoded_keys_to_keep))
    return;

  base::FileEnumerator enumerator(
      cache_dir_, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name(path.BaseName().MaybeAsASCII());
    if (encoded_keys_to_keep.find(name) == encoded_keys_to_keep.end())
      DeleteCacheFile(path, true);
  }
}

void ResourceCache::PurgeOtherSubkeys(
    const std::string& key,
    const std::set<std::string>& subkeys_to_keep) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Enterprise.ResourceCacheTiming.PurgeOtherSubkeys");
  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  std::set<std::string> encoded_subkeys_to_keep;
  if (!Base64UrlEncode(subkeys_to_keep, &encoded_subkeys_to_keep))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name(path.BaseName().MaybeAsASCII());
    if (encoded_subkeys_to_keep.find(name) == encoded_subkeys_to_keep.end())
      DeleteCacheFile(path, false);
  }
  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if all of its subkeys
  // were just removed and does nothing otherwise.
  DeleteCacheFile(key_path, false);
}

bool ResourceCache::VerifyKeyPath(const std::string& key,
                                  bool allow_create,
                                  base::FilePath* path) {
  if (key.empty()) {
    NOTREACHED();
    return false;
  }

  std::string encoded;
  base::Base64UrlEncode(key, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded);

  *path = cache_dir_.AppendASCII(encoded);
  return allow_create ? base::CreateDirectory(*path) :
                        base::DirectoryExists(*path);
}

bool ResourceCache::VerifyKeyPathAndGetSubkeyPath(const std::string& key,
                                                  bool allow_create_key,
                                                  const std::string& subkey,
                                                  base::FilePath* path) {
  if (subkey.empty()) {
    NOTREACHED();
    return false;
  }

  base::FilePath key_path;
  if (!VerifyKeyPath(key, allow_create_key, &key_path))
    return false;

  std::string encoded;
  base::Base64UrlEncode(subkey, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded);

  *path = key_path.AppendASCII(encoded);
  return true;
}

void ResourceCache::InitCurrentCacheSize() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Enterprise.ResourceCacheTiming.Init");
  current_cache_size_ = GetCacheDirectoryOrFileSize(cache_dir_);
}

bool ResourceCache::WriteCacheFile(const base::FilePath& path,
                                   const std::string& data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(cache_dir_.IsParent(path));
  bool success = DeleteCacheFile(path, false);
  int size = base::checked_cast<int>(data.size());
  int bytes_written = base::WriteFile(path, data.data(), size);
  if (max_cache_size_.has_value())
    current_cache_size_ += bytes_written;
  return success && bytes_written == size;
}

bool ResourceCache::DeleteCacheFile(const base::FilePath& path,
                                    bool recursive) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(cache_dir_.IsParent(path));
  int64_t size = GetCacheDirectoryOrFileSize(path);
  bool success = base::DeleteFile(path, recursive);
  if (success && max_cache_size_.has_value())
    current_cache_size_ -= size;
  return success;
}

int64_t ResourceCache::GetCacheDirectoryOrFileSize(
    const base::FilePath& path) const {
  DCHECK(path == cache_dir_ || cache_dir_.IsParent(path));
  if (base::IsLink(path)) {
    DLOG(WARNING) << "Symlink " << path.LossyDisplayName()
                  << " detected in cache directory";
    return 0;
  }
  int64_t path_size = 0;
  if (base::DirectoryExists(path)) {
    int types = base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES;
    base::FileEnumerator enumerator(path, /* recursive */ false, types);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      path_size += GetCacheDirectoryOrFileSize(path);
    }
  } else if (!base::GetFileSize(path, &path_size)) {
    path_size = 0;
  }
  return path_size;
}

}  // namespace policy

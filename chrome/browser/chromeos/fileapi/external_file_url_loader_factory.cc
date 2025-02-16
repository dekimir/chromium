// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_loader_factory.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_resolver.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"

namespace chromeos {
namespace {

constexpr size_t kDefaultPipeSize = 65536;

// An IOBuffer that doesn't own its data.
class MojoPipeIOBuffer : public net::IOBuffer {
 public:
  explicit MojoPipeIOBuffer(void* data)
      : net::IOBuffer(static_cast<char*>(data)) {}

 protected:
  ~MojoPipeIOBuffer() override {
    // Set data_ to null so ~IOBuffer won't try to delete it.
    data_ = nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoPipeIOBuffer);
};

// A helper class to read data from a FileStreamReader, and write it to a
// Mojo data pipe.
class FileSystemReaderDataPipeProducer {
 public:
  FileSystemReaderDataPipeProducer(
      mojo::ScopedDataPipeProducerHandle producer_handle,
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      int remaining_bytes,
      base::OnceCallback<void(net::Error)> callback)
      : producer_handle_(std::move(producer_handle)),
        stream_reader_(std::move(stream_reader)),
        remaining_bytes_(remaining_bytes),
        total_bytes_written_(0),
        pipe_watcher_(std::make_unique<mojo::SimpleWatcher>(
            FROM_HERE,
            mojo::SimpleWatcher::ArmingPolicy::MANUAL,
            base::SequencedTaskRunnerHandle::Get())),
        callback_(std::move(callback)),
        weak_ptr_factory_(this) {
    pipe_watcher_->Watch(
        producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(&FileSystemReaderDataPipeProducer::OnHandleReady,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void Write() {
    while (remaining_bytes_ > 0) {
      if (!producer_handle_.is_valid())
        CompleteWithResult(net::ERR_FAILED);
      void* pipe_buffer;
      uint32_t buffer_size = kDefaultPipeSize;
      MojoResult result = producer_handle_->BeginWriteData(
          &pipe_buffer, &buffer_size, MOJO_WRITE_DATA_FLAG_NONE);
      // If we can't synchronously get the buffer to write to, stop for now and
      // wait for the SimpleWatcher to notify us that the pipe is writable.
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        pipe_watcher_->ArmOrNotify();
        return;
      }
      if (result != MOJO_RESULT_OK) {
        CompleteWithResult(MojoResultToErrorCode(result));
        return;
      }

      DCHECK(base::IsValueInRangeForNumericType<int>(buffer_size));
      scoped_refptr<MojoPipeIOBuffer> io_buffer =
          base::MakeRefCounted<MojoPipeIOBuffer>(pipe_buffer);
      const int read_size = stream_reader_->Read(
          io_buffer.get(), std::min<int64_t>(buffer_size, remaining_bytes_),
          base::BindOnce(
              &FileSystemReaderDataPipeProducer::OnPendingReadComplete,
              weak_ptr_factory_.GetWeakPtr()));
      // Read will return ERR_IO_PENDING if the read couldn't be completed
      // synchronously. In that case return, and OnPendingReadComplete will
      // be called when the read is complete.
      if (read_size == net::ERR_IO_PENDING)
        return;
      OnWroteData(read_size);
    }
    CompleteWithResult(net::OK);
  }

  int64_t total_bytes_written() { return total_bytes_written_; }

 private:
  void OnWroteData(int read_size) {
    MojoResult result =
        producer_handle_->EndWriteData(std::max<int>(0, read_size));
    if (read_size <= 0) {
      CompleteWithResult(static_cast<net::Error>(read_size));
      return;
    }
    if (result != MOJO_RESULT_OK) {
      CompleteWithResult(MojoResultToErrorCode(result));
      return;
    }
    remaining_bytes_ -= read_size;
    total_bytes_written_ += read_size;
  }

  void OnPendingReadComplete(int read_result) {
    OnWroteData(read_result);
    Write();
  }

  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state) {
    if (result != MOJO_RESULT_OK) {
      CompleteWithResult(MojoResultToErrorCode(result));
      return;
    }
    Write();
  }

  void CompleteWithResult(net::Error error) {
    pipe_watcher_.reset();
    std::move(callback_).Run(error);
  }

  net::Error MojoResultToErrorCode(MojoResult result) {
    switch (result) {
      case MOJO_RESULT_OK:
        return net::OK;
      case MOJO_RESULT_CANCELLED:
        return net::ERR_ABORTED;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        return net::ERR_TIMED_OUT;
      case MOJO_RESULT_NOT_FOUND:
        return net::ERR_FILE_NOT_FOUND;
      case MOJO_RESULT_PERMISSION_DENIED:
        return net::ERR_ACCESS_DENIED;
      case MOJO_RESULT_RESOURCE_EXHAUSTED:
        return net::ERR_INSUFFICIENT_RESOURCES;
      case MOJO_RESULT_UNIMPLEMENTED:
        return net::ERR_NOT_IMPLEMENTED;
      default:
        return net::ERR_FAILED;
    }
  }

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<storage::FileStreamReader> stream_reader_;
  int64_t remaining_bytes_;
  int64_t total_bytes_written_;
  std::unique_ptr<mojo::SimpleWatcher> pipe_watcher_;
  base::OnceCallback<void(net::Error)> callback_;
  base::WeakPtrFactory<FileSystemReaderDataPipeProducer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemReaderDataPipeProducer);
};

class ExternalFileURLLoader : public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      void* profile_id,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtrInfo client_info) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClientPtr
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* external_file_url_loader = new ExternalFileURLLoader(
        profile_id, std::move(loader), std::move(client_info));
    external_file_url_loader->Start(request);
  }

  // network::mojom::URLLoader:
  void FollowRedirect(
      const base::Optional<std::vector<std::string>>&
          to_be_removed_request_headers,
      const base::Optional<net::HttpRequestHeaders>& modified_request_headers,
      const base::Optional<GURL>& new_url) override {}
  void ProceedWithResponse() override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  explicit ExternalFileURLLoader(
      void* profile_id,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtrInfo client_info)
      : binding_(this),
        resolver_(std::make_unique<ExternalFileResolver>(profile_id)),
        weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    binding_.Bind(std::move(loader));
    binding_.set_connection_error_handler(base::BindOnce(
        &ExternalFileURLLoader::OnConnectionError, base::Unretained(this)));
    client_.Bind(std::move(client_info));
  }
  ~ExternalFileURLLoader() override = default;

  void Start(const network::ResourceRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    head_.request_start = base::TimeTicks::Now();

    resolver_->ProcessHeaders(request.headers);
    resolver_->Resolve(
        request.method, request.url,
        base::BindOnce(&ExternalFileURLLoader::CompleteWithError,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ExternalFileURLLoader::OnRedirectURLObtained,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ExternalFileURLLoader::OnStreamObtained,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnRedirectURLObtained(const std::string& mime_type,
                             const GURL& redirect_url) {
    head_.mime_type = mime_type;
    head_.response_start = base::TimeTicks::Now();
    head_.encoded_data_length = 0;
    net::RedirectInfo redirect_info;
    redirect_info.new_method = "GET";
    redirect_info.status_code = 302;
    redirect_info.new_url = redirect_url;
    client_->OnReceiveRedirect(redirect_info, head_);
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnStreamObtained(
      const std::string& mime_type,
      std::unique_ptr<IsolatedFileSystemScope> isolated_file_system_scope,
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      int64_t size) {
    head_.mime_type = mime_type;
    head_.content_length = size;
    isolated_file_system_scope_ = std::move(isolated_file_system_scope);

    mojo::DataPipe pipe(kDefaultPipeSize);
    if (!pipe.consumer_handle.is_valid() || !pipe.producer_handle.is_valid()) {
      CompleteWithError(net::ERR_FAILED);
      return;
    }
    head_.response_start = base::TimeTicks::Now();
    client_->OnReceiveResponse(head_);
    client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));

    data_producer_ = std::make_unique<FileSystemReaderDataPipeProducer>(
        std::move(pipe.producer_handle), std::move(stream_reader), size,
        base::BindOnce(&ExternalFileURLLoader::OnFileWritten,
                       weak_ptr_factory_.GetWeakPtr()));
    data_producer_->Write();
  }

  void OnFileWritten(net::Error error) {
    int64_t total_bytes_written = data_producer_->total_bytes_written();
    data_producer_.reset();
    if (error != net::OK) {
      CompleteWithError(error);
      return;
    }
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = total_bytes_written;
    status.encoded_body_length = total_bytes_written;
    status.decoded_body_length = total_bytes_written;
    client_->OnComplete(status);
    client_.reset();
    MaybeDeleteSelf();
  }

  void CompleteWithError(net::Error net_error) {
    client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnConnectionError() {
    data_producer_.reset();
    client_.reset();
    binding_.Close();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!binding_.is_bound() && !client_.is_bound())
      delete this;
  }

  mojo::Binding<network::mojom::URLLoader> binding_;
  network::mojom::URLLoaderClientPtr client_;

  std::unique_ptr<ExternalFileResolver> resolver_;
  network::ResourceResponseHead head_;
  std::unique_ptr<IsolatedFileSystemScope> isolated_file_system_scope_;
  std::unique_ptr<FileSystemReaderDataPipeProducer> data_producer_;

  base::WeakPtrFactory<ExternalFileURLLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalFileURLLoader);
};

}  // namespace

ExternalFileURLLoaderFactory::ExternalFileURLLoaderFactory(void* profile_id)
    : profile_id_(profile_id) {}

ExternalFileURLLoaderFactory::~ExternalFileURLLoaderFactory() = default;

void ExternalFileURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ExternalFileURLLoader::CreateAndStart, profile_id_,
                     request, std::move(loader), client.PassInterface()));
}

void ExternalFileURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader) {
  bindings_.AddBinding(this, std::move(loader));
}

}  // namespace chromeos

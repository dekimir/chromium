// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UPLOADER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UPLOADER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/webrtc/webrtc_logging_handler_host.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class SimpleURLLoader;
}

typedef struct z_stream_s z_stream;

// Used when uploading is done to perform post-upload actions. |log_path| is
// also used pre-upload.
struct WebRtcLogUploadDoneData : public WebRtcLogPaths {
  WebRtcLogUploadDoneData();
  WebRtcLogUploadDoneData(const WebRtcLogUploadDoneData& other);
  ~WebRtcLogUploadDoneData();

  WebRtcLoggingHandlerHost::UploadDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> host;
  std::string local_log_id;
  // Used for statistics. See |WebRtcLoggingHandlerHost::web_app_id_|.
  int web_app_id;
};

// WebRtcLogUploader uploads WebRTC logs, keeps count of how many logs have
// been started and denies further logs if a limit is reached. It also adds
// the timestamp and report ID of the uploded log to a text file. There must
// only be one object of this type.
class WebRtcLogUploader {
 public:
  WebRtcLogUploader();
  ~WebRtcLogUploader();

  // Returns true is number of logs limit is not reached yet. Increases log
  // count if true is returned. Must be called before UploadLog().
  bool ApplyForStartLogging();

  // Notifies that logging has stopped and that the log should not be uploaded.
  // Decreases log count. May only be called if permission to log has been
  // granted by calling ApplyForStartLogging() and getting true in return.
  // After this function has been called, a new permission must be granted.
  // Call either this function or LoggingStoppedDoUpload().
  void LoggingStoppedDontUpload();

  // Notifies that that logging has stopped and that the log should be uploaded.
  // Decreases log count. May only be called if permission to log has been
  // granted by calling ApplyForStartLogging() and getting true in return. After
  // this function has been called, a new permission must be granted. Call
  // either this function or LoggingStoppedDontUpload().
  // |upload_done_data.local_log_id| is set and used internally and should be
  // left empty.
  void LoggingStoppedDoUpload(std::unique_ptr<WebRtcLogBuffer> log_buffer,
                              std::unique_ptr<MetaDataMap> meta_data,
                              const WebRtcLogUploadDoneData& upload_done_data);

  // Uploads a previously stored log (see LoggingStoppedDoStore()).
  void UploadStoredLog(const WebRtcLogUploadDoneData& upload_data);

  // Similarly to LoggingStoppedDoUpload(), we store the log in compressed
  // format on disk but add the option to specify a unique |log_id| for later
  // identification and potential upload.
  void LoggingStoppedDoStore(
      const WebRtcLogPaths& log_paths,
      const std::string& log_id,
      std::unique_ptr<WebRtcLogBuffer> log_buffer,
      std::unique_ptr<MetaDataMap> meta_data,
      const WebRtcLoggingHandlerHost::GenericDoneCallback& done_callback);

  // Cancels URL fetcher operation by deleting all URL fetchers. This cancels
  // any pending uploads and releases SystemURLRequestContextGetter references.
  // Sets |shutting_down_| which prevent new fetchers to be created.
  void StartShutdown();

  // For testing purposes. If called, the multipart will not be uploaded, but
  // written to |post_data_| instead.
  void OverrideUploadWithBufferForTesting(std::string* post_data) {
    DCHECK((post_data && !post_data_) || (!post_data && post_data_));
    post_data_ = post_data;
  }

  const scoped_refptr<base::SequencedTaskRunner>& background_task_runner()
      const {
    return background_task_runner_;
  }

 private:
  // Allow the test class to call AddLocallyStoredLogInfoToUploadListFile.
  friend class WebRtcLogUploaderTest;
  FRIEND_TEST_ALL_PREFIXES(WebRtcLogUploaderTest,
                           AddLocallyStoredLogInfoToUploadListFile);
  FRIEND_TEST_ALL_PREFIXES(WebRtcLogUploaderTest,
                           AddUploadedLogInfoToUploadListFile);

  void InitURLLoaderFactoryIfNeeded();

  void OnFactoryConnectionClosed();

  // Sets up a multipart body to be uploaded. The body is produced according
  // to RFC 2046.
  void SetupMultipart(std::string* post_data,
                      const std::string& compressed_log,
                      const base::FilePath& incoming_rtp_dump,
                      const base::FilePath& outgoing_rtp_dump,
                      const std::map<std::string, std::string>& meta_data);

  std::string CompressLog(WebRtcLogBuffer* buffer);

  void UploadCompressedLog(const WebRtcLogUploadDoneData& upload_done_data,
                           std::unique_ptr<std::string> post_data);

  void DecreaseLogCount();

  void ShutdownOnIOThread();

  // Must be called on the FILE thread.
  void WriteCompressedLogToFile(const std::string& compressed_log,
                                const base::FilePath& log_file_path);

  void PrepareMultipartPostData(
      const std::string& compressed_log,
      std::unique_ptr<MetaDataMap> meta_data,
      const WebRtcLogUploadDoneData& upload_done_data);

  // Append information (upload time, report ID and local ID) about a log to a
  // log list file, limited to |kLogListLimitLines| entries. This list is used
  // for viewing the logs under chrome://webrtc-logs, see WebRtcLogUploadList.
  // The list has the format
  // upload_time,report_id,local_id
  // upload_time,report_id,local_id
  // etc.
  // where each line represents a log. "upload_time" is the time when the log
  // was uploaded in Unix time. "report_id" is the ID reported back by the
  // server. "local_id" is the ID for the locally stored log. It's the time
  // stored in Unix time and it's also used as file name.
  // AddLocallyStoredLogInfoToUploadListFile() will first be called,
  // "upload_time" and "report_id" is the left empty in the entry written to the
  // list file. If uploading is successful, AddUploadedLogInfoToUploadListFile()
  // is called and those empty items are filled out.
  // Must be called on the FILE thread.
  void AddLocallyStoredLogInfoToUploadListFile(
      const base::FilePath& upload_list_path,
      const std::string& local_log_id);
  static void AddUploadedLogInfoToUploadListFile(
      const base::FilePath& upload_list_path,
      const std::string& local_log_id,
      const std::string& report_id);

  void NotifyUploadDone(int response_code,
                        const std::string& report_id,
                        const WebRtcLogUploadDoneData& upload_done_data);

  using SimpleURLLoaderList =
      std::list<std::unique_ptr<network::SimpleURLLoader>>;

  void OnSimpleLoaderComplete(SimpleURLLoaderList::iterator it,
                              const WebRtcLogUploadDoneData& upload_done_data,
                              std::unique_ptr<std::string> response_body);

  // This is the UI thread for Chromium. Some other thread for tests.
  THREAD_CHECKER(create_thread_checker_);

  // Background sequence where we run background, potentially blocking,
  // operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Keeps track of number of currently open logs. Must be accessed on the IO
  // thread.
  int log_count_ = 0;

  // For testing purposes, see OverrideUploadWithBufferForTesting. Only accessed
  // on the FILE thread.
  std::string* post_data_ = nullptr;

  // Only accessed on the IO thread.
  SimpleURLLoaderList pending_uploads_;

  // When shutting down, don't create new URL loaders.
  bool shutting_down_ = false;

  // URLLoaderFactory bound to the IO thread.
  network::mojom::URLLoaderFactoryPtr url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLogUploader);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UPLOADER_H_

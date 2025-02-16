// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/after_startup_task_utils.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

namespace {

base::test::ScopedTaskEnvironment::MainThreadType GetThreadTypeFromOptions(
    int options) {
  if (options & TestBrowserThreadBundle::PLAIN_MAINLOOP)
    return base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT;
  if (options & TestBrowserThreadBundle::IO_MAINLOOP)
    return base::test::ScopedTaskEnvironment::MainThreadType::IO;
  return base::test::ScopedTaskEnvironment::MainThreadType::UI;
}

}  // namespace

TestBrowserThreadBundle::TestBrowserThreadBundle(
    base::test::ScopedTaskEnvironment::MainThreadType main_thread_type,
    base::test::ScopedTaskEnvironment::ExecutionMode execution_control_mode,
    int options)
    : base::test::ScopedTaskEnvironment(main_thread_type,
                                        execution_control_mode),
      options_(options) {
  // Infer |options_| from |main_thread_type|.
  switch (main_thread_type) {
    case base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT:
    case base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME:
      options_ |= TestBrowserThreadBundle::PLAIN_MAINLOOP;
      break;
    case base::test::ScopedTaskEnvironment::MainThreadType::IO:
      options_ |= TestBrowserThreadBundle::IO_MAINLOOP;
      break;
    case base::test::ScopedTaskEnvironment::MainThreadType::UI:
    case base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME:
      break;

    default:
      NOTREACHED();
  }

  Init();
}

TestBrowserThreadBundle::TestBrowserThreadBundle(int options)
    : TestBrowserThreadBundle(
          GetThreadTypeFromOptions(options),
          base::test::ScopedTaskEnvironment::ExecutionMode::ASYNC,
          options) {}

TestBrowserThreadBundle::~TestBrowserThreadBundle() {
  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But stopping a fake thread does not
  // automatically flush the message loop, so we have to do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  ui_thread_->Stop();
  base::RunLoop().RunUntilIdle();

  // Skip the following steps when RunAllTasksUntilIdle might result in a hang
  // (ExecutionMode::QUEUED) or for MainThreadType::MOCK_TIME where we haven't
  // enforced there being no pending tasks.
  if (main_thread_type() != MainThreadType::MOCK_TIME &&
      execution_control_mode() != ExecutionMode::QUEUED) {
    // This is required to ensure we run all remaining MessageLoop and
    // TaskScheduler tasks in an atomic step. This is a bit different than
    // production where the main thread is not flushed after it's done running
    // but this approach is preferred in unit tests as running more tasks can
    // merely uncover more issues (e.g. if a bad tasks is posted but never
    // blocked upon it could make a test flaky whereas by flushing we guarantee
    // it will blow up).
    RunAllTasksUntilIdle();
    CHECK(!MainThreadHasPendingTask());
  }

  BrowserTaskExecutor::ResetForTesting();

  // Run DestructionObservers before our fake threads go away to ensure
  // BrowserThread::CurrentlyOn() returns the results expected by the observers.
  NotifyDestructionObserversAndReleaseSequenceManager();

#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

void TestBrowserThreadBundle::Init() {
  // Check that the UI thread hasn't already been initialized. This will fail if
  // multiple TestBrowserThreadBundles are initialized in the same scope.
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI));

  // Check for conflicting options can't have two IO threads.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & REAL_IO_THREAD));
  // Check for conflicting main loop options.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & PLAIN_MAINLOOP));

#if defined(OS_WIN)
  // Similar to Chrome's UI thread, we need to initialize COM separately for
  // this thread as we don't call Start() for the UI TestBrowserThread; it's
  // already started!
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
  CHECK(com_initializer_->Succeeded());
#endif

  BrowserTaskExecutor::Create();

  if (options_ & IO_MAINLOOP)
    CHECK(base::MessageLoopCurrentForIO::IsSet());
  else if (!(options_ & PLAIN_MAINLOOP))
    CHECK(base::MessageLoopCurrentForUI::IsSet());

  // Set the current thread as the UI thread.
  ui_thread_ = std::make_unique<TestBrowserThread>(
      BrowserThread::UI, base::ThreadTaskRunnerHandle::Get());

  if (options_ & REAL_IO_THREAD) {
    io_thread_ = std::make_unique<TestBrowserThread>(BrowserThread::IO);
    io_thread_->StartIOThread();
  } else {
    io_thread_ = std::make_unique<TestBrowserThread>(
        BrowserThread::IO, base::ThreadTaskRunnerHandle::Get());
  }

  // Consider startup complete such that after-startup-tasks always run in
  // the scope of the test they were posted from (http://crbug.com/732018).
  SetBrowserStartupIsCompleteForTesting();
}

void TestBrowserThreadBundle::RunIOThreadUntilIdle() {
  // Use a RunLoop to run until idle if already on BrowserThread::IO (which is
  // the main thread unless using Options::REAL_IO_THREAD).
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::WaitableEvent io_thread_idle(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](base::WaitableEvent* io_thread_idle) {
            base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed)
                .RunUntilIdle();
            io_thread_idle->Signal();
          },
          Unretained(&io_thread_idle)));
  io_thread_idle.Wait();
}

}  // namespace content

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/views_mus_test_suite.h"

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "services/catalog/catalog.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/ws/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/mus/input_method_mus_test_api.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/test/fake_context_factory.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/views_mus_tests_catalog_source.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/test/views_test_helper_aura.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

namespace views {
namespace {

void EnsureCommandLineSwitch(const std::string& name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(name))
    cmd_line->AppendSwitch(name);
}

class ServiceManagerConnection {
 public:
  ServiceManagerConnection()
      : thread_("Persistent service_manager connections"),
        default_service_binding_(&default_service_) {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::Thread::Options options;
    thread_.StartWithOptions(options);
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceManagerConnection::SetUpConnectionsOnBackgroundThread,
            base::Unretained(this), &wait));
    wait.Wait();
  }

  ~ServiceManagerConnection() {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceManagerConnection::TearDownConnectionsOnBackgroundThread,
            base::Unretained(this), &wait));
    wait.Wait();
  }

  std::unique_ptr<MusClient> CreateMusClient() {
    MusClient::InitParams params;
    params.connector = GetConnector();
    params.identity = service_manager_identity_;
    return std::make_unique<MusClient>(params);
  }

 private:
  service_manager::Connector* GetConnector() {
    service_manager_connector_.reset();
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceManagerConnection::CloneConnector,
                                  base::Unretained(this), &wait));
    wait.Wait();
    DCHECK(service_manager_connector_);
    return service_manager_connector_.get();
  }

  void CloneConnector(base::WaitableEvent* wait) {
    service_manager_connector_ =
        default_service_binding_.GetConnector()->Clone();
    wait->Signal();
  }

  void SetUpConnectionsOnBackgroundThread(base::WaitableEvent* wait) {
    background_service_manager_ =
        std::make_unique<service_manager::BackgroundServiceManager>(
            nullptr, CreateViewsMusTestsCatalog());
    service_manager::mojom::ServicePtr service;
    default_service_binding_.Bind(mojo::MakeRequest(&service));
    background_service_manager_->RegisterService(
        service_manager::Identity(GetTestName(),
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(service), nullptr);
    service_manager_connector_ =
        default_service_binding_.GetConnector()->Clone();
    service_manager_identity_ = default_service_binding_.identity();
    wait->Signal();
  }

  void TearDownConnectionsOnBackgroundThread(base::WaitableEvent* wait) {
    default_service_binding_.Close();
    background_service_manager_.reset();
    wait->Signal();
  }

  // Returns the name of the test executable, e.g. "views_mus_unittests".
  std::string GetTestName() {
    base::FilePath executable = base::CommandLine::ForCurrentProcess()
                                    ->GetProgram()
                                    .BaseName()
                                    .RemoveExtension();
    return executable.MaybeAsASCII();
  }

  base::Thread thread_;
  std::unique_ptr<service_manager::BackgroundServiceManager>
      background_service_manager_;
  service_manager::Service default_service_;
  service_manager::ServiceBinding default_service_binding_;
  std::unique_ptr<service_manager::Connector> service_manager_connector_;
  service_manager::Identity service_manager_identity_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerConnection);
};

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus() {
    mus_client_ = service_manager_connection_.CreateMusClient();
    ViewsDelegate::GetInstance()->set_native_widget_factory(base::Bind(
        &PlatformTestHelperMus::CreateNativeWidget, base::Unretained(this)));
  }
  ~PlatformTestHelperMus() override {}

  // PlatformTestHelper:
  void OnTestHelperCreated(ViewsTestHelper* helper) override {
    static_cast<ViewsTestHelperAura*>(helper)->EnableMusWithWindowTreeClient(
        mus_client_->window_tree_client());
  }
  void SimulateNativeDestroy(Widget* widget) override {
    aura::WindowTreeHostMus* window_tree_host =
        static_cast<aura::WindowTreeHostMus*>(
            widget->GetNativeView()->GetHost());
    static_cast<aura::WindowTreeClientDelegate*>(mus_client_.get())
        ->OnEmbedRootDestroyed(window_tree_host);
  }

  void InitializeContextFactory(
      ui::ContextFactory** context_factory,
      ui::ContextFactoryPrivate** context_factory_private) override {
    *context_factory = &context_factory_;
    *context_factory_private = nullptr;
  }

 private:
  NativeWidget* CreateNativeWidget(const Widget::InitParams& init_params,
                                   internal::NativeWidgetDelegate* delegate) {
    NativeWidget* native_widget =
        mus_client_->CreateNativeWidget(init_params, delegate);
    if (!native_widget)
      return nullptr;

    // Disable sending KeyEvents to IME as tests aren't set up to wait for an
    // ack (and tests run concurrently).
    aura::WindowTreeHostMus* window_tree_host_mus =
        static_cast<aura::WindowTreeHostMus*>(
            static_cast<DesktopNativeWidgetAura*>(native_widget)->host());
    aura::InputMethodMusTestApi::Disable(window_tree_host_mus->input_method());
    return native_widget;
  }

  ServiceManagerConnection service_manager_connection_;
  std::unique_ptr<MusClient> mus_client_;
  ui::FakeContextFactory context_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

std::unique_ptr<PlatformTestHelper> CreatePlatformTestHelper() {
  return std::make_unique<PlatformTestHelperMus>();
}

}  // namespace

ViewsMusTestSuite::ViewsMusTestSuite(int argc, char** argv)
    : ViewsTestSuite(argc, argv), ipc_thread_("IPC thread") {}

ViewsMusTestSuite::~ViewsMusTestSuite() {}

void ViewsMusTestSuite::Initialize() {
  PlatformTestHelper::SetIsMus();
  // Let other services know that we're running in tests. Do this with a
  // command line flag to avoid making blocking calls to other processes for
  // setup for tests (e.g. to unlock the screen in the window manager).
  EnsureCommandLineSwitch(ws::switches::kUseTestConfig);

  EnsureCommandLineSwitch(switches::kOverrideUseSoftwareGLForTests);

  ViewsTestSuite::Initialize();

  // NOTE: this has to be after ViewsTestSuite::Initialize() as
  // TestSuite::Initialize() resets kEnableFeatures and the command line.
  feature_list_.InitAndEnableFeature(features::kMash);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableFeatures, features::kMash.name);

  PlatformTestHelper::set_factory(base::Bind(&CreatePlatformTestHelper));

  mojo::core::Init();
  ipc_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      ipc_thread_.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
}

void ViewsMusTestSuite::InitializeEnv() {
  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);
}

void ViewsMusTestSuite::DestroyEnv() {
  env_.reset();
}

}  // namespace views

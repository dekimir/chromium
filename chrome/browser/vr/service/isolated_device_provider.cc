// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/isolated_device_provider.h"

#include "chrome/browser/vr/service/vr_ui_host.h"
#include "content/public/common/service_manager_connection.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/isolated_gamepad_data_fetcher.h"
#include "services/service_manager/public/cpp/connector.h"

namespace vr {

void IsolatedVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(device::mojom::XRDeviceId,
                                 device::mojom::VRDisplayInfoPtr,
                                 device::mojom::XRRuntimePtr)>
        add_device_callback,
    base::RepeatingCallback<void(device::mojom::XRDeviceId)>
        remove_device_callback,
    base::OnceClosure initialization_complete) {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(
      device::mojom::kVrIsolatedServiceName,
      mojo::MakeRequest(&device_provider_));

  device_provider_.set_connection_error_handler(base::BindOnce(
      &IsolatedVRDeviceProvider::OnServerError, base::Unretained(this)));

  device::mojom::IsolatedXRRuntimeProviderClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  device_provider_->RequestDevices(std::move(client));

  add_device_callback_ = std::move(add_device_callback);
  remove_device_callback_ = std::move(remove_device_callback);
  initialization_complete_ = std::move(initialization_complete);
}

bool IsolatedVRDeviceProvider::Initialized() {
  return initialized_;
}

void IsolatedVRDeviceProvider::OnDeviceAdded(
    device::mojom::XRRuntimePtr device,
    device::mojom::IsolatedXRGamepadProviderFactoryPtr gamepad_factory,
    device::mojom::XRCompositorHostPtr compositor_host,
    device::mojom::VRDisplayInfoPtr display_info) {
  device::mojom::XRDeviceId id = display_info->id;
  add_device_callback_.Run(id, display_info.Clone(), std::move(device));

  auto ui_host = (*VRUiHost::GetFactory())(std::move(display_info),
                                           std::move(compositor_host));
  ui_host_map_.insert(std::make_pair(id, std::move(ui_host)));
  device::IsolatedGamepadDataFetcher::Factory::AddGamepad(
      id, std::move(gamepad_factory));
}

void IsolatedVRDeviceProvider::OnDeviceRemoved(device::mojom::XRDeviceId id) {
  remove_device_callback_.Run(id);
  ui_host_map_.erase(id);
  device::IsolatedGamepadDataFetcher::Factory::RemoveGamepad(id);
}

void IsolatedVRDeviceProvider::OnServerError() {
  // An error occurred - any devices we have added are now disconnected and
  // should be removed.
  for (auto& entry : ui_host_map_) {
    auto id = entry.first;
    remove_device_callback_.Run(id);
    device::IsolatedGamepadDataFetcher::Factory::RemoveGamepad(id);
  }
  ui_host_map_.clear();

  // At this point, XRRuntimeManager may be blocked waiting for us to return
  // that we've enumerated all runtimes/devices.  If we lost the connection to
  // the service, we won't ever get devices, so report we are done now.
  // This will unblock WebXR/WebVR promises so they can reject indicating we
  // never found devices.
  if (!initialized_)
    OnDevicesEnumerated();
}

void IsolatedVRDeviceProvider::OnDevicesEnumerated() {
  initialized_ = true;
  std::move(initialization_complete_).Run();
}

IsolatedVRDeviceProvider::IsolatedVRDeviceProvider() : binding_(this) {}

IsolatedVRDeviceProvider::~IsolatedVRDeviceProvider() {
  for (auto& entry : ui_host_map_) {
    auto device_id = entry.first;
    device::IsolatedGamepadDataFetcher::Factory::RemoveGamepad(device_id);
  }
  // Default destructor handles renderer_host_map_ cleanup.
}

}  // namespace vr

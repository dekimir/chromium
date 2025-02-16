// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_LE_SCAN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_LE_SCAN_H_

#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan_filter_init.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BluetoothLEScan final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BluetoothLEScan* Create(mojo::BindingId,
                                 Bluetooth*,
                                 bool keep_repeated_devices,
                                 bool accept_all_advertisements);

  BluetoothLEScan(mojo::BindingId,
                  Bluetooth*,
                  bool keep_repeated_devices,
                  bool accept_all_advertisements);

  // IDL exposed interface:
  const HeapVector<Member<BluetoothLEScanFilterInit>>& filters() const;
  bool keepRepeatedDevices() const;
  bool acceptAllAdvertisements() const;
  bool active() const;
  bool stop();

  // Interface required by garbage collection.
  void Trace(blink::Visitor*) override;

 private:
  mojo::BindingId id_;
  HeapVector<Member<BluetoothLEScanFilterInit>> filters_;
  Member<Bluetooth> bluetooth_;
  const bool keep_repeated_devices_;
  const bool accept_all_advertisements_;
  bool is_active_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_LE_SCAN_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_PORT_IMPL_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_PORT_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/serial/serial_io_handler.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

// TODO(leonhsl): Merge this class with SerialIoHandler if/once
// SerialIoHandler is exposed only via the Device Service.
// crbug.com/748505
// This class must be constructed and run on IO thread.
class SerialPortImpl : public mojom::SerialPort {
 public:
  static void Create(
      const std::string& path,
      mojom::SerialPortRequest request,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  explicit SerialPortImpl(
      const std::string& path,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~SerialPortImpl() override;

 private:
  // mojom::SerialPort methods:
  void Open(mojom::SerialConnectionOptionsPtr options,
            OpenCallback callback) override;
  void Read(uint32_t bytes, ReadCallback callback) override;
  void Write(const std::vector<uint8_t>& data, WriteCallback callback) override;
  void CancelRead(mojom::SerialReceiveError reason) override;
  void CancelWrite(mojom::SerialSendError reason) override;
  void Flush(FlushCallback callback) override;
  void GetControlSignals(GetControlSignalsCallback callback) override;
  void SetControlSignals(mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override;
  void ConfigurePort(mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override;
  void GetPortInfo(GetPortInfoCallback callback) override;
  void SetBreak(SetBreakCallback callback) override;
  void ClearBreak(ClearBreakCallback callback) override;

  std::string path_;
  scoped_refptr<device::SerialIoHandler> io_handler_;

  DISALLOW_COPY_AND_ASSIGN(SerialPortImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_PORT_IMPL_H_

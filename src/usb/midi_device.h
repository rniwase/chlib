// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __midi_device_h__
#define __midi_device_h__

#include "usb_device.h"

struct midi_device {
  void (*recv)(const uint8_t* buffer, uint8_t len);
  void (*connected)(void);
  void (*suspend)(void);
  void (*bus_reset)(void);
  const char *serial_number;
};

bool midi_device_send(const uint8_t* data, uint8_t len);
void midi_device_allow_recv(bool allow);
void midi_device_init(struct midi_device* device);
void midi_device_deinit(void);

#endif  // __midi_device_h__

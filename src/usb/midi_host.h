// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __midi_host_h__
#define __midi_host_h__

#include <stdbool.h>
#include <stdint.h>

#include "usb_host.h"

struct midi_host {
  // Callback functions
  void (*recv)(const uint8_t* data, uint16_t size);
  void (*detected)(void);
  void (*disconnected)(void);
  // Detected endpoints
  int8_t detected_ep_in;
  int8_t detected_ep_out;
};

void midi_host_init(struct midi_host* midi_host);
void midi_host_deinit(void);
void midi_host_poll(void);
bool midi_host_ready(void);
bool midi_host_recv(void);
bool midi_host_send(uint8_t* data, uint8_t size);

#endif  // __midi_host_h__

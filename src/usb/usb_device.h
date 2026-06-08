// Copyright 2021 Takashi Toyoshima <toyoshim@gmail.com>. All rights reserved.
// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __usb_device_h__
#define __usb_device_h__

#include <stdbool.h>
#include <stdint.h>

#include "../interrupt.h"
#include "usb.h"

extern void usb_int(void) __interrupt(INT_NO_USB) __using(1);

struct usb_device {
  uint8_t (*get_descriptor_size)(uint8_t type, uint8_t no);
  const uint8_t* (*get_descriptor)(uint8_t type, uint8_t no);
  bool (*setup)(const struct usb_setup_req* req, uint8_t* buffer, uint8_t* len);
  bool (*ep_out)(const uint8_t* buffer, uint8_t len);
  void (*connected)(void);
  void (*suspend)(void);
  void (*bus_reset)(void);
};

enum {
  // states
  UD_STATE_IDLE = 0,
  UD_STATE_SETUP = 1,
  UD_STATE_READY = 2,
};

void usb_device_init(struct usb_device* device);
void usb_device_deinit(void);
uint8_t usb_device_state(void);
bool usb_ep1_is_send_ready(void);
void usb_ep1_send(const uint8_t* buffer, uint8_t len);
void usb_ep1out_set_hs(bool ack);

#endif  // __usb_device_h__

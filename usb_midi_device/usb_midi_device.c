// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb/midi_device.h"
#include "usb/midi_util.h"
#include "ch559.h"
#include "serial.h"
#include "uart1.h"
#include "fifo.h"

static char chip_uid_hex[9];

static struct uart1int u1int;
static struct midi_device usb_midi_device;

static bool device_connected;

static struct fifo fifo_u2m;
static struct fifo fifo_m2u;
static struct m2u_data m2u;
static struct u2m_data u2m;
static uint8_t event_buf[64];
static uint8_t event_len = 0;

void init_buffers(void) {
  fifo_init(&fifo_u2m);
  fifo_init(&fifo_m2u);
  m2u_init(&m2u, 0);  // Cable Number = 0
  event_len = 0;
  Serial.printf("Initialize buffers\n");
}

void uart1_rx_recv_ready(void) {
  fifo_wr(&fifo_m2u, uart1_rx_recv());
}

void uart1_thr_empty(void) {
  uint8_t txdata;
  if (fifo_rd(&fifo_u2m, &txdata)) {
    uart1_tx_send(txdata);
  } else {
    midi_device_allow_recv(true);
  }
}

void usb_midi_device_connected(void) {
  Serial.printf("USB Device connected\n");
  device_connected = true;
  uart1_set_interrupt(true, true);
}

void usb_midi_device_suspend(void) {
  Serial.printf("USB Device suspend\n");
  uart1_set_interrupt(false, false);
  device_connected = false;
  init_buffers();
}

void usb_midi_device_bus_reset(void) {
  Serial.printf("USB Device bus reset\n");
  uart1_set_interrupt(false, false);
  device_connected = false;
  init_buffers();
}

void usb_midi_device_recv(const uint8_t* buffer, uint8_t len) {
  uint8_t txdata;
  bool res;
  for (uint8_t i = 0; i < len; i = i + 4) {
    res = u2m_convert(&u2m, buffer + i);
    if (!res || (u2m.cn != 0)) {  // Conversion failed or Cable Number is not 0
      continue;
    }
    fifo_wr_data(&fifo_u2m, u2m.message, u2m.len);
  }
  if (uart1_tx_empty()) {
    res = fifo_rd(&fifo_u2m, &txdata);
    if (!res)
      return;
    uart1_tx_send(txdata);
  }
}

void main(void) {
  uint8_t rxdata;

  initialize();

  disable_interrupt();

  u1int.recv_ready = uart1_rx_recv_ready;
  u1int.thr_empty = uart1_thr_empty;

  uart1_init(UART1_P2, UART1_31250, &u1int);

  get_chip_uid_hex(chip_uid_hex);

  Serial.printf("CH559 USB-MIDI Device Example\n");
  Serial.printf("Chip UID: %s\n", chip_uid_hex);

  usb_midi_device.recv = usb_midi_device_recv;
  usb_midi_device.connected = usb_midi_device_connected;
  usb_midi_device.suspend = usb_midi_device_suspend;
  usb_midi_device.bus_reset = usb_midi_device_bus_reset;
  usb_midi_device.serial_number = chip_uid_hex;

  init_buffers();

  reset_interrupt_priority();
  set_interrupt_priority(INT_NO_USB);
  set_interrupt_priority(INT_NO_UART1);

  enable_interrupt();

  delay(20);

  midi_device_init(&usb_midi_device);

  for (;;) {
    if (usb_device_state() != 2)
      continue;
    if (!fifo_rd(&fifo_m2u, &rxdata))
      continue;
    if (!m2u_convert(&m2u, rxdata))
      continue;

    event_buf[event_len + 0] = m2u.event[0];
    event_buf[event_len + 1] = m2u.event[1];
    event_buf[event_len + 2] = m2u.event[2];
    event_buf[event_len + 3] = m2u.event[3];
    event_len += 4;

    if (midi_device_send(event_buf, event_len)) {
      event_len = 0;
    }

    if (event_len == 64) {
      // USB TX Buffer is full, data discarded
      event_len = 0;
    }
  }
}

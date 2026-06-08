// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb/midi_host.h"
#include "usb/midi_util.h"
#include "ch559.h"
#include "serial.h"
#include "uart1.h"
#include "fifo.h"

static struct uart1int u1int;
static struct midi_host usb_midi_host;

static bool host_out_detected;
static bool host_in_detected;

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
  }
}

void usb_midi_host_detected(void) {
  Serial.printf("USB Host connected\n");
  if (usb_midi_host.detected_ep_in != -1) {
    host_in_detected = true;
    Serial.printf("USB MIDI Input detected in Endpoint %d\n", usb_midi_host.detected_ep_in);
  }
  if (usb_midi_host.detected_ep_out != -1) {
    host_out_detected = true;
    Serial.printf("USB MIDI Output detected in Endpoint %d\n", usb_midi_host.detected_ep_out);
  }
  uart1_set_interrupt(host_in_detected, host_out_detected);
}

void usb_midi_host_disconnected(void) {
  Serial.println("USB Host disconnected");
  uart1_set_interrupt(false, false);
  host_in_detected = false;
  host_out_detected = false;
  init_buffers();
}

void usb_midi_host_recv(uint8_t* data, uint16_t size) {
  uint8_t txdata;
  bool res;
  for (uint16_t i = 0; i < size; i+=4) {
    res = u2m_convert(&u2m, data + i);
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

  Serial.printf("CH559 USB-MIDI Host Example\n");

  usb_midi_host.recv = usb_midi_host_recv;
  usb_midi_host.detected = usb_midi_host_detected;
  usb_midi_host.disconnected = usb_midi_host_disconnected;

  init_buffers();

  reset_interrupt_priority();
  set_interrupt_priority(INT_NO_USB);
  set_interrupt_priority(INT_NO_UART1);

  enable_interrupt();

  delay(20);

  midi_host_init(&usb_midi_host);
  delay(100);

  for (;;) {
    do {
      midi_host_poll();
    } while (!midi_host_ready());

    // USB -> MIDI
    if (host_in_detected) {
      midi_host_recv();
    }

    do {
      midi_host_poll();
    } while (!midi_host_ready());

    // MIDI -> USB
    if (host_out_detected) {
      if (!fifo_rd(&fifo_m2u, &rxdata))
        continue;
      if (!m2u_convert(&m2u, rxdata))
        continue;

      event_buf[event_len + 0] = m2u.event[0];
      event_buf[event_len + 1] = m2u.event[1];
      event_buf[event_len + 2] = m2u.event[2];
      event_buf[event_len + 3] = m2u.event[3];
      event_len += 4;

      if (event_len != 0) {
        if (midi_host_send(event_buf, event_len)) {
          event_len = 0;
        }
      }

      if (event_len == 64) {
        // USB TX Buffer is full, data discarded
        event_len = 0;
      }
    }
  }
}

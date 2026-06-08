// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __midi_util_h__
#define __midi_util_h__

#include <stdbool.h>
#include <stdint.h>

struct m2u_data {
  uint8_t event[4];  // Converted USB-MIDI Event packet
  bool is_sysmsg;  // Data is System message

  // Internal variables
  uint8_t cn;  // Cable Number (0 - 15)
  uint8_t status;  // Stored status byte
  int8_t vlen;  // Expected voice message length (-1: SysEx)
  int8_t count;  // Received count
};

struct u2m_data {
  uint8_t message[3];  // Converted UART-MIDI message
  uint8_t len;  // Valid message length (0: Invalid)
  uint8_t cn;  // Cable Number (0 - 15)
  bool is_sysmsg;  // Data is System message
};

void m2u_init(struct m2u_data* m2u, uint8_t cn);
bool m2u_convert(struct m2u_data* m2u, uint8_t recv);
bool u2m_convert(struct u2m_data* u2m, const uint8_t* event);

#endif  // __midi_util_h__

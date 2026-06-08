// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fifo.h"

void fifo_init(struct fifo* b) {
  b->idx_wr = 0;
  b->idx_rd = 0;
  b->count = 0;
  b->full = false;
}

bool fifo_wr(struct fifo* b, uint8_t data) {
  if (b->full) {
    return false;  // return false if FIFO is full
  }
  b->buffer[b->idx_wr] = data;
  b->idx_wr = (b->idx_wr + 1) % FIFO_SIZE;
  if (b->idx_wr == b->idx_rd) {
    b->full = true;
  }
  b->count++;
  return true;
}

uint8_t fifo_wr_data(struct fifo* b, const uint8_t* data, uint8_t len) {
  uint8_t i;
  bool res;
  for (i = 0; i < len; i++) {
    res = fifo_wr(b, data[i]);
    if (!res) {  // If the FIFO is full
      return i;  // return the number of bytes written after interruption
    }
  }
  return len;
}

bool fifo_rd(struct fifo* b, uint8_t* data) {
  if ((b->idx_rd == b->idx_wr) && (b->full == 0)) {  // FIFO is empty
    return false;
  }
  b->full = false;
  *data = b->buffer[b->idx_rd];
  b->idx_rd = ((b->idx_rd + 1) % FIFO_SIZE);
  b->count--;
  return true;
}

uint8_t fifo_rd_data(struct fifo* b, uint8_t* data, uint8_t len) {
  uint8_t i;
  bool res;
  for (i = 0; i < len; i++) {
    res = fifo_rd(b, data + i);
    if (!res) {  // If the FIFO is empty
      return i;  // return the number of bytes read after interruption
    }
  }
  return len;
}

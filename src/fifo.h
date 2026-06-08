// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __fifo_h__
#define __fifo_h__

#include <stdbool.h>
#include <stdint.h>

#define FIFO_SIZE 64

struct fifo {
  uint8_t buffer[FIFO_SIZE];
  uint8_t idx_wr;
  uint8_t idx_rd;
  uint8_t count;
  bool full;
};

void fifo_init(struct fifo* b);
bool fifo_wr(struct fifo* b, uint8_t data);
uint8_t fifo_wr_data(struct fifo* b, const uint8_t* data, uint8_t len);
bool fifo_rd(struct fifo* b, uint8_t* data);
uint8_t fifo_rd_data(struct fifo* b, uint8_t* data, uint8_t len);

#endif  // __fifo_h__

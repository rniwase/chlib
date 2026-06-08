// Copyright 2021 Takashi Toyoshima <toyoshim@gmail.com>. All rights reserved.
// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "uart1.h"

#include "io.h"

static struct uart1int* u1int = 0;

void uart1_interrupt(void) __interrupt(INT_NO_UART1) __using(0) {
  switch (SER1_IIR & 0x0F) {
    case 0x02:  // U1_INT_THR_EMPTY
      if (u1int->thr_empty) {
        u1int->thr_empty();
      }
      return;
    case 0x04:  // U1_INT_RECV_RDY
    case 0x0C:  // U1_INT_RECV_TOUT
      if (u1int->recv_ready) {
        u1int->recv_ready();
      }
      return;
    default:
      return;
  }
}

void uart1_set_interrupt(bool tx_empty, bool rx_ready) {
  // SER1_FCR &= ~((1 << 7) | (1 << 6));  // {bFCR_FIFO_TRIG1, bFCR_FIFO_TRIG0} = {0, 0}
  SER1_FCR |= bFCR_R_FIFO_CLR | bFCR_T_FIFO_CLR;
  SER1_MCR |= bMCR_OUT2;
  if (rx_ready)
    SER1_IER |= bIER_RECV_RDY;  // enable UART1 RX interrupt
  if (tx_empty)
    SER1_IER |= bIER_THR_EMPTY;  // enable UART1 TX empty interrupt
  // SER1_IER |= bIER_LINE_STAT;  // enable UART1 RX line status interrupt
  IE_UART1 = 1;  // Enable UART1 interrupt
  // EA = 1;       // Enable interrupt
}

void uart1_init(uint8_t options, uint8_t speed, struct uart1int* u1i) {
  if (options & UART1_RS485) {
    // Enable half-duplex mode.
    SER1_MCR |= bMCR_HALF;
    // Disable ALE clock.
    XBUS_AUX &= ~bALE_CLK_EN;
    // Use only XA/XB pins.
    SER1_IER &= ~bIER_PIN_MOD0;
    SER1_IER |= bIER_PIN_MOD1;
  } else {
    // Disable half-duplex mode.
    SER1_MCR &= ~bMCR_HALF;
    // Enable AL clock to disable iRS485 mode.
    XBUS_AUX |= bALE_CLK_EN;
    if (options & UART1_P4) {
      // RXD1/TXD1 connect P4.0/P4.4.
      SER1_IER |= bIER_PIN_MOD0;
      SER1_IER &= ~bIER_PIN_MOD1;
      P4_DIR &= ~(1 << 0);
      P4_DIR |= (1 << 4);
    } else {
      // RXD1/TXD1 connect P2.6/P2.7.
      SER1_IER &= ~bIER_PIN_MOD0;
      SER1_IER |= bIER_PIN_MOD1;
      P2_DIR &= ~(1 << 6);
      P2_DIR |= (1 << 7);
    }
  }

  // no parity, stop bit 1-bit, no interrupts by default
  SER1_LCR = bLCR_WORD_SZ0 | bLCR_WORD_SZ1;  // data length 8-bits

  uart1_set_speed(speed);

  // set interrupt callback
  if (u1i != 0)
    u1int = u1i;
}

void uart1_set_speed(uint8_t speed) {
  SER1_FCR &= ~bFCR_FIFO_EN;  // Disable FIFO
  SER1_LCR |= bLCR_DLAB;      // Allow SER1_DIV, SER1_DLM, and SER1_DLL use
  SER1_DIV = 1;
  switch (speed) {
    case UART1_115200:
      // { SER1_DLM, SER1_DLL } = Fsys(48M) * 2 / SER1_DIV / 16 /
      // baudrate(115200)
      SER1_DLM = 0;
      SER1_DLL = 52;  // should be set before enabling FIFO
      break;
    case UART1_1M:
      // { SER1_DLM, SER1_DLL } = Fsys(48M) * 2 / SER1_DIV / 16 / baudrate(1M)
      SER1_DLM = 0;
      SER1_DLL = 6;  // should be set before enabling FIFO
      break;
    case UART1_3M:
      // { SER1_DLM, SER1_DLL } = Fsys(48M) * 2 / SER1_DIV / 16 / baudrate(3M)
      SER1_DLM = 0;
      SER1_DLL = 2;  // should be set before enabling FIFO
      break;
    case UART1_31250:
      // { SER1_DLM, SER1_DLL } = Fsys(48M) * 2 / SER1_DIV / 16 / baudrate(31250)
      SER1_DLM = 0;
      SER1_DLL = 192;  // should be set before enabling FIFO
      break;
  }
  SER1_LCR &= ~bLCR_DLAB;
  SER1_FCR = bFCR_FIFO_EN;  // Enable FIFO
}

inline bool uart1_tx_empty(void) {
  return (SER1_LSR & bLSR_T_FIFO_EMP) != 0;
}

inline void uart1_tx_send(uint8_t val) {
  SER1_FIFO = val;
}

inline bool uart1_rx_ready(void) {
  return (SER1_LSR & bLSR_DATA_RDY) != 0;
}

inline uint8_t uart1_rx_recv(void) {
  return SER1_FIFO;
}

// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "interrupt.h"
#include "io.h"

inline void enable_interrupt(void) {
  EA = 1;
}

inline void disable_interrupt(void) {
  EA = 0;
}

inline void reset_interrupt_priority(void) {
  IP = 0;
  IP_EX = 0;
}

void set_interrupt_priority(uint8_t int_no) {
  switch (int_no) {
    case INT_NO_INT0  : IP_PX0 = 1; break;
    case INT_NO_TMR0  : IP_PT0 = 1; break;
    case INT_NO_INT1  : IP_PX1 = 1; break;
    case INT_NO_TMR1  : IP_PT1 = 1; break;
    case INT_NO_UART  : IP_PS = 1; break;
    case INT_NO_TMR2  : IP_PT2 = 1; break;
    case INT_NO_SPI0  : IP_EX |= bIP_SPI0; break;
    case INT_NO_TMR3  : IP_EX |= bIP_TMR3; break;
    case INT_NO_USB   : IP_EX |= bIP_USB; break;
    case INT_NO_ADC   : IP_EX |= bIP_ADC; break;
    case INT_NO_UART1 : IP_EX |= bIP_UART1; break;
    case INT_NO_PWM1  : IP_EX |= bIP_PWM1; break;
    case INT_NO_GPIO  : IP_EX |= bIP_GPIO; break;
    case INT_NO_WDOG  : break;  // watchdog interrupt priority is unsupported
  }
}

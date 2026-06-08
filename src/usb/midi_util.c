// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midi_util.h"

/*  SysEx UART-MIDI to USB-MIDI Event packet conversion example
    recv b0 b1 b2 b3 vlen count return
-------------------------------------- No data SysEx
SOX F0   -- F0 00 00   -1     1  false
EOX F7   06 -- F7 00    0     -   true -> Event packet: 06 F0 F7 00
-------------------------------------- 1-byte data SysEx
SOX F0   -- F0 00 00   -1     1  false
DAT _g   -- -- _g --    -     2  false
EOX F7   07 -- -- F7    0     -   true -> Event packet: 07 F0 _g F7
-------------------------------------- 2-byte data SysEx
SOX F0   -- F0 00 00   -1     1  false
DAT _h   -- -- _h --    -     2  false
DAT _i   04 -- -- _i    -  3->0   true -> Event packet: 04 F0 _h _i
EOX F7   05 F7 00 00    0     -   true -> Event packet: 05 F7 00 00
-------------------------------------- 3-byte data SysEx
SOX F0   -- F0 00 00   -1     1  false
DAT _j   -- -- _j --    -     2  false
DAT _k   04 -- -- _k    -  3->0   true -> Event packet: 04 F0 _j _k
DAT _l   -- _l -- --    -     1  false
EOX F7   06 -- F7 00    0     -   true -> Event packet: 06 _l F7 00
-------------------------------------- 4-byte data SysEx
SOX F0   -- F0 00 00   -1     1  false
DAT _m   -- -- _m --    -     2  false
DAT _n   04 -- -- _n    -  3->0   true -> Event packet: 04 F0 _m _n
DAT _o   -- _o -- --    -     1  false
DAT _p   -- -- _p --    -     2  false
EOX F7   07 -- -- F7    0     -   true -> Event packet: 07 _o _p F7
-------------------------------------- Invalid EOX
EOX F7   05 F7 00 00    0     1   true -> Event packet: 05 F7 00 00
--------------------------------------
*/

/* m2u_init - Initialize m2u_data structure      */
/*   Parameters                                  */
/*     struct m2u_data* m2u : m2u_data structure */
/*     uint8_t cn           : Cable Number       */
void m2u_init(struct m2u_data* m2u, uint8_t cn) {
  m2u->cn = cn << 4;
  m2u->vlen = 0;
  m2u->count = 0;
}

/* m2u_convert - Convert UART-MIDI data to USB-MIDI Event packet             */
/*   Parameters                                                              */
/*     struct m2u_data* m2u : m2u_data structure                             */
/*     uint8_t recv         : Receive data from UART                         */
/*   Return : bool                                                           */
/*     true                 : Event packet is ready                          */
/*     false                : Event packet is not ready or conversion failed */
bool m2u_convert(struct m2u_data* m2u, uint8_t recv) {
  if (recv & 0x80) {  // Status byte
    m2u->status = recv;
    m2u->is_sysmsg = (m2u->status & 0xF0) == 0xF0;

    switch (m2u->status & 0xF0) {
      // Channel voice message
      case 0x80:  // Note off
      case 0x90:  // Note on
      case 0xA0:  // Polyphonic key pressure (After touch)
      case 0xB0:  // Control change
      case 0xE0:  // Pitch bend change
        m2u->event[0] = m2u->cn | (m2u->status >> 4);  // Channel voice message
        m2u->event[1] = m2u->status;
        m2u->event[2] = 0x00;
        m2u->event[3] = 0x00;
        m2u->count = 1;
        m2u->vlen = 3;
        return false;
      case 0xC0:  // Program change
      case 0xD0:  // Channel pressure (After touch)
        m2u->event[0] = m2u->cn | (m2u->status >> 4);  // Channel voice message
        m2u->event[1] = m2u->status;
        m2u->event[2] = 0x00;
        m2u->event[3] = 0x00;
        m2u->count = 1;
        m2u->vlen = 2;
        return false;

      // System message
      case 0xF0:
        switch (m2u->status) {
          // System realtime message
          case 0xF8:  // Timing clock
          case 0xFA:  // Start
          case 0xFB:  // Continue
          case 0xFC:  // Stop
          case 0xFE:  // Active sensing
          case 0xFF:  // System reset
            m2u->event[0] = m2u->cn | 0x0F;  // Single byte
            m2u->event[1] = m2u->status;
            m2u->event[2] = 0x00;
            m2u->event[3] = 0x00;
            m2u->count = 1;
            m2u->vlen = 0;
            return true;

          // System common message
          case 0xF2:  // Song position pointor
            m2u->event[0] = m2u->cn | 0x03;  // 3-byte System common message
            m2u->event[1] = m2u->status;
            m2u->event[2] = 0x00;
            m2u->event[3] = 0x00;
            m2u->count = 1;
            m2u->vlen = 3;
            return false;
          case 0xF1:  // MIDI Timecode quarter frame
          case 0xF3:  // Song select
            m2u->event[0] = m2u->cn | 0x02;  // 2-byte System common message
            m2u->event[1] = m2u->status;
            m2u->event[2] = 0x00;
            m2u->event[3] = 0x00;
            m2u->count = 1;
            m2u->vlen = 2;
            return false;
          case 0xF6:  // Tune request
            m2u->event[0] = m2u->cn | 0x05;  // 1-byte System common message
            m2u->event[1] = m2u->status;
            m2u->event[2] = 0x00;
            m2u->event[3] = 0x00;
            m2u->count = 1;
            m2u->vlen = 0;
            return true;

          case 0xF0:  // Start of exclusive (SOX)
            // CIN(m2u->buffer[0]) is unexpectable at this time
            m2u->event[1] = m2u->status;
            m2u->event[2] = 0x00;
            m2u->event[3] = 0x00;
            m2u->count = 1;
            m2u->vlen = -1;
            return false;
          case 0xF7:  // End of exclusive (EOX)
            if (m2u->vlen == -1) {
              if (m2u->count == 1) {  // No data SysEx or 1-byte continuous SysEx
                m2u->event[0] = m2u->cn | 0x06;  // EOX with following 2-byte
                m2u->event[2] = 0xF7;
                m2u->event[3] = 0x00;
                m2u->vlen = 0;
                return true;
              }
              if (m2u->count == 2) {  // 1-byte SysEx or 2-byte continuous SysEx
                m2u->event[0] = m2u->cn | 0x07;  // EOX with following 3-byte
                m2u->event[3] = 0xF7;
                m2u->vlen = 0;
                return true;
              }
            }
            // No continuous or invalid EOX
            m2u->event[0] = m2u->cn | 0x05;  // EOX with following 1-byte
            m2u->event[1] = m2u->status;
            m2u->event[2] = 0x00;
            m2u->event[3] = 0x00;
            m2u->vlen = 0;
            return true;
          default:  // Invalid system message
            break;
        }
        break;
      default:  // Invalid status byte
        break;
    }

    m2u->vlen = 0;
    return false;

  } else {  // Data byte
    if (m2u->vlen > 0) {
      if (m2u->count == 0) {  // Running status
        m2u->event[0] = m2u->cn | (m2u->status >> 4);
        m2u->event[1] = m2u->status;
        m2u->count++;
      }
      if (m2u->count < m2u->vlen) {
        m2u->event[m2u->count + 1] = recv;  // Byte2 or 3
        m2u->count++;
      }
      if (m2u->count == m2u->vlen) {
        m2u->count = 0;
        return true;
      }
    } else if (m2u->vlen == -1) {  // SysEx data
      m2u->count++;
      m2u->event[m2u->count] = recv;
      if (m2u->count == 3) {
        m2u->event[0] = m2u->cn | 0x04;  // 3 byte SysEx
        m2u->count = 0;
        return true;
      }
    } else {  // Invalid data byte
      return false;
    }
  }

  return false;
}

// Conversion table for USB-MIDI Code Index to Legacy MIDI length
static const uint8_t table_code_idx_u2m_len[16] = {
      // code_idx:
  0,  //   0x0 -> Invalid
  0,  //   0x1 -> Invalid
  2,  //   0x2 -> 2-byte System common message
  3,  //   0x3 -> 3-byte System common message
  3,  //   0x4 -> SysEx starts or continues
  1,  //   0x5 -> 1-byte System common message or SysEx ends with following 1 bytes
  2,  //   0x6 -> SysEx ends with following 2 bytes
  3,  //   0x7 -> SysEx ends with following 3 bytes
  3,  //   0x8 -> Note off
  3,  //   0x9 -> Note on
  3,  //   0xA -> Polyphonic key pressure (After touch)
  3,  //   0xB -> Control change
  2,  //   0xC -> Program change
  2,  //   0xD -> Channel pressure (After touch)
  3,  //   0xE -> Pitch bend change
  1,  //   0xF -> Single byte
};

/* u2m_convert - Convert USB-MIDI Event packet to UART-MIDI data */
/*   Parameters                                                  */
/*     struct u2m_data* u2m : u2m_data structure                 */
/*     uint8_t* event : USB-MIDI Event packet, 4 bytes required  */
/*   Return : bool                                               */
/*    true  : Successful conversion                              */
/*    false : Failed to convert                                  */
bool u2m_convert(struct u2m_data* u2m, const uint8_t* event) {
  uint8_t code_idx = event[0] & 0x0F;  // Code Index Number
  u2m->cn = event[0] >> 4;  // Cable Number
  u2m->message[0] = event[1];
  u2m->message[1] = event[2];
  u2m->message[2] = event[3];

  u2m->len = table_code_idx_u2m_len[code_idx];
  u2m->is_sysmsg = ((code_idx >= 0x2) && (code_idx <= 0x7)) || ((code_idx == 0xF) && ((event[1] & 0xF0) == 0xF0));
  return u2m->len != 0;
}

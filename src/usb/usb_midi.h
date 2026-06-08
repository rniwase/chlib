// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __usb_midi_h__
#define __usb_midi_h__

#include "usb.h"

struct usb_desc_cs_ac_interface {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint16_t bcdADC;
  uint16_t wTotalLength;
  uint8_t bInCollection;
  uint8_t baInterfaceNr1;
};

struct usb_desc_cs_ms_interface {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint16_t bcdADC;
  uint16_t wTotalLength;
};

struct usb_desc_midi_in_jack {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bJackType;
  uint8_t bJackID;
  uint8_t iJack;
};

struct usb_desc_midi_out_jack {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bJackType;
  uint8_t bJackID;
  uint8_t bNrInputPins;
  uint8_t baSourceID1;
  uint8_t baSourcePin1;
  uint8_t iJack;
};

struct usb_desc_cs_midi_endpoint {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bNumEmbMIDIJack;
  uint8_t baAssocJackID1;
};

struct usb_desc_cs_interface {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  void* content;
};

struct usb_desc_cs_interface_ac {
  uint16_t bcdADC;
  uint16_t wTotalLength;
  uint8_t bInCollection;
  uint8_t baInterfaceNr1;
};

struct usb_desc_cs_interface_ms {
  uint16_t bcdADC;
  uint16_t wTotalLength;
};

struct usb_desc_cs_midi_in_jack {
  uint8_t bJackType;
  uint8_t bJackID;
  uint8_t iJack;
};

struct usb_desc_cs_midi_out_jack {
  uint8_t bJackType;
  uint8_t bJackID;
  uint8_t bNrInputPins;
  uint8_t baSourceID1;
  uint8_t baSourcePin1;
  uint8_t iJack;
};

struct usb_desc_cs_endpoint {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  void* content;
};

struct usb_desc_cs_endpoint_midi {
  uint8_t bNumEmbMIDIJack;
  uint8_t baAssocJackID1;
};

#endif  // __usb_midi_h__

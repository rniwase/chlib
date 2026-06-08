// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midi_device.h"
#include "usb_midi.h"

#ifndef USB_DEVICE_VID
#define USB_DEVICE_VID 0x4348
#endif
#ifndef USB_DEVICE_PID
#define USB_DEVICE_PID 0x55e0
#endif
#ifndef USB_DEVICE_BCD
#define USB_DEVICE_BCD 0x0000
#endif
#ifndef USB_DEVICE_MANUFACTURER
#define USB_DEVICE_MANUFACTURER "WCH"
#endif
#ifndef USB_DEVICE_PRODUCT
#define USB_DEVICE_PRODUCT "CH559 USB MIDI"
#endif

static struct usb_device usb_device;

static const uint8_t kString00Language[] = {4, 3, 0x09, 0x04};
static const char kString01Manufacturer[] = USB_DEVICE_MANUFACTURER;
static const char kString02Product[] = USB_DEVICE_PRODUCT;
static const char *kString03SerialNumber;
static char descriptor_buffer[64];

static struct midi_device* midi_device = 0;

static const struct usb_desc_device desc_device = {
  sizeof(struct usb_desc_device),  // bLength
  USB_DESC_DEVICE,                 // bDescriptorType (DEVICE)
  0x0110,                          // bcdUSB (1.10)
  0x00,                            // bDeviceClass (Device defined at Interface level)
  0x00,                            // bDeviceSubClass
  0x00,                            // bDeviceProtocol
  64,                              // bMaxPacketSize0
  USB_DEVICE_VID,                  // idVendor
  USB_DEVICE_PID,                  // idProduct
  USB_DEVICE_BCD,                  // bcdDevice
  1,                               // iManufacturer
  2,                               // iProduct
  3,                               // iSerialNumber
  1,                               // bNumConfigurations
};

static const struct {
  struct usb_desc_configuration     desc_conf;
  struct usb_desc_interface         desc_if_ac;
  struct usb_desc_cs_ac_interface   desc_cs_if_ac;
  struct usb_desc_interface         desc_if_ms;
  struct usb_desc_cs_ms_interface   desc_cs_if_ms;
  struct usb_desc_midi_in_jack      desc_min_jack_embd;
  struct usb_desc_midi_in_jack      desc_min_jack_ext;
  struct usb_desc_midi_out_jack     desc_mout_jack_embd;
  struct usb_desc_midi_out_jack     desc_mout_jack_ext;
  struct usb_desc_endpoint          desc_ep1_out;
  struct usb_desc_cs_midi_endpoint  desc_cs_ep1_out;
  struct usb_desc_endpoint          desc_ep1_in;
  struct usb_desc_cs_midi_endpoint  desc_cs_ep1_in;
} desc_configuration = {
  { /* Configuration Descriptor */
    sizeof(struct usb_desc_configuration),    // bLength
    USB_DESC_CONFIGURATION,                   // bDescriptorType (CONFIGURATION)
    sizeof(desc_configuration),               // wTotalLength
    2,                                        // bNumInterfaces
    1,                                        // bConfigurationValue
    0,                                        // iConfiguration
    0x80,                                     // bmAttributes (Bus-powered, no remote-wakeup)
    50,                                       // bMaxPower (100mA)
  },
  { /* Interface Descriptor - Audio Control */
    sizeof(struct usb_desc_interface),        // bLength
    USB_DESC_INTERFACE,                       // bDescriptorType (INTERFACE)
    0,                                        // bInterfaceNumber
    0,                                        // bAlternateSetting
    0,                                        // bNumEndpoints
    0x01,                                     // bInterfaceClass (AUDIO)
    0x01,                                     // bInterfaceSubClass (AUDIO_CONTROL)
    0x00,                                     // bInterfaceProtocol
    0,                                        // iInterface
  },
  { /* Class-specific Audio Control Interface Descriptor - Header */
    sizeof(struct usb_desc_cs_ac_interface),  // bLength
    USB_DESC_CS_INTERFACE,                    // bDescriptorType (CS_INTERFACE)
    0x01,                                     // bDescriptorSubtype (HEADER)
    0x0100,                                   // bcdADC (1.0)
    sizeof(struct usb_desc_cs_ac_interface),  // wTotalLength
    1,                                        // bInCollection
    1,                                        // baInterfaceNr1
  },
  { /* Interface Descriptor - MIDI Streaming */
    sizeof(struct usb_desc_interface),        // bLength
    USB_DESC_INTERFACE,                       // bDescriptorType (INTERFACE)
    1,                                        // bInterfaceNumber
    0,                                        // bAlternateSetting
    2,                                        // bNumEndpoints
    0x01,                                     // bInterfaceClass (AUDIO)
    0x03,                                     // bInterfaceSubClass (MIDISTREAMING)
    0x00,                                     // bInterfaceProtocol
    0,                                        // iInterface
  },
  { /* Class-specific MIDI Streaming Interface Descriptor - Header */
    sizeof(struct usb_desc_cs_ms_interface),  // bLength
    USB_DESC_CS_INTERFACE,                    // bDescriptorType (CS_INTERFACE)
    0x01,                                     // bDescriptorSubtype (MS_HEADER)
    0x0100,                                   // bcdADC (1.0)
    sizeof(struct usb_desc_cs_ms_interface)  +
    sizeof(struct usb_desc_midi_in_jack)     +
    sizeof(struct usb_desc_midi_in_jack)     +
    sizeof(struct usb_desc_midi_out_jack)    +
    sizeof(struct usb_desc_midi_out_jack)    +
    sizeof(struct usb_desc_endpoint)         +
    sizeof(struct usb_desc_cs_midi_endpoint) +
    sizeof(struct usb_desc_endpoint)         +
    sizeof(struct usb_desc_cs_midi_endpoint), // wTotalLength
  },
  { /* Class-specific MIDI Streaming Interface Descriptor - MIDI IN Jack, Embedded, ID 1 */
    sizeof(struct usb_desc_midi_in_jack),     // bLength
    USB_DESC_CS_INTERFACE,                    // bDescriptorType (CS_INTERFACE)
    0x02,                                     // bDescriptorSubtype (MIDI_IN_JACK)
    0x01,                                     // bJackType (EMBEDDED)
    0x01,                                     // bJackID (1)
    0x00,                                     // iJack
  },
  { /* Class-specific MIDI Streaming Interface Descriptor - MIDI IN Jack, External, ID 2 */
    sizeof(struct usb_desc_midi_in_jack),     // bLength
    USB_DESC_CS_INTERFACE,                    // bDescriptorType (CS_INTERFACE)
    0x02,                                     // bDescriptorSubtype (MIDI_IN_JACK)
    0x02,                                     // bJackType (EXTERNAL)
    0x02,                                     // bJackID (2)
    0x00,                                     // iJack
  },
  { /* Class-specific MIDI Streaming Interface Descriptor - MIDI OUT Jack, Embedded, ID 3 */
    sizeof(struct usb_desc_midi_out_jack),    // bLength
    USB_DESC_CS_INTERFACE,                    // bDescriptorType (CS_INTERFACE)
    0x03,                                     // bDescriptorSubtype (MIDI_OUT_JACK)
    0x01,                                     // bJackType (EMBEDDED)
    0x03,                                     // bJackID (3)
    0x01,                                     // bNrInputPins
    0x02,                                     // baSourceID1
    0x01,                                     // baSourcePin1
    0x00,                                     // iJack
  },
  { /* Class-specific MIDI Streaming Interface Descriptor - MIDI OUT Jack, External, ID 4 */
    sizeof(struct usb_desc_midi_out_jack),    // bLength
    USB_DESC_CS_INTERFACE,                    // bDescriptorType (CS_INTERFACE)
    0x03,                                     // bDescriptorSubtype (MIDI_OUT_JACK)
    0x02,                                     // bJackType (EXTERNAL)
    0x04,                                     // bJackID (4)
    0x01,                                     // bNrInputPins
    0x01,                                     // baSourceID1
    0x01,                                     // baSourcePin1
    0x00,                                     // iJack
  },
  { /* Endpoint Descriptor - Endpoint 1 Bulk OUT */
    sizeof(struct usb_desc_endpoint),         // bLength
    USB_DESC_ENDPOINT,                        // bDescriptorType (ENDPOINT)
    0x01,                                     // bEndpointAddress (OUT, 1)
    0x02,                                     // bmAttributes (Bulk, not shared)
    0x0040,                                   // wMaxPacketSize (64 bytes / packet)
    0x00,                                     // bInterval
    0x00,                                     // bRefresh
    0x00,                                     // bSynchAddress
  },
  { /* Class-specific MIDI Stream Endpoint Descriptor - JackID 1 */
    sizeof(struct usb_desc_cs_midi_endpoint), // bLength
    USB_DESC_CS_ENDPOINT,                     // bDescriptorType (CS_ENDPOINT)
    0x01,                                     // bDescriptorSubtype (MS_GENERAL)
    1,                                        // bNumEmbMIDIJack
    1,                                        // baAssocJackID0 (1)
  },
  { /* Endpoint Descriptor - Endpoint 1 Bulk IN */
    sizeof(struct usb_desc_endpoint),         // bLength
    USB_DESC_ENDPOINT,                        // bDescriptorType (ENDPOINT)
    0x81,                                     // bEndpointAddress (IN, 1)
    0x02,                                     // bmAttributes (Bulk, not shared)
    0x0040,                                   // wMaxPacketSize (64 bytes / packet)
    0x00,                                     // bInterval
    0x00,                                     // bRefresh
    0x00,                                     // bSynchAddress
  },
  { /* Class-specific MIDI Stream Endpoint Descriptor - JackID 3 */
    sizeof(struct usb_desc_cs_midi_endpoint), // bLength
    USB_DESC_CS_ENDPOINT,                     // bDescriptorType (CS_ENDPOINT)
    0x01,                                     // bDescriptorSubtype (MS_GENERAL)
    1,                                        // bNumEmbMIDIJack
    3,                                        // baAssocJackID0 (3)
  },
};

static uint8_t get_kString_length(uint8_t no) {
  switch (no) {
    case 1:
      return sizeof(kString01Manufacturer) - 1;
    case 2:
      return sizeof(kString02Product) - 1;
    case 3:
      for (uint8_t i = 0; i < 32; i++) {
        if (kString03SerialNumber[i] == '\0')
          return i;
      }
  }
  return 0;
}

static const char* get_kString(uint8_t no) {
  switch (no) {
    case 1:
      return kString01Manufacturer;
    case 2:
      return kString02Product;
    case 3:
      return kString03SerialNumber;
  }
  return 0;
}

static uint8_t get_string_descriptor_size(uint8_t no) {
  switch (no) {
    case 0:
      return sizeof(kString00Language);
    case 1:
    case 2:
    case 3:
      return get_kString_length(no) * 2 + 2;
  }
  return 0;
}

static const uint8_t* get_string_descriptor(uint8_t no) {
  uint8_t length = get_kString_length(no);
  const char* string = get_kString(no);
  switch (no) {
    case 0:
      return kString00Language;
    case 1:
    case 2:
    case 3:
      descriptor_buffer[0] = length * 2 + 2;
      descriptor_buffer[1] = USB_DESC_STRING;
      for (uint8_t i = 0; i < length; ++i) {
        descriptor_buffer[2 + i * 2] = string[i];
        descriptor_buffer[2 + i * 2 + 1] = 0;
      }
      return descriptor_buffer;
  }
  return 0;
}

static uint8_t get_descriptor_size(uint8_t type, uint8_t no) {
  no;
  switch (type) {
    case USB_DESC_DEVICE:
      return sizeof(desc_device);
    case USB_DESC_CONFIGURATION:
      return sizeof(desc_configuration);
    case USB_DESC_STRING:
      return get_string_descriptor_size(no);
  }
  return 0;
}

static const uint8_t* get_descriptor(uint8_t type, uint8_t no) {
  no;
  switch (type) {
    case USB_DESC_DEVICE:
      return (const uint8_t*)&desc_device;
    case USB_DESC_CONFIGURATION:
      return (const uint8_t*)&desc_configuration;
    case USB_DESC_STRING:
      return get_string_descriptor(no);
  }
  return 0;
}

static bool ep_out(const uint8_t* buffer, uint8_t len) {
  usb_ep1out_set_hs(false);
  if ((len == 0) || (len % 4 != 0)) {  // Invalid packet
    return false;
  }
  if (midi_device->recv) {
    midi_device->recv(buffer, len);
  }
  return true;
}

void midi_device_allow_recv(bool allow) {
  usb_ep1out_set_hs(allow);
}

bool midi_device_send(const uint8_t* data, uint8_t len) {
  if (usb_ep1_is_send_ready()) {
    usb_ep1_send(data, len);
    return true;
  }
  return false;
}

void midi_device_init(struct midi_device* device) {
  midi_device = device;
  usb_device.get_descriptor_size = get_descriptor_size;
  usb_device.get_descriptor = get_descriptor;
  usb_device.setup = 0;
  usb_device.ep_out = ep_out;
  usb_device.connected = device->connected;
  usb_device.suspend = device->suspend;
  usb_device.bus_reset = device->bus_reset;
  kString03SerialNumber = midi_device->serial_number;
  usb_device_init(&usb_device);
}

void midi_device_deinit(void) {
  usb_device_deinit();
}

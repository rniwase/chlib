// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midi_host.h"
#include "usb_midi.h"

#include "../ch559.h"
#include "../serial.h"

#include <string.h>

static struct midi_host* midi_host;
static struct usb_host host;

static void disconnected(uint8_t hub) {
  if (hub != 0) {
    return;
  }
  midi_host->detected_ep_in = -1;
  midi_host->detected_ep_out = -1;
  if (midi_host->disconnected) {
    midi_host->disconnected();
  }
}

static void check_device_desc(uint8_t hub, const uint8_t* data) {
  if (hub != 0) {
    return;
  }
  const struct usb_desc_device* desc = (const struct usb_desc_device*)data;

  Serial.printf("Device vendor: %x%x\n", desc->idVendor >> 8,
                desc->idVendor & 0xff);
  Serial.printf("Device product: %x%x\n", desc->idProduct >> 8,
                desc->idProduct & 0xff);
  Serial.printf("Device class: %x\n", desc->bDeviceClass);
  Serial.printf("Device subclass: %x\n", desc->bDeviceSubClass);
  Serial.printf("Device protocol: %x\n", desc->bDeviceProtocol);
}

static uint8_t check_configuration_desc(uint8_t hub, const uint8_t* data) {
  if (hub != 0) {
    return 0;
  }
  const struct usb_desc_configuration* desc = (const struct usb_desc_configuration*)data;
  struct usb_desc_head* head = (struct usb_desc_head*)data;

  if (desc->wTotalLength == 0) {
    Serial.printf("Invalid descriptor, Total Length is 0\n");
    return 1;
  }

  for (uint8_t i = head->bLength; i < desc->wTotalLength; i += head->bLength) {
    head = (struct usb_desc_head*)(data + i);

    Serial.printf("Descriptor Type: %x\n", head->bDescriptorType);

    if (head->bLength == 0) {
      Serial.printf("Invalid descriptor, Data length is 0\n");
      return 1;
    }

    switch (head->bDescriptorType) {
      case USB_DESC_INTERFACE: {
        const struct usb_desc_interface* intf = (const struct usb_desc_interface*)(data + i);
        Serial.println("- Interface");
        Serial.printf("  bInterfaceClass: %x\n", intf->bInterfaceClass);
        Serial.printf("  bInterfaceSubClass: %x\n", intf->bInterfaceSubClass);
        Serial.printf("  bInterfaceProtocol: %x\n", intf->bInterfaceProtocol);
        break;
      }
      case USB_DESC_CS_INTERFACE: {
        const struct usb_desc_cs_interface* cs_intf = (const struct usb_desc_cs_interface*)(data + i);
        Serial.println("- Class-specific Interface");
        Serial.printf("  bDescriptorSubtype: %x\n", cs_intf->bDescriptorSubtype);
        switch (cs_intf->bDescriptorSubtype) {
          case 0x01: {
            Serial.println("  - MIDI Stream Header");
            break;
          }
          case 0x02: {
            const struct usb_desc_cs_midi_in_jack* cs_intf_midi_in = (const struct usb_desc_cs_midi_in_jack*)(cs_intf->content);
            Serial.println("  - MIDI IN Jack");
            Serial.printf("    bJackType: %x\n", cs_intf_midi_in->bJackType);
            Serial.printf("    bJackID: %x\n", cs_intf_midi_in->bJackID);
            break;
          }
          case 0x03: {
            const struct usb_desc_cs_midi_out_jack* cs_intf_midi_out = (const struct usb_desc_cs_midi_out_jack*)(cs_intf->content);
            Serial.println("  - MIDI OUT Jack");
            Serial.printf("    bJackType: %x\n", cs_intf_midi_out->bJackType);
            Serial.printf("    bJackID: %x\n", cs_intf_midi_out->bJackID);
            break;
          }
        }
        break;
      }
      case USB_DESC_ENDPOINT: {
        const struct usb_desc_endpoint* ep = (const struct usb_desc_endpoint*)(data + i);
        Serial.println("- Endpoint");
        Serial.printf("  bEndpointAddress: %x\n", ep->bEndpointAddress);
        Serial.printf("  bmAttributes: %x\n", ep->bmAttributes);

        if (ep->bmAttributes == 0x02) {  // Bulk, not shared
          switch (ep->bEndpointAddress & 0xf0) {
            case 0x00: {
              if (midi_host->detected_ep_out == -1) {
                midi_host->detected_ep_out = (int8_t)(ep->bEndpointAddress & 0x0f);
              }
              break;
            }
            case 0x80: {
              if (midi_host->detected_ep_in == -1) {
                midi_host->detected_ep_in = (int8_t)(ep->bEndpointAddress & 0x0f);
              }
              break;
            }
          }
        }

        break;
      }
      case USB_DESC_CS_ENDPOINT: {
        const struct usb_desc_cs_endpoint* cs_ep = (const struct usb_desc_cs_endpoint*)(data + i);
        Serial.println("- Class-specific Endpoint");
        Serial.printf("  bDescriptorSubtype: %x\n", cs_ep->bDescriptorSubtype);
        switch (cs_ep->bDescriptorSubtype) {
          case 0x01: {
            const struct usb_desc_cs_endpoint_midi* cs_ep_midi = (const struct usb_desc_cs_endpoint_midi*)(cs_ep->content);
            Serial.println("  - MIDI Stream");
            Serial.printf("    bNumEmbMIDIJack: %x\n", cs_ep_midi->bNumEmbMIDIJack);
            Serial.printf("    baAssocJackID1: %x\n", cs_ep_midi->baAssocJackID1);
          }
        }
        break;
      }
      default: {
        Serial.println("");
      }
    }
  }

  if (midi_host->detected) {
    midi_host->detected();
  }

  return 0;
}

static void in(uint8_t hub, uint8_t ep, uint8_t* data, uint16_t size) {
  if (hub != 0) {
    return;
  }
  if (size == 0) {
    return;
  }
  if (midi_host->recv && ((int8_t)ep == midi_host->detected_ep_in)) {
    midi_host->recv(data, size);
  }
}

void midi_host_init(struct midi_host* new_midi_host) {
  midi_host = new_midi_host;
  midi_host->detected_ep_in = -1;
  midi_host->detected_ep_out = -1;
  host.flags = USE_HUB0;  // HUB1 is not used
  host.disconnected = disconnected;
  host.check_device_desc = check_device_desc;
  host.check_string_desc = 0;
  host.check_configuration_desc = check_configuration_desc;
  host.check_hid_report_desc = 0;
  host.in = in;
  host.hid_report = 0;
  usb_host_init(&host);
}

void midi_host_deinit(void) {
  midi_host->detected_ep_in = -1;
  midi_host->detected_ep_out = -1;
  usb_host_deinit();
}

void midi_host_poll(void) {
  usb_host_poll();
}

bool midi_host_ready(void) {
  return usb_host_ready(0);
}

bool midi_host_recv(void) {
  if (midi_host->detected_ep_in != -1) {
    return usb_host_in(0, (uint8_t)midi_host->detected_ep_in, 4);  // Fix size
  } else {
    return false;
  }
}

bool midi_host_send(uint8_t* data, uint8_t size) {
  if (midi_host->detected_ep_out != -1) {
    return usb_host_out(0, (uint8_t)midi_host->detected_ep_out, data, size);
  } else {
    return false;
  }
}

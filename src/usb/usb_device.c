// Copyright 2021 Takashi Toyoshima <toyoshim@gmail.com>. All rights reserved.
// Copyright 2026 Ryohei Niwase <ryohei@niwase.net>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_device.h"
#include <stdint.h>

#include "../ch559.h"
#include "../io.h"
#include "../serial.h"

#define NOTREACHED(e) halt(e)

static const uint8_t SIZE_EP0 = 64;
static uint8_t* ep0_buffer = &RSVD_USB_BUF[0];  // 0x1740
static uint8_t* ep1_buffer = &RSVD_USB_BUF[64];  // 0x1780
static uint8_t* ep1_out_buffer = &RSVD_USB_BUF[64];  // 0x1780
static uint8_t* ep1_in_buffer = &RSVD_USB_BUF[128];  // 0x17c0

static struct usb_device* usb_device = 0;
static uint8_t state = UD_STATE_IDLE;

static struct usb_setup_req last_setup_req;
static const uint8_t* sending_data_ptr = 0;
static uint8_t sending_data_len = 0;

static void halt(const char* message) {
#if 0
  message;
#else
  Serial.printf("HALT: %s\n", message);
  Serial.print("type: ");
  Serial.printc(last_setup_req.bRequestType, HEX);
  Serial.println("");
  Serial.print("req: ");
  Serial.printc(last_setup_req.bRequest, HEX);
  Serial.println("");
  Serial.print("value: ");
  Serial.printc(last_setup_req.wValue >> 8, HEX);
  Serial.printc(last_setup_req.wValue & 0xff, HEX);
  Serial.println("");
  Serial.print("index: ");
  Serial.printc(last_setup_req.wIndex >> 8, HEX);
  Serial.printc(last_setup_req.wIndex & 0xff, HEX);
  Serial.println("");
  Serial.print("length: ");
  Serial.printc(last_setup_req.wLength >> 8, HEX);
  Serial.printc(last_setup_req.wLength & 0xff, HEX);
  Serial.println("");
  for (;;)
    ;
#endif
}

static void bus_reset(void) {
  // ACK for SETUP and OUT, NAK for IN.
  UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
  UEP1_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
  USB_DEV_AD = 0x00;  // clear USB device addrss and user defined flag bit
}

static void stall(void) {
  UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
}

static void ep0_send(uint8_t len, const uint8_t* data) {
  uint8_t transfer_len = (len <= SIZE_EP0) ? len : SIZE_EP0;
  if (data) {
    for (uint8_t i = 0; i < transfer_len; ++i) {
      ep0_buffer[i] = data[i];
    }
  }
  UEP0_T_LEN = transfer_len;
  UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
  sending_data_ptr = &data[transfer_len];
  sending_data_len = len - transfer_len;
}

static void ep0_cont(void) {
  uint8_t transfer_len =
      (sending_data_len <= SIZE_EP0) ? sending_data_len : SIZE_EP0;
  for (uint8_t i = 0; i < transfer_len; ++i) {
    ep0_buffer[i] = sending_data_ptr[i];
  }
  UEP0_T_LEN = transfer_len;
  UEP0_CTRL ^= (bUEP_R_TOG | bUEP_T_TOG);
  sending_data_ptr += transfer_len;
  sending_data_len -= transfer_len;
}

static void get_descriptor(void) {
  uint8_t type = last_setup_req.wValue >> 8;
  uint8_t no = last_setup_req.wValue & 0xff;
  uint8_t size = usb_device->get_descriptor_size(type, no);
  if (0 == size) {
    // NOTREACHED("unknown descriptor");
    ep0_send(0, 0);
    return;
  }
  if (size > last_setup_req.wLength) {
    size = last_setup_req.wLength;
  }
  ep0_send(size, usb_device->get_descriptor(type, no));
}

static void setup(void) {
  if (USB_RX_LEN != sizeof(struct usb_setup_req)) {
    NOTREACHED("unexpected request size");
    return;
  }
  struct usb_setup_req* req = (struct usb_setup_req*)ep0_buffer;
  // last_setup_req = *req;
  last_setup_req.bRequestType = req->bRequestType;
  last_setup_req.bRequest = req->bRequest;
  last_setup_req.wValue = req->wValue;
  last_setup_req.wIndex = req->wIndex;
  last_setup_req.wLength = req->wLength;

  if ((req->bRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) {
    switch (req->bRequest) {
      case USB_GET_STATUS:
        ep0_send(2, "\0\0");
        return;
      case USB_CLEAR_FEATURE:
        ep0_send(0, 0);
        return;
      case USB_SET_FEATURE:
        ep0_send(0, 0);
        return;
      case USB_SET_ADDRESS:
        ep0_send(0, 0);
        return;
      case USB_GET_DESCRIPTOR:
        get_descriptor();
        return;
      case USB_GET_CONFIGURATION:
        ep0_send(1, "\0");
        return;
      case USB_SET_CONFIGURATION:
        ep0_send(0, 0);
        state = UD_STATE_READY;
        UEP1_CTRL &= ~bUEP_T_TOG;  // Reset EP1 IN toggle
        if (usb_device->connected) {
          usb_device->connected();
        }
        return;
      default:
        break;
    }
  } else {
    uint8_t len = 0;
    if (usb_device->setup && usb_device->setup(req, ep0_buffer, &len)) {
      ep0_send(len, 0);
      return;
    }
  }
  NOTREACHED("setup");
}

void in(void) {
  if ((last_setup_req.bRequestType & USB_REQ_TYPE_MASK) ==
      USB_REQ_TYPE_STANDARD) {
    if (last_setup_req.bRequest == USB_SET_ADDRESS) {
      USB_DEV_AD = last_setup_req.wValue;
      UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
      return;
    }
  }
  if ((last_setup_req.bRequestType & USB_REQ_DIR_MASK) == USB_REQ_DIR_IN) {
    ep0_cont();
  } else {
    // Status Out
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
  }
}

void out(void) {
  if ((last_setup_req.bRequestType & USB_REQ_DIR_MASK) == USB_REQ_DIR_IN) {
    // Status Out
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    return;
  }
  if ((last_setup_req.bRequestType & USB_REQ_TYPE_MASK) ==
      USB_REQ_TYPE_STANDARD) {
    NOTREACHED("out0 standard");
  } else {
    // bool result = false;
    // if (usb_device->ep_out) {
    //   result = usb_device->ep_out(ep0_buffer, USB_RX_LEN);
    // }
    // if (!result) {
    //   NOTREACHED("out0");
    // }
    UEP0_CTRL ^= bUEP_R_TOG;
  }
}

void usb_transfer(uint8_t int_status, uint8_t rx_len) {
  uint8_t token_ep = int_status & (MASK_UIS_TOKEN | MASK_UIS_ENDP);

  // if (U_IS_NAK) {
  //   // Serial.printf("nak: ep %d token %d\n", ep, token);
  //   return;
  // }

  switch (token_ep) {
    // Endpoint 0
    case UIS_TOKEN_OUT | 0:
      out();
      return;
    case UIS_TOKEN_IN | 0:
      in();
      return;
    case UIS_TOKEN_SETUP | 0:
      setup();
      return;

    // Endpoint 1
    case UIS_TOKEN_OUT | 1:
      UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_R_RES | UEP_R_RES_NAK;  // Pause OUT transaction
      usb_device->ep_out(ep1_out_buffer, rx_len);
      return;
    case UIS_TOKEN_IN | 1:
      UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;  // Pause upload
      return;

    default:
      return;
  }
}

void usb_int(void) __interrupt(INT_NO_USB) __using(1) {
  if (UIF_TRANSFER) {
    usb_transfer(USB_INT_ST, USB_RX_LEN);
    UIF_TRANSFER = 0;
  } else if (UIF_SUSPEND) {
    UIF_TRANSFER = 0;
    UIF_SUSPEND = 0;
    if (usb_device->suspend) {
      usb_device->suspend();
    }
  } else if (UIF_BUS_RST) {
    bus_reset();
    UIF_TRANSFER = 0;
    UIF_BUS_RST = 0;
    if (usb_device->bus_reset) {
      usb_device->bus_reset();
    }
  }
}

void usb_device_init(struct usb_device* device) {
  usb_device = device;

  IE_USB = 0;  // Disable USB interrupts
  USB_CTRL = 0x00;  // USB Device mode, Full speed, Disable internal pull-up resistor, Clear USB processor reset bit

  UEP4_1_MOD = bUEP1_TX_EN | bUEP1_RX_EN;  // Enable EP1 IN/OUT, Disable EP4 IN/OUT
  UEP2_3_MOD = 0x00;  // Disable EP2,3 IN/OUT

  // Setup DMA start addresses
  UEP0_DMA_H = (uint16_t)ep0_buffer >> 8;
  UEP0_DMA_L = (uint16_t)ep0_buffer & 0xff;
  UEP1_DMA_H = (uint16_t)ep1_buffer >> 8;
  UEP1_DMA_L = (uint16_t)ep1_buffer & 0xff;

  bus_reset();
  UDEV_CTRL = bUD_DP_PD_DIS | bUD_DM_PD_DIS;  // Disable DM/DP internal pull-down resistor, Enable receiver
  USB_CTRL = bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;  // Enable internal pull-up resistor, Enable auto-response busy NAK, Enable DMA
  UDEV_CTRL |= bUD_PORT_EN;  // Enable USB port
  USB_INT_FG = 0xff;         // Clear interrupt flags
  USB_INT_EN = bUIE_TRANSFER | bUIE_BUS_RST | bUIE_SUSPEND;  // Enable transfer completion / bus reset / suspend interrupt
  // USB_INT_EN |= bUIE_DEV_SOF;  // Enable SOF receive interrupt
  // USB_INT_EN |= bUIE_DEV_NAK;  // Enable NAK receive interrupt
  IE_USB = 1;  // Enable USB interrupts
  // EA = 1;      // Enable interrupts
}

void usb_device_deinit(void) {
  IE_USB = 0;  // Disable USB interrupts
  USB_CTRL = 0x00;  // Clear USB processor reset bit
  UEP4_1_MOD = 0x00;
  UEP2_3_MOD = 0x00;
  USB_INT_EN = 0x00;
  UDEV_CTRL = bUD_RECV_DIS;  // Disable USB device receiver
}

uint8_t usb_device_state(void) {
  return state;
}

bool usb_ep1_is_send_ready(void) {
  return (UEP1_CTRL & MASK_UEP_T_RES) != UEP_T_RES_ACK;
}

void usb_ep1_send(const uint8_t* data, uint8_t len) {
  if (data) {
    for (uint8_t i = 0; i < len; ++i) {
      ep1_in_buffer[i] = data[i];
    }
  }
  UEP1_T_LEN = len;
  UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
}

void usb_ep1out_set_hs(bool ack) {
  if (ack) {
    UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_R_RES | UEP_R_RES_ACK;  // Allow OUT transaction
  } else {
    UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_R_RES | UEP_R_RES_NAK;  // Pause OUT transaction
  }
}

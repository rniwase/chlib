# chlib
CH559 Peripheral Library

This branch primarily provides resources for implementing USB-MIDI device or host.

## Build examples

1. Install [Nix](https://nixos.org/download/) on the system

2. Clone this repository

   ```bash
   git clone https://github.com/rniwase/chlib.git
   cd chlib/
   ```

3. Enter Nix shell

   ```bash
   nix develop --extra-experimental-features "nix-command flakes"
   ```

4. Build examples

   ```bash
   make
   ```

## Flash

1. Connect CH559's BOOT pin to GND, then connect the USB to the system

2. Run the following command on Nix shell to flash the firmware

   ```bash
   # For USB MIDI Device example
   make -C usb_midi_device flash
   # For USB MIDI Host example
   make -C usb_midi_host flash
   ```

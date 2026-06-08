TARGET := usb_midi_device
WORKDIR := workdir

CFLAGS := -V -mmcs51 --model-large \
  --xram-size 0x1740 --xram-loc 0x0000 --code-size 0xec00 \
  --stack-auto --Werror --opt-code-speed

CC := sdcc
SDOBJCOPY := sdobjcopy
WCHISP := wchisp

OBJS := \
  $(WORKDIR)/ch559.rel \
  $(WORKDIR)/serial.rel \
  $(WORKDIR)/uart1.rel \
  $(WORKDIR)/fifo.rel \
  $(WORKDIR)/timer3.rel \
  $(WORKDIR)/interrupt.rel \
  $(WORKDIR)/usb_device.rel \
  $(WORKDIR)/midi_device.rel \
  $(WORKDIR)/midi_util.rel

.PHONY: build clean flash

build: $(TARGET).bin

$(WORKDIR):
	mkdir -p $(WORKDIR)

$(WORKDIR)/%.rel: ../src/%.c ../src/*.h ../src/usb/*.h | $(WORKDIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(WORKDIR)/%.rel: ../src/usb/%.c ../src/*.h ../src/usb/*.h | $(WORKDIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(WORKDIR)/$(TARGET).rel: $(TARGET).c ../src/*.h ../src/usb/*.h | $(WORKDIR)
	$(CC) -c $(CFLAGS) -I../src -o $@ $<

$(WORKDIR)/$(TARGET).ihx: $(OBJS) $(WORKDIR)/$(TARGET).rel
	$(CC) $(CFLAGS) $^ -o $@

%.bin: $(WORKDIR)/%.ihx
	$(SDOBJCOPY) -I ihex -O binary $< $@

clean:
	rm -rf $(WORKDIR) $(TARGET).bin

flash: $(TARGET).bin
	$(WCHISP) flash $<

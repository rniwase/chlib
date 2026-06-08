SUBDIRS := usb_midi_device usb_midi_host

.PHONY: build clean

build clean:
	@ for d in $(SUBDIRS); do \
		$(MAKE) -C $$d $@; \
	done

# Google Calendar for the Bally Astrocade. Standalone, outside mekkogx, in
# the netcat/battleship mold: the build is build.sh (zmac + the layout
# checks), this file is the convenience wrapper.

SRCS := gcal.asm HVGLIB.H $(wildcard *.inc) assets/font.inc

all: build/gcal.bin

build/gcal.bin: $(SRCS) build.sh tools/checkrom.py tools/checksize.py
	./build.sh

run: build/gcal.bin
	./run.sh

# The static mock screens: layout and palette tuning by screenshot, no
# FujiNet needed. Any key steps splash -> day -> month -> form.
demo:
	DEMO=1 ./build.sh
	./run.sh

MAME_DIR ?= $(HOME)/Workspace/mame

# Headless screenshot pass over the DEMO screens.
demoshot:
	DEMO=1 ./build.sh
	rm -rf build/astrocde
	$(MAME_DIR)/mame astrocde \
	    -rompath $(MAME_DIR)/roms \
	    -cartslot fujinet -cart $(CURDIR)/build/gcal.bin \
	    -autoboot_script emu/demoshot.lua -video none -sound none \
	    -seconds_to_run 30 -snapshot_directory $(CURDIR)/build
	@ls build/astrocde/

# Headless end-to-end smoke: launch, land on the live day view, walk the
# views. Needs a running fujinet-pc (BoIP on 127.0.0.1:9995) with the GCAL:
# adapter and Google authorised.
smoke: build/gcal.bin
	rm -rf build/astrocde
	FUJINET_DEBUG=$${FUJINET_DEBUG:-1} $(MAME_DIR)/mame astrocde \
	    -rompath $(MAME_DIR)/roms \
	    -cartslot fujinet -cart $(CURDIR)/build/gcal.bin \
	    -autoboot_script emu/smoke.lua -video none -sound none \
	    -seconds_to_run 60 -snapshot_directory $(CURDIR)/build
	@ls build/astrocde/

test:
	python3 tools/test_categories.py

clean:
	rm -rf build

.PHONY: all run demo demoshot smoke test clean

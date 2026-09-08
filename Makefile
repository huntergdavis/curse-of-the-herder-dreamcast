# Curse of the Herder (Dreamcast). Build inside the KallistiOS Docker image: ./scripts/dc-build.sh
TARGET   = herder.elf
OBJS     = src/main.o src/core/rng.o src/core/noise.o src/core/map/terrain.o src/core/map/path.o src/core/map/generate.o src/core/progression.o src/core/names.o src/core/sim/flock.o src/core/sim/book.o src/core/sim/step.o src/core/lang/morphology.o src/core/lang/banned.o src/core/lang/grammar.o src/core/lang/speech.o src/render/fb.o src/data/lang_data.o
KOS_CFLAGS += -std=gnu11 -Wall -Wextra -O2 -Isrc
KOS_LOCAL_LDFLAGS = -lm

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS) build/*.bin build/*.cdi build/1ST_READ.BIN

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

# Disc image: scramble the binary, stamp an IP.BIN, and wrap it as a self-booting CDI.
cdi: $(TARGET)
	mkdir -p build build/disc
	sh-elf-objcopy -R .stack -O binary $(TARGET) build/herder.bin
	scramble build/herder.bin build/disc/1ST_READ.BIN
	cd disc && makeip ip.txt ../build/IP.BIN
	mkisofs -C 0,11702 -V HERDER -G build/IP.BIN -joliet -rock -l -o build/herder.iso build/disc
	cdi4dc build/herder.iso build/herder.cdi
	ls -la build/herder.cdi

# Plain single-session ISO images, which the browser emulator (Flycast WASM) loads; two variants while we learn
# whether its high-level BIOS descrambles 1ST_READ.BIN.
iso: $(TARGET)
	mkdir -p build build/disc build/disc-plain
	sh-elf-objcopy -R .stack -O binary $(TARGET) build/herder.bin
	scramble build/herder.bin build/disc/1ST_READ.BIN
	cp build/herder.bin build/disc-plain/1ST_READ.BIN
	cd disc && makeip ip.txt ../build/IP.BIN
	mkisofs -V HERDER -G build/IP.BIN -joliet -rock -l -o build/herder-scrambled.iso build/disc
	mkisofs -V HERDER -G build/IP.BIN -joliet -rock -l -o build/herder-plain.iso build/disc-plain
	ls -la build/*.iso

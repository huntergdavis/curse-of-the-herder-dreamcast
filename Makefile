# Curse of the Herder (Dreamcast). Build inside the KallistiOS Docker image: ./scripts/dc-build.sh
TARGET   = herder.elf
OBJS     = src/main.o
KOS_CFLAGS += -std=gnu11 -Wall -Wextra -O2 -Isrc

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS) build/*.bin build/*.cdi build/1ST_READ.BIN

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

# Disc image: scramble the binary, stamp an IP.BIN, and wrap it as a self-booting CDI.
cdi: $(TARGET)
	mkdir -p build build/disc
	sh-elf-objcopy -R .stack -O binary $(TARGET) build/herder.bin
	scramble build/herder.bin build/disc/1ST_READ.BIN
	makeip disc/ip.txt build/IP.BIN
	mkisofs -C 0,11702 -V HERDER -G build/IP.BIN -joliet -rock -l -o build/herder.iso build/disc
	cdi4dc build/herder.iso build/herder.cdi
	ls -la build/herder.cdi

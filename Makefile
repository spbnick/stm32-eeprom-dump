CCPFX=arm-none-eabi-

TARGET_CFLAGS = -mcpu=cortex-m3 -mthumb
COMMON_CFLAGS = $(TARGET_CFLAGS) -Wall -Wextra -Werror
LIBS = -lstammer

LDSCRIPTS = \
    memory.ld   \
    flash.ld

# In order of symbol resolution
MODS = \
    vectors \
    dump

OBJS = $(addsuffix .o, $(MODS))
DEPS = $(OBJS:.o=.d)

.PHONY: clean

all: dump.bin

%.o: %.c
	$(CCPFX)gcc $(COMMON_CFLAGS) $(CFLAGS) -c -o $@ $<
	$(CCPFX)gcc $(COMMON_CFLAGS) $(CFLAGS) -MM $< > $*.d

%.o: %.S
	$(CCPFX)gcc $(COMMON_CFLAGS) $(CFLAGS) -D__ASSEMBLY__ -c -o $@ $<
	$(CCPFX)gcc $(COMMON_CFLAGS) $(CFLAGS) -D__ASSEMBLY__ -MM $< > $*.d

%.bin: %.elf
	$(CCPFX)objcopy -O binary $< $@

dump.elf: $(OBJS) $(LDSCRIPTS)
	$(CCPFX)ld -T flash.ld $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f dump.elf
	rm -f dump.bin

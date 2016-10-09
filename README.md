stm32-eeprom-dump
=================

[![Device top][top_thumb]][top] [![Device bottom][bottom_thumb]][bottom] [![Wiring map][map_thumb]][map]

Stm32-eeprom-dump is a little program for a [board][board] based on
stm32f103c8t6 chip, dumping the contents of an EEPROM chip SST28SF040
connected to the board's I/O pins, through a USART interface.

Building
--------
First you'll need the compiler. If you're using a Debian-based Linux you can
get it by installing `gcc-arm-none-eabi` package. Then you'll need the
[libstammer][libstammer] library. Build it first, then export environment
variables pointing to it:

    export CFLAGS=-ILIBSTAMMER_DIR LDFLAGS=-LLIBSTAMMER_DIR

`LIBSTAMMER_DIR` above is the directory where the built libstammer is located.

After that you can build the program using `make`.

[top]: top.jpg
[top_thumb]: top_thumb.jpg
[bottom]: bottom.jpg
[bottom_thumb]: bottom_thumb.jpg
[board]: http://item.taobao.com/item.htm?spm=a1z10.1-c.w4004-9679183684.4.26lLDG&id=22097803050
[map]: map.jpg
[map_thumb]: map_thumb.jpg
[libstammer]: https://github.com/spbnick/libstammer

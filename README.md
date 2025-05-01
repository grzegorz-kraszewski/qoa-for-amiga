# qoa-for-amiga
Quite OK Audio codec and tools optimized for M68k architecture.

## tools
- **QoaToAiff**, CLI based decoder, decodes QOA files to uncompressed AIFF files.
- **QoaPlay**, CLI based player, for now 8-bit playback through `audio.device`.
  
## build
QOA tools are compiled with GCC 2.95.3-4, VASM and GNU make on Amiga.

## dependencies
- **libminteger**, implements GCC 32-bit integer multiplication and division as calls to `utility.library`.
- **libmfloat**, implements GCC single precision floating point basic operations as calls to `mathieeesingbas.library`.

Both of these libraries can be found in my [amiga-gcc-2.95.3-math](https://github.com/grzegorz-kraszewski/amiga-gcc-2.95.3-math) repo.

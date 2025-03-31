# qoa-for-amiga
Quite OK Audio codec and tools optimized for M68k architecture.
## differencies between mono and stereo decoder
Mono decoder does one pass over the frame.
1. Load LMS state, then decode consecutive slices (offset 0, increment 8), store output samples at offset 0, increment 2.
   
To avoid reloading LMS history and weights at each slice, decoder of stereo frame should do two passes over the frame.
1. Load LMS state of L channel, decode only even (L) slices (offset 0, increment 16), output samples should be stored at offset 0 incremented 4.
2. Load LMS state of R channel, decode only odd (R) slices (offset 8, increment 16), output samples should be stored at offset 2, increment 4.

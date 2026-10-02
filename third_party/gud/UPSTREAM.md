# GUD reference code

Source: https://github.com/notro/gud-pico/tree/984a171447d188a6b5dff5ab3c82061558db5ad0/libraries/gud_pico

`gud.c` and `gud.h` implement GUD protocol version 1. Local changes validate rectangle bounds, buffer sizes, compression lengths, properties and Boolean values, and reset protocol state after a USB reset. `lz4.c` and `lz4.h` are unchanged upstream sources; firmware compiles them with portable memory accesses.

License: MIT for GUD; BSD-2-Clause for LZ4. See `LICENSE.md` and the notices in `lz4.c` and `lz4.h`.

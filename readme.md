# Chrissly Decimal Floating Point Unit (DFPU)
Firmware and Client-API for a DFPU prototype based on the teensy 4.0 board.
## Motivation
For fun an education.
## Usage
### Install Firmware on Device
1. Open "teensy_firmware.ino" in the Arduino IDE
2. Select Board: "Teensy 4.0"
3. Tools -> USB Type -> "Raw HID"
4. Upload
### Client-API
Note that you have to fetch the required submodules before compiling the sample. Run these two commands from the main project:
```console
git submodule init
git submodule update
```
Simple example:
```c
#include <stdio.h>
#define CHRISSLY_DFPU_WINDOWS
#define CHRISSLY_DFPU_IMPLEMENTATION
#include "chrissly_dfpu.h"

int
main(void)
{
    dfpu_init();

    decimal_t vec_a[4U] = {{3, 2, 12345}, {3, 2, 12345}, {3, 2, 12345}, {3, 2, 12345}};
    decimal_t vec_b[4U] = {{1, 3, 6789}, {1, 3, 6789}, {1, 3, 6789}, {1, 3, 6789}};
    decimal_t vec_r[4U] = {0};

    dfpu_add_packed(vec_a, vec_b, vec_r);

    dfpu_term();

    return 0;
}
```
## Status
- teensy firmware done
- Client-API implemented for windows
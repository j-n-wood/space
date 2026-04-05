# Notes

## Build

* CD into the build folder
* run `./premake5 gmake`
* CD back to the root
* run `make`

## General

https://github.com/raylib-extras/raylib-quickstart

https://kitemetric.com/blogs/animating-the-solar-system-with-c-and-raylib-a-comprehensive-guide

https://www.raylib.com/cheatsheet/cheatsheet.html

sudo apt install libsqlite3-dev

## Debug

valgrind --leak-check=full --show-leak-kinds=all ./bin/Debug/space

https://github.com/ThrowTheSwitch/Unity

```shell
make tests
```

Since you are on Ubuntu, you have a significant advantage: Valgrind and AddressSanitizer (ASan) are natively available and are the gold standard for killing "double free" bugs in C.

Here is how to set up your Premake environment on Ubuntu to catch that SQLite error.
1. Enable AddressSanitizer (The "Instant Fix" Scout)

Before even writing a unit test, you should enable ASan. It's built into gcc and will stop your program the exact millisecond a double-free occurs, providing a backtrace of both the first and second free().

Update your premake5.lua for both your app and your test project:
Lua

filter "system:linux"
    buildoptions { "-fsanitize=address", "-g" }
    linkoptions  { "-fsanitize=address" }

5. Using Valgrind for Deep Inspection

If ASan doesn't give you enough info, use Valgrind. On Ubuntu:

    sudo apt update && sudo apt install valgrind

    Run your test binary through it:
    Bash

    valgrind --leak-check=full --show-leak-kinds=all ./bin/Debug/UnitTests

Valgrind will tell you if the "double free" is actually happening inside libsqlite3.so (which usually means you didn't finalize a statement) or in your own code.

## Image processing

### Magenta -> transparent

for img in *.png; do
    magick "$img" -fuzz 10% -transparent "#FF00FF" -alpha set "${img%.png}_out.png"
done

### Locate (rectangular) sprites in a sheet

magick UI_out.png \
  -define connected-components:verbose=true \
  -define connected-components:area-threshold=10 \
  -connected-components 4 \
  null:

// limit A to 0 -> 1.0

magick "$img" -channel A -threshold 90% +channel "${img%.png}_clean.png"

magick UI_out.png -channel A -threshold 90% +channel UI_out_90.png

magick UI_out.png \
  -alpha extract \
  -threshold 90% \
  -define connected-components:verbose=true \
  -define connected-components:area-threshold=100 \
  -connected-components 4 \
  null:

Detects sprites in a sheet

```
  46: 272x168+24+32 159.5,115.5 45696 srgb(255,255,255)
  47: 272x168+304+32 439.5,115.5 45696 srgb(255,255,255)
  48: 272x168+584+32 719.5,115.5 45696 srgb(255,255,255)
  49: 272x168+864+32 989.0,102.3 19712 srgb(255,255,255) 
  57: 272x168+24+208 159.8,291.7 45550 srgb(255,255,255) 
  58: 272x168+304+208 439.5,291.5 45696 srgb(255,255,255)
  59: 272x168+584+208 719.5,291.5 45696 srgb(255,255,255)
  60: 272x168+864+208 999.5,291.5 45696 srgb(255,255,255)
  81: 272x168+864+512 999.5,595.5 45696 srgb(255,255,255)  
  73: 240x120+56+387 175.5,446.5 28800 srgb(255,255,255)
  50: 248x112+1152+32 1275.5,87.5 27776 srgb(255,255,255)
  72: 224x120+911+385 1022.5,444.5 26880 srgb(255,255,255)
  87: 208x128+1152+536 1255.5,599.5 26624 srgb(255,255,255)
  53: 224x116+896+68 1007.5,125.5 25984 srgb(0,0,0)
  55: 208x120+1152+152 1255.5,211.5 24960 srgb(255,255,255)
  63: 208x120+1152+280 1255.5,339.5 24960 srgb(255,255,255)
  75: 208x120+1152+408 1255.5,467.5 24960 srgb(255,255,255)
  56: 200x120+1368+152 1467.5,211.5 24000 srgb(255,255,255)
  64: 200x120+1368+280 1467.5,339.5 24000 srgb(255,255,255)  
  106: 208x80+17+624 120.5,663.5 16640 srgb(255,255,255)
  74: 128x57+304+388 367.5,416.0 7296 srgb(255,255,255)
  265: 176x24+209+744 296.5,755.5 4224 srgb(255,255,255)
  71: 32x120+864+385 879.5,444.5 3840 srgb(255,255,255)
  271: 48x80+121+792 144.5,831.5 3840 srgb(255,255,255)
  89: 96x40+152+560 194.7,582.0 3184 srgb(255,255,255)
  66: 42x75+586+384 606.5,421.0 3150 srgb(255,255,255)
  270: 48x64+65+792 88.5,823.5 3072 srgb(255,255,255)
  67: 42x75+642+384 662.6,421.3 2646 srgb(255,255,255)
  68: 42x75+698+384 718.7,421.6 2214 srgb(255,255,255)
  111: 25x72+145+712 157.0,747.5 1800 srgb(255,255,255)
  78: 42x75+586+472 606.9,510.2 1782 srgb(255,255,255)
  107: 24x72+17+712 28.5,747.5 1728 srgb(255,255,255)
  108: 24x72+49+712 60.5,747.5 1728 srgb(255,255,255)
  109: 24x72+81+712 92.5,747.5 1728 srgb(255,255,255)
  110: 24x72+113+712 124.5,747.5 1728 srgb(255,255,255)
  112: 24x72+177+712 188.5,747.5 1728 srgb(255,255,255)
  266: 40x40+247+785 266.5,804.5 1600 srgb(255,255,255)
  267: 40x40+295+785 314.5,804.5 1600 srgb(255,255,255)
  268: 40x40+343+785 362.5,804.5 1600 srgb(255,255,255)
  269: 40x40+17+792 36.5,811.5 1600 srgb(255,255,255)
  79: 42x75+642+472 663.2,511.0 1350 srgb(255,255,255)
  80: 42x75+698+472 721.2,513.1 846 srgb(255,255,255)
  65: 32x24+544+384 559.5,395.5 768 srgb(255,255,255)
  76: 32x24+544+416 559.5,427.5 768 srgb(255,255,255)
  77: 32x24+544+448 559.5,459.5 768 srgb(255,255,255)
  90: 38x34+106+563 125.4,580.6 749 srgb(255,255,255)
  86: 88x8+56+536 99.5,539.5 704 srgb(255,255,255)
  51: 32x16+1408+32 1423.5,39.5 512 srgb(255,255,255)
  52: 32x16+1408+56 1423.5,63.5 512 srgb(255,255,255)
  54: 32x16+1408+80 1423.5,87.5 512 srgb(255,255,255)
  69: 14x16+752+384 758.5,391.5 224 srgb(255,255,255)
  70: 14x16+776+384 782.5,391.5 224 srgb(255,255,255)
  88: 15x17+56+554 61.9,562.7 169 srgb(255,255,255)
  61: 16x17+45+213 51.8,220.9 146 srgb(0,0,0)

```
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

## Macos

```shell
brew install premake

cd build

premake5 gmake

cd ..

make
```

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

magick ui_buttons_out.png \
  -alpha extract \
  -threshold 90% \
  -define connected-components:verbose=true \
  -define connected-components:area-threshold=100 \
  -connected-components 4 \
  null:

Manual location: 174x23 - 224x125 (51x103) - EC controls (6)
(7) -> general body controls


```
Objects (id: bounding-box centroid area mean-color):
  0: 296x296+0+0 147.7,167.7 46838 srgb(255,255,255)
  74: 101x104+7+111 69.9,163.8 6280 srgb(0,0,0)
  8: 48x64+120+24 142.8,57.6 2665 srgb(0,0,0)
  92: 100x69+181+144 249.5,182.9 1780 srgb(0,0,0)
  44: 48x16+231+75 254.5,82.5 768 srgb(255,255,255)
  81: 48x16+231+126 254.5,133.5 768 srgb(255,255,255)
  7: 51x120+230+23 256.0,82.5 744 srgb(0,0,0)
  6: 51x103+174+23 199.0,74.0 645 srgb(0,0,0)
  93: 32x16+182+145 197.5,152.5 512 srgb(255,255,255)
  94: 32x16+215+145 230.5,152.5 512 srgb(255,255,255)
  95: 32x16+248+145 263.5,152.5 512 srgb(255,255,255)
  117: 32x16+182+162 197.5,169.5 512 srgb(255,255,255)
  118: 32x16+215+162 230.5,169.5 512 srgb(255,255,255)
  120: 32x16+182+179 197.5,186.5 512 srgb(255,255,255)
  121: 32x16+215+179 230.5,186.5 512 srgb(255,255,255)
  122: 32x16+248+179 263.5,186.5 512 srgb(255,255,255)
  124: 32x16+182+196 197.5,203.5 512 srgb(255,255,255)
  125: 32x16+215+196 230.5,203.5 512 srgb(255,255,255)
  33: 101x35+7+63 57.0,80.0 463 srgb(0,0,0)
  2: 24x16+8+23 19.5,30.5 384 srgb(255,255,255)
  3: 24x16+33+23 44.5,30.5 384 srgb(255,255,255)
  4: 24x16+58+23 69.5,30.5 384 srgb(255,255,255)
  5: 24x16+83+23 94.5,30.5 384 srgb(255,255,255)
  9: 24x16+175+24 186.5,31.5 384 srgb(255,255,255)
  10: 24x16+200+24 211.5,31.5 384 srgb(255,255,255)
  11: 24x16+231+24 242.5,31.5 384 srgb(255,255,255)
  12: 24x16+256+24 267.5,31.5 384 srgb(255,255,255)
  19: 24x16+175+41 186.5,48.5 384 srgb(255,255,255)
  20: 24x16+200+41 211.5,48.5 384 srgb(255,255,255)
  21: 24x16+231+41 242.5,48.5 384 srgb(255,255,255)
  22: 24x16+256+41 267.5,48.5 384 srgb(255,255,255)
  29: 24x16+175+58 186.5,65.5 384 srgb(255,255,255)
  30: 24x16+200+58 211.5,65.5 384 srgb(255,255,255)
  31: 24x16+231+58 242.5,65.5 384 srgb(255,255,255)
  32: 24x16+256+58 267.5,65.5 384 srgb(255,255,255)
  34: 24x16+8+64 19.5,71.5 384 srgb(255,255,255)
  35: 24x16+33+64 44.5,71.5 384 srgb(255,255,255)
  36: 24x16+58+64 69.5,71.5 384 srgb(255,255,255)
  37: 24x16+83+64 94.5,71.5 384 srgb(255,255,255)
  42: 24x16+175+75 186.5,82.5 384 srgb(255,255,255)
  43: 24x16+200+75 211.5,82.5 384 srgb(255,255,255)
  50: 24x16+8+81 19.5,88.5 384 srgb(255,255,255)
  51: 24x16+33+81 44.5,88.5 384 srgb(255,255,255)
  52: 24x16+58+81 69.5,88.5 384 srgb(255,255,255)
  53: 24x16+83+81 94.5,88.5 384 srgb(255,255,255)
  62: 24x16+175+92 186.5,99.5 384 srgb(255,255,255)
  63: 24x16+200+92 211.5,99.5 384 srgb(255,255,255)
  64: 24x16+231+92 242.5,99.5 384 srgb(255,255,255)
  65: 24x16+256+92 267.5,99.5 384 srgb(255,255,255)
  70: 24x16+175+109 186.5,116.5 384 srgb(255,255,255)
  71: 24x16+200+109 211.5,116.5 384 srgb(255,255,255)
  72: 24x16+231+109 242.5,116.5 384 srgb(255,255,255)
  73: 24x16+256+109 267.5,116.5 384 srgb(255,255,255)
  75: 24x16+8+112 19.5,119.5 384 srgb(255,255,255)
  76: 24x16+34+112 45.5,119.5 384 srgb(255,255,255)
  79: 24x16+144+113 155.5,120.5 384 srgb(0,0,0)
  86: 24x16+8+130 19.5,137.5 384 srgb(255,255,255)
  96: 24x16+8+147 19.5,154.5 384 srgb(255,255,255)
  97: 24x16+33+147 44.5,154.5 384 srgb(255,255,255)
  98: 24x16+58+147 69.5,154.5 384 srgb(255,255,255)
  99: 24x16+83+147 94.5,154.5 384 srgb(255,255,255)
  101: 24x16+144+147 155.5,154.5 384 srgb(0,0,0)
  119: 24x16+8+164 19.5,171.5 384 srgb(255,255,255)
  123: 24x16+8+181 19.5,188.5 384 srgb(255,255,255)
  126: 24x16+8+198 19.5,205.5 384 srgb(255,255,255)
  127: 24x16+33+198 44.5,205.5 384 srgb(255,255,255)
  1: 101x18+7+22 57.0,30.5 282 srgb(0,0,0)
  87: 24x16+119+130 129.1,137.7 272 srgb(0,0,0)
  130: 24x16+40+232 51.1,239.8 246 srgb(0,0,0)
  135: 24x16+144+232 155.1,239.8 246 srgb(0,0,0)
  144: 24x16+40+256 51.1,265.5 246 srgb(0,0,0)
  149: 24x16+144+256 155.1,263.8 246 srgb(0,0,0)
  100: 24x16+119+147 130.5,154.5 241 srgb(0,0,0)
  68: 24x16+144+96 152.3,105.7 213 srgb(0,0,0)
  88: 24x16+144+130 150.6,136.5 203 srgb(0,0,0)
  78: 14x16+129+113 137.5,120.3 156 srgb(0,0,0)
  67: 24x16+119+96 132.2,107.5 150 srgb(0,0,0)
  102: 14x14+124+148 130.4,154.2 138 srgb(255,255,255)
  138: 21x9+42+235 52.3,239.0 138 srgb(255,255,255)
  139: 21x9+146+235 156.3,239.0 138 srgb(255,255,255)
  152: 21x9+146+259 156.3,263.0 138 srgb(255,255,255)
  132: 12x16+84+232 91.1,239.5 111 srgb(0,0,0)
  137: 12x16+188+232 195.1,239.5 111 srgb(0,0,0)
  146: 12x16+84+256 91.1,263.5 111 srgb(0,0,0)
  151: 12x16+188+256 195.1,263.5 111 srgb(0,0,0)

```

define the rayGUI implementation define BEFORE include.
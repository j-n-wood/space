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

## Initialisation errors

Driver update, check `nvidia-smi`.

To update manually:

1. Update your package lists
sudo apt update

2. Tell Ubuntu to fix any broken or half-installed packages
sudo apt --fix-broken install

3. Explicitly trigger a reconfiguration of the NVIDIA kernel modules
sudo dpkg-reconfigure nvidia-kernel-common-580



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

magick ui_buttons_out.png \
  -alpha extract \
  -threshold 90% \
  -define connected-components:verbose=true \
  -define connected-components:area-threshold=100 \
  -connected-components 4 \
  null:

Manual location: 174x23 - 224x125 (51x103) - EC controls (6)
(7) -> general body controls

define the rayGUI implementation define BEFORE include.

magick Items_out.png \
  -alpha extract \
  -threshold 90% \
  -define connected-components:verbose=true \
  -define connected-components:area-threshold=100 \
  -connected-components 4 \
  null:

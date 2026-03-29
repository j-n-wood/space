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
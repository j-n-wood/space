# Automation

## Unit tests on save

Watch source and test files, rebuild and run tests automatically on each save.

Requires [entr](https://eradman.com/entrproject/): `brew install entr` (macOS) or `sudo apt install entr` (Linux).

```bash
find src include tests -name '*.cpp' -o -name '*.h' | entr -c sh -c 'make tests config=debug_arm64 -j10 && ./bin/Debug/tests'
```

`-c` clears the screen between runs. Incremental builds with `-j10` mean only changed files recompile.

If you add or delete source files, restart the command (`entr` watches a fixed file list). To handle this automatically, wrap in a loop with `-d`:

```bash
while true; do find src include tests -name '*.cpp' -o -name '*.h' | entr -d -c sh -c 'make tests config=debug_arm64 -j10 && ./bin/Debug/tests'; done
```

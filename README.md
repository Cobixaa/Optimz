## Optimz

Command-line tool to optimize already-compiled C/C++ binaries on Linux and Termux.

### Features
- Strips symbols to reduce binary size (uses `llvm-strip` or `strip` if available)
- Optionally compresses binaries with `upx` if installed
- Optionally compresses debug sections with `llvm-objcopy`/`objcopy` if present
- Detects ELF binaries; safely skips non-ELF files
- Works on Linux x86_64 and Termux (aarch64/arm64)

### Build (no Makefile)
```bash
cd Optimz
mkdir -p bin
g++ -O2 -std=c++17 -Wall -Wextra -Wpedantic -o bin/Opt src/main.cpp
```

### Usage
```bash
./bin/Opt <program_path> -<times>

# Examples
./bin/Opt samples/hello -1
./bin/Opt /path/to/app -3
```

- The `-<times>` argument specifies how many optimization passes to run (default: 1 if omitted).
- The tool makes a one-time backup next to the target as `<program_path>.bak` on the first run.
 - Steps attempted each pass (skipping unavailable tools):
   - Strip unneeded and all symbols (`llvm-strip`/`strip`)
   - Remove debug info and metadata sections; compress debug sections (`llvm-objcopy`/`objcopy`)
   - Shrink RPATH (`patchelf`)
   - Aggressive super-strip if available (`sstrip`)
   - Final packing (`upx --best --lzma`)

### Termux notes
- Ensure the `clang` and `binutils`/`llvm` packages are installed:
```bash
pkg install clang
# Optional tools the optimizer can use if present
pkg install binutils upx patchelf
```

### Test locally
Build the tool and a sample program, then optimize it:
```bash
# build Optimz (see Build section above)

# build sample
cc -O2 -Wall -Wextra -pipe -o samples/hello samples/hello.c

# run optimizer
./bin/Opt samples/hello -2
```

### Install (optional)
Copy the built binary to a directory in your `PATH`:
```bash
# Linux (may require sudo)
install -m 0755 bin/Opt /usr/local/bin/Opt

# Termux
install -m 0755 bin/Opt $PREFIX/bin/Opt
```

### Notes
- Optimizations are conservative and rely on external tools when available. If a tool is missing, the corresponding step is skipped.
- Running more passes than necessary is harmless; later passes will generally be no-ops once the binary stops shrinking.
- 

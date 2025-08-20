## Optimz

Command-line tool to optimize already-compiled C/C++ binaries on Linux and Termux.

### Features
- Strips symbols to reduce binary size (uses `llvm-strip` or `strip` if available)
- Optionally compresses binaries with `upx` if installed
- Optionally compresses debug sections with `llvm-objcopy`/`objcopy` if present
- Detects ELF binaries; safely skips non-ELF files
- Works on Linux x86_64 and Termux (aarch64/arm64)

### Build
```bash
cd Optimz
make
```

The binary will be created at `bin/Opt`.

### Usage
```bash
./bin/Opt <program_path> -<times>

# Examples
./bin/Opt samples/hello -1
./bin/Opt /path/to/app -3
```

- The `-<times>` argument specifies how many optimization passes to run (default: 1 if omitted).
- The tool makes a one-time backup next to the target as `<program_path>.bak` on the first run.

### Termux notes
- Ensure the `clang`, `make`, and `binutils`/`llvm` packages are installed:
```bash
pkg install clang make
# Optional tools the optimizer can use if present
pkg install binutils upx
```

### Test locally
Build the tool and a sample program, then optimize it:
```bash
make samples
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


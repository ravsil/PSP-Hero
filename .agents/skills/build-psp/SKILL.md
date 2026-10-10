---
name: build-psp
description: >-
  Build, compile, and package PSP Hero for PlayStation Portable (real hardware and PPSSPP emulator) using the Dockerized toolchain. Use when compiling the game, cleaning builds, updating Makefile rules, or troubleshooting PSP compilation issues.
---

# PSP Hero Compilation Runbook

This skill guides you through compiling PSP Hero into an encrypted `EBOOT.PBP` compatible with both real PSP hardware and PPSSPP.

## 1. Prerequisites
- Docker engine installed and running.
- Access to the official PSP toolchain container: `pspdev/pspdev:latest`.

## 2. Standard Build Command
Run the provided Docker build script from the root repository directory:
```bash
./docker-build.sh
```

### What this command executes:
1. Mounts the workspace to `/src` inside the container:
   `docker run --rm -v $(pwd):/src -w /src pspdev/pspdev:latest make -j$(nproc)`
2. Compiles all C++ sources with `psp-g++ -std=c++17 -fno-exceptions -fno-rtti -O2 -G0`.
3. Links object files with required static libraries:
   `-lstdc++ -lopusfile -lopus -logg -lpspaudio -lpsppower -lpspgum -lpspgu -lpsprtc -lm`
   *(Vorbis decoding is handled by embedded `stb_vorbis`, MP3 via embedded `minimp3`).*
4. Generates `psphero.elf`, exports symbols via `psp-fixup-imports`, and creates `psphero.prx`.
5. Encrypts and signs `psphero.prx` with `PrxEncrypter` (enables OFW/CFW execution without "Data corrupted" errors).
6. Packages `EBOOT.PBP` containing `PARAM.SFO`, `ICON0.PNG`, and the encrypted `psphero.prx`.

## 3. Clean Build
To perform a complete clean build:
```bash
docker run --rm -v $(pwd):/src -w /src pspdev/pspdev:latest make clean
./docker-build.sh
```

## 4. Build Artifacts
After a successful build, verify the presence of:
- `EBOOT.PBP` (~5.4 MB) — The final executable package to copy to the PSP.
- `psphero.elf` — Unstripped debug ELF.
- `psphero.prx` — Relocatable PRX module.

## 5. Troubleshooting Common Build Issues
- **`cannot find -lvorbisidec` or `-lopusfile`**: Verify that `-L$(shell psp-config --pspsdk-path)/lib` and standard PSP include directories are resolved in the Makefile.
- **"Data is corrupted" on real PSP**: Ensure `BUILD_PRX = 1` and `ENCRYPT = 1` are present in `Makefile`.
- **Undefined RTC or Audio symbols**: Ensure `-lpspaudio` and `-lpsprtc` are listed in `LIBS`.

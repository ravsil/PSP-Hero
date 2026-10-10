---
name: deploy-psp
description: >-
  Deploy PSP Hero to real PlayStation Portable hardware or PPSSPP emulator. Use when transferring game builds, setting up memory stick directories, configuring controls, or testing on hardware.
---

# Deploying PSP Hero to Hardware & PPSSPP

This skill provides step-by-step instructions for deploying and testing PSP Hero on real PSP consoles and the PPSSPP emulator.

## 1. Directory Structure on PSP Memory Stick
On a real PSP (via USB connection or Memory Stick / micro SD adapter), game folders are located at:
```text
ms0:/PSP/GAME/cloneHero/
├── EBOOT.PBP           # The signed, encrypted game executable
└── songs/              # The songs folder containing all song subdirectories
    ├── Song 1/
    ├── Song 2/
    └── ...
```

## 2. Deploying to Real Hardware
1. Connect the PSP to your PC via USB and enable **USB Connection** in the PSP XMB menu.
2. Navigate to your Memory Stick mount point (e.g. `/media/user/disk/` or drive letter `E:`).
3. If `PSP/GAME/cloneHero` does not exist, create it:
   ```bash
   mkdir -p /path/to/psp/PSP/GAME/cloneHero
   ```
4. Copy `EBOOT.PBP` and the `songs/` folder:
   ```bash
   cp EBOOT.PBP /path/to/psp/PSP/GAME/cloneHero/
   cp -r songs /path/to/psp/PSP/GAME/cloneHero/
   sync
   ```
5. Disconnect the USB connection.
6. Launch **PSP Hero** from the Game -> Memory Stick menu in the PSP XMB.

---

## 3. Testing in PPSSPP Emulator
1. **PPSSPP Game Directory**:
   - Linux: `~/.config/ppsspp/PSP/GAME/cloneHero/`
   - Windows: `<PPSSPP folder>\memstick\PSP\GAME\cloneHero\`
   - macOS: `~/Library/Application Support/PPSSPP/PSP/GAME/cloneHero/`
2. **Direct Launch via PPSSPP CLI**:
   ```bash
   ppsspp EBOOT.PBP
   ```
   *(Or open `EBOOT.PBP` directly from the PPSSPP File menu).*

---

## 4. Default Game Controls
- **Green Fret**: D-Pad Left
- **Red Fret**: D-Pad Down
- **Yellow Fret**: Triangle
- **Blue Fret**: Circle
- **Orange Fret**: Cross
- **Strum Up / Down**: D-Pad Up / D-Pad Down
- **Star Power**: Select / L-Trigger / R-Trigger
- **Pause Menu**: Start

*Custom controls can be configured in the in-game **Button Remapper** menu and are saved persistently to `config.dat`.*

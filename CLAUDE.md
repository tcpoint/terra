# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A collection of guitar pedal DSP effects for the Electrosmith Daisy platform (Daisy Seed / Daisy Petal),
targeting the PedalPCB Terrarium enclosure. Code is derived from the `DaisyExamples` repo and libraries
(`libDaisy`, `DaisySP`). Each top-level directory is one standalone effect (delay, chorus, flanger, verb,
compressor, tremolo, etc.) with its own `Makefile`, `.cpp` source(s), and `README.md` describing knob/switch
controls.

This repo (`terra`) is expected to live as a sibling of a separately-cloned `DaisyExamples` checkout, e.g.:

```
~/projects/DaisyExamples/   (git clone --recursive https://github.com/electro-smith/DaisyExamples)
~/projects/terra/           (this repo)
```

Every effect's `Makefile` references the libraries via `../../DaisyExamples/libDaisy` and
`../../DaisyExamples/DaisySP`, so build commands must be run from inside an effect's own directory.

## Build / flash commands

Run from inside a specific effect directory (e.g. `cd basic-delay`):

- `make` — build `build/<target>.bin` / `.hex` / `.elf` for the STM32H750
- `make clean` — remove that effect's `build/` directory
- `make program-dfu` — flash over USB DFU (put the Daisy in bootloader/DFU mode first)
- `make program` — flash via OpenOCD (only works for the `BOOT` app type; see the generic Makefile for details)
- `make debug` / `make openocd` — start OpenOCD and a GDB session (requires OpenOCD running via `make openocd` first)

There is no top-level build — there's no single Makefile that builds all effects at once. Build systems,
flags, and flash targets are all inherited from the generic Makefile in
`DaisyExamples/libDaisy/core/Makefile`; individual effect Makefiles only set `TARGET`, `CPP_SOURCES`, and
extra `C_INCLUDES` (most add `-I../Terrarium` for the shared hardware header).

There is no test suite — verification is done by building, flashing to hardware, and listening.

## Architecture

- **`Terrarium/terrarium.h`** — the one genuinely shared file. Defines the `terrarium::Terrarium` enum
  class mapping the PedalPCB Terrarium board's footswitches, knobs, and LEDs to the pin/index numbers
  `DaisyPetal` expects (e.g. `Terrarium::KNOB_1`, `Terrarium::FOOTSWITCH_1`). Effects that use the Petal
  hardware add `-I../Terrarium` to their Makefile and `#include "terrarium.h"`.
- **No shared DSP library.** Helper files like `biquad.h`/`biquad.cpp` and `common.h` are duplicated
  byte-for-byte across several effect directories (`double-chorus`, `echoverb`, `overdrive`, `mverb`,
  `tonestack`, etc.) rather than factored into a shared location. If you fix a bug in one copy, it will
  not propagate to the others — check whether the same fix is needed elsewhere before assuming it's covered.
- **Effect structure** follows the DaisyExamples pattern almost everywhere:
  - Global `DaisyPetal petal;` plus `Parameter` objects per knob, initialized in `InitControls()`.
  - `AudioCallback(in, out, size)` runs the per-sample DSP in the audio ISR — keep this path cheap/branch-light.
  - `ProcessControls()` runs in the main `while(1)` loop (not the audio callback) and typically staggers
    ADC/knob reads and switch polling across calls using a counter modulo (e.g. `processCnt % 8`), rather
    than reading everything every call.
  - Footswitches toggle bypass/passthrough state and drive an `Led` that is `.Update()`d in the main loop.
  - Delay-based effects size their buffer with `DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS` (placed in
    external SDRAM, not internal RAM, since delay buffers are too large for internal SRAM).
- **`binaries/`** holds pre-built `.bin` files for some effects, independent of each effect's own
  `build/` output (which is git-ignored).
- **`.vscode/`** per-effect directories provide OpenOCD/SVD debug configs; also git-ignored.

## Workflow conventions from git history

Commit history shows every new effect (and most bugfix passes) went through a code review pass before
being committed — commit messages like "Add X effect with code review fixes applied" or "Apply code
review fixes to Y" are the norm, not one-offs. When adding a new effect or making non-trivial DSP changes,
run `/code-review` before committing.

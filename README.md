# MHModern

MHModern is a Manhunt 1 modernization plugin for the original PC release. It focuses on startup stability and modern Windows compatibility.

Nexus Mods: https://www.nexusmods.com/manhunt/mods/52

## Build

Requirements:

- Visual Studio 2022 Build Tools with the x86 toolchain
- PowerShell

Commands:

```powershell
cd D:\DESKTOP\MHModern
mingw32-make test
mingw32-make
```

Direct PowerShell entrypoints:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Target test
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Target dll
```

Outputs:

- `build\test_time_math.exe`
- `build\MHModern.asi`

## Install

Copy these files into the game root:

- `build\MHModern.asi`
- `MHModern.ini`

The current setup assumes Ultimate ASI Loader is already active through `dinput8.dll`, which your Manhunt install already has.

## Runtime verification

On launch, the plugin writes `MHModern.log` in the game directory. Confirm:

- the bootstrap completed
- the Miles audio patch installed, and any driver-open retries were logged if they were needed
- the raw `SFX.RAW` buffered-open shim installed if it was enabled
- the optional event patch is disabled for the release baseline unless explicitly enabled for testing
- the heap patch is disabled for the release baseline unless explicitly enabled for testing
- the input patch installed, and DirectInput auto-reacquire and raw mouse input are enabled if desired
- the version patch installed so legacy OS checks see the real Windows version
- the DirectX probe patch installed so the old `dpnhpast.dll` gate is bypassed cleanly
- the system-parameters patch installed so startup no longer toggles global screensaver, power, sticky-key, or foreground-lock settings
- the error-mode patch installed so shutdown restores the prior process error mode instead of forcing zero
- the process power-throttling opt-out applied cleanly or logged that the Windows API was unavailable
- DPI awareness enabled or fell back cleanly
- `timeGetTime`, `GetTickCount`, and pointer-probe imports were patched in `manhunt.exe`
- the internal `rdtsc` helper and wait routine patches were applied
- the crash handler installed successfully if crash dumps are enabled

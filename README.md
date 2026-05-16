# MHModern

`MHModern.asi` is a conservative Manhunt 1 modernization plugin for the original PC release. It focuses on startup stability and modern Windows compatibility.

## NexusMods Description

MHModern is compatibility patch for the original Manhunt 1 PC release. It focuses on startup stability and modern Windows compatibility. It fixes Miles startup and fallback audio, buffered `SFX.RAW` opens, DirectInput reacquire, optional raw mouse input, version checks, the old DirectX probe, global Windows setting writes, error-mode shutdown behavior, timer and busy-wait behavior, crash dumps, DPI awareness, scheduler precision, and process power-throttling opt-out.

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

## Notes

The current wait-routine patch was chosen because telemetry showed the original game busy-waiting in a tight 5 ms loop. After the patch, the dominant game-side timer hotspot disappeared, which is the main modernization win in this first release candidate.

The Miles audio pass is intentionally narrow. It does not replace Miles or rewrite the mixer. It wraps `AIL_startup` and `AIL_open_digital_driver` so startup can be diagnosed when needed. With `DetectMilesPrimaryFormat=1`, the plugin queries the default Windows render endpoint and uses that detected sample rate, bit depth, and channel count as the first fallback. The game still tries its original audio open request first; if that fails, the plugin falls back through the detected system format, the configured PCM values, and canonical stereo defaults. This keeps startup stable on Manhunt while still giving the fallback path a better format choice than a hard-coded legacy default.

The raw SFX startup read path is also narrow. Headless Ghidra analysis showed `manhunt.exe` opening `AUDIO\PC\SFX\SFX.RAW` with `FILE_FLAG_NO_BUFFERING` and `FILE_FLAG_OVERLAPPED`. `MHModern.asi` strips the no-buffering flag from that one open so the file is read with normal buffered I/O while leaving the overlapped behavior intact. This is a targeted fix for the hang-prone startup read, not a general file-I/O rewrite.

The follow-up event pass is also narrow. Ghidra analysis showed the main `SFX.RAW` streamer already has persistent event objects, but the startup/indexing path still allocates unnamed auto-reset events just to perform immediate blocking overlapped reads. `MHModern.asi` includes an optional event workaround for that path, but it remains disabled by default until it is revalidated against the stable baseline.

The heap pass is intentionally conservative, but it is no longer considered release-safe for Manhunt 1. In practice it can interfere with Miles startup on this game, so `MHModern.asi` now skips heap hardening automatically whenever the Miles audio patch is enabled, and the shipped INI leaves `EnableHeapPatch=0` by default.

The input pass is still intentionally narrow, but it is no longer limited to reacquire-only behavior. `MHModern.asi` wraps `DirectInput8Create` so device-state reads can reacquire the device automatically after common focus-loss errors such as `DIERR_INPUTLOST` and `DIERR_NOTACQUIRED`. When `EnableRawMouseInput=1`, the same wrapper also registers `WM_INPUT` on the game window and substitutes raw relative mouse deltas back into the DirectInput mouse state. This keeps the original DirectInput path intact while bypassing legacy Windows mouse acceleration and filtering for camera look.

The process-power pass is similarly narrow. When `EnablePowerThrottlingOptOut=1`, `MHModern.asi` calls `SetProcessInformation(ProcessPowerThrottling)` if the host OS exposes that API, so Windows does not apply execution-speed throttling to the game process. If the API is missing or unsupported, the plugin logs that and continues without failing startup.

The version pass is deliberately simple. `manhunt.exe` still imports `GetVersion`, which can return compatibility-limited results on modern Windows. `MHModern.asi` replaces that import with a `RtlGetVersion`-backed result so any old OS checks see the actual host version instead of a manifest-limited lie.

The DirectX probe pass is also narrow. Headless Ghidra analysis showed `manhunt.exe` calling a single compatibility-check helper before RenderWare startup. That helper loaded `DDRAW.DLL`, `D3D8.DLL`, `dpnhpast.dll`, and `D3D9.DLL`, then treated a missing `dpnhpast.dll` as a hard failure even though the caller only used the result to gate an old DirectX warning. `MHModern.asi` now replaces that probe so `dpnhpast.dll` is treated as optional while the real `DDRAW`, `DirectDrawCreateEx`, `D3D8`, and `D3D9` checks remain intact.

The `SystemParametersInfoA` pass is similarly narrow. Ghidra analysis of the main startup path showed `manhunt.exe` calling `SPI_SETSCREENSAVEACTIVE`, `SPI_SETLOWPOWERACTIVE`, `SPI_SETPOWEROFFACTIVE`, `SPI_SETSTICKYKEYS`, and `SPI_SETFOREGROUNDLOCKTIMEOUT` from the bootstrap path, which means the game can mutate the user's global Windows settings instead of just its own process behavior. `MHModern.asi` now suppresses only those writes from the main bootstrap path and leaves the rest of the game's `SystemParametersInfoA` usage intact.

The `SetErrorMode` pass is the same class of fix. Ghidra analysis of the same startup path showed the game calling `SetErrorMode(SEM_FAILCRITICALERRORS)` on startup and then `SetErrorMode(0)` on exit, which throws away any prior process error-mode bits instead of restoring them. `MHModern.asi` now preserves the original mode for that bootstrap path and restores it on shutdown.

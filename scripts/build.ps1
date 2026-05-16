param(
    [ValidateSet('dll', 'test', 'clean')]
    [string]$Target = 'dll'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'
$srcDir = Join-Path $repoRoot 'src'
$testDir = Join-Path $repoRoot 'tests'

function Find-VsDevCmd {
    $candidatePaths = @(
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat'
    )

    foreach ($candidate in $candidatePaths) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $vsWherePaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    )
    foreach ($vsWhere in $vsWherePaths) {
        if (-not (Test-Path -LiteralPath $vsWhere)) {
            continue
        }

        $installPath = & $vsWhere `
            -latest `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installPath)) {
            $candidate = Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    return $null
}

$vsDevCmd = Find-VsDevCmd

if ([string]::IsNullOrWhiteSpace($vsDevCmd) -or -not (Test-Path -LiteralPath $vsDevCmd)) {
    throw 'VsDevCmd.bat not found. Install Visual Studio Build Tools with the x86/x64 C++ toolchain.'
}

function Invoke-MsvcBuild {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandLine
    )

    $batchName = '__invoke_msvc_{0}_{1}.cmd' -f $PID, ([guid]::NewGuid().ToString('N'))
    $batchPath = Join-Path $buildDir $batchName
    $batchContents = @(
        '@echo off',
        "call `"$vsDevCmd`" -arch=x86 -host_arch=x64 >nul",
        $CommandLine
    ) -join "`r`n"
    try {
        Set-Content -LiteralPath $batchPath -Value $batchContents -Encoding Ascii
        & cmd.exe /d /c $batchPath
        if ($LASTEXITCODE -ne 0) {
            throw "MSVC command failed: $CommandLine"
        }
    } finally {
        Remove-Item -LiteralPath $batchPath -Force -ErrorAction SilentlyContinue
    }
}

if ($Target -eq 'clean') {
    if (Test-Path $buildDir) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
    exit 0
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$commonFlags = @(
    '/nologo',
    '/std:c++20',
    '/EHsc',
    '/W4',
    '/WX',
    '/permissive-',
    '/DWIN32_LEAN_AND_MEAN',
    '/DUNICODE',
    '/D_UNICODE',
    "/Fo`"$buildDir\\`"",
    "/I`"$srcDir`""
) -join ' '

if ($Target -eq 'test') {
    $testTimeMathOutput = Join-Path $buildDir 'test_time_math.exe'
    $testTimeMathSources = @(
        (Join-Path $testDir 'test_time_math.cpp'),
        (Join-Path $srcDir 'qpc_timer.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileTimeMath = "cl $commonFlags /Fe`"$testTimeMathOutput`" $($testTimeMathSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileTimeMath
    & $testTimeMathOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testPtrProbeOutput = Join-Path $buildDir 'test_ptr_probe.exe'
    $testPtrProbeSources = @(
        (Join-Path $testDir 'test_ptr_probe.cpp'),
        (Join-Path $srcDir 'ptr_probe.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compilePtrProbe = "cl $commonFlags /Fe`"$testPtrProbeOutput`" $($testPtrProbeSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compilePtrProbe
    & $testPtrProbeOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testTelemetryOutput = Join-Path $buildDir 'test_telemetry.exe'
    $testTelemetrySources = @(
        (Join-Path $testDir 'test_telemetry.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'telemetry.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileTelemetry = "cl $commonFlags /Fe`"$testTelemetryOutput`" $($testTelemetrySources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileTelemetry
    & $testTelemetryOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testAudioPatchOutput = Join-Path $buildDir 'test_audio_patch.exe'
    $testAudioPatchSources = @(
        (Join-Path $testDir 'test_audio_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'iat_patch.cpp'),
        (Join-Path $srcDir 'audio_patch.cpp'),
        (Join-Path $srcDir 'audio_patch_detail.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileAudioPatch = "cl $commonFlags /Fe`"$testAudioPatchOutput`" $($testAudioPatchSources -join ' ') ole32.lib uuid.lib"
    Invoke-MsvcBuild -CommandLine $compileAudioPatch
    & $testAudioPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testEventPatchOutput = Join-Path $buildDir 'test_event_patch.exe'
    $testEventPatchSources = @(
        (Join-Path $testDir 'test_event_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'iat_patch.cpp'),
        (Join-Path $srcDir 'event_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileEventPatch = "cl $commonFlags /Fe`"$testEventPatchOutput`" $($testEventPatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileEventPatch
    & $testEventPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testHeapPatchOutput = Join-Path $buildDir 'test_heap_patch.exe'
    $testHeapPatchSources = @(
        (Join-Path $testDir 'test_heap_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'heap_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileHeapPatch = "cl $commonFlags /Fe`"$testHeapPatchOutput`" $($testHeapPatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileHeapPatch
    & $testHeapPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testInputPatchOutput = Join-Path $buildDir 'test_input_patch.exe'
    $testInputPatchSources = @(
        (Join-Path $testDir 'test_input_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'iat_patch.cpp'),
        (Join-Path $srcDir 'input_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileInputPatch = "cl $commonFlags /Fe`"$testInputPatchOutput`" $($testInputPatchSources -join ' ') dinput8.lib dxguid.lib"
    Invoke-MsvcBuild -CommandLine $compileInputPatch
    & $testInputPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testVersionPatchOutput = Join-Path $buildDir 'test_version_patch.exe'
    $testVersionPatchSources = @(
        (Join-Path $testDir 'test_version_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'iat_patch.cpp'),
        (Join-Path $srcDir 'version_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileVersionPatch = "cl $commonFlags /Fe`"$testVersionPatchOutput`" $($testVersionPatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileVersionPatch
    & $testVersionPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testModernPatchConfigOutput = Join-Path $buildDir 'test_modern_patch_config.exe'
    $testModernPatchConfigSources = @(
        (Join-Path $testDir 'test_modern_patch_config.cpp'),
        (Join-Path $srcDir 'modern_patch_config.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileModernPatchConfig = "cl $commonFlags /Fe`"$testModernPatchConfigOutput`" $($testModernPatchConfigSources -join ' ') user32.lib"
    Invoke-MsvcBuild -CommandLine $compileModernPatchConfig
    & $testModernPatchConfigOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testDirectXProbePatchOutput = Join-Path $buildDir 'test_directx_probe_patch.exe'
    $testDirectXProbePatchSources = @(
        (Join-Path $testDir 'test_directx_probe_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'code_patch.cpp'),
        (Join-Path $srcDir 'directx_probe_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileDirectXProbePatch = "cl $commonFlags /Fe`"$testDirectXProbePatchOutput`" $($testDirectXProbePatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileDirectXProbePatch
    & $testDirectXProbePatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testCodePatchOutput = Join-Path $buildDir 'test_code_patch.exe'
    $testCodePatchSources = @(
        (Join-Path $testDir 'test_code_patch.cpp'),
        (Join-Path $srcDir 'code_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileCodePatch = "cl $commonFlags /Fe`"$testCodePatchOutput`" $($testCodePatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileCodePatch
    & $testCodePatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testSystemParametersPatchOutput = Join-Path $buildDir 'test_system_parameters_patch.exe'
    $testSystemParametersPatchSources = @(
        (Join-Path $testDir 'test_system_parameters_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'iat_patch.cpp'),
        (Join-Path $srcDir 'system_parameters_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileSystemParametersPatch = "cl $commonFlags /Fe`"$testSystemParametersPatchOutput`" $($testSystemParametersPatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileSystemParametersPatch
    & $testSystemParametersPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testErrorModePatchOutput = Join-Path $buildDir 'test_error_mode_patch.exe'
    $testErrorModePatchSources = @(
        (Join-Path $testDir 'test_error_mode_patch.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'iat_patch.cpp'),
        (Join-Path $srcDir 'error_mode_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileErrorModePatch = "cl $commonFlags /Fe`"$testErrorModePatchOutput`" $($testErrorModePatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileErrorModePatch
    & $testErrorModePatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testCrashHandlerOutput = Join-Path $buildDir 'test_crash_handler.exe'
    $testCrashHandlerSources = @(
        (Join-Path $testDir 'test_crash_handler.cpp'),
        (Join-Path $srcDir 'logger.cpp'),
        (Join-Path $srcDir 'crash_handler.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileCrashHandler = "cl $commonFlags /Fe`"$testCrashHandlerOutput`" $($testCrashHandlerSources -join ' ') dbghelp.lib"
    Invoke-MsvcBuild -CommandLine $compileCrashHandler
    & $testCrashHandlerOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $testWaitPatchOutput = Join-Path $buildDir 'test_wait_patch.exe'
    $testWaitPatchSources = @(
        (Join-Path $testDir 'test_wait_patch.cpp'),
        (Join-Path $srcDir 'wait_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }
    $compileWaitPatch = "cl $commonFlags /Fe`"$testWaitPatchOutput`" $($testWaitPatchSources -join ' ')"
    Invoke-MsvcBuild -CommandLine $compileWaitPatch
    & $testWaitPatchOutput
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    exit $LASTEXITCODE
}

function Build-MHModernDll {
    $dllOutput = Join-Path $buildDir 'MHModern.asi'
    $dllSources = @(
    (Join-Path $srcDir 'dllmain.cpp'),
    (Join-Path $srcDir 'audio_patch.cpp'),
    (Join-Path $srcDir 'audio_patch_detail.cpp'),
    (Join-Path $srcDir 'code_patch.cpp'),
    (Join-Path $srcDir 'crash_handler.cpp'),
    (Join-Path $srcDir 'directx_probe_patch.cpp'),
    (Join-Path $srcDir 'error_mode_patch.cpp'),
    (Join-Path $srcDir 'event_patch.cpp'),
    (Join-Path $srcDir 'heap_patch.cpp'),
    (Join-Path $srcDir 'iat_patch.cpp'),
    (Join-Path $srcDir 'input_patch.cpp'),
    (Join-Path $srcDir 'logger.cpp'),
    (Join-Path $srcDir 'modern_patch.cpp'),
    (Join-Path $srcDir 'modern_patch_config.cpp'),
    (Join-Path $srcDir 'ptr_probe.cpp'),
    (Join-Path $srcDir 'qpc_timer.cpp'),
    (Join-Path $srcDir 'system_parameters_patch.cpp'),
    (Join-Path $srcDir 'telemetry.cpp'),
    (Join-Path $srcDir 'version_patch.cpp'),
    (Join-Path $srcDir 'wait_patch.cpp')
    ) | ForEach-Object { "`"$_`"" }

    $dllCompile = "cl $commonFlags /LD /O2 /MD /Fe`"$dllOutput`" $($dllSources -join ' ') user32.lib winmm.lib shcore.lib dbghelp.lib dinput8.lib dxguid.lib ole32.lib uuid.lib"
    Invoke-MsvcBuild -CommandLine $dllCompile
}

if ($Target -eq 'dll') {
    Build-MHModernDll
    exit 0
}













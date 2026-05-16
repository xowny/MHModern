#include "audio_patch.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

namespace mhmodern::audio_patch::detail {

namespace {

constexpr std::uint32_t kGenericRead = 0x80000000u;
constexpr std::uint32_t kFileShareRead = 0x00000001u;
constexpr std::uint32_t kOpenExisting = 0x00000003u;
constexpr std::uint32_t kFileFlagNoBuffering = 0x20000000u;
constexpr std::uint32_t kFileFlagOverlapped = 0x40000000u;
constexpr char kRawSfxPath[] = "AUDIO\\PC\\SFX\\SFX.RAW";

}  // namespace

std::optional<DriverOpenAttempt> query_default_device_format() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize_com = hr == S_OK;
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audio_client = nullptr;
    WAVEFORMATEX* mix_format = nullptr;
    std::optional<DriverOpenAttempt> detected;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        goto cleanup;
    }

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        goto cleanup;
    }

    hr = device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&audio_client));
    if (FAILED(hr)) {
        goto cleanup;
    }

    hr = audio_client->GetMixFormat(&mix_format);
    if (FAILED(hr) || mix_format == nullptr) {
        goto cleanup;
    }

    detected = normalize_miles_driver_format(
        mix_format->nSamplesPerSec,
        static_cast<std::int32_t>(mix_format->wBitsPerSample),
        static_cast<std::int32_t>(mix_format->nChannels));

cleanup:
    if (mix_format != nullptr) {
        CoTaskMemFree(mix_format);
    }
    if (audio_client != nullptr) {
        audio_client->Release();
    }
    if (device != nullptr) {
        device->Release();
    }
    if (enumerator != nullptr) {
        enumerator->Release();
    }
    if (uninitialize_com) {
        CoUninitialize();
    }
    return detected;
}

DriverOpenAttempt normalize_miles_driver_format(
    const std::uint32_t sample_rate,
    const std::int32_t bits,
    const std::int32_t channels) {
    DriverOpenAttempt normalized{};
    normalized.sample_rate = sample_rate != 0 ? sample_rate : 48000;
    switch (bits) {
    case 8:
    case 16:
    case 24:
    case 32:
        normalized.bits = bits;
        break;
    default:
        normalized.bits = 16;
        break;
    }
    normalized.channels = channels <= 1 ? 1 : 2;
    return normalized;
}

std::size_t build_driver_attempts(
    std::uint32_t requested_rate,
    std::int32_t requested_bits,
    std::int32_t requested_channels,
    std::uint32_t fallback_rate,
    std::int32_t fallback_bits,
    std::int32_t fallback_channels,
    std::array<DriverOpenAttempt, 3>& attempts) {
    std::size_t count = 0;

    const auto push_unique = [&attempts, &count](std::uint32_t rate, std::int32_t bits, std::int32_t channels) {
        for (std::size_t index = 0; index < count; ++index) {
            if (attempts[index].sample_rate == rate &&
                attempts[index].bits == bits &&
                attempts[index].channels == channels) {
                return;
            }
        }
        if (count < attempts.size()) {
            attempts[count++] = {rate, bits, channels};
        }
    };

    push_unique(requested_rate, requested_bits, requested_channels);
    push_unique(fallback_rate, fallback_bits, fallback_channels);
    push_unique(44100, 16, 2);
    return count;
}

bool should_buffer_raw_audio_open(
    const char* file_name,
    const std::uint32_t desired_access,
    const std::uint32_t share_mode,
    const std::uint32_t creation_disposition,
    const std::uint32_t flags_and_attributes) {
    return file_name != nullptr &&
        lstrcmpiA(file_name, kRawSfxPath) == 0 &&
        desired_access == kGenericRead &&
        share_mode == kFileShareRead &&
        creation_disposition == kOpenExisting &&
        flags_and_attributes == (kFileFlagNoBuffering | kFileFlagOverlapped);
}

}  // namespace mhmodern::audio_patch::detail

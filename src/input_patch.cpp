#include "input_patch.h"

#include "iat_patch.h"
#include "logger.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

#include <new>

namespace {

using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

mhmodern::input_patch::Settings g_settings{};
DirectInput8CreateFn g_original_direct_input8_create = nullptr;

class DirectInputDeviceProxy final : public IDirectInputDevice8A {
public:
    explicit DirectInputDeviceProxy(IDirectInputDevice8A* inner)
        : reference_count_(1), inner_(inner) {
        if (inner_ != nullptr) {
            inner_->AddRef();
        }
    }

    ~DirectInputDeviceProxy() {
        if (inner_ != nullptr) {
            inner_->Release();
            inner_ = nullptr;
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDirectInputDeviceA || riid == IID_IDirectInputDevice2A ||
            riid == IID_IDirectInputDevice7A || riid == IID_IDirectInputDevice8A) {
            *object = static_cast<IDirectInputDevice8A*>(this);
            AddRef();
            return S_OK;
        }
        return inner_->QueryInterface(riid, object);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&reference_count_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG count = InterlockedDecrement(&reference_count_);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS device_caps) override {
        return inner_->GetCapabilities(device_caps);
    }

    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA callback, LPVOID reference, DWORD flags) override {
        return inner_->EnumObjects(callback, reference, flags);
    }

    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID property_guid, LPDIPROPHEADER property_header) override {
        return inner_->GetProperty(property_guid, property_header);
    }

    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID property_guid, LPCDIPROPHEADER property_header) override {
        return inner_->SetProperty(property_guid, property_header);
    }

    HRESULT STDMETHODCALLTYPE Acquire() override {
        return inner_->Acquire();
    }

    HRESULT STDMETHODCALLTYPE Unacquire() override {
        return inner_->Unacquire();
    }

    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD data_size, LPVOID data) override {
        HRESULT result = inner_->GetDeviceState(data_size, data);
        if (!g_settings.auto_reacquire || !mhmodern::input_patch::detail::should_retry_input_hresult(result)) {
            return result;
        }

        const HRESULT acquire_result = inner_->Acquire();
        if (FAILED(acquire_result) && g_settings.log_input_init) {
            mhmodern::logger::write("DirectInput device reacquire failed: hr=0x%08X", static_cast<unsigned>(acquire_result));
            return result;
        }

        result = inner_->GetDeviceState(data_size, data);
        if (g_settings.log_input_init) {
            mhmodern::logger::write("DirectInput device reacquire retry: hr=0x%08X", static_cast<unsigned>(result));
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD object_data_size, LPDIDEVICEOBJECTDATA object_data, LPDWORD entries, DWORD flags) override {
        HRESULT result = inner_->GetDeviceData(object_data_size, object_data, entries, flags);
        if (!g_settings.auto_reacquire || !mhmodern::input_patch::detail::should_retry_input_hresult(result)) {
            return result;
        }

        const HRESULT acquire_result = inner_->Acquire();
        if (FAILED(acquire_result)) {
            return result;
        }
        return inner_->GetDeviceData(object_data_size, object_data, entries, flags);
    }

    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT data_format) override {
        return inner_->SetDataFormat(data_format);
    }

    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE event_handle) override {
        return inner_->SetEventNotification(event_handle);
    }

    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND window, DWORD flags) override {
        return inner_->SetCooperativeLevel(window, flags);
    }

    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA object_instance, DWORD object, DWORD how) override {
        return inner_->GetObjectInfo(object_instance, object, how);
    }

    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA device_instance) override {
        return inner_->GetDeviceInfo(device_instance);
    }

    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND owner, DWORD flags) override {
        return inner_->RunControlPanel(owner, flags);
    }

    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE instance, DWORD version, REFGUID guid) override {
        return inner_->Initialize(instance, version, guid);
    }

    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID guid, LPCDIEFFECT effect, LPDIRECTINPUTEFFECT* direct_input_effect, LPUNKNOWN outer) override {
        return inner_->CreateEffect(guid, effect, direct_input_effect, outer);
    }

    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA callback, LPVOID reference, DWORD effect_type) override {
        return inner_->EnumEffects(callback, reference, effect_type);
    }

    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA effect_info, REFGUID guid) override {
        return inner_->GetEffectInfo(effect_info, guid);
    }

    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD out_flags) override {
        return inner_->GetForceFeedbackState(out_flags);
    }

    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD flags) override {
        return inner_->SendForceFeedbackCommand(flags);
    }

    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK callback, LPVOID reference, DWORD flags) override {
        return inner_->EnumCreatedEffectObjects(callback, reference, flags);
    }

    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE escape) override {
        return inner_->Escape(escape);
    }

    HRESULT STDMETHODCALLTYPE Poll() override {
        HRESULT result = inner_->Poll();
        if (!g_settings.auto_reacquire || !mhmodern::input_patch::detail::should_retry_input_hresult(result)) {
            return result;
        }
        const HRESULT acquire_result = inner_->Acquire();
        if (FAILED(acquire_result)) {
            return result;
        }
        return inner_->Poll();
    }

    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD object_data_size, LPCDIDEVICEOBJECTDATA object_data, LPDWORD entries, DWORD flags) override {
        return inner_->SendDeviceData(object_data_size, object_data, entries, flags);
    }

    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR file_name, LPDIENUMEFFECTSINFILECALLBACK callback, LPVOID reference, DWORD flags) override {
        return inner_->EnumEffectsInFile(file_name, callback, reference, flags);
    }

    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR file_name, DWORD entries, LPDIFILEEFFECT effects, DWORD flags) override {
        return inner_->WriteEffectToFile(file_name, entries, effects, flags);
    }

    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA action_format, LPCSTR user_name, DWORD flags) override {
        return inner_->BuildActionMap(action_format, user_name, flags);
    }

    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA action_format, LPCSTR user_name, DWORD flags) override {
        return inner_->SetActionMap(action_format, user_name, flags);
    }

    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA image_info_header) override {
        return inner_->GetImageInfo(image_info_header);
    }

private:
    LONG reference_count_;
    IDirectInputDevice8A* inner_;
};

class DirectInput8Proxy final : public IDirectInput8A {
public:
    explicit DirectInput8Proxy(IDirectInput8A* inner)
        : reference_count_(1), inner_(inner) {
        if (inner_ != nullptr) {
            inner_->AddRef();
        }
    }

    ~DirectInput8Proxy() {
        if (inner_ != nullptr) {
            inner_->Release();
            inner_ = nullptr;
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDirectInput8A) {
            *object = static_cast<IDirectInput8A*>(this);
            AddRef();
            return S_OK;
        }
        return inner_->QueryInterface(riid, object);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&reference_count_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG count = InterlockedDecrement(&reference_count_);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICE8A* device, LPUNKNOWN outer) override {
        if (device == nullptr) {
            return E_POINTER;
        }
        IDirectInputDevice8A* real_device = nullptr;
        const HRESULT result = inner_->CreateDevice(guid, &real_device, outer);
        if (FAILED(result) || real_device == nullptr) {
            return result;
        }

        auto* proxy = new (std::nothrow) DirectInputDeviceProxy(real_device);
        real_device->Release();
        if (proxy == nullptr) {
            return E_OUTOFMEMORY;
        }

        if (g_settings.log_input_init) {
            mhmodern::logger::write("DirectInput CreateDevice wrapped: device=%p", real_device);
        }

        *device = proxy;
        return result;
    }

    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD device_type, LPDIENUMDEVICESCALLBACKA callback, LPVOID reference, DWORD flags) override {
        return inner_->EnumDevices(device_type, callback, reference, flags);
    }

    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID guid) override {
        return inner_->GetDeviceStatus(guid);
    }

    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND owner, DWORD flags) override {
        return inner_->RunControlPanel(owner, flags);
    }

    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE instance, DWORD version) override {
        return inner_->Initialize(instance, version);
    }

    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID guid_class, LPCSTR name, LPGUID guid_instance) override {
        return inner_->FindDevice(guid_class, name, guid_instance);
    }

    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(
        LPCSTR user_name,
        LPDIACTIONFORMATA action_format,
        LPDIENUMDEVICESBYSEMANTICSCBA callback,
        LPVOID reference,
        DWORD flags) override {
        return inner_->EnumDevicesBySemantics(user_name, action_format, callback, reference, flags);
    }

    HRESULT STDMETHODCALLTYPE ConfigureDevices(
        LPDICONFIGUREDEVICESCALLBACK callback,
        LPDICONFIGUREDEVICESPARAMSA configure_params,
        DWORD flags,
        LPVOID reference) override {
        return inner_->ConfigureDevices(callback, configure_params, flags, reference);
    }

private:
    LONG reference_count_;
    IDirectInput8A* inner_;
};

HRESULT WINAPI hooked_direct_input8_create(
    HINSTANCE instance,
    DWORD version,
    REFIID riid,
    LPVOID* out,
    LPUNKNOWN outer) {
    if (g_original_direct_input8_create == nullptr) {
        return E_FAIL;
    }

    HRESULT result = g_original_direct_input8_create(instance, version, riid, out, outer);
    if (FAILED(result) || out == nullptr || *out == nullptr) {
        return result;
    }

    if (riid == IID_IDirectInput8A) {
        auto* proxy = new (std::nothrow) DirectInput8Proxy(static_cast<IDirectInput8A*>(*out));
        if (proxy == nullptr) {
            return E_OUTOFMEMORY;
        }
        static_cast<IDirectInput8A*>(*out)->Release();
        *out = proxy;
        if (g_settings.log_input_init) {
            mhmodern::logger::write("DirectInput8Create wrapped IDirectInput8A");
        }
    }

    return result;
}

}  // namespace

namespace mhmodern::input_patch::detail {

bool should_retry_input_hresult(HRESULT result) {
    return result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED;
}

}  // namespace mhmodern::input_patch::detail

namespace mhmodern::input_patch {

bool install(const Settings& settings) {
    g_settings = settings;
    if (!settings.enabled) {
        return true;
    }

    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original = nullptr;
    const bool patched = mhmodern::patch_iat(
        exe_module,
        "DINPUT8.dll",
        "DirectInput8Create",
        reinterpret_cast<void*>(&hooked_direct_input8_create),
        &original);
    g_original_direct_input8_create = reinterpret_cast<DirectInput8CreateFn>(original);

    mhmodern::logger::write(
        "Input patch: DirectInput8Create=%s original=%p autoReacquire=%d log=%d",
        patched ? "yes" : "no",
        original,
        settings.auto_reacquire ? 1 : 0,
        settings.log_input_init ? 1 : 0);
    return patched;
}

}  // namespace mhmodern::input_patch

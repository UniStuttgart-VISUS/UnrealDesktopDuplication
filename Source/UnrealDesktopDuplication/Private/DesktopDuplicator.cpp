// <copyright file="DesktopDuplicator.cpp" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2025 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE file for details.
// </copyright>
// <author>Christoph Müller</author>

#include "DesktopDuplicator.h"

#include <cassert>
#include <regex>

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <dxgi1_2.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "Runtime/RHI/Public/RHI.h"


// TODO: find out how this is done correctly ...
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")


DEFINE_LOG_CATEGORY(DesktopDuplicatorLog);


/*
 * UDesktopDuplicator::UDesktopDuplicator
 */
UDesktopDuplicator::UDesktopDuplicator(void)
    : _device(nullptr), _duplication(nullptr) { }

/*
 * UDesktopDuplicator::UDesktopDuplicator
 */
UDesktopDuplicator::UDesktopDuplicator(const FObjectInitializer& initialiser)
    : Super(initialiser), _device(nullptr), _duplication(nullptr) { }


/*
 * UDesktopDuplicator::~UDesktopDuplicator
 */
UDesktopDuplicator::~UDesktopDuplicator(void) noexcept {
    if (this->_device != nullptr) {
        this->_device->Release();
    }
    if (this->_duplication != nullptr) {
        this->_duplication->Release();
    }
}


/*
 * UDesktopDuplicator::Start
 */
bool UDesktopDuplicator::Start(void) {
    if (this->_duplication != nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("The desktop duplicator is already running."));
        return false;
    }

    IDXGIOutput *output = this->GetOutputForDisplayName(this->DisplayName);
    if (output == nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("Could not find output \"%s\" to be duplicated."),
            *this->DisplayName);
        return false;
    }

    IDXGIOutput1 *output1 = nullptr;
    {
        auto hr = output->QueryInterface(::IID_IDXGIOutput1,
            reinterpret_cast<void **>(&output1));

        // We don't need the gen1 output anymore, regardless of what happened.
        output->Release();
        output = nullptr;

        if (FAILED(hr)) {
            UE_LOG(DesktopDuplicatorLog,
                Error,
                TEXT("Failed to obtain IDXGIOutput1: 0x%x."), hr);
            return false;
        }
    }

    auto unrealDevice = GDynamicRHI->RHIGetNativeDevice();
    IUnknown *unknownDevice = nullptr;
    if (unrealDevice == nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Warning,
            TEXT("The game does not seem to use Direct3D. The desktop ")
            TEXT("duplicator will create its own device and move the data ")
            TEXT("via system RAM."));
        //throw "TODO";

    } else {
        unknownDevice = static_cast<IUnknown *>(unrealDevice);
    }

    {
        auto hr = output1->DuplicateOutput(unknownDevice, &this->_duplication);
        if (FAILED(hr)) {
            UE_LOG(DesktopDuplicatorLog,
                Error,
                TEXT("Duplicating output \"%s\" failed with with error 0x%x."),
                *this->DisplayName, hr);
            assert(this->_duplication == nullptr);
        }
    }

    return (this->_duplication != nullptr);
}


/*
 * UDesktopDuplicator::GetOutputForDisplayName
 */
IDXGIOutput *UDesktopDuplicator::GetOutputForDisplayName(
        const FString& name) noexcept {
    IDXGIAdapter1 *adapter = nullptr;
    DXGI_OUTPUT_DESC desc{ };
    IDXGIFactory1 *factory = nullptr;
    IDXGIOutput *output = nullptr;

    //// Convert 'display' into a regular expression.
    //{
    //    auto it = std::remove_if(display.begin(),
    //        display.end(),
    //        [](const wchar_t c) { return (c == L'\\') || (c == '.'); });
    //    display.erase(it, display.end());
    //    display.insert(0, L".*");
    //    display.push_back(L'$');
    //}

    //std::wregex rx(display, std::regex_constants::icase);

    auto hr = ::CreateDXGIFactory1(::IID_IDXGIFactory1,
            reinterpret_cast<void **>(&factory));
    if (FAILED(hr)) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("Failed to obtain DXGI factory with error 0x%x."), hr);
    }

    for (DWORD a = 0; SUCCEEDED(hr); ++a) {
        auto ir = factory->EnumAdapters1(a, &adapter);
        if (FAILED(hr)) {
            UE_LOG(DesktopDuplicatorLog,
                Error,
                TEXT("Failed to obtain DXGI adapter %d with error 0x%x."),
                a, ir);
        }

        for (DWORD o = 0; SUCCEEDED(ir); ++o) {
            ir = adapter->EnumOutputs(o, &output);

            if (SUCCEEDED(ir)) {
                ir = output->GetDesc(&desc);
                assert(SUCCEEDED(ir));

                if (true) {
                    UE_LOG(DesktopDuplicatorLog,
                        Display,
                        TEXT("Found DXGI output \"%ls\"."), desc.DeviceName);
                    goto output_found;
                }

                //if (std::regex_match(desc.DeviceName, rx)
                //    && candidate.try_query_to(output.put())) {
                //    return true;
                //}

                // This was not the output we are looking for.
                assert(output != nullptr);
                output->Release();
                output = nullptr;

            } else if (ir != DXGI_ERROR_NOT_FOUND) {
                UE_LOG(DesktopDuplicatorLog,
                    Error,
                    TEXT("Failed to obtain DXGI output %d of adapter %d ")
                    TEXT("with error 0x%x."), o, a, ir);
            }
        } /* for (DWORD o = 0; SUCCEEDED(ir); ++o) */

        if (adapter != nullptr) {
            adapter->Release();
            adapter = nullptr;
        }
    } /* for (DWORD a = 0; SUCCEEDED(hr); ++a) */

output_found:
    assert(adapter == nullptr);
    if (factory != nullptr) {
        factory->Release();
    }

    return output;
}

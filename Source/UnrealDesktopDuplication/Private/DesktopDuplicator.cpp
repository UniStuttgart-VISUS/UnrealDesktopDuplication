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
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "Misc/ScopeExit.h"

#include "Runtime/RHI/Public/RHI.h"

#include "ID3D11DynamicRHI.h"


// TODO: find out how this is done correctly ...
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")


DEFINE_LOG_CATEGORY(DesktopDuplicatorLog);


/*
 * UDesktopDuplicator::UDesktopDuplicator
 */
UDesktopDuplicator::UDesktopDuplicator(void)
    : AllowGpuCopy(false),
    _context(nullptr),
    _device(nullptr),
    _duplication(nullptr),
    _fence(nullptr),
    _stagingTexture(nullptr) { }


/*
 * UDesktopDuplicator::UDesktopDuplicator
 */
UDesktopDuplicator::UDesktopDuplicator(const FObjectInitializer& initialiser)
    : Super(initialiser),
    _context(nullptr),
    _device(nullptr),
    _duplication(nullptr),
    _fence(nullptr),
    _stagingTexture(nullptr) { }


/*
 * UDesktopDuplicator::~UDesktopDuplicator
 */
UDesktopDuplicator::~UDesktopDuplicator(void) noexcept {
    this->Stop();
}


/*
 * UDesktopDuplicator::Acquire
 */
bool UDesktopDuplicator::Acquire(const int32 timeout) noexcept {
    assert(IsInGameThread());
    if (this->_duplication == nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("The desktop duplicator is not running. Call Start() before ")
            TEXT("acquiring a frame and make sure that the display \"%s\" ")
            TEXT("exists on the system."), *this->DisplayName);
        return false;
    }

    if (this->Target == nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("No duplication target has been set."));
        return false;
    }

    if (this->_busy.AtomicSet(true)) {
        UE_LOG(DesktopDuplicatorLog,
            Display,
            TEXT("Previous duplication frame is still being processed."));
        return false;
    }

    DXGI_OUTDUPL_FRAME_INFO info { };
    IDXGIResource *resource = nullptr;

    {
        UE_LOG(DesktopDuplicatorLog,
            Display,
            TEXT("Releasing previously acquired desktop."));
        auto hr = this->_duplication->ReleaseFrame();
        if (FAILED(hr)) {
            UE_LOG(DesktopDuplicatorLog,
                Warning,
                TEXT("Releasing the previous desktop duplication frame ")
                TEXT("failed with error 0x%x. Error 0x%x is expected for the ")
                TEXT("first frame and if the previous acquisition timed out."),
                hr, DXGI_ERROR_INVALID_CALL);
        }
    }

    UE_LOG(DesktopDuplicatorLog,
        Display,
        TEXT("Acquire the next desktop with %d ms timeout."), timeout);
    auto hr = this->_duplication->AcquireNextFrame(timeout, &info, &resource);
    switch (hr) {
        case DXGI_ERROR_WAIT_TIMEOUT:
            UE_LOG(DesktopDuplicatorLog,
                Display,
                TEXT("No frame available within %d ms."), timeout);
            this->_busy.AtomicSet(false);
            return false;

        case DXGI_ERROR_ACCESS_LOST:
            UE_LOG(DesktopDuplicatorLog,
                Warning,
                TEXT("Access to the desktop duplication was lost. Restarting ")
                TEXT("the duplicator."));
            this->Stop();
            this->Start();
            this->_busy.AtomicSet(false);
            return false;

        case S_OK:
            return this->Stage(resource);

        default:
            UE_LOG(DesktopDuplicatorLog,
                Error,
                TEXT("Acquiring next frame failed with unexpected error 0x%x."),
                hr);
            this->_busy.AtomicSet(false);
            return false;
    }
}


/*
 * UDesktopDuplicator::Start
 */
bool UDesktopDuplicator::Start(void) {
    assert(IsInGameThread());

    if (this->_duplication != nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("The desktop duplicator is already running."));
        return false;
    }

    if (this->_device != nullptr) {
        UE_LOG(DesktopDuplicatorLog,
            Display,
            TEXT("The desktop duplicator is releasing it previously used ")
            TEXT("Direct3D device."));
        this->_device->Release();
        this->_device = nullptr;
    }

    auto output = this->GetOutputForDisplayName(this->DisplayName);
    if (output == nullptr) {
        return false;
    }

    if (this->AllowGpuCopy && ::IsRHID3D11()) {
        auto rhi = ::GetID3D11DynamicRHI();
        assert(this->_device == nullptr);
        this->_device = rhi->RHIGetDevice();
        assert(this->_device != nullptr);
        this->_device->AddRef();
        assert(this->_fence == nullptr);

    } else {
        UE_LOG(DesktopDuplicatorLog,
            Warning,
            TEXT("The game does not seem to use Direct3D 11. The desktop ")
            TEXT("duplicator will create its own device and move the data ")
            TEXT("via system memory."));
        assert(this->_device == nullptr);
        this->_device = this->CreateDevice();
        assert(this->_device != nullptr);

        this->_device->GetImmediateContext(&this->_context);
        assert(this->_context != nullptr);

        ID3D11Device5 *device5 = nullptr;
        ON_SCOPE_EXIT{ if (device5 != nullptr) { device5->Release(); } };

        {
            auto hr = this->_device->QueryInterface(::IID_ID3D11Device5,
                reinterpret_cast<void **>(&device5));
            if (FAILED(hr)) {
                UE_LOG(DesktopDuplicatorLog,
                    Error,
                    TEXT("Failed to obtain a ID3D11Device5, which is ")
                    TEXT("required for creating a fence: 0x%x"), hr);
                assert(device5 == nullptr);
                this->_device->Release();
                this->_device = nullptr;
            }
        }

        if (device5 != nullptr) {
            auto hr = device5->CreateFence(0,
                D3D11_FENCE_FLAG_NONE,
                ::IID_ID3D11Fence,
                reinterpret_cast<void **>(&this->_fence));
            if (FAILED(hr)) {
                UE_LOG(DesktopDuplicatorLog,
                    Error,
                    TEXT("Creating a fence for synchronisation of ")
                    TEXT("desktop duplication failed with error 0x%x."),
                    hr);
                assert(this->_fence == nullptr);
                this->_device->Release();
                this->_device = nullptr;
            }
        } /* if (device5 != nullptr) */
    } /* if (::IsRHID3D11()) */

    if (this->_device != nullptr) {
        auto hr = output->DuplicateOutput(this->_device, &this->_duplication);
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
 * UDesktopDuplicator::Stop
 */
void UDesktopDuplicator::Stop(void) noexcept {
    assert(IsInGameThread());

    if (this->_context != nullptr) {
        this->_context->Release();
        this->_context = nullptr;
    }
    if (this->_device != nullptr) {
        this->_device->Release();
        this->_device = nullptr;
    }
    if (this->_duplication != nullptr) {
        this->_duplication->Release();
        this->_duplication = nullptr;
    }
    if (this->_fence != nullptr) {
        this->_fence->Release();
        this->_fence = nullptr;
    }
    if (this->_stagingTexture != nullptr) {
        this->_stagingTexture->Release();
        this->_stagingTexture = nullptr;
    }
}


/*
 * UDesktopDuplicator::CreateDevice
 */
ID3D11Device *UDesktopDuplicator::CreateDevice(void) noexcept {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ID3D11Device *retval = nullptr;

#if ((defined(UE_BUILD_DEBUG) && (UE_BUILD_DEBUG != 0)) || (defined(UE_BUILD_DEVELOPMENT) && (UE_BUILD_DEVELOPMENT != 0)))
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif /* (defined(UE_BUILD_DEBUG) && ... */

    auto hr = ::D3D11CreateDevice(nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        flags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &retval,
        nullptr,
        nullptr);
    if (FAILED(hr)) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("Failed to create Direct3D 11 device for desktop duplication ")
            TEXT("with error 0x % x."), hr);
        assert(retval == nullptr);
    }

    return retval;
}


/*
 * UDesktopDuplicator::GetOutputForDisplayName
 */
IDXGIOutput1 *UDesktopDuplicator::GetOutputForDisplayName(
        const FString& name) noexcept {
    // Convert 'name' into a regular expression.
    auto displayName = name.Replace(TEXT("\\"), TEXT(""))
        .Replace(TEXT("."), TEXT(""));
    TString<wchar_t> displayPattern(TEXT(".*") + displayName + TEXT("$"));
    std::wregex rx(*displayPattern, std::regex_constants::icase);

    // Obtain a DXGI factory.
    IDXGIFactory1 *factory = nullptr;
    auto hr = ::CreateDXGIFactory1(::IID_IDXGIFactory1,
            reinterpret_cast<void **>(&factory));
    ON_SCOPE_EXIT { if (factory != nullptr) { factory->Release(); } };
    if (FAILED(hr)) {
        UE_LOG(DesktopDuplicatorLog,
            Error,
            TEXT("Failed to obtain DXGI factory with error 0x%x."), hr);
    }

    // Enumerate all adapters and their outputs.
    for (DWORD a = 0; SUCCEEDED(hr); ++a) {
        IDXGIAdapter1 *adapter = nullptr;
        auto ir = hr = factory->EnumAdapters1(a, &adapter);
        ON_SCOPE_EXIT { if (adapter != nullptr) { adapter->Release(); } };
        if (FAILED(hr)) {
            UE_LOG(DesktopDuplicatorLog,
                Error,
                TEXT("Failed to obtain DXGI adapter %d with error 0x%x."),
                a, ir);
        }

        for (DWORD o = 0; SUCCEEDED(ir); ++o) {
            IDXGIOutput *output = nullptr;
            ir = adapter->EnumOutputs(o, &output);
            ON_SCOPE_EXIT { if (output != nullptr) { output->Release(); } };

            // If we got another output, retrieve its description to check its
            // name. If it is the one we are looking for, retrieve its DXGI 1.2
            // interface required for desktop duplication.
            if (SUCCEEDED(ir)) {
                DXGI_OUTPUT_DESC desc;
                ir = output->GetDesc(&desc);
                assert(SUCCEEDED(ir));

                if (std::regex_match(desc.DeviceName, rx)) {
                    UE_LOG(DesktopDuplicatorLog,
                        Display,
                        TEXT("Found DXGI output \"%ls\"."), desc.DeviceName);
                    IDXGIOutput1 *retval = nullptr;
                    if (FAILED(output->QueryInterface(
                            ::IID_IDXGIOutput1,
                            reinterpret_cast<void **>(&retval)))) {
                        UE_LOG(DesktopDuplicatorLog,
                            Error,
                            TEXT("Found the requested output \"%ls\", but it ")
                            TEXT("does not support DXGI 1.2, which is ")
                            TEXT("required for desktop duplication."),
                            desc.DeviceName);
                    }

                    return retval;
                } else {
                    UE_LOG(DesktopDuplicatorLog,
                        Display,
                        TEXT("DXGI output \"%ls\" does not match \"%s\"."),
                        desc.DeviceName, *name);
                }
            } else if (ir != DXGI_ERROR_NOT_FOUND) {
                UE_LOG(DesktopDuplicatorLog,
                    Error,
                    TEXT("Failed to obtain DXGI output %d of adapter %d ")
                    TEXT("with error 0x%x."), o, a, ir);
            }
        } /* for (DWORD o = 0; SUCCEEDED(ir); ++o) */
    } /* for (DWORD a = 0; SUCCEEDED(hr); ++a) */

    UE_LOG(DesktopDuplicatorLog,
        Error,
        TEXT("Could not find output \"%s\" to be duplicated."),
        *name);
    return nullptr;
}


/*
 * UDesktopDuplicator::HasSize
 */
bool UDesktopDuplicator::HasSize(ID3D11Texture2D *target,
        ID3D11Texture2D *reference) noexcept {
    assert(reference != nullptr);
    if (target == nullptr) {
        return false;
    }

    D3D11_TEXTURE2D_DESC tgtDesc;
    target->GetDesc(&tgtDesc);

    D3D11_TEXTURE2D_DESC refDesc;
    reference->GetDesc(&refDesc);

    return (tgtDesc.Width == refDesc.Width)
        && (tgtDesc.Height == refDesc.Height)
        && (tgtDesc.Format == refDesc.Format);
}


/*
 * UDesktopDuplicator::MatchTarget
 */
bool UDesktopDuplicator::MatchTarget(ID3D11Texture2D *texture) noexcept {
    assert(texture != nullptr);
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    assert(desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM);

    const auto retval = HasSize(this->Target, desc.Width, desc.Height);

    if (!retval && (this->Target != nullptr)) {
        UE_LOG(DesktopDuplicatorLog,
            Display,
            TEXT("Resizing desktop duplication target."));
        this->Target->InitCustomFormat(desc.Width,
            desc.Height,
            EPixelFormat::PF_B8G8R8A8,
            false);
        this->Target->RenderTargetFormat
            = ETextureRenderTargetFormat::RTF_RGBA8;
        this->Target->UpdateResource();
    }

    return retval;
}


/*
 * UDesktopDuplicator::Stage
 */
bool UDesktopDuplicator::Stage(IDXGIResource *resource) noexcept {
    assert(resource != nullptr);
    assert(this->_busy);
    auto retval = true;
    ID3D11Texture2D *texture = nullptr;

    if (retval) {
        auto hr = resource->QueryInterface(&texture);

        // The contract of the method is that it release the 'resource' whatever
        // happens. Therefore, we release it right now as we never need it
        // again, regardless of whether we got its texture interface or not.
        resource->Release();

        if (FAILED(hr)) {
            UE_LOG(DesktopDuplicatorLog,
                Error,
                TEXT("The given DXGI resource is not a Direct3D 11 texture. ")
                TEXT("This should never happen as desktop duplication is ")
                TEXT("currently based on Direct3D 11."));
            assert(texture == nullptr);
            retval = false;
        }
    }

    if (retval && !this->MatchTarget(texture)) {
        UE_LOG(DesktopDuplicatorLog,
            Display,
            TEXT("Dropping desktop duplication as the target needs to be ")
            TEXT("resized."));
        retval = false;
    }

    if (retval && (this->_fence == nullptr)) {
        // The duplicator shares the device with the game, so we can perform a
        // texture copy directly on the GPU.
        assert(::IsRHID3D11());

        ENQUEUE_RENDER_COMMAND(CopyRTCommand)(
            [this, texture](FRHICommandListImmediate& cmdList) {
                assert(texture != nullptr);
                auto rhi = ::GetID3D11DynamicRHI();
                auto src = rhi->RHICreateTexture2DFromResource(
                    EPixelFormat::PF_B8G8R8A8,
                    ETextureCreateFlags::None,
                    FClearValueBinding::None,
                    texture);
                src->SetName(TEXT("Desktop source"));
                auto dst = this->Target
                    ->GetRenderTargetResource()
                    ->GetRenderTargetTexture();

                FRHIRenderPassInfo rpi(dst, ERenderTargetActions::Load_Store);
                //cmdList.BeginRenderPass(rpi, TEXT("CopyRT"));
                cmdList.CopyTexture(src, dst, FRHICopyTextureInfo());
                //cmdList.EndRenderPass();

                src.SafeRelease();
//                this->_duplication->ReleaseFrame();
                texture->Release();
                this->_busy.AtomicSet(false);
            });

        // At this point, the lambda "owns" the texture.
        texture = nullptr;

    } else if (retval) {
        // If the duplicator does have its own device, it means that the
        // duplication cannot use the same device as the game. Therefore, we
        // need to transfer the data manually via system memory.
        assert(this->_context != nullptr);

        if (!HasSize(this->_stagingTexture, texture)) {
            if (this->_stagingTexture != nullptr) {
                this->_stagingTexture->Release();
            }
            this->_stagingTexture = nullptr;
        }

        if (this->_stagingTexture == nullptr) {
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.MiscFlags = 0;
            auto hr = this->_device->CreateTexture2D(&desc, nullptr,
                &this->_stagingTexture);
            if (FAILED(hr)) {
                UE_LOG(DesktopDuplicatorLog,
                    Error,
                    TEXT("Creating a staging texture for desktop duplication ")
                    TEXT("failed with error 0x%x."), hr);
                assert(this->_stagingTexture == nullptr);
                retval = false;
            }
        }

        this->_context->CopyResource(this->_stagingTexture, texture);

        ENQUEUE_RENDER_COMMAND(UpdateRTCommand)(
            [this, texture](FRHICommandListImmediate& cmdList) {
                D3D11_MAPPED_SUBRESOURCE data { };
                auto hr = this->_context->Map(this->_stagingTexture,
                    0, D3D11_MAP_READ, 0, &data);
                if (FAILED(hr)) {
                    UE_LOG(DesktopDuplicatorLog,
                        Error,
                        TEXT("Mapping the staging texture for desktop ")
                        TEXT("duplication failed with error 0x%x."), hr);
                    return;
                }

                FUpdateTextureRegion2D region(0, 0, 0, 0,
                    this->Target->GetSurfaceWidth(),
                    this->Target->GetSurfaceHeight());

                auto res = this->Target->GetRenderTargetResource();
                GDynamicRHI->RHIUpdateTexture2D(
                    cmdList,
                    res->GetRenderTargetTexture(),
                    0,
                    region,
                    data.RowPitch,
                    static_cast<const uint8 *>(data.pData));

                this->_context->Unmap(this->_stagingTexture, 0);
                this->_busy.AtomicSet(false);
            });
    }

    if (!retval) {
        UE_LOG(DesktopDuplicatorLog,
            Warning,
            TEXT("Cleaning up resources of failed staging attempt of ")
            TEXT("duplicated desktop."));
//        this->_duplication->ReleaseFrame();
        this->_busy.AtomicSet(false);
    }

    if (texture != nullptr) {
        texture->Release();
    }

    return retval;
}

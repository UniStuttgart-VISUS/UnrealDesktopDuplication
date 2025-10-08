// <copyright file="DesktopDuplicator.Build.cs" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2025 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE file for details.
// </copyright>
// <author>Christoph Müller</author>

#pragma once

#include "CoreMinimal.h"

#include "Engine/TextureRenderTarget2D.h"

#include "HAL/ThreadSafeBool.h"

#include "DesktopDuplicator.generated.h"


// Forward declarations
class ID3D11Device;
class ID3D11DeviceContext;
class ID3D11Fence;
class ID3D11Texture2D;
class IDXGIOutput1;
class IDXGIOutputDuplication;
class IDXGIResource;
struct IUnknown;


DECLARE_LOG_CATEGORY_EXTERN(DesktopDuplicatorLog, Log, All);


/// <summary>
/// Represents the duplication of a single output to a render target.
/// </summary>
UCLASS(BlueprintType, hidecategories = (Object))
class UNREALDESKTOPDUPLICATION_API UDesktopDuplicator final : public UObject {
    GENERATED_BODY()

public:

    /// <summary>
    /// Initialises a new instance.
    /// </summary>
    UDesktopDuplicator(void);

    /// <summary>
    /// Initialises a new instance.
    /// </summary>
    /// <param name="initialiser"></param>
    UDesktopDuplicator(const FObjectInitializer& initialiser);

    /// <summary>
    /// Finalises the instance.
    /// </summary>
    virtual ~UDesktopDuplicator(void) noexcept;

    /// <summary>
    /// Allows for copying the duplicated frames without involving the CPU.
    /// </summary>
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Desktop duplication")
    bool AllowGpuCopy;

    /// <summary>
    /// Specifies the name of the display to be duplicated.
    /// </summary>
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Desktop duplication")
    FString DisplayName;

    /// <summary>
    /// The render target which receives the duplicated output.
    /// </summary>
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Desktop duplication")
    UTextureRenderTarget2D *Target;

    /// <summary>
    /// Tries to acquire a new frame to <see cref="Target"/>.
    /// </summary>
    /// <param name="timeout">The timeout for the acquisition in
    /// milliseconds.</param>
    /// <returns></returns>
    UFUNCTION(BlueprintCallable, Category = "Desktop duplication")
    bool Acquire(const int32 timeout) noexcept;

    /// <summary>
    /// Starts duplication the display identified by <see cref="DisplayName"/>.
    /// </summary>
    /// <returns></returns>
    UFUNCTION(BlueprintCallable, Category = "Desktop duplication")
    bool Start();

    /// <summary>
    /// Releases all resource used for desktop duplication.
    /// </summary>
    UFUNCTION(BlueprintCallable, Category = "Desktop duplication")
    void Stop() noexcept;

private:

    /// <summary>
    /// Creates a new Direct3D 11 device.
    /// </summary>
    /// <returns></returns>
    static ID3D11Device *CreateDevice(void) noexcept;

    /// <summary>
    /// Searches the DXGI output for the specified display name.
    /// </summary>
    /// <param name="name"></param>
    /// <returns></returns>
    static IDXGIOutput1 *GetOutputForDisplayName(const FString& name) noexcept;

    /// <summary>
    /// Answer whether the given <paramref name="texture" /> has the given size.
    /// </summary>
    /// <param name="texture"></param>
    /// <param name="width"></param>
    /// <param name="height"></param>
    /// <returns></returns>
    static inline bool HasSize(const UTextureRenderTarget2D *target,
            const float width, const float height) noexcept {
        return (target != nullptr)
            && (target->GetSurfaceWidth() == width)
            && (target->GetSurfaceHeight() == height);
    }

    /// <summary>
    /// Answer whether the given <paramref name="target" /> texture has the same
    /// size as the given <paramref name="reference" /> texture.
    /// </summary>
    /// <param name="target"></param>
    /// <param name="reference"></param>
    /// <returns></returns>
    static bool HasSize(ID3D11Texture2D *target,
        ID3D11Texture2D *reference) noexcept;

    /// <summary>
    /// Makes sure that <see cref="Target"/> matches the size of the given
    /// texture.
    /// </summary>
    /// <param name="texture"></param>
    /// <returns></returns>
    bool MatchTarget(ID3D11Texture2D *texture) noexcept;

    /// <summary>
    /// Stages the given resource for copying to the <see cref="Target"/> and
    /// releases the resource.
    /// </summary>
    /// <param name="resource"></param>
    /// <returns><see langword="true" /> if the resource has been staged. If
    /// <see langword="false" /> is returned, it has been dropped.</returns>
    bool Stage(IDXGIResource *resource) noexcept;

    FThreadSafeBool _busy;
    ID3D11DeviceContext *_context;
    ID3D11Device *_device;
    IDXGIOutputDuplication *_duplication;
    ID3D11Fence *_fence;
    ID3D11Texture2D *_stagingTexture;
};

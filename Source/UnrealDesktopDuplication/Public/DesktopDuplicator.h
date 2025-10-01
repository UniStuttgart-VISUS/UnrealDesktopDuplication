// <copyright file="DesktopDuplicator.Build.cs" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2025 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE file for details.
// </copyright>
// <author>Christoph Müller</author>

#pragma once

#include "CoreMinimal.h"

#include "Engine/TextureRenderTarget2D.h"

#include "DesktopDuplicator.generated.h"


// Forward declarations
class IDXGIOutputDuplication;


DECLARE_LOG_CATEGORY_EXTERN(DesktopDuplicator, Log, All);


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
    /// The render target which receives the duplicated output..
    /// </summary>
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    UTextureRenderTarget2D *Target;

private:

    IDXGIOutputDuplication *_duplication;

};

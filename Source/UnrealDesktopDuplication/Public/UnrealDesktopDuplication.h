// <copyright file="FUnrealDesktopDuplicationModule.Build.cs" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2025 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE file for details.
// </copyright>
// <author>Christoph Müller</author>

#pragma once

#include "Modules/ModuleManager.h"


class FUnrealDesktopDuplicationModule : public IModuleInterface {

public:

    /// <inheritdoc />
    virtual void ShutdownModule(void) override;

    /// <inheritdoc />
    virtual void StartupModule(void) override;
};

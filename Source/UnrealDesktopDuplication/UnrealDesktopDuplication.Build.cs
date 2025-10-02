// <copyright file="UnrealDesktopDuplication.Build.cs" company="Visualisierungsinstitut der Universit�t Stuttgart">
// Copyright � 2025 Visualisierungsinstitut der Universit�t Stuttgart.
// Licensed under the MIT licence. See LICENCE file for details.
// </copyright>
// <author>Christoph M�ller</author>

using UnrealBuildTool;

public class UnrealDesktopDuplication : ModuleRules {

    public UnrealDesktopDuplication(ReadOnlyTargetRules target) : base(target) {
        this.PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        this.PublicIncludePaths.AddRange([]);
        this.PrivateIncludePaths.AddRange([]);
        this.PublicDependencyModuleNames.AddRange([ "Core" ]);
        this.PrivateDependencyModuleNames.AddRange([
            "CoreUObject",
            "D3D11RHI",
            "Engine",
            "RenderCore",
            "RHI",
            "Slate",
            "SlateCore",
        ]);
        this.DynamicallyLoadedModuleNames.AddRange([]);
    }
}

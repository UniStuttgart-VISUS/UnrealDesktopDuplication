// <copyright file="DesktopDuplicator.cpp" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2025 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE file for details.
// </copyright>
// <author>Christoph Müller</author>

#include "DesktopDuplicator.h"

#include "Windows/PreWindowsApi.h"
#include <Windows.h>
#include <dxgi1_2.h>
#include "Windows/PostWindowsApi.h"

#include "Runtime/RHI/Public/RHI.h"


/*
 * UDesktopDuplicator::UDesktopDuplicator
 */
UDesktopDuplicator::UDesktopDuplicator(void) : _duplication(nullptr) { }

/*
 * UDesktopDuplicator::UDesktopDuplicator
 */
UDesktopDuplicator(const FObjectInitializer& initialiser)
    : Super(initialiser), _duplication(nullptr) { }


/*
 * UDesktopDuplicator::~UDesktopDuplicator
 */
UDesktopDuplicator::~UDesktopDuplicator(void) noexcept {
    if (this->_duplication != nullptr) {
        this->_duplication->Release();
    }
}


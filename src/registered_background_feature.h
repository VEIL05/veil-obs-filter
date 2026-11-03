#pragma once
// Retained for a separate future Git commit; excluded from the product build.
// No runtime switch can enable reference capture/storage in the default build.
#ifndef VEIL_REGISTERED_BACKGROUND
#define VEIL_REGISTERED_BACKGROUND 0
#endif
namespace veil {inline constexpr bool registered_background_enabled=VEIL_REGISTERED_BACKGROUND!=0;}

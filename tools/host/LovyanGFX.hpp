// Host-only stand-in for the Arduino <M5GFX.h> include: pulls in just the
// LovyanGFX sprite code (from an M5GFX checkout) so ui.cpp can draw into
// memory on a desktop computer.
#pragma once
#include "lgfx/v1/platforms/device.hpp"
#include "lgfx/v1/platforms/common.hpp"
#include "lgfx/v1/lgfx_filesystem_support.hpp"
#include "lgfx/v1/LGFXBase.hpp"
#include "lgfx/v1/LGFX_Sprite.hpp"

namespace fonts {
using namespace lgfx::v1::fonts;
}

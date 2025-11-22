/***************************************************************************



***************************************************************************/
#pragma once

#include "WorldLibEnums.h"

// Force all maps to be PVP: Declare override for worldRegionGetType function
WorldRegionType OVERRIDE_LATELINK_worldRegionGetType(const struct WorldRegion *region);


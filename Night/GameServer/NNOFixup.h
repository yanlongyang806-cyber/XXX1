/***************************************************************************



***************************************************************************/
#pragma once

#include "WorldLibEnums.h"

// 强制全地图PVP：声明覆盖worldRegionGetType函数
WorldRegionType OVERRIDE_LATELINK_worldRegionGetType(const struct WorldRegion *region);


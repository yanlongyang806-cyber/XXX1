#pragma once

#include"GlobalTypeEnum.h"
#include"UGCCommon.h"
#include"UGCAchievements.h"

typedef struct Entity Entity;
typedef struct InfoForUGCProjectSaveOrPublish InfoForUGCProjectSaveOrPublish;
typedef struct ResourceCache ResourceCache;
typedef struct UGCProjectAutosaveData UGCProjectAutosaveData;
typedef struct UGCProjectData UGCProjectData;
typedef struct UGCProjectInfo UGCProjectInfo;
typedef struct UGCProjectVersion UGCProjectVersion;
typedef struct ZoneMapInfo ZoneMapInfo;
typedef struct UGCGenesisProceduralObjectParams UGCGenesisProceduralObjectParams;

/// gslUGC.c
void gslUGC_DoPlayDialogTree( Entity* pEntity, UGCProjectData* ugc_proj, U32 dialog_tree_id, int prompt_id );

/// gslUGCGenerate.c

void ugcGenesisProceduralObjectSetEventVolume(UGCGenesisProceduralObjectParams *params);
void ugcGenesisProceduralObjectSetActionVolume(UGCGenesisProceduralObjectParams *params);
void ugcGenesisProceduralObjectSetOptionalActionVolume(UGCGenesisProceduralObjectParams *params);

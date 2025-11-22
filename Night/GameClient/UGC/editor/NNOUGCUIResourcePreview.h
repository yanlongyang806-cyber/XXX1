//// A Widget that contains a resource preview.  It also automatically
//// does the expand on hover logic.
#pragma once

#include "ReferenceSystem.h"
#include "UILib.h"

typedef struct ResourceInfo ResourceInfo;
typedef struct WorldUGCProperties WorldUGCProperties;

typedef struct UGCUIResourcePreview
{
	UI_INHERIT_FROM( UI_WIDGET_TYPE );

	DictionaryHandleOrName dictName;
	const char* objectName;
	AtlasTex* defaultTex;
	const void* pExtraData;
} UGCUIResourcePreview;

SA_RET_NN_VALID UGCUIResourcePreview* ugcui_ResourcePreviewCreate(void);
void ugcui_ResourcePreviewFreeInternal(SA_PRE_NN_VALID SA_POST_NN_FREE UGCUIResourcePreview* preview);

void ugcui_ResourcePreviewSetResource( UGCUIResourcePreview* preview, DictionaryHandleOrName dictName, const char* objectName );
void ugcui_ResourcePreviewSetExtraData( UGCUIResourcePreview* preview, const void* pExtraData );
void ugcui_ResourcePreviewSetDefault( UGCUIResourcePreview* preview, const char* defaultTex );

void *ugcui_ResourcePreviewGetData( UGCUIResourcePreview* preview );
ResourceInfo *ugcui_ResourcePreviewGetResourceInfo( UGCUIResourcePreview* preview );
const WorldUGCProperties *ugcui_ResourcePreviewGetWorldUGCProperties( UGCUIResourcePreview* preview );

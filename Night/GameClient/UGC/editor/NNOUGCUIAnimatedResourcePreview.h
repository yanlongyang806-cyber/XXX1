//// A widget that shows an animated, interactive resource preview.
//// Used by the costume editor and the asset library.
#pragma once

#include "ReferenceSystem.h"
#include "UILib.h"

typedef struct PlayerCostume PlayerCostume;
typedef struct WLCostume WLCostume;

typedef struct UGCUIAnimatedResourcePreview
{
	UI_INHERIT_FROM( UI_WIDGET_TYPE );

	DictionaryHandleOrName dictName;
	const char* objectName;
	bool isYTranslate;

	bool bPreviewChanged;

	UISlider* pZoomSlider;

	// Costume data, because it needs to be translated to a WLCostume
	// to work and streamed down from the server to work.
	REF_TO(PlayerCostume) hPlayerCostume;
	REF_TO(WLCostume) hPreviewWLCostume;

	// Camera contrtol
	bool camMoving;
	float camHeight;
	float camDist;
	Vec3 camPyr;

	// Cached bounds data from costume or object
	float minDist;
	float maxDist;
	float minHeight;
	float maxHeight;
} UGCUIAnimatedResourcePreview;

SA_RET_NN_VALID UGCUIAnimatedResourcePreview* ugcui_AnimatedResourcePreviewCreate( void );
void ugcui_AnimatedResourcePreviewFreeInternal( SA_PRE_NN_VALID SA_POST_NN_FREE UGCUIAnimatedResourcePreview* preview );
void ugcui_AnimatedResourcePreviewSetResource( SA_PARAM_NN_VALID UGCUIAnimatedResourcePreview* preview, DictionaryHandleOrName dictName, const char* objectName, bool isYTranslate );
void ugcui_AnimatedResourcePreviewSetCostume( SA_PARAM_NN_VALID UGCUIAnimatedResourcePreview* preview, PlayerCostume* playerCostume, bool isYTranslate );
void ugcui_AnimatedResourcePreviewResetCamera( SA_PARAM_NN_VALID UGCUIAnimatedResourcePreview* preview );

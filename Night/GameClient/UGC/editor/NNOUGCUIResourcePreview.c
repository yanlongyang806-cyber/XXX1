#include "NNOUGCUIResourcePreview.h"

#include "CostumeCommon.h"
#include "NNOUGCCommon.h"
#include "NNOUGCResource.h"
#include "GfxPrimitive.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "GfxTexAtlas.h"
#include "ResourceInfo.h"
#include "ResourceManagerUI.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "gclResourceSnap.h"
#include "inputMouse.h"
#include "wlGroupPropertyStructs.h"
#include "wlUGC.h"

static void ugcui_ResourcePreviewTick( UGCUIResourcePreview *preview, UI_PARENT_ARGS );
static void ugcui_ResourcePreviewDraw( UGCUIResourcePreview *preview, UI_PARENT_ARGS );

#define RESOURCE_PREVIEW_HOVER_SIZE 256

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

UGCUIResourcePreview* ugcui_ResourcePreviewCreate( void )
{
	UGCUIResourcePreview* preview = calloc( 1, sizeof( *preview ));
	
	ui_WidgetInitialize( UI_WIDGET( preview ), ugcui_ResourcePreviewTick, ugcui_ResourcePreviewDraw, ugcui_ResourcePreviewFreeInternal, NULL, NULL );
	ui_WidgetSetDimensions( UI_WIDGET( preview ), 64, 64 );
	ugcui_ResourcePreviewSetDefault( preview, NULL );

	return preview;
}

void ugcui_ResourcePreviewTick( UGCUIResourcePreview *preview, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( preview );
	UI_TICK_EARLY( preview, true, false );
	UI_TICK_LATE( preview );
}

void ugcui_ResourcePreviewDraw( UGCUIResourcePreview *preview, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( preview );
	CBox imgBox;
	F32 imgZ;
	bool useRestrict;

	w = h = MIN(w,h);
	box.hx = x + w;
	box.hy = y + h;
	UI_DRAW_EARLY( preview );

	if( (UI_WIDGET( preview )->state & kWidgetModifier_Hovering) && w < RESOURCE_PREVIEW_HOVER_SIZE ) {
		BuildCBoxFromCenter(&imgBox, x + w / 2, y + h / 2, RESOURCE_PREVIEW_HOVER_SIZE, RESOURCE_PREVIEW_HOVER_SIZE);
		imgZ = UI_TOP_Z + 50;
		useRestrict = false;
	} else {
		imgBox = box;
		imgZ = z;
		useRestrict = true;
	}

	if( useRestrict ) {
		clipperPushRestrict( &imgBox );
	} else {
		clipperPush( &imgBox );
	}
	
	{
		ResourceInfo costumeInfo = { 0 };
		ResourceInfo *info = ugcui_ResourcePreviewGetResourceInfo(preview);
		void* pData = ugcui_ResourcePreviewGetData(preview);
		const WorldUGCProperties* ugcProperties = ugcui_ResourcePreviewGetWorldUGCProperties(preview);

		if(info && (stricmp(info->resourceDict, "UGCSound") == 0 || stricmp(info->resourceDict, "UGCSoundDSP") == 0))
		{
			// TODO: Need an audio preview texture to draw here
		}
		else
		{
			bool previewDrawn = false;
			if( ugcProperties && !nullStr( ugcProperties->pchImageOverride )) {
				previewDrawn = true;
				display_sprite_box( atlasLoadTexture( ugcProperties->pchImageOverride ), &imgBox, imgZ + 0.1, RGBAFromColor( ColorWhite ));
			} else if( info ) {
				previewDrawn = resDrawPreview( info, preview->pExtraData, imgBox.lx, imgBox.ly, imgBox.hx - imgBox.lx, imgBox.hy - imgBox.ly, 1, imgZ + 0.1, 255 );
			} else if( pData ) {
				previewDrawn = resDrawResource( preview->dictName, pData, preview->pExtraData, imgBox.lx, imgBox.ly, imgBox.hx - imgBox.lx, imgBox.hy - imgBox.ly, 1, imgZ + 0.1, 255 );
			}

			if( !previewDrawn ) {
				display_sprite_box( preview->defaultTex, &imgBox, imgZ, RGBAFromColor( ColorWhite ));
			} else {
				gfxDrawQuad( imgBox.lx, imgBox.ly, imgBox.hx, imgBox.hy, imgZ, ColorBlack );
				gfxfont_Printf( (imgBox.lx + imgBox.hx) / 2, (imgBox.ly + imgBox.hy) / 2, imgZ, 1, 1, CENTER_XY, "Loading");
			}
		}
	}
	clipperPop();

	UI_DRAW_LATE( preview );
}

void ugcui_ResourcePreviewFreeInternal( UGCUIResourcePreview* preview)
{
	SAFE_FREE( preview->objectName );
	ui_WidgetFreeInternal( UI_WIDGET( preview ));
}

void *ugcui_ResourcePreviewGetData( UGCUIResourcePreview* preview )
{
	void* pData = NULL;
	if( preview->dictName && !nullStr( preview->objectName ))
		pData = ugcEditorGetObject( preview->dictName, preview->objectName );
	return pData;
}

ResourceInfo *ugcui_ResourcePreviewGetResourceInfo( UGCUIResourcePreview* preview )
{
	ResourceInfo *info = NULL;
	if( preview->dictName && !nullStr( preview->objectName )) {
		void *pData = ugcEditorGetObject( preview->dictName, preview->objectName );
		if( !pData ) {
			if( stricmp( preview->dictName, "Trap" ) == 0 ) {
				int objId;
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				sscanf( preview->objectName, "%d,%s", &objId, buffer );
				info = ugcResourceGetInfoInt( "ObjectLibrary", objId );
			} else if( stricmp( preview->dictName, "UGCItem" ) == 0 ) {
				UGCItem* item = ugcEditorGetItemByName( preview->objectName );
				if( item )
					info = ugcResourceGetInfo( "Texture", item->strIcon );
			} else {
				info = ugcResourceGetInfo( preview->dictName, preview->objectName );
			}
		}
	}
	return info;
}

const WorldUGCProperties *ugcui_ResourcePreviewGetWorldUGCProperties( UGCUIResourcePreview* preview )
{
	const WorldUGCProperties* ugcProperties = NULL;
	if( preview->dictName && !nullStr( preview->objectName )) {
		void *pData = ugcEditorGetObject( preview->dictName, preview->objectName );
		if( !pData ) {
			if( stricmp( preview->dictName, "Trap" ) == 0 ) {
				int objId;
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				sscanf( preview->objectName, "%d,%s", &objId, buffer );
				ugcProperties = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", objId );
			} else if( stricmp( preview->dictName, "UGCItem" ) == 0 ) {
				UGCItem* item = ugcEditorGetItemByName( preview->objectName );
				if( item )
					ugcProperties = NULL;
			} else {
				ugcProperties = ugcResourceGetUGCProperties( preview->dictName, preview->objectName );
			}
		}
	}
	return ugcProperties;
}

void ugcui_ResourcePreviewSetResource( UGCUIResourcePreview* preview, DictionaryHandleOrName dictName, const char* objectName )
{
	if( dictName && !nullStr( objectName )) {
		preview->dictName = dictName;
		preview->objectName = strdup( objectName );
	} else {
		preview->dictName = NULL;
		SAFE_FREE( preview->objectName );
	}
}

void ugcui_ResourcePreviewSetExtraData( UGCUIResourcePreview* preview, const void* pExtraData )
{
	preview->pExtraData = pExtraData;
}

void ugcui_ResourcePreviewSetDefault( UGCUIResourcePreview* preview, const char* defaultTex )
{
	if( !defaultTex ) {
		preview->defaultTex = atlasLoadTexture( "CF_Icon_NoPreview" );
	} else {
		preview->defaultTex = atlasLoadTexture( defaultTex );
	}
}

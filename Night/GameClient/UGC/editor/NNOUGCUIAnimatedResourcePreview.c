#include "NNOUGCUIAnimatedResourcePreview.h"

#include "Color.h"
#include "CostumeCommon.h"
#include "CostumeCommonGenerate.h"
#include "GfxClipper.h"
#include "GfxHeadshot.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCResource.h"
#include "ObjectLibrary.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UICore.h"
#include "WorldGrid.h"
#include "dynSkeleton.h"
#include "inputMouse.h"
#include "soundLib.h"
#include "wlCostume.h"

#define CAM_DIST_MIN 2
#define CAM_DIST_MAX 9

#define SPACE_CAM_DIST_MIN 5
#define SPACE_CAM_DIST_MAX 65

static void ugcui_AnimatedResourcePreviewTick( UGCUIAnimatedResourcePreview *preview, UI_PARENT_ARGS );
static void ugcui_AnimatedResourcePreviewDraw( UGCUIAnimatedResourcePreview *preview, UI_PARENT_ARGS );
static void ugcui_AnimatedResourcePreviewCameraCB( UserData rawWidget, DynSkeleton *pSkel, GroupDef* pDef, Vec3 camPos, Vec3 camDir );
static void ugcui_AnimatedResourcePreviewSliderSetScale( UISlider* slider, bool finished, UserData rawWidget );

static BasicTexture* s_AnimatedPreviewTexture = NULL;
static const char* s_astrPlayingAudioEvent = NULL;

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

UGCUIAnimatedResourcePreview* ugcui_AnimatedResourcePreviewCreate( void )
{
	UGCUIAnimatedResourcePreview* preview = calloc( 1, sizeof( *preview ));
	
	ui_WidgetInitialize( UI_WIDGET( preview ), ugcui_AnimatedResourcePreviewTick, ugcui_AnimatedResourcePreviewDraw, ugcui_AnimatedResourcePreviewFreeInternal, NULL, NULL );
	ui_WidgetSetDimensions( UI_WIDGET( preview ), 64, 64 );

	preview->pZoomSlider = ui_SliderCreate( 20, 0, 100, 0, 1, 1 );
	ui_SliderSetPolicy( preview->pZoomSlider, UISliderContinuous );
	ui_SliderSetBias( preview->pZoomSlider, 2, 0 );
	ui_SliderSetChangedCallback( preview->pZoomSlider, ugcui_AnimatedResourcePreviewSliderSetScale, preview );
	ui_SliderSetValue( preview->pZoomSlider, 0.5 );
	ui_WidgetAddChild( UI_WIDGET( preview ), UI_WIDGET( preview->pZoomSlider ));

	return preview;
}

void ugcui_AnimatedResourcePreviewTick( UGCUIAnimatedResourcePreview *preview, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( preview );
	
	// Update the WLCostume if the PlayerCostume is active, streamed
	// down, and finished
	if( IS_HANDLE_ACTIVE( preview->hPlayerCostume ) && !IS_HANDLE_ACTIVE( preview->hPreviewWLCostume )) {
		PlayerCostume* playerCostume = GET_REF( preview->hPlayerCostume );
		if( playerCostume ) {
			WLCostume** eaSubCostumes = NULL;
			WLCostume* pWLCostume = costumeGenerate_CreateWLCostume( playerCostume, GET_REF( playerCostume->hSpecies ), NULL, NULL, NULL, NULL, NULL, NULL, NULL, "UGCUI.", 0, 0, false, &eaSubCostumes );

			if( pWLCostume && pWLCostume->bComplete ) {					
				FOR_EACH_IN_EARRAY( eaSubCostumes, WLCostume, pSubCostume ) {
					wlCostumePushSubCostume( pSubCostume, pWLCostume );
				} FOR_EACH_END;
				wlCostumeAddToDictionary( pWLCostume, pWLCostume->pcName );
				SET_HANDLE_FROM_REFERENT( "Costume", pWLCostume, preview->hPreviewWLCostume );
			} else {
				StructDestroy( parse_WLCostume, pWLCostume );
				eaDestroyStruct( &eaSubCostumes, parse_WLCostume );
			}
		}
	}

	// Children have to be ticked before the preview, so that the zoom
	// slider will work.
	ui_WidgetGroupTick( &UI_WIDGET( preview )->children, UI_MY_VALUES );

	{
		bool mouseButtonJustDown = mouseDownHit( MS_LEFT, &box ) || mouseDownHit( MS_MID, &box ) || mouseDownHit( MS_RIGHT, &box );
		bool mouseButtonDown = mouseIsDown( MS_LEFT ) || mouseIsDown( MS_MID ) || mouseIsDown( MS_RIGHT );
		if(  mouseButtonJustDown || (mouseIsLocked() && mouseButtonDown) ) {
			int dx, dy;
			mouseDiff( &dx, &dy );
			mouseLockThisFrame();

			preview->camMoving = true;
			preview->camPyr[ 1 ] += dx / 120.0;
			if( !preview->isYTranslate ) {
				preview->camPyr[ 0 ] = CLAMP( preview->camPyr[ 0 ] + dy / 120.0, -HALFPI * 0.9, HALFPI * 0.9 );
			} else {
				preview->camHeight = CLAMP( preview->camHeight + dy * 0.0025, 0, 1 );
			}
		} else {
			preview->camMoving = false;
		}
	}

	// This has to be after the mouse locking is handled so the widget
	// hover state is correctly set.
	UI_TICK_EARLY( preview, false, false );

	if( mouseScrollHit( &box )) {
		preview->camDist = CLAMP( preview->camDist - mouseZ() * 0.05, 0, 1 );
		ui_SliderSetValue( preview->pZoomSlider, 1 - preview->camDist );
	}
	
	UI_TICK_LATE( preview );
}

void ugcui_AnimatedResourcePreviewDraw( UGCUIAnimatedResourcePreview *preview, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( preview );
	CBox imgBox;
	F32 imgZ;

	UI_DRAW_EARLY( preview );
	
	imgBox = box;
	imgZ = z;
	clipperPushRestrict( &imgBox );

	if( !ugcAnimatedHeadshotThisFrame() ) {
		ugcSetAnimatedHeadshotThisFrame();
		
		{
			CBox drawBox;
			BuildCBoxFromCenter( &drawBox, (imgBox.lx + imgBox.hx) / 2, (imgBox.ly + imgBox.hy) / 2, CBoxHeight( &imgBox ), CBoxHeight( &imgBox ));
			display_sprite_box2( texFind( "white", false ), &box, imgZ, RGBAFromColor( ColorBlack ));
			if( s_AnimatedPreviewTexture ) {
				display_sprite_box2( s_AnimatedPreviewTexture, &drawBox, imgZ, RGBAFromColor( ColorWhite ));
			}
		}
		
		if( preview->bPreviewChanged || (!s_AnimatedPreviewTexture && !s_astrPlayingAudioEvent) ) {
			if ( s_AnimatedPreviewTexture ) {
				gfxHeadshotRelease( s_AnimatedPreviewTexture );
				s_AnimatedPreviewTexture = NULL;
			}
			if( s_astrPlayingAudioEvent ) {
				sndStopUIAudio( s_astrPlayingAudioEvent );
				sndMusicClear( true );
				s_astrPlayingAudioEvent = NULL;
			}
			
			if( stricmp( preview->dictName, "PlayerCostume" ) == 0 ) {
				static bool bfgSet = false;
				static DynBitFieldGroup bfg;

				if( !bfgSet ) {
					dynBitFieldGroupAddBits( &bfg, "", true );
				}

				if( GET_REF( preview->hPreviewWLCostume )) {
					s_AnimatedPreviewTexture = gfxHeadshotCaptureAnimatedCostumeScene( "UGCUIAnimatedResourcePreview", 1024, 1024, GET_REF( preview->hPreviewWLCostume ), NULL, NULL, ColorBlack, false, &bfg, NULL, NULL, ugcui_AnimatedResourcePreviewCameraCB, preview, 55, "UGC_Ground_CostumeEditor", NULL );
					preview->bPreviewChanged = false;
				}
			} else if( stricmp( preview->dictName, "ObjectLibrary" ) == 0 ) {
				GroupDef* def = objectLibraryGetGroupDefByName( preview->objectName, false );
				s_AnimatedPreviewTexture = gfxHeadshotCaptureAnimatedGroupScene( "UGCUIAnimatedResourcePreview", 1024, 1024, def, NULL, ColorBlack, NULL, "Ugc_Room_Preview", 0.1, false, true, ugcui_AnimatedResourcePreviewCameraCB, preview );
				preview->bPreviewChanged = false;
			} else if( stricmp( preview->dictName, "UGCSound" ) == 0 ) {
				UGCSound* sound = RefSystem_ReferentFromString( "UGCSound", preview->objectName );
				if( sound ) {
					sndPlayUIAudio( sound->strSoundName, NULL );
					s_astrPlayingAudioEvent = allocAddString( sound->strSoundName );
				}
				preview->bPreviewChanged = false;
			}
		}
	}

	// zoom slider
	{
		AtlasTex* zoomOutText = atlasLoadTexture( "UGC_Widgets_Slider_Zoom_Out_Idle" );
		AtlasTex* zoomInText = atlasLoadTexture( "UGC_Widgets_Slider_Zoom_In_Idle" );
		
		BuildCBox( &box, x + 4, y + 4, zoomOutText->width, zoomOutText->height );
		display_sprite_box( zoomOutText, &box, UI_GET_Z(), -1 );
		BuildCBox( &box, x + 122, y + 4, zoomInText->width, zoomInText->height );
		display_sprite_box( zoomInText, &box, UI_GET_Z(), -1 );
	}

	// Preview text
	{
		AtlasTex* iconTexture = atlasLoadTexture( "UGC_Icons_Preview_3D_Idle" );
		AtlasTex* iconTextureHover = atlasLoadTexture( "UGC_Icons_Preview_3D_Over" );
		UIStyleFont* font = RefSystem_ReferentFromString( "UIStyleFont", "UGC_Important_Preview" );
		UIStyleFont* fontHover = RefSystem_ReferentFromString( "UIStyleFont", "UGC_Important_Preview_Over" );
		float xPos = x + 4;
		float yPos = y + h - 4;

		if( ui_IsHovering( UI_WIDGET( preview )) || preview->camMoving ) {
			display_sprite( iconTextureHover, xPos, yPos - iconTextureHover->height, UI_GET_Z(), scale, scale, -1 );
			xPos += iconTextureHover->width;
			ui_StyleFontUse( fontHover, false, 0 );
		} else {
			display_sprite( iconTexture, xPos, yPos - iconTexture->height, UI_GET_Z(), scale, scale, -1 );
			xPos += iconTexture->width;
			ui_StyleFontUse( font, false, 0 );
		}

		gfxfont_Print( xPos, yPos, UI_GET_Z(), scale, scale, 0, "Preview" );
	}

	clipperPop();

	UI_DRAW_LATE( preview );
}

void ugcui_AnimatedResourcePreviewFreeInternal( UGCUIAnimatedResourcePreview* preview)
{
	SAFE_FREE( preview->objectName );
	REMOVE_HANDLE( preview->hPlayerCostume );
	REMOVE_HANDLE( preview->hPreviewWLCostume );
	gfxHeadshotRelease( s_AnimatedPreviewTexture );
	s_AnimatedPreviewTexture = NULL;
	if( s_astrPlayingAudioEvent ) {
		sndStopUIAudio( s_astrPlayingAudioEvent );
		sndMusicClear( true );
		s_astrPlayingAudioEvent = NULL;
	}

	ui_WidgetQueueFree( UI_WIDGET( preview->pZoomSlider ));
	ui_WidgetFreeInternal( UI_WIDGET( preview ));
}

void ugcui_AnimatedResourcePreviewSetResource( UGCUIAnimatedResourcePreview* preview, DictionaryHandleOrName dictName, const char* objectName, bool isYTranslate )
{
	preview->dictName = NULL;
	SAFE_FREE( preview->objectName );
	
	if( dictName && !nullStr( objectName )) {
		preview->dictName = dictName;
		preview->objectName = strdup( objectName );
		preview->isYTranslate = isYTranslate;

		REMOVE_HANDLE( preview->hPlayerCostume );
		REMOVE_HANDLE( preview->hPreviewWLCostume );
		if( stricmp( preview->dictName, "PlayerCostume" ) == 0 ) {
			SET_HANDLE_FROM_STRING( preview->dictName, preview->objectName, preview->hPlayerCostume );
		}

		preview->bPreviewChanged = true;
	}
}

void ugcui_AnimatedResourcePreviewSetCostume( SA_PARAM_NN_VALID UGCUIAnimatedResourcePreview* preview, PlayerCostume* playerCostume, bool isYTranslate )
{
	preview->dictName = allocAddString( "PlayerCostume" );
	SAFE_FREE( preview->objectName );
	preview->isYTranslate = isYTranslate;

	REMOVE_HANDLE( preview->hPlayerCostume );
	REMOVE_HANDLE( preview->hPreviewWLCostume );
	if( playerCostume ) {
		WLCostume** eaSubCostumes = NULL;
		WLCostume* pWLCostume = costumeGenerate_CreateWLCostume( playerCostume, GET_REF( playerCostume->hSpecies ), NULL, NULL, NULL, NULL, NULL, NULL, NULL, "UGCUI.", 0, 0, false, &eaSubCostumes );

		if( pWLCostume ) {
			// MJF: This widget may need to get more smarts to handle costume part streaming.
			assert( pWLCostume->bComplete );
					
			FOR_EACH_IN_EARRAY( eaSubCostumes, WLCostume, pSubCostume ) {
				wlCostumePushSubCostume( pSubCostume, pWLCostume );
			} FOR_EACH_END;
			wlCostumeAddToDictionary( pWLCostume, pWLCostume->pcName );
			SET_HANDLE_FROM_REFERENT( "Costume", pWLCostume, preview->hPreviewWLCostume );
		}
	}

	preview->bPreviewChanged = true;
}

void ugcui_AnimatedResourcePreviewResetCamera( SA_PARAM_NN_VALID UGCUIAnimatedResourcePreview* preview )
{
	preview->camDist = 1.0;
	ui_SliderSetValue( preview->pZoomSlider, preview->camDist );
	preview->camHeight = 0.7;
	if(!IS_HANDLE_ACTIVE(preview->hPlayerCostume) && !IS_HANDLE_ACTIVE(preview->hPreviewWLCostume))
		setVec3( preview->camPyr, RAD( 30 ), RAD( -30 ), RAD( 0 ) );
	else
		setVec3( preview->camPyr, RAD( 0 ), RAD( 0 ), RAD( 0 ) );

	preview->minDist = FLT_MAX;
	preview->maxDist = -FLT_MAX;
	preview->minHeight = FLT_MAX;
	preview->maxHeight = -FLT_MAX;
}

void ugcui_AnimatedResourcePreviewCameraCB( UserData rawWidget, DynSkeleton* skel, GroupDef* def, Vec3 camPos, Vec3 camDir )
{
	UGCUIAnimatedResourcePreview* preview = rawWidget;
	Vec3 pos;
	Vec3 dir;

	if(preview->minDist == FLT_MAX || preview->maxDist == -FLT_MAX || preview->minHeight == FLT_MAX || preview->maxHeight == -FLT_MAX)
	{
		if( skel ) {
			if( preview->isYTranslate ) {
				F32 radius1 = (skel->vCurrentGroupExtentsMax[0] - skel->vCurrentGroupExtentsMin[0]) / 2.0;
				F32 radius2 = (skel->vCurrentGroupExtentsMax[2] - skel->vCurrentGroupExtentsMin[2]) / 2.0;
				F32 radius = (radius1 + radius2) / 2.0;
				preview->minDist = MIN(preview->minDist, radius * 1);
				preview->maxDist = MAX(preview->maxDist, radius * 7);
				preview->minHeight = MIN(preview->minHeight, skel->vCurrentGroupExtentsMin[1]);
				preview->maxHeight = MAX(preview->maxHeight, skel->vCurrentGroupExtentsMax[1]);
			} else {
				preview->minDist = SPACE_CAM_DIST_MIN;
				preview->maxDist = SPACE_CAM_DIST_MAX;
				preview->minHeight = 0;
				preview->maxHeight = 0;
			}
		} else {
			preview->minDist = def->bounds.radius * 0.5;
			preview->maxDist = def->bounds.radius * 2;
			preview->minHeight = def->bounds.min[ 1 ];
			preview->maxHeight = def->bounds.max[ 1 ];
		}
	}

	if( !preview->isYTranslate ) {
		setVec3( pos, 0, (preview->minHeight + preview->maxHeight) / 2, lerp( preview->minDist, preview->maxDist, preview->camDist ));
	} else {
		setVec3( pos, 0, lerp( preview->minHeight, preview->maxHeight, preview->camHeight ),
				 lerp( preview->minDist, preview->maxDist, preview->camDist ));
	}
	setVec3( dir, 0, 0, -1 );

	// rotate by camPyr
	{
		Mat3 transform;
		createMat3YPR( transform, preview->camPyr );
		mulVecMat3( pos, transform, camPos );
		mulVecMat3( dir, transform, camDir );
	}
}

void ugcui_AnimatedResourcePreviewSliderSetScale( UISlider* slider, bool finished, UserData rawWidget )
{
	UGCUIAnimatedResourcePreview* preview = rawWidget;
	preview->camDist = 1 - ui_SliderGetValue( slider );
}

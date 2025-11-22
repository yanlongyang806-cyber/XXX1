#include"NNOUGCMapEditorWidgets.h"

#include"NNOUGCMapEditor.h"
#include"GfxClipper.h"
#include"GfxSprite.h"
#include"GfxTexAtlas.h"
#include"UIScrollbar.h"

void ugcLayoutDrawTranslateGrid( UGCMapEditorDoc* doc, F32 x, F32 y )
{
	UIScrollbar* sb = UI_WIDGET(doc->map_widget)->sb;
	AtlasTex* tex = atlasFindTexture( "CF_Grid_Marker" );
	F32 z = doc->frame_z + 999.9f;
	F32 translateSnapXZ = ugcMapEditorGetObjectTranslateSnap();
	Vec3 minViewableWorldPos;
	Vec3 maxViewableWorldPos;
	float xIt;
	float zIt;

	{
		const CBox* cbox = clipperGetCurrentCBox();
		Vec2 minUnscaledPos = { sb->xpos / doc->layout_scale, sb->ypos / doc->layout_scale };
		Vec2 maxUnscaledPos = { (sb->xpos + CBoxWidth( cbox )) / doc->layout_scale,
								(sb->ypos + CBoxHeight( cbox )) / doc->layout_scale };
		ugcLayoutGetWorldCoords( doc, minUnscaledPos, minViewableWorldPos );
		ugcLayoutGetWorldCoords( doc, maxUnscaledPos, maxViewableWorldPos );
	}

	minViewableWorldPos[ 0 ] = floor( minViewableWorldPos[ 0 ] / translateSnapXZ ) * translateSnapXZ;
	minViewableWorldPos[ 2 ] = ceil( minViewableWorldPos[ 2 ] / translateSnapXZ ) * translateSnapXZ;
	maxViewableWorldPos[ 0 ] = ceil( maxViewableWorldPos[ 0 ] / translateSnapXZ ) * translateSnapXZ;
	maxViewableWorldPos[ 2 ] = floor( maxViewableWorldPos[ 2 ] / translateSnapXZ ) * translateSnapXZ;

	// If this gets too fine, it's not useful anymore!
	{
		float pixelDelta = translateSnapXZ * doc->layout_grid_size / doc->layout_kit_spacing * doc->layout_scale;
		if( pixelDelta < tex->width * 4 || pixelDelta < tex->height * 4 ) {
			return;
		}
	}

	for( zIt = maxViewableWorldPos[ 2 ]; zIt <= minViewableWorldPos[ 2 ]; zIt += translateSnapXZ ) {
		for( xIt = minViewableWorldPos[ 0 ]; xIt <= maxViewableWorldPos[ 0 ]; xIt += translateSnapXZ ) {
			Vec3 worldPos = { xIt, 0, zIt };
			Vec2 uiPos;
			ugcLayoutGetUICoords( doc, worldPos, uiPos );
						
			display_sprite( tex,
							floor( (uiPos[0] * doc->layout_scale) + x - tex->width / 2 ),
							floor( (uiPos[1] * doc->layout_scale) + y - tex->height / 2 ), z,
							1, 1, -1 );
		}
	}
}

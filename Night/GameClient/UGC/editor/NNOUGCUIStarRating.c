#include "NNOUGCUIStarRating.h"

#include "GfxClipper.h"
#include "GfxSprite.h"
#include "GfxTexAtlas.h"
#include "inputMouse.h"

static void ugcui_StarRatingInitialize( UGCUIStarRating* rating );
static void ugcui_StarRatingTick( UGCUIStarRating* rating, UI_PARENT_ARGS );
static void ugcui_StarRatingDraw( UGCUIStarRating* rating, UI_PARENT_ARGS );

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

UGCUIStarRating* ugcui_StarRatingCreate( void )
{
	UGCUIStarRating* rating = calloc( 1, sizeof( *rating ));
	ugcui_StarRatingInitialize( rating );
	return rating;
}

void ugcui_StarRatingInitialize( UGCUIStarRating* rating )
{
	AtlasTex* starTex = atlasFindTexture( "UGC_Widgets_Star_Rating_Fill" );
	ui_WidgetInitialize( UI_WIDGET( rating ), ugcui_StarRatingTick, ugcui_StarRatingDraw, ugcui_StarRatingFreeInternal, NULL, NULL );
	ui_WidgetSetDimensions( UI_WIDGET( rating ), starTex->width, starTex->height );
}


void ugcui_StarRatingTick( UGCUIStarRating* rating, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( rating );
	UI_TICK_EARLY( rating, true, true );
	UI_TICK_LATE( rating );
}

void ugcui_StarRatingDraw( UGCUIStarRating* rating, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( rating );
	AtlasTex* starTex = atlasFindTexture( "UGC_Widgets_Star_Rating_Fill" );
	AtlasTex* emptyTex = atlasFindTexture( "UGC_Widgets_Star_Rating_Empty" );
	float quantizedValue = round( rating->value * 10 ) / 10.0f; 
	float xMax = interpF32( quantizedValue, box.lx, box.hx );
	
	UI_DRAW_EARLY( rating );
	display_sprite_box( emptyTex, &box, z, -1 );
	{
		CBox starBox = box;
		starBox.hx = xMax;
		clipperPushRestrict( &starBox );
		display_sprite_box( starTex, &box, z + 0.1, -1 );
		clipperPop();
	}
	UI_DRAW_LATE( rating );
}

void ugcui_StarRatingFreeInternal( UGCUIStarRating* rating )
{
	ui_WidgetFreeInternal( UI_WIDGET( rating ));
}

void ugcui_StarRatingSet( UGCUIStarRating* rating, float value )
{
	rating->value = value;
}

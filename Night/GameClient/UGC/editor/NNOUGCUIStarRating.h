//// A widget that displays a star rating, using the UGC styling.
//// 
//// This widget by default will set its size to 5 stars.  If you set it to a different size, the widget will stretch the stars.
#pragma once

#include "UILib.h"

typedef struct UGCUIStarRating {
	UI_INHERIT_FROM( UI_WIDGET_TYPE );

	float value;
} UGCUIStarRating;

SA_RET_NN_VALID UGCUIStarRating* ugcui_StarRatingCreate( void );
void ugcui_StarRatingFreeInternal( SA_PRE_NN_VALID SA_POST_NN_FREE UGCUIStarRating* rating );

void ugcui_StarRatingSet( SA_PRE_NN_VALID SA_POST_NN_FREE UGCUIStarRating* rating, float value );

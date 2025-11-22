#include"NNOUGCUITutorialNode.h"

#include"GfxClipper.h"
#include"StashTable.h"
#include"StringCache.h"
#include"earray.h"
#include"inputMouse.h"

static UGCUITutorialNode** s_eaNodes;

static void ugcui_TutorialNodeTick( UGCUITutorialNode* node, UI_PARENT_ARGS );
static void ugcui_TutorialNodeDraw( UGCUITutorialNode* node, UI_PARENT_ARGS );
static void ugcui_TutorialNodeFree( UGCUITutorialNode* node );
static UGCUITutorialNode* ugcui_TutorialNodeFind( const char* name );

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

void ugcui_TutorialNodeAssign( const char* nodeName, UIWidget* widget )
{
	UGCUITutorialNode* node = ugcui_TutorialNodeFind( nodeName );
	assert( widget );

	if( node ) {
		ui_WidgetRemoveFromGroup( UI_WIDGET( node ));
	} else {
		node = calloc( 1, sizeof( *node ));
		ui_WidgetInitialize( UI_WIDGET( node ), ugcui_TutorialNodeTick, ugcui_TutorialNodeDraw, ugcui_TutorialNodeFree, NULL, NULL );
		ui_WidgetSetPosition( UI_WIDGET( node ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( node ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		node->widget.uClickThrough = true;
		eaPush( &s_eaNodes, node );
	}

	node->nodeName = allocAddString( nodeName );
	ui_WidgetAddChild( widget, UI_WIDGET( node ));
}

void ugcui_TutorialNodeGetScreenBox( CBox* out_box, const char* nodeName )
{
	UGCUITutorialNode* node = ugcui_TutorialNodeFind( nodeName );
	CBoxSet( out_box, 0, 0, 0, 0 );
		
	if( node ) {
		*out_box = node->box;
	}
}

void ugcui_TutorialNodeTick( UGCUITutorialNode* node, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( node );
	node->box = pBox;
	
	UI_TICK_EARLY( node, false, false );
	UI_TICK_LATE( node );
}

void ugcui_TutorialNodeDraw( UGCUITutorialNode* node, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( node );
	UI_DRAW_EARLY( node );
	UI_DRAW_LATE( node );
}

void ugcui_TutorialNodeFree( UGCUITutorialNode* node )
{
	eaFindAndRemove( &s_eaNodes, node );
	ui_WidgetFreeInternal( UI_WIDGET( node ));
}

UGCUITutorialNode* ugcui_TutorialNodeFind( const char* name )
{
	int it;
	
	name = allocFindString( name );
	for( it = 0; it != eaSize( &s_eaNodes ); ++it ) {
		UGCUITutorialNode* node = s_eaNodes[ it ];
		if( node->nodeName == name ) {
			return node;
		}
	}

	return NULL;
}

//// A widget that keeps track of its last rectangle.  This exists so
//// we can know where in screen space every widget is for tutorial
//// nodes.
////
//// Expected usage:
////
//// When adding a node, call ugcui_TutorialNodeAssign as
//// ui_WidgetAddChild( parent, ugcui_TutorialNodeAssign( child, "ThisChild" ));
////
//// When rendering the tutorial, use ugcui_TutorialNodeGetScreenPos
//// to find its position on the screen.
#pragma once

#include"UICore.h"
#include"mathutil.h"
#include"CBox.h"

typedef struct UGCUITutorialNode {
	UI_INHERIT_FROM( UI_WIDGET_TYPE );
	const char* nodeName;			AST( POOL_STRING )
	CBox box;
} UGCUITutorialNode;

void ugcui_TutorialNodeAssign( const char* nodeName, UIWidget* widget );
void ugcui_TutorialNodeGetScreenBox( CBox* out_box, const char* nodeName );

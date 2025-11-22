#include "NNOUGCItemEditor.h"

#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UIPane.h"
#include "UITextureAssembly.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

static void ugcItemEditorDeleteItemCB( UIButton* ignored, UserData rawIndex );
static int ugcItemEditorSortItemByNameCB( const UGCItem** ppItem1, const UGCItem** ppItem2, const void* ignored );
static int ugcItemEditorSortItemByIconNameCB( const UGCItem** ppItem1, const UGCItem** ppItem2, const void* ignored );

UGCItemEditorDoc *ugcItemEditorCreate(void)
{
	UGCItemEditorDoc *pDoc = calloc(1, sizeof(UGCItemEditorDoc));

	pDoc->pRootPane = ui_PaneCreate(0, 0, 1.0f, 1.0f, UIUnitPercentage, UIUnitPercentage, 0);

	return pDoc;
}

void ugcItemEditorDestroy(UGCItemEditorDoc **ppDoc)
{
	if( !*ppDoc ) {
		return;
	}
	
	MEContextDestroyByName( "UGCItemEditor" );
	ui_WidgetQueueFree( UI_WIDGET( (*ppDoc)->pRootPane ));
	SAFE_FREE( *ppDoc );
}

void ugcItemEditorRefresh(UGCItemEditorDoc *pDoc)
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	MEFieldContext* uiCtx;
	UIScrollArea* scrollArea;
	UIPane* pane;
	UIPane* parentPane;
	MEFieldContextEntry* entry;
	UIWidget* widget;

	uiCtx = MEContextPush( "UGCItemEditor", NULL, NULL, NULL );
	uiCtx->pUIContainer = UI_WIDGET( pDoc->pRootPane );
	uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
	MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
	MEContextSetErrorIcon( "UGC_Icons_Labels_Alert", -1, -1 );
	uiCtx->iXPos = 0;
	uiCtx->iYPos = 0;
	uiCtx->iXLabelStart = 0;
	uiCtx->iYLabelStart = 0;
	uiCtx->bLabelPaddingFromData = false;
	uiCtx->iXDataStart = 20;
	uiCtx->iYDataStart = UGC_ROW_HEIGHT - 10;
	uiCtx->iYStep = UGC_ROW_HEIGHT * 2 - 10;
	MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
	setVec2( uiCtx->iErrorIconOffset, 0, UGC_ROW_HEIGHT - 10 + 3 );
	uiCtx->bErrorIconOffsetFromRight = false;
	uiCtx->iErrorIconSpaceWidth = 0;
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;

	// Add the toolbar
	pane = MEContextPushPaneParent( "Toolbar" );
	{
		float x = 10;
		float y = 6;
		
		entry = ugcMEContextAddEditorButton( UGC_ACTION_ITEM_CREATE, true, false );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		ui_WidgetSetPosition( widget, x, y );
		ui_WidgetSetHeight( widget, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( widget );

		entry = ugcMEContextAddEditorButton( UGC_ACTION_ITEM_SORT_BY_NAME, false, false );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		ui_WidgetSetPosition( widget, x, y );
		ui_WidgetSetHeight( widget, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( widget );

		entry = ugcMEContextAddEditorButton( UGC_ACTION_ITEM_SORT_BY_ICON, false, false );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		ui_WidgetSetPosition( widget, x, y );
		ui_WidgetSetHeight( widget, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( widget );
	}
	ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, UGC_PANE_TOP_BORDER, UIUnitPercentage, UIUnitFixed );
	MEContextPop( "Toolbar" );

	if( eaSize( &ugcProj->items ) == 0 ) {
		parentPane = MEContextPushPaneParent( "FTUEContent" );
		{
			entry = MEContextAddLabelMsg( "Text", "UGC_ItemEditor.FTUEAddItem", NULL );
			widget = UI_WIDGET( ENTRY_LABEL( entry ));
			ENTRY_LABEL( entry )->textFrom = UITop;
			ui_WidgetSetFont( widget, "UGC_Important_Alternate" );
			ui_WidgetSetPositionEx( widget, 0, -UGC_ROW_HEIGHT, 0, 0.5, UITop );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

			entry = MEContextAddButtonMsg( "UGC_ItemEditor.AddItem", "UGC_Icons_Labels_New_02", ugcEditorCreateNewItem, NULL, "Button", NULL, "UGC_ItemEditor.AddItem.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0.5, UITop );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ENTRY_BUTTON( entry )->bCenterImageAndText = true;
			widget->width = MAX( widget->width, 200 );
			widget->height = MAX( widget->height, 50 );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		}
		MEContextPop( "FTUEContent" );
		ui_PaneSetStyle( parentPane, "UGC_Story_BackgroundArea", true, false );
		ui_WidgetSetPosition( UI_WIDGET( parentPane ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( parentPane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( parentPane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );
	} else {
		parentPane = MEContextPushPaneParent( "Content" );
		scrollArea = MEContextPushScrollAreaParent( "ScrollArea" );
		{
			int itemX = 0;
			int itemY = MEContextGetCurrent()->iYPos;
			int it;
			for( it = 0; it != eaSize( &ugcProj->items ); ++it ) {
				UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_ContentArea" );
				UGCItem* item = ugcProj->items[ it ];
				int yMax = 0;
				MEContextPush( item->astrName, item, item, parse_UGCItem );
				MEContextSetErrorContext( ugcMakeTempErrorContextUGCItem( item->astrName ));
				pane = MEContextPushPaneParent( "Container" );
				ui_PaneSetStyle( pane, texas->pchName, true, false );
				pane->widget.uClickThrough = true;
				{
					entry = MEContextAddSprite( "white", "IconPreviewBG", NULL, NULL );
					widget = UI_WIDGET( ENTRY_SPRITE( entry ));
					ENTRY_SPRITE( entry )->tint = ColorBlack;
					ui_WidgetSetPosition( widget, 0, 0 );
					ui_WidgetSetDimensions( widget, 100, 100 );

					entry = MEContextAddSprite( ugcItemGetIconName( item ), "IconPreview", NULL, NULL );
					widget = UI_WIDGET( ENTRY_SPRITE( entry ));
					ui_WidgetSetPosition( widget, 0, 0 );
					ui_WidgetSetDimensions( widget, 100, 100 );
					ui_WidgetGroupSteal( widget->group, widget );

					entry = ugcMEContextAddResourcePickerMsg( "MissionItemIcon", "UGC_ItemEditor.MissionItemIcon", "UGC_ItemEditor.MissionItemIcon_PickerTitle", false, "Icon", NULL, "UGC_ItemEditor.MissionItemIcon.Tooltip" );
					widget = UI_WIDGET( ENTRY_SPRITE( entry ));
					ui_WidgetSetPosition( widget, 2, 100 - widget->height );
					ui_WidgetGroupSteal( widget->group, widget );
					widget = UI_WIDGET( ENTRY_BUTTON( entry ));
					ui_WidgetSetTextMessage( widget, "UGC_ItemEditor.MissionItemIcon" );
					ui_WidgetSetPosition( widget, 0, 101 );
					ui_WidgetSetWidth( widget, 100 );
					ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

					yMax = MAX( yMax, ui_WidgetGetNextY( widget ));

					MEContextGetCurrent()->iXPos = 110;
					MEContextGetCurrent()->iYPos = 5;

					entry = MEContextAddTextMsg( false, NULL, "DisplayName", "UGC_ItemEditor.DisplayName", "UGC_ItemEditor.DisplayName.Tooltip" );
					widget = ENTRY_FIELD( entry )->pUIWidget;

					entry = MEContextAddTextMsg( true, NULL, "Description", "UGC_ItemEditor.Description", "UGC_ItemEditor.Description.Tooltip" );
					widget = ENTRY_FIELD( entry )->pUIWidget;
					// Swap height and padding so that this fills the rest of the space
					ui_WidgetSetPaddingEx( widget, widget->leftPad, widget->rightPad, widget->y, 0 );
					widget->y = 0;
					ui_WidgetSetHeightEx( widget, 1, UIUnitPercentage );

				
					entry = MEContextAddButton( NULL, "ugc_icon_window_controls_close", ugcItemEditorDeleteItemCB, (UserData)it, "DeleteButton", NULL, NULL );
					widget = UI_WIDGET( ENTRY_BUTTON( entry ));
					SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
					ui_ButtonResize( ENTRY_BUTTON( entry ));
					ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
					ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
				}

				if( itemX == 0 ) {
					ui_WidgetSetPositionEx( UI_WIDGET( pane ), 5, itemY, 0.5, 0, UITopRight );
				} else {
					ui_WidgetSetPositionEx( UI_WIDGET( pane ), 5, itemY, 0.5, 0, UITopLeft );
				}
				ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 400, yMax + ui_TextureAssemblyHeight( texas ), UIUnitFixed, UIUnitFixed );

				++itemX;
				if( itemX >= 2 ) {
					itemX = 0;
					itemY = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
				}

				MEContextPop( "Container" );
				MEContextPop( item->astrName );
			}
		}
		MEContextPop( "ScrollArea" );
		MEContextPop( "Content" );
		scrollArea->autosize = true;
		ui_WidgetSetPosition( UI_WIDGET( scrollArea ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( scrollArea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( scrollArea ), 10, 10, 10, 10 );
		ui_PaneSetStyle( parentPane, "UGC_Story_BackgroundArea", true, false );
		ui_WidgetSetPosition( UI_WIDGET( parentPane ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( parentPane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( parentPane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );

		if( pDoc->numItemsLastRefresh < eaSize( &ugcProj->items )) {
			ui_ScrollAreaScrollToPosition( scrollArea, 0, 32000 );
		}
		pDoc->numItemsLastRefresh = eaSize( &ugcProj->items );
	}

	MEContextPop("UGCItemEditor");
}

void ugcItemEditorSetVisible(UGCItemEditorDoc *pDoc)
{
	ugcEditorSetDocPane(pDoc->pRootPane);
}

void ugcItemEditorOncePerFrame(UGCItemEditorDoc *pDoc)
{
}

bool ugcItemEditorQueryAction(UGCItemEditorDoc *pDoc, UGCActionID action, char** out_estr)
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	switch( action ) {
		xcase UGC_ACTION_ITEM_SORT_BY_NAME:
			return eaSize( &ugcProj->items ) > 0;
		xcase UGC_ACTION_ITEM_SORT_BY_ICON:
			return eaSize( &ugcProj->items ) > 0;
	}
	
	return false;
}

void ugcItemEditorHandleAction(UGCItemEditorDoc *pDoc, UGCActionID action)
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	switch( action ) {
		xcase UGC_ACTION_ITEM_SORT_BY_NAME:
			eaStableSort( ugcProj->items, NULL, ugcItemEditorSortItemByNameCB );
			ugcEditorQueueApplyUpdate();
		xcase UGC_ACTION_ITEM_SORT_BY_ICON:
			eaStableSort( ugcProj->items, NULL, ugcItemEditorSortItemByIconNameCB );
			ugcEditorQueueApplyUpdate();
	}
}

void ugcItemEditorDeleteItemCB( UIButton* ignored, UserData rawIndex )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	int index = (int)rawIndex;
	UGCItem* item = eaGet( &ugcProj->items, index );

	if( UIYes != ugcModalDialogMsg( "UGC_ItemEditor.DeleteItem_Title", "UGC_ItemEditor.DeleteItem_Details", UIYes | UINo )) {
		return;
	}

	if( item ) {
		StructDestroySafe( parse_UGCItem, &item );
		eaRemove( &ugcProj->items, index );
		ugcEditorQueueApplyUpdate();
	}
}

int ugcItemEditorSortItemByNameCB( const UGCItem** ppItem1, const UGCItem** ppItem2, const void* ignored )
{
	return stricmp( NULL_TO_EMPTY( (*ppItem1)->strDisplayName ), NULL_TO_EMPTY( (*ppItem2)->strDisplayName ));
}

int ugcItemEditorSortItemByIconNameCB( const UGCItem** ppItem1, const UGCItem** ppItem2, const void* ignored )
{
	const WorldUGCProperties* ugcProps1 = ugcResourceGetUGCProperties( "Texture", (*ppItem1)->strIcon );
	const WorldUGCProperties* ugcProps2 = ugcResourceGetUGCProperties( "Texture", (*ppItem2)->strIcon );
	const char* iconName1 = NULL;
	const char* iconName2 = NULL;

	if( ugcProps1 ) {
		iconName1 = TranslateDisplayMessage( ugcProps1->dVisibleName );
	}
	if( ugcProps2 ) {
		iconName2 = TranslateDisplayMessage( ugcProps2->dVisibleName );
	}

	return stricmp( NULL_TO_EMPTY( iconName1 ), NULL_TO_EMPTY( iconName2 ));
}

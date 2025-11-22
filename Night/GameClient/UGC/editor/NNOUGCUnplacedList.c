#include "NNOUGCUnplacedList.h"

#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditorWidgets.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "UIButton.h"
#include "UIList.h"
#include "UIPane.h"
#include "UISprite.h"
#include "UITextureAssembly.h"

typedef struct UGCUnplacedList
{
	UGCUnplacedListMode mode;
	UIList* widget;
	UGCComponent** eaModel;

	// details
	UIPane* detailsPane;
	UISprite* detailsSprite;

	// callbacks
	UserData userdata;
	UGCUnplacedListSelectFn pDragFn;
} UGCUnplacedList;

static void ugcUnplacedListCellDraw( UIList *pList, UIListColumn *pCol, UI_MY_ARGS, F32 z, CBox* pBox, int index, void* unused );
static void ugcUnplacedListTick( UIList* pList, UI_PARENT_ARGS );
static void ugcUnplacedListSelectCB( UIList* pList, UserData ignored );
static void ugcUnplacedListDragCB( UIList* pList, UGCUnplacedList* unplacedList );
static void ugcUnplacedListDetailsClose( UIButton* button, UGCUnplacedList* unplacedList );
static void ugcUnplacedListGoToObjectiveCB( UIButton* button, UGCMissionObjective* objective );

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

UGCUnplacedList* ugcUnplacedListCreate( UGCUnplacedListMode mode, UGCUnplacedListSelectFn pDragFn, UserData userdata )
{
	UGCUnplacedList* accum = calloc( 1, sizeof( *accum ));
	accum->mode = mode;
	accum->widget = ui_ListCreate( NULL, NULL, 1 );
	accum->pDragFn = pDragFn;
	accum->userdata = userdata;

	// Set up the widget's style
	accum->widget->bDrawSelection = false;
	ui_ListDestroyColumns( accum->widget );
	ui_ListAppendColumn( accum->widget, ui_ListColumnCreateCallback( "Name", ugcUnplacedListCellDraw, NULL ));
	ui_ListSetModel( accum->widget, parse_UGCComponent, &accum->eaModel );
	accum->widget->fHeaderHeight = 0;
	accum->widget->fRowHeight = UGC_UNPLACED_LIST_ROW_HEIGHT;
	UI_WIDGET( accum->widget )->tickF = ugcUnplacedListTick;
	ui_ListSetSelectedCallback( accum->widget, ugcUnplacedListSelectCB, accum );
	ui_WidgetSetDragCallback( UI_WIDGET( accum->widget ), ugcUnplacedListDragCB, accum );
	ui_ListSetActivatedCallback( accum->widget, NULL, NULL );
	ui_WidgetSetTextMessage( UI_WIDGET( accum->widget ), "UGC_MapEditor.NoUnplacedComponents" );

	return accum;
}

void ugcUnplacedListDestroy( UGCUnplacedList** ppList )
{
	if( !*ppList ) {
		return;
	}

	ui_WidgetQueueFree( UI_WIDGET( (*ppList)->widget ));
	eaDestroy( &(*ppList)->eaModel );
	SAFE_FREE( *ppList );
}

void ugcUnplacedListSetMap( UGCUnplacedList* list, const char* mapName )
{
	eaDestroy( &list->eaModel );
	list->eaModel = ugcComponentFindPlacements( ugcEditorGetComponentList(), mapName, GENESIS_UNPLACED_ID );
}

void ugcUnplacedListSetSelectedComponent( UGCUnplacedList* list, UGCComponent* component )
{
	ui_ListSetSelectedObject( list->widget, component );
	ui_ListCallSelectionChangedCallback( list->widget );
}

UIWidget* ugcUnplacedListGetUIWidget( UGCUnplacedList* list )
{
	return UI_WIDGET( list->widget );
}

int ugcUnplacedListGetContentHeight( UGCUnplacedList* list )
{
	return UGC_UNPLACED_LIST_ROW_HEIGHT * eaSize( &list->eaModel );
}

static void ugcUnplacedListCellDraw( UIList *pList, UIListColumn *pCol, UI_MY_ARGS, F32 z, CBox* pBox, int index, void* unused )
{
	char buffer[ 512 ];
	UITextureAssembly* normal = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Idle" );
	UITextureAssembly* hover = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Over" );
	UITextureAssembly* pressed = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Pressed" );
	UITextureAssembly* selected = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Selected" );
	UITextureAssembly* overlay = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Overlay" );
	UITextureAssembly* overlayIdle = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Overlay_Idle" );

	bool isSelected = ui_ListIsSelected( pList, NULL, index );
	bool isHovering = ui_ListIsHovering( pList, NULL, index );
	UGCComponent* component = eaGet( pList->peaModel, index );
	AtlasTex* componentTexture = atlasLoadTexture( g_ComponentIcons[ component->eType ]);
	CBox iconBox;
	CBox previewBox;
	CBox textBox;

	BuildCBox( &iconBox, x, y, 80, h );

	// Draw background
	if( isHovering && mouseIsDown( MS_LEFT )) {
		ui_TextureAssemblyDraw( pressed, &iconBox, &previewBox, scale, z, z + 0.1, 255, NULL );
		ui_SetCursorByName( "UGC_Cursors_Move_Pointer" );
	} else if( isSelected ) {
		ui_TextureAssemblyDraw( selected, &iconBox, &previewBox, scale, z, z + 0.1, 255, NULL );
	} else if( isHovering && !mouseIsDown( MS_LEFT )) {
		ui_TextureAssemblyDraw( hover, &iconBox, &previewBox, scale, z, z + 0.1, 255, NULL );
	} else {
		ui_TextureAssemblyDraw( normal, &iconBox, &previewBox, scale, z, z + 0.1, 255, NULL );
	}

	// preview
	{
		float texScale = CBoxWidth( &previewBox ) / componentTexture->width;
		display_sprite( componentTexture, previewBox.lx, previewBox.ly, z + 0.2, texScale, texScale, -1 );
	}

	ui_TextureAssemblyDraw( overlay, &iconBox, &textBox, scale, z + 0.3, z + 0.4, 255, NULL );
	if( (isHovering ? mouseIsDown( MS_LEFT ) : true) && !isSelected ) {
		ui_TextureAssemblyDraw( overlayIdle, &iconBox, NULL, scale, z + 0.3, z + 0.4, 255, NULL );
	}

	ui_StyleFontUse( RefSystem_ReferentFromString( "UIStyleFont", "UGC_Default_Alternate" ), false, 0 );
	{
		// Ensure no more than 2 lines of the name are diaplayed.
		F32 fLineHeight;
		F32 fLastLineWidth;
		S32 iLineCount;

		ugcComponentGetDisplayNameDefault( buffer, ugcEditorGetProjectData(), component, false );
		iLineCount = gfxfont_PrintWrappedEx( textBox.lx, 0, z + 0.5, CBoxWidth( &textBox ), 0, 0, 2, &fLastLineWidth, &fLineHeight, scale, scale, 0, false, buffer );
		if( iLineCount > 1 ) { // 2 lines needed, start name higher by 1 line
			gfxfont_PrintWrappedEx( textBox.lx, textBox.hy - fLineHeight, z + 0.5, CBoxWidth( &textBox ), 0, 0, 2, &fLastLineWidth, &fLineHeight, scale, scale, 0, true, buffer );
		} else { // only 1 line needed, place it at bottom
			gfxfont_PrintWrapped( textBox.lx, textBox.hy, z + 0.5, w - 4, scale, scale, 0, true, buffer );
		}
	}

	// Draw the text to the right
	{
		UGCMissionObjective* objective = ugcObjectiveFindComponentRelated( ugcEditorGetMission()->objectives, ugcEditorGetComponentList(), component->uID );
		float textX = iconBox.hx + 4;
		float textY = iconBox.ly;

		if( objective ) {
			UIStyleFont* font;
			font = RefSystem_ReferentFromString( "UIStyleFont", "UGC_Default" );
			ui_StyleFontUse( font, false, 0 );
			gfxfont_Printf( textX, textY + ui_StyleFontLineHeight( font, scale ), z, scale, scale, 0,  "%s",
							TranslateMessageKey( "UGC_MapEditor.UnplacedComponentsUsedBy" ));
			textY += ui_StyleFontLineHeight( font, scale );

			font = RefSystem_ReferentFromString( "UIStyleFont", "UGC_Header_Large" );
			ui_StyleFontUse( font, false, 0 );
			gfxfont_Printf( textX, textY + ui_StyleFontLineHeight( font, scale ), z, scale, scale, 0, "%s", ugcMissionObjectiveUIString( objective ));
			textY += ui_StyleFontLineHeight( font, scale );

			font = RefSystem_ReferentFromString( "UIStyleFont", "UGC_Default" );
			ui_StyleFontUse( font, false, 0 );
			ugcComponentGetDisplayName( buffer, ugcEditorGetProjectData(), component, false );
			gfxfont_Printf( textX, textY + ui_StyleFontLineHeight( font, scale ), z, scale, scale, 0, "%s", buffer );
			textY += ui_StyleFontLineHeight( font, scale );
		}
	}

	// Draw an error icon
	{
		AtlasTex* errorIcon = atlasFindTexture( "ugc_icons_labels_alert" );
		display_sprite( errorIcon, x + w - errorIcon->width, y, z, scale, scale, -1 );
	}
}

void ugcUnplacedListTick( UIList* list, UI_PARENT_ARGS )
{
	float startYPos = list->widget.sb->ypos;
	
	ui_ListTick( list, UI_PARENT_VALUES );
	if( ui_ListGetSelectedRow( list ) == -1 ) {
		return;
	}

	if( !ui_IsFocused( list )) {
		ui_ListClearSelected( list );
		ui_ListCallSelectionChangedCallback( list );
	}
	if( startYPos != list->widget.sb->ypos ) {
		ui_ListClearSelected( list );
		ui_ListCallSelectionChangedCallback( list );
	}
}

// This function is designed to look like ugcAssetLibraryPaneRefreshUI()
//
// It is intentionally copy/pasted because I expect the two to
// diverge.
void ugcUnplacedListSelectCB( UIList* pList, UGCUnplacedList* unplacedList )
{
	UGCComponent* selected = ui_ListGetSelectedObject( pList );
	UGCMissionObjective* objective = selected ? ugcObjectiveFindComponentRelated( ugcEditorGetMission()->objectives, ugcEditorGetComponentList(), selected->uID ) : NULL;
	UITextureAssembly* paneTexas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Popup_Window" );
	UILabel* label;
	UIPane* pane;
	UIPane* scrollAreaPane;
	UISMFView* smfView;
	UIButton* button;
	UIScrollArea* scrollArea;
	UISprite* sprite;
	UIWidget* widget;
	UIWidget* buttonWidget;
	float closeButtonPadding;
	float y;
	CBox rowBox;
	float paneContentWidth;
	float paneContentHeight;
	
	ui_WidgetQueueFreeAndNull( &unplacedList->detailsPane );
	ui_WidgetQueueFreeAndNull( &unplacedList->detailsSprite );

	if( !selected || !objective ) {
		return;
	}

	ui_ListGetSelectedRowBox( pList, &rowBox );
	{
		AtlasTex* spriteTex = atlasFindTexture( "UGC_Kits_Details_Arrow" );
		float paneWidth = 500;
		float paneHeight = 300;

		float selectedYCenter = CLAMP( (rowBox.ly + rowBox.hy) / 2, unplacedList->widget->lastDrawBox.ly, unplacedList->widget->lastDrawBox.hy );
		float paneY = MIN( selectedYCenter - paneHeight / 2,
						   unplacedList->widget->lastDrawBox.hy + spriteTex->height / 2 - paneHeight );;

		unplacedList->detailsSprite = ui_SpriteCreate( 0, 0, -1, -1, spriteTex->name );
		ui_WidgetSetPosition( UI_WIDGET( unplacedList->detailsSprite ),
							  unplacedList->widget->lastDrawBox.lx - spriteTex->width + 8,
							  selectedYCenter - spriteTex->height / 2 );
		ui_WidgetAddToDevice( UI_WIDGET( unplacedList->detailsSprite ), NULL );

		unplacedList->detailsPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		ui_PaneSetStyle( unplacedList->detailsPane, paneTexas->pchName, true, false );
		ui_WidgetSetPositionEx( UI_WIDGET( unplacedList->detailsPane ), unplacedList->detailsSprite->widget.x - paneWidth, paneY, 0, 0, UITopLeft );
		ui_WidgetSetDimensions( UI_WIDGET( unplacedList->detailsPane ), paneWidth, paneHeight );
		ui_WidgetAddToDevice( UI_WIDGET( unplacedList->detailsPane ), NULL );
		paneContentWidth = paneWidth - ui_TextureAssemblyWidth( paneTexas );
		paneContentHeight = paneHeight - ui_TextureAssemblyHeight( paneTexas );
	}

	sprite = ui_SpriteCreate( 0, 0, 1, 1, g_ComponentIcons[ selected->eType ]);
	sprite->bPreserveAspectRatioFill = true;
	widget = UI_WIDGET( sprite );
	ui_WidgetSetPosition( widget, 0, 0 );
	ui_WidgetSetDimensions( widget, paneContentHeight, paneContentHeight );
	ui_PaneAddChild( unplacedList->detailsPane, widget );

	y = 0;

	pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	widget = UI_WIDGET( pane );
	widget->name = "Test";
	ui_PaneSetStyle( pane, "UGC_Details_Popup_Pane", true, false );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, paneContentHeight, 0, 0, 0 );
	ui_PaneAddChild( unplacedList->detailsPane, widget );

	button = ui_ButtonCreateImageOnly( "ugc_icon_window_controls_close", 0, 0, ugcUnplacedListDetailsClose, unplacedList );
	widget = UI_WIDGET( button );
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
	ui_ButtonResize( button );
	ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
	ui_PaneAddChild( pane, widget );
	closeButtonPadding = ui_WidgetGetWidth( widget );
	buttonWidget = widget;

	{
		char buffer[ 256 ];
		ugcComponentGetDisplayNameDefault( buffer, ugcEditorGetProjectData(), selected, false );
		label = ui_LabelCreate( buffer, 0, 0 );
	}
	widget = UI_WIDGET( label );
	UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
	ui_LabelResize( label );
	ui_LabelSetWidthNoAutosize( label, 1, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 0, closeButtonPadding, 0, 0 );
	ui_PaneAddChild( pane, widget );
	y = ui_WidgetGetNextY( widget );

	// Make sure the close button is centered
	ui_WidgetSetHeight( buttonWidget, ui_WidgetGetHeight( widget ));

	if( unplacedList->mode == UGCUnplacedList_MapEditor ) {
		button = ui_ButtonCreate( "", 0, 0, NULL, NULL );
		widget = UI_WIDGET( button );
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Hyperlink", widget->hOverrideSkin );
		button->widget.userinfo = unplacedList;
		ui_ButtonSetText( button, ugcMissionObjectiveUIString( objective ));
		ui_ButtonSetCallback( button, ugcUnplacedListGoToObjectiveCB, objective );
		ui_ButtonResize( button );
		ui_WidgetSetPosition( widget, 0, y );
		ui_PaneAddChild( pane, widget );
		y = ui_WidgetGetNextY( widget );
	}
	y += 2;

	scrollAreaPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	widget = UI_WIDGET( scrollAreaPane );
	ui_PaneSetStyle( scrollAreaPane, "UGC_Pane_ContentArea", true, false );
	ui_WidgetSetPosition( widget, 0, y );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_PaneAddChild( pane, widget );

	scrollArea = ui_ScrollAreaCreate( 0, 0, 0, 0, 0, 0, false, true );
	widget = UI_WIDGET( scrollArea );
	ui_WidgetSetPosition( widget, 0, 0 );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_PaneAddChild( scrollAreaPane, widget );

	smfView = ui_SMFViewCreate( 0, 0, 0, 0 );
	widget = UI_WIDGET( smfView );
	ui_SMFViewSetText( smfView, TranslateMessageKey( "UGC_MapEditor.UnplacedComponentsUsedInStoryTask" ), NULL );
	ui_WidgetSetPosition( widget, 0, 0 );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitFixed );
	ui_ScrollAreaAddChild( scrollArea, widget );
}

void ugcUnplacedListDragCB( UIList* pList, UGCUnplacedList* unplacedList )
{
	UGCComponent* selected = ui_ListGetSelectedObject( pList );
	unplacedList->pDragFn( unplacedList, unplacedList->userdata, SAFE_MEMBER( selected, uID ));
}

void ugcUnplacedListDetailsClose( UIButton* button, UGCUnplacedList* unplacedList )
{
	ui_WidgetQueueFreeAndNull( &unplacedList->detailsPane );
	ui_WidgetQueueFreeAndNull( &unplacedList->detailsSprite );
	ui_ListClearSelected( unplacedList->widget );
}

void ugcUnplacedListGoToObjectiveCB( UIButton* button, UGCMissionObjective* objective )
{
	UGCUnplacedList* unplacedList = button->widget.userinfo;
	ui_WidgetQueueFreeAndNull( &unplacedList->detailsPane );
	ui_WidgetQueueFreeAndNull( &unplacedList->detailsSprite );
	ui_ListClearSelected( unplacedList->widget );
	ugcEditorEditMissionObjective( ugcMissionObjectiveLogicalNameTemp( objective ));
}

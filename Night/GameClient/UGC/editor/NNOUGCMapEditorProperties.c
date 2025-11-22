#include "NNOUGCMapEditorProperties.h"

#include "BlockEarray.h"
#include "Expression.h"
#include "GfxTexAtlas.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCDialogPromptPicker.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCInteriorCommon.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMapEditorWidgets.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCPlayingEditor.h"
#include "NNOUGCResource.h"
#include "NNOUGCZeniPicker.h"
#include "RegionRules.h"
#include "ResourceInfo.h"
#include "StateMachine.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UGCInteriorCommon.h"
#include "UITextureAssembly.h"

// MJF TODO: It would be nice to remove this dependancy.  UGC should not care about ObjectLibrary internals.
#include "ObjectLibrary.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

typedef struct UGCRoomDoorData
{
	UGCComponent* door;
	
	UGCComponent* room1;
	int room1DoorIdx;
	
	UGCComponent* room2;
	int room2DoorIdx;

	int* eaDoorTypeID;
} UGCRoomDoorData;

typedef struct UGCMapEditorPropertiesState {
	bool bBehaviorExpanded;
	bool bInteractExpanded;
} UGCMapEditorPropertiesState;

static void ugcMapEditorPropertiesRefreshToolbar( UGCMapEditorDoc* doc, UGCComponent* component );
static void ugcMapEditorPropertiesRefreshBasic( UGCComponent* component, bool isPlayingEditor );
static void ugcMapEditorPropertiesRefreshComponentPosition( UGCMap* map, UGCComponent* component, bool isPlayingEditor );
static void ugcMapEditorPropertiesRefreshLinks( UGCComponent* component );
static void ugcMapEditorPropertiesRefreshBehavior( UGCComponent* component, bool isPlayingEditor );
static void ugcMapEditorPropertiesRefreshStates( UGCComponent* component );
static void ugcMapEditorPropertiesRefreshRoomAdvanced( UGCComponent* component );
static void ugcMapEditorRefreshDialogTree( UGCComponent* component, bool isPlayingEditor );
static void ugcMapEditorRefreshInteract( UGCComponent* component );
static void ugcMapEditorPropertiesPlaceXYZEntry( MEFieldContextEntry* entry, MEFieldContext* uiCtx, int index );
static void ugcMapEditorRefreshExpanderButton( const char* uid, const char* strCollapse, const char* strExpand, bool *pState );
static void ugcRoomDoorDataReset( UGCRoomDoorData* data );
static bool ugcRoomDoorDataFilterCB( UGCRoomDoorData* doorData, UGCAssetLibraryRow* row );

static void ugcMapEditorPropertiesComponentPositionChangedCB( MEField* pField, bool bFinished, UGCComponentPlacementData* pPlacementData );
static void ugcMapEditorPropertiesComponentParentChangedCB( MEField* pField, bool bFinished, UGCComponentValidPositionUI* parent_ui );
static void ugcMapEditorPropertiesVolumeRadiusChangedCB( MEField* pField, bool bFinished, UGCMarkerVolumeRadiusData* pVolumeRadiusData );
static void ugcMapEditorPropertiesEncounterChangedCB( MEField* pField, bool bFinished, UGCComponent* component );
static void ugcComponentCustomizeCostumeCB( UIButton* ignored, UGCComponent* component );
static void ugcComponentAddPatrolPointCB( UIButton* ignored, UGCComponent* component );
static void ugcComponentAddPatrolPointBeforeCB( UIButton* ignored, UGCComponent* component );
static void ugcComponentAddPatrolPointAfterCB( UIButton* ignored, UGCComponent* component );
static void ugcComponentTrapPickerShowCB( UIButton* ignored, UGCComponent* component );
static void ugcComponentTrapPickerSetCB( UGCAssetLibraryPane* pane, UGCComponent* component, UGCAssetLibraryRow* row );
static void ugcComponentDefaultPromptToggledCB( UICheckButton *button, UGCComponent *component );
static void ugcComponentInteractToggledCB( UICheckButton *button, UGCComponent *component );
static void ugcComponentGoToCB( UIButton* ignored, UGCComponent* component );
static void ugcComponentGoToDialogTreeCB( UIButton* ignored, UGCComponent* component );
static void ugcObjectiveGoToCB( UIButton* ignored, UGCMissionObjective* objective );
static void ugcMapLinkGoToCB( UIButton* ignored, UGCMissionMapLink* mapLink );
static void ugcBoolClearCB( UIButton* ignored, bool* bToClear );
static void ugcBoolSetCB( UIButton* ignored, bool* bToSet );
static const char* ugcMapEditorPropertiesLengthUnit( UGCMap* map );

UGCMapEditorPropertiesState g_UGCMapEditorPropertiesState;
UGCComponent g_emptyComponent = { 0 };
UGCInteractProperties g_emptyInteractProps = { NULL, NULL, { NULL }, UGCDURATION_MEDIUM };

static void ugcMapEditorPinButtonCB( UIButton* ignored, UserData ignored2 )
{
	ugcMapEditorTogglePropertiesPaneIsDocked();
	ugcEditorQueueUIUpdate();
}

void ugcMapEditorPropertiesRefresh( UGCMapEditorDoc* doc )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	char strContextName[ 256 ];
	UGCComponent* component;

	sprintf( strContextName, "UGCMapEditor_%s_Properties", doc->doc_name );

	if( eaiSize( &doc->selected_components ) != 1 || doc->drag_state ) {
		MEContextDestroyByName( strContextName );
		ui_WidgetQueueFreeAndNull( &doc->properties_pane );
		ui_WidgetQueueFreeAndNull( &doc->properties_sprite );
		return;
	}

	component = ugcEditorFindComponentByID( doc->selected_components[ 0 ]);
	if( !doc->properties_pane ) {
		doc->properties_pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	// Intentionally don't set the position here... it is calculated in the once-per-frame
	ui_PaneSetStyle( doc->properties_pane, "UGC_Details_Popup_Window_Vertical", true, false );
	UI_WIDGET( doc->properties_pane )->priority = UI_HIGHEST_PRIORITY;
	ui_WidgetAddToDevice( UI_WIDGET( doc->properties_pane ), NULL );

	if( !doc->properties_sprite ) {
		doc->properties_sprite = ui_SpriteCreate( 0, 0, -1, -1, "white" );
	}
	// Intentionally don't set the position here.. it is calculated in the once-per-frame
	UI_WIDGET( doc->properties_sprite )->priority = UI_HIGHEST_PRIORITY;
	ui_WidgetAddToDevice( UI_WIDGET( doc->properties_sprite ), NULL );

	{
		MEFieldContext* uiCtx = MEContextPush( strContextName, component, component, parse_UGCComponent );
		UGCRuntimeErrorContext* errorCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL );
		UIScrollArea* scrollarea;

		uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
		uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
		uiCtx->bTextEntryTrimWhitespace = true;
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
			
		MEContextSetParent( UI_WIDGET( doc->properties_pane ));
		{
			char buffer[ 256 ];
			float buttonX;
			MEFieldContextEntry* entry;
			UIWidget* widget;
			UIWidget* buttonWidget;
			// UIWidget* buttonWidget2;

			// Explicitly make sure the header doesn't have any error
			// icons capturing input
			MEContextSetErrorFunction( NULL );

			buttonX = 0;
			entry = MEContextAddButton( NULL, "ugc_icon_window_controls_close", ugcMapEditorClearSelectionWidgetCB, doc, "CloseButton", NULL, NULL );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, buttonX, 0, 0, 0, UITopRight );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
			buttonX = ui_WidgetGetNextX( widget );
			buttonWidget = widget;

			// entry = MEContextAddButton( NULL,
			// 							(ugcMapEditorGetPropertiesPaneIsDocked() ? "UGC_Icon_Window_Controls_Unpin" : "UGC_Icon_Window_Controls_Pin"),
			// 							ugcMapEditorPinButtonCB, NULL, "PinButton", NULL, NULL );
			// widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			// SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
			// ui_ButtonResize( ENTRY_BUTTON( entry ));
			// ui_WidgetSetPositionEx( widget, buttonX, 0, 0, 0, UITopRight );
			// ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
			// buttonX = ui_WidgetGetNextX( widget );
			// buttonWidget2 = widget;

			ugcComponentGetDisplayNameDefault( buffer, ugcProj, component, false );
			entry = MEContextAddLabel( "Title", buffer, NULL );
			widget = UI_WIDGET( ENTRY_LABEL( entry ));
			UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
			ui_LabelResize( ENTRY_LABEL( entry ));
			ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
			ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 0, buttonX, 0, 0 );
			uiCtx->iYPos = ui_WidgetGetNextY( widget ) + 5;

			// Set the buttons' height to be the same as the text
			ui_WidgetSetHeight( buttonWidget, ui_WidgetGetHeight( widget ));
			// ui_WidgetSetHeight( buttonWidget2, ui_WidgetGetHeight( widget ));

			MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
			MEContextSetErrorContext( errorCtx );
			
			ugcMapEditorPropertiesRefreshToolbar( doc, component );

			scrollarea = MEContextPushScrollAreaParent( "ScrollArea" );
			ui_WidgetSetDimensionsEx( UI_WIDGET( scrollarea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			{
				ugcMapEditorPropertiesRefreshComponent( component, false );
			}
			MEContextPop( "ScrollArea" );
		}
		MEContextPop( strContextName );
		StructDestroySafe( parse_UGCRuntimeErrorContext, &errorCtx );
	}
}

void ugcMapEditorPropertiesOncePerFrame( UGCMapEditorDoc* doc )
{
	UGCComponent* component;
	CBox componentScreenBox;

	if( !doc->properties_pane || eaiSize( &doc->selected_components ) == 0 ) {
		return;
	}

	component = ugcEditorFindComponentByID( doc->selected_components[ 0 ]);
	{
		UGCRoomInfo* roomInfo = ugcRoomGetRoomInfo( component->iObjectLibraryId );
		Vec2 componentScreenPos;
		ugcLayoutGetUICoords( doc, component->sPlacement.vPos, componentScreenPos );
		componentScreenPos[ 0 ] = componentScreenPos[ 0 ] * doc->layout_scale + doc->backdrop_last_box.lx;
		componentScreenPos[ 1 ] = componentScreenPos[ 1 ] * doc->layout_scale + doc->backdrop_last_box.ly;

		if( roomInfo && roomInfo->footprint_buf ) {
			float drawScale = doc->layout_grid_size * doc->layout_scale / doc->layout_kit_spacing;
			CBoxSet( &componentScreenBox,
					 componentScreenPos[ 0 ] + roomInfo->footprint_min[ 0 ] * UGC_ROOM_GRID * drawScale,
					 componentScreenPos[ 1 ] - (roomInfo->footprint_max[ 1 ] + 1) * UGC_ROOM_GRID * drawScale,
					 componentScreenPos[ 0 ] + (roomInfo->footprint_max[ 0 ] + 1) * UGC_ROOM_GRID * drawScale,
					 componentScreenPos[ 1 ] - roomInfo->footprint_min[ 1 ] * UGC_ROOM_GRID * drawScale );
		} else {
			// So the arrow doesn't point exactly at the center of the sprite, add some padding
			componentScreenBox.lx = componentScreenPos[ 0 ] - 10;
			componentScreenBox.hx = componentScreenPos[ 0 ] + 10;
			componentScreenBox.ly = componentScreenPos[ 1 ] - 10;
			componentScreenBox.hy = componentScreenPos[ 1 ] + 10;
		}
	}

	// Place the pane on the left or right, based on where the
	// component is in the backdrop.
	ugcEditorPlacePropertiesWidgetsForBoxes( doc->properties_pane, doc->properties_sprite, &doc->layout_last_box, &componentScreenBox,
											 ugcMapEditorGetPropertiesPaneIsDocked(),
											 doc->map_widget->scrollToTargetRemaining > 0 );
}

void ugcMapEditorPropertiesRefreshToolbar( UGCMapEditorDoc* doc, UGCComponent* component )
{
	int y = MEContextGetCurrent()->iYPos;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIWidget** eaWidgets = NULL;
	
	entry = ugcMEContextAddEditorButton( UGC_ACTION_DUPLICATE, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, 0, y );
	ui_WidgetSetDimensions( widget, UGC_PANE_BUTTON_HEIGHT, UGC_PANE_BUTTON_HEIGHT );
	ui_WidgetSetPadding( widget, 0, 0 );
	eaPush( &eaWidgets, widget );
	
	entry = ugcMEContextAddEditorButton( UGC_ACTION_DELETE, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, 0, y );
	ui_WidgetSetDimensions( widget, UGC_PANE_BUTTON_HEIGHT, UGC_PANE_BUTTON_HEIGHT );
	ui_WidgetSetPadding( widget, 0, 0 );
	eaPush( &eaWidgets, widget );

	if( component->eType == UGC_COMPONENT_TYPE_ROOM ) {
		entry = ugcMEContextAddEditorButton( UGC_ACTION_ROOM_POPULATE, false, false );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, 0, y );
		ui_WidgetSetDimensions( widget, UGC_PANE_BUTTON_HEIGHT, UGC_PANE_BUTTON_HEIGHT );
		ui_WidgetSetPadding( widget, 0, 0 );
		eaPush( &eaWidgets, widget );
	}

	// Place the widgets
	{
		int it;
		for( it = 0; it != eaSize( &eaWidgets ); ++it ) {
			ui_WidgetSetPositionEx( eaWidgets[ it ], 0, eaWidgets[ it ]->y, (float)it / eaSize( &eaWidgets ), 0, UITopLeft );
			ui_WidgetSetWidthEx( eaWidgets[ it ], 1.f / eaSize( &eaWidgets ), UIUnitPercentage );
		}
	}

	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget ) + 5;
	eaDestroy( &eaWidgets );
}

void ugcMapEditorPropertiesRefreshComponent( UGCComponent* component, bool isPlayingEditor )
{
	ugcMapEditorPropertiesRefreshBasic( component, isPlayingEditor );
	ugcMapEditorPropertiesRefreshStates( component );
	ugcMapEditorPropertiesRefreshBehavior( component, isPlayingEditor );
	ugcMapEditorPropertiesRefreshRoomAdvanced( component );
	ugcMapEditorRefreshDialogTree( component, isPlayingEditor );
	ugcMapEditorRefreshInteract( component );
	if( !isPlayingEditor ) {
		ugcMapEditorPropertiesRefreshLinks( component );
	}
}

void ugcMapEditorPropertiesRefreshBasic( UGCComponent* component, bool isPlayingEditor )
{
	UGCMap* map = ugcEditorGetMapByName( component->sPlacement.pcMapName ); 
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	char buffer[ 256 ];
	MEFieldContextEntry* entry;
	UIPane* pane;
	UIWidget* widget;
	
	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MapEditor.Properties", true );

	// Name
	entry = MEContextAddTextMsg( false, NULL, "VisibleName", "UGC_MapEditor.Name", "UGC_MapEditor.Name.Tooltip" );
	ugcComponentGetDisplayNameDefault( buffer, ugcEditorGetProjectData(), component, true );
	if( !nullStr( buffer )) {
		ui_EditableSetDefaultString( ENTRY_FIELD( entry )->pUIEditable, buffer );
	} else {
		ui_EditableSetDefaultMessage( ENTRY_FIELD( entry )->pUIEditable, "UGC_MapEditor.DefaultName" );
	}

	if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		UGCComponent* killComponent = ugcEditorFindComponentByID( component->uParentID );
		const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", SAFE_MEMBER( killComponent, iObjectLibraryId ));
		const char* groupName = NULL;
		if( ugcProps ) {
			int index = eaiFind( &killComponent->uChildIDs, component->uID );
			if( index >= 0 ) {
				groupName = REF_STRING_FROM_HANDLE( ugcProps->groupDefProps.eaEncounterActors[ index ]->groupDisplayNameMsg.hMessage );
			}
		}			
		MEContextAddTextMsg( false, groupName, "ActorCritterGroupName", "UGC_MapEditor.ActorCritterGroupName", "UGC_MapEditor.ActorCritterGroupName.Tooltip" );
	}

	ugcMapEditorPropertiesRefreshComponentPosition( map, component, isPlayingEditor );

	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_CONTACT: case UGC_COMPONENT_TYPE_ACTOR:
			entry = ugcMEContextAddResourcePickerMsg( "Costume", "UGC_MapEditor.Costume_Default", "UGC_MapEditor.Costume_PickerTitle", false,
													  "CostumeName", "UGC_MapEditor.Costume", "UGC_MapEditor.Costume.Tooltip" );
			if( !nullStr( component->pcCostumeName ) && !isPlayingEditor ) {
				MEContextEntryAddActionButtonMsg( entry, "UGC_MapEditor.Customize", NULL, ugcComponentCustomizeCostumeCB, component, 80, "UGC_MapEditor.Customize.Tooltip" );
			}
			
		xcase UGC_COMPONENT_TYPE_KILL: {
			MEFieldContext* uiCtx =  MEContextPush( "Encounter", component, component, parse_UGCComponent );
			uiCtx->cbChanged = ugcMapEditorPropertiesEncounterChangedCB;
			uiCtx->pChangedData = component;
			
			ugcMEContextAddResourcePickerExMsg( "Encounter", NULL, NULL, "UGC_MapEditor.Encounter_Default", "UGC_MapEditor.Encounter_PickerTitle", false, "UGC_MapEditor.Encounter_PickerChoosingWillResetActors",
												"ObjectID", "UGC_MapEditor.Encounter", "UGC_MapEditor.Encounter.Tooltip" );
			if(ugcIsMissionItemsEnabled())
				ugcMEContextAddResourcePickerMsg("MissionItem", "UGC_MapEditor.DropItem_Default", "UGC_MapEditor.DropItem_PickerTitle", true,
					"DropItem", "UGC_MapEditor.DropItem", "UGC_MapEditor.DropItem.Tooltip" );

			MEContextPop( "Encounter" );
		}

		xcase UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
			ugcMEContextAddResourcePickerMsg( "Detail", "UGC_MapEditor.Object_Default", "UGC_MapEditor.Object_PickerTitle", false,
											  "ObjectID", "UGC_MapEditor.Object", "UGC_MapEditor.Object.Tooltip" );

		xcase UGC_COMPONENT_TYPE_SOUND:
			ugcMEContextAddResourcePickerMsg( "UGCSound", "UGC_MapEditor.Sound_Default", "UGC_MapEditor.Sound_PickerTitle", false,
											  "SoundEvent", "UGC_MapEditor.Sound", "UGC_MapEditor.Sound.Tooltip" );

		xcase UGC_COMPONENT_TYPE_ROOM_MARKER: {
			const char* lengthUnitsMessageKey = ugcMapEditorPropertiesLengthUnit( map );
			UGCMarkerVolumeRadiusData* pVolumeRadiusData;
			MEFieldContext* uiCtx;
			char* estr = NULL;

			pVolumeRadiusData = MEContextAllocStruct( "MarkerVolumeRadius", parse_UGCMarkerVolumeRadiusData, true );
			pVolumeRadiusData->pComponent = component;
			pVolumeRadiusData->fVolumeRadius = component->fVolumeRadius;

			uiCtx = MEContextPush( "UGCEditor_Marker_VolumeRadius", pVolumeRadiusData, pVolumeRadiusData, parse_UGCMarkerVolumeRadiusData );
			uiCtx->cbChanged = ugcMapEditorPropertiesVolumeRadiusChangedCB;
			uiCtx->pChangedData = pVolumeRadiusData;

			ugcFormatMessageKey( &estr, "UGC_MapEditor.Radius",
								 STRFMT_MESSAGEKEY( "Units", lengthUnitsMessageKey ),
								 STRFMT_END );
			MEContextAddSimple( kMEFieldType_TextEntry, "VolumeRadius", estr, TranslateMessageKey( "UGC_MapEditor.Radius.Tooltip" ));
	
			MEContextPop( "UGCEditor_Marker_VolumeRadius" );
			estrDestroy( &estr );
		}

		xcase UGC_COMPONENT_TYPE_ROOM:
			ugcMEContextAddResourcePickerMsg( "Room", "UGC_MapEditor.Room_Default", "UGC_MapEditor.Room_PickerTitle", false,
											  "ObjectID", "UGC_MapEditor.Room", "UGC_MapEditor.Room.Tooltip" );

		xcase UGC_COMPONENT_TYPE_ROOM_DOOR: case UGC_COMPONENT_TYPE_FAKE_DOOR: {
			UGCRoomDoorData* doorData = MEContextAllocMem( "RoomDoor", sizeof( *doorData ), ugcRoomDoorDataReset, true );
			UGCComponent* room = ugcEditorFindComponentByID( component->uParentID );
			if( room ) {
				UGCDoorSlotState state = ugcRoomGetDoorSlotState( ugcEditorGetComponentList(), room, component->iRoomDoorID,
																  NULL, &doorData->eaDoorTypeID,
																  &doorData->room2, &doorData->room2DoorIdx );
				doorData->door = component;
				doorData->room1 = ugcEditorFindComponentByID( component->uParentID );
				doorData->room1DoorIdx = component->iRoomDoorID;
	
				assert(   (state == UGC_DOOR_SLOT_OCCUPIED || state == UGC_DOOR_SLOT_OCCUPIED_MULTIPLE)
						  && room && component->iRoomDoorID >= 0 );
				if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ) {
					assert( doorData->room2 && doorData->room2DoorIdx >= 0 );
				} else {
					assert( !doorData->room2 && doorData->room2DoorIdx == 0 );
				}
	
				ugcMEContextAddResourcePickerExMsg( "RoomDoor", ugcRoomDoorDataFilterCB, doorData, "UGC_MapEditor.RoomDoor_Default", "UGC_MapEditor.RoomDoor_PickerTitle",
													component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR,
													NULL,
													"ObjectID", "UGC_MapEditor.RoomDoor", "UGC_MapEditor.RoomDoor.Tooltip" );
			} else {
				ugcMEContextAddResourcePickerMsg( "RoomDoor", "UGC_MapEditor.RoomDoor_Default", "UGC_MapEditor.RoomDoor_PickerTitle",
												  component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR,
												  "ObjectID", "UGC_MapEditor.RoomDoor", "UGC_MapEditor.RoomDoor.Tooltip" );
			}
		}

		xcase UGC_COMPONENT_TYPE_TRAP:
			entry = MEContextAddButton( ugcTrapGetDisplayName( component->iObjectLibraryId, component->pcTrapPower, TranslateMessageKey( "UGC_MapEditor.Trap_Default" )),
										NULL, ugcComponentTrapPickerShowCB, component,
										"TrapName",
										TranslateMessageKey( "UGC_MapEditor.Trap" ),
										TranslateMessageKey( "UGC_MapEditor.Trap.Tooltip" ));
			MEContextSetEntryErrorForField( entry, "" );

		xcase UGC_COMPONENT_TYPE_PATROL_POINT:
			entry = MEContextAddButtonMsg( "UGC_MapEditor.Patrol_AddPointBefore", NULL, ugcComponentAddPatrolPointBeforeCB, component, "PatrolAddBefore", "UGC_MapEditor.Patrol_AddPoint", "UGC_MapEditor.Patrol_AddPoint.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			widget->u64 = isPlayingEditor;
			entry = MEContextAddButtonMsg( "UGC_MapEditor.Patrol_AddPointAfter", NULL, ugcComponentAddPatrolPointAfterCB, component, "PatrolAddAfter", NULL, NULL );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			widget->u64 = isPlayingEditor;

		xcase UGC_COMPONENT_TYPE_COMBAT_JOB:
			ugcMEContextAddResourcePickerMsg( "CombatJob", "UGC_MapEditor.CombatJob_Default", "UGC_MapEditor.CombatJob_PickerTitle", false,
											  "ObjectID", "UGC_MapEditor.CombatJob", "UGC_MapEditor.CombatJob.Tooltip" );
	}

	//
	if( component->eType == UGC_COMPONENT_TYPE_OBJECT || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR ) {
		ugcMEContextAddBooleanMsg( "InteractIsMissionReturn", "UGC_MapEditor.AbortExit", "UGC_MapEditor.AbortExit.Tooltip" );
	}


	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
}

static int ugcMapEditorPropertiesSortValidPositions( const UGCComponentValidPosition **a, const UGCComponentValidPosition **b )
{
	if ((*a)->platform_height > (*b)->platform_height)
		return -1;
	if ((*a)->platform_height < (*b)->platform_height)
		return 1;

	return 0;
}

void ugcMapEditorPropertiesRefreshComponentPosition( UGCMap* map, UGCComponent* component, bool isPlayingEditor )
{
	const char* lengthUnitsMessageKey = ugcMapEditorPropertiesLengthUnit( map );
	UGCMapType mapType = ugcMapGetType( map );
	UGCComponent* roomParent = ugcComponentGetRoomParent( ugcEditorGetComponentList(), component );

	UGCComponentPlacementData* pPlacementData;
	MEFieldContext* uiCtx;
	MEFieldContextEntry* entry;
	UIComboBox* combo;
	char* estr = NULL;
		
	// Set up the placement data for the context
	pPlacementData = MEContextAllocStruct( __FUNCTION__, parse_UGCComponentPlacementData, true );
	pPlacementData->pPrimaryComponent = component;
	pPlacementData->isPlayingEditor = isPlayingEditor;

	if(   !isPlayingEditor
		  || component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ABSOLUTE
		  || component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE ) {
		copyVec3( component->sPlacement.vPos, pPlacementData->vPrimaryOldPos );
	} else {
		Mat4 mat4;
		Vec3 spawnPos = { 0 };
		if( map->pPrefab ) {
			ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, spawnPos, NULL );
		}
			
		ugcPlayingEditorComponentMat4( component, mat4 );
		copyVec3( mat4[ 3 ], pPlacementData->vPrimaryOldPos );
		pPlacementData->vPrimaryOldPos[ 1 ] -= spawnPos[ 1 ];
	}
	copyVec3( component->sPlacement.vRotPYR, pPlacementData->vPrimaryOldRot );
	pPlacementData->fXPos = pPlacementData->vPrimaryOldPos[ 0 ];
	pPlacementData->fYPos = pPlacementData->vPrimaryOldPos[ 1 ];
	pPlacementData->fZPos = pPlacementData->vPrimaryOldPos[ 2 ];
	pPlacementData->fRotPitch = pPlacementData->vPrimaryOldRot[0];
	pPlacementData->fRotYaw = pPlacementData->vPrimaryOldRot[1];
	pPlacementData->fRotRoll = pPlacementData->vPrimaryOldRot[2];

	uiCtx = MEContextPush( __FUNCTION__, pPlacementData, pPlacementData, parse_UGCComponentPlacementData );
	uiCtx->cbChanged = ugcMapEditorPropertiesComponentPositionChangedCB;
	uiCtx->pChangedData = pPlacementData;

	ugcFormatMessageKey( &estr, "UGC_MapEditor.Position",
						 STRFMT_MESSAGEKEY( "Units", lengthUnitsMessageKey ),
						 STRFMT_END );
	entry = MEContextAddLabel( "PosLabel", estr, TranslateMessageKey( "UGC_MapEditor.Position.Tooltip" ));
	MEContextStepBackUp();
	
	entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "XPos", "UGC_MapEditor.XAxis", "UGC_MapEditor.XAxis.Tooltip" );
	MEContextStepBackUp();
	ugcMapEditorPropertiesPlaceXYZEntry( entry, uiCtx, 0 );
	
	entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "YPos", "UGC_MapEditor.YAxis", "UGC_MapEditor.YAxis.Tooltip" );
	MEContextStepBackUp();
	ugcMapEditorPropertiesPlaceXYZEntry( entry, uiCtx, 1 );

	entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "ZPos", "UGC_MapEditor.ZAxis", "UGC_MapEditor.ZAxis.Tooltip" );
	MEContextStepBackUp();
	ugcMapEditorPropertiesPlaceXYZEntry( entry, uiCtx, 2 );
	MEContextStepDown();

	if( !isPlayingEditor ) {
		if( mapType == UGC_MAP_TYPE_PREFAB_SPACE || mapType == UGC_MAP_TYPE_SPACE ) {
			MEContextAddTwoLabelsMsg( "RelPosition_Label", "UGC_MapEditor.YRelativeTo", "UGC_MapEditor.YRelativeTo_ZeroAltitude", "UGC_MapEditor.YRelativeTo_Space.Tooltip" );
		} else if( mapType == UGC_MAP_TYPE_INTERIOR && (!roomParent || roomParent == component) ) {
			MEContextAddTwoLabelsMsg( "RelPosition_Label", "UGC_MapEditor.YRelativeTo", "UGC_MapEditor.YRelativeTo_ZeroAltitude", "UGC_MapEditor.YRelativeTo_NoParent.Tooltip" );
		} else if( mapType == UGC_MAP_TYPE_PREFAB_GROUND ) {
			MEContextPush( "UGCEditor_Component_HeightSnap", &component->sPlacement, &component->sPlacement, parse_UGCComponentPlacement );
			entry = MEContextAddEnumMsg( kMEFieldType_Combo, UGCComponentHeightSnapEnum, "Snap", "UGC_MapEditor.YRelativeTo", "UGC_MapEditor.YRelativeTo.Tooltip" );
			combo = ENTRY_FIELD( entry )->pUICombo;
			combo->bUseMessage = true;
			ui_ComboBoxEnumRemoveAllValues( combo );
			ui_ComboBoxEnumAddValue( combo, "UGC_MapEditor.YRelativeTo_Terrain", COMPONENT_HEIGHT_SNAP_TERRAIN );
			ui_ComboBoxEnumAddValue( combo, "UGC_MapEditor.YRelativeTo_Geometry", COMPONENT_HEIGHT_SNAP_WORLDGEO );
			ui_ComboBoxEnumAddValue( combo, "UGC_MapEditor.YRelativeTo_ZeroAltitude", COMPONENT_HEIGHT_SNAP_ABSOLUTE );

			if( ugcComponentSupportsNormalSnapping( component )) {
				bool active = ugcComponentPlacementNormalSnappingActive( &component->sPlacement );
				entry = ugcMEContextAddBooleanMsg( "SnapNormal", "UGC_MapEditor.SnapNormal", "UGC_MapEditor.SnapNormal.Tooltip" );
				ui_SetActive( UI_WIDGET( entry->pRadioButton1 ), active );
				ui_SetActive( UI_WIDGET( entry->pRadioButton2 ), active );
				ui_SetActive( UI_WIDGET( ENTRY_LABEL( entry )), active );
			}

			MEContextPop( "UGCEditor_Component_HeightSnap" );
		} else if( mapType == UGC_MAP_TYPE_PREFAB_INTERIOR ) {
			MEContextPush( "UGCEditor_Component_HeightSnap", &component->sPlacement, &component->sPlacement, parse_UGCComponentPlacement );
			entry = MEContextAddEnumMsg( kMEFieldType_Combo, UGCComponentHeightSnapEnum, "Snap", "UGC_MapEditor.YRelativeTo", "UGC_MapEditor.YRelativeTo.Tooltip" );
			combo = ENTRY_FIELD( entry )->pUICombo;
			ui_ComboBoxEnumRemoveAllValues( combo );
			ui_ComboBoxEnumAddValue( combo, "UGC_MapEditor.YRelativeTo_RoomPlatform", COMPONENT_HEIGHT_SNAP_TERRAIN );
			ui_ComboBoxEnumAddValue( combo, "UGC_MapEditor.YRelativeTo_ZeroAltitude", COMPONENT_HEIGHT_SNAP_ABSOLUTE );
			MEContextPop( "UGCEditor_Component_HeightSnap" );
		} else if( mapType == UGC_MAP_TYPE_INTERIOR ) {
			UGCComponentValidPositionUI* parent_ui = MEContextAllocStruct( "ValidPositions", parse_UGCComponentValidPositionUI, true );
			MEFieldContext* pSubContext = MEContextPush( "UGCEditor_Component_Parent", parent_ui, parent_ui, parse_UGCComponentValidPositionUI );
			pSubContext->cbChanged = ugcMapEditorPropertiesComponentParentChangedCB;
			pSubContext->pChangedData = parent_ui;

			parent_ui->component = component;
			parent_ui->results = ugcComponentFindValidPositions( ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), component, component->sPlacement.vPos );
			eaQSort( parent_ui->results, ugcMapEditorPropertiesSortValidPositions );

			parent_ui->selected = component->sPlacement.eSnap;
			if( component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_LEGACY || component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ROOM_PARENTED ) {
				FOR_EACH_IN_EARRAY_FORWARDS( parent_ui->results, UGCComponentValidPosition, result ) {
					if(   result->room_id == component->uParentID
						  && result->room_level == component->sPlacement.iRoomLevel ) {
						parent_ui->selected = 1000 + eaSize( &parent_ui->results ) - FOR_EACH_IDX( parent_ui->results, result );
					}
				} FOR_EACH_END;
			}

			entry = MEContextAddEnum( kMEFieldType_Combo, UGCComponentHeightSnapEnum, "Selected", TranslateMessageKey( "UGC_MapEditor.YRelativeTo"), TranslateMessageKey( "UGC_MapEditor.YRelativeTo.Tooltip" ));
			combo = ENTRY_FIELD( entry )->pUICombo;
			combo->bUseMessage = true;
			ui_ComboBoxEnumRemoveAllValues( combo );
			ui_ComboBoxEnumAddValue( combo, TranslateMessageKey( "UGC_MapEditor.YRelativeTo_ZeroAltitude"), COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE );

			FOR_EACH_IN_EARRAY_FORWARDS( parent_ui->results, UGCComponentValidPosition, result ) {
				if( result->room_level > -1 ) {
					UGCComponent* pParent = ugcEditorFindComponentByID( result->room_id );
					if( pParent ) {
						char* label;

						if( pParent->eType == UGC_COMPONENT_TYPE_ROOM ) {
							char floatBuf[ 256 ];
							sprintf( floatBuf, "%0.02f", result->platform_height );
							ugcFormatMessageKey( &estr, "UGC_MapEditor.YRelativeTo_RoomPlatformIndexed",
												 STRFMT_INT( "Level", result->room_level + 1 ),
												 STRFMT_STRING( "Height", floatBuf ),
												 STRFMT_END );
						} else {
							char parentName[ 256 ];
							char floatBuf[ 256 ];
							ugcComponentGetDisplayName( parentName, ugcEditorGetProjectData(), pParent, false );
							sprintf( floatBuf, "%0.02f", result->platform_height );
							ugcFormatMessageKey( &estr, "UGC_MapEditor.YRelativeTo_ObjectPlatform",
												 STRFMT_STRING( "ObjectName", parentName ),
												 STRFMT_STRING( "Height", floatBuf ),
												 STRFMT_END );
						}

						label = StructAllocString( estr );
						eaPush( &parent_ui->labels, label );
						ui_ComboBoxEnumAddValue( combo, label, 1000 + eaSize( &parent_ui->results ) - FOR_EACH_IDX( parent_ui->results, result ));
					}
				}
			} FOR_EACH_END;
			ui_SetActive( UI_WIDGET( combo ), eaSize( combo->model ) > 1 );

			MEContextPop( "UGCEditor_Component_Parent" );
		}
	}

	if( ugcLayoutComponentCanRotate( component->eType )) {
		if( ugcComponentAllow3DRotation( component->eType )) {
			entry = MEContextAddLabelMsg( "RotLabel", "UGC_MapEditor.Rotation3D", "UGC_MapEditor.Rotation3D.Tooltip" );
			MEContextStepBackUp();

			entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "RotPitch", "UGC_MapEditor.RotationPitch", "UGC_MapEditor.RotationPitch.Tooltip" );
			MEContextStepBackUp();
			ugcMapEditorPropertiesPlaceXYZEntry( entry, uiCtx, 0 );
	
			entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "RotYaw", "UGC_MapEditor.RotationYaw", "UGC_MapEditor.RotationYaw.Tooltip" );
			MEContextStepBackUp();
			ugcMapEditorPropertiesPlaceXYZEntry( entry, uiCtx, 1 );

			entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "RotRoll", "UGC_MapEditor.RotationRoll", "UGC_MapEditor.RotationRoll.Tooltip" );
			MEContextStepBackUp();
			ugcMapEditorPropertiesPlaceXYZEntry( entry, uiCtx, 2 );

			MEContextStepDown();
		} else {
			MEContextAddSimpleMsg( kMEFieldType_TextEntry, "RotYaw", "UGC_MapEditor.Rotation", "UGC_MapEditor.Rotation.Tooltip" );
		}
	}

	if(  !isPlayingEditor
		 && component->eType != UGC_COMPONENT_TYPE_ROOM
		 && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR
		 && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR ) {
		MEFieldContext* zorderCtx = MEContextPush( "ZOrder", &component->sPlacement, &component->sPlacement, parse_UGCComponentPlacement );
		zorderCtx->bDontSortComboEnums = true;
		MEContextAddEnumMsg( kMEFieldType_Combo, UGCZOrderSortEnum, "ZOrderSort", "UGC_MapEditor.ZOrderSort", "UGC_MapEditor.ZOrderSort.Tooltip" );
		MEContextPop( "ZOrder" );
	}

	MEContextPop( __FUNCTION__ );
	estrDestroy( &estr );
}

void ugcMapEditorPropertiesPlaceXYZEntry( MEFieldContextEntry* entry, MEFieldContext* uiCtx, int index )
{
	// place the label
	ui_WidgetSetPosition( UI_WIDGET( ENTRY_LABEL( entry )),
						  uiCtx->iXPos + uiCtx->iXDataStart + 70 * index + 4,
						  uiCtx->iYPos + uiCtx->iYDataStart + 5 );
	ui_WidgetSetWidth( UI_WIDGET( ENTRY_LABEL( entry )), 12 );
	ui_WidgetSetPadding( UI_WIDGET( ENTRY_LABEL( entry )), 0, 0 );

	// place the field
	ui_WidgetSetPosition( ENTRY_FIELD( entry )->pUIWidget,
						  uiCtx->iXPos + uiCtx->iXDataStart + 70 * index + 14,
						  uiCtx->iYPos + uiCtx->iYDataStart );
	ui_WidgetSetWidth( ENTRY_FIELD( entry )->pUIWidget, 54 );
	ui_WidgetSetPadding( ENTRY_FIELD( entry )->pUIWidget, 0, 0 );
}

void ugcMapEditorPropertiesRefreshLinks( UGCComponent* component )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UGCMissionObjective* objective = ugcObjectiveFindComponentRelated( ugcEditorGetMission()->objectives, ugcEditorGetComponentList(), component->uID );
	UGCMissionMapLink* mapLink = ugcMissionFindLinkByExitComponent( ugcEditorGetProjectData(), component->uID );
	UGCComponent** eaTriggeredComponents = NULL;
	MEFieldContext* uiCtx;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIPane* pane;
	char* estr = NULL;

	if(   component->eType != UGC_COMPONENT_TYPE_CONTACT && component->eType != UGC_COMPONENT_TYPE_KILL
		  && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR
		  && component->eType != UGC_COMPONENT_TYPE_ROOM_MARKER && component->eType != UGC_COMPONENT_TYPE_OBJECT ) {
		return;
	}
	
	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MapEditor.Links", true );
	uiCtx = MEContextGetCurrent();
	uiCtx->astrOverrideSkinName = "UGCButton_Hyperlink";
	uiCtx->iXPos = 0;
	uiCtx->iYPos = 0;
	uiCtx->iXLabelStart = 0;
	uiCtx->iYLabelStart = 0;
	uiCtx->iXDataStart = 0;
	uiCtx->iYDataStart = 0;
	uiCtx->iYStep = UGC_ROW_HEIGHT;

	ugcBacklinkTableFindAllTriggers( ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), component->uID, 0, &eaTriggeredComponents );

	if( objective ) {
		ugcFormatMessageKey( &estr, "UGC_MapEditor.Link_GoToObjective",
							 STRFMT_STRING( "Objective", ugcMissionObjectiveUIString( objective )),
							 STRFMT_END );
		MEContextAddButton( estr, NULL, ugcObjectiveGoToCB, objective,
							"UsedInLink", NULL, TranslateMessageKey( "UGC_MapEditor.Link_GoToObjective.Tooltip" ));
	} else if( mapLink ) {
		MEContextAddButtonMsg( "UGC_MapEditor.Link_GoToMapTransfer", NULL, ugcMapLinkGoToCB, mapLink,
							   "UsedInLink", NULL, "UGC_MapEditor.Link_GoToMapTransfer.Tooltip" );
	}

	FOR_EACH_IN_EARRAY_FORWARDS( eaTriggeredComponents, UGCComponent, triggeredComponent ) {
		char idBuffer[ 256 ];
		char componentNameBuffer[ 256 ];
		sprintf( idBuffer, "Component_%d", triggeredComponent->uID );
		ugcComponentGetDisplayName( componentNameBuffer, ugcEditorGetProjectData(), triggeredComponent, false );
		MEContextAddButton( componentNameBuffer, NULL, ugcComponentGoToCB, triggeredComponent, idBuffer, NULL,
							TranslateMessageKey( "UGC_MapEditor.Link_GoToComponent.Tooltip" ));
	} FOR_EACH_END;

	if( eaSize( &eaTriggeredComponents ) == 0 && !mapLink && !objective ) {
		entry = MEContextAddLabelMsg( "NoLink", "UGC_MapEditor.NoLinks", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		REMOVE_HANDLE( widget->hOverrideSkin );
		MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;

	eaDestroy( &eaTriggeredComponents );
	estrDestroy( &estr );
}

void ugcMapEditorPropertiesRefreshBehavior( UGCComponent* component, bool isPlayingEditor )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UGCFSMMetadata* pFSMMetadata = ugcResourceGetFSMMetadata( component->fsmProperties.pcFSMNameRef );
	bool usingKillParent = false;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIPane* pane;

	// If we are looking at an actor, we want to display the parent component's fields here
	if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		component = ugcEditorFindComponentByID( component->uParentID );
		usingKillParent = true;
	}

	if( component->eType != UGC_COMPONENT_TYPE_KILL && component->eType != UGC_COMPONENT_TYPE_CONTACT ) {
		return;
	}

	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header",
													(usingKillParent ? "UGC_MapEditor.BehaviorGroupKillParent" : "UGC_MapEditor.BehaviorGroup"),
													true );
	MEContextPush( "FSMProperties", &component->fsmProperties, &component->fsmProperties, parse_UGCFSMProperties );

	// Special case here -- add a single, full width, button, unless it has an error
	{
		MEFieldContext* ctx = MEContextGetCurrent();
		float y = ctx->iYPos;
		entry = ugcMEContextAddResourcePickerMsg(
				(component->eType == UGC_COMPONENT_TYPE_KILL ? "Behavior" : "NonCombatBehavior"),
				"UGC_MapEditor.Behavior_Default", "UGC_MapEditor.Behavior_PickerTitle", false,
				"FSMRef", NULL, "UGC_MapEditor.Behavior.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		
		if( stricmp( ui_WidgetGetText( UI_WIDGET( ENTRY_SPRITE( entry ))),
					 "alpha8x8" ) == 0 ) {
			// no error
			ui_WidgetSetPosition( widget, ctx->iXPos, y );
		} else {
			ui_WidgetSetPosition( widget, ctx->iXPos + ctx->iXDataStart, y );
		}

		ctx->iYPos = y + UGC_ROW_HEIGHT;
	}
	if( ugcComponentHasPatrol( component, NULL )) {
		MEFieldContext* uiCtx = MEContextPush( "SingleLineButton", NULL, NULL, NULL );
		uiCtx->iXLabelStart = uiCtx->iYLabelStart = 0;
		uiCtx->iXDataStart = uiCtx->iYDataStart = 0;
		uiCtx->iYStep = UGC_ROW_HEIGHT;

		entry = MEContextAddButtonMsg( "UGC_MapEditor.Behavior_AddPatrolPoint", NULL, ugcComponentAddPatrolPointCB, component, "AddPatrolPoint", NULL, "UGC_MapEditor.Behavior_AddPatrolPoint.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		widget->u64 = isPlayingEditor;

		MEContextPop( "SingleLineButton" );
	}

	if( g_UGCMapEditorPropertiesState.bBehaviorExpanded ) {
		MEContextAddColoredSeparator( -1, "Separator", 2 );

		if( !pFSMMetadata || eaSize( &pFSMMetadata->eaExternVars ) == 0 ) {
			MEContextAddLabel( "FSMNoParams", "No Parameters", NULL );
			MEContextDestroyByName( "FSMVars" );
		} else {
			MEContextPush( "FSMVars", NULL, NULL, NULL );
			
			FOR_EACH_IN_EARRAY_FORWARDS( pFSMMetadata->eaExternVars, UGCFSMExternVar, externVar ) {
				const char* name = REF_STRING_FROM_HANDLE( externVar->defProps.hDisplayName );
				const char* tooltip = REF_STRING_FROM_HANDLE( externVar->defProps.hTooltip );
				UGCFSMVar* var = ugcComponentBehaviorGetFSMVar( component, externVar->astrName );

				MEContextPush( externVar->astrName, var, var, parse_UGCFSMVar );
				ugcEditorSetErrorPrefix( "%s.", externVar->astrName );

				if( stricmp( externVar->scType, "bool" ) == 0 ) {
					ugcMEContextAddBooleanMsg( "IntVal", name, tooltip );
				} else if( stricmp( externVar->scType, "AIAnimList" ) == 0 ) {
					ugcMEContextAddResourcePickerMsg( "AIAnimList", "UGC_MapEditor.Animation_Default", "UGC_MapEditor.Animation_PickerTitle", true,
													  "StringVal", name, tooltip );
				} else if( stricmp( externVar->scType, "AllMissionsIndex" ) == 0 ) {
					ugcMEContextAddWhenPickerMsg( component->sPlacement.pcMapName, var->pWhenVal, "WhenVal", 0, false, UGCWhenTypeNormalEnum, name, tooltip );
				} else if( externVar->type == MULTI_INT ) {
					MEContextAddSimpleMsg( kMEFieldType_TextEntry, "FloatVal", name, tooltip );
				} else if( externVar->type == MULTI_FLOAT ) {
					MEContextAddSimpleMsg( kMEFieldType_TextEntry, "FloatVal", name, tooltip );
				} else if( externVar->type == MULTI_STRING ) {
					entry = MEContextAddTextMsg( false, NULL, "StringVal", name, tooltip );
					ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_SINGLE_LINE_MAX_LENGTH );
				}

				ugcEditorSetErrorPrefix( NULL );
				MEContextPop( externVar->astrName );
			} FOR_EACH_END;

			MEContextPop( "FSMVars" );
		}
	}
	MEContextPop( "FSMProperties" );

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane ));

	ugcMapEditorRefreshExpanderButton( "Expander", "UGC_MapEditor.Behavior_Collapse", "UGC_MapEditor.Behavior_Expand", &g_UGCMapEditorPropertiesState.bBehaviorExpanded );

	MEContextAddCustomSpacer( 10 );
}

void ugcMapEditorPropertiesRefreshStates( UGCComponent* component )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UGCMissionObjective* objective = ugcObjectiveFindComponentRelated( ugcEditorGetMission()->objectives, ugcEditorGetComponentList(), component->uID );
	bool usingKillParent = false;
	UIPane* pane;

	// If we are looking at an actor, we want to display the parent component's fields here
	if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		component = ugcEditorFindComponentByID( component->uParentID );
		usingKillParent = true;
	}

	if( !ugcComponentStateCanBeEdited( ugcEditorGetProjectData(), component )) {
		return;
	}

	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header",
													(usingKillParent ? "UGC_MapEditor.StatesKillParent" : "UGC_MapEditor.States"),
													true );
	{
		const char* hideWhenLabel;
		const char* hideWhenTooltip;
		StaticDefineInt* hideWhenEnum;
		const char* showWhenLabel;
		const char* showWhenTooltip;
		StaticDefineInt* showWhenEnum;
		
		if( componentStateCanHaveCheckedAttrib( component->eType )) {
			MEFieldContext* checkedAttribCtx = MEContextPush( "CheckedAttrib", &component->visibleCheckedAttrib, &component->visibleCheckedAttrib, parse_UGCCheckedAttrib );
			checkedAttribCtx->bLabelsDisabled = checkedAttribCtx->bDisabled = (objective != NULL);
			
			ugcMEContextAddResourcePickerMsg( "CheckedAttrib", "UGC_MapEditor.VisibleCheckedAttrib_Default", "UGC_MapEditor.VisibleCheckedAttrib_PickerTitle", true,
											  "SkillName", "UGC_MapEditor.VisibleCheckedAttrib", "UGC_MapEditor.VisibleCheckedAttrib.Tooltip" );
			ugcMEContextAddResourcePickerMsg( "MissionItem", "UGC_MapEditor.VisibleItem_Default", "UGC_MapEditor.VisibleItem_PickerTitle", true,
											  "ItemName", "UGC_MapEditor.VisibleItem", "UGC_MapEditor.VisibleItem.Tooltip" );
			ugcMEContextAddBooleanMsg( "Not", "UGC_MapEditor.VisibleNot", "UGC_MapEditor.VisibleNot.Tooltip" );
			MEContextPop( "CheckedAttrib" );
		}

		if( component->eType == UGC_COMPONENT_TYPE_TRAP ) {
			hideWhenLabel = "UGC_MapEditor.HideWhen_Trap";
			hideWhenTooltip = "UGC_MapEditor.HideWhen_Trap.Tooltip";
			showWhenLabel = "UGC_MapEditor.ShowWhen_Trap";
			showWhenTooltip = "UGC_MapEditor.ShowWhen_Trap.Tooltip";
		} else if( component->eType == UGC_COMPONENT_TYPE_SOUND ) {
			hideWhenLabel = "UGC_MapEditor.HideWhen_Sound";
			hideWhenTooltip = "UGC_MapEditor.HideWhen_Sound.Tooltip";
			showWhenLabel = "UGC_MapEditor.ShowWhen_Sound";
			showWhenTooltip = "UGC_MapEditor.ShowWhen_Sound.Tooltip";
		} else {
			hideWhenLabel = "UGC_MapEditor.HideWhen";
			hideWhenTooltip = "UGC_MapEditor.HideWhen.Tooltip";
			showWhenLabel = "UGC_MapEditor.ShowWhen";
			showWhenTooltip = "UGC_MapEditor.ShowWhen.Tooltip";
		}

		if( objective ) {
			if( component->eType == UGC_COMPONENT_TYPE_CONTACT ) {
				hideWhenEnum = UGCWhenTypeObjectiveContactHideEnum;
				showWhenEnum = UGCWhenTypeObjectiveShowEnum;
			} else if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ) {
				hideWhenEnum = UGCWhenTypeObjectiveRoomDoorHideEnum;
				showWhenEnum = UGCWhenTypeObjectiveShowEnum;
			} else {
				hideWhenEnum = UGCWhenTypeObjectiveHideEnum;
				showWhenEnum = UGCWhenTypeObjectiveShowEnum;
			}
		} else {
			if( component->eType == UGC_COMPONENT_TYPE_CONTACT ) {
				hideWhenEnum = UGCWhenTypeContactHideEnum;
				showWhenEnum = UGCWhenTypeNormalEnum;
			} else if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				hideWhenEnum = UGCWhenTypeHideEnum;
				showWhenEnum = UGCWhenTypeDialogShowEnum;
			} else if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ) {
				hideWhenEnum = UGCWhenTypeRoomDoorHideEnum;
				showWhenEnum = UGCWhenTypeNormalEnum;
			} else {
				hideWhenEnum = UGCWhenTypeHideEnum;
				showWhenEnum = UGCWhenTypeNormalEnum;
			}
		}

		ugcMEContextAddWhenPickerMsg( component->sPlacement.pcMapName, component->pStartWhen, "ShowWhen", 0, objective != NULL,
									  showWhenEnum, showWhenLabel, showWhenTooltip );

		if( componentStateCanBecomeHidden( component->eType )) {
			ugcMEContextAddWhenPickerMsg( component->sPlacement.pcMapName, component->pHideWhen, "HideWhen", 1, objective != NULL,
										  hideWhenEnum, hideWhenLabel, hideWhenTooltip );
		}
	}
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane ));
	MEContextAddCustomSpacer( 10 );
}

void ugcMapEditorPropertiesRefreshRoomAdvanced( UGCComponent* component )
{
	UGCRoomInfo* roomInfo = ugcRoomGetRoomInfo( component->iObjectLibraryId );
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UIPane* pane;

	if( component->eType != UGC_COMPONENT_TYPE_ROOM ) {
		return;
	}
	
	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MapEditor.Room", true );

	// Refresh room detail set selection
	if( roomInfo ) {
		FOR_EACH_IN_EARRAY_FORWARDS( roomInfo->details, UGCRoomDetailDef, detailDef ) {
			StaticDefineInt* enumModel = MEContextAllocMemIndex(
					"RoomDetail", FOR_EACH_IDX( roomInfo->details, detailDef ),
					sizeof( StaticDefineInt ) * (detailDef->iChildCount + 2), NULL, true );
			UGCRoomDetailData* roomData = NULL;
			char buffer[ 256 ];

			FOR_EACH_IN_EARRAY( component->eaRoomDetails, UGCRoomDetailData, data ) {
				if( data->iIndex == FOR_EACH_IDX( roomInfo->details, detailDef )) {
					roomData = data;
					break;
				}
			} FOR_EACH_END;
			if( !roomData ) {
				continue;
			}

			sprintf( buffer, "RoomDetail%d", FOR_EACH_IDX( roomInfo->details, detailDef ));
			MEContextPush( buffer, roomData, roomData, parse_UGCRoomDetailData );
			
			// Fill out the model
			{
				int it;
				enumModel[ 0 ].key = U32_TO_PTR( DM_INT );
				for( it = 0; it != detailDef->iChildCount; ++it ) {
					enumModel[ it + 1 ].key = detailDef->eaNames[ it + 1 ];
					enumModel[ it + 1 ].value = it;
				}
				enumModel[ detailDef->iChildCount + 1 ].key = U32_TO_PTR( DM_END );
			}

			MEContextAddEnumMsg( kMEFieldType_Combo, enumModel, "Choice", detailDef->eaNames[ 0 ], "UGC_MapEditor.RoomDetail.Tooltip" );
			MEContextPop( buffer );
		} FOR_EACH_END;
	}

	ugcMEContextAddResourcePickerMsg( "UGCRoomTone", "UGC_MapEditor.RoomBG_Default", "UGC_MapEditor.RoomBG_PickerTitle", true,
									  "SoundEvent", "UGC_MapEditor.RoomBG", "UGC_MapEditor.RoomBG.Tooltip" );
	ugcMEContextAddResourcePickerMsg( "UGCSoundDSP", "UGC_MapEditor.RoomDSP_Default", "UGC_MapEditor.RoomDSP_PickerTitle", true,
									  "SoundDSP", "UGC_MapEditor.RoomDSP", "UGC_MapEditor.RoomDSP.Tooltip" );

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );	
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane ));
	MEContextAddCustomSpacer( 10 );
}

void ugcMapEditorRefreshDialogTree( UGCComponent* component, bool isPlayingEditor )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UGCComponent* dialogTree;
	UIPane* pane;
	MEFieldContextEntry* entry;

	if( component->eType != UGC_COMPONENT_TYPE_CONTACT && component->eType != UGC_COMPONENT_TYPE_OBJECT ) {
		return;
	}

	dialogTree = ugcComponentFindDefaultPromptForID( ugcEditorGetComponentList(), component->uID );
	if( !dialogTree ) {
		dialogTree = &g_emptyComponent;
	}
	
	pane = ugcMEContextPushPaneParentWithBooleanCheckMsg( __FUNCTION__, "Header", "UGC_MapEditor.DialogTree", dialogTree != &g_emptyComponent, true, ugcComponentDefaultPromptToggledCB, component );
	MEContextPush( "UGCEditor_Component_Prompt", &dialogTree->dialogBlock.initialPrompt, &dialogTree->dialogBlock.initialPrompt, parse_UGCDialogTreePrompt );
	MEContextGetCurrent()->bLabelsDisabled = MEContextGetCurrent()->bDisabled = (dialogTree == &g_emptyComponent);
	{
		if( component->eType == UGC_COMPONENT_TYPE_CONTACT ) {
			ugcMEContextAddResourcePickerMsg( "AIAnimList", "UGC_MapEditor.Animation_Default", "UGC_MapEditor.Animation_PickerTitle", true,
											  "PromptStyle", "UGC_MapEditor.Animation", "UGC_MapEditor.Animation_PickerTitle" );
		}

		entry = ugcMEContextAddMultilineTextMsg( "PromptBody", "UGC_MapEditor.PromptBody_Preview", "UGC_MapEditor.PromptBody_Preview.Tooltip" );
		ui_EditableSetDefaultMessage( ENTRY_FIELD( entry )->pUIEditable, "UGC_MapEditor.PromptBody_Preview_Default" );

		if( !isPlayingEditor ) {
			MEContextAddButtonMsg( "UGC_MapEditor.AdvancedDialogEditor", NULL, ugcComponentGoToDialogTreeCB, component,
								   "PromptDialogAdvanced", NULL, "UGC_MapEditor.AdvancedDialogEditor.Tooltip" );
		}
	}
	MEContextPop( "UGCEditor_Component_Prompt" );
	
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );	
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane ));
	MEContextAddCustomSpacer( 10 );
}

void ugcMapEditorRefreshInteract( UGCComponent* component )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UGCMissionObjective* objective = ugcObjectiveFindComponentRelated( ugcEditorGetMission()->objectives, ugcEditorGetComponentList(), component->uID );
	UGCMapType mapType = ugcMapGetType( ugcMapFindByName( ugcEditorGetProjectData(), component->sPlacement.pcMapName ));
	UGCInteractProperties* interactProps;
	UIPane* pane;

	if( component->eType != UGC_COMPONENT_TYPE_OBJECT ) {
		return;
	}

	assert( eaSize( &component->eaTriggerGroups ) <= 1 );
	if( objective ) {
		interactProps = &objective->sInteractProps;
	} else {
		interactProps = eaGet( &component->eaTriggerGroups, 0 );
		if( !interactProps ) {
			interactProps = &g_emptyInteractProps;
		}
	}
	
	pane = ugcMEContextPushPaneParentWithBooleanCheckMsg(
			__FUNCTION__, "Header", "UGC_MapEditor.Interact",
			interactProps != &g_emptyInteractProps,
			!objective && !ugcBacklinkTableFindTrigger( ugcEditorGetBacklinkTable(), component->uID, 0 ),
			ugcComponentInteractToggledCB, component );
	MEContextGetCurrent()->bLabelsDisabled = MEContextGetCurrent()->bDisabled = (interactProps == &g_emptyInteractProps);

	if( g_UGCMapEditorPropertiesState.bInteractExpanded ) {
		ugcRefreshInteractProperties(
				interactProps, mapType,
				(UGCINPR_BASIC | UGCINPR_CUSTOM_ANIM_AND_DURATION
				 | UGCINPR_CHECKED_ATTRIB_ITEMS | UGCINPR_CHECKED_ATTRIB_SKILLS) );
	} else {
		ugcRefreshInteractProperties(
				interactProps, mapType,
				(UGCINPR_BASIC | UGCINPR_CUSTOM_ANIM_AND_DURATION) );
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextPop( __FUNCTION__ );	
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane ));

	ugcMapEditorRefreshExpanderButton( "Expander", "UGC_MapEditor.Interact_Collapse", "UGC_MapEditor.Interact_Expand", &g_UGCMapEditorPropertiesState.bInteractExpanded );
	
	MEContextAddCustomSpacer( 10 );
}

void ugcMapEditorRefreshExpanderButton( const char* uid, const char* strCollapse, const char* strExpand, bool *pState )
{
	float y = MEContextGetCurrent()->iYPos;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	char tooltipBuffer[ RESOURCE_NAME_MAX_SIZE ];

	if( *pState ) {
		sprintf( tooltipBuffer, "%s.Tooltip", strCollapse );
		entry = MEContextAddButtonMsg( strCollapse, NULL, ugcBoolClearCB, pState, "Expander", NULL, tooltipBuffer );
	} else {
		sprintf( tooltipBuffer, "%s.Tooltip", strCollapse );
		entry = MEContextAddButtonMsg( strExpand, NULL, ugcBoolSetCB, pState, "Expander", NULL, tooltipBuffer );
	}
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	SET_HANDLE_FROM_STRING( g_hUISkinDict, *pState ? "UGCButton_Collapser" : "UGCButton_Expander", widget->hOverrideSkin );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPositionEx( widget, 0, y - 1, 0, 0, UITop );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );
}

void ugcRoomDoorDataReset( UGCRoomDoorData* data )
{
	eaiDestroy( &data->eaDoorTypeID );
	memset( data, 0, sizeof( *data ));
}

bool ugcRoomDoorDataFilterCB( UGCRoomDoorData* doorData, UGCAssetLibraryRow* row )
{
	int doorID = ugcRoomDoorGetTypeIDForResourceInfo( ugcResourceGetInfo( "ObjectLibrary", row->pcName ));

	if( doorID == -1 || eaiFind( &doorData->eaDoorTypeID, doorID ) == -1 ) {
		return false;
	}
	
	return true;
}

void ugcMapEditorPropertiesComponentPositionChangedCB( MEField* pField, bool bFinished, UGCComponentPlacementData* pPlacementData )
{
	UGCComponent* primary_component = pPlacementData->pPrimaryComponent;

	if( bFinished && !ugcEditorIsIgnoringChanges() ) {
		UGCMap *map = ugcEditorGetMapByName( primary_component->sPlacement.pcMapName );
		Vec2 min_bounds = { -1e8, -1e8 }, max_bounds = { 1e8, 1e8 };
		Vec3 clamp_min = { -1e8, -1e8, -1e8 }, clamp_max = { 1e8, 1e8, 1e8 };
		F32 spawn_height = 0;
		UGCComponentPlacement *placement = &primary_component->sPlacement;
		Vec3 vComponentOldRotation;
		Vec3 vComponentOldPosition;

		// put this here because we are disabling the Snap Normal boolean and want to communicate that it is false.
		if( pField && pField->pTable == parse_UGCComponentPlacement && stricmp( pField->pchFieldName, "Snap" ) == 0 ) {
			if( !ugcComponentPlacementNormalSnappingActive( placement )) {
				placement->bSnapNormal = false;
			}
		}

		// Get map playable bounds
		if( map ) {
			if( map->pPrefab ) {
				Vec3 out_spawn_pos;
				if( ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, out_spawn_pos, NULL )) {
					spawn_height = out_spawn_pos[ 1 ];
				}
			}
			ugcMapComponentValidBounds( clamp_min, clamp_max, ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), map, primary_component );
		}

		if( !pPlacementData->isPlayingEditor ) {
			placement->vPos[ 0 ] = pPlacementData->fXPos;
			placement->vPos[ 1 ] = pPlacementData->fYPos;
			placement->vPos[ 2 ] = pPlacementData->fZPos;
		} else {
			Mat4 mat4;
			identityMat4( mat4 );
			mat4[ 3 ][ 0 ] = pPlacementData->fXPos;
			mat4[ 3 ][ 1 ] = pPlacementData->fYPos + spawn_height;
			mat4[ 3 ][ 2 ] = pPlacementData->fZPos;
			ugcPlayingEditorComponentApplyMat4( primary_component, mat4 );
		}
		placement->vRotPYR[ 0 ] = fixAngleDeg( pPlacementData->fRotPitch );
		placement->vRotPYR[ 1 ] = fixAngleDeg( pPlacementData->fRotYaw );
		placement->vRotPYR[ 2 ] = fixAngleDeg( pPlacementData->fRotRoll );

		// Re-set our old pos/rot so they reflect our current values since we are updating now
		copyVec3( pPlacementData->vPrimaryOldPos,vComponentOldPosition );
		copyVec3( placement->vPos,pPlacementData->vPrimaryOldPos );

		copyVec3( pPlacementData->vPrimaryOldRot, vComponentOldRotation );
		copyVec3( placement->vRotPYR, pPlacementData->vPrimaryOldRot );
		
		// Find a valid position and move there if possible
		{
			UGCComponentValidPosition new_valid_pos;

			placement->vPos[ 0 ] = CLAMP( placement->vPos[ 0 ], clamp_min[ 0 ], clamp_max[ 0 ]);
			placement->vPos[ 1 ] = CLAMP( placement->vPos[ 1 ] + spawn_height, clamp_min[ 1 ], clamp_max[ 1 ])-spawn_height;
			placement->vPos[ 2 ] = CLAMP( placement->vPos[ 2 ], clamp_min[ 2 ], clamp_max[ 2 ]);

			if( ugcComponentIsValidPosition( ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), primary_component, placement->vPos, NULL, false, 0.01, 0.01, &new_valid_pos )) {
				ugcComponentSetValidPosition( ugcEditorGetProjectData(), primary_component, &new_valid_pos );
			}

			ugcComponentOpSetPlacement( ugcEditorGetProjectData(), primary_component, map, placement->uRoomID );
			ugcLayoutComponentFixupChildLocations( primary_component, vComponentOldPosition, placement->vPos, min_bounds, max_bounds );
		}

		ugcLayoutComponentFixupChildRotations( primary_component, primary_component, vComponentOldRotation[1], placement->vRotPYR[1] );

		ugcEditorQueueApplyUpdate();
	}
}

void ugcMapEditorPropertiesComponentParentChangedCB( MEField* pField, bool bFinished, UGCComponentValidPositionUI* parent_ui )
{
	if( bFinished ) {
		parent_ui->component->sPlacement.eSnap = parent_ui->selected;
		FOR_EACH_IN_EARRAY( parent_ui->results, UGCComponentValidPosition, result ) {
			if( parent_ui->selected == 1000 + eaSize( &parent_ui->results ) - FOR_EACH_IDX( parent_ui->results, result )) {
				UGCComponent* component = parent_ui->component;
				
				component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_PARENTED;
				component->sPlacement.iRoomLevel = result->room_level;

				if( ugcComponentCanReparent( component->eType )) {
					if( component->uParentID ) {
						UGCComponent* parentComponent = ugcEditorFindComponentByID( component->uParentID );
						if( parentComponent ) {
							eaiFindAndRemove( &parentComponent->uChildIDs, component->uID );
						}
					}
					component->uParentID = result->room_id;
					if( component->uParentID ) {
						UGCComponent* parentComponent = ugcEditorFindComponentByID( component->uParentID );
						if( parentComponent ) {
							eaiPush( &parentComponent->uChildIDs, component->uID );
						}
					}
				}
				break;
			}
		} FOR_EACH_END;
		ugcEditorQueueApplyUpdate();
	}
}

void ugcMapEditorPropertiesVolumeRadiusChangedCB( MEField* pField, bool bFinished, UGCMarkerVolumeRadiusData* pVolumeRadiusData )
{
	UGCComponent* pComponent = pVolumeRadiusData->pComponent;
	
	if( bFinished ) {
		pComponent->fVolumeRadius = CLAMP( pVolumeRadiusData->fVolumeRadius, 0.0f, 15000.0f );
		ugcEditorQueueApplyUpdate();
	}
}

void ugcMapEditorPropertiesEncounterChangedCB( MEField* pField, bool bFinished, UGCComponent* component )
{
	if( bFinished ) {
		ugcComponentOpDeleteChildren( ugcEditorGetProjectData(), component, true );
		ugcEditorQueueApplyUpdate();
	}
}



void ugcComponentCustomizeCostumeCB( UIButton* ignored, UGCComponent* component )
{
	// The idea of "customizing" a costume only makes sense in
	// Neverwinter, where all UGC costumes are unrestricted.
	assert( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_NEVERWINTER );
	
	if( nullStr( component->pcCostumeName )) {
		// do nothing, no costume!
		return;
	}

	if( ugcCostumeFindByName( ugcEditorGetProjectData(), component->pcCostumeName )) {
		ugcEditorEditCostume( NULL, component->pcCostumeName );
	} else {
		const char* oldName = ugcResourceGetDisplayName( "PlayerCostume", component->pcCostumeName, NULL );
		char* estrName = NULL;
		UGCCostume* newCostume;

		if( nullStr( oldName )) {
			estrPrintf( &estrName, "Customized Costume" );
		} else {
			estrPrintf( &estrName, "Customized %s", oldName );
		}
		newCostume = ugcEditorCreateCostume( estrName, component->pcCostumeName, 0 );
		StructCopyString( &component->pcCostumeName, newCostume->astrName );
		if( newCostume ) {
			ugcEditorEditCostume(NULL, newCostume->astrName);
		}
		estrDestroy( &estrName );
	}
}

void ugcComponentAddPatrolPointCB( UIButton* widget, UGCComponent* component )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	bool isPlayingEditor = (bool)UI_WIDGET( widget )->u64;

	int size = eaiSize( &component->eaPatrolPoints );
	if( size > 0 ) {
		UGCComponent* lastPoint = ugcEditorFindComponentByID( component->eaPatrolPoints[ size - 1 ]);
		UGCComponent* newPoint = ugcComponentOpCreate( ugcProj, UGC_COMPONENT_TYPE_PATROL_POINT, component->uParentID );
		if( !isPlayingEditor ) {
			StructCopyAll( parse_UGCComponentPlacement, &lastPoint->sPlacement, &newPoint->sPlacement );
		} else {
			Mat4 mat4;
			StructCopyString( &newPoint->sPlacement.pcMapName, lastPoint->sPlacement.pcMapName );
			ugcPlayingEditorComponentMat4( lastPoint, mat4 );
			ugcPlayingEditorComponentApplyMat4( newPoint, mat4 );
		}
		newPoint->sPlacement.vPos[0] += 20;
		newPoint->uPatrolParentID = component->uID;
		eaiPush( &component->eaPatrolPoints, newPoint->uID );
	}

	ugcEditorQueueApplyUpdate();
}

void ugcComponentAddPatrolPointBeforeCB( UIButton* widget, UGCComponent* component )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	bool isPlayingEditor = (bool)UI_WIDGET( widget )->u64;
	UGCComponent* patrolComponent = ugcEditorFindComponentByID( component->uPatrolParentID );
	int index = -1;

	if( patrolComponent ) {
		index = eaiFind( &patrolComponent->eaPatrolPoints, component->uID );
	}

	if( index >= 0 ) {
		UGCComponent* newPoint = ugcComponentOpCreate( ugcProj, UGC_COMPONENT_TYPE_PATROL_POINT, patrolComponent->uParentID );
		if( !isPlayingEditor ) {
			StructCopyAll( parse_UGCComponentPlacement, &component->sPlacement, &newPoint->sPlacement );
		} else {
			Mat4 mat4;
			StructCopyString( &newPoint->sPlacement.pcMapName, component->sPlacement.pcMapName );
			ugcPlayingEditorComponentMat4( component, mat4 );
			ugcPlayingEditorComponentApplyMat4( newPoint, mat4 );
		}
		
		if( index == 0 ) {
			newPoint->sPlacement.vPos[0] -= 10;
		} else {
			UGCComponent* prevPoint = ugcEditorFindComponentByID( patrolComponent->eaPatrolPoints[ index - 1 ]);
			if( prevPoint ) {
				Vec3 toPrev;
				subVec3( prevPoint->sPlacement.vPos, component->sPlacement.vPos, toPrev );
				normalVec3( toPrev );
				scaleVec3( toPrev, 10, toPrev );
				addVec3( newPoint->sPlacement.vPos, toPrev, newPoint->sPlacement.vPos );
			}
		}
		
		newPoint->uPatrolParentID = patrolComponent->uID;
		eaiInsert( &patrolComponent->eaPatrolPoints, newPoint->uID, index );
	}

	ugcEditorQueueApplyUpdate();
}

void ugcComponentAddPatrolPointAfterCB( UIButton* widget, UGCComponent* component )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	bool isPlayingEditor = (bool)UI_WIDGET( widget )->u64;
	UGCComponent* patrolComponent = ugcEditorFindComponentByID( component->uPatrolParentID );
	int index = -1;

	if( patrolComponent ) {
		index = eaiFind( &patrolComponent->eaPatrolPoints, component->uID );
	}

	if( index >= 0 ) {
		UGCComponent* newPoint = ugcComponentOpCreate( ugcProj, UGC_COMPONENT_TYPE_PATROL_POINT, patrolComponent->uParentID );
		if( !isPlayingEditor ) {
			StructCopyAll( parse_UGCComponentPlacement, &component->sPlacement, &newPoint->sPlacement );
		} else {
			Mat4 mat4;
			StructCopyString( &newPoint->sPlacement.pcMapName, component->sPlacement.pcMapName );
			ugcPlayingEditorComponentMat4( component, mat4 );
			ugcPlayingEditorComponentApplyMat4( newPoint, mat4 );
		}

		if( index == eaiSize( &patrolComponent->eaPatrolPoints ) - 1 ) {
			newPoint->sPlacement.vPos[0] += 10;
		} else {
			UGCComponent* nextPoint = ugcEditorFindComponentByID( patrolComponent->eaPatrolPoints[ index + 1 ]);
			if( nextPoint ) {
				Vec3 toNext;
				subVec3( nextPoint->sPlacement.vPos, component->sPlacement.vPos, toNext );
				normalVec3( toNext );
				scaleVec3( toNext, 10, toNext );
				addVec3( newPoint->sPlacement.vPos, toNext, newPoint->sPlacement.vPos );
			}
		}
		
		newPoint->uPatrolParentID = patrolComponent->uID;
		eaiInsert( &patrolComponent->eaPatrolPoints, newPoint->uID, index + 1 );
	}

	ugcEditorQueueApplyUpdate();
}

void ugcComponentTrapPickerShowCB( UIButton* button, UGCComponent* component )
{
	ugcAssetLibraryShowPicker( component, true, TranslateMessageKey( "UGC_MapEditor.Trap_PickerTitle" ), NULL, "Trap", "DEFAULT_VALUE", ugcComponentTrapPickerSetCB );
}

void ugcComponentTrapPickerSetCB( UGCAssetLibraryPane* pane, UGCComponent* component, UGCAssetLibraryRow* row )
{
	int objlibID = 0;
	char powerName[ 256 ] = "";
	sscanf_s( row->pcName, "%d,%s", &objlibID, SAFESTR( powerName ));

	if( objlibID && !nullStr( powerName )) {
		component->iObjectLibraryId = objlibID;
		StructCopyString( &component->pcTrapPower, powerName );
	}

	ugcEditorQueueApplyUpdate();
}

void ugcComponentDefaultPromptToggledCB( UICheckButton* button, UGCComponent* component )
{
	UGCComponent* defaultPrompt = ugcComponentFindDefaultPromptForID( ugcEditorGetProjectData()->components, component->uID );

	if( ui_CheckButtonGetState( button )) {
		if( !defaultPrompt ) {
			defaultPrompt = ugcComponentOpCreate( ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_DIALOG_TREE, 0 );
			defaultPrompt->bIsDefault = true;
			defaultPrompt->uActorID = component->uID;
		}
	} else {
		if( defaultPrompt ) {
			ugcComponentOpDelete(ugcEditorGetProjectData(), defaultPrompt, false);
		}
	}
	ugcEditorQueueApplyUpdate();
}

void ugcComponentInteractToggledCB( UICheckButton* button, UGCComponent* component )
{
	component->bInteractForce = ui_CheckButtonGetState( button );
	ugcEditorQueueApplyUpdate();
}

void ugcComponentGoToCB( UIButton* ignored, UGCComponent* component )
{
	ugcEditorEditMapComponent( component->sPlacement.pcMapName, component->uID, false, true );
}

void ugcComponentGoToDialogTreeCB( UIButton* ignored, UGCComponent* component )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent* dialogTree = ugcComponentFindDefaultPromptForID( ugcProj->components, component->uID );

	if( dialogTree ) {
		ugcEditorEditDialogTreeBlock( dialogTree->uID, -1, 0 );
	}
}

void ugcObjectiveGoToCB( UIButton* ignored, UGCMissionObjective* objective )
{
	ugcEditorEditMissionObjective( ugcMissionObjectiveLogicalNameTemp( objective ));
}

void ugcMapLinkGoToCB( UIButton* ignored, UGCMissionMapLink* mapLink )
{
	ugcEditorEditMissionMapTransitionByMapLink( mapLink );
}

void ugcBoolClearCB( UIButton* ignored, bool* bToClear )
{
	*bToClear = false;
	ugcEditorQueueApplyUpdate();
}

void ugcBoolSetCB( UIButton* ignored, bool* bToSet )
{
	*bToSet = true;
	ugcEditorQueueApplyUpdate();
}

const char* ugcMapEditorPropertiesLengthUnit( UGCMap* map )
{
	UGCMapType mapType = ugcMapGetType( map );
	RegionRules* rules = NULL;

	if( mapType == UGC_MAP_TYPE_SPACE || mapType == UGC_MAP_TYPE_PREFAB_SPACE ) {
		rules = getRegionRulesFromRegionType( StaticDefineIntGetInt( WorldRegionTypeEnum, "Space" ));
	} else {
		rules = getRegionRulesFromRegionType( StaticDefineIntGetInt( WorldRegionTypeEnum, "Ground" ));
	}

	if( rules && IS_HANDLE_ACTIVE( rules->dmsgDistUnitsShort.hMessage )) {
		return REF_STRING_FROM_HANDLE( rules->dmsgDistUnitsShort.hMessage );
	} else {
		return "UGC_MapEditor.DefaultLengthUnit";
	}
}

void ugcMapEditorGlobalPropertiesWindowShow( UGCMapEditorDoc* doc )
{
	if( !doc->global_properties_window ) {
		doc->global_properties_window = ui_WindowCreate( "", 0, 0, 150, 100 );
	}

	ugcMapEditorGlobalPropertiesWindowRefresh( doc );
	elUICenterWindow( doc->global_properties_window );
	ui_WindowSetModal( doc->global_properties_window, true );
	ui_WindowPresentEx( doc->global_properties_window, true );
}

void ugcMapEditorGlobalPropertiesWindowRefresh( UGCMapEditorDoc* doc )
{
	char strContextName[ 256 ];
	MEFieldContext* uiCtx;

	if( !doc->global_properties_window ) {
		return;
	}

	ui_WidgetSetDimensions( UI_WIDGET( doc->global_properties_window ), 150, 100 );
	ui_WidgetSetTextMessage( UI_WIDGET( doc->global_properties_window ), "UGC_MapEditor.GlobalProperties" );
	ui_WindowSetResizable( doc->global_properties_window, false );

	sprintf( strContextName, "UGCMapEditor_%s_GlobalProperties", doc->doc_name );
	uiCtx = MEContextPush( strContextName, doc->map_data, doc->map_data, parse_UGCMap );
	uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
	MEContextSetParent( UI_WIDGET( doc->global_properties_window ));
	MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
	MEContextSetErrorContext( ugcMakeTempErrorContextMap( doc->map_data->pcName ));
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
	uiCtx->bTextEntryTrimWhitespace = true;
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

	MEContextAddSimpleMsg( kMEFieldType_TextEntry, "DisplayName", "UGC_MapEditor.MapName", "UGC_MapEditor.MapName.Tooltip" );
	MEContextAddSimpleMsg( kMEFieldType_TextEntry, "Notes", "UGC_MapEditor.Notes", "UGC_MapEditor.Notes.Tooltip" );

	MEContextPop( strContextName );
}

void ugcMapEditorGlobalPropertiesErrorButtonRefresh( UGCMapEditorDoc* doc, UISprite** ppErrorSprite )
{
	ugcErrorButtonRefresh( ppErrorSprite, ugcEditorGetRuntimeStatus(), ugcMakeTempErrorContextMap( doc->map_data->pcName ),
						   "DisplayName Notes", 0, NULL );
}

void ugcMapEditorBackdropWindowShow( UGCMapEditorDoc* doc )
{
	if( !doc->backdrop_properties_window ) {
		doc->backdrop_properties_window = ui_WindowCreate( "", 0, 0, 300, 200 );
	}
	ugcMapEditorBackdropPropertiesWindowRefresh( doc );
	elUICenterWindow( doc->backdrop_properties_window );
	ui_WindowSetModal( doc->backdrop_properties_window, true );
	ui_WindowPresentEx( doc->backdrop_properties_window, true );
}

void ugcMapEditorBackdropPropertiesRefresh( UGCMap* map, bool isPlayingEditor )
{
	UGCMapType mapType = ugcMapGetType( map );
	UGCGenesisBackdrop* backdrop = NULL;
	const char* baseTagType = NULL;
	const char* overrideTagType = NULL;
	if( map->pPrefab ) {
		backdrop = &map->pPrefab->backdrop;
	} else if( map->pSpace ) {
		backdrop = &map->pSpace->backdrop;
	} else {
		assert( 0 );
	}
	switch( mapType ) {
		case UGC_MAP_TYPE_SPACE: case UGC_MAP_TYPE_PREFAB_SPACE:
			baseTagType = "SpaceSkyLayerBase";
			overrideTagType = "SpaceSkyLayerOverride";
			break;
		case UGC_MAP_TYPE_GROUND: case UGC_MAP_TYPE_PREFAB_GROUND:
			baseTagType = "ExteriorSkyLayerBase";
			overrideTagType = "ExteriorSkyLayerOverride";
			break;
		case UGC_MAP_TYPE_INTERIOR: case UGC_MAP_TYPE_PREFAB_INTERIOR:
			baseTagType = "InteriorSkyLayerBase";
			overrideTagType = "InteriorSkyLayerOverride";
			break;
		default:
			assert( 0 );
	}

	MEContextPush( "BackdropData", backdrop, backdrop, parse_UGCGenesisBackdrop );

	ugcMEContextAddResourcePickerMsg( baseTagType, "UGC_MapEditor.Backdrop_Default", "UGC_MapEditor.Backdrop_PickerTitle", false,
		"SkyBase", "UGC_MapEditor.Backdrop", "UGC_MapEditor.Backdrop.Tooltip" );

	if(!isPlayingEditor)
		if(mapType == UGC_MAP_TYPE_SPACE || mapType == UGC_MAP_TYPE_PREFAB_SPACE || mapType == UGC_MAP_TYPE_GROUND || mapType == UGC_MAP_TYPE_PREFAB_GROUND)
			ugcMEContextAddResourcePickerMsg( "UGCAmbientSound", "UGC_MapEditor.BackdropSound_Default", "UGC_MapEditor.BackdropSound_PickerTitle", true,
				"AmbSoundOverride", "UGC_MapEditor.BackdropSound", "UGC_MapEditor.BackdropSound.Tooltip" );

	if( baseTagType ) {
		int it;
		for( it = 0; it != eaSize( &backdrop->eaSkyOverrides ); ++it ) {
			char overrideContextName[ 256 ];
			char overrideErrorContextName[ 256 ];
			MEFieldContext* skyUICtx;
			MEFieldContextEntry* entry;

			sprintf( overrideContextName, "Override%d", it );
			sprintf( overrideErrorContextName, "SkyOverride%d", it );
			skyUICtx = MEContextPush( overrideContextName, backdrop->eaSkyOverrides[ it ], backdrop->eaSkyOverrides[ it ], parse_UGCGenesisBackdropSkyOverride );
			if( it > 0 ) {
				skyUICtx->iYDataStart = 0;
				setVec2( skyUICtx->iErrorIconOffset, 0, 3 );
				skyUICtx->iYStep = UGC_ROW_HEIGHT;;
			}
			entry = ugcMEContextAddResourcePickerMsg( overrideTagType, "UGC_MapEditor.BackdropSky_Default", "UGC_MapEditor.BackdropSky_PickerTitle", true,
				"SkyOverride",
				(it == 0 ? "UGC_MapEditor.BackdropSky" : NULL),
				"UGC_MapEditor.BackdropSky.Tooltip" );
			MEContextSetEntryErrorForField( entry, overrideErrorContextName );
			MEContextPop( overrideContextName );
		}
	}

	MEContextPop( "BackdropData" );
}

void ugcMapEditorBackdropPropertiesWindowRefresh( UGCMapEditorDoc* doc )
{
	char strContextName[ 256 ];
	MEFieldContext* uiCtx;
	UIScrollArea* scrollarea;

	if( !doc->backdrop_properties_window ) {
		return;
	}

	ui_WidgetSetDimensions( UI_WIDGET( doc->backdrop_properties_window ), 300, 200 );
	ui_WidgetSetTextMessage( UI_WIDGET( doc->backdrop_properties_window ), "UGC_MapEditor.BackdropProperties" );
	ui_WindowSetResizable( doc->backdrop_properties_window, false );

	sprintf( strContextName, "UGCMapEditor_%s_BackdropProperties", doc->doc_name );
	uiCtx = MEContextPush( strContextName, doc->map_data, doc->map_data, parse_UGCMap );
	uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
	MEContextSetParent( UI_WIDGET( doc->backdrop_properties_window ));
	MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
	MEContextSetErrorContext( ugcMakeTempErrorContextMap( doc->map_data->pcName ));
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
	uiCtx->bTextEntryTrimWhitespace = true;
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

	if( doc->map_data->pUnitializedMap ) {
		MEContextPop( strContextName );
		return;
	}

	scrollarea = MEContextPushScrollAreaParent( "ScrollArea" );
	ui_WidgetSetDimensionsEx( UI_WIDGET( scrollarea ), 1, 1, UIUnitPercentage, UIUnitPercentage );

	ugcMapEditorBackdropPropertiesRefresh( doc->map_data, /*isPlayingEditor=*/false );

	MEContextPop( "ScrollArea" );
	MEContextPop( strContextName );
}

void ugcMapEditorBackdropPropertiesErrorButtonRefresh( UGCMapEditorDoc* doc, UISprite** ppErrorSprite )
{
	char* estr = NULL;
	int it;
	estrConcatf( &estr, "SkyBase AmbSoundOverride" );
	{
		UGCGenesisBackdrop* backdrop = NULL;
		if( doc->map_data->pPrefab ) {
			backdrop = &doc->map_data->pPrefab->backdrop;
		} else if( doc->map_data->pSpace ) {
			backdrop = &doc->map_data->pSpace->backdrop;
		} else {
			assert( 0 );
		}
	
		for( it = 0; it != eaSize( &backdrop->eaSkyOverrides ); ++it ) {
			estrConcatf( &estr, " SkyOverride%d", it );
		}
	}
	
	ugcErrorButtonRefresh( ppErrorSprite, ugcEditorGetRuntimeStatus(), ugcMakeTempErrorContextMap( doc->map_data->pcName ),
						   estr, 0, NULL );

	estrDestroy( &estr );
}

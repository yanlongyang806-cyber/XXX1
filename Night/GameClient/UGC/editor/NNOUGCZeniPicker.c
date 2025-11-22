#include "NNOUGCZeniPicker.h"

#include "EditLibUIUtil.h"
#include "GfxClipper.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "HashFunctions.h"
#include "NNOUGCCommon.h"
#include "ResourceInfo.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCProjectUtils.h"
#include "UIButton.h"
#include "UICore.h"
#include "UIList.h"
#include "UIMinimap.h"
#include "UIPane.h"
#include "UITabs.h"
#include "UITextureAssembly.h"
#include "UITree.h"
#include "UIWindow.h"
#include "WorldGrid.h"
#include "allegiance.h"
#include "inputMouse.h"

#define STANDARD_ROW_HEIGHT 26

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

typedef struct UGCZeniPickerWindow {
	UIWindow* window;
	
	UIWidget* rootWidget;
	UIWidget* overworldMapWidget;
	
	UGCZeniPickerCallback cb;
	UserData userData;

	// Window level filtering
	const char* defaultZmap;
	UGCZeniPickerFilterFn filterFn;
	UserData filterData;
} UGCZeniPickerWindow;

typedef struct UGCZeniPickerOverwoldMapWindow {
	UIWindow* window;
	UIWidget* overworldMapWidget;
	UIButton* okButton;

	UGCZeniPickerCallback cb;
	UserData userData;
} UGCZeniPickerOverwoldMapWindow;

typedef struct UGCZeniPicker {
	UIPane* rootPane;
	UICheckButton* pickerHideInvalidCheck;
	UITree* pickerTree;
	UIScrollArea* minimapArea;
	UIMinimap* minimap;
	UIWidget* minimapCustomWidget;
	
	UIPane* selectedDetails;

	ZoneMapEncounterInfo **ugcInfoOverrides;
	S32* filterValidTypes;
	const char* filterZmap;
	UGCZeniPickerFilterFn filterFn;
	UserData filterData;

	ZoneMapEncounterInfo** zenis;
} UGCZeniPicker;

typedef struct UGCZeniPickerOverworldMap {
	UIPane* rootPane;
	UIList* iconList;
	UIWidgetWidget* icon;

	const char** eaIconNames;

	// Widget info for converting between screen space coords and
	// percentage coords
	Vec2 mapLastTopLeft;
	float mapLastWidth;
	float mapLastHeight;
	
	Vec2 iconPosition;
	const char* iconName;

	// Drag and Drop info
	bool isDragging;

	// Changed callbacks
	UGCZeniPickerChangedFn changedFn;
	UserData changedData;
} UGCZeniPickerOverworldMap;

static int ugcZeniPickerWindowsOpen = 0;

static void ui_ZeniPickerIconTick( UIWidgetWidget* icon, UI_PARENT_ARGS );
static void ui_ZeniPickerIconDraw( UIWidgetWidget* icon, UI_PARENT_ARGS );
static void ui_ZeniPickerIconFree( UIWidgetWidget* icon );

static bool ugcZeniPickerObjectAcceptsType( ZoneMapEncounterObjectInfo* object, UGCZeniPickerFilterType type )
{
	if (type == UGCZeniPickerType_Any)
		return true;

	if( object->interactType == WL_ENC_CONTACT ) {
		return (type == UGCZeniPickerType_Contact);
	}
	
	switch( object->type ) {
		case WL_ENC_INTERACTABLE:
			switch( object->interactType ) {
				case WL_ENC_DOOR:
					return (type == UGCZeniPickerType_Door || type == UGCZeniPickerType_Usable_As_Warp
							|| type == UGCZeniPickerType_Usable_For_ComponentComplete);
				case WL_ENC_UGC_OPEN_DOOR:
					return (type == UGCZeniPickerType_Door);
				case WL_ENC_CLICKIE:
					return (type == UGCZeniPickerType_Clickie || type == UGCZeniPickerType_Usable_As_Warp
							|| type == UGCZeniPickerType_Usable_For_ComponentComplete);
				case WL_ENC_DESTRUCTIBLE:
					return (type == UGCZeniPickerType_Destructible);
				case WL_ENC_REWARD_BOX:
					return (type == UGCZeniPickerType_Reward_Box);
			}

		xcase WL_ENC_ENCOUNTER:
			return (type == UGCZeniPickerType_Encounter || type == UGCZeniPickerType_Usable_For_ComponentComplete);
			
		xcase WL_ENC_NAMED_VOLUME:
			return (type == UGCZeniPickerType_Volume || type == UGCZeniPickerType_Usable_For_ComponentReached);

		xcase WL_ENC_SPAWN_POINT:
			return (type == UGCZeniPickerType_Spawn);
	}

	return false;
}

static bool ugcZeniPickerObjectMatchesActiveFilter( UGCZeniPicker* picker, const char* zmName, ZoneMapEncounterObjectInfo* object )
{
	bool isUsingWholeMap = eaiSize( &picker->filterValidTypes ) == 1 && picker->filterValidTypes[ 0 ] == UGCZeniPickerType_WholeMap;

	if( picker->filterFn && !picker->filterFn( zmName, object, picker->filterData )) {
		return false;
	}

	if( ea32Size( &picker->filterValidTypes ) == 0 || isUsingWholeMap ) {
		return true;
	} else {
		int i;
		for (i = ea32Size( &picker->filterValidTypes )-1; i >= 0; --i)
		{
			if (ugcZeniPickerObjectAcceptsType( object, picker->filterValidTypes[ i ]))
			{
				return true;
			}
		}

		return false;
	}
}

static const char* ugcZeniPickerObjectIcon( ZoneMapEncounterObjectInfo* object )
{
	switch( object->type ) {
		xcase WL_ENC_CONTACT:
			return "UGC_Icons_Map_NPC";
			
		xcase WL_ENC_INTERACTABLE:
			switch( object->interactType ) {
				xcase WL_ENC_DOOR: case WL_ENC_UGC_OPEN_DOOR:
					return "UGC_Icons_Map_Door";
				xcase WL_ENC_REWARD_BOX:
					return "UGC_Icons_Map_Treasure";
				xcase WL_ENC_CLICKIE:
					return "UGC_Icons_Map_Object";
			}
		xcase WL_ENC_ENCOUNTER:
			return "UGC_Icons_Map_Critter_Group";
		xcase WL_ENC_NAMED_VOLUME:
			return "UGC_Icons_Map_Marker";
		xcase WL_ENC_SPAWN_POINT:
			return "UGC_Icons_Map_Spawn";
	}

	return "UGC_Icons_Map_Object";
}

static int ugcZeniPickerCompareObjects( const ZoneMapEncounterObjectInfo** obj1, const ZoneMapEncounterObjectInfo** obj2 )
{
	const char* name1 = NULL;
	const char* name2 = NULL;

	if( g_ui_State.bInUGCEditor ) {
		name1 = TranslateMessageRef( (*obj1)->displayName );
		name1 = TranslateMessageRef( (*obj2)->displayName );
	}

	if( !name1 ) {
		name1 = (*obj1)->logicalName;
	}
	if( !name2 ) {
		name2 = (*obj2)->logicalName;
	}

	return stricmp_safe( name1, name2 );
}

static int ugcZeniPickerCompareZenis( const ZoneMapEncounterInfo** zeni1, const ZoneMapEncounterInfo** zeni2 )
{
	ZoneMapInfo* zminfo1 = zmapInfoGetByPublicName( (*zeni1)->map_name );
	ZoneMapInfo* zminfo2 = zmapInfoGetByPublicName( (*zeni2)->map_name );
	const char* name1 = NULL;
	const char* name2 = NULL;

	if( g_ui_State.bInUGCEditor ) {
		name1 = TranslateMessagePtr( zmapInfoGetDisplayNameMessagePtr( zminfo1 ));
		name2 = TranslateMessagePtr( zmapInfoGetDisplayNameMessagePtr( zminfo2 ));
	}

	if( !name1 ) {
		name1 = (*zeni1)->map_name;
	}
	if( !name2 ) {
		name2 = (*zeni2)->map_name;
	}

	return stricmp_safe( name1, name2 );
}

static void ugcZeniPickerMaybeAddZeni( UGCZeniPicker* picker, ZoneMapEncounterInfo* zeni, int* hiddenBecauseInvalidCount )
{
	ZoneMapEncounterInfo* zeniClone = StructClone( parse_ZoneMapEncounterInfo, zeni );
	int it;
	bool isUsingWholeMap = eaiSize( &picker->filterValidTypes ) == 1 && picker->filterValidTypes[ 0 ] == UGCZeniPickerType_WholeMap;

	assert( zeniClone );
	for( it = 0; it != eaSize( &zeniClone->objects ); ++it ) {
		ZoneMapEncounterObjectInfo* zeniObj = zeniClone->objects[ it ];
			
		if( !ugcZeniPickerObjectMatchesActiveFilter( picker, zeni->map_name, zeniObj )) {
			StructDestroy( parse_ZoneMapEncounterObjectInfo, zeniObj );
			eaRemove( &zeniClone->objects, it );
			--it;
		} else if( zeniObj->ugcIsInvalidSelection && ui_CheckButtonGetState( picker->pickerHideInvalidCheck )) {
			++*hiddenBecauseInvalidCount;
			StructDestroy( parse_ZoneMapEncounterObjectInfo, zeniObj );
			eaRemove( &zeniClone->objects, it );
			--it;
		}
	}
		
	if( eaSize( &zeniClone->objects ) == 0 ) {
		StructDestroySafe( parse_ZoneMapEncounterInfo, &zeniClone );
	} else if( isUsingWholeMap ) {
		eaDestroyStruct( &zeniClone->objects, parse_ZoneMapEncounterObjectInfo );
	}


	if( zeniClone ) {
		eaQSort( zeniClone->objects, ugcZeniPickerCompareObjects );
		eaPush( &picker->zenis, zeniClone );
	}
}

static void ugcZeniPickerRefreshUI( UGCZeniPicker* picker, bool selectionMakeVisible, const char* forceSetZmap, const char* forceSetObject, bool* out_setFound )
{
	ZoneMapEncounterInfo* selectedZeni = NULL;
	ZoneMapEncounterObjectInfo* selectedObject = NULL;
	char* estr = NULL;
	int hiddenBecauseInvalidCount = 0;

	if( out_setFound ) {
		*out_setFound = false;
	}

	// Refresh the tree
	eaDestroyStruct( &picker->zenis, parse_ZoneMapEncounterInfo );

	if( eaSize( &picker->ugcInfoOverrides )) {
		int it;
		for( it = 0; it != eaSize( &picker->ugcInfoOverrides ); ++it ) {
			ugcZeniPickerMaybeAddZeni( picker, picker->ugcInfoOverrides[ it ], &hiddenBecauseInvalidCount );
		}
	} else {
		FOR_EACH_IN_REFDICT( "ZoneMapEncounterInfo", ZoneMapEncounterInfo, zeni ) {
			ugcZeniPickerMaybeAddZeni( picker, zeni, &hiddenBecauseInvalidCount );
		} FOR_EACH_END;
	}

	ugcFormatMessageKey( &estr, "UGC_ZeniPicker.HideInvalidComponents",
						 STRFMT_INT( "NumHidden", hiddenBecauseInvalidCount ),
						 STRFMT_END );
	ui_CheckButtonSetText( picker->pickerHideInvalidCheck, estr );
	
	eaQSort( picker->zenis, ugcZeniPickerCompareZenis );
	ui_TreeRefresh( picker->pickerTree );

	if( forceSetZmap ) {
		int zeniIt;
		int objIt;
		for( zeniIt = 0; zeniIt != eaSize( &picker->zenis ); ++zeniIt ) {
			ZoneMapEncounterInfo* zeni = picker->zenis[ zeniIt ];

			if (resNamespaceBaseNameEq( forceSetZmap, zeni->map_name)) {
				if( forceSetObject ) {
					for( objIt = 0; objIt != eaSize( &zeni->objects ); ++objIt ) {
						ZoneMapEncounterObjectInfo* object = zeni->objects[ objIt ];

						if( stricmp( forceSetObject, object->logicalName ) == 0 ) {
							selectedZeni = zeni;
							selectedObject = object;
							break;
						}
					}
				} else {
					selectedZeni = zeni;
					selectedObject = NULL;
				}
				break;
			}
		}
		
		if( selectedZeni ) {
			void* selectPath[ 3 ] = { 0 };
			selectPath[ 0 ] = selectedZeni;
			selectPath[ 1 ] = selectedObject;
			ui_TreeExpandAndSelect( picker->pickerTree, selectPath, false );

			if( out_setFound ) {
				*out_setFound = true;
			}
		}
	} else {
		UITreeNode* selectedNode = ui_TreeGetSelected( picker->pickerTree );
		
		if( selectedNode && selectedNode->table == parse_ZoneMapEncounterObjectInfo ) {
			selectedObject = selectedNode->contents;
		}
		while( selectedNode && selectedNode->table != parse_ZoneMapEncounterInfo ) {
			selectedNode = ui_TreeNodeFindParent( picker->pickerTree, selectedNode );
		}
		if( selectedNode ) {
			selectedZeni = selectedNode->contents;
		}
	}

	if( !selectedZeni && eaSize( &picker->zenis )) {
		void* selectPath[ 2 ] = { 0 };
		
		selectedZeni = picker->zenis[ 0 ];
		selectPath[ 0 ] = selectedZeni;
		ui_TreeExpandAndSelect( picker->pickerTree, selectPath, false );
	}

	// Refresh the minimap
	{
		bool mapChanged = false;
		ui_WidgetRemoveFromGroup( picker->minimapCustomWidget );
		picker->minimapCustomWidget = NULL;
		if( !selectedZeni ) {
			mapChanged = ui_MinimapSetMap( picker->minimap, NULL );
		} else if( picker->ugcInfoOverrides ) {
			ui_MinimapSetMapInfo( picker->minimap, selectedZeni );
			ui_MinimapSetScale( picker->minimap, selectedZeni->ugc_map_scale );
			if( selectedZeni->ugc_picker_widget ) {
				ui_ScrollAreaAddChild( picker->minimapArea, selectedZeni->ugc_picker_widget );
				picker->minimapCustomWidget = selectedZeni->ugc_picker_widget;
			}
		} else {
			mapChanged = ui_MinimapSetMap( picker->minimap, selectedZeni->map_name );
		}

		ui_MinimapClearObjects( picker->minimap );
		if( selectedZeni ) {
			int it;
			for( it = 0; it != eaSize( &selectedZeni->objects ); ++it ) {
				ZoneMapEncounterObjectInfo* object = selectedZeni->objects[ it ];
				const char* displayName = TranslateMessageRef( object->displayName );
				if( !displayName ) {
					displayName = object->ugcDisplayName;
				}
				if( !displayName ) {
					displayName = object->logicalName;
				}
			
				ui_MinimapAddObject( picker->minimap, object->pos, displayName, ugcZeniPickerObjectIcon( object ), object );
				if( selectedObject == object ) {
					ui_MinimapSetSelectedObject( picker->minimap, object, false );
				}
			}
		}

		if( selectionMakeVisible ) {
			bool sane_default_scroll_pos = false;
			if( mapChanged ) {
				ui_ScrollAreaSetChildScale( picker->minimapArea, 0.0001 );
			}
			
			if( selectedObject ) {
				FOR_EACH_IN_EARRAY(picker->minimap->objects, UIMinimapObject, minimapObject) {
					if (minimapObject->data == selectedObject) {
						Vec2 objectPos;
						ui_MinimapGetObjectPos( picker->minimap, minimapObject, objectPos );
						ui_ScrollAreaScrollToPosition( picker->minimapArea, objectPos[0], objectPos[1] );
						picker->minimapArea->autoScrollCenter = true;
						sane_default_scroll_pos = true;
						break;
					}
				} FOR_EACH_END;
			}

			if(!sane_default_scroll_pos)
			{
				ui_ScrollAreaScrollToPosition( picker->minimapArea, picker->minimap->layout_size[0] / 2, picker->minimap->layout_size[1] / 2 );
				picker->minimapArea->autoScrollCenter = true;
			}
		}
	}

	// Refresh the details
	{
		ui_WidgetGroupQueueFreeAndRemove( &picker->selectedDetails->widget.children );
		
		if( selectedObject ) {
			UIScrollArea* detailsScroll = NULL;
			const char* selectedDisplayName = NULL;
			UITextEntry* headerLabel = NULL;
			UILabel* restrictionsLabel = NULL;
			UILabel* detailsLabel = NULL;
			const char *details_str;
			UILabel* debugLabel = NULL;
			int yIt = 0;

			selectedDisplayName = selectedObject->ugcDisplayName;
			if( !selectedDisplayName ) {
				selectedDisplayName = TranslateMessageRef( selectedObject->displayName );
			}

			detailsScroll = ui_ScrollAreaCreate( 0, yIt, 0, 0, 0, 0, false, true );
			ui_WidgetSetDimensionsEx( UI_WIDGET( detailsScroll ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			detailsScroll->autosize = true;
			yIt = 0;

			{
				char* estrRestrictions = NULL;

				if( selectedObject->restrictions.iMinLevel || selectedObject->restrictions.iMaxLevel ) {
					estrConcatf( &estrRestrictions, "%s",
								 (!estrRestrictions ? TranslateMessageKey( "UGC_ZeniPicker.Restrictions" )
								  : TranslateMessageKey( "UGC_ZeniPicker.SeparatorRestrict" )));
					if( selectedObject->restrictions.iMinLevel && selectedObject->restrictions.iMaxLevel ) {
						ugcConcatMessageKey( &estrRestrictions, "UGC_ZeniPicker.LevelRestrict_MinMax",
											 STRFMT_INT( "Min", selectedObject->restrictions.iMinLevel ),
											 STRFMT_INT( "Max", selectedObject->restrictions.iMaxLevel ),
											 STRFMT_END );
					} else if( selectedObject->restrictions.iMinLevel ) {
						ugcConcatMessageKey( &estrRestrictions, "UGC_ZeniPicker.LevelRestrict_Min",
											 STRFMT_INT( "Min", selectedObject->restrictions.iMinLevel ),
											 STRFMT_END );
					} else if( selectedObject->restrictions.iMaxLevel ) {
						ugcConcatMessageKey( &estrRestrictions, "UGC_ZeniPicker.LevelRestrict_Max",
											 STRFMT_INT( "Max", selectedObject->restrictions.iMaxLevel ),
											 STRFMT_END );
					}
				}

				if( eaSize( &selectedObject->restrictions.eaFactions )) {
					int it;
					for( it = 0; it != eaSize( &selectedObject->restrictions.eaFactions ); ++it ) {
						const char* allegianceName = selectedObject->restrictions.eaFactions[ it ]->pcFaction;
						AllegianceDef* allegiance = allegiance_FindByName( allegianceName );

						if( allegiance ) {
							estrConcatf( &estrRestrictions, "%s",
										 (!estrRestrictions ? TranslateMessageKey( "UGC_ZeniPicker.Restrictions" )
										  : TranslateMessageKey( "UGC_ZeniPicker.SeparatorRestrict" )));
							ugcConcatMessageKey( &estrRestrictions, "UGC_ZeniPicker.FactionRestrict",
												 STRFMT_STRING( "Faction", TranslateDisplayMessage( allegiance->displayNameMsg )),
												 STRFMT_END );
						}
					}
				}

				if( estrRestrictions ) {
					restrictionsLabel = ui_LabelCreate( estrRestrictions, 0, yIt );
					yIt += 28;
				}
				estrDestroy( &estrRestrictions );
			}

			details_str = TranslateMessageRef( selectedObject->displayDetails );
			if (!details_str)
				details_str = selectedObject->ugcDisplayDetails;
			
			detailsLabel = ui_LabelCreate( details_str, 0, yIt );
			ui_LabelSetWordWrap( detailsLabel, true );
			ui_WidgetSetWidthEx( UI_WIDGET( detailsLabel ), 1.0, UIUnitPercentage );

			ui_PaneAddChild( picker->selectedDetails, headerLabel );
			ui_PaneAddChild( picker->selectedDetails, detailsScroll );
			if( restrictionsLabel ) {
				ui_ScrollAreaAddChild( detailsScroll, restrictionsLabel );
			}
			ui_ScrollAreaAddChild( detailsScroll, detailsLabel );
			if( debugLabel ) {
				ui_PaneAddChild( picker->selectedDetails, debugLabel );
			}
		}
	}
}

static void ugcZeniPickerClickFunc( UIMinimap* minimap, UserData rawPicker, UserData rawObject )
{
	UGCZeniPicker* picker = rawPicker;
	const ZoneMapEncounterObjectInfo* selectedObject = (ZoneMapEncounterObjectInfo*)rawObject;
	const char* selectedMapName = ui_MinimapGetMap( minimap );
	char* selectedObjectName = NULL;
	strdup_alloca( selectedObjectName, selectedObject->logicalName );

	ugcZeniPickerRefreshUI( picker, false, selectedMapName, selectedObjectName, NULL );
}

static void ugcZeniPickerSelect( UIWidget* ignored, UserData rawData )
{
	UGCZeniPickerWindow* data = rawData;

	if( data->cb ) {
		const char* mapName = NULL;
		const char* logicalName = NULL;
		ugcZeniPickerWidgetGetSelection( data->rootWidget, &mapName, &logicalName );
		data->cb( mapName, logicalName, NULL, NULL, data->userData );
	}

	--ugcZeniPickerWindowsOpen;
	assert( ugcZeniPickerWindowsOpen >= 0 );
	ui_WidgetQueueFreeAndNull( &data->window );
	free( data );
}

static void ugcZeniPickerClear( UIWidget* ignored, UserData rawData )
{
	UGCZeniPickerWindow* data = rawData;
	
	if( data->cb ) {
		data->cb( NULL, NULL, NULL, NULL, data->userData );
	}

	--ugcZeniPickerWindowsOpen;
	assert( ugcZeniPickerWindowsOpen >= 0 );
	ui_WidgetForceQueueFree( UI_WIDGET( data->window ));
	free( data );
}

static bool ugcZeniPickerClose( UIWidget* ignored, UserData rawData )
{
	ugcZeniPickerClear( NULL, rawData );
	return true;
}

static bool ugcZeniPickerFilterProject( const char* zmName, ZoneMapEncounterObjectInfo* object, UGCZeniPickerWindow* window )
{
	if( !zeniObjIsUGC( object )) {
		return false;
	}
	if( !resNamespaceBaseNameEq( zmName, window->defaultZmap )) {
		return false;
	}

	if( window->filterFn ) {
		return window->filterFn( zmName, object, window->filterData );
	} else {
		return true;
	}
}

static bool ugcZeniPickerFilterCryptic( const char* zmName, ZoneMapEncounterObjectInfo* object, UGCZeniPickerWindow* window )
{
	if( !zeniObjIsUGC( object )) {
		return false;
	}
	if( resNamespaceIsUGC( zmName )) {
		return false;
	}

	if( window->filterFn ) {
		return window->filterFn( zmName, object, window->filterData );
	} else {
		return true;
	}
}

bool ugcZeniPickerShow(ZoneMapEncounterInfo **ugcInfoOverrides,
					   UGCZeniPickerFilterType forceFilterType,
					   const char* defaultZmap, const char* defaultObj,
					   UGCZeniPickerFilterFn filterFn, UserData filterData,
					   UGCZeniPickerCallback cb, UserData userData )
{
	UGCZeniPickerWindow* data = calloc( 1, sizeof( *data ));
	UIButton* okButton;
	UIButton* cancelButton;

	data->defaultZmap = allocAddString( defaultZmap );
	data->filterFn = filterFn;
	data->filterData = filterData;

	data->window = ui_WindowCreate( "", 0, 0, 900, 700 );
	if( forceFilterType == UGCZeniPickerType_WholeMap ) {
		ui_WidgetSetTextMessage( UI_WIDGET( data->window ), "UGC_ZeniPicker.SelectMap" );
	} else {
		ui_WidgetSetTextMessage( UI_WIDGET( data->window ), "UGC_ZeniPicker.SelectComponent" );
	}
	++ugcZeniPickerWindowsOpen;
	ui_WindowSetCloseCallback( data->window, ugcZeniPickerClose, data );

	if( ugcInfoOverrides ) {
		data->rootWidget = ugcZeniPickerWidgetCreate( NULL, ugcInfoOverrides, forceFilterType, defaultZmap, defaultObj, ugcZeniPickerFilterProject, data );
	} else {
		data->rootWidget = ugcZeniPickerWidgetCreate( NULL, NULL, forceFilterType, defaultZmap, defaultObj, ugcZeniPickerFilterCryptic, data );
	}

	if (!data->rootWidget)
	{
		ugcZeniPickerClear( NULL, data );
		return false;
	}
	ui_WidgetSetDimensionsEx(data->rootWidget, 1, 1, UIUnitPercentage, UIUnitPercentage);
	ui_WindowAddChild(data->window, data->rootWidget);
	
	okButton = ui_ButtonCreate( "", 0, 0, ugcZeniPickerSelect, data );
	ui_ButtonSetMessage( okButton, "UGC.Ok" );
	ui_WidgetSetPositionEx( UI_WIDGET( okButton ), 0, 0, 0, 0, UIBottomRight );
	ui_WidgetSetWidth( UI_WIDGET( okButton ), 80 );

	cancelButton = ui_ButtonCreate( "", 0, 0, ugcZeniPickerClear, data );
	ui_ButtonSetMessage( cancelButton, "UGC.Cancel" );
	ui_WidgetSetPositionEx( UI_WIDGET( cancelButton ), ui_WidgetGetNextX( UI_WIDGET( okButton )), 0, 0, 0, UIBottomRight );
	ui_WidgetSetWidth( UI_WIDGET( cancelButton ), 80 );
	data->rootWidget->bottomPad = UI_WIDGET(cancelButton)->height+5;
	
	ui_WindowSetDimensions( data->window, 900, 700, 450, 350 );
	elUICenterWindow( data->window );
	if( g_ui_State.bInUGCEditor ) {
		ui_WindowSetModal( data->window, true );
	}

	ui_WindowAddChild( data->window, okButton );
	ui_WindowAddChild( data->window, cancelButton );
	ui_WindowShowEx( data->window, true );

	data->cb = cb;
	data->userData = userData;

	return true;
}

static void ugcZeniPickerOverworldMapSelect( UIWidget* ignored, UserData rawData )
{
	UGCZeniPickerOverwoldMapWindow* data = rawData;
	Vec2 vec = { 0, 0 };
	const char* icon = NULL;
	ugcZeniPickerOverworldMapWidgetGetSelection( data->overworldMapWidget, vec, &icon );
	data->cb( NULL, NULL, vec, icon, data->userData );
	ui_WidgetQueueFreeAndNull( &data->window );
	free( data );
}

static void ugcZeniPickerOverworldMapClear( UIWidget* ignored, UserData rawData )
{
	UGCZeniPickerOverwoldMapWindow* data = rawData;
	
	if( data->cb ) {
		data->cb( NULL, NULL, NULL, NULL, data->userData );
	}

	--ugcZeniPickerWindowsOpen;
	assert( ugcZeniPickerWindowsOpen >= 0 );
	ui_WidgetForceQueueFree( UI_WIDGET( data->window ));
	free( data );
}

static bool ugcZeniPickerOverworldMapClose( UIWidget* ignored, UserData rawData )
{
	ugcZeniPickerOverworldMapClear( NULL, rawData );
	return true;
}

static void ugcZeniPickerOverworldMapChanged( UGCZeniPickerOverwoldMapWindow* data )
{
	Vec2 vec = { 0, 0 };
	const char* icon = NULL;
	ugcZeniPickerOverworldMapWidgetGetSelection( data->overworldMapWidget, vec, &icon );

	if( vec[ 0 ] >= 0 && vec[ 1 ] >= 0 && icon ) {
		ui_SetActive( UI_WIDGET( data->okButton ), true );
	} else {
		ui_SetActive( UI_WIDGET( data->okButton ), false );
	}
}

void ugcZeniPickerOverworldMapShow(const char** eaIconNames, float* defaultPos, const char* defaultIcon,
								   UGCZeniPickerCallback cb, UserData userData )
{
	UGCZeniPickerOverwoldMapWindow* data = calloc( 1, sizeof( *data ));
	UIButton* cancelButton;

	data->window = ui_WindowCreate( "", 0, 0, 900, 700 );
	ui_WidgetSetTextMessage( UI_WIDGET( data->window ), "UGC_ZeniPicker.SelectLocation" );
	ui_WindowSetCloseCallback( data->window, ugcZeniPickerOverworldMapClose, data );
	data->overworldMapWidget = ugcZeniPickerOverworldMapWidgetCreate( eaIconNames, defaultPos, defaultIcon, ugcZeniPickerOverworldMapChanged, data );
	ui_WindowAddChild( data->window, data->overworldMapWidget );
	assert( data->overworldMapWidget );
	++ugcZeniPickerWindowsOpen;
	
	data->okButton = ui_ButtonCreate( "", 0, 0, ugcZeniPickerOverworldMapSelect, data );
	ui_ButtonSetMessage( data->okButton, "UGC.Ok" );
	ui_WidgetSetPositionEx( UI_WIDGET( data->okButton ), 0, 0, 0, 0, UIBottomRight );
	ui_WidgetSetWidth( UI_WIDGET( data->okButton ), 80 );

	cancelButton = ui_ButtonCreate( "", 0, 0, ugcZeniPickerOverworldMapClear, data );
	ui_ButtonSetMessage( cancelButton, "UGC.Cancel" );
	ui_WidgetSetPositionEx( UI_WIDGET( cancelButton ), ui_WidgetGetNextX( UI_WIDGET( data->okButton )), 0, 0, 0, UIBottomRight );
	ui_WidgetSetWidth( UI_WIDGET( cancelButton ), 80 );
	data->overworldMapWidget->bottomPad = UI_WIDGET(cancelButton)->height+5;
	
	ui_WindowSetDimensions( data->window, 900, 700, 450, 350 );
	elUICenterWindow( data->window );
	ui_WindowSetModal( data->window, true );

	ui_WindowAddChild( data->window, data->okButton );
	ui_WindowAddChild( data->window, cancelButton );
	ui_WindowShowEx( data->window, true );

	data->cb = cb;
	data->userData = userData;

	ugcZeniPickerOverworldMapChanged( data );
}

static void ugcZeniPickerHideInvalidToggled( UICheckButton* ignored, UserData rawPicker )
{
	UGCZeniPicker* picker = rawPicker;
	ugcZeniPickerRefreshUI( picker, false, NULL, NULL, NULL );
}

static void ugcZeniPickerTreeSelected( UITreeNode* node, UserData rawPicker )
{
	UGCZeniPicker* picker = rawPicker;
	ugcZeniPickerRefreshUI( picker, true, NULL, NULL, NULL );
}

static void ugcZeniPickerTreeDisplayObject( UITreeNode* node, UserData ignored, UI_MY_ARGS, F32 z )
{
	UITree* tree = node->tree;
	ZoneMapEncounterObjectInfo* zeniInfo = node->contents;
	CBox box = { x, y, x + w, y + h };
	bool bSelected = ui_TreeIsNodeSelected( node->tree, node );
	bool bHover = false;
	UIStyleFont* font = ui_TreeItemGetFont( node->tree, bSelected, bHover );

	clipperPushRestrict( &box );
	{
		const char* name;

		if( g_ui_State.bInUGCEditor ) {
			name = TranslateMessageRef( zeniInfo->displayName );
		} else {
			name = zeniInfo->logicalName;
		}
		if( !name ) {
			name = zeniInfo->ugcDisplayName;
		}
		if( !name ) {
			name = "UNTRANSLATED";
		}

		ui_StyleFontUse( font, bSelected, UI_WIDGET( node->tree )->state );
		ui_DrawTextInBoxSingleLine( font, name, true, &box, z, scale, UILeft );
	}
	clipperPop();
}

static void ugcZeniPickerTreeFillMap( UITreeNode* mapNode, UserData ignored )
{
	ZoneMapEncounterInfo* zmEncInfo = mapNode->contents;
	int it;

	for( it = 0; it != eaSize( &zmEncInfo->objects ); ++it ) {
		ZoneMapEncounterObjectInfo* zeniInfo = zmEncInfo->objects[ it ];
		
		UITreeNode* node = ui_TreeNodeCreate( mapNode->tree, hashString( zeniInfo->logicalName, false ), parse_ZoneMapEncounterObjectInfo, zeniInfo,
											  NULL, NULL, ugcZeniPickerTreeDisplayObject, NULL,
											  20 );
		ui_TreeNodeAddChild( mapNode, node );
	}
}

static void ugcZeniPickerTreeDisplayMap( UITreeNode* node, UserData ignored, UI_MY_ARGS, F32 z )
{
	UITree* tree = node->tree;
	ZoneMapEncounterInfo* zmEncInfo = node->contents;
	ZoneMapInfo* zminfo = zmapInfoGetByPublicName( zmEncInfo->map_name );
	CBox box = { x, y, x + w, y + h };
	bool bSelected = ui_TreeIsNodeSelected( node->tree, node );
	bool bHover = false;
	UIStyleFont* font = ui_TreeItemGetFont( node->tree, bSelected, bHover );

	clipperPushRestrict( &box );
	{
		const char* name;

		if( g_ui_State.bInUGCEditor ) {
			name = TranslateMessagePtr( zmapInfoGetDisplayNameMessagePtr( zminfo ));
		} else {
			name = "UNTRANSLATED";
		}
		
		if( !name ) {
			name = zmEncInfo->ugc_display_name;
		}
		if( !name ) {
			name = zmEncInfo->map_name;
		}

		ui_StyleFontUse( font, bSelected, UI_WIDGET( node->tree )->state );
		ui_DrawTextInBoxSingleLine( font, name, true, &box, z, scale, UILeft );
	}
	clipperPop();
}

static void ugcZeniPickerTreeFillRoot( UITreeNode* rootNode, UserData rawPicker )
{
	UGCZeniPicker* picker = rawPicker;
	int it;
	for( it = 0; it != eaSize( &picker->zenis ); ++it ) {
		ZoneMapEncounterInfo* zeni = picker->zenis[ it ];
		UITreeNode* node = ui_TreeNodeCreate( rootNode->tree, hashString( zeni->map_name, false ), parse_ZoneMapEncounterInfo, zeni,
											  (eaSize( &zeni->objects ) ? ugcZeniPickerTreeFillMap : NULL),
											  NULL, ugcZeniPickerTreeDisplayMap, NULL,
											  20 );
		ui_TreeNodeAddChild( rootNode, node );
	}
}

static void ugcZeniPickerWidgetFree( UIWidget* widget )
{
	UGCZeniPicker* picker = (UGCZeniPicker*)(widget->u64);
	
	ui_PaneFreeInternal( (UIPane*)widget );
	eaDestroyStruct( &picker->ugcInfoOverrides, parse_ZoneMapEncounterInfo );
	ea32Destroy( &picker->filterValidTypes );
	eaDestroyStruct( &picker->zenis, parse_ZoneMapEncounterInfo );
	free( picker );
}

UIWidget* ugcZeniPickerWidgetCreate(bool* out_selectedDefault, ZoneMapEncounterInfo **ugcInfoOverrides,
									UGCZeniPickerFilterType forceFilterType,
									const char* defaultZmap, const char* defaultObj,
									UGCZeniPickerFilterFn filterFn, UserData filterData)
{
	UGCZeniPicker* pickerAccum = calloc( 1, sizeof( *pickerAccum ));
	
	{
		UIPane* rootPane;
		UICheckButton* pickerHideInvalidCheck;
		UITree* pickerTree;
				
		UIScrollArea* minimapArea;
		UISprite* sprite;
		UIMinimap* minimap;
		UIPane* selectedDetails;

		rootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
		rootPane->invisible = true;

		if( forceFilterType ) {
			ea32Push( &pickerAccum->filterValidTypes, forceFilterType );
		}

		pickerHideInvalidCheck = ui_CheckButtonCreate( 0, 0, "---", true );
		ui_CheckButtonSetToggledCallback( pickerHideInvalidCheck, ugcZeniPickerHideInvalidToggled, pickerAccum );
		ui_PaneAddChild( rootPane, pickerHideInvalidCheck );

		// Initialize the tree to check if we have any matching maps
		pickerTree = ui_TreeCreate( 0, STANDARD_ROW_HEIGHT, 300, 1 );
		ui_WidgetSetHeightEx( UI_WIDGET( pickerTree ), 1, UIUnitPercentage );
		ui_TreeNodeSetFillCallback( &pickerTree->root, ugcZeniPickerTreeFillRoot, pickerAccum );
		ui_TreeSetSelectedCallback( pickerTree, ugcZeniPickerTreeSelected, pickerAccum );
		ui_TreeNodeExpand( &pickerTree->root );
		ui_PaneAddChild( rootPane, pickerTree );

		minimapArea = ui_ScrollAreaCreate( 0, 0, 0, 0, 0, 0, true, true );
		UI_WIDGET(minimapArea)->sb->scrollBoundsX = UIScrollBounds_KeepContentsAtViewCenter;
		UI_WIDGET(minimapArea)->sb->scrollBoundsY = UIScrollBounds_KeepContentsAtViewCenter;
		ui_ScrollAreaSetNoCtrlDraggable( minimapArea, true );
		ui_ScrollAreaSetZoomSlider( minimapArea, true );
		ui_WidgetSetDimensionsEx( UI_WIDGET( minimapArea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( minimapArea ), 304, 0, 0, 164 );
		minimapArea->autosize = true;
		ui_PaneAddChild( rootPane, minimapArea );

		sprite = ui_SpriteCreate( 0, 0, 1, 1, "white" );
		sprite->tint = ColorBlack;
		ui_WidgetSetDimensionsEx( UI_WIDGET( sprite ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		sprite->widget.bNoScrollX = true;
		sprite->widget.bNoScrollY = true;
		ui_ScrollAreaAddChild( minimapArea, sprite );

		minimap = ui_MinimapCreate();
		ui_MinimapSetObjectClickCallback( minimap, ugcZeniPickerClickFunc, pickerAccum );
		ui_WidgetSetPositionEx( UI_WIDGET( minimap ), 0, 0, 0, 0, UITopLeft );
		minimap->autosize = true;
		minimap->widget.priority = 5;
		ui_ScrollAreaAddChild( minimapArea, minimap );

		selectedDetails = ui_PaneCreate( 0, 0, 1, 160, UIUnitPercentage, UIUnitFixed, 0 );
		ui_WidgetSetPositionEx( UI_WIDGET( selectedDetails ), 304, 0, 0, 0, UIBottomLeft );
		ui_PaneSetStyle( selectedDetails, "UGC_Pane_ContentArea", true, false );
		ui_PaneAddChild( rootPane, selectedDetails );

		pickerAccum->rootPane = rootPane;
		pickerAccum->pickerTree = pickerTree;
		pickerAccum->pickerHideInvalidCheck = pickerHideInvalidCheck;
		pickerAccum->minimapArea = minimapArea;
		pickerAccum->minimap = minimap;
		pickerAccum->selectedDetails = selectedDetails;
	}

	pickerAccum->filterFn = filterFn;
	pickerAccum->filterData = filterData;
	eaCopyStructs( &ugcInfoOverrides, &pickerAccum->ugcInfoOverrides, parse_ZoneMapEncounterInfo );
	
	ugcZeniPickerRefreshUI( pickerAccum, true, defaultZmap, defaultObj, out_selectedDefault );

	pickerAccum->rootPane->widget.u64 = (U64)pickerAccum;
	pickerAccum->rootPane->widget.freeF = ugcZeniPickerWidgetFree;

	if (eaSize(&pickerAccum->pickerTree->root.children) == 0)
	{
		// We have no maps. Bail and return NULL
		ugcZeniPickerWidgetFree(&pickerAccum->rootPane->widget);
		return NULL;
	}
	else if (eaSize(&pickerAccum->pickerTree->root.children) == 1)
	{
		// If there is only one map, expand it.
		// But if we have a defaultZmap and defaultObj we should already have set up our selected object and expanded the map
		//   it is within. Do not call ui_TreeNodeExpand again as it will undo that work and leave us with nothing selected.
		if (!(defaultZmap && defaultObj))
		{
			ui_TreeNodeExpand(pickerAccum->pickerTree->root.children[0]);
		}
	}

	return UI_WIDGET( pickerAccum->rootPane );
}

void ugcZeniPickerWidgetGetSelection( UIWidget* widget, const char** out_mapName, const char** out_logicalName )
{
	UGCZeniPicker* picker = (UGCZeniPicker*)widget->u64;
	ZoneMapEncounterInfo* selectedZeni = NULL;
	ZoneMapEncounterObjectInfo* selectedObject = NULL;
	bool isUsingWholeMap = eaiSize( &picker->filterValidTypes ) == 1 && picker->filterValidTypes[ 0 ] == UGCZeniPickerType_WholeMap;

	UITreeNode* selectedNode = ui_TreeGetSelected( picker->pickerTree );
	if( selectedNode && selectedNode->table == parse_ZoneMapEncounterObjectInfo ) {
		selectedObject = selectedNode->contents;
	}
	while( selectedNode && selectedNode->table != parse_ZoneMapEncounterInfo ) {
		selectedNode = ui_TreeNodeFindParent( picker->pickerTree, selectedNode );
	}
	if( selectedNode ) {
		selectedZeni = selectedNode->contents;
	}

	if( selectedZeni && selectedObject ) {
		*out_mapName = selectedZeni->map_name;
		*out_logicalName = selectedObject->logicalName;
	} else if( selectedZeni && isUsingWholeMap ) {
		*out_mapName = selectedZeni->map_name;
	} else {
		*out_mapName = NULL;
		*out_logicalName = NULL;
	}
}

void ugcZeniPickerOverworldMapApplyData( UGCZeniPickerOverworldMap* picker )
{
	Vec2 iconPos;

	if( picker->iconPosition[0] >= 0 && picker->iconPosition[1] >= 0 ) {
		iconPos[0] = picker->iconPosition[0];
		iconPos[1] = picker->iconPosition[1];
	} else {
		setVec2( iconPos, 0.5, 0.5 );
	}

	ui_ZeniPickerIconSetIcon( picker->icon, picker->iconName );
	ui_ListSetSelectedObject( picker->iconList, allocAddString( NULL_TO_EMPTY( picker->iconName )));
	ui_WidgetSetPositionEx( UI_WIDGET( picker->icon ), -22, -22, iconPos[0], iconPos[1], UITopLeft );

	if( picker->changedFn ) {
		picker->changedFn( picker->changedData );
	}
}

static void ugcZeniPickerOverworldMapWidgetFree( UIWidget* widget )
{
	UGCZeniPickerOverworldMap* picker = (UGCZeniPickerOverworldMap*)(widget->u64);

	ui_PaneFreeInternal( picker->rootPane );
	free( picker );
}

static void ugcZeniPickerOverworldMapIconPreDrag( UIWidget* widget, UGCZeniPickerOverworldMap* picker )
{
	ui_SetCursorByName( "UGC_Cursors_Move_Pointer" );
	ui_CursorLock();
}

static void ugcZeniPickerOverworldMapIconPositionToMouseCoords( UGCZeniPickerOverworldMap* picker, const Vec2 iconPos, int* out_mouseCoords )
{
	out_mouseCoords[0] = picker->mapLastTopLeft[0] + iconPos[0] * picker->mapLastWidth;
	out_mouseCoords[1] = picker->mapLastTopLeft[1] + iconPos[1] * picker->mapLastHeight;
}

static void ugcZeniPickerOverworldMapIconMouseCoordsToPosition( UGCZeniPickerOverworldMap* picker, const int* mouseCoords, Vec2 out_iconPos )
{
	setVec2( out_iconPos,
			 CLAMP( (mouseCoords[0] - picker->mapLastTopLeft[0]) / picker->mapLastWidth, 0, 1 ),
			 CLAMP( (mouseCoords[1] - picker->mapLastTopLeft[1]) / picker->mapLastHeight, 0, 1 ));
}

static void ugcZeniPickerOverworldMapIconDrag( UIWidget* widget, UGCZeniPickerOverworldMap* picker )
{
	int mouse[2];
	if( picker->iconPosition[0] >= 0 && picker->iconPosition[1] >= 0 ) {
		ugcZeniPickerOverworldMapIconPositionToMouseCoords( picker, picker->iconPosition, mouse );
	} else {
		Vec2 center = { 0.5, 0.5 };
		ugcZeniPickerOverworldMapIconPositionToMouseCoords( picker, center, mouse );
	}
	mouseSetScreen( mouse[0], mouse[1] );
	ui_DragStartEx( widget, "overworld_map_icon_drag", NULL, atlasFindTexture( "alpha8x8" ), 0xFFFFFFFF, true, "UGC_Cursors_Move" );

	picker->isDragging = true;
}

static void ugcZeniPickerOverworldMapIconDragEnd( UIWidget* dragWidget, UIWidget* ignoredDest, UIDnDPayload* ignored, UGCZeniPickerOverworldMap* picker )
{
	int mouse[2];
	mousePos( &mouse[0], &mouse[1] );
	ugcZeniPickerOverworldMapIconMouseCoordsToPosition( picker, mouse, picker->iconPosition );
	picker->isDragging = false;

	ugcZeniPickerOverworldMapApplyData( picker );
}

static void ugcZeniPickerOverworldMapIconPickerMapTick( UISprite* sprite, UI_PARENT_ARGS )
{
	UGCZeniPickerOverworldMap* picker = (UGCZeniPickerOverworldMap*)sprite->widget.u64;
	UI_GET_COORDINATES( sprite );
	AtlasTex* texture = atlasLoadTexture( ui_WidgetGetText( UI_WIDGET( sprite )));
	assert( texture );
	
	if( sprite->bPreserveAspectRatio ) {
		float aspectRatio = CBoxWidth( &box ) / CBoxHeight( &box );
		float boxCenterX;
		float boxCenterY;
		float spriteWidth = 1;
		float spriteHeight = 1;

		CBoxGetCenter( &box, &boxCenterX, &boxCenterY );
		spriteWidth = texture->width;
		spriteHeight = texture->height;

		if( aspectRatio > spriteWidth / spriteHeight ) {
			float height = CBoxHeight( &box );
			BuildCBoxFromCenter( &box, boxCenterX, boxCenterY, height / spriteHeight * spriteWidth, height );
		} else {
			float width = CBoxWidth( &box );
			BuildCBoxFromCenter( &box, boxCenterX, boxCenterY, width, width / spriteWidth * spriteHeight );
		}
	}
	picker->mapLastTopLeft[0] = box.lx;
	picker->mapLastTopLeft[1] = box.ly;
	picker->mapLastWidth = box.hx - box.lx;
	picker->mapLastHeight = box.hy - box.ly;
	
	ui_SpriteTick( sprite, UI_PARENT_VALUES );
}

static void ugcZeniPickerOverworldMapListDrawIcon(UIList *pList, UIListColumn *pColumn, UI_MY_ARGS, F32 z, CBox *pLogicalBox, S32 iRow, UserData pDrawData)
{
	const char* iconName = eaGet( pList->peaModel, iRow );
	if( iconName ) {
		AtlasTex* icon = atlasLoadTexture( iconName );
		CBox iconBox;
		const char* iconDisplayName = NULL;
		char iconDisplayNameBuffer[ 256 ];

		iconBox.lx = x + 2;
		iconBox.ly = y + 4;
		iconBox.hy = y + h - 4;
		iconBox.hx = iconBox.lx + iconBox.hy - iconBox.ly;
		display_sprite_box( icon, &iconBox, z, -1 );

		if( g_ui_State.bInUGCEditor ) {
			const WorldUGCProperties* iconProps = ugcResourceGetUGCProperties( "Texture", iconName );
			if( iconProps ) {
				iconDisplayName = TranslateDisplayMessage( iconProps->dVisibleName );
			}

			if( !iconDisplayName ) {
				sprintf( iconDisplayNameBuffer, "%s (UNTRANSLATED)", iconName );
				iconDisplayName = iconDisplayNameBuffer;
			}
		} else {
			iconDisplayName = iconName;
		}
		gfxfont_PrintMaxWidth( iconBox.hx + 2, (iconBox.ly + iconBox.hy) / 2, z, x + w - iconBox.hx - 4, scale, scale, CENTER_Y,
							   iconDisplayName );
	}
}

static void ugcZeniPickerOverworldMapWidgetIconChanged( UIList* pList, UGCZeniPickerOverworldMap* picker )
{
	const char* iconName = ui_ListGetSelectedObject( pList );
	if( iconName ) {
		picker->iconName = allocAddString( iconName );
	}
	
	ugcZeniPickerOverworldMapApplyData( picker );
}

UIWidget* ugcZeniPickerOverworldMapWidgetCreate( const char** eaIconNames, const float* defaultPos, const char* defaultIcon, UGCZeniPickerChangedFn changedFn, UserData changedData )
{
	UGCZeniPickerOverworldMap* pickerAccum = calloc( 1, sizeof( *pickerAccum ));
	
	UIPane* rootPane;
		UIList* iconList;
		UIPane* mapArea;
			UISprite* map;
				UIWidgetWidget* icon;

	{
		int it;
		for( it = 0; it != eaSize( &eaIconNames ); ++it ) {
			eaPush( &pickerAccum->eaIconNames, allocAddString( eaIconNames[ it ]));
		}
	}

	rootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	rootPane->invisible = true;

	iconList = ui_ListCreate( NULL, (char***)&pickerAccum->eaIconNames, 32 );
	iconList->fHeaderHeight = 0;	
	ui_ListAppendColumn( iconList, ui_ListColumnCreateCallback( "Icon", ugcZeniPickerOverworldMapListDrawIcon, NULL ));
	ui_ListSetSelectedCallback( iconList, ugcZeniPickerOverworldMapWidgetIconChanged, pickerAccum );
	ui_WidgetSetDimensionsEx( UI_WIDGET( iconList ), 300, 1, UIUnitFixed, UIUnitPercentage );
	ui_PaneAddChild( rootPane, iconList );

	mapArea = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( mapArea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( mapArea ), 304, 0, 0, 0 );
	mapArea->invisible = true;
	ui_PaneAddChild( rootPane, mapArea );

	map = ui_SpriteCreate( 0, 0, 1, 1, "World_Map" );
	map->widget.tickF = ugcZeniPickerOverworldMapIconPickerMapTick;
	map->widget.u64 = (U64)pickerAccum;
	ui_WidgetSetDimensionsEx( UI_WIDGET( map ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	map->bPreserveAspectRatio = true;
	map->bChildrenUseDrawBox = true;
	ui_PaneAddChild( mapArea, map );

	icon = ui_ZeniPickerIconCreate( pickerAccum );
	ui_WidgetAddChild( UI_WIDGET( map ), UI_WIDGET( icon ));
	assert( !icon->widget.preDragF && !icon->widget.dragF && !icon->widget.dropF );
	ui_WidgetSetPreDragCallback( UI_WIDGET( icon ), ugcZeniPickerOverworldMapIconPreDrag, pickerAccum );
	ui_WidgetSetDragCallback( UI_WIDGET( icon ), ugcZeniPickerOverworldMapIconDrag, pickerAccum );
	ui_WidgetSetAcceptCallback( UI_WIDGET( icon ), ugcZeniPickerOverworldMapIconDragEnd, pickerAccum );

	rootPane->widget.u64 = (U64)pickerAccum;
	rootPane->widget.freeF = ugcZeniPickerOverworldMapWidgetFree;
	pickerAccum->rootPane = rootPane;
	pickerAccum->iconList = iconList;
	pickerAccum->icon = icon;

	if( defaultPos ) {
		copyVec2( defaultPos, pickerAccum->iconPosition );
	} else {
		setVec2( pickerAccum->iconPosition, 0.5, 0.5 );
	}
	pickerAccum->iconName = allocAddString( defaultIcon );
	ugcZeniPickerOverworldMapApplyData( pickerAccum );

	// Make sure this is after the call to ApplyData, so changedFn
	// doesn't get called during startup.
	pickerAccum->changedFn = changedFn;
	pickerAccum->changedData = changedData;

	return UI_WIDGET( rootPane );
}

UIWidgetWidget* ui_ZeniPickerIconCreate( UGCZeniPickerOverworldMap* picker )
{
	UIWidgetWidget* accum = calloc( 1, sizeof( *accum ));
	ui_WidgetInitialize( UI_WIDGET( accum ), ui_ZeniPickerIconTick, ui_ZeniPickerIconDraw, ui_ZeniPickerIconFree, NULL, NULL );
	ui_WidgetSetDimensions( UI_WIDGET( accum ), 44, 44 );
	accum->widget.u64 = (U64)picker;

	return accum;
}

void ui_ZeniPickerIconSetIcon( UIWidgetWidget* widget, const char* iconName )
{
	ui_WidgetSetTextString( UI_WIDGET( widget ), iconName );
}

void ui_ZeniPickerIconTick( UIWidgetWidget* icon, UI_PARENT_ARGS )
{
	UGCZeniPickerOverworldMap* picker = (UGCZeniPickerOverworldMap*)UI_WIDGET( icon )->u64;
	UI_GET_COORDINATES( icon );

	UI_TICK_EARLY( icon, true, true );
	UI_TICK_LATE( icon );
}

void ui_ZeniPickerIconDraw( UIWidgetWidget* icon, UI_PARENT_ARGS )
{
	UGCZeniPickerOverworldMap* picker = (UGCZeniPickerOverworldMap*)UI_WIDGET( icon )->u64;
	AtlasTex* iconTex;
	AtlasTex* maskTex;
	AtlasTex* layerTex;
	UITextureAssembly* bgTexas;
	CBox iconBox;
	UI_GET_COORDINATES( icon );

	if( picker && picker->isDragging ) {
		int mouse[2];
		Vec2 dragPosition;
		mousePos( &mouse[0], &mouse[1] );
		ugcZeniPickerOverworldMapIconMouseCoordsToPosition( picker, mouse, dragPosition );
	
		x = floorf( pX + UI_WIDGET( icon )->x * pScale + dragPosition[ 0 ] * pW );
		y = floorf( pY + UI_WIDGET( icon )->y * pScale + dragPosition[ 1 ] * pH );
		BuildCBox( &box, x, y, w, h );
		ui_SoftwareCursorThisFrame();
	}

	if( ui_WidgetGetText( UI_WIDGET( icon ))) {
		iconTex = atlasFindTexture( ui_WidgetGetText( UI_WIDGET( icon )));
	} else {
		iconTex = atlasFindTexture( "MapIcon_QuestionMark_01" );
	}
	maskTex = atlasFindTexture( "Power_Icon_Round_Mask" );
	if( picker && (picker->isDragging || ui_IsHovering( UI_WIDGET( icon ))) ) {
		layerTex = atlasFindTexture( "HUD_Playerframe_Powers_Glow_Over" );
		bgTexas = RefSystem_ReferentFromString( "UITextureAssembly", "Overworld_Map_Location_MouseOver" );
	} else {
		layerTex = atlasFindTexture( "HUD_Playerframe_Powers_Glow_Idle" );
		bgTexas = RefSystem_ReferentFromString( "UITextureAssembly", "Overworld_Map_Location_Idle" );
	}

	UI_DRAW_EARLY( icon );
	ui_TextureAssemblyDraw( bgTexas, &box, &iconBox, 1, z, z + 0.1, 255, NULL );
	display_sprite_box_mask( iconTex, maskTex, &iconBox, z + 0.1, -1 );
	display_sprite_box( layerTex, &iconBox, z + 0.2, -1 );
	UI_DRAW_LATE( icon );
}

void ui_ZeniPickerIconFree( UIWidgetWidget* icon )
{
	ui_WidgetFreeInternal( UI_WIDGET( icon ));
}

void ugcZeniPickerOverworldMapWidgetGetSelection( UIWidget* widget, float* out_mapPos, const char** out_icon )
{
	UGCZeniPickerOverworldMap* picker = (UGCZeniPickerOverworldMap*)widget->u64;

	if( picker->iconPosition[ 0 ] >= 0 && picker->iconPosition[ 1 ] >= 0 ) {
		copyVec2( picker->iconPosition, out_mapPos );
	} else {
		setVec2( out_mapPos, 0.5, 0.5 );
	}
	*out_icon = picker->iconName;
}

bool ugcZeniPickerWindowOpen( void )
{
	return ugcZeniPickerWindowsOpen > 0;
}

#include "NNOUGCZeniPicker_h_ast.c"

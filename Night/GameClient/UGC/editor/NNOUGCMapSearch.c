#include"NNOUGCMapSearch.h"

#include"GfxSprite.h"
#include"GfxSpriteText.h"
#include"GfxTexAtlas.h"
#include"NNOUGCCommon.h"
#include"NNOUGCEditorPrivate.h"
#include"NNOUGCInteriorCommon.h"
#include"NNOUGCMapEditor.h"
#include"NNOUGCMapEditorWidgets.h"
#include"NNOUGCMissionCommon.h"
#include"NNOUGCResource.h"
#include"ObjectLibrary.h"
#include"StringCache.h"
#include"StringFormat.h"
#include"StringUtil.h"
#include"UGCError.h"
#include"UGCInteriorCommon.h"
#include"UIComboBox.h"
#include"UIPane.h"
#include"UITextEntry.h"
#include"UITree.h"
#include"UIWindow.h"

AUTO_ENUM;
typedef enum UGCMapSearchSortType
{
	UGCMapSearchSortType_ByType,		ENAMES("UGC_MapEditor.SortByType")
	UGCMapSearchSortType_ByName,		ENAMES("UGC_MapEditor.SortByName")
	UGCMapSearchSortType_ByUse,			ENAMES("UGC_MapEditor.SortByUsage")
	UGCMapSearchSortType_ByHasError,	ENAMES("UGC_MapEditor.SortByHasError")
} UGCMapSearchSortType;
extern StaticDefineInt UGCMapSearchSortTypeEnum[];

typedef struct UGCMapSearchNode
{
	U32 uComponentID;
	int iLevelID;
	bool isPatrolRouteParent;

	const char* astrIconName;
	char* strAssetBrowserText;

	UISprite *pErrorSprite;
	bool hasMissionLink;
	bool hasTriggerLink;

	struct UGCMapSearchNode *pParent;
	struct UGCMapSearchNode **eaChildren;
} UGCMapSearchNode;

typedef struct UGCMapSearchWidget
{
	char* strMapName;
	UIPane* rootPane;
	UIComboBox* sortCombo;
	UITextEntry* filterEntry;
	UITree* tree;

	UGCMapSearchNode rootNode;
} UGCMapSearchWidget;

static void ugcMapSearchWidgetFree( UIWidget* widget );
static void ugcMapSearchWidgetRefresh( UGCMapSearchWidget* searchWidget );
static void ugcMapSearchSortChangedCB( UIComboBox* box, int value, UserData rawSearchWidget );
static void ugcMapSearchFilterChangedCB( UITextEntry* entry, UserData rawSearchWidget );
static void ugcMapSearchFillRootCB( UITreeNode* parent, UserData ignored );
static void ugcMapSearchFillMapDocNodeCB( UITreeNode* parent, UGCMapSearchNode* parentNode );
static void ugcMapSearchTreeNodeDisplayCB( UITreeNode *node, UserData rawGroup, UI_MY_ARGS, F32 z );
static void ugcMapSearchTreeSelectedCB( UITree* tree, UserData rawSearchWidget );
static void ugcMapSearchNodeFillComponent( UGCMapSearchWidget* searchWidget, UGCMapSearchNode* node, UGCComponent* component );
static U32 ugcMapSearchNodeCRC( UGCMapSearchNode *node );
static int ugcMapSearchSortNodes( UGCMapSearchWidget* searchWidget, const UGCMapSearchNode **nodeA, const UGCMapSearchNode **nodeB );

AUTO_RUN_ANON( memBudgetAddMapping( __FILE__, BUDGET_Editors ); );

UIWindow* ugcMapSearchWindowCreate( UGCProjectData* ugcProj, const char* mapName, UGCMapSearchCallback cb, UserData data )
{
	UIWindow* window = ui_WindowCreate( "", 0, 0, 300, 600 );
	UGCMapSearchWidget* searchWidget = calloc( 1, sizeof( *searchWidget ));
	ui_WidgetSetTextMessage( UI_WIDGET( window ), "UGC_MapEditor.Search" );
	
	{
		UIPane* rootPane;
			UIComboBox* sortCombo;
			UITextEntry* filterEntry;
			UITree* tree;
		int y = 0;
		
		rootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
		ui_PaneSetInvisible( rootPane, true );

		sortCombo = ui_ComboBoxCreateWithEnum( 0, y, 0, UGCMapSearchSortTypeEnum, NULL, NULL );
		sortCombo->bStringAsMessageKey = true;
		ui_WidgetSetWidthEx( UI_WIDGET( sortCombo ), 1, UIUnitPercentage );
		ui_ComboBoxSetSelectedEnumCallback( sortCombo, ugcMapSearchSortChangedCB, searchWidget );
		ui_ComboBoxSetSelectedEnum( sortCombo, UGCMapSearchSortType_ByType );
		ui_PaneAddChild( rootPane, sortCombo );
		y += UGC_ROW_HEIGHT;

		filterEntry = ui_TextEntryCreate( "", 0, y );
		ui_EditableSetDefaultMessage( UI_EDITABLE( filterEntry ), "UGC_Editor.Filter" );
		ui_WidgetSetWidthEx( UI_WIDGET( filterEntry ), 1, UIUnitPercentage );
		ui_TextEntrySetChangedCallback( filterEntry, ugcMapSearchFilterChangedCB, searchWidget );
		ui_PaneAddChild( rootPane, filterEntry );
		y += UGC_ROW_HEIGHT;

		tree = ui_TreeCreate( 0, 0, 1, 1 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( tree ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( tree ), 0, 0, y, 0 );
		ui_TreeNodeSetFillCallback( &tree->root, ugcMapSearchFillRootCB, NULL );
		UI_WIDGET( tree )->u64 = (U64)searchWidget;
		ui_TreeSetSelectedCallback( tree, ugcMapSearchTreeSelectedCB, searchWidget );
		ui_WidgetSetTextMessage( UI_WIDGET( tree ), "UGC_MapEditor.NoComponentsMatchFilter" );
		ui_PaneAddChild( rootPane, tree );

		// Hook up to search widget
		UI_WIDGET( rootPane )->u64 = (U64)searchWidget;
		UI_WIDGET( rootPane )->freeF = ugcMapSearchWidgetFree;
		searchWidget->rootPane = rootPane;
		searchWidget->sortCombo = sortCombo;
		searchWidget->filterEntry = filterEntry;
		searchWidget->tree = tree;
		searchWidget->strMapName = strdup( mapName );
	
		// Now refresh the UI (must be last!)
		ugcMapSearchWidgetRefresh( searchWidget );
	}

	UI_WIDGET( window )->u64 = (U64)searchWidget;
	ui_WindowAddChild( window, searchWidget->rootPane );
	elUICenterWindow( window );
	
	return window;
}

static void ugcMapSearchNodeReset( UGCMapSearchNode *node )
{
	node->astrIconName = NULL;
	StructFreeStringSafe(&node->strAssetBrowserText);
	ui_WidgetQueueFreeAndNull(&node->pErrorSprite);

	FOR_EACH_IN_EARRAY(node->eaChildren, UGCMapSearchNode, child_node)
	{
		ugcMapSearchNodeReset(child_node);
		SAFE_FREE(child_node);
	}
	FOR_EACH_END;
	eaDestroy(&node->eaChildren);
}

void ugcMapSearchWindowRefresh( UIWindow* window )
{
	UGCMapSearchWidget* searchWidget = (UGCMapSearchWidget*)UI_WIDGET( window )->u64;

	ugcMapSearchWidgetRefresh( searchWidget );
}

static void ugcMapSearchNodesFilter( UGCMapSearchWidget* searchWidget, UGCMapSearchNode*** peaNodes )
{
	const char* filterText = ui_TextEntryGetText( searchWidget->filterEntry );
	FOR_EACH_IN_EARRAY( *peaNodes, UGCMapSearchNode, node ) {
		ugcMapSearchNodesFilter( searchWidget, &node->eaChildren );
		if( eaSize( &node->eaChildren ) == 0 && !strstri( node->strAssetBrowserText, filterText )) {
			ugcMapSearchNodeReset( node );
			free( node );
			eaRemove( peaNodes, FOR_EACH_IDX( *peaNodes, node ));
		}
	} FOR_EACH_END;
}

void ugcMapSearchWidgetRefresh( UGCMapSearchWidget* searchWidget )
{
	ugcMapSearchNodeReset(&searchWidget->rootNode);

	// Set up children
	{
		UGCComponent **component_array = ugcComponentFindPlacements( ugcEditorGetComponentList(), searchWidget->strMapName, UGC_ANY_ROOM_ID );
		FOR_EACH_IN_EARRAY_FORWARDS(component_array, UGCComponent, component) {
			if(   component->uParentID == 0
				  && ugcComponentLayoutCompatible(component->eType, UGC_MAP_TYPE_ANY)
				  && component->eType != UGC_COMPONENT_TYPE_PATROL_POINT ) {
				UGCMapSearchNode *new_node = calloc(1, sizeof(UGCMapSearchNode));
				ugcMapSearchNodeFillComponent( searchWidget, new_node, component);
				new_node->pParent = &searchWidget->rootNode;
				eaPush(&searchWidget->rootNode.eaChildren, new_node);
			}
		} FOR_EACH_END;
		eaQSort_s( searchWidget->rootNode.eaChildren, ugcMapSearchSortNodes, searchWidget );
		eaDestroy(&component_array);		
	}

	ugcMapSearchNodesFilter( searchWidget, &searchWidget->rootNode.eaChildren );
	
	ui_TreeRefresh( searchWidget->tree );
}

void ugcMapSearchWidgetFree( UIWidget* widget )
{
	UGCMapSearchWidget* searchWidget = (UGCMapSearchWidget*)widget->u64;

	ui_PaneFreeInternal( searchWidget->rootPane );
	free( searchWidget->strMapName );
	ugcMapSearchNodeReset( &searchWidget->rootNode );
	free( searchWidget );
}

void ugcMapSearchSortChangedCB( UIComboBox* box, int value, UserData rawSearchWidget )
{
	UGCMapSearchWidget* searchWidget = rawSearchWidget;
	ugcMapSearchWidgetRefresh( searchWidget );
}

void ugcMapSearchFilterChangedCB( UITextEntry* entry, UserData rawSearchWidget )
{
	UGCMapSearchWidget* searchWidget = rawSearchWidget;
	ugcMapSearchWidgetRefresh( searchWidget );
}

void ugcMapSearchFillRootCB( UITreeNode* parent, UserData ignored )
{
	UGCMapSearchWidget* searchWidget = (UGCMapSearchWidget*)UI_WIDGET( parent->tree )->u64;
	
	ugcMapSearchFillMapDocNodeCB( parent, &searchWidget->rootNode );
}

static void ugcMapSearchTreeNodeFreeError(UITreeNode *node)
{
	   FOR_EACH_IN_EARRAY((UIWidget**)node->widgets, UIWidget, widget)
	   {
			   widget->group = NULL;
	   }
	   FOR_EACH_END;
	   eaDestroy((UIWidget***)&node->widgets);
}

void ugcMapSearchFillMapDocNodeCB( UITreeNode* parent, UGCMapSearchNode* parentNode )
{
	FOR_EACH_IN_EARRAY_FORWARDS( parentNode->eaChildren, UGCMapSearchNode, childNode ) {
		bool hasChildren = (eaSize( &childNode->eaChildren ) > 0);
		UITreeNode* node = ui_TreeNodeCreate( parent->tree, ugcMapSearchNodeCRC( childNode ), (ParseTable*)UGC_AB_MAP_NODE, childNode,
											  (hasChildren ? ugcMapSearchFillMapDocNodeCB : NULL ), childNode,
											  ugcMapSearchTreeNodeDisplayCB, childNode, 20 );

		if( childNode->pErrorSprite ) {
			ui_TreeNodeSetFreeCallback( node, ugcMapSearchTreeNodeFreeError );
			ui_WidgetGroupAdd( &node->widgets, UI_WIDGET( childNode->pErrorSprite ));
		}
		ui_TreeNodeAddChild( parent, node );
	} FOR_EACH_END;
}

void ugcMapSearchTreeNodeDisplayCB( UITreeNode *node, UserData rawGroup, UI_MY_ARGS, F32 z )
{
	UGCMapSearchNode* group = rawGroup;
	
	UITree* tree = node->tree;
	CBox box = {x, y, x + w, y + h};
	UIStyleFont *font = GET_REF(UI_GET_SKIN(tree)->hNormal);

	if( group->pErrorSprite && stricmp( ui_WidgetGetText( UI_WIDGET( group->pErrorSprite )), "ugc_icons_labels_alert" ) == 0 ) {
		box.right -= ui_WidgetGetWidth( UI_WIDGET( group->pErrorSprite ));
		w -= ui_WidgetGetWidth( UI_WIDGET( group->pErrorSprite ));
	}
	
	ui_StyleFontUse(font, false, UI_WIDGET(tree)->state);
	{
		AtlasTex* texture = atlasLoadTexture( group->astrIconName );
		CBox iconBox = { x, y, x + h, y + h };
		display_sprite_box( texture, &iconBox, z, -1 );
	}

	if( group->hasMissionLink || group->hasTriggerLink ) {
		AtlasTex* texture = atlasLoadTexture( "ugc_icon_link");
		CBox iconBox = {x + w - h, y, x + w, y + h };
		display_sprite_box( texture, &iconBox, z, -1 );

		box.right -= h;
	}
	
	clipperPushRestrict(&box);
	gfxfont_PrintMaxWidth(x + h + 2, y + h/2, z, box.right - box.left - h+2, scale, scale, CENTER_Y, group->strAssetBrowserText );
	clipperPop();
}

void ugcMapSearchTreeSelectedCB( UITree* tree, UserData rawSearchWidget )
{
	UGCMapSearchWidget* searchWidget = rawSearchWidget;
	const UITreeNode*const*const*  eaSelected = ui_TreeGetSelectedNodes( tree );
	const UITreeNode* node = eaGet( eaSelected, 0 );
	if( node ) {
		UGCMapSearchNode* group = node->contents;

		if( group->uComponentID ) {
			ugcEditorEditMapComponent( searchWidget->strMapName, group->uComponentID, false, true );
		} else {
			ugcEditorEditMapComponent( searchWidget->strMapName, UGC_NONE, false, true );
		}
	}
}

void ugcMapSearchNodeFillComponent( UGCMapSearchWidget* searchWidget, UGCMapSearchNode* node, UGCComponent* component )
{
	bool is_unplaced = (component->sPlacement.uRoomID == GENESIS_UNPLACED_ID);
	UGCMissionObjective* objective = ugcObjectiveFindComponentRelated( ugcEditorGetMission()->objectives, ugcEditorGetComponentList(), component->uID );
	UGCComponent **trigger_list = NULL;
	UGCComponent **child_components = NULL;
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);

	ugcBacklinkTableFindAllTriggers(ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), component->uID, 0, &trigger_list);

	// Set up component node
	node->uComponentID = component->uID;
	node->astrIconName = allocAddString(g_ComponentIcons[component->eType]);
	node->hasMissionLink = (objective != NULL);
	node->hasTriggerLink = (eaSize(&trigger_list) > 0);
	assert(node->astrIconName);
	{
		char buffer[ 256 ];
		ugcComponentGetDisplayName( buffer, ugcEditorGetProjectData(), component, false );
		
		if (is_unplaced && objective)
		{
			char* estr = NULL;
			FormatMessageKey( &estr, "UGC_MapEditor.UnplacedComponentUsedInObjective",
							  STRFMT_STRING( "ComponentName", buffer ),
							  STRFMT_STRING( "ObjectiveName", ugcMissionObjectiveUIString( objective )),
							  STRFMT_END );
			StructCopyString(&node->strAssetBrowserText, estr);
			estrDestroy( &estr );
		}
		else
		{
			StructCopyString(&node->strAssetBrowserText, buffer);
		}
	}

	ugcErrorButtonRefresh(&node->pErrorSprite, ugcEditorGetRuntimeStatus(), ugcMakeTempErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL ), "", 0, NULL);

	// Set up child nodes -- patrol routes
	if( ugcComponentHasPatrol( component, NULL )) {
		UGCMapSearchNode* patrolFolderNode = calloc(1, sizeof(UGCMapSearchNode));
		patrolFolderNode->astrIconName = allocAddString("white");
		StructCopyString( &patrolFolderNode->strAssetBrowserText, TranslateMessageKey( "UGC_MapEditor.PatrolRoute" ));
		patrolFolderNode->uComponentID = component->uID;
		patrolFolderNode->isPatrolRouteParent = true;
		patrolFolderNode->pParent = node;
		eaPush(&node->eaChildren, patrolFolderNode);

		{
			int it;
			for( it = 0; it != eaiSize( &component->eaPatrolPoints ); ++it ) {
				UGCMapSearchNode* patrolPointNode = calloc( 1, sizeof( UGCMapSearchNode ));
				UGCComponent* patrolPoint = ugcEditorFindComponentByID( component->eaPatrolPoints[ it ]);
				ugcMapSearchNodeFillComponent( searchWidget, patrolPointNode, patrolPoint );
				patrolPointNode->pParent = patrolFolderNode;
				eaPush( &patrolFolderNode->eaChildren, patrolPointNode );
			}
		}
	}

	// Set up child nodes -- child components
	FOR_EACH_IN_EARRAY(ugcEditorGetComponentList()->eaComponents, UGCComponent, child_component)
	{
		if(   child_component->uParentID == component->uID
			  && ugcComponentLayoutCompatible(child_component->eType, UGC_MAP_TYPE_ANY)
			  && child_component->eType != UGC_COMPONENT_TYPE_PATROL_POINT ) {
			eaPush(&child_components, child_component);
		}
	}
	FOR_EACH_END;

	if (room_info && room_info->iNumLevels > 1)
	{
		int level;
		for (level = 0; level < room_info->iNumLevels; level++)
		{
			UGCMapSearchNode *level_node = calloc(1, sizeof(UGCMapSearchNode));
			level_node->astrIconName = allocAddString("ugc_icon_map"); // TomY TODO need an icon here
			level_node->uComponentID = component->uID;
			level_node->iLevelID = level+1;
			{
				char* estr = NULL;
				FormatMessageKey( &estr, "UGC_MapEditor.RoomLevel",
								  STRFMT_INT( "LevelNumber", level + 1 ),
								  STRFMT_END );
				StructCopyString(&level_node->strAssetBrowserText, estr);
				estrDestroy( &estr );
			}
			level_node->pParent = node;
			eaPush(&node->eaChildren, level_node);

			FOR_EACH_IN_EARRAY_FORWARDS(child_components, UGCComponent, child_component)
			{
				if (child_component->sPlacement.iRoomLevel == level)
				{
					UGCMapSearchNode *new_node = calloc(1, sizeof(UGCMapSearchNode));
					ugcMapSearchNodeFillComponent(searchWidget, new_node, child_component);
					new_node->pParent = level_node;
					eaPush(&level_node->eaChildren, new_node);
				}
			}
			FOR_EACH_END;

			eaQSort_s(level_node->eaChildren, ugcMapSearchSortNodes, searchWidget);
		}
	}
	else
	{
		FOR_EACH_IN_EARRAY_FORWARDS(child_components, UGCComponent, child_component)
		{
			UGCMapSearchNode *new_node = calloc(1, sizeof(UGCMapSearchNode));
			ugcMapSearchNodeFillComponent(searchWidget, new_node, child_component);
			new_node->pParent = node;
			eaPush(&node->eaChildren, new_node);
		}
		FOR_EACH_END;

		eaQSort_s(node->eaChildren, ugcMapSearchSortNodes, searchWidget);
	}

	eaDestroy( &trigger_list );
}

U32 ugcMapSearchNodeCRC( UGCMapSearchNode *node )
{
	if (node->uComponentID)
	{
		return 0x20000000 + (node->iLevelID << 16) + node->uComponentID;
	}
	else
	{
		return 0x10000000;
	}
}

int ugcMapSearchSortNodes( UGCMapSearchWidget* searchWidget, const UGCMapSearchNode **nodeA, const UGCMapSearchNode **nodeB )
{
	UGCMapSearchSortType sortType = ui_ComboBoxGetSelectedEnum( searchWidget->sortCombo );
	UGCComponent* a = ugcEditorFindComponentByID( (*nodeA)->uComponentID );
	UGCComponent* b = ugcEditorFindComponentByID( (*nodeB)->uComponentID );
	assert( a && b );
	
	if( sortType == UGCMapSearchSortType_ByHasError ) {
		bool aHasError = (stricmp( ui_WidgetGetText(UI_WIDGET((*nodeA)->pErrorSprite)), "ugc_icons_labels_alert" ) == 0);
		bool bHasError = (stricmp( ui_WidgetGetText(UI_WIDGET((*nodeB)->pErrorSprite)), "ugc_icons_labels_alert" ) == 0);

		if( aHasError != bHasError ) {
			if( aHasError ) {
				return -1;
			}
			if( bHasError ) {
				return +1;
			}
		}
	}
	if( sortType == UGCMapSearchSortType_ByUse ) {
		bool aHasMissionLink = (*nodeA)->hasMissionLink;
		bool bHasMissionLink = (*nodeB)->hasMissionLink;
		bool aHasTriggerLink = (*nodeA)->hasTriggerLink;
		bool bHasTriggerLink = (*nodeB)->hasTriggerLink;

		if( aHasMissionLink != bHasMissionLink ) {
			if( aHasMissionLink ) {
				return -1;
			}
			if( bHasMissionLink ) {
				return +1;
			}
		}
		if( aHasTriggerLink != bHasTriggerLink ) {
			if( aHasTriggerLink ) {
				return -1;
			}
			if( bHasTriggerLink ) {
				return +1;
			}
		}
	}

	if( a->sPlacement.uRoomID != b->sPlacement.uRoomID ) {
		return b->sPlacement.uRoomID - a->sPlacement.uRoomID;
	}
	
	if( sortType == UGCMapSearchSortType_ByType ) {
		if( a->eType != b->eType ) {
			return a->eType - b->eType;
		}
	}
	return stricmp_safe(a->pcVisibleName, b->pcVisibleName);
}

#include"NNOUGCMapSearch_c_ast.c"

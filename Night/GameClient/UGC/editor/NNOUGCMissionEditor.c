#include"NNOUGCMissionEditor.h"

#include"Color.h"
#include"GameClientLib.h"
#include"GfxSprite.h"
#include"GfxSpriteText.h"
#include"MultiEditField.h"
#include"MultiEditFieldContext.h"
#include"NNOUGCAssetLibrary.h"
#include"NNOUGCCommon.h"
#include"NNOUGCDialogPromptPicker.h"
#include"NNOUGCEditorPrivate.h"
#include"NNOUGCMissionCommon.h"
#include"NNOUGCModalDialog.h"
#include"NNOUGCResource.h"
#include"NNOUGCUIResourcePreview.h"
#include"NNOUGCUITutorialNode.h"
#include"NNOUGCZeniPicker.h"
#include"ResourceSearch.h"
#include"StringCache.h"
#include"StringFormat.h"
#include"StringUtil.h"
#include"UGCCommon.h"
#include"UGCCommon.h"
#include"UGCError.h"
#include"UGCProjectCommon.h"
#include"UGCProjectUtils.h"
#include"UITextureAssembly.h"
#include"UITooltips.h"
#include"UITreechart.h"
#include"WorldGrid.h"
#include"contact_common.h"
#include"gclUIGen.h"
#include"gfxHeadshot.h"
#include"tokenstore.h"
#include"wlCostume.h"
#include"wlUGC.h"

typedef struct UGCMissionDoc UGCMissionDoc;
typedef struct UGCMissionLibraryModel UGCMissionLibraryModel;
typedef struct UGCMissionNodeGroup UGCMissionNodeGroup;
typedef struct UGCMissionTreechartPos UGCMissionTreechartPos;
typedef struct ZoneMap ZoneMap;

static UGCMissionLibraryModel*** ugcMissionEditorObjectivesNullModel( void );
static UGCMissionLibraryModel*** ugcMissionEditorObjectivesModel( void );
static void ugcMissionObjectiveListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData ignored );
static void ugcMissionDocAddObjective( UIButton* ignored, UserData rawDoc );
static void ugcMissionDocAddNewNode( UGCMissionDoc* doc );
static void ugcMissionDocObjectiveListDrag( UIWidget* rawList, UserData rawDoc );

static void ugcMissionDocFreeTreechartAndLibrary( UGCMissionDoc* doc );
static void ugcMissionDocRefreshTreechart( UGCMissionDoc* doc );
static void ugcMissionDocRefreshProperties( UGCMissionDoc* doc );
static void ugcMissionPropertiesOncePerFrame( UGCMissionDoc* doc );

static void ugcObjectiveAddComponent( UIButton* ignored, UserData rawObjectiveGroup );
static void ugcObjectiveTalkToContactDialogEdit( UIButton* ignored, UserData rawObjectiveGroup );
static void ugcDialogTreeBlockEdit( UIButton* ignored, UserData rawBlockGroup );

static void ugcMissionSetNoSelection( UGCMissionDoc* doc );
static void ugcMissionUpdateFocusForSelection( UGCMissionDoc* doc );


static UGCMissionNodeGroup* ugcMissionNodeGroupRefreshObjective( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCMapTransitionInfo** transitions, UGCMissionObjective* tmogObjective, bool isFirstObjectiveOnMap );
static void ugcMissionNodeGroupRefreshObjectiveProperties( UGCMissionNodeGroup* group );
static void ugcMissionNodeGroupRefreshObjectivePropertiesBasic( UGCMissionNodeGroup* group, UGCMissionObjective* objective, UGCComponent* component );
static void ugcMissionNodeGroupRefreshObjectivePropertiesDialogTree( UGCMissionNodeGroup* group, UGCMissionObjective* objective, UGCComponent* component );
static void ugcMissionNodeGroupRefreshObjectivePropertiesInteract( UGCMissionNodeGroup* group, UGCMissionObjective* objective, UGCComponent* component );

static UGCMissionNodeGroup* ugcMissionNodeGroupRefreshDialogTree( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCComponent* component );
static void ugcMissionNodeGroupRefreshDialogTreeProperties( UGCMissionNodeGroup* group, bool isPropertiesPane );

static UGCMissionNodeGroup* ugcMissionNodeGroupRefreshMap( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCMapTransitionInfo** transitions, UGCMissionObjective** mapObjectives );
static void ugcMissionNodeGroupRefreshMapProperties( UGCMissionNodeGroup* group );

static UGCMissionNodeGroup* ugcMissionNodeGroupRefreshMapTransition( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCMapTransitionInfo** transitions, UGCMissionObjective** mapObjectives );
static void ugcMissionRefreshReturnMapLink( UGCMissionDoc* doc );
static void ugcMissionNodeGroupRefreshMapTransitionProperties( UGCMissionNodeGroup* group );

static void ugcMissionRefreshRewardBox( UGCMissionDoc* doc, UGCComponent *reward_box_component );

static UGCMissionNodeGroup* ugcMissionNodeGroupIntern( UGCMissionDoc* doc, int* pGroupIt );
static void ugcMissionNodeGroupReset( UGCMissionNodeGroup* group );
static void ugcMissionNodeGroupDestroy( UGCMissionNodeGroup* group );
static int ugcMissionNodeGroupChildDepth( UGCMissionNodeGroup* group );
static int ugcMissionNodeGroupParentDepth( UGCMissionNodeGroup* group );

static UGCMissionNodeGroup* ugcMissionDocNodeGroupFindSelectedNode( UGCMissionDoc* doc );
static UGCMissionNodeGroup* ugcMissionDocNodeGroupFindObjective( UGCMissionDoc* doc, U32 id );


static void ugcMissionDocNewGroupApplyType( UGCMissionDoc* doc );
static void ugcMissionTreechartAddWidget(SA_PARAM_NN_VALID UGCMissionDoc* doc, SA_PARAM_OP_VALID UIWidget* beforeWidget, SA_PARAM_NN_VALID UIWidget* widget, SA_PARAM_OP_STR const char* iconName, UserData data, UITreechartNodeFlags flags );
static void ugcMissionTreechartAddGroup( UGCMissionDoc* doc, UGCMissionNodeGroup* prevGroup, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* group, UITreechartNodeFlags flags );

static bool ugcMissionTreechartDragNodeNode( UITreechart* ignored, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawDestGroup );
static bool ugcMissionTreechartDragNodeArrow( UITreechart* ignored, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawBeforeDestGroup, UserData rawAfterDestGroup );
static void ugcMissionTreechartNodeAnimate( UITreechart* ignored, UserData rawDoc, UserData rawGroup, float x, float y );
static void ugcMissionTreechartDragNodeTrash( UITreechart* ignored, UserData rawDoc, UserData rawGroup );
static bool ugcMissionTreechartDragNodeNodeColumn( UITreechart* ignored, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawDestGroup, int column );

static const char* ugcMissionTreechartMapName( UGCMissionNodeGroup* before, UGCMissionNodeGroup* after );
static void ugcMissionTreechartCalcPos( UGCMissionNodeGroup* beforeDestGroup, UGCMissionNodeGroup* afterDestGroup, UGCMissionTreechartPos* out_pos );
static void ugcMissionShowInsertFirstMapMenuCB( UIButton* button, UGCMissionDoc* doc );
static void ugcMissionShowInsertLastMapMenuCB( UIButton* button, UGCMissionDoc* doc );
static void ugcMissionShowInsertMapMenuCB( UIButton* button, UGCMissionNodeGroup* node );
static void ugcMissionSelectInsertMapMenuCB( UIMenuItem* item, UGCMissionLibraryModel* model );
static void ugcMissionSetNoSelectionCB( UIButton* ignored, UGCMissionDoc* doc );

static void ugcMissionModelsRefresh( UGCMissionDoc* doc );
static void ugcMissionObjectiveFillLibraryModel( UGCMissionObjective* objective, UGCMissionLibraryModel* model );
static void ugcMissionLibraryModelText( UGCMissionLibraryModel* model, char** pestr );
static const char* ugcMissionLibraryModelToTexture( UGCMissionLibraryModel* model );
static const char* ugcMissionObjectiveComponentTypeToTexture( UGCComponentType type, bool isTalkToObjective, bool isTilted );

static void ugcMissionPlaySelectedNode( UIButton* ignored, UserData rawDoc );
static void ugcMissionDeleteSelectedNode( UIButton* ignored, UserData rawDoc );

static void ugcMissionDialogTreeRenumberSequence( UGCComponent** dialogTrees, UGCWhenType whenType, U32 whenObjectiveID );

static void** nullModel = NULL;

#define OBJECTIVE_DATA_COL   100

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

AUTO_STRUCT;
typedef struct UGCMissionLibraryModel {
	UGCMissionLibraryModelType type;	AST(NAME("Type"))
	UGCComponentType componentType;		AST(NAME("ComponentType"))

	char* mapName;						AST(NAME("MapName"))
} UGCMissionLibraryModel;
extern ParseTable parse_UGCMissionLibraryModel[];
#define TYPE_parse_UGCMissionLibraryModel UGCMissionLibraryModel

typedef enum UGCMissionNodeType {
	UGCNODE_DUMMY,
	UGCNODE_MAP,
	UGCNODE_MAP_FTUE,
	UGCNODE_MAP_TRANSITION,
	UGCNODE_RETURN_MAP_TRANSITION,
	UGCNODE_OBJECTIVE,
	UGCNODE_DIALOG_TREE,
	UGCNODE_REWARD_BOX,
} UGCMissionNodeType;

/// A "UINode" in the treechart.  These correspond one-to-one with
/// each widget in the treechart.
typedef struct UGCMissionNodeGroup {
	UGCMissionDoc* doc;
	UGCMissionNodeType type;
	UGCMissionNodeGroup* parentGroup;
	UGCMissionNodeGroup* prevGroup;
	UGCMissionNodeGroup* nextGroup;
	UGCMissionNodeGroup* firstChildGroup;

	UIPane* parent;
	UIButton* leftButton;

	/// For the asset browser
	const char* astrIconName;

	/// type == UGCNODE_DUMMY
	struct {
		UILabel* label;
	} dummy;

	/// type == UGCNODE_OBJECTIVE
	struct {
		U32 objectiveID;
	} objective;

	/// type == UGCNODE_DIALOG_TREE
	struct {
		U32 componentID;
	} dialogTree;

	/// type == UGCNODE_MAP_TRANSITION
	struct {
		U32 objectiveID;
		char* prevMapName;
		char* nextMapName;
		int* objectiveIDs;
		UGCMissionMapLink editingLink;
	} mapTransition;

	/// type == UGCNODE_MAP
	struct {
		U32 objectiveID;
		char* prevMapName;
		char* nextMapName;
		int* objectiveIDs;
		UGCMissionMapLink editingLink;
	} map;

	/// type == UGCNODE_MAP_FTUE
	struct {
		U32 objectiveID;
	} mapFtue;

	/// type == UGCNODE_REWARD_BOX
	struct {
		int unused;
	} rewardBox;
} UGCMissionNodeGroup;

typedef struct UGCMissionObjectiveComponentGroup {
	UGCMissionNodeGroup* group;
	int index;
} UGCMissionObjectiveComponentGroup;

AUTO_STRUCT;
typedef struct ZoneMapDisplayNamePair
{
	char* displayNameText;
	char* zmapName;
} ZoneMapDisplayNamePair;
extern ParseTable parse_ZoneMapDisplayNamePair[];
#define TYPE_parse_ZoneMapDisplayNamePair ZoneMapDisplayNamePair

AUTO_STRUCT;
typedef struct UGCObjectivePriorityPair
{
	U32 objectiveID;				AST( NAME("ObjectiveID"))
	int priority;					AST( NAME("Priority") )
} UGCObjectivePriorityPair;
extern ParseTable parse_UGCObjectivePriorityPair[];
#define TYPE_parse_UGCObjectivePriorityPair UGCObjectivePriorityPair

/// Structure for the MissionEditor
typedef struct UGCMissionDoc {
	UGCMission* mission;
	bool ignoreChanges;

	// properties pane
	UIPane* propertiesPane;
	UISprite* propertiesSprite;

	// widget tree
	UIPane* pRootPane;

	// library pane
	UIPane* libraryPane;
	UILabel* libraryObjectivesLabel;
	UIList* libraryObjectivesList;

	// objectives
	UITreechart* treechart;

	UIMenu* insertMapMenu;
	UGCMissionNodeGroup* insertMapNode;

	UGCMissionNodeGroup** nodeGroups;
	const char** eaContextNames;

	UGCMissionNodeGroup dummyStartGroup;
	Vec2 startPos;
	UGCMissionNodeGroup dummyEndGroup;
	Vec2 endPos;
	UGCMissionNodeGroup returnMapLinkGroup;
	UGCMissionNodeGroup rewardBoxGroup;

	UGCMissionNodeGroup newGroup;
	UGCMissionLibraryModel newGroupModel;

	UGCMissionNodeType selectedNodeType;
	U32 selectedNodeID;
	bool selectedNodeMakeVisible;

	// Data
	UGCMissionLibraryModel** eaInsertMapModel;

	// For the special DnD popup behavior
	UGCMissionNodeGroup* dndLastDestNode;
	U32 dndLastDestNodeStart;
	bool dndUsed;

	bool queueRefresh;
} UGCMissionDoc;

/// Structure for calculating positions.
typedef struct UGCMissionTreechartPos {
	UGCMissionObjective*** peaDestObjectives;
	int destObjectiveIndex;

	UGCWhenType destDialogPromptWhen;
	int destDialogPromptIndex;
} UGCMissionTreechartPos;

bool ugcMissionEditorShowReachMap = false;
AUTO_CMD_INT( ugcMissionEditorShowReachMap, ugcMissionEditorShowReachMap );

UGCMission* ugcMissionGetMission( UGCMissionDoc* doc )
{
	if (!doc)
		return NULL;
	return doc->mission;
}

UGCMissionDoc* ugcMissionDocLoad( void )
{
	UGCMissionDoc* doc = calloc( 1, sizeof( *doc ));

	resRequestAllResourcesInDictionary( "ZoneMapEncounterInfo" );
	resRequestAllResourcesInDictionary( "ZoneMapExternalMapSnap" );

	// layout the widgets
	doc->pRootPane = ui_PaneCreate(0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0);

	// Setup the newGroup group
	doc->newGroup.doc = doc;

	ugcMissionDocRefresh( doc );

	return doc;
}

void ugcMissionSetVisible(UGCMissionDoc *doc)
{
	ugcEditorSetDocPane( doc->pRootPane );
}

void ugcMissionDocClose( UGCMissionDoc** ppDoc )
{
	if( !*ppDoc ) {
		return;
	}

	ugcMissionDocFreeTreechartAndLibrary( *ppDoc );

	ui_WidgetQueueFreeAndNull( &(*ppDoc)->propertiesPane );
	ui_WidgetQueueFreeAndNull( &(*ppDoc)->propertiesSprite );
	ui_WidgetQueueFreeAndNull( &(*ppDoc)->pRootPane );
	ui_WidgetQueueFreeAndNull( &(*ppDoc)->insertMapMenu );

	free( *ppDoc );
	*ppDoc = NULL;
}

void ugcMissionDocOncePerFrame( UGCMissionDoc* doc, bool isActive )
{
	UGCMissionNodeGroup* selectedNode;
	if( !SAFE_MEMBER( doc->treechart, draggingNode ) || !doc->dndUsed ) {
		doc->dndLastDestNode = NULL;
		doc->dndLastDestNodeStart = 0;
	}

	if( doc->queueRefresh ) {
		doc->queueRefresh = false;
		ugcMissionDocRefresh( doc );
	}

	doc->dndUsed = false;

	ugcMissionPropertiesOncePerFrame( doc );

	// Focus and selection should be in sync.  If this is not the
	// case, then the user has specifically focused on a widget.
	// Therefore, clear the selection.
	selectedNode = ugcMissionDocNodeGroupFindSelectedNode( doc );
	if( selectedNode ) {
		if( !isActive ) {
			ugcMissionSetNoSelection( doc );
			ugcEditorQueueUIUpdate();
		} else if( doc->propertiesPane && g_ui_State.focused
				   && !ugcAssetLibraryPickerWindowOpen() && !ugcZeniPickerWindowOpen()
				   && !ugcDialogPromptPickerWindowOpen()
				   && !ui_IsFocused( selectedNode->parent ) && !ui_IsFocusedOrChildren( doc->propertiesPane )) {
			ugcMissionSetNoSelection( doc );
			ugcEditorQueueUIUpdate();
		}
	}

	// MJF Mar/25/2013 -- If the insert map menu is opened, we can't
	// allow tooltips to come up or they might obscure the menu!
	if( doc->insertMapMenu && doc->insertMapMenu->opened ) {
		ui_TooltipsClearActive();
	}
}


UGCMissionLibraryModel*** ugcMissionEditorObjectivesNullModel( void )
{
	static UGCMissionLibraryModel** model = NULL;
	return &model;
}

static UGCMissionLibraryModel* ugcCreateLibraryModelComponentType( UGCComponentType type )
{
	UGCMissionLibraryModel* accum = StructCreate( parse_UGCMissionLibraryModel );
	accum->type = UGCMIMO_NEW_OBJECTIVE;
	accum->componentType = type;
	return accum;
}

static UGCMissionLibraryModel* ugcCreateLibraryModelUnlockDoorComponentType( UGCComponentType type )
{
	UGCMissionLibraryModel* accum = StructCreate( parse_UGCMissionLibraryModel );
	accum->type = UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE;
	accum->componentType = type;
	return accum;
}

static UGCMissionLibraryModel* ugcCreateLibraryModelTalkToComponentType( UGCComponentType type )
{
	UGCMissionLibraryModel* accum = StructCreate( parse_UGCMissionLibraryModel );
	accum->type = UGCMIMO_NEW_TALK_TO_OBJECTIVE;
	accum->componentType = type;
	return accum;
}

static UGCMissionLibraryModel* ugcCreateLibraryModelNewMap( void )
{
	UGCMissionLibraryModel* accum = StructCreate( parse_UGCMissionLibraryModel );
	accum->type = UGCMIMO_NEW_PROJECT_MAP;
	return accum;
}

static UGCMissionLibraryModel* ugcCreateLibraryModelMap( UGCMap* map )
{
	UGCMissionLibraryModel* accum = StructCreate( parse_UGCMissionLibraryModel );
	accum->type = UGCMIMO_PROJECT_MAP;
	accum->mapName = StructAllocString( map->pcName );
	return accum;
}

// earray version of int list so that UIList can iterate over it.
UGCMissionLibraryModel*** ugcMissionEditorObjectivesModel( void )
{
	static UGCMissionLibraryModel** model = NULL;
	eaClearStruct( &model, parse_UGCMissionLibraryModel );

	// Objective types
	eaPush( &model, ugcCreateLibraryModelComponentType( UGC_COMPONENT_TYPE_OBJECT ));
//	eaPush( &model, ugcCreateLibraryModelComponentType( UGC_COMPONENT_TYPE_DESTRUCTIBLE ));
	eaPush( &model, ugcCreateLibraryModelComponentType( UGC_COMPONENT_TYPE_KILL ));
	eaPush( &model, ugcCreateLibraryModelTalkToComponentType( UGC_COMPONENT_TYPE_CONTACT ));
	if( ugcIsDialogWithObjectEnabled() ) {
		eaPush( &model, ugcCreateLibraryModelTalkToComponentType( UGC_COMPONENT_TYPE_OBJECT ));
	}
	if( ugcIsInteriorEditorEnabled() ) {
		eaPush( &model, ugcCreateLibraryModelUnlockDoorComponentType( UGC_COMPONENT_TYPE_ROOM_DOOR ));
	}
	if( ugcDefaultsDialogStyle() == UGC_DIALOG_STYLE_WINDOW ) {
		eaPush( &model, ugcCreateLibraryModelComponentType( UGC_COMPONENT_TYPE_DIALOG_TREE ));
	}
	eaPush( &model, ugcCreateLibraryModelComponentType( UGC_COMPONENT_TYPE_ROOM_MARKER ));

	return &model;
}

void ugcMissionObjectiveFillLibraryModel( UGCMissionObjective* objective, UGCMissionLibraryModel* model )
{
	// Fill in a default, in case we can't find anything better
	model->type = UGCMIMO_NEW_OBJECTIVE;
	model->componentType = UGC_COMPONENT_TYPE_OBJECT;
	
	if( objective->type == UGCOBJ_COMPLETE_COMPONENT ) {
		UGCComponent* component = ugcEditorFindComponentByID( objective->componentID );
		if( component ) {
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				UGCComponent* contactComponent = ugcEditorFindComponentByID( component->uActorID );
				if( contactComponent ) {
					model->type = UGCMIMO_NEW_TALK_TO_OBJECTIVE;
					model->componentType = contactComponent->eType;
				}
			} else {
				model->type = UGCMIMO_NEW_OBJECTIVE;
				model->componentType = component->eType;
			}
		}
	} else if( objective->type == UGCOBJ_UNLOCK_DOOR ) {
		UGCComponent* component = ugcEditorFindComponentByID( objective->componentID );
		if( component ) {
			model->type = UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE;
			model->componentType = component->eType;
		}
	}
}

void ugcMissionLibraryModelText( UGCMissionLibraryModel* model, char** pestr )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	estrClear( pestr );

	switch( model->type ) {
		xcase UGCMIMO_NEW_OBJECTIVE:
			switch( model->componentType ) {
				xcase UGC_COMPONENT_TYPE_OBJECT:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_Object" ));
				xcase UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_Building" ));
				xcase UGC_COMPONENT_TYPE_FAKE_DOOR: case UGC_COMPONENT_TYPE_ROOM_DOOR:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_Door" ));
				xcase UGC_COMPONENT_TYPE_CLUSTER_PART:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_ClusterPart" ));
				xcase UGC_COMPONENT_TYPE_DESTRUCTIBLE:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_Destructable" ));
				xcase UGC_COMPONENT_TYPE_KILL:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_Kill" ));
				xcase UGC_COMPONENT_TYPE_DIALOG_TREE:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_DialogTree" ));
				xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_RoomMarker" ));
				xcase UGC_COMPONENT_TYPE_PLANET:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_Planet" ));
				xcase UGC_COMPONENT_TYPE_WHOLE_MAP:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CompleteComponent_WholeMap" ));
			}
		xcase UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE:
			estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.UnlockDoor" ));
		xcase UGCMIMO_NEW_TALK_TO_OBJECTIVE:
			switch( model->componentType ) {
				xcase UGC_COMPONENT_TYPE_OBJECT:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.TalkTo_Object" ));
				xcase UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.TalkTo_Building" ));
				xcase UGC_COMPONENT_TYPE_CLUSTER_PART:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.TalkTo_ClusterPart" ));
				xcase UGC_COMPONENT_TYPE_CONTACT:
					estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.TalkTo_Contact" ));
			}

		xcase UGCMIMO_PROJECT_MAP: {
			UGCMap* map = ugcEditorGetMapByName( model->mapName );
			
			ugcFormatMessageKey( pestr, "UGC_MissionEditor.ProjectMap",
								 STRFMT_STRING( "MapName", ugcMapGetDisplayName( ugcProj, SAFE_MEMBER( map, pcName ))),
								 STRFMT_END );
		}
		xcase UGCMIMO_NEW_PROJECT_MAP:
			estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.NewProjectMap" ));
		xcase UGCMIMO_CRYPTIC_MAP:
			estrConcatf( pestr, "%s", TranslateMessageKey( "UGC_MissionEditor.CrypticMap" ));
	}
}

void ugcMissionObjectiveListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData rawDoc )
{
	UGCMissionDoc* doc = rawDoc;
	UGCMissionLibraryModel* model = eaGet( pList->peaModel, iRow );
	bool isSelected = ui_ListIsSelected( pList, NULL, iRow );
	bool isHovering = ui_ListIsHovering( pList, NULL, iRow );

	if( isHovering && mouseIsDown( MS_LEFT )) {
		ui_SetCursorByName( "UGC_Cursors_Move_Pointer" );
	}

	if( model ) {
		AtlasTex* texture = atlasFindTexture( ugcMissionLibraryModelToTexture( model ));

		{
			CBox spriteBox;
			int rgba = 0;

			if( isSelected ) {
				rgba |= 0xFFFFFFFF;
			} else {
				rgba |= 0x3e3e3e81;
			}
			BuildCBox( &spriteBox, x + 5, y + (h - texture->height) / 2, texture->width, texture->height );
			display_sprite_box( texture, &spriteBox, z, rgba );
		}

		{
			char* estr = NULL;
			ugcMissionLibraryModelText( model, &estr );
			gfxfont_PrintMaxWidth( x + texture->width + 15, y + h / 2, z, w - texture->width - 20, scale, scale, CENTER_Y, estr );
			estrDestroy( &estr );
		}
	}
}

void ugcMissionDocAddObjective( UIButton* ignored, UserData rawDoc )
{
	// To keep all the code in one place, route this throught ugcMissionTreechartDragNodeArrow()
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionLibraryModel* model = ui_ListGetSelectedObject( doc->libraryObjectivesList );

	if( model ) {
		StructCopyAll( parse_UGCMissionLibraryModel, model, &doc->newGroupModel );
		ugcMissionDocAddNewNode( doc );
	}
}

/// Add a new node to DOC, controlled by the data in DOC->NEWGROUP.
void ugcMissionDocAddNewNode( UGCMissionDoc* doc )
{
	UGCMissionNodeGroup* selectedNode = ugcMissionDocNodeGroupFindSelectedNode( doc );
	UGCMissionNodeGroup* selectedNodeMapNode;
	if( !selectedNode && eaSize( &doc->mission->objectives )) {
		UGCMissionObjective* lastObj = eaTail( &doc->mission->objectives );
		selectedNode = ugcMissionDocNodeGroupFindObjective( doc, lastObj->id );
	}
	selectedNodeMapNode = selectedNode;
	while( selectedNodeMapNode && selectedNodeMapNode->type != UGCNODE_MAP ) {
		selectedNodeMapNode = selectedNodeMapNode->parentGroup;
	}


	doc->selectedNodeMakeVisible = true;
	if(   doc->newGroupModel.type == UGCMIMO_PROJECT_MAP
		  || doc->newGroupModel.type == UGCMIMO_NEW_PROJECT_MAP
		  || doc->newGroupModel.type == UGCMIMO_CRYPTIC_MAP ) {
		ugcMissionTreechartDragNodeArrow( doc->treechart, doc, true, &doc->newGroup, selectedNodeMapNode, NULL );
	} else {
		if( selectedNode ) {
			if( selectedNode->type == UGCNODE_MAP ) {
				ugcMissionTreechartDragNodeNode( doc->treechart, doc, true, &doc->newGroup, selectedNode );
			} else {
				ugcMissionTreechartDragNodeArrow( doc->treechart, doc, true, &doc->newGroup, selectedNode, NULL );
			}
		}
	}

}

void ugcMissionDocObjectiveListDrag( UIWidget* rawList, UserData rawDoc )
{
	UIList* list = (UIList*)rawList;
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionLibraryModel* model = ui_ListGetSelectedObject( list );

	if( !model ) {
		return;
	}

	StructCopyAll( parse_UGCMissionLibraryModel, model, &doc->newGroupModel );
	ui_TreechartSetExternalDrag( doc->treechart, ugcMissionLibraryModelToTexture( &doc->newGroupModel ), &doc->newGroup );
}

void ugcMissionSetSelectedObjectiveByName( UGCMissionDoc* doc, const char* name )
{
	if( !strStartsWith( name, "Objective_" )) {
		doc->selectedNodeType = UGCNODE_DUMMY;
		doc->selectedNodeID = 0;
	} else {
		doc->selectedNodeType = UGCNODE_OBJECTIVE;
		doc->selectedNodeID = strtol( name + strlen( "Objective_" ), NULL, 10 );
	}
	doc->selectedNodeMakeVisible = true;
	doc->queueRefresh = true;
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionSetSelectedObjectiveByComponentName( UGCMissionDoc* doc, const char* name )
{
	UGCMission* mission = doc->mission;
	UGCComponent* component = ugcComponentFindByLogicalName( ugcEditorGetComponentList(), name );
	UGCMissionObjective* objective =  component ? ugcObjectiveFindComponent( mission->objectives, component->uID ) : NULL;

	if( objective ) {
		doc->selectedNodeType = UGCNODE_OBJECTIVE;
		doc->selectedNodeID = objective->id;
	}
	doc->selectedNodeMakeVisible = true;
	doc->queueRefresh = true;
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionSetSelectedObjectiveByDialogName( UGCMissionDoc* doc, const char* name )
{
	UGCMission* mission = doc->mission;
	UGCComponent* component = ugcComponentFindByLogicalName( ugcEditorGetComponentList(), name );

	if( ugcComponentStartWhenType(component) == UGCWHEN_OBJECTIVE_IN_PROGRESS ) {
		U32 objectiveID = ea32Get( &component->eaObjectiveIDs, 0 );

		if( objectiveID ) {
			doc->selectedNodeType = UGCNODE_OBJECTIVE;
			doc->selectedNodeID = objectiveID;
		}
	} else {
		doc->selectedNodeType = UGCNODE_DIALOG_TREE;
		doc->selectedNodeID = component->uID;
	}
	doc->selectedNodeMakeVisible = true;
	doc->queueRefresh = true;
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionSetSelectedDialogTreeBlock( UGCMissionDoc* doc, U32 dialog_id )
{
	doc->selectedNodeType = UGCNODE_DIALOG_TREE;
	doc->selectedNodeID = dialog_id;
	doc->selectedNodeMakeVisible = true;
	doc->queueRefresh = true;
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionSetSelectedMapTransition( UGCMissionDoc* doc, U32 map_transition_objective_id )
{
	doc->selectedNodeType = UGCNODE_MAP_TRANSITION;
	doc->selectedNodeID = map_transition_objective_id;
	doc->selectedNodeMakeVisible = true;
	doc->queueRefresh = true;
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionSetSelectedMapTransitionByMapLink( UGCMissionDoc* doc, UGCMissionMapLink* mapLink )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();

	int it;
	for( it = 0; it != eaSize( &doc->nodeGroups ); ++it ) {
		UGCMissionNodeGroup* group = doc->nodeGroups[ it ];
		UGCMapTransitionInfo* transition;
		UGCMissionObjective* objective;
		bool isInternal;
		const char* mapName;

		if( group->type != UGCNODE_MAP_TRANSITION ) {
			continue;
		}

		{
			UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );
			transition = ugcMissionFindTransitionForObjective( transitions, group->mapTransition.objectiveID );
			objective = ugcObjectiveFind( doc->mission->objectives, transition->objectiveID );
			mapName = ugcObjectiveMapName( ugcEditorGetProjectData(), objective, &isInternal );

			if( transition && mapLink == ugcMissionFindLink( ugcProj->mission, ugcProj->components,
															 isInternal ? mapName : NULL,
															 transition->prevIsInternal ? transition->prevMapName : NULL )) {
				ugcMissionSetSelectedGroup( group );
				eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
				return;
			}
			eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
		}
	}
	ugcMissionSetNoSelection( doc );
}

void ugcMissionSetSelectedMap( UGCMissionDoc* doc, U32 map_objective_id )
{
	doc->selectedNodeType = UGCNODE_MAP;
	doc->selectedNodeID = map_objective_id;
	doc->selectedNodeMakeVisible = true;
	doc->queueRefresh = true;
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionSetSelectedMapByMapLink( UGCMissionDoc* doc, UGCMissionMapLink* mapLink )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();

	int it;
	for( it = 0; it != eaSize( &doc->nodeGroups ); ++it ) {
		UGCMissionNodeGroup* group = doc->nodeGroups[ it ];
		UGCMapTransitionInfo* transition;
		UGCMissionObjective* objective;
		bool isInternal;
		const char* mapName;

		if( group->type != UGCNODE_MAP ) {
			continue;
		}

		{
			UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );
			transition = ugcMissionFindTransitionForObjective( transitions, group->map.objectiveID );
			objective = ugcObjectiveFind( doc->mission->objectives, transition->objectiveID );
			mapName = ugcObjectiveMapName( ugcEditorGetProjectData(), objective, &isInternal );

			if( transition && mapLink == ugcMissionFindLink( ugcProj->mission, ugcProj->components,
															 isInternal ? mapName : NULL,
															 transition->prevIsInternal ? transition->prevMapName : NULL )) {
				ugcMissionSetSelectedGroup( group );
				eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
				return;
			}
			eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
		}
	}
	ugcMissionSetNoSelection( doc );
}

void ugcMissionSetNoSelection( UGCMissionDoc* doc )
{
	doc->selectedNodeType = UGCNODE_DUMMY;
	doc->selectedNodeID = 0;
	ugcMissionDocRefresh( doc );
	ugcMissionUpdateFocusForSelection( doc );
}

void ugcMissionUpdateFocusForSelection( UGCMissionDoc* doc )
{
	UGCMissionNodeGroup* selectedGroup = ugcMissionDocNodeGroupFindSelectedNode( doc );
	if( SAFE_MEMBER( selectedGroup, parent )) {
		ui_SetFocus( selectedGroup->parent );
	}
}

static void ugcMissionTreechartWithFocusTick( UITreechart* treechart, UI_PARENT_ARGS )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)treechart->widget.u64;
	UI_GET_COORDINATES( treechart );

	// Make sure the treechart doesn't consume the click so we can handle
	// it.
	{
		bool oldClickThrough = treechart->widget.uClickThrough;
		treechart->widget.uClickThrough = true;
		ui_TreechartTick( treechart, UI_PARENT_VALUES );
		treechart->widget.uClickThrough = oldClickThrough;
	}

	if( !treechart->draggingNode && mouseClickHit( MS_LEFT, &box )) {
		ui_SetFocus( treechart );
		ugcMissionSetNoSelection(doc);
	}

	if ((treechart->widget.state & kWidgetModifier_Hovering) && (!treechart->widget.uClickThrough))
		inpHandled();
}

void ugcMissionDocRefresh( UGCMissionDoc* doc )
{
	MEFieldContext* uiCtx = MEContextPush( "UGCMissionEditor", NULL, NULL, NULL );
	MEFieldContextEntry* entry;
	UIPane* pane;
	UIWidget* widget;
	doc->mission = ugcEditorGetMission();

	uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
	MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
	MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
	uiCtx->iXDataStart = OBJECTIVE_DATA_COL;
	uiCtx->iYStep = UGC_ROW_HEIGHT;
	MEContextSetParent( UI_WIDGET( doc->pRootPane ));

	doc->ignoreChanges = true;
	ugcMissionModelsRefresh( doc );

	// Header
	pane = MEContextPushPaneParent( "HeaderPane" );
	{
		entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAY_MISSION, true, false );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 4, 6, 0, 0, UITopRight );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetHeight( widget, UGC_ROW_HEIGHT*1.5-12 );
	}
	MEContextPop( "HeaderPane" );
	ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, UGC_PANE_TOP_BORDER, UIUnitPercentage, UIUnitFixed );
	ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, 0, 0 );
	
	if( eaSize( &doc->mission->objectives ) == 0 ) {
		ugcMissionDocFreeTreechartAndLibrary( doc );

		pane = MEContextPushPaneParent( "FTUE" );
		{
			entry = MEContextAddLabelMsg( "Text", "UGC_MissionEditor.FTUEAddMap", NULL );
			widget = UI_WIDGET( ENTRY_LABEL( entry ));
			ENTRY_LABEL( entry )->textFrom = UITop;
			ui_WidgetSetFont( widget, "UGC_Important_Alternate" );
			ui_WidgetSetPositionEx( widget, 0, -UGC_ROW_HEIGHT, 0, 0.5, UITop );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

			entry = MEContextAddButtonMsg( "UGC_MissionEditor.AddMap", "UGC_Icons_Labels_New_02", ugcMissionShowInsertFirstMapMenuCB, doc, "Button", NULL, "UGC_MissionEditor.AddMap.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0.5, UITop );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ENTRY_BUTTON( entry )->bCenterImageAndText = true;
			widget->width = MAX( widget->width, 200 );
			widget->height = MAX( widget->height, 50 );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		}
		MEContextPop( "FTUE" );
		ui_PaneSetStyle( pane, "UGC_Story_BackgroundArea", true, false );
		ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );
	} else {
		if( !doc->libraryPane ) {
			doc->libraryPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		}
		ui_WidgetSetPositionEx( UI_WIDGET( doc->libraryPane ), 0, 0, 0, 0, UITopRight );
		ui_WidgetSetDimensionsEx( UI_WIDGET( doc->libraryPane ), UGC_LIBRARY_PANE_WIDTH_SMALL, 1, UIUnitFixed, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( doc->libraryPane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );
		ui_WidgetGroupMove( &UI_WIDGET( doc->pRootPane )->children, UI_WIDGET( doc->libraryPane ));

		if( !doc->treechart ) {
			doc->treechart = ui_TreechartCreate( doc, ugcMissionTreechartDragNodeNode, ugcMissionTreechartDragNodeArrow, NULL, ugcMissionTreechartNodeAnimate, ugcMissionTreechartDragNodeTrash, NULL, NULL, ugcMissionTreechartDragNodeNodeColumn );
		}
		doc->treechart->clipForFullWidth = true;
		UI_WIDGET(doc->treechart)->sb->scrollBoundsX = UIScrollBounds_KeepContentsAtViewCenter;
		UI_WIDGET(doc->treechart)->sb->scrollBoundsY = UIScrollBounds_KeepContentsAtViewCenter;
		ui_ScrollAreaSetNoCtrlDraggable( UI_SCROLLAREA( doc->treechart ), true );
		ui_ScrollAreaSetZoomSlider( UI_SCROLLAREA( doc->treechart ), false );
		UI_SCROLLAREA( doc->treechart )->maxZoomScale = 1;
		UI_SCROLLAREA( doc->treechart )->minZoomScale = 1;
		doc->treechart->widget.tickF = ugcMissionTreechartWithFocusTick;
		doc->treechart->widget.u64 = (U64)doc;
		ui_WidgetSetPosition( UI_WIDGET( doc->treechart ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( doc->treechart ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( doc->treechart ), 0, UGC_LIBRARY_PANE_WIDTH_SMALL, UGC_PANE_TOP_BORDER, 0 );
		ui_WidgetGroupMove( &UI_WIDGET( doc->pRootPane )->children, UI_WIDGET( doc->treechart ));
	
		ugcMissionDocRefreshTreechart( doc );

		// Refresh the library pane
		ui_PaneSetTitleHeight( doc->libraryPane, 24 );
		MEExpanderRefreshLabel( &doc->libraryObjectivesLabel, "", NULL, 0, 0, 0, UI_WIDGET( doc->libraryPane ));
		ui_WidgetSetTextMessage( UI_WIDGET( doc->libraryObjectivesLabel ), "UGC_MissionEditor.Library" );
		ui_WidgetSetFont( UI_WIDGET( doc->libraryObjectivesLabel ), "UGC_Header_Alternate" );
		ui_WidgetSetPosition( UI_WIDGET( doc->libraryObjectivesLabel ), 4, 2 );
		ui_LabelResize( doc->libraryObjectivesLabel );

		if( !doc->libraryObjectivesList ) {
			UIListColumn* customColumn = NULL;
			doc->libraryObjectivesList = ui_ListCreate( parse_UGCMissionLibraryModel, (UIModel)ugcMissionEditorObjectivesModel(), 48 );
			customColumn = ui_ListColumnCreateCallback( "", ugcMissionObjectiveListDrawCB, doc );
			ui_ListColumnSetWidth( customColumn, true, 0 );
			ui_ListAppendColumn( doc->libraryObjectivesList, customColumn );
		}
		ui_ListSetColumnsSelectable( doc->libraryObjectivesList, false );
		doc->libraryObjectivesList->fHeaderHeight = 0;
		ui_WidgetSetDragCallback( UI_WIDGET( doc->libraryObjectivesList ), ugcMissionDocObjectiveListDrag, doc );
		ugcui_TutorialNodeAssign( "MissionEditorLibrary", UI_WIDGET( doc->libraryObjectivesList ));
		ui_WidgetSetPosition( UI_WIDGET( doc->libraryObjectivesList ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( doc->libraryObjectivesList ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( doc->libraryObjectivesList ), 10, 10, 24 + 10, 10 );
		ui_WidgetGroupMove( &UI_WIDGET( doc->libraryPane )->children, UI_WIDGET( doc->libraryObjectivesList ));

		{
			UGCProjectBudget* budget = ugcFindBudget( UGC_BUDGET_TYPE_OBJECTIVE, 0 );
			int numObjectives;
			int maxDepth;
			ugcMissionObjectivesCount( ugcEditorGetProjectData(), &maxDepth, &numObjectives );
			if( budget && numObjectives >= budget->iHardLimit ) {
				ui_ListSetModel( doc->libraryObjectivesList, parse_UGCMissionLibraryModel, (UIModel)ugcMissionEditorObjectivesNullModel() );
				ui_WidgetSetTextMessage( UI_WIDGET( doc->libraryObjectivesList ), "UGC_MissionEditor.Library_TooManyObjective" );
			} else {
				ui_ListSetModel( doc->libraryObjectivesList, parse_UGCMissionLibraryModel, (UIModel)ugcMissionEditorObjectivesModel() );
			}
		}
	}

	// Refresh the properties pane
	ugcMissionDocRefreshProperties( doc );

	// Refresh the insert maps menu
	if( !doc->insertMapMenu ) {
		doc->insertMapMenu = ui_MenuCreate( NULL );
	}
	{
		int it;
		ui_MenuClear( doc->insertMapMenu );
		for( it = 0; it != eaSize( &doc->eaInsertMapModel ); ++it ) {
			UGCMissionLibraryModel* model = doc->eaInsertMapModel[ it ];
			char* estr = NULL;
			ugcMissionLibraryModelText( model, &estr );
			ui_MenuAppendItem( doc->insertMapMenu,
							   ui_MenuItemCreate( estr, UIMenuCallback, ugcMissionSelectInsertMapMenuCB, model, doc ));
			estrDestroy( &estr );
		}
	}

	doc->ignoreChanges = false;
	MEContextPop( "UGCMissionEditor" );
}

void ugcMissionDocFreeTreechartAndLibrary( UGCMissionDoc* doc )
{
	if( doc->treechart ) {
		ui_TreechartClear( doc->treechart );
	}
	eaDestroyEx( &doc->nodeGroups, ugcMissionNodeGroupDestroy );
	MEContextPush( "UGCMissionEditor", NULL, NULL, NULL );
	MEContextPush( "UGCMissionEditor_Treechart", NULL, NULL, NULL );
	{
		int it;
		for( it = 0; it != eaSize( &doc->eaContextNames ); ++it ) {
			MEContextDestroyByName( doc->eaContextNames[ it ]);
		}
		eaDestroy( &doc->eaContextNames );
	}
	MEContextPop( "UGCMissionEditor_Treechart" );
	MEContextPop( "UGCMissionEditor" );
		
	ugcMissionNodeGroupReset( &doc->dummyStartGroup );
	ugcMissionNodeGroupReset( &doc->dummyEndGroup );
	ugcMissionNodeGroupReset( &doc->returnMapLinkGroup );
	ugcMissionNodeGroupReset( &doc->rewardBoxGroup );

	ui_WidgetQueueFreeAndNull( &doc->treechart );
	ui_WidgetQueueFreeAndNull( &doc->libraryObjectivesList );
	ui_WidgetQueueFreeAndNull( &doc->libraryObjectivesLabel );
	ui_WidgetQueueFreeAndNull( &doc->libraryPane );
}

void ugcMissionDocRefreshTreechart( UGCMissionDoc* doc )
{
	doc->dummyStartGroup.doc = doc;
	doc->dummyStartGroup.type = UGCNODE_DUMMY;
	if( !doc->dummyStartGroup.dummy.label ) {
		doc->dummyStartGroup.dummy.label = ui_LabelCreate( "", 0, 0 );
	}
	doc->dummyEndGroup.type = UGCNODE_DUMMY;
	if( !doc->dummyEndGroup.dummy.label ) {
		doc->dummyEndGroup.dummy.label = ui_LabelCreate( "", 0, 0 );
	}

	{
		int groupIt = 0;
		UGCMissionNodeGroup* prevGroup = NULL;
		UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcEditorGetProjectData(), doc->mission->objectives );
		const char** eaPrevContextNames = NULL;
		eaPushEArray( &eaPrevContextNames, &doc->eaContextNames );
		eaDestroy( &doc->eaContextNames );

		MEContextPush( "UGCMissionEditor_Treechart", NULL, NULL, NULL );

		ui_TreechartBeginRefresh( doc->treechart );

		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCMissionEditor_NoDragLabel", UI_WIDGET( doc->dummyStartGroup.dummy.label )->hOverrideSkin );
		ui_LabelSetMessage( doc->dummyStartGroup.dummy.label, "UGC_MissionEditor.MissionStart" );
		doc->dummyStartGroup.dummy.label->textFrom = UITop;
		ui_WidgetSetPosition( UI_WIDGET( doc->dummyStartGroup.dummy.label ), doc->startPos[ 0 ], doc->startPos[ 1 ]);
		ui_LabelSetWidthNoAutosize( doc->dummyStartGroup.dummy.label, 380, UIUnitFixed );
		ugcMissionTreechartAddWidget( doc, NULL, UI_WIDGET( doc->dummyStartGroup.dummy.label ), "start", &doc->dummyStartGroup, TreeNode_NoDrag | TreeNode_NoSelect );

		{
			UGCComponent** missionStartPrompts = ugcComponentFindPopupPromptsForMissionStart( ugcEditorGetComponentList() );
			int it;
			for( it = 0; it != eaSize( &missionStartPrompts ); ++it ) {
				prevGroup = ugcMissionNodeGroupRefreshDialogTree( doc, &groupIt, NULL, prevGroup, missionStartPrompts[ it ]);
			}
			eaDestroy( &missionStartPrompts );
		}

		// Refresh global objectives
		//
		// MJF TODO: once we commit to having map missions placed
		// directly, this should not call
		// ugcMissionTransmogrifyObjectives(), that should instead get
		// called during fixup.
		{
			UGCMissionObjective** globalObjectives = NULL;
			int it;

			ugcMissionTransmogrifyObjectives( ugcEditorGetProjectData(), doc->mission->objectives, NULL, true, &globalObjectives );
			for( it = 0; it != eaSize( &globalObjectives ); ++it ) {
				UGCMissionObjective* globalObjective = globalObjectives[ it ];

				prevGroup = ugcMissionNodeGroupRefreshObjective(
						doc, &groupIt, NULL, prevGroup, transitions,
						globalObjective, false );
			}

			eaDestroyStruct( &globalObjectives, parse_UGCMissionObjective );
		}

		if( ugcDefaultsIsFinalRewardBoxSupported() ) {
			UGCComponent *reward_box_component = ugcComponentFindFinalRewardBox( ugcEditorGetProjectData()->components );
			if( reward_box_component ) {
				UGCMissionNodeGroup* lastChild = prevGroup->firstChildGroup;
				while( lastChild && lastChild->nextGroup ) {
					lastChild = lastChild->nextGroup;
				}
				ugcMissionRefreshRewardBox( doc, reward_box_component );
				if( lastChild ) {
					ugcMissionTreechartAddGroup( doc, lastChild, prevGroup, &doc->rewardBoxGroup, TreeNode_NoDrag | TreeNode_NoSelect );
				}
			}
		}

		ugcMissionRefreshReturnMapLink( doc );
		ugcMissionTreechartAddGroup( doc, prevGroup, NULL, &doc->returnMapLinkGroup, TreeNode_FullWidthDropTarget | TreeNode_NoDrag );

		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCMissionEditor_NoDragLabel", UI_WIDGET( doc->dummyEndGroup.dummy.label )->hOverrideSkin );
		ui_LabelSetMessage( doc->dummyEndGroup.dummy.label, "UGC_MissionEditor.MissionEnd" );
		doc->dummyEndGroup.dummy.label->textFrom = UITop;
		ui_WidgetSetPosition( UI_WIDGET( doc->dummyEndGroup.dummy.label ), doc->endPos[ 0 ], doc->endPos[ 1 ]);
		ui_LabelSetWidthNoAutosize( doc->dummyEndGroup.dummy.label, 380, UIUnitFixed );
		ugcMissionTreechartAddWidget( doc, NULL, UI_WIDGET( doc->dummyEndGroup.dummy.label ), "end", &doc->dummyEndGroup,
									  TreeNode_NoDrag | TreeNode_NoSelect );
		ui_TreechartEndRefresh( doc->treechart );

		// Destroy all left over contexts, node groups
		{
			int it;
			for( it = 0; it != eaSize( &eaPrevContextNames ); ++it ) {
				const char* prevContextName = eaPrevContextNames[ it ];
				if( eaFind( &doc->eaContextNames, prevContextName ) == -1 ) {
					MEContextDestroyByName( prevContextName );
				}
			}

			while( groupIt < eaSize( &doc->nodeGroups )) {
				assert( doc->nodeGroups );
				ugcMissionNodeGroupDestroy( doc->nodeGroups[ groupIt ]);
				eaRemove( &doc->nodeGroups, groupIt );
			}
		}
		eaDestroy( &eaPrevContextNames );

		eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
		MEContextPop( "UGCMissionEditor_Treechart" );
	}
	assert( !doc->treechart->draggingNode );

	if( ugcEditorMissionIsLoaded() ) {
		UGCMissionNodeGroup* selectedGroup = ugcMissionDocNodeGroupFindSelectedNode( doc );

		if( selectedGroup ) {
			ui_TreechartSetSelectedChild( doc->treechart, UI_WIDGET( selectedGroup->parent ), doc->selectedNodeMakeVisible );
		} else {
			doc->selectedNodeType = UGCNODE_DUMMY;
			doc->selectedNodeID = 0;
			ui_TreechartSetSelectedChild( doc->treechart, UI_WIDGET( doc->dummyStartGroup.parent ), doc->selectedNodeMakeVisible );
		}
		doc->selectedNodeMakeVisible = false;
	}
}

void ugcMissionDocRefreshProperties( UGCMissionDoc* doc )
{
	UGCMissionNodeGroup* selectedNode = ugcMissionDocNodeGroupFindSelectedNode( doc );
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIWidget* buttonWidget;
	UIScrollArea* scrollarea;

	if( !selectedNode ) {
		MEContextDestroyByName( "UGCMissionEditor_Properties" );
		ui_WidgetQueueFreeAndNull( &doc->propertiesPane );
		ui_WidgetQueueFreeAndNull( &doc->propertiesSprite );
		return;
	}

	if( !doc->propertiesPane ) {
		doc->propertiesPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	UI_WIDGET( doc->propertiesPane )->priority = UI_HIGHEST_PRIORITY;
	ui_WidgetAddToDevice( UI_WIDGET( doc->propertiesPane ), NULL );

	if( !doc->propertiesSprite ) {
		doc->propertiesSprite = ui_SpriteCreate( 0, 0, -1, -1, "white" );
	}
	UI_WIDGET( doc->propertiesSprite )->priority = UI_HIGHEST_PRIORITY;
	ui_WidgetAddToDevice( UI_WIDGET( doc->propertiesSprite ), NULL );

	if( selectedNode ) {
		MEFieldContext* uiCtx = MEContextPush( "UGCMissionEditor_Properties", NULL, NULL, NULL );
		float closeButtonPadding;
		const char* text = NULL;
		
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
		MEContextSetParent( UI_WIDGET( doc->propertiesPane ));
		MEContextSetErrorFunction( NULL );

		switch( selectedNode->type ) {
			xcase UGCNODE_OBJECTIVE:
				text = "UGC_MissionEditor.PropertiesHeader_Objective";

			xcase UGCNODE_DIALOG_TREE:
				text = "UGC_MissionEditor.PropertiesHeader_DialogTree";

			xcase UGCNODE_MAP_TRANSITION: case UGCNODE_RETURN_MAP_TRANSITION:
				text = "UGC_MissionEditor.PropertiesHeader_MapTransition";

			xcase UGCNODE_MAP:
				text = "UGC_MissionEditor.PropertiesHeader_Map";
		}

		entry = MEContextAddButton( NULL, "UGC_icon_window_controls_close", ugcMissionSetNoSelectionCB, doc, "CloseButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		closeButtonPadding = ui_WidgetGetWidth( widget );
		buttonWidget = widget;

		entry = MEContextAddLabel( "Title", TranslateMessageKey( text ), NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
		ui_LabelResize( ENTRY_LABEL( entry ));
		ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ui_WidgetSetPaddingEx( widget, 0, closeButtonPadding, 0, 0 );
		uiCtx->iYPos = ui_WidgetGetNextY( widget ) + 5;

		// Make sure the close button is centered
		ui_WidgetSetHeight( buttonWidget, ui_WidgetGetHeight( widget ));

		MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );

		{
			int y = uiCtx->iYPos;

			entry = ugcMEContextAddEditorButton( UGC_ACTION_MISSION_PLAY_SELECTION, false, false );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
			ui_WidgetSetDimensionsEx( widget, 0.5, UGC_PANE_BUTTON_HEIGHT, UIUnitPercentage, UIUnitFixed );
			ui_WidgetSetPadding( widget, 0, 0 );

			entry = ugcMEContextAddEditorButton( UGC_ACTION_MISSION_DELETE, false, false );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, y, 0.5, 0, UITopLeft );
			ui_WidgetSetDimensionsEx( widget, 0.5, UGC_PANE_BUTTON_HEIGHT, UIUnitPercentage, UIUnitFixed );
			ui_WidgetSetPadding( widget, 0, 0 );

			uiCtx->iYPos = ui_WidgetGetNextY( widget ) + 5;
		}

		scrollarea = MEContextPushScrollAreaParent( "ScrollArea" );
		ui_WidgetSetDimensionsEx( UI_WIDGET( scrollarea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		{
			switch( selectedNode->type ) {
				xcase UGCNODE_MAP:
					ugcMissionNodeGroupRefreshMapProperties( selectedNode );

				xcase UGCNODE_MAP_TRANSITION: case UGCNODE_RETURN_MAP_TRANSITION:
					ugcMissionNodeGroupRefreshMapTransitionProperties( selectedNode );

				xcase UGCNODE_OBJECTIVE:
					ugcMissionNodeGroupRefreshObjectiveProperties( selectedNode );

				xcase UGCNODE_DIALOG_TREE:
					ugcMissionNodeGroupRefreshDialogTreeProperties( selectedNode, true );					
			}
		}
		MEContextPop( "ScrollArea" );
		MEContextPop( "UGCMissionEditor_Properties" );
	}
}

void ugcMissionPropertiesOncePerFrame( UGCMissionDoc* doc )
{
	UGCMissionNodeGroup* selectedNode = ugcMissionDocNodeGroupFindSelectedNode( doc );
	float widgetScale = SAFE_MEMBER( doc->treechart, lastDrawScale ) * SAFE_MEMBER( doc->treechart, scrollArea.childScale );
	CBox selectedBox;
	float baseX;
	float baseY;

	if( !doc->propertiesPane || !selectedNode ) {
		return;
	}

	baseX = (selectedNode->parent->widget.bNoScrollX ? doc->treechart->lastDrawPixelsRect.lx : doc->treechart->lastDrawX);
	baseY = (selectedNode->parent->widget.bNoScrollY ? doc->treechart->lastDrawPixelsRect.ly : doc->treechart->lastDrawY);
	BuildCBox( &selectedBox,
			   baseX + selectedNode->parent->widget.x * widgetScale,
			   baseY + selectedNode->parent->widget.y * widgetScale,
			   selectedNode->parent->widget.width * widgetScale,
			   selectedNode->parent->widget.height * widgetScale );

	ugcEditorPlacePropertiesWidgetsForBoxes( doc->propertiesPane, doc->propertiesSprite, &doc->treechart->lastDrawPixelsRect, &selectedBox,
											 false,
											 doc->selectedNodeMakeVisible || doc->treechart->selectedNodeMakeVisible );
}

static int ugcMissionObjectivePairCmp( const UGCObjectivePriorityPair** pair1, const UGCObjectivePriorityPair** pair2 )
{
	return (*pair1)->objectiveID - (*pair2)->objectiveID;
}

void ugcObjectiveAddComponent( UIButton* ignored, UserData rawObjectiveGroup )
{
	UGCProjectData* data = ugcEditorGetProjectData();
	UGCMissionNodeGroup* group = rawObjectiveGroup;
	UGCMissionDoc *doc = group ? group->doc : NULL;
	UGCMissionObjective* objective = group ? ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID ) : NULL;
	UGCComponent* primaryComponent = objective ? ugcEditorFindComponentByID( objective->componentID ) : NULL;
	UGCComponent* newComponent;

	if( !primaryComponent ) {
		return;
	}

	// would need to create an associated contact for dialog trees
	assert( primaryComponent->eType != UGC_COMPONENT_TYPE_DIALOG_TREE );

	newComponent = ugcComponentOpCreate( data, primaryComponent->eType, 0 );
	ea32Push( &objective->extraComponentIDs, newComponent->uID );
	if( !primaryComponent->sPlacement.bIsExternalPlacement ) {
		ugcComponentOpSetPlacement(data, newComponent, ugcMapFindByName(ugcEditorGetProjectData(), primaryComponent->sPlacement.pcMapName), GENESIS_UNPLACED_ID );
	} else {
		newComponent->sPlacement.bIsExternalPlacement = true;
		StructCopyString( &newComponent->sPlacement.pcExternalMapName, primaryComponent->sPlacement.pcExternalMapName );
	}

	if(doc && objective)
	{
		group->doc->selectedNodeType = UGCNODE_OBJECTIVE;
		doc->selectedNodeID = objective->id;
	}

	ugcEditorQueueApplyUpdate();
}

void ugcObjectiveTalkToContactDialogEdit( UIButton* ignored, UserData rawObjectiveGroup )
{
	UGCMissionNodeGroup* group = rawObjectiveGroup;
	UGCMissionDoc* doc = group->doc;
	UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );
	assert( group->type == UGCNODE_OBJECTIVE );

	ugcEditorEditDialogTreeBlock( objective->componentID, -1, 0 );
}

void ugcDialogTreeBlockEdit( UIButton* ignored, UserData rawBlockGroup )
{
	UGCMissionNodeGroup* group = rawBlockGroup;
	assert( group->type == UGCNODE_DIALOG_TREE );

	ugcEditorEditDialogTreeBlock( group->dialogTree.componentID, -1, 0 );
}

void ugcMissionDialogTreeRenumberSequence( UGCComponent** dialogTrees, UGCWhenType whenType, U32 whenObjectiveID )
{
	int it;
	for( it = 0; it != eaSize( &dialogTrees ); ++it ) {
		UGCComponent* prompt = dialogTrees[ it ];
		prompt->dialogBlock.blockIndex = it;

		if( !prompt->pStartWhen ) {
			prompt->pStartWhen = StructCreate( parse_UGCWhen );
		}

		prompt->pStartWhen->eType = whenType;
		ea32Clear( &prompt->eaObjectiveIDs );
		if( whenObjectiveID ) {
			ea32Push( &prompt->eaObjectiveIDs, whenObjectiveID );
		}
		prompt->bIsDefault = false;
	}
}

void ugcMissionSetSelectedGroup(UGCMissionNodeGroup *group)
{
	UGCMissionDoc *doc = group->doc;
	UGCMissionNodeGroup* oldSelectedGroup = ugcMissionDocNodeGroupFindSelectedNode( doc );

	switch( group->type ) {
		case UGCNODE_OBJECTIVE: {
			UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );
			doc->selectedNodeType = UGCNODE_OBJECTIVE;
			doc->selectedNodeID = objective->id;
		}

		xcase UGCNODE_DIALOG_TREE:
			doc->selectedNodeType = UGCNODE_DIALOG_TREE;
			doc->selectedNodeID = group->dialogTree.componentID;

		xcase UGCNODE_MAP:
			doc->selectedNodeType = UGCNODE_MAP;
			doc->selectedNodeID = group->map.objectiveID;

		xcase UGCNODE_MAP_TRANSITION:
			doc->selectedNodeType = UGCNODE_MAP_TRANSITION;
			doc->selectedNodeID = group->mapTransition.objectiveID;

		xcase UGCNODE_RETURN_MAP_TRANSITION:
			doc->selectedNodeType = UGCNODE_RETURN_MAP_TRANSITION;
			doc->selectedNodeID = 0;
	}
	ugcMissionUpdateFocusForSelection( doc );

	{
		UGCMissionNodeGroup* selectedGroup = ugcMissionDocNodeGroupFindSelectedNode( doc );

		if( selectedGroup != oldSelectedGroup ) {
			ugcEditorQueueUIUpdate();
		}
	}
}


void ugcMissionSetSelectedGroupAndMakeVisible(UGCMissionNodeGroup *group)
{
	group->doc->selectedNodeMakeVisible = true;
	ugcMissionSetSelectedGroup( group );
}

/// MJF TODO: possibly in another way
static void ugcMissionPaneWithFocusTick(UIPane *pane, UI_PARENT_ARGS)
{
	UGCMissionNodeGroup* group = (UGCMissionNodeGroup*)pane->widget.u64;
	UGCMissionDoc* doc = group->doc;
	UI_GET_COORDINATES( pane );
	assert( !pane->viewportPane );

	// Make sure the pane doesn't consume the click so we can handle
	// it.
	{
		bool oldClickThrough = pane->widget.uClickThrough;
		pane->widget.uClickThrough = true;
		ui_PaneTick( pane, UI_PARENT_VALUES );
		pane->widget.uClickThrough = oldClickThrough;
	}

	if( !doc->treechart->draggingNode && mouseClickHit( MS_LEFT, &box )) {
		ui_SetFocus( pane );
		ugcMissionSetSelectedGroup(group);
	}

	if ((pane->widget.state & kWidgetModifier_Hovering) && (!pane->widget.uClickThrough))
		inpHandled();
}

static void ugcMissionFocusCallback(UIAnyWidget *widget, UGCMissionNodeGroup *group)
{
	if( group->doc->ignoreChanges ) {
		return;
	}

	ugcMissionSetSelectedGroup(group);
}

void ugcMissionObjectiveClearComponentCB( UIButton* ignored, UGCMissionObjectiveComponentGroup* group )
{
	UGCMissionDoc* doc = group->group->doc;
	UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->group->objective.objectiveID );

	if( group->index < 0 ) {
		if( eaiSize( &objective->extraComponentIDs )) {
			objective->componentID = objective->extraComponentIDs[ 0 ];
			eaiRemove( &objective->extraComponentIDs, 0 );
		}
	} else {
		eaiRemove( &objective->extraComponentIDs, group->index );
	}

	ugcEditorQueueApplyUpdate();
}

UGCMissionNodeGroup* ugcMissionNodeGroupRefreshObjective( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCMapTransitionInfo** transitions, UGCMissionObjective* tmogObjective, bool isFirstObjectiveOnMap )
{
	UGCComponent* component;
	char contextName[ 256 ];
	const char* iconName = NULL;
	UGCMissionNodeGroup* group = NULL;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIWidget* moverWidget;
	UIWidget* separatorWidget;

	if( tmogObjective->type == UGCOBJ_COMPLETE_COMPONENT || tmogObjective->type == UGCOBJ_UNLOCK_DOOR ) {
		component = ugcEditorFindComponentByID( tmogObjective->componentID );
	} else {
		component = NULL;
	}

	// If this is really a transition objective, we should refresh the whole map
	if( tmogObjective->type == UGCOBJ_TMOG_MAP_MISSION || tmogObjective->type == UGCOBJ_TMOG_REACH_INTERNAL_MAP ) {
		// Child nodes are all the map nodes for this transition
		//
		// MJF TODO: once we commit to having map missions place directly,
		// this should not call ugcMissionTransmogrifyObjectives(), that
		// should instead get called during fixup.
		UGCMissionObjective** mapObjectives = NULL;
		ugcMissionTransmogrifyObjectives( ugcEditorGetProjectData(), doc->mission->objectives, tmogObjective->astrMapName, true, &mapObjectives );

		group = ugcMissionNodeGroupRefreshMap( doc, pGroupIt, parentGroup, prevGroup, transitions, mapObjectives );
		eaDestroyStruct( &mapObjectives, parse_UGCMissionObjective );

		return group;
	} else if( tmogObjective->type == UGCOBJ_TMOG_REACH_CRYPTIC_MAP ) {
		return ugcMissionNodeGroupRefreshMap( doc, pGroupIt, parentGroup, prevGroup, transitions, tmogObjective->eaChildren );
	}

	sprintf( contextName, "UGCMissionEditor_Objective_%d", tmogObjective->id );
	group = ugcMissionNodeGroupIntern( doc, pGroupIt );
	group->parentGroup = parentGroup;
	group->type = UGCNODE_OBJECTIVE;
	group->objective.objectiveID = tmogObjective->id;

	// create the parent group
	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 250, 50, UIUnitFixed, UIUnitFixed, 0 );
		group->parent->widget.tickF = ugcMissionPaneWithFocusTick;
	}
	group->parent->widget.u64 = (U64)group;
	ui_PaneSetStyle( group->parent, "UGC_Button_Default_Darkbg_Idle_NoPadding", true, false );
	group->parent->invisible = false;
	ui_WidgetSetPosition( UI_WIDGET( group->parent ), tmogObjective->editorX, tmogObjective->editorY );
	ui_WidgetQueueFreeAndNull( &group->leftButton );

	{
		MEFieldContext* ctx = MEContextPush( contextName, NULL, NULL, NULL );
		const char* objectiveName = NULL;
		const char* componentName = NULL;
		char componentNameBuffer[ 256 ];
		char* estr = NULL;
		float textPaddingAccum = 0;

		eaPush( &doc->eaContextNames, allocAddString( contextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));

		if( tmogObjective->type == UGCOBJ_ALL_OF ) {
			objectiveName = TranslateMessageKey( "UGC_MissionEditor.CompleteAll" );
			iconName = "white";
		} else if( tmogObjective->type == UGCOBJ_COMPLETE_COMPONENT || tmogObjective->type == UGCOBJ_UNLOCK_DOOR ) {
			bool isTalkToObjective = false; 

			// If we are looking at a dialog tree, we want to display the Contact that is talking
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				isTalkToObjective = true;
				component = ugcEditorFindComponentByID( component->uActorID );
			}

			if( !nullStr( tmogObjective->uiString )) {
				objectiveName = tmogObjective->uiString;
			} else {
				objectiveName = TranslateMessageKey( "UGC_MissionEditor.ObjectiveNode_Default" );
			}

			if( component->sPlacement.uRoomID != GENESIS_UNPLACED_ID ) {
				ugcComponentGetDisplayName( componentNameBuffer, ugcEditorGetProjectData(), component, false );
				componentName = componentNameBuffer;
				iconName = ugcMissionObjectiveComponentTypeToTexture( component->eType, isTalkToObjective, true );
			} else {
				UGCMissionLibraryModel model = { 0 };
				ugcMissionObjectiveFillLibraryModel( tmogObjective, &model );
				ugcMissionLibraryModelText( &model, &estr );
				componentName = estr;
				iconName = ugcMissionObjectiveComponentTypeToTexture( component->eType, isTalkToObjective, true );
			}
		}

		entry = MEContextAddSprite( iconName, "Icon", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		widget->uClickThrough = true;
		ENTRY_SPRITE( entry )->tint = colorFromRGBA( 0x3e3e3e81 );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
		textPaddingAccum = MAX( textPaddingAccum, ui_WidgetGetNextX( widget ));

		entry = MEContextAddSprite( "UGC_Icons_Labels_Mover", "MoveIcon", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		widget->uClickThrough = true;
		ui_WidgetSetPositionEx( widget, 4, 0, 0, 0, UILeft );
		moverWidget = widget;

		entry = MEContextAddSeparator( "Separator" );
		widget = UI_WIDGET( entry->pSeparator );
		entry->pSeparator->orientation = UIVertical;
		ui_SeparatorResize( entry->pSeparator );
		ui_WidgetSetPosition( widget, ui_WidgetGetNextX( moverWidget ) + 2, 0 );
		separatorWidget = widget;
		textPaddingAccum = MAX( textPaddingAccum, ui_WidgetGetNextX( widget ));

		entry = ugcMEContextAddErrorSpriteForMissionObjective( ugcMissionObjectiveLogicalNameTemp( tmogObjective ));
		widget = ENTRY_WIDGET( entry );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomRight );
		
		entry = MEContextAddLabel( "ObjectiveName", objectiveName, NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ENTRY_LABEL( entry )->textFrom = UITop;
		ui_WidgetSetFont( widget, "UGC_Header_Large" );
		ui_WidgetSetPositionEx( widget, 0, 10, 0, 0, UITop );
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ui_WidgetSetPaddingEx( widget, textPaddingAccum, textPaddingAccum, 0, 0 );

		entry = MEContextAddLabel( "ComponentName", componentName, NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ENTRY_LABEL( entry )->textFrom = UITop;
		ui_WidgetSetPositionEx( widget, 0, 10, 0, 0, UIBottom );
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ui_WidgetSetPaddingEx( widget, textPaddingAccum, textPaddingAccum, 0, 0 );

		if( tmogObjective->type == UGCOBJ_ALL_OF ) {
			ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 0, 0 );
		} else {
			ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 275, 60 );
		}
		MEContextPop( contextName );

		estrDestroy( &estr );
	}

	// Add DialogTree components that are associated with this objective
	//  Do this before the iconName return as we need to display Popups for the invisible Completion
	{
		UGCComponent** beforePrompts = ugcComponentFindPopupPromptsForObjectiveStart( ugcEditorGetComponentList(), tmogObjective->id );
		int it;
		for( it = 0; it != eaSize( &beforePrompts ); ++it ) {
			prevGroup = ugcMissionNodeGroupRefreshDialogTree( doc, pGroupIt, parentGroup, prevGroup, beforePrompts[ it ]);
		}
		eaDestroy( &beforePrompts );
	}

	// If we have no icon we are not meant to be displayed.
	if( component && component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP && !ugcMissionEditorShowReachMap ) {
		return prevGroup;
	}

	// Add to the treechart
	group->astrIconName = allocAddString( iconName );
	{
		UITreechartNodeFlags flags = 0;
		if( tmogObjective->type == UGCOBJ_ALL_OF ) {
			flags |= TreeNode_BranchArrowUI;
		}

		ugcMissionTreechartAddGroup( doc, prevGroup, parentGroup, group, flags );
	}

	// Refresh children -- need to work on the tmog'ed data!
	if( tmogObjective->type == UGCOBJ_ALL_OF ) {
		int colIt;
		int childIt;

		assert( eaSize( &tmogObjective->eaChildren ));
		for( childIt = 0; childIt != eaSize( &tmogObjective->eaChildren ); ++childIt ) {
			assert( tmogObjective->eaChildren[ childIt ]->type == UGCOBJ_IN_ORDER
					|| tmogObjective->eaChildren[ childIt ]->type == UGCOBJ_TMOG_MAP_MISSION
					|| tmogObjective->eaChildren[ childIt ]->type == UGCOBJ_TMOG_REACH_INTERNAL_MAP
					|| tmogObjective->eaChildren[ childIt ]->type == UGCOBJ_TMOG_REACH_CRYPTIC_MAP );
		}
		for( colIt = 0; colIt != eaSize( &tmogObjective->eaChildren ); ++colIt ) {
			UGCMissionObjective* tmogColObjective = tmogObjective->eaChildren[ colIt ];

			if( tmogColObjective->type != UGCOBJ_IN_ORDER ) {
				ugcMissionNodeGroupRefreshObjective( doc, pGroupIt, group, NULL, transitions, tmogColObjective, false );
			} else {
				UGCMissionNodeGroup* prevChildGroup = NULL;

				for( childIt = 0; childIt != eaSize( &tmogColObjective->eaChildren ); ++childIt ) {
					prevChildGroup = ugcMissionNodeGroupRefreshObjective(
							doc, pGroupIt, group, prevChildGroup, transitions,
							tmogColObjective->eaChildren[ childIt ], false );
				}
			}
		}
	}

	prevGroup = group;

	// Potentially refresh the after prompt
	{
		UGCComponent** afterPrompts = ugcComponentFindPopupPromptsForObjectiveComplete( ugcEditorGetComponentList(), tmogObjective->id );
		int it;
		for( it = 0; it != eaSize( &afterPrompts ); ++it ) {
			prevGroup = ugcMissionNodeGroupRefreshDialogTree( doc, pGroupIt, parentGroup, prevGroup, afterPrompts[ it ]);
		}
		eaDestroy( &afterPrompts );
	}

	ui_PaneSetStyle( group->parent, "UGC_Button_Default_Darkbg_Idle_NoPadding", true, false );
	group->parent->invisible = false;
	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcMissionFocusCallback, group );
	return prevGroup;
}

static StaticDefineInt UGCWaypointModeWithoutAreaEnum[] =
{
	DEFINE_INT
	{ "UGC.WaypointNone", UGC_WAYPOINT_NONE },
	{ "UGC.WaypointPoints", UGC_WAYPOINT_POINTS },
	DEFINE_END
};

static StaticDefineInt UGCWaypointModeWithoutPointsEnum[] =
{
	DEFINE_INT
	{ "UGC.WaypointNone", UGC_WAYPOINT_NONE },
	{ "UGC.WaypointArea", UGC_WAYPOINT_AREA },
	DEFINE_END
};

void ugcMissionNodeGroupRefreshObjectiveProperties( UGCMissionNodeGroup* group )
{
	UGCMissionDoc* doc = group->doc;
	UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );
	MEFieldContext* uiCtx = MEContextPush( "Properties", objective, objective, parse_UGCMissionObjective );
	UGCRuntimeErrorContext* errorCtx = ugcMakeErrorContextObjective( ugcMissionObjectiveLogicalNameTemp( objective ), doc->mission->name );
	MEContextSetErrorContext( errorCtx );

	if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR ) {
		UGCComponent* component = ugcEditorFindComponentByID( objective->componentID );

		ugcMissionNodeGroupRefreshObjectivePropertiesBasic( group, objective, component );
		ugcMissionNodeGroupRefreshObjectivePropertiesDialogTree( group, objective, component );
		ugcMissionNodeGroupRefreshObjectivePropertiesInteract( group, objective, component );
	}

	StructDestroySafe( parse_UGCRuntimeErrorContext, &errorCtx );
	MEContextPop( "Properties" );
}

void ugcMissionNodeGroupRefreshObjectivePropertiesBasic( UGCMissionNodeGroup* group, UGCMissionObjective* objective, UGCComponent* component )
{
	bool objectiveIsInternal;
	const char* objectiveMapName = ugcObjectiveMapName( ugcEditorGetProjectData(), objective, &objectiveIsInternal );
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UIPane* pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.Properties", true );
	UGCComponent* contactComponent = NULL;
	MEFieldContextEntry* entry;
	
	if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
		contactComponent = ugcEditorFindComponentByID( component->uActorID );
	}

	MEContextAddText( false, ugcMissionObjectiveUIString( NULL ), "UIString",
					  TranslateMessageKey( "UGC_MissionEditor.MissionText" ),
					  TranslateMessageKey( "UGC_MissionEditor.MissionText.Tooltip" ));

	{
		MEFieldContext* noSortCtx = MEContextPush( "Properties", objective, objective, parse_UGCMissionObjective );
		noSortCtx->bDontSortComboEnums = true;
		if( objectiveIsInternal ) {
			if( component->eType == UGC_COMPONENT_TYPE_ROOM_MARKER ) {
				MEContextAddEnumMsg( kMEFieldType_Combo, UGCWaypointModeWithoutPointsEnum, "WaypointMode",
									 "UGC_MissionEditor.WaypointMode",  "UGC_MissionEditor.WaypointMode.Tooltip" );
			} else {
				MEContextAddEnumMsg( kMEFieldType_Combo, UGCWaypointModeEnum, "WaypointMode",
									 "UGC_MissionEditor.WaypointMode",  "UGC_MissionEditor.WaypointMode.Tooltip" );
			}
		} else {
			MEContextAddEnumMsg( kMEFieldType_Combo, UGCWaypointModeWithoutAreaEnum, "WaypointMode",
								 "UGC_MissionEditor.WaypointMode",  "UGC_MissionEditor.WaypointMode.Tooltip" );
		}

		MEContextPop( "Properties" );
	}

	if( contactComponent ) {
		MEContextPush( "ObjectiveDialogTree", component, component, parse_UGCComponent );
		ugcMEContextAddComponentPicker( TranslateMessageKey( "UGC_MissionEditor.ObjectiveComponent_Default" ), "ActorID", ugcComponentTypeGetDisplayName( contactComponent->eType, true ), TranslateMessageKey( "UGC_MissionEditor.ObjectiveComponent.Tooltip" ));
		MEContextPop( "ObjectiveDialogTree" );
	} else {
		bool supportMultiComponents = (objective->type == UGCOBJ_COMPLETE_COMPONENT
									   && (component->eType == UGC_COMPONENT_TYPE_OBJECT
										   || component->eType == UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
										   || component->eType == UGC_COMPONENT_TYPE_KILL
										   || component->eType == UGC_COMPONENT_TYPE_DESTRUCTIBLE
										   || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR
										   || component->eType == UGC_COMPONENT_TYPE_CLUSTER_PART));
		entry = ugcMEContextAddComponentPicker( TranslateMessageKey( "UGC_MissionEditor.ObjectiveComponent_Default" ), "ComponentID", ugcComponentTypeGetDisplayName( component->eType, true ), TranslateMessageKey( "UGC_MissionEditor.ObjectiveComponent.Tooltip" ));
		if( supportMultiComponents && eaiSize( &objective->extraComponentIDs )) {
			UGCMissionObjectiveComponentGroup* componentGroup = MEContextAllocMem( "ComponentIDActions", sizeof( *componentGroup ), NULL, false );
			componentGroup->group = group;
			componentGroup->index = -1;
			MEContextEntryAddActionButtonMsg( entry, NULL, "UGC_Icons_Labels_Delete", ugcMissionObjectiveClearComponentCB, componentGroup, MEContextGetCurrent()->iWidgetHeight, "UGC_MissionEditor.ObjectiveComponent_Delete.Tooltip" );
		}
		if( supportMultiComponents ) {
			MEFieldContext* multiUICtx = MEContextPush( "MultiComponent", objective, objective, parse_UGCMissionObjective );
			int it;

			multiUICtx->iYDataStart = 0;
			multiUICtx->iErrorIconOffset[ 1 ] = 3;
			multiUICtx->iYStep = UGC_ROW_HEIGHT;
			for( it = 0; it != eaiSize( &objective->extraComponentIDs ); ++it ) {
				UGCMissionObjectiveComponentGroup* componentGroup = MEContextAllocMemIndex( "ExtraComponentIDActions", it, sizeof( *componentGroup ), NULL, false );
				entry = ugcMEContextAddComponentPickerIndexMsg( "UGC_MissionEditor.ObjectiveComponent_Default", "ExtraComponentID", it, NULL, "UGC_MissionEditor.ObjectiveComponent.Tooltip" );
				componentGroup->group = group;
				componentGroup->index = it;
				MEContextEntryAddActionButton( entry, NULL, "UGC_Icons_Labels_Delete", ugcMissionObjectiveClearComponentCB, componentGroup, MEContextGetCurrent()->iWidgetHeight, "UGC_MissionEditor.ObjectiveComponent_Delete.Tooltip" );
			}
			MEContextAddButtonMsg( "UGC_MissionEditor.ObjectiveComponent_Add", NULL, ugcObjectiveAddComponent, group, "ComponentAdd", NULL, "UGC_MissionEditor.ObjectiveComponent_Add.Tooltip" );

			MEContextPop( "MultiComponent" );
		}
	}

	if( !objectiveIsInternal ) {
		ZoneMapInfo* zminfo = zmapInfoGetByPublicName( objectiveMapName );
		MEContextAddTwoLabelsMsg( "ComponentMap", "UGC_MissionEditor.ObjectiveCrypticMap", zmapInfoGetDisplayNameMsgKey( zminfo ), NULL );
	}

	if( component->eType == UGC_COMPONENT_TYPE_ROOM_MARKER || component->eType == UGC_COMPONENT_TYPE_PLANET ) {
		MEContextAddTextMsg( false, "UGC_MissionEditor.ReachText_Default", "SuccessFloaterText", "UGC_MissionEditor.ReachText", "UGC_MissionEditor.ReachText.Tooltip" );
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	MEContextPop( __FUNCTION__ );
}

void ugcMissionNodeGroupRefreshObjectivePropertiesDialogTree( UGCMissionNodeGroup* group, UGCMissionObjective* objective, UGCComponent* component )
{
	UGCRuntimeErrorContext* errorCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL );
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UIPane* pane;
	UGCComponent* contactComponent;
	MEFieldContext* uiCtx;

	if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
		return;
	}

	contactComponent = ugcEditorFindComponentByID( component->uActorID );
	if( contactComponent->eType == UGC_COMPONENT_TYPE_CONTACT ) {
		pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.DialogTree", true );
	} else {
		pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.DialogTreeObject", true );
	}
	
	// NOTE: MissionEditor Objective nodes duplicate the text field from the
	// corresponding DialogTree editor.  This list MUST be kept in sync with
	// ugcErrorAddKeysForMissionObjective()'s s filtering of dialog-data!
	uiCtx = MEContextPush( "Prompt", &component->dialogBlock.initialPrompt, &component->dialogBlock.initialPrompt, parse_UGCDialogTreePrompt );
	MEContextSetErrorContext( errorCtx );

	if( contactComponent->eType == UGC_COMPONENT_TYPE_CONTACT ) {
		ugcMEContextAddResourcePickerMsg( "AIAnimList", "UGC_MissionEditor.DialogAnimation_Default", "UGC_MissionEditor.DialogAnimation_PickerTitle", true, "PromptStyle", "UGC_MissionEditor.DialogAnimation", "UGC_MissionEditor.DialogAnimation.Tooltip" );
	}
	ugcMEContextAddMultilineTextMsg( "PromptBody", "UGC_MissionEditor.DialogText", "UGC_MissionEditor.DialogText.Tooltip" );
	MEContextAddButtonMsg( "UGC_MissionEditor.AdvancedDialogEditor", NULL, ugcObjectiveTalkToContactDialogEdit, group,
						   "PromptGoToAdvanced", NULL, "UGC_MissionEditor.AdvancedDialogEditor.Tooltip" );
	MEContextPop( "Prompt" );

	if( ugcIsMissionItemsEnabled() ) {
		ugcMEContextAddResourcePickerMsg( "MissionItem", "UGC_MissionEditor.DropItem_Default", "UGC_MissionEditor.DropItem_PickerTitle", true, "DropItem", "UGC_MissionEditor.DropItem", "UGC_MissionEditor.DropItem.Tooltip" );
	}
	
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	MEContextPop( __FUNCTION__ );

	StructDestroySafe( parse_UGCRuntimeErrorContext, &errorCtx );
}

void ugcMissionNodeGroupRefreshObjectivePropertiesInteract( UGCMissionNodeGroup* group, UGCMissionObjective* objective, UGCComponent* component )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UIPane* pane;

	if(   component->eType != UGC_COMPONENT_TYPE_OBJECT && component->eType != UGC_COMPONENT_TYPE_CLUSTER_PART
		  && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR ) {
		return;
	}

	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.Interact", true );

	if(   component->eType == UGC_COMPONENT_TYPE_OBJECT || component->eType == UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
		  || component->eType == UGC_COMPONENT_TYPE_CLUSTER_PART ) {
		ugcRefreshInteractProperties(
				&objective->sInteractProps, ugcComponentMapType( ugcEditorGetProjectData(), component ),
				UGCINPR_BASIC | UGCINPR_CUSTOM_ANIM_AND_DURATION | UGCINPR_CHECKED_ATTRIB_ITEMS );
	} else if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR ) {
		ugcRefreshInteractProperties(
				&objective->sInteractProps, ugcComponentMapType( ugcEditorGetProjectData(), component ),
				UGCINPR_BASIC | UGCINPR_CHECKED_ATTRIB_ITEMS );
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	MEContextPop( __FUNCTION__ );
}

UGCMissionNodeGroup* ugcMissionNodeGroupRefreshDialogTree( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCComponent* component )
{
	const char* styleBorderName = (eaSize(&component->dialogBlock.prompts) ? "CarbonFibre_TreeChartDialogStack" : "CarbonFibre_TreeChartDialog");
	char contextName[ 256 ];
	UGCMissionNodeGroup* group = ugcMissionNodeGroupIntern( doc, pGroupIt );

	group->type = UGCNODE_DIALOG_TREE;
	group->dialogTree.componentID = component->uID;

	sprintf( contextName, "UGCMissionEditor_DialogTree_%d", component->uID );

	// create the parent group
	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 250, 50, UIUnitFixed, UIUnitFixed, 0 );
		group->parent->widget.tickF = ugcMissionPaneWithFocusTick;
	}
	group->parent->widget.u64 = (U64)group;
	ui_WidgetSetPosition( UI_WIDGET( group->parent ), component->dialogBlock.editorX, component->dialogBlock.editorY );
	ui_WidgetQueueFreeAndNull( &group->leftButton );

	{
		MEFieldContext* ctx = MEContextPush( contextName, NULL, NULL, NULL );
		eaPush( &doc->eaContextNames, allocAddString( contextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));
		ugcMissionNodeGroupRefreshDialogTreeProperties( group, false );
		MEContextPop( contextName );

		ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 375, ctx->iYPos + ui_StyleBorderHeight( ui_StyleBorderGet( styleBorderName )));
	}

	group->astrIconName = allocAddString( "white" );
	ugcMissionTreechartAddGroup( doc, prevGroup, parentGroup, group, 0 );
	ui_PaneSetStyleEx( group->parent, styleBorderName, NULL, true, false );
	group->parent->invisible = false;
	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcMissionFocusCallback, group );

	return group;
}

void ugcMissionNodeGroupRefreshDialogTreeProperties( UGCMissionNodeGroup* group, bool isPropertiesPane )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCMissionDoc* doc = group->doc;
	UGCComponent* component = ugcEditorFindComponentByID( group->dialogTree.componentID );
	MEFieldContext* ctx = MEContextPush( "DTreeProperties", component, component, parse_UGCComponent );
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UIPane* pane;
	MEContextSetErrorContext( ugcMakeTempErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL ));
	
	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.Properties", true );

	// Disabled because it's not active in NW
	// if( ugcDefaultsDialogStyle() == UGC_DIALOG_STYLE_WINDOW ) {
	// 	MEContextAddSimple( kMEFieldType_TextEntry, "PromptTitle", "Title", NULL );
	// }

	// ugcMEContextAddMultilineText( "PromptBody", "Text", NULL );
	// MEContextAddButton( "Advanced Dialog Editor", NULL, ugcDialogTreeBlockEdit, group,
	// 					"PromptGoToAdvanced", NULL, NULL );

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	MEContextPop( __FUNCTION__ );

	MEContextPop( "DTreeProperties" );
}

void ugcMissionMapMEFieldChangedCB( MEField* pField, bool finished, UGCMissionNodeGroup* group )
{
	if( finished && !group->doc->ignoreChanges ) {
		UGCMissionMapLink* link = ugcMissionFindLink( group->doc->mission, ugcEditorGetComponentList(),
													  resNamespaceIsUGC( group->map.nextMapName ) ? group->map.nextMapName : NULL,
													  resNamespaceIsUGC( group->map.prevMapName ) ? group->map.prevMapName : NULL );
		
		if( !link ) {
			link = StructCreate( parse_UGCMissionMapLink );
			eaPush( &group->doc->mission->map_links, link );
		}


		StructCopyAll( parse_UGCMissionMapLink, &group->map.editingLink, link );
		ugcEditorQueueApplyUpdate();
	}
}

void ugcMissionTransitionMEFieldChangedCB( MEField* pField, bool finished, UGCMissionNodeGroup* group )
{
	if( finished && !group->doc->ignoreChanges ) {
		UGCMissionMapLink* link;

		if( group->type == UGCNODE_RETURN_MAP_TRANSITION ) {
			link = group->doc->mission->return_map_link;
			assert( link );
		} else {
			link = ugcMissionFindLink( group->doc->mission, ugcEditorGetComponentList(),
									   resNamespaceIsUGC( group->mapTransition.nextMapName ) ? group->mapTransition.nextMapName : NULL,
									   resNamespaceIsUGC( group->mapTransition.prevMapName ) ? group->mapTransition.prevMapName : NULL );

			if( !link ) {
				link = StructCreate( parse_UGCMissionMapLink );
				eaPush( &group->doc->mission->map_links, link );
			}
		}


		StructCopyAll( parse_UGCMissionMapLink, &group->mapTransition.editingLink, link );
		ugcEditorQueueApplyUpdate();
	}
}

UGCMissionNodeGroup* ugcMissionNodeGroupRefreshMap( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCMapTransitionInfo** transitions, UGCMissionObjective** mapObjectives )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	const char* styleBorderName = "UGC_Button_Default_Darkbg_Idle";
	const char* mapName = ugcObjectiveInternalMapName( ugcProj, mapObjectives[ 0 ]);
	UGCMapTransitionInfo* transition = ugcMissionFindTransitionForObjective( transitions, mapObjectives[ 0 ]->id );
	UGCMissionNodeGroup* group = ugcMissionNodeGroupIntern( doc, pGroupIt );
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIWidget* moverWidget;
	char mapContextName[ 256 ];
	char mapBlankSlateName[ 256 ];

	sprintf( mapContextName, "UGCMissionEditor_Map_Objective%d", mapObjectives[ 0 ]->id );
	sprintf( mapBlankSlateName, "UGCMapEditor_MapBlankSlate_Objective%d", mapObjectives[ 0 ]->id );

	// Fill out prevMapName, nextMapName, and objectiveID
	group->type = UGCNODE_MAP;
	SAFE_FREE( group->map.prevMapName );
	SAFE_FREE( group->map.nextMapName );
	group->map.objectiveID = mapObjectives[ 0 ]->id;
	{
		if( transition && transition->prevMapName ) {
			if( transition->prevIsInternal ) {
				group->map.prevMapName = strdupf( "%s:%s", ugcProj->ns_name, transition->prevMapName );
			} else {
				group->map.prevMapName = strdup( transition->prevMapName );
			}
		} else {
			group->map.prevMapName = NULL;
		}
		if( mapName ) {
			group->map.nextMapName = strdupf( "%s:%s", ugcEditorGetProjectData()->ns_name, mapName );
		} else {
			group->map.nextMapName = strdup( mapName );
		}
	}

	prevGroup = ugcMissionNodeGroupRefreshMapTransition( doc, pGroupIt, parentGroup, prevGroup, transitions, mapObjectives );
	
	// Setup the editing link
	StructReset( parse_UGCMissionMapLink, &group->map.editingLink );
	if( transition ) {
		UGCMissionMapLink* existingLink = ugcMissionFindLink( doc->mission, ugcEditorGetComponentList(),
															  mapName,
															  transition->prevIsInternal ? transition->prevMapName : NULL );
		if( existingLink ) {
			StructCopyAll( parse_UGCMissionMapLink, existingLink, &group->map.editingLink );
			if( group->map.editingLink.pDialogPrompt ) {
				eaSetSizeStruct( &group->map.editingLink.pDialogPrompt->eaActions, parse_UGCDialogTreePromptAction, 1 );
			}
		} else {
			UGCPerProjectDefaults* config = ugcGetDefaults();
			UGCMissionMapLink* defaultLink = &group->map.editingLink;

			if( config->pDefaultTransitionPrompt ) {
				defaultLink->pDialogPrompt = StructClone( parse_UGCDialogTreePrompt, config->pDefaultTransitionPrompt );
				devassert( defaultLink->pDialogPrompt );
				StructFreeStringSafe( &defaultLink->pDialogPrompt->pcPromptBody );
				eaSetSizeStruct( &defaultLink->pDialogPrompt->eaActions, parse_UGCDialogTreePromptAction, 1 );
			}

			// Find Spawn component on nextMap
			if( resNamespaceIsUGC( group->map.nextMapName )) {
				char ns[ RESOURCE_NAME_MAX_SIZE ];
				char internalMapName[ RESOURCE_NAME_MAX_SIZE ];
				resExtractNameSpace( group->map.nextMapName, ns, internalMapName );
				StructCopyString( &defaultLink->strSpawnInternalMapName, internalMapName );
			}

			// Find Whole Map component on prevMap
			{
				UGCComponent *component;
				char* prevMapName = group->map.prevMapName;
				if( !prevMapName ) {
					UGCPerAllegianceDefaults* allegianceDef = ugcGetAllegianceDefaults( ugcProj );
					if( allegianceDef ) {
						prevMapName = allegianceDef->pcDefaultCrypticMap;
					}
				}
				if( !prevMapName ) {
					prevMapName = "Neverwinter_Protectors_Enclave";
				}

				component = ugcMissionGetDefaultComponentForMap( ugcProj, UGC_COMPONENT_TYPE_WHOLE_MAP, prevMapName );
				if( component ) {
					defaultLink->uDoorComponentID = component->uID;
				} else {
					StructDestroy( parse_UGCMissionMapLink, existingLink );
					Alertf("Internal error");
					return prevGroup;
				}
			}
		}
	} else {
		assert( !mapName );
	}

	// create the parent group
	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		group->parent->widget.tickF = ugcMissionPaneWithFocusTick;
	}
	group->parent->widget.u64 = (U64)group;
	ui_WidgetQueueFreeAndNull( &group->leftButton );

	{
		MEFieldContext* ctx = MEContextPush( mapContextName, NULL, NULL, NULL );
		eaPush( &doc->eaContextNames, allocAddString( mapContextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));

		{
			UGCMap* map = ugcEditorGetMapByName( group->map.nextMapName );
			char* estr = NULL;
			UIWidget* textWidget;
			UIWidget* separatorWidget;

			if( map ) {
				ugcFormatMessageKey( &estr, "UGC_MissionEditor.MapNode_Project",
									 STRFMT_STRING( "MapName", ugcMapGetDisplayName( ugcProj, map->pcName )),
									 STRFMT_END );
			} else {
				ugcFormatMessageKey( &estr, "UGC_MissionEditor.MapNode_Cryptic", STRFMT_END );
			}

			entry = MEContextAddLabel( "MapName", estr, NULL );
			widget = UI_WIDGET( ENTRY_LABEL( entry ));
			ENTRY_LABEL( entry )->bRotateCCW = true;
			ENTRY_LABEL( entry )->textFrom = UIBottom;
			ENTRY_LABEL( entry )->bNoAutosizeWidth = true;
			ui_WidgetSetFont( widget, "UGC_Header_Large" );
			ui_WidgetSetPosition( widget, 0, 0 );
			ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
			textWidget = widget;

			entry = MEContextAddSprite( "UGC_Icons_Labels_Mover", "MoveIcon", NULL, NULL );
			widget = UI_WIDGET( ENTRY_SPRITE( entry ));
			widget->uClickThrough = true;
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITop );
			moverWidget = widget;

			entry = MEContextAddSeparator( "Separator" );
			widget = UI_WIDGET( entry->pSeparator );
			entry->pSeparator->orientation = UIHorizontal;
			ui_SeparatorResize( entry->pSeparator );
			ui_WidgetSetPosition( widget, 0, ui_WidgetGetNextY( moverWidget ) + 2 );
			separatorWidget = widget;

			entry = ugcMEContextAddErrorSpriteForMissionMap( group->map.nextMapName, ugcMissionObjectiveIDLogicalNameTemp( group->map.objectiveID ));
			widget = ENTRY_WIDGET( entry );
			ui_WidgetSetPositionEx( widget, 0, ui_WidgetGetNextY( separatorWidget ) + 2, 0, 0, UITop );
			
			ui_WidgetSetPaddingEx( textWidget, 0, 0, ui_WidgetGetNextY( widget ), 0 );

			estrDestroy( &estr );
		}

		MEContextPop( mapContextName );
	}

	group->astrIconName = allocAddString( "ugc_icon_map" );
	ugcMissionTreechartAddGroup( doc, prevGroup, parentGroup, group,
								 TreeNode_FullWidthContainerUI | TreeNode_NoArrowBefore | TreeNode_NoArrowAfter );

	ui_PaneSetStyleEx( group->parent, styleBorderName, styleBorderName, true, false );
	group->parent->invisible = false;
	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcMissionFocusCallback, group );

	// Refresh all the child nodes on this map
	{
		UGCMissionNodeGroup* prevMapGroup = NULL;
		int it;

		eaiClear( &group->map.objectiveIDs );

		for( it = 0; it != eaSize( &mapObjectives ); ++it ) {
			UGCMissionObjective* mapObjective = mapObjectives[ it ];

			prevMapGroup = ugcMissionNodeGroupRefreshObjective(
					doc, pGroupIt, group, prevMapGroup, transitions,
					mapObjective, (it == 0) );
			eaiPush( &group->map.objectiveIDs, mapObjective->id );
		}

		// If there are no objectives on this map, we should place a
		// blank slate to tell you what to do.
		if( !prevMapGroup ) {
			UGCMissionNodeGroup* ftueGroup = ugcMissionNodeGroupIntern( doc, pGroupIt );
			
			ftueGroup->type = UGCNODE_MAP_FTUE;
			ftueGroup->mapFtue.objectiveID = group->map.objectiveID;

			// Create the parent group
			if( !ftueGroup->parent ) {
				ftueGroup->parent = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
				ftueGroup->parent->widget.tickF = ugcMissionPaneWithFocusTick;
			}
			ftueGroup->parent->widget.u64 = (U64)ftueGroup;
			ui_WidgetQueueFreeAndNull( &group->leftButton );

			{
				MEFieldContext* uiCtx = MEContextPush( mapBlankSlateName, NULL, NULL, NULL );
				eaPush( &doc->eaContextNames, allocAddString( mapBlankSlateName ));
				MEContextSetParent( UI_WIDGET( ftueGroup->parent ));
				uiCtx->astrOverrideSkinName = "UGCMissionEditor_NoDrag";
				{
					entry = MEContextAddLabelMsg( "Text", "UGC_MissionEditor.FTUEAddObjective", NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					ENTRY_LABEL( entry )->textFrom = UINoDirection;
					ui_WidgetSetPosition( widget, 0, 0 );
					ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
					ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
				}
				MEContextPop( mapBlankSlateName );
			}

			ftueGroup->parent->invisible = true;
			ui_WidgetSetDimensions( UI_WIDGET( ftueGroup->parent ), 300, 80 );
			ugcMissionTreechartAddGroup( doc, NULL, group, ftueGroup, TreeNode_NoDrag | TreeNode_NoSelect | TreeNode_NoArrowBefore | TreeNode_NoArrowAfter );
		}
	}

	return group;
}

UGCMissionNodeGroup* ugcMissionNodeGroupRefreshMapTransition( UGCMissionDoc* doc, int* pGroupIt, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* prevGroup, UGCMapTransitionInfo** transitions, UGCMissionObjective** mapObjectives )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	const char* styleBorderName = "UGC_Button_NoGradient_Darkbg_Idle_NoPadding";
	const char* mapName = ugcObjectiveInternalMapName( ugcProj, mapObjectives[ 0 ]);
	UGCMapTransitionInfo* transition = ugcMissionFindTransitionForObjective( transitions, mapObjectives[ 0 ]->id );
	UGCMissionNodeGroup* group = ugcMissionNodeGroupIntern( doc, pGroupIt );
	char contextName[ 256 ];
	MEFieldContextEntry* entry;
	UIWidget* widget;

	sprintf( contextName, "UGCMissionEditor_MapTransition_Objective%d", mapObjectives[ 0 ]->id );

	// Fill out prevMapName, nextMapName, and objectiveID
	group->type = UGCNODE_MAP_TRANSITION;
	SAFE_FREE( group->mapTransition.prevMapName );
	SAFE_FREE( group->mapTransition.nextMapName );
	group->mapTransition.objectiveID = mapObjectives[ 0 ]->id;
	{
		if( transition && transition->prevMapName ) {
			if( transition->prevIsInternal ) {
				group->mapTransition.prevMapName = strdupf( "%s:%s", ugcProj->ns_name, transition->prevMapName );
			} else {
				group->mapTransition.prevMapName = strdup( transition->prevMapName );
			}
		} else {
			group->mapTransition.prevMapName = NULL;
		}
		if( mapName ) {
			group->mapTransition.nextMapName = strdupf( "%s:%s", ugcEditorGetProjectData()->ns_name, mapName );
		} else {
			group->mapTransition.nextMapName = NULL;
		}
	}

	// Setup the editing link
	StructReset( parse_UGCMissionMapLink, &group->mapTransition.editingLink );
	if( transition ) {
		UGCMissionMapLink* existingLink = ugcMissionFindLink( doc->mission, ugcEditorGetComponentList(),
															  mapName,
															  transition->prevIsInternal ? transition->prevMapName : NULL );
		if( existingLink ) {
			StructCopyAll( parse_UGCMissionMapLink, existingLink, &group->mapTransition.editingLink );
			if( group->mapTransition.editingLink.pDialogPrompt ) {
				eaSetSizeStruct( &group->mapTransition.editingLink.pDialogPrompt->eaActions, parse_UGCDialogTreePromptAction, 1 );
			}
		} else {
			UGCPerProjectDefaults* config = ugcGetDefaults();
			UGCMissionMapLink* defaultLink = &group->mapTransition.editingLink;

			if( config->pDefaultTransitionPrompt ) {
				defaultLink->pDialogPrompt = StructClone( parse_UGCDialogTreePrompt, config->pDefaultTransitionPrompt );
				devassert( defaultLink->pDialogPrompt );
				StructFreeStringSafe( &defaultLink->pDialogPrompt->pcPromptBody );
				eaSetSizeStruct( &defaultLink->pDialogPrompt->eaActions, parse_UGCDialogTreePromptAction, 1 );
			}

			// Find Spawn component on nextMap
			if( resNamespaceIsUGC( group->mapTransition.nextMapName )) {
				char ns[ RESOURCE_NAME_MAX_SIZE ];
				char internalMapName[ RESOURCE_NAME_MAX_SIZE ];
				resExtractNameSpace( group->mapTransition.nextMapName, ns, internalMapName );
				StructCopyString( &defaultLink->strSpawnInternalMapName, internalMapName );
			}

			// Find Whole Map component on prevMap
			{
				UGCComponent *component;
				char* prevMapName = group->mapTransition.prevMapName;
				if( !prevMapName ) {
					UGCPerAllegianceDefaults* allegianceDef = ugcGetAllegianceDefaults( ugcProj );
					if( allegianceDef ) {
						prevMapName = allegianceDef->pcDefaultCrypticMap;
					}
				}
				if( !prevMapName ) {
					prevMapName = "Neverwinter_Protectors_Enclave";
				}

				component = ugcMissionGetDefaultComponentForMap( ugcProj, UGC_COMPONENT_TYPE_WHOLE_MAP, prevMapName );
				if( component ) {
					defaultLink->uDoorComponentID = component->uID;
				} else {
					StructDestroy( parse_UGCMissionMapLink, existingLink );
					Alertf("Internal error");
					return prevGroup;
				}
			}
		}
	} else {
		assert( !mapName );
	}

	// create the parent group
	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 250, 1, UIUnitFixed, UIUnitFixed, 0 );
		group->parent->widget.tickF = ugcMissionPaneWithFocusTick;
	}
	group->parent->widget.u64 = (U64)group;

	if( !group->leftButton ) {
		group->leftButton = ui_ButtonCreate( "", 0, 0, NULL, NULL );
	}
	ui_WidgetSetTextMessage( UI_WIDGET( group->leftButton ), "UGC_MissionEditor.AddMap" );
	ui_WidgetSetTooltipMessage( UI_WIDGET( group->leftButton ), "UGC_MissionEditor.AddMap.Tooltip" );
	ui_ButtonSetImage( group->leftButton, "UGC_Icons_Labels_New_02" );
	ui_ButtonResize( group->leftButton );
	ui_ButtonSetCallback( group->leftButton, ugcMissionShowInsertMapMenuCB, group );

	{
		MEFieldContext* ctx = MEContextPush( contextName, NULL, NULL, NULL );
		UIWidget* textWidget;

		eaPush( &doc->eaContextNames, allocAddString( contextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));

		entry = MEContextAddLabelMsg( "Text", "UGC_MissionEditor.MapTransitionNode", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ui_WidgetSetFont( widget, "UGC_Header_Large" );
		ui_LabelResize( ENTRY_LABEL( entry ));
		ui_WidgetSetPositionEx( widget, 0, 8, 0, 0, UITop );
		textWidget = widget;
		
		entry = ugcMEContextAddErrorSpriteForMissionMapTransition( ugcMissionObjectiveIDLogicalNameTemp( group->mapTransition.objectiveID ));
		widget = ENTRY_WIDGET( entry );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomRight );

		entry = MEContextAddSprite( "UGC_Icons_Story_Objectives_Transfer_Tilted", "Icon", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		widget->uClickThrough = true;
		ENTRY_SPRITE( entry )->tint = colorFromRGBA( 0x3e3e3e81 );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );

		ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 275, 40 );
		MEContextPop( contextName );
	}
	ugcMissionTreechartAddGroup( doc, prevGroup, parentGroup, group,
								 TreeNode_FullWidthDropTarget | TreeNode_NoDrag );

	ui_PaneSetStyleEx( group->parent, styleBorderName, styleBorderName, true, false );
	group->parent->invisible = false;
	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcMissionFocusCallback, group );

	// Setup objectiveIDs
	eaiClear( &group->mapTransition.objectiveIDs );
	{
		int it;
		for( it = 0; it != eaSize( &mapObjectives ); ++it ) {
			UGCMissionObjective* mapObjective = mapObjectives[ it ];
			eaiPush( &group->mapTransition.objectiveIDs, mapObjective->id );
		}
	}

	return group;
}

/// Special case to refresh the final map link.
static void ugcMissionRefreshReturnMapLink( UGCMissionDoc* doc )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	const char* styleBorderName = "UGCMissionEditor_NoDrag";
	UGCMissionNodeGroup* group = &doc->returnMapLinkGroup;
	char contextName[ 256 ];
	MEFieldContextEntry* entry;
	UIWidget* widget;

	sprintf( contextName, "UGCMapEditor_MapTransition_ReturnMapLink" );

	group->doc = doc;
	group->type = UGCNODE_RETURN_MAP_TRANSITION;
	SAFE_FREE( group->mapTransition.prevMapName );

	// Setup the editing link
	if( doc->mission->return_map_link ) {
		UGCComponent* component = ugcEditorFindComponentByID( doc->mission->return_map_link->uDoorComponentID );
		StructCopyAll( parse_UGCMissionMapLink, doc->mission->return_map_link, &group->mapTransition.editingLink );
		assert( component && !component->sPlacement.bIsExternalPlacement );
		group->mapTransition.prevMapName = strdupf( "%s:%s", ugcProj->ns_name, component->sPlacement.pcMapName );
	} else {
		group->mapTransition.prevMapName = NULL;
	}

	// create the parent group
	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 250, 50, UIUnitFixed, UIUnitFixed, 0 );
		group->parent->widget.tickF = ugcMissionPaneWithFocusTick;
	}
	group->parent->widget.u64 = (U64)group;
	
	if( !group->leftButton ) {
		group->leftButton = ui_ButtonCreate( "", 0, 0, NULL, NULL );
	}
	ui_WidgetSetTextMessage( UI_WIDGET( group->leftButton ), "UGC_MissionEditor.AddMap" );
	ui_WidgetSetTooltipMessage( UI_WIDGET( group->leftButton ), "UGC_MissionEditor.AddMap.Tooltip" );
	ui_ButtonSetImage( group->leftButton, "UGC_Icons_Labels_New_02" );
	ui_ButtonResize( group->leftButton );
	ui_ButtonSetCallback( group->leftButton, ugcMissionShowInsertMapMenuCB, group );

	{
		MEFieldContext* ctx = MEContextPush( contextName, NULL, NULL, NULL );
		eaPush( &doc->eaContextNames, allocAddString( contextName ));

		MEContextSetParent( UI_WIDGET( group->parent ));

		entry = MEContextAddLabelMsg( "Text", "UGC_MissionEditor.MapTransitionNode", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ui_WidgetSetFont( widget, "UGC_Header_Large" );
		ui_LabelResize( ENTRY_LABEL( entry ));
		ui_WidgetSetPositionEx( widget, 0, 8, 0, 0, UITop );

		entry = MEContextAddSprite( "UGC_Icons_Story_Objectives_Transfer_Tilted", "Icon", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		widget->uClickThrough = true;
		ENTRY_SPRITE( entry )->tint = colorFromRGBA( 0x3e3e3e81 );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );

		ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 275, 40 );
		MEContextPop( contextName );
	}
}

static void ugcMissionRewardChestSetCB( const char* zmapName, const char* logicalName, const float* overworldPos, const char* overworldIcon, UGCComponent *reward_box_component )
{
	if(zmapName && logicalName)
	{
		StructCopyString(&reward_box_component->sPlacement.pcExternalMapName, zmapName);
		StructCopyString(&reward_box_component->sPlacement.pcExternalObjectName, logicalName);

		ugcEditorQueueApplyUpdate();
	}
}

static void ugcMissionRewardChestPickCB( UIButton* ignored, UGCMissionNodeGroup* group )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent *reward_box_component = ugcComponentFindFinalRewardBox( ugcProj->components );

	if(reward_box_component)
	{
		if( reward_box_component->sPlacement.bIsExternalPlacement ) {
			char defaultMap[ RESOURCE_NAME_MAX_SIZE ];
			sprintf( defaultMap, "%s", reward_box_component->sPlacement.pcExternalMapName ? reward_box_component->sPlacement.pcExternalMapName : "" );

			if( !ugcZeniPickerShow( NULL,
									UGCZeniPickerType_Reward_Box,
									defaultMap, reward_box_component->sPlacement.pcExternalObjectName,
									ugcEditorEncObjFilter, NULL, ugcMissionRewardChestSetCB, reward_box_component )) {
				ugcModalDialogMsg( "UGC_Editor.NoComponents_Cryptic", "UGC_Editor.NoComponents_CrypticDetails", UIOk );
			}
		}
		else
		{
			// select reward chest and show map pane, centered on reward chest
			ugcEditorEditMapComponent(reward_box_component->sPlacement.pcMapName, reward_box_component->uID, false, true);
		}
	}
}

/// Special case to refresh the reward box.
static void ugcMissionRefreshRewardBox( UGCMissionDoc* doc, UGCComponent *reward_box_component )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	const char* styleBorderName = "UGC_Story_Flow_Block";
	UGCMissionNodeGroup* group = &doc->rewardBoxGroup;
	char contextName[ 256 ];
	MEFieldContextEntry* entry;
	UIWidget* widget;
	MEFieldContext* uiCtx;

	sprintf( contextName, "UGCMapEditor_RewardBox" );

	group->doc = doc;
	group->type = UGCNODE_REWARD_BOX;

	// create the parent group
	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 250, 50, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetQueueFreeAndNull( &group->leftButton );

	uiCtx = MEContextPush( "RewardBox", NULL, NULL, NULL );
	uiCtx->astrOverrideSkinName = "UGCMissionEditor_NoDrag";
	MEContextSetParent( UI_WIDGET( group->parent ));

	entry = MEContextAddLabelMsg( "Header", "UGC_MissionEditor.RewardChest", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ENTRY_LABEL( entry )->textFrom = UITop;
	ui_WidgetSetPosition( widget, 0, 0 );
	ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

	entry = MEContextAddButtonMsg( NULL, "UGC_Widgets_Hyperlink_Light_Idle", ugcMissionRewardChestPickCB, group, "RewardLink", NULL, "UGC_MissionEditor.PickRewardChest.Tooltip" );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIRight );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

	ui_PaneSetStyle( group->parent, styleBorderName, true, false );
	group->parent->invisible = false;
	ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 380,
							ui_WidgetGetNextY( widget ) + ui_StyleBorderHeight( ui_StyleBorderGet( styleBorderName )));
	MEContextPop( "RewardBox" );
}

void ugcMissionNodeGroupRefreshMapProperties( UGCMissionNodeGroup* group )
{
	UGCMissionDoc* doc = group->doc;
	UGCMap* map = ugcEditorGetMapByName( group->map.nextMapName );
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->map.objectiveID );
	UGCRuntimeErrorContext* errorCtx = ugcMakeTempErrorContextMapTransition( ugcMissionObjectiveLogicalNameTemp( objective ), doc->mission->name );
	UIPane* pane;
	MEFieldContextEntry* entry;
	MEContextSetErrorContext( errorCtx );

	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.Properties", true );

	if( map ) {
		UGCRuntimeErrorContext* mapErrorCtx = ugcMakeErrorContextMap( map->pcName );

		MEContextPush( "MapProperties", map, map, parse_UGCMap );
		MEContextSetErrorContext( mapErrorCtx );
		{
			// NOTE: MissionEditor Map nodes duplicate some fields from the
			// corresponding map editor.  This UI MUST be kept in sync with
			// ugcErrorAddKeysForMissionMap()'s filtering of map-data!
			MEContextAddText( false, ugcMapGetDisplayName( ugcEditorGetProjectData(), "ugc_NONE:1" ), "DisplayName",
							  TranslateMessageKey( "UGC_MissionEditor.MapName" ),
							  TranslateMessageKey( "UGC_MissionEditor.MapName.Tooltip" ));
		}
		MEContextPop( "MapProperties" );
		StructDestroySafe( parse_UGCRuntimeErrorContext, &mapErrorCtx );

		{
			MEFieldContext* ctx = MEContextPush( "MapTransitionProperties", &group->map.editingLink, &group->map.editingLink, parse_UGCMissionMapLink );
			ctx->cbChanged = ugcMissionMapMEFieldChangedCB;
			ctx->pChangedData = group;

			if( !ugcDefaultsSingleMissionEnabled() ) {
				MEContextAddSimple( kMEFieldType_TextEntry, "OpenMissionName", "UGC_MissionEditor.MissionText", "UGC_MissionEditor.MissionText.Tooltip" );
			}
			if( ugcDefaultsPreviewImagesAndOverworldMapEnabled() ) {
				UGCMapLocation* mapLocation = group->map.editingLink.pDoorMapLocation;
				AtlasTex* mapTex = atlasFindTexture( "World_Map" );
				float texHeight = 140;
				float texWidth = texHeight / mapTex->height * mapTex->width;
				MEFieldContextEntry* mapEntry;
				MEFieldContextEntry* mapLocationEntry;
				UISprite* mapSprite;
				UIWidgetWidget* mapLocationIcon;

				ugcMEContextAddLocationPickerMsg( "DoorMapLocation", "UGC_MissionEditor.Location", "UGC_MissionEditor.Location.Tooltip" );

				mapEntry = MEContextAddCustom( "DoorMapSprite" );
				mapLocationEntry = MEContextAddCustom( "DoorMapLocationSprite" );

				if( !ENTRY_WIDGET( mapEntry )) {
					ENTRY_WIDGET( mapEntry ) = UI_WIDGET( ui_SpriteCreate( 0, 0, 1, 1, "white" ));
				}
				ui_WidgetRemoveFromGroup( ENTRY_WIDGET( mapEntry ));
				ui_WidgetAddChild( ctx->pUIContainer, ENTRY_WIDGET( mapEntry ));

				if( !ENTRY_WIDGET( mapLocationEntry )) {
					ENTRY_WIDGET( mapLocationEntry ) = UI_WIDGET( ui_ZeniPickerIconCreate( NULL ));
				}
				ui_WidgetRemoveFromGroup( ENTRY_WIDGET( mapLocationEntry ));
				ui_WidgetAddChild( ENTRY_WIDGET( mapEntry ), ENTRY_WIDGET( mapLocationEntry ));

				mapSprite = (UISprite*)ENTRY_WIDGET( mapEntry );
				mapLocationIcon = (UIWidgetWidget*)ENTRY_WIDGET( mapLocationEntry );

				ui_SpriteSetTexture( mapSprite, "World_Map" );
				ui_WidgetSetPosition( UI_WIDGET( mapSprite ), ctx->iXPos, ctx->iYPos );
				ui_WidgetSetDimensions( UI_WIDGET( mapSprite ), texWidth, texHeight );

				ui_WidgetSetDimensions( UI_WIDGET( mapLocationIcon ), 44, 44 );
				if( mapLocation && mapLocation->positionX >= 0 && mapLocation->positionY >= 0 ) {
					ui_ZeniPickerIconSetIcon( mapLocationIcon, mapLocation->astrIcon );
					ui_WidgetSetPositionEx( UI_WIDGET( mapLocationIcon ), -22, -22,
											mapLocation->positionX, mapLocation->positionY,
											UITopLeft );
				} else {
					ui_ZeniPickerIconSetIcon( mapLocationIcon, NULL );
					ui_WidgetSetPositionEx( UI_WIDGET( mapLocationIcon ), -22, -22, 0.5, 0.5, UITopLeft );
				}
				ctx->iYPos += texHeight;
			}

			ugcMEContextAddComponentPicker( "Default Spawn", "SpawnComponent", "Start Spawn", NULL );
			MEContextPop( "MapTransitionProperties" );
		}
	} else {
		// Trying to reuse and change the parsetable on "MapProperties" to NULL results in an assert. So use a different context name.
		MEContextPush( "MapPropertiesEmpty", NULL, NULL, NULL );
		{
			entry = MEContextAddLabelMsg( "NoFields", "UGC_MissionEditor.MapNoProperties", NULL );
			ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
			ui_WidgetSetHeightEx( UI_WIDGET( ENTRY_LABEL( entry )), 1, UIUnitPercentage );
		}
		MEContextPop( "MapPropertiesEmpty" );
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	MEContextPop( __FUNCTION__ );
}

static void ugcMissionNodeGroupRefreshMapTransitionProperties( UGCMissionNodeGroup* group )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCMissionDoc* doc = group->doc;
	MEFieldContext* ctx = MEContextPush( "TransitionProperties", &group->mapTransition.editingLink, &group->mapTransition.editingLink, parse_UGCMissionMapLink );
	UGCRuntimeErrorContext* errorCtx = ugcMakeTempErrorContextMapTransition( ugcMissionObjectiveIDLogicalNameTemp( group->mapTransition.objectiveID ), doc->mission->name );
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	UIPane* pane;
	MEFieldContextEntry* entry;

	MEContextSetErrorContext( errorCtx );
	ctx->cbChanged = ugcMissionTransitionMEFieldChangedCB;
	ctx->pChangedData = group;

	pane = ugcMEContextPushPaneParentWithHeaderMsg( __FUNCTION__, "Header", "UGC_MissionEditor.Properties", true );

	if( group->type == UGCNODE_MAP_TRANSITION && !group->mapTransition.prevMapName && !group->mapTransition.nextMapName ) {
		entry = MEContextAddLabelMsg( "NoFields", "UGC_MissionEditor.MapTransitionNoProperties", NULL );
			ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
			ui_WidgetSetHeightEx( UI_WIDGET( ENTRY_LABEL( entry )), 1, UIUnitPercentage );
	} else if( group->type == UGCNODE_RETURN_MAP_TRANSITION && !doc->mission->return_map_link ) {
			entry = MEContextAddLabelMsg( "NoFields", "UGC_MissionEditor.MapTransitionNoProperties", NULL );
			ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
			ui_WidgetSetHeightEx( UI_WIDGET( ENTRY_LABEL( entry )), 1, UIUnitPercentage );
	} else if( resNamespaceIsUGC( group->mapTransition.nextMapName ) || ugcDefaultsMapTransitionsSpecifyDoor() ) {
		if( ugcDefaultsSingleMissionEnabled() ) {
			MEContextAddText( false, ugcMapMissionLinkReturnText( NULL ), "ReturnText",
							  TranslateMessageKey( "UGC_MissionEditor.MissionText" ),
							  TranslateMessageKey( "UGC_MissionEditor.MissionText.Tooltip" ));
		}

		ugcMEContextAddBooleanMsg( "DoorUsesMapLocation", "UGC_MissionEditor.UseMap", "UGC_MissionEditor.UseMap.Tooltip");

		entry = ugcMEContextAddComponentPickerExMsg( NULL, NULL, UGCZeniPickerType_Usable_As_Warp, "DoorComponent", "UGC_MissionEditor.MapTransitionDoor", "UGC_MissionEditor.MapTransitionDoor.Tooltip" );
		if( ugcDefaultsMapTransitionsSpecifyDoor() ) {
			UGCComponent* door = ugcEditorFindComponentByID( group->mapTransition.editingLink.uDoorComponentID );
			if( !door || door->eType == UGC_COMPONENT_TYPE_WHOLE_MAP ) {
				ui_ButtonSetMessage( ENTRY_BUTTON( entry ), "UGC_MissionEditor.MapTransitionDoor_Default" );
			}
		}

		if( !resNamespaceIsUGC( group->mapTransition.prevMapName ) && group->mapTransition.editingLink.bDoorUsesMapLocation ) {
			ui_SetActive( UI_WIDGET( ENTRY_BUTTON( entry )), false );
			ui_WidgetSetTooltipString( UI_WIDGET( ENTRY_BUTTON( entry )), "The map is available in fixed locations in the Cryptic world.  Set 'Use Overworld Map' to false if you want to customize where the player leaves from." );
		} else {
			ui_SetActive( UI_WIDGET( ENTRY_BUTTON( entry )), true );
			ui_WidgetSetTooltipString( UI_WIDGET( ENTRY_BUTTON( entry )), NULL );
		}

		MEContextAddText( false, ugcLinkButtonText( NULL ), "InteractText",
						  TranslateMessageKey( "UGC_MissionEditor.MapTransitionInteract" ),
						  TranslateMessageKey( "UGC_MissionEditor.MapTransitionInteract.Tooltip" ));
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	UI_WIDGET( pane )->rightPad = 10;
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	MEContextPop( __FUNCTION__ );

	MEContextPop( "TransitionProperties" );
}

UGCMissionNodeGroup* ugcMissionNodeGroupIntern( UGCMissionDoc* doc, int* pGroupIt )
{
	UGCMissionNodeGroup* group;

	if( *pGroupIt >= eaSize( &doc->nodeGroups )) {
		group = calloc( 1, sizeof( *group ));
		group->doc = doc;
		eaPush( &doc->nodeGroups, group );
	}

	assert( *pGroupIt < eaSize( &doc->nodeGroups ));
	assert( doc->nodeGroups );
	group = doc->nodeGroups[ *pGroupIt ];
	++*pGroupIt;

	group->parentGroup = NULL;
	group->prevGroup = NULL;
	group->nextGroup = NULL;
	group->firstChildGroup = NULL;
	return group;
}

void ugcMissionNodeGroupReset( UGCMissionNodeGroup* group )
{
	ui_WidgetForceQueueFree( UI_WIDGET( group->dummy.label ));
	ui_WidgetForceQueueFree( UI_WIDGET( group->parent ));

	SAFE_FREE( group->map.prevMapName );
	SAFE_FREE( group->map.nextMapName );
	StructReset( parse_UGCMissionMapLink, &group->map.editingLink );

	SAFE_FREE( group->mapTransition.prevMapName );
	SAFE_FREE( group->mapTransition.nextMapName );
	StructReset( parse_UGCMissionMapLink, &group->mapTransition.editingLink );

	memset( group, 0, sizeof( *group ));
}

void ugcMissionNodeGroupDestroy( UGCMissionNodeGroup* group )
{
	ugcMissionNodeGroupReset( group );
	free( group );
}

int ugcMissionNodeGroupChildDepth( UGCMissionNodeGroup* group )
{
	if( !group ) {
		return 0;
	} else {
		UGCMissionNodeGroup* it;
		int max = 0;

		for( it = group->firstChildGroup; it != NULL; it = it->nextGroup ) {
			max = MAX( max, 1 + ugcMissionNodeGroupChildDepth( it ));
		}

		return max;
	}
}

int ugcMissionNodeGroupParentDepth( UGCMissionNodeGroup* group )
{
	if( !group ) {
		return 0;
	} else {
		int accum = 0;
		while( group->parentGroup ) {
			++accum;
			group = group->parentGroup;
		}

		return accum;
	}
}


UGCMissionNodeGroup* ugcMissionDocNodeGroupFindSelectedNode( UGCMissionDoc* doc )
{
	if( UGCNODE_DUMMY == doc->selectedNodeType ) {
		return NULL;
	} else if( doc->selectedNodeType == UGCNODE_RETURN_MAP_TRANSITION ) {
		return &doc->returnMapLinkGroup;
	} else {
		int it;
		for( it = 0; it != eaSize( &doc->nodeGroups ); ++it ) {
			UGCMissionNodeGroup* group = doc->nodeGroups[ it ];
			if( group->type == doc->selectedNodeType ) {
				switch( group->type ) {
					case UGCNODE_OBJECTIVE: {
						UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );
						if( objective && objective->id == doc->selectedNodeID ) {
							return group;
						}
					}

					xcase UGCNODE_DIALOG_TREE:
						if( group->dialogTree.componentID == doc->selectedNodeID ) {
							return group;
						}

					xcase UGCNODE_MAP:
						if( group->map.objectiveID == doc->selectedNodeID ) {
							return group;
						}

					xcase UGCNODE_MAP_TRANSITION:
						if( group->mapTransition.objectiveID == doc->selectedNodeID ) {
							return group;
						}
				}
			}
		}
		return NULL;
	}
}

UGCMissionNodeGroup* ugcMissionDocNodeGroupFindObjective( UGCMissionDoc* doc, U32 id )
{
	int it;
	for( it = 0; it != eaSize( &doc->nodeGroups ); ++it ) {
		UGCMissionNodeGroup* group = doc->nodeGroups[ it ];
		if( group->type == UGCNODE_OBJECTIVE ) {
			UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );

			if( objective && objective->id == id ) {
				return group;
			}
		}
	}

	return NULL;
}

void ugcMissionDocNewGroupApplyType( UGCMissionDoc* doc )
{
	switch( doc->newGroupModel.type ) {
		xcase UGCMIMO_NEW_OBJECTIVE:
			if( doc->newGroupModel.componentType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				doc->newGroup.type = UGCNODE_DIALOG_TREE;
			} else {
				doc->newGroup.type = UGCNODE_OBJECTIVE;
			}

		xcase UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE: case UGCMIMO_NEW_TALK_TO_OBJECTIVE:
			doc->newGroup.type = UGCNODE_OBJECTIVE;

		xcase UGCMIMO_PROJECT_MAP: case UGCMIMO_CRYPTIC_MAP: case UGCMIMO_NEW_PROJECT_MAP:
			doc->newGroup.type = UGCNODE_MAP;

		xdefault:
			assert( 0 );
	}
}

void ugcMissionTreechartAddWidget( UGCMissionDoc* doc, UIWidget* beforeWidget, UIWidget* widget, const char* iconName, UserData data, UITreechartNodeFlags flags )
{
	ui_TreechartAddWidget( doc->treechart, beforeWidget, widget, iconName, data, flags );
}

void ugcMissionTreechartAddGroup( UGCMissionDoc* doc, UGCMissionNodeGroup* prevGroup, UGCMissionNodeGroup* parentGroup, UGCMissionNodeGroup* group, UITreechartNodeFlags flags )
{
	UIWidget* prev = NULL;

	group->parentGroup = parentGroup;
	group->prevGroup = prevGroup;
	if( prevGroup ) {
		prevGroup->nextGroup = group;
	}
	if( !prevGroup && parentGroup ) {
		parentGroup->firstChildGroup = group;
	}

	prev = UI_WIDGET( SAFE_MEMBER( prevGroup, parent ));
	if( prev ) {
		ui_TreechartAddWidget( doc->treechart, prev, UI_WIDGET( group->parent ), group->astrIconName, group, flags );
	} else if( parentGroup ) {
		ui_TreechartAddChildWidget( doc->treechart, UI_WIDGET( parentGroup->parent ), UI_WIDGET( group->parent ), group->astrIconName, group, flags );
	} else {
		ui_TreechartAddWidget( doc->treechart, NULL, UI_WIDGET( group->parent ), group->astrIconName, group, flags );
	}
	ui_TreechartSetLeftWidget( doc->treechart, UI_WIDGET( group->parent ), UI_WIDGET( group->leftButton ));
}


static void ugcMissionCleanupOrphanGroups1( UGCMission* mission, UGCMissionObjective*** peaObjectives )
{
	UGCComponentList* componentList = ugcEditorGetComponentList();

	int it;
	for( it = 0; it != eaSize( peaObjectives ); ++it ) {
		UGCMissionObjective* objective = (*peaObjectives)[ it ];

		ugcMissionCleanupOrphanGroups1( mission, &objective->eaChildren );

		if( objective->type == UGCOBJ_ALL_OF ) {
			UGCComponent** startPrompts = ugcComponentFindPopupPromptsForObjectiveStart( componentList, objective->id );
			UGCComponent** completePrompts = ugcComponentFindPopupPromptsForObjectiveComplete( componentList, objective->id );

			if( eaSize( &objective->eaChildren ) == 0 ) {
				StructDestroySafe( parse_UGCMissionObjective, &objective );
				eaRemove( peaObjectives, it );
				--it;
			} else if( eaSize( &objective->eaChildren ) == 1 ) {
				UGCMissionObjective* child = objective->eaChildren[ 0 ];
				assert( child->type == UGCOBJ_IN_ORDER );

				eaRemove( peaObjectives, it );
				eaInsertEArray( peaObjectives, &child->eaChildren, it );

				if( eaSize( &child->eaChildren )) {
					UGCMissionObjective* firstChild = child->eaChildren[ 0 ];
					UGCMissionObjective* lastChild = eaTail( &child->eaChildren );
					int promptIt;

					for( promptIt = 0; promptIt != eaSize( &startPrompts ); ++promptIt ) {
						ea32Clear( &startPrompts[ promptIt ]->eaObjectiveIDs );
						ea32Push( &startPrompts[ promptIt ]->eaObjectiveIDs, firstChild->id );
					}
					for( promptIt = 0; promptIt != eaSize( &completePrompts ); ++promptIt ) {
						ea32Clear( &completePrompts[ promptIt ]->eaObjectiveIDs );
						ea32Push( &completePrompts[ promptIt ]->eaObjectiveIDs, lastChild->id );
					}
				}

				it += eaSize( &child->eaChildren ) - 1;
				eaDestroy( &child->eaChildren );

				StructDestroySafe( parse_UGCMissionObjective, &objective );
			}

			eaDestroy( &startPrompts );
			eaDestroy( &completePrompts );
		} else if( objective->type == UGCOBJ_IN_ORDER ) {
			if( eaSize( &objective->eaChildren ) == 0 ) {
				StructDestroySafe( parse_UGCMissionObjective, &objective );
				eaRemove( peaObjectives, it );
				--it;
			}
		}
	}
}

static void ugcMissionCleanupOrphanGroups( void )
{
	UGCProjectData* data = ugcEditorGetProjectData();

	ugcMissionCleanupOrphanGroups1( data->mission, &data->mission->objectives );

	// Cleanup components
	{
		UGCComponentList* components = ugcEditorGetComponentList();
		int it;
		for( it = 0; it != eaSize( &components->eaComponents ); ++it ) {
			UGCComponent* component = components->eaComponents[ it ];

			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				// Make prompts ObjectiveComplete when possible
				if( ugcComponentStartWhenType(component) == UGCWHEN_OBJECTIVE_START ) {
					UGCMissionObjective* objective = ugcObjectiveFind( data->mission->objectives, component->eaObjectiveIDs[ 0 ]);
					UGCMissionObjective* beforeObjective = ugcObjectiveFindPrevious( data->mission->objectives, component->eaObjectiveIDs[ 0 ]);
					const char* mapName = objective ? ugcObjectiveMapName( data, objective, NULL ) : NULL;
					const char* beforeMapName = beforeObjective ? ugcObjectiveMapName( data, beforeObjective, NULL ) : NULL;

					if( objective && beforeObjective && stricmp_safe( mapName, beforeMapName ) == 0 ) {
						component->pStartWhen->eType = UGCWHEN_OBJECTIVE_COMPLETE;
						ea32Clear( &component->eaObjectiveIDs );
						ea32Push( &component->eaObjectiveIDs, beforeObjective->id );
					}
				}
			}
		}
	}
}

static void ugcMissionNodeRemove( UGCMissionNodeGroup* group )
{
	UGCComponentList* componentList = ugcEditorGetComponentList();
	UGCMissionDoc* doc = group->doc;

	if( group == &doc->newGroup ) {
		return;
	}

	switch( group->type ) {
		case UGCNODE_DIALOG_TREE: {
			UGCComponent* dialogTree = ugcComponentFindByID( componentList, group->dialogTree.componentID );
			UGCComponent** otherDialogTrees = ugcComponentFindPopupPromptsForWhenInDialog( componentList, dialogTree );

			eaFindAndRemove( &otherDialogTrees, dialogTree );
			ugcMissionDialogTreeRenumberSequence( otherDialogTrees, dialogTree->pStartWhen->eType, eaiGet( &dialogTree->eaObjectiveIDs, 0 ));
			dialogTree->pStartWhen->eType = UGCWHEN_MANUAL;
			ea32Clear( &dialogTree->eaObjectiveIDs );
		}

		xcase UGCNODE_OBJECTIVE: {
			int objectiveIndex;
			UGCMissionObjective*** peaObjectives = ugcObjectiveFindParentEA( &doc->mission->objectives, group->objective.objectiveID, &objectiveIndex );
			UGCMissionObjective* objective = (*peaObjectives)[ objectiveIndex ];
			UGCComponent** startPrompts = ugcComponentFindPopupPromptsForObjectiveStart( componentList, objective->id );
			UGCComponent** completePrompts = ugcComponentFindPopupPromptsForObjectiveComplete( componentList, objective->id );

			objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );
			startPrompts = ugcComponentFindPopupPromptsForObjectiveStart( componentList, objective->id );
			completePrompts = ugcComponentFindPopupPromptsForObjectiveComplete( componentList, objective->id );

			eaRemove( peaObjectives, objectiveIndex );

			// Combine startPrompt into completePrompt
			eaInsertEArray( &completePrompts, &startPrompts, 0 );
			eaDestroy( &startPrompts );

			if( eaSize( &completePrompts )) {
				UGCWhenType whenType;
				U32 whenObjectiveID;
				UGCComponent** whenExistingPrompts;

				{
					UGCMissionObjective* beforeObjective = eaGet( peaObjectives, objectiveIndex - 1 );
					UGCMissionObjective* afterObjective = eaGet( peaObjectives, objectiveIndex );

					if( !beforeObjective && !afterObjective ) {
						if( !group->parentGroup || group->parentGroup->type != UGCNODE_OBJECTIVE ) {
							whenType = UGCWHEN_MISSION_START;
							whenObjectiveID = 0;
							whenExistingPrompts = ugcComponentFindPopupPromptsForMissionStart( componentList );
						} else {
							UGCMissionObjective* parentObjective = ugcObjectiveFind( doc->mission->objectives, group->parentGroup->objective.objectiveID );
							whenType = UGCWHEN_OBJECTIVE_COMPLETE;
							whenObjectiveID = parentObjective->id;
							whenExistingPrompts = ugcComponentFindPopupPromptsForObjectiveComplete( componentList, whenObjectiveID );
						}
					} else {
						const char* beforeMapName = beforeObjective ? ugcObjectiveMapName( ugcEditorGetProjectData(), beforeObjective, NULL ) : NULL;
						const char* afterMapName = afterObjective ? ugcObjectiveMapName( ugcEditorGetProjectData(), afterObjective, NULL ) : NULL;
						const char* objectiveMapName = ugcObjectiveMapName( ugcEditorGetProjectData(), objective, NULL );

						if( beforeObjective && (!objectiveMapName || !afterMapName
												|| stricmp_safe( beforeMapName, objectiveMapName) == 0
												||  stricmp_safe( afterMapName, objectiveMapName ) != 0) ) {
							whenType = UGCWHEN_OBJECTIVE_COMPLETE;
							whenObjectiveID = beforeObjective->id;
							whenExistingPrompts = ugcComponentFindPopupPromptsForObjectiveComplete( componentList, whenObjectiveID );
						} else {
							whenType = UGCWHEN_OBJECTIVE_START;
							whenObjectiveID = afterObjective->id;
							whenExistingPrompts = ugcComponentFindPopupPromptsForObjectiveStart( componentList, whenObjectiveID );
						}
					}
				}

				eaPushEArray( &whenExistingPrompts, &completePrompts );
				eaDestroy( &completePrompts );

				ugcMissionDialogTreeRenumberSequence( whenExistingPrompts, whenType, whenObjectiveID );
			}
		}
	}
}

static UGCMissionObjective** ugcMissionMapFindAndRemoveObjectives( UGCMissionDoc* doc, UGCMissionNodeGroup* mapGroup )
{
	UGCMissionObjective** accum = NULL;

	assert( mapGroup->type == UGCNODE_MAP );

	if( mapGroup == &doc->newGroup ) {
		UGCComponent* mapComponent = NULL;
		if( doc->newGroupModel.type == UGCMIMO_PROJECT_MAP ) {
			mapComponent = ugcMissionGetDefaultComponentForMap( ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_WHOLE_MAP, doc->newGroupModel.mapName );
		} else if( doc->newGroupModel.type == UGCMIMO_CRYPTIC_MAP ) {
			mapComponent = ugcMissionGetDefaultComponentForMap( ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_WHOLE_MAP, ugcGetDefaultMapName( ugcEditorGetProjectData() ));
		} else if( doc->newGroupModel.type == UGCMIMO_NEW_PROJECT_MAP ) {
			UGCMap* map = ugcEditorUninitializedMapCreate( UGC_MAP_TYPE_INTERIOR );
			mapComponent = ugcMissionGetDefaultComponentForMap( ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_WHOLE_MAP, map->pcName );
		}

		if( mapComponent ) {
			UGCMissionObjective* mapObjective = StructCreate( parse_UGCMissionObjective );
			U32 id;
			ugcObjectiveGenerateIDs( ugcEditorGetProjectData(), &id, 1 );
			mapObjective->id = id;
			mapObjective->componentID = mapComponent->uID;
			mapObjective->type = UGCOBJ_COMPLETE_COMPONENT;
			mapObjective->waypointMode = UGC_WAYPOINT_POINTS;
			// This appears to be just for the default component so we should not have to worry about naming children
			StructCopyString( &mapObjective->strComponentInternalMapName, mapComponent->sPlacement.pcMapName );

			eaPush( &accum, mapObjective );
		}
	} else {
		UGCMissionObjective*** peaChildObjectives = NULL;

		{
			int it;
			for( it = 0; it != eaiSize( &mapGroup->map.objectiveIDs ); ++it ) {
				int objectiveIndex;
				UGCMissionObjective*** peaObjectives = ugcObjectiveFindParentEA(
						&doc->mission->objectives, mapGroup->map.objectiveIDs[ it ], &objectiveIndex );
				eaPush( &accum, (*peaObjectives)[ objectiveIndex ]);

				if( !peaChildObjectives ) {
					peaChildObjectives = peaObjectives;
				} else {
					devassert( peaChildObjectives == peaObjectives );
				}
			}
		}
		assert( eaSize( &accum ));

		// Remove all the objectives removed
		{
			int it;
			for( it = 0; it != eaSize( peaChildObjectives ); ++it ) {
				UGCMissionObjective* childObjective = (*peaChildObjectives)[ it ];
				if( eaFind( &accum, childObjective ) >= 0 ) {
					eaRemove( peaChildObjectives, it );
					--it;
				}
			}
		}
	}

	return accum;
}


//  Set the internal map name of an objective and all its children.
//  If we don't set the children to the same map as the parent, all
//  sorts of strange things can display in the mission tree.
static void ugcMissionObjectiveSetInternalMapName(UGCMissionObjective* pObjective, const char* targetMapName)
{
	int iChildIndex;

	StructCopyString(&(pObjective->strComponentInternalMapName), targetMapName);

	for( iChildIndex = 0; iChildIndex != eaSize( &(pObjective->eaChildren) ); ++iChildIndex )
	{
		UGCMissionObjective* pChild = (pObjective->eaChildren)[iChildIndex];
		ugcMissionObjectiveSetInternalMapName(pChild,targetMapName);
	}
}


static UGCMissionNodeGroup *ugcGetMapNodeGroupFromMissionNodeGroup(UGCMissionNodeGroup *group)
{
	UGCMissionNodeGroup *mapGroup = group;
	while(mapGroup && mapGroup->type != UGCNODE_MAP)
		mapGroup = mapGroup->parentGroup;
	return mapGroup;
}

static bool ugcIsMapGroupNodeExternalMap(UGCMissionNodeGroup *mapGroup)
{
	bool isInternal = false;
	if(mapGroup && mapGroup->type == UGCNODE_MAP)
	{
		UGCMissionObjective *mapTransitionObjective = ugcObjectiveFind(ugcEditorGetProjectData()->mission->objectives, mapGroup->map.objectiveID);
		if(mapTransitionObjective)
			ugcObjectiveMapName(ugcEditorGetProjectData(), mapTransitionObjective, &isInternal);
	}
	return !isInternal;
}

static bool ugcIsMissionNodeGroupAllowedOnMapGroup(UGCMissionDoc* doc, UGCMissionNodeGroup *srcGroup, UGCMissionNodeGroup *mapGroup)
{
	// Cannot drag a Kill Objective onto an external map (i.e. Cryptic Map)
	if(srcGroup->type == UGCNODE_OBJECTIVE)
	{
		if(ugcIsMapGroupNodeExternalMap(mapGroup))
		{
			if(srcGroup == &doc->newGroup)
			{
				if(doc->newGroupModel.componentType == UGC_COMPONENT_TYPE_KILL)
					return false;
			}
			else
			{
				UGCMissionObjective *objective = ugcObjectiveFind(doc->mission->objectives, srcGroup->objective.objectiveID);
				if(objective)
				{
					UGCComponent *component = ugcEditorFindComponentByID(objective->componentID);
					if(component && component->eType == UGC_COMPONENT_TYPE_KILL)
						return false;
				}
			}
		}
	}

	return true; // default to true, code above restricts what's allowed
}

bool ugcMissionTreechartDragNodeNode( UITreechart* ignored, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawDestGroup )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionNodeGroup* srcGroup = (UGCMissionNodeGroup*)rawSrcGroup;
	UGCMissionNodeGroup* destGroup = (UGCMissionNodeGroup*)rawDestGroup;
	UGCMissionNodeGroup* mapGroup = ugcGetMapNodeGroupFromMissionNodeGroup( destGroup );

	if( !ugcIsMissionNodeGroupAllowedOnMapGroup( doc, srcGroup, mapGroup )) {
		return false;
	}
	if( !destGroup ) {
		return false;
	}

	ugcMissionDocNewGroupApplyType( doc );

	switch( srcGroup->type ) {
		xcase UGCNODE_OBJECTIVE: case UGCNODE_DIALOG_TREE:
			if( destGroup->type != UGCNODE_MAP_FTUE ) {
				return false;
			}
		xcase UGCNODE_MAP:
			if(   destGroup->type != UGCNODE_MAP_TRANSITION && destGroup->type != UGCNODE_RETURN_MAP_TRANSITION
				  && destGroup->type != UGCNODE_DUMMY ) {
				return false;
			}

			// Dummy is only allowed to insert the first objective
			if( destGroup->type == UGCNODE_DUMMY && eaSize( &doc->mission->objectives )) {
				return false;
			}

		xdefault:
			return false;
	}

	if( !isCommit ) {
		return true;
	}

	if( srcGroup == destGroup ) {
		return true;
	}

	switch( srcGroup->type ) {
		xcase UGCNODE_OBJECTIVE: {
			U32 ids[ 1 ];
			UGCMissionObjective* srcObjective;
			const char* targetMapName = ugcMissionTreechartMapName( destGroup, NULL );

			ugcObjectiveGenerateIDs( ugcEditorGetProjectData(), ids, 1 );

			if( srcGroup == &doc->newGroup ) {
				srcObjective = ugcEditorObjectiveCreate( doc->newGroupModel.componentType, doc->newGroupModel.type, ids[ 0 ], targetMapName );
			} else {
				srcObjective = ugcObjectiveFind( doc->mission->objectives, srcGroup->objective.objectiveID );
			}

			if( destGroup->type == UGCNODE_MAP_FTUE ) {
				UGCMissionObjective* firstMapObjective = ugcObjectiveFind( doc->mission->objectives, destGroup->mapFtue.objectiveID );

				// Special check if we are dragging the first group/objective in a map onto the map itself. This should be a no-op.
				if( firstMapObjective == srcObjective ) {
					return true;
				}

				ugcMissionNodeRemove( srcGroup );

				if( firstMapObjective ) {
					UGCMissionTreechartPos pos = { 0 };
					int index;
					UGCMissionObjective*** peaObjectives = ugcObjectiveFindParentEA(
							&doc->mission->objectives, firstMapObjective->id, &index );

					if( peaObjectives && index >= 0 ) {
						eaInsert( peaObjectives, srcObjective, index );
					}
				}
			}
			ugcMissionObjectiveSetInternalMapName( srcObjective,targetMapName );	// Set the objective and all its children to this map
		}

		xcase UGCNODE_DIALOG_TREE: {
			UGCComponent* srcDialogTree;

			// Either create a new group, or grab hold of the already existing group and remove it from its current location
			if( srcGroup == &doc->newGroup ) {
				srcDialogTree = ugcComponentOpCreate( ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_DIALOG_TREE, 0 );
			} else {
				srcDialogTree = ugcEditorFindComponentByID( srcGroup->dialogTree.componentID );
			}

			ugcMissionNodeRemove( srcGroup );

			{
				// Insert the DialogTree into the list of other OBJECTIVE_START DialogTrees associated with the firstMapObjective.
				//  We are guaranteed to have at least one objective, though it may be invisible in the UI.

				UGCMissionObjective* firstMapObjective = ugcObjectiveFind( doc->mission->objectives, destGroup->map.objectiveIDs[ 0 ]);

				if( firstMapObjective )
				{
					UGCComponent** destDialogTrees = NULL;
					UGCWhenType destWhen;
					U32 destObjectiveID;
					int iDialogTreeInsertionIndex;

					// Get the ObjectStart DialogTrees for the FirstMapObjective
					destDialogTrees = ugcComponentFindPopupPromptsForObjectiveStart( ugcEditorGetComponentList(), firstMapObjective->id );
					destWhen = UGCWHEN_OBJECTIVE_START;
					destObjectiveID = firstMapObjective->id;

					// Put the new dialog at the start of the list of existing dialogs
					iDialogTreeInsertionIndex=0;
					eaInsert( &destDialogTrees, srcDialogTree, iDialogTreeInsertionIndex);

					// Resequence
					ugcMissionDialogTreeRenumberSequence( destDialogTrees, destWhen, destObjectiveID );
					eaDestroy( &destDialogTrees );
				}
			}
		}

		xcase UGCNODE_MAP: {
			UGCMissionObjective** objectives = ugcMissionMapFindAndRemoveObjectives( doc, srcGroup );
			UGCMissionTreechartPos pos = { 0 };

			ugcMissionTreechartCalcPos( NULL, destGroup, &pos );

			// insert into new position -- simplified version of the
			// objective insert in DragNodeArrow since it doesn't have
			// to deal with prompts.
			if( !pos.peaDestObjectives && pos.destObjectiveIndex == -1 ) {  //< mission start special flags
				eaInsertEArray( &doc->mission->objectives, &objectives, 0 );
			} else {
				eaInsertEArray( pos.peaDestObjectives, &objectives, pos.destObjectiveIndex );
			}
		}

		xdefault:
			assert( 0 ); //< unexpected type, should have been rejected earlier.
	}


	ugcMissionCleanupOrphanGroups();
	ugcEditorFixupObjectivesComponentMapNames( ugcEditorGetProjectData(), doc->mission->objectives, NULL );
	ugcEditorApplyUpdate();
	return true;
}

static UGCMissionObjective* ugcMissionTreechartGroupObjective( UGCMissionNodeGroup* group )
{
	UGCMissionDoc* doc = group->doc;

	switch( group->type ) {
		xcase UGCNODE_OBJECTIVE:
			return ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );

		xcase UGCNODE_MAP:
			return ugcObjectiveFind( ugcEditorGetMission()->objectives, group->map.objectiveID );

		xcase UGCNODE_MAP_FTUE:
			return ugcObjectiveFind( ugcEditorGetMission()->objectives, group->mapFtue.objectiveID );

		xcase UGCNODE_MAP_TRANSITION:
			return ugcObjectiveFind( ugcEditorGetMission()->objectives, group->mapTransition.objectiveID );

		xcase UGCNODE_DIALOG_TREE: {
			UGCComponent* dialogTree = ugcEditorFindComponentByID( group->dialogTree.componentID );

			switch( ugcComponentStartWhenType(dialogTree) ) {
				xcase UGCWHEN_OBJECTIVE_COMPLETE: case UGCWHEN_OBJECTIVE_START:
					return ugcObjectiveFind( doc->mission->objectives, dialogTree->eaObjectiveIDs[ 0 ]);
			}
		}
	}

	return NULL;
}

static bool ugcMissionIsGroupCompleteAllMaps( UGCMissionNodeGroup* group )
{
	if( group ) {
		UGCMissionObjective* objective = ugcMissionTreechartGroupObjective( group );
		if( group->type != UGCNODE_OBJECTIVE ) {
			return false;
		}
		if( objective->type != UGCOBJ_ALL_OF ) {
			return false;
		}
		{
			UGCMissionNodeGroup* childGroup = group->firstChildGroup;
			while( childGroup ) {
				if( childGroup->type == UGCNODE_MAP ) {
					return true;
				}
				childGroup = childGroup->nextGroup;
			}
		}
	}

	return false;
}

bool ugcMissionTreechartDragNodeArrow( UITreechart* ignored, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawBeforeDestGroup, UserData rawAfterDestGroup )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionNodeGroup* srcGroup = (UGCMissionNodeGroup*)rawSrcGroup;
	UGCMissionNodeGroup* beforeDestGroup = (UGCMissionNodeGroup*)rawBeforeDestGroup;
	UGCMissionNodeGroup* afterDestGroup = (UGCMissionNodeGroup*)rawAfterDestGroup;
	UGCMissionNodeGroup *destGroup = beforeDestGroup ? beforeDestGroup : afterDestGroup;
	UGCMissionNodeGroup *mapGroup = ugcGetMapNodeGroupFromMissionNodeGroup(destGroup);

	if(!ugcIsMissionNodeGroupAllowedOnMapGroup(doc, srcGroup, mapGroup))
		return false;

	ugcMissionDocNewGroupApplyType( doc );

	if( ugcMissionNodeGroupChildDepth( srcGroup ) + MAX( ugcMissionNodeGroupParentDepth( beforeDestGroup ), ugcMissionNodeGroupParentDepth( afterDestGroup )) > UGC_OBJECTIVE_MAX_DEPTH ) {
		return false;
	}

	if( srcGroup->type == UGCNODE_MAP ) {
		return false;
	}

	if( srcGroup->type == UGCNODE_OBJECTIVE ) {
		if(destGroup)
		{
			// Not allowed to place objectives except next to other objectives or popup dialogs.
			if(destGroup->type != UGCNODE_OBJECTIVE && destGroup->type != UGCNODE_DIALOG_TREE)
				return false;
		}
	}

	if( !isCommit ) {
		return true;
	}

	// Nop
	if( srcGroup == beforeDestGroup || srcGroup == afterDestGroup ) {
		return true;
	}

	switch( srcGroup->type ) {
		xcase UGCNODE_OBJECTIVE: {
			UGCMissionObjective* srcObjective = NULL;
			UGCMissionTreechartPos pos = { 0 };
			const char* targetMapName = ugcMissionTreechartMapName( beforeDestGroup, afterDestGroup );

			if( srcGroup == &doc->newGroup ) {
				U32 id;
				ugcObjectiveGenerateIDs( ugcEditorGetProjectData(), &id, 1 );
				srcObjective = ugcEditorObjectiveCreate( doc->newGroupModel.componentType, doc->newGroupModel.type, id, targetMapName );
			} else {
				srcObjective = ugcObjectiveFind( doc->mission->objectives, srcGroup->objective.objectiveID );
			}

			ugcMissionNodeRemove( srcGroup );
			ugcMissionTreechartCalcPos( beforeDestGroup, afterDestGroup, &pos );

			// Insert into the new position
			ugcMissionObjectiveSetInternalMapName(srcObjective,targetMapName);	// Set the objective and all its children to this map
			if( !pos.peaDestObjectives && pos.destObjectiveIndex == -1 ) {  //< mission start special flags
				eaInsert( &doc->mission->objectives, srcObjective, 0 );
			} else {
				eaInsert( pos.peaDestObjectives, srcObjective, pos.destObjectiveIndex );
			}

			{
				UGCComponent** beforeCompletePrompts;

				if( !pos.peaDestObjectives && pos.destObjectiveIndex == -1 ) {  //< mission start special flags
					beforeCompletePrompts = ugcComponentFindPopupPromptsForMissionStart( ugcEditorGetComponentList() );
				} else {
					UGCMissionObjective* beforeObjective = eaGet( pos.peaDestObjectives, pos.destObjectiveIndex - 1 );
					if( beforeObjective ) {
						beforeCompletePrompts = ugcComponentFindPopupPromptsForObjectiveComplete( ugcEditorGetComponentList(), beforeObjective->id );
					} else {
						beforeCompletePrompts = NULL;
					}
				}

				if( pos.destDialogPromptIndex < eaSize( &beforeCompletePrompts )) {
					UGCComponent* beforeComponent = beforeCompletePrompts[ 0 ];
					UGCComponent** completePrompts = NULL;

					// now, split up the components
					while( eaSize( &beforeCompletePrompts ) > pos.destDialogPromptIndex ) {
						eaPush( &completePrompts, beforeCompletePrompts[ pos.destDialogPromptIndex ]);
						eaRemove( &beforeCompletePrompts, pos.destDialogPromptIndex );
					}

					ugcMissionDialogTreeRenumberSequence( beforeCompletePrompts, beforeComponent->pStartWhen->eType, eaiGet( &beforeComponent->eaObjectiveIDs, 0 ));
					ugcMissionDialogTreeRenumberSequence( completePrompts, UGCWHEN_OBJECTIVE_COMPLETE, srcObjective->id );

					eaDestroy( &completePrompts );
				}

				eaDestroy( &beforeCompletePrompts );
			}

			// in this case, we want to also steal its start prompt
			if( afterDestGroup && afterDestGroup->type == UGCNODE_OBJECTIVE ) {
				UGCMissionObjective* afterObjective = ugcObjectiveFind( doc->mission->objectives, afterDestGroup->objective.objectiveID );
				UGCComponent** afterStartPrompts = ugcComponentFindPopupPromptsForObjectiveStart( ugcEditorGetComponentList(), afterObjective->id );
				UGCComponent** completePrompts = ugcComponentFindPopupPromptsForObjectiveComplete( ugcEditorGetComponentList(), srcObjective->id );

				eaPushEArray( &completePrompts, &afterStartPrompts );
				ugcMissionDialogTreeRenumberSequence( completePrompts, UGCWHEN_OBJECTIVE_COMPLETE, srcObjective->id );
				eaDestroy( &afterStartPrompts );
				eaDestroy( &completePrompts );
			}
		}

		xcase UGCNODE_DIALOG_TREE: {
			UGCComponent* srcDialogTree;

			if( srcGroup == &doc->newGroup ) {
				srcDialogTree = ugcComponentOpCreate( ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_DIALOG_TREE, 0 );
			} else {
				srcDialogTree = ugcEditorFindComponentByID( srcGroup->dialogTree.componentID );
			}

			ugcMissionNodeRemove( srcGroup );

			// Insert into new position
			if( (!beforeDestGroup || beforeDestGroup->type == UGCNODE_DUMMY ) && (!afterDestGroup || afterDestGroup->type == UGCNODE_DUMMY) ) {
				// only way this can happen is if the node is being
				// inserted into an empty mission

				UGCComponent** destDialogs = NULL;
				eaPush( &destDialogs, srcDialogTree );
				ugcMissionDialogTreeRenumberSequence( destDialogs, UGCWHEN_MISSION_START, 0 );
				eaDestroy( &destDialogs );
			} else {
				UGCMissionTreechartPos pos = { 0 };
				UGCComponent** destDialogTrees = NULL;
				UGCWhenType destWhen;
				U32 destObjectiveID;

				ugcMissionTreechartCalcPos( beforeDestGroup, afterDestGroup, &pos );

				if( !pos.peaDestObjectives && pos.destObjectiveIndex == -1 ) {
					destDialogTrees = ugcComponentFindPopupPromptsForMissionStart( ugcEditorGetComponentList() );
					destWhen = UGCWHEN_MISSION_START;
					destObjectiveID = 0;
				} else {
					if( pos.destDialogPromptWhen == UGCWHEN_OBJECTIVE_START ) {
						UGCMissionObjective* destObjective = eaGet( pos.peaDestObjectives, pos.destObjectiveIndex );
						destDialogTrees = ugcComponentFindPopupPromptsForObjectiveStart( ugcEditorGetComponentList(), destObjective->id );
						destWhen = UGCWHEN_OBJECTIVE_START;
						destObjectiveID = destObjective->id;
					} else {
						UGCMissionObjective* destObjective = eaGet( pos.peaDestObjectives, pos.destObjectiveIndex - 1 );
						destDialogTrees = ugcComponentFindPopupPromptsForObjectiveComplete( ugcEditorGetComponentList(), destObjective->id );
						destWhen = UGCWHEN_OBJECTIVE_COMPLETE;
						destObjectiveID = destObjective->id;
					}
				}

				if( pos.destDialogPromptIndex > eaSize( &destDialogTrees )) {
					eaPush( &destDialogTrees, srcDialogTree );
				} else {
					eaInsert( &destDialogTrees, srcDialogTree, pos.destDialogPromptIndex );
				}
				ugcMissionDialogTreeRenumberSequence( destDialogTrees, destWhen, destObjectiveID );
				eaDestroy( &destDialogTrees );
			}
		}

		xdefault:
			devassertmsgf( 0, "Unsupported node type: %d", srcGroup->type );
	}

	ugcMissionCleanupOrphanGroups();
	ugcEditorFixupObjectivesComponentMapNames( ugcEditorGetProjectData(), doc->mission->objectives, NULL );
	ugcEditorApplyUpdate();

	return true;
}

void ugcMissionTreechartNodeAnimate( UITreechart* ignored, UserData rawDoc, UserData rawGroup, float x, float y )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionNodeGroup* group = (UGCMissionNodeGroup*)rawGroup;

	if( !group || group == &doc->newGroup ) {
		return;
	}

	switch( group->type ) {
		case UGCNODE_DUMMY:
		if( group == &doc->dummyStartGroup ) {
			setVec2( doc->startPos, x, y );
		} else {
			setVec2( doc->endPos, x, y );
		}

		xcase UGCNODE_MAP:
			;

		xcase UGCNODE_MAP_FTUE:
			;

		xcase UGCNODE_MAP_TRANSITION: case UGCNODE_RETURN_MAP_TRANSITION:
			;

		xcase UGCNODE_OBJECTIVE: {
			UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );

			objective->editorX = x;
			objective->editorY = y;
		}

		xcase UGCNODE_DIALOG_TREE: {
			UGCComponent* dialogTree = ugcEditorFindComponentByID( group->dialogTree.componentID );

			dialogTree->dialogBlock.editorX = x;
			dialogTree->dialogBlock.editorY = y;
		}

		xcase UGCNODE_REWARD_BOX:
			;

		xdefault:
			devassertmsgf( 0, "Unsupported node type: %d", group->type );
	}
}

static void ugcMissionObjectiveDeleteDialogTrees( UGCMissionObjective* objective )
{
	if( objective->type == UGCOBJ_COMPLETE_COMPONENT ) {
		UGCComponent* component = ugcEditorFindComponentByID( objective->componentID );

		if( component && component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			ugcComponentOpDelete( ugcEditorGetProjectData(), component, false );
		}
	}

	{
		UGCComponent** startPrompts = ugcComponentFindPopupPromptsForObjectiveStart( ugcEditorGetComponentList(), objective->id );
		int it;
		for( it = 0; it != eaSize( &startPrompts ); ++it ) {
			ugcComponentOpDelete( ugcEditorGetProjectData(), startPrompts[ it ], false );
		}
		eaDestroy( &startPrompts );
	}
	{
		UGCComponent** completePrompts = ugcComponentFindPopupPromptsForObjectiveComplete( ugcEditorGetComponentList(), objective->id );
		int it;
		for( it = 0; it != eaSize( &completePrompts ); ++it ) {
			ugcComponentOpDelete( ugcEditorGetProjectData(), completePrompts[ it ], false );
		}
	}

	{
		int it;
		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			ugcMissionObjectiveDeleteDialogTrees( objective->eaChildren[ it ]);
		}
	}
}

void ugcMissionTreechartDragNodeTrash( UITreechart* ignored, UserData rawDoc, UserData rawGroup )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionNodeGroup* group = (UGCMissionNodeGroup*)rawGroup;

	if( group == &doc->newGroup ) {
		return;
	}

	switch( group->type ) {
		xcase UGCNODE_DIALOG_TREE: {
			UGCComponent* dialogTree = ugcEditorFindComponentByID( group->dialogTree.componentID );
			ugcMissionNodeRemove( group );
			ugcComponentOpDelete( ugcEditorGetProjectData(), dialogTree, false );
		}

		xcase UGCNODE_OBJECTIVE: {
			UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->objective.objectiveID );

			ugcMissionNodeRemove( group );
			ugcMissionObjectiveDeleteDialogTrees( objective );
			StructDestroySafe( parse_UGCMissionObjective, &objective );
		}

		xcase UGCNODE_MAP: {
			int it;

			for( it = 0; it != eaiSize( &group->map.objectiveIDs ); ++it ) {
				UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, group->map.objectiveIDs[ it ]);
				UGCMissionNodeGroup* childGroup = ugcMissionDocNodeGroupFindObjective( doc, objective->id );

				ugcMissionNodeRemove( childGroup );
				ugcMissionObjectiveDeleteDialogTrees( objective );
				StructDestroySafe( parse_UGCMissionObjective, &objective );
			}
		}

		xdefault:
			devassertmsgf( 0, "Unsupported node type: %d", group->type );
	}

	doc->selectedNodeType = UGCNODE_DUMMY;

	ugcMissionCleanupOrphanGroups();
	ugcEditorFixupObjectivesComponentMapNames( ugcEditorGetProjectData(), doc->mission->objectives, NULL );
	ugcEditorApplyUpdate();
}

bool ugcMissionTreechartDragNodeNodeColumn( UITreechart* ignored, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawDestGroup, int column )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;
	UGCMissionNodeGroup* srcGroup = (UGCMissionNodeGroup*)rawSrcGroup;
	UGCMissionNodeGroup* destGroup = (UGCMissionNodeGroup*)rawDestGroup;
	UGCMissionObjective* srcObjective;
	UGCMissionObjective* destObjective;
	UGCMissionNodeGroup *mapGroup = ugcGetMapNodeGroupFromMissionNodeGroup( destGroup );

	if( !ugcIsMissionNodeGroupAllowedOnMapGroup( doc, srcGroup, mapGroup )) {
		return false;
	}
	
	if( srcGroup->type != UGCNODE_OBJECTIVE ) {
		return false;
	}
	if( !destGroup || destGroup->type != UGCNODE_OBJECTIVE ) {
		return false;
	}
	if( ugcMissionNodeGroupChildDepth( srcGroup ) + ugcMissionNodeGroupParentDepth( destGroup ) + 1 > UGC_OBJECTIVE_MAX_DEPTH ) {
		return false;
	}
	
	if( !isCommit ) {
		return true;
	}

	if( srcGroup == destGroup ) {
		return true;
	}

	{
		const char* targetMapName = ugcMissionTreechartMapName( destGroup, NULL );
		U32 ids[ 4 ];

		ugcObjectiveGenerateIDs( ugcEditorGetProjectData(), ids, 4 );

		if( srcGroup == &doc->newGroup ) {
			srcObjective = ugcEditorObjectiveCreate( doc->newGroupModel.componentType, doc->newGroupModel.type, ids[ 0 ], targetMapName );
		} else {
			srcObjective = ugcObjectiveFind( doc->mission->objectives, srcGroup->objective.objectiveID );
		}
		destObjective = ugcObjectiveFind( doc->mission->objectives, destGroup->objective.objectiveID );
		ugcMissionNodeRemove( srcGroup );

		if( destObjective->type != UGCOBJ_ALL_OF ) {
			UGCMissionObjective* metaObjective = StructCreate( parse_UGCMissionObjective );
			UGCMissionObjective* colDest = StructCreate( parse_UGCMissionObjective );
			UGCMissionObjective* colSrc = StructCreate( parse_UGCMissionObjective );
			UGCMissionTreechartPos pos = { 0 };

			ugcMissionTreechartCalcPos( NULL, destGroup, &pos );
			eaRemove( pos.peaDestObjectives, pos.destObjectiveIndex );

			colSrc->type = UGCOBJ_IN_ORDER;
			colSrc->id = ids[ 3 ];
			eaPush( &colSrc->eaChildren, srcObjective );
			colDest->type = UGCOBJ_IN_ORDER;
			colDest->id = ids[ 2 ];
			eaPush( &colDest->eaChildren, destObjective );
			metaObjective->type = UGCOBJ_ALL_OF;
			metaObjective->id = ids[ 1 ];
			eaPush( &metaObjective->eaChildren, colDest );
			eaInsert( &metaObjective->eaChildren, colSrc, column );

			eaInsert( pos.peaDestObjectives, metaObjective, pos.destObjectiveIndex );
		} else {
			UGCMissionObjective* colSrc = StructCreate( parse_UGCMissionObjective );

			colSrc->type = UGCOBJ_IN_ORDER;
			colSrc->id = ids[ 1 ];
			eaPush( &colSrc->eaChildren, srcObjective );
			eaInsert( &destObjective->eaChildren, colSrc, column );
		}
		
		ugcMissionObjectiveSetInternalMapName(srcObjective,targetMapName);	// Set the objective and all its children to this map
	}
	
	ugcMissionCleanupOrphanGroups();
	ugcEditorFixupObjectivesComponentMapNames( ugcEditorGetProjectData(), doc->mission->objectives, NULL );
	ugcEditorApplyUpdate();
	return true;
}

const char* ugcMissionTreechartMapName( UGCMissionNodeGroup* before, UGCMissionNodeGroup* after )
{
	if( before ) {
		UGCMissionObjective* beforeObjective = ugcMissionTreechartGroupObjective( before );
		if( beforeObjective ) {
			return ugcObjectiveInternalMapName( ugcEditorGetProjectData(), beforeObjective );
		}
	}
	if( after ) {
		UGCMissionObjective* afterObjective = ugcMissionTreechartGroupObjective( after );
		if( afterObjective ) {
			return ugcObjectiveInternalMapName( ugcEditorGetProjectData(), afterObjective );
		}
	}

	return NULL;
}

void ugcMissionTreechartCalcPos( UGCMissionNodeGroup* beforeDestGroup, UGCMissionNodeGroup* afterDestGroup, UGCMissionTreechartPos* out_pos )
{
	if(   (!beforeDestGroup || beforeDestGroup->type == UGCNODE_DUMMY)
		  && (!afterDestGroup || afterDestGroup->type == UGCNODE_DUMMY) ) {
		// Empty mission, would be the first dialog
		out_pos->peaDestObjectives = NULL;
		out_pos->destObjectiveIndex = -1;
		out_pos->destDialogPromptWhen = UGCWHEN_MISSION_START;
		out_pos->destDialogPromptIndex = 0;
	} else if( !beforeDestGroup || beforeDestGroup->type == UGCNODE_DUMMY ) {
		if( afterDestGroup->type == UGCNODE_OBJECTIVE ) {
			out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &afterDestGroup->doc->mission->objectives, afterDestGroup->objective.objectiveID,
																   &out_pos->destObjectiveIndex );
			out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
			out_pos->destDialogPromptIndex = 0;
		} else if( afterDestGroup->type == UGCNODE_DIALOG_TREE ) {
			UGCComponent* afterDialogTree = ugcEditorFindComponentByID( afterDestGroup->dialogTree.componentID );

			switch( ugcComponentStartWhenType(afterDialogTree) ) {
				case UGCWHEN_MISSION_START:
					out_pos->peaDestObjectives = NULL;
					out_pos->destObjectiveIndex = -1;
					out_pos->destDialogPromptWhen = UGCWHEN_MISSION_START;
					out_pos->destDialogPromptIndex = afterDialogTree->dialogBlock.blockIndex;
				xcase UGCWHEN_OBJECTIVE_COMPLETE: {
					out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &afterDestGroup->doc->mission->objectives, afterDialogTree->eaObjectiveIDs[ 0 ],
																		   &out_pos->destObjectiveIndex );
					++out_pos->destObjectiveIndex;
					assert( out_pos->peaDestObjectives );

					out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_COMPLETE;
					out_pos->destDialogPromptIndex = afterDialogTree->dialogBlock.blockIndex;
				}
				xcase UGCWHEN_OBJECTIVE_START: {
					out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &afterDestGroup->doc->mission->objectives, afterDialogTree->eaObjectiveIDs[ 0 ],
																		   &out_pos->destObjectiveIndex );
					out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
					out_pos->destDialogPromptIndex = afterDialogTree->dialogBlock.blockIndex;
				}
				xdefault:
					devassertmsgf( 0, "Unexpected when type: %d", ugcComponentStartWhenType(afterDialogTree) );
			}
		} else if( afterDestGroup->type == UGCNODE_MAP ) {
			out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &afterDestGroup->doc->mission->objectives, afterDestGroup->map.objectiveIDs[ 0 ],
																   &out_pos->destObjectiveIndex );
			out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
			out_pos->destDialogPromptIndex = 0;
		} else if( afterDestGroup->type == UGCNODE_MAP_TRANSITION ) {
			if( !afterDestGroup->mapTransition.prevMapName && !afterDestGroup->mapTransition.nextMapName ) {
				out_pos->peaDestObjectives = &afterDestGroup->doc->mission->objectives;
				out_pos->destObjectiveIndex = 0;
				out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
				out_pos->destDialogPromptIndex = 0;
			} else {
				out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &afterDestGroup->doc->mission->objectives, afterDestGroup->mapTransition.objectiveIDs[ 0 ],
																	   &out_pos->destObjectiveIndex );
				out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
				out_pos->destDialogPromptIndex = 0;
			}
		} else if( afterDestGroup->type == UGCNODE_RETURN_MAP_TRANSITION ) {
			out_pos->peaDestObjectives = &afterDestGroup->doc->mission->objectives;
			out_pos->destObjectiveIndex = eaSize( &afterDestGroup->doc->mission->objectives );
			out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
			out_pos->destDialogPromptIndex = 0;
		}
	} else {
		if( beforeDestGroup->type == UGCNODE_OBJECTIVE ) {
			out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &beforeDestGroup->doc->mission->objectives, beforeDestGroup->objective.objectiveID,
																   &out_pos->destObjectiveIndex );
			++out_pos->destObjectiveIndex;
			out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_COMPLETE;
			out_pos->destDialogPromptIndex = 0;
		} else if( beforeDestGroup->type == UGCNODE_DIALOG_TREE ) {
			UGCComponent* beforeDialogTree = ugcEditorFindComponentByID( beforeDestGroup->dialogTree.componentID );

			switch( ugcComponentStartWhenType(beforeDialogTree) ) {
				case UGCWHEN_MISSION_START:
					out_pos->peaDestObjectives = NULL;
					out_pos->destObjectiveIndex = -1;
					out_pos->destDialogPromptWhen = UGCWHEN_MISSION_START;
					out_pos->destDialogPromptIndex = beforeDialogTree->dialogBlock.blockIndex + 1;
				xcase UGCWHEN_OBJECTIVE_COMPLETE: {
					out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &beforeDestGroup->doc->mission->objectives, beforeDialogTree->eaObjectiveIDs[ 0 ],
																		   &out_pos->destObjectiveIndex );
					++out_pos->destObjectiveIndex;
					assert( out_pos->peaDestObjectives );
					out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_COMPLETE;
					out_pos->destDialogPromptIndex = beforeDialogTree->dialogBlock.blockIndex + 1;
				}
				xcase UGCWHEN_OBJECTIVE_START: {
					out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &beforeDestGroup->doc->mission->objectives, beforeDialogTree->eaObjectiveIDs[ 0 ],
																		   &out_pos->destObjectiveIndex );
					out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_START;
					out_pos->destDialogPromptIndex = beforeDialogTree->dialogBlock.blockIndex + 1;
				}
				xdefault:
					devassertmsgf( 0, "Unexpected when type: %d", ugcComponentStartWhenType(beforeDialogTree) );
			}
		} else if( beforeDestGroup->type == UGCNODE_MAP ) {
			int lastObjectiveID = beforeDestGroup->map.objectiveIDs[ eaiSize( &beforeDestGroup->map.objectiveIDs ) - 1 ];

			out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &beforeDestGroup->doc->mission->objectives, lastObjectiveID,
																   &out_pos->destObjectiveIndex );
			++out_pos->destObjectiveIndex;
			out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_COMPLETE;
			out_pos->destDialogPromptIndex = 0;
		} else if( beforeDestGroup->type == UGCNODE_MAP_TRANSITION ) {
			int lastObjectiveID = beforeDestGroup->map.objectiveIDs[ eaiSize( &beforeDestGroup->mapTransition.objectiveIDs ) - 1 ];

			out_pos->peaDestObjectives = ugcObjectiveFindParentEA( &beforeDestGroup->doc->mission->objectives, lastObjectiveID,
																   &out_pos->destObjectiveIndex );
			++out_pos->destObjectiveIndex;
			out_pos->destDialogPromptWhen = UGCWHEN_OBJECTIVE_COMPLETE;
			out_pos->destDialogPromptIndex = 0;
		}
	}
}

void ugcMissionShowInsertFirstMapMenuCB( UIButton* button, UGCMissionDoc* doc )
{
	doc->insertMapNode = &doc->dummyStartGroup;
	ui_MenuPopupAtCursorOrWidgetBox( doc->insertMapMenu );
}

void ugcMissionShowInsertLastMapMenuCB( UIButton* button, UGCMissionDoc* doc )
{
	doc->insertMapNode = &doc->dummyEndGroup;
	ui_MenuPopupAtCursorOrWidgetBox( doc->insertMapMenu );
}

void ugcMissionShowInsertMapMenuCB( UIButton* button, UGCMissionNodeGroup* node )
{
	UGCMissionDoc* doc = node->doc;
	doc->insertMapNode = node;
	ui_MenuPopupAtCursorOrWidgetBox( doc->insertMapMenu );
}

void ugcMissionSelectInsertMapMenuCB( UIMenuItem* item, UGCMissionLibraryModel* model )
{
	UGCMissionDoc* doc = item->data.voidPtr;

	if( doc->insertMapNode ) {
		if( model->type == UGCMIMO_NEW_PROJECT_MAP ) {
			ugcEditorCreateNewMap( true );
		} else {
			StructCopyAll( parse_UGCMissionLibraryModel, model, &doc->newGroupModel );
			ugcMissionTreechartDragNodeNode( doc->treechart, doc, true, &doc->newGroup, doc->insertMapNode );
		}
	}
}

void ugcMissionSetNoSelectionCB( UIButton* ignored, UGCMissionDoc* doc )
{
	ugcMissionSetNoSelection( doc );
}

static int eaSortCompare( const char** str1, const char** str2 )
{
	return stricmp( *str1, *str2 );
}

void ugcMissionModelsRefresh( UGCMissionDoc* doc )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	UGCMapTransitionInfo** infos = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );
	int it;

	eaClearStruct( &doc->eaInsertMapModel, parse_UGCMissionLibraryModel );

	{
		UGCMissionLibraryModel* newModel;
		newModel = StructCreate( parse_UGCMissionLibraryModel );
		newModel->type = UGCMIMO_CRYPTIC_MAP;
		eaPush( &doc->eaInsertMapModel, newModel );
	}
	for( it = 0; it != eaSize( &ugcProj->maps ); ++it ) {
		bool isUsedInMission = false;

		FOR_EACH_IN_EARRAY( infos, UGCMapTransitionInfo, info ) {
			const char* mapName = ugcObjectiveInternalMapName( ugcProj, ugcObjectiveFind( ugcProj->mission->objectives, info->objectiveID ));

			if( mapName && resNamespaceBaseNameEq( mapName, ugcProj->maps[ it ]->pcName )) {
				isUsedInMission = true;
				break;
			}
		} FOR_EACH_END;

		if( !isUsedInMission ) {
			eaPush( &doc->eaInsertMapModel, ugcCreateLibraryModelMap( ugcProj->maps[ it ]));
		}
	}
	{
		UGCMissionLibraryModel* newModel = ugcCreateLibraryModelNewMap();
		eaPush( &doc->eaInsertMapModel, newModel );
	}

	eaDestroyStruct( &infos, parse_UGCMapTransitionInfo );
}

const char* ugcMissionLibraryModelToTexture( UGCMissionLibraryModel* model )
{
	switch( model->type ) {
		xcase UGCMIMO_NEW_OBJECTIVE:
			return ugcMissionObjectiveComponentTypeToTexture( model->componentType, false, false );
		xcase UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE:
			return ugcMissionObjectiveComponentTypeToTexture( UGC_COMPONENT_TYPE_ROOM_DOOR, false, false );
		xcase UGCMIMO_NEW_TALK_TO_OBJECTIVE:
			return ugcMissionObjectiveComponentTypeToTexture( model->componentType, true, false );
		xcase UGCMIMO_PROJECT_MAP: case UGCMIMO_CRYPTIC_MAP:
			return "ugc_icon_map";
		xcase UGCMIMO_NEW_PROJECT_MAP:
			return "UGC_Icon_NewDocument";

		xdefault:
			return "white";
	}
}

const char* ugcMissionObjectiveComponentTypeToTexture( UGCComponentType type, bool isTalkToObjective, bool isTilted )
{
	switch( type ) {
		case UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		case UGC_COMPONENT_TYPE_CLUSTER_PART:
			if( isTalkToObjective ) {
				if( isTilted ) {
					return "UGC_Icons_Story_Objectives_Inspect_Tilted";
				} else {
					return "UGC_Icons_Story_Objectives_Inspect";
				}
			} else {
				if( isTilted ) {
					return "UGC_Icons_Story_Objectives_Interact_Tilted";
				} else {
					return "UGC_Icons_Story_Objectives_Interact";
				}
			}
		case UGC_COMPONENT_TYPE_ROOM_DOOR: case UGC_COMPONENT_TYPE_FAKE_DOOR:
			if( isTilted ) {
				return "UGC_Icons_Story_Objectives_Unlock_Tilted";
			} else {
				return "UGC_Icons_Story_Objectives_Unlock";
			}
		case UGC_COMPONENT_TYPE_CONTACT:
			if( isTilted ) {
				return "UGC_Icons_Story_Objectives_Dialog_Tilted";
			} else {
				return "UGC_Icons_Story_Objectives_Dialog";
			}
		case UGC_COMPONENT_TYPE_ROOM_MARKER: case UGC_COMPONENT_TYPE_PLANET:
		case UGC_COMPONENT_TYPE_WHOLE_MAP:
			if( isTilted ) {
				return "UGC_Icons_Story_Objectives_Reach_Tilted";
			} else {
				return "UGC_Icons_Story_Objectives_Reach";
			}
		case UGC_COMPONENT_TYPE_KILL:
			if( isTilted ) {
				return "UGC_Icons_Story_Objectives_Kill_Tilted";
			} else {
				return "UGC_Icons_Story_Objectives_Kill";
			}
		default:
			return "white";
	}
}

void ugcMissionPlaySelectedNode( UIButton* ignored, UserData rawDoc )
{
	UGCMissionDoc* doc = (UGCMissionDoc*)rawDoc;

	if( doc->selectedNodeType == UGCNODE_OBJECTIVE || doc->selectedNodeType == UGCNODE_MAP ) {
		UGCMissionObjective* objective = ugcObjectiveFind( doc->mission->objectives, doc->selectedNodeID );
		if( objective ) {
			ugcEditorPlay( NULL, objective->id, true, NULL, NULL );
		}
	}
}

void ugcMissionDeleteSelectedNode( UIButton* ignored, UserData rawDoc )
{
	UGCMissionDoc* doc = rawDoc;
	UGCMissionNodeGroup* selectedGroup = ugcMissionDocNodeGroupFindSelectedNode( doc );

	if( selectedGroup ) {
		ugcMissionTreechartDragNodeTrash( doc->treechart, doc, selectedGroup );
	}
}

void ugcMissionDocHandleAction(UGCMissionDoc *doc, UGCActionID action)
{
	switch (action)
	{
		xcase UGC_ACTION_PLAY_MISSION:
			ugcEditorPlayFromStart();
		xcase UGC_ACTION_MISSION_PLAY_SELECTION:
			ugcMissionPlaySelectedNode( NULL, doc );
		xcase UGC_ACTION_MISSION_DELETE:
			ugcMissionDeleteSelectedNode( NULL, doc );
		xcase UGC_ACTION_MISSION_CREATE_CLICKIE_OBJECTIVE:
			doc->newGroupModel.type = UGCMIMO_NEW_OBJECTIVE;
			doc->newGroupModel.componentType = UGC_COMPONENT_TYPE_OBJECT;
			ugcMissionDocAddNewNode( doc );
		xcase UGC_ACTION_MISSION_CREATE_KILL_OBJECTIVE:
			doc->newGroupModel.type = UGCMIMO_NEW_OBJECTIVE;
			doc->newGroupModel.componentType = UGC_COMPONENT_TYPE_KILL;
			ugcMissionDocAddNewNode( doc );
		xcase UGC_ACTION_MISSION_CREATE_DIALOG_OBJECTIVE:
			doc->newGroupModel.type = UGCMIMO_NEW_TALK_TO_OBJECTIVE;
			doc->newGroupModel.componentType = UGC_COMPONENT_TYPE_CONTACT;
			ugcMissionDocAddNewNode( doc );
		xcase UGC_ACTION_MISSION_CREATE_DIALOG:
			doc->newGroupModel.type = UGCMIMO_NEW_OBJECTIVE;
			doc->newGroupModel.componentType = UGC_COMPONENT_TYPE_DIALOG_TREE;
			ugcMissionDocAddNewNode( doc );
		xcase UGC_ACTION_MISSION_CREATE_MARKER_OBJECTIVE:
			doc->newGroupModel.type = UGCMIMO_NEW_OBJECTIVE;
			doc->newGroupModel.componentType = UGC_COMPONENT_TYPE_ROOM_MARKER;
			ugcMissionDocAddNewNode( doc );
		xcase UGC_ACTION_MISSION_CREATE_UNLOCK_OBJECTIVE:
			doc->newGroupModel.type = UGCMIMO_NEW_OBJECTIVE;
			doc->newGroupModel.componentType = UGC_COMPONENT_TYPE_ROOM_DOOR;
			ugcMissionDocAddNewNode( doc );

		xcase UGC_ACTION_DELETE:
			ugcMissionDeleteSelectedNode( NULL, doc );
	}
}

bool ugcMissionDocQueryAction(UGCMissionDoc *doc, UGCActionID action, char** out_estr)
{
	switch( action ) {
		xcase UGC_ACTION_PLAY_MISSION:
			ugcEditorPlayFromStart();

		xcase UGC_ACTION_MISSION_PLAY_SELECTION:
			return (doc->selectedNodeType == UGCNODE_OBJECTIVE || doc->selectedNodeType == UGCNODE_MAP);

		xcase UGC_ACTION_MISSION_DELETE:
			return (doc->selectedNodeType == UGCNODE_OBJECTIVE || doc->selectedNodeType == UGCNODE_MAP);
		
		xcase UGC_ACTION_MISSION_CREATE_CLICKIE_OBJECTIVE:
		case UGC_ACTION_MISSION_CREATE_KILL_OBJECTIVE:
		case UGC_ACTION_MISSION_CREATE_DIALOG_OBJECTIVE:
		case UGC_ACTION_MISSION_CREATE_DIALOG:
		case UGC_ACTION_MISSION_CREATE_MARKER_OBJECTIVE:
		case UGC_ACTION_MISSION_CREATE_UNLOCK_OBJECTIVE:
			return true;

		xcase UGC_ACTION_DELETE:
			return (doc->selectedNodeType != UGCNODE_DUMMY
					&& doc->selectedNodeType != UGCNODE_MAP_TRANSITION);
	}

	return false;
}

void ugcMissionDocHandleNewMap( UGCMissionDoc* doc, UGCMap* map )
{
	if( doc->insertMapNode ) {
		// Set up the map to be dragged
		doc->newGroupModel.type = UGCMIMO_PROJECT_MAP;
		StructCopyString( &doc->newGroupModel.mapName, map->pcName );

		// Run fixup, so we have a WHOLE_MAP component
		ugcEditorFixupMaps( ugcEditorGetProjectData() );

		ugcMissionTreechartDragNodeNode( doc->treechart, doc, true, &doc->newGroup, doc->insertMapNode );
	}
}

#include "AutoGen/NNOUGCMissionEditor_c_ast.c"

#include "NNOUGCDialogTreeEditor.h"

#include "GfxClipper.h"
#include "MultiEditField.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCommon.h"
#include "NNOUGCDialogPromptPicker.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "NNOUGCZeniPicker.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UGCError.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIPane.h"
#include "UITextureAssembly.h"
#include "UITreechart.h"
#include "WorldGrid.h"
#include "wlUGC.h"

typedef struct UGCDialogTreeNodeGroup UGCDialogTreeNodeGroup;

typedef enum UGCDialogTreeNodeType {
	UGC_DT_NODE_DUMMY,
	UGC_DT_NODE_PROMPT,
	UGC_DT_NODE_END_ACTION,
	UGC_DT_NODE_PROMPT_LINK,
} UGCDialogTreeNodeType; 

/// Structure for each node in the dialog tree
typedef struct UGCDialogTreeNodeGroup {
	UGCDialogTreeDoc* doc;
	UGCDialogTreeNodeType type;
	UGCDialogTreeNodeGroup* parentGroup;
	UGCDialogTreeNodeGroup* prevGroup;
	UGCDialogTreeNodeGroup* nextGroup;
	UGCDialogTreeNodeGroup** childGroups;

	UIPane* parent;

	struct {
		UILabel* label;
	} dummy;

	struct {
		int id;
		UIWidget** eaResponseWidgets;
	} prompt;

	struct {
		int actionIndex;
	} endAction;

	struct {
		int id;
	} promptLink;
} UGCDialogTreeNodeGroup;

/// Top level structure for the DialogTree editor
typedef struct UGCDialogTreeDoc {
	// Special flag -- disallows editing
	bool isPicker;

	// Special callback -- called when the selection changes
	UGCDialogTreeDocSelectionFn selectionFn;
	UserData selectionData;
	
	U32 componentID;

	bool ignoreChanges;

	// properties pane
	UIPane* propertiesPane;
	UISprite* propertiesSprite;

	// Widget tree
	UIPane* pane;
	UIButton* displayNameButton;
	UIButton* playDialogTreeButton;
	UIButton* deleteDialogTreeButton;
	UIButton* usageButton;
	
	UITreechart* treechart;
	UGCDialogTreeNodeGroup** nodeGroups;
	const char** eaContextNames;
	int* promptsInUse;
	UGCDialogTreeNodeGroup newGroup;

	// Selection
	UGCDialogTreeNodeType selectedNodeType;
	int selectedNodeID;
	int selectedNodeSubID;
	bool selectedNodeMakeVisible;
} UGCDialogTreeDoc;

/// Shows if there are no dialog trees to edit
typedef struct UGCNoDialogTreesDoc {
	UIPane* pRootPane;
} UGCNoDialogTreesDoc;

typedef enum UGCDialogTreeUsageType {
	UGC_USAGE_UNKNOWN = 0,
	UGC_USAGE_OBJECTIVE_START,
	UGC_USAGE_OBJECTIVE_COMPLETE,
	UGC_USAGE_OBJECTIVE_GOAL,
	UGC_USAGE_MISSION_START,
	UGC_USAGE_COMPONENT_DEFAULT_TEXT,
	UGC_USAGE_MAP_PLACEMENT,
} UGCDialogTreeUsageType;

typedef struct UGCDialogTreeUsage {
	UGCDialogTreeUsageType type;
	U32 uid;
	const char *map_name;
} UGCDialogTreeUsage;

static UGCDialogTreeUsage ugcDialogTreeDocGetLink( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );
static const char* ugcDialogTreeDocGetMapName( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );
static void ugcDialogTreeDocNavigateToLink( UIButton* ignored, UserData rawDoc );

static UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupRefreshPrompt( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, SA_PARAM_NN_VALID int* pGroupIt, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* parentGroup, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* prevGroup, int promptID );
static UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupRefreshEndAction( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, SA_PARAM_NN_VALID int* pGroupIt, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* parentGroup, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* prevGroup, int actionIndex );
static UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupRefreshPromptLink( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, SA_PARAM_NN_VALID int* pGroupIt, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* parentGroup, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* prevGroup, int promptID );
static UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupIntern( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, SA_PARAM_NN_VALID int* pGroupIt );
static void ugcDialogTreeNodeGroupDestroy( SA_PARAM_NN_VALID UGCDialogTreeNodeGroup* group );
static UGCDialogTreePrompt* ugcDialogTreeGroupGetPrompt( UGCDialogTreeNodeGroup* group );
static UGCDialogTreeNodeGroup* ugcDialogTreeNodeGetChildGroup( UGCDialogTreeNodeGroup* group, int index );
static int ugcDialogTreeGroupGetPromptID( UGCDialogTreeNodeGroup* group );
static UGCDialogTreeNodeGroup* ugcDialogTreePromptGetGroup( UGCDialogTreeDoc* doc, UGCDialogTreePrompt* prompt );
static void ugcDialogTreeDocRefreshTreechart( UGCDialogTreeDoc* doc );
static void ugcDialogTreeDocRefreshProperties( UGCDialogTreeDoc* doc );
static void ugcDialogTreePropertiesOncePerFrame( UGCDialogTreeDoc* doc );
static int ugcDialogTreeGroupGetActionIndex( UGCDialogTreeNodeGroup* group );
static void ugcDialogTreeGroupDeleteSelectedNodeAction( UIButton* button, UGCDialogTreeDoc* doc );
static void ugcDialogTreeGroupMoveUpSelectedNodeAction( UIButton* button, UGCDialogTreeDoc* doc );
static void ugcDialogTreeGroupMoveDownSelectedNodeAction( UIButton* button, UGCDialogTreeDoc* doc );
static void ugcDialogTreeGroupAddAction( UIButton* ignored, UserData rawGroup );
static void ugcDialogTreeSetNoSelectionCB( UIButton* ignored, UGCDialogTreeDoc* doc );
static UGCRuntimeErrorContext* ugcDialogTreeNodeMakeTempErrorContext( UGCDialogTreeNodeGroup* group );

// Treechart callbacks
static bool ugcDialogTreeTreechartDragNodeArrow( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawBeforeDestGroup, UserData rawAfterDestGroup );
static bool ugcDialogTreeTreechartDragArrowNode( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, bool isCommit, UserData rawBeforeSrcGroup, UserData rawAfterSrcGroup, UserData rawDestGroup );
static void ugcDialogTreeTreechartNodeAnimate( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, UserData rawGroup, float x, float y );
static void ugcDialogTreeTreechartNodeTrash( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, UserData rawGroup );
static bool ugcDialogTreeInsertionPlusCB(UITreechart* chart, UserData userData, bool isCommit, UserData prevNode, UserData nextNode);

// Selection handling
static void ugcDialogTreePaneWithLinkDraw( UIPane* pane, UI_PARENT_ARGS );
static void ugcDialogTreeTreechartWithFocusTick( UITreechart* treechart, UI_PARENT_ARGS );
static void ugcDialogTreePaneWithFocusTick( UIPane *pane, UI_PARENT_ARGS );
static void ugcDialogTreeWidgetFocus( UIWidget* ignored, UserData rawGroup );
static void ugcDialogTreeDocSetNoSelection( UGCDialogTreeDoc* doc );
static void ugcDialogTreeDocSetSelectedGroup( UGCDialogTreeNodeGroup* group );
static UGCDialogTreeNodeGroup* ugcDialogTreeDocFindSelectedNode( UGCDialogTreeDoc* doc );
static void ugcDialogTreeUpdateFocusForSelection( UGCDialogTreeDoc* doc );

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

/// Create a new doc for editing this specific Dialog Tree
UGCDialogTreeDoc* ugcDialogTreeDocCreate( U32 componentID )
{
	UGCComponent* component = NULL;

	component = ugcEditorFindComponentByID( componentID );
	if( !component ) {
		return NULL;
	}

	{
		UGCDialogTreeDoc* accum = calloc( 1, sizeof( *accum ));

		accum->componentID = componentID;

		accum->pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );

		// Setup the newGroup group
		accum->newGroup.doc = accum;
		
		ugcDialogTreeDocRefresh( accum );
	
		return accum;
	}
}

/// Destroy the specified editor, does not refresh the UI.
void ugcDialogTreeDocDestroy( UGCDialogTreeDoc* doc )
{
	char strContextName[ 256 ];
	sprintf( strContextName, "UGCDialogTree_%d%s",
			 doc->componentID,
			 (doc->isPicker ? "_Picker" : "") );
	
	ui_TreechartClear( doc->treechart );
	eaDestroyEx( &doc->nodeGroups, ugcDialogTreeNodeGroupDestroy );
	MEContextPush( strContextName, NULL, NULL, NULL );
	MEContextPush( "UGCDialogTreeEditor_Treechart", NULL, NULL, NULL );
	{
		int it;
		for( it = 0; it != eaSize( &doc->eaContextNames ); ++it ) {
			MEContextDestroyByName( doc->eaContextNames[ it ]);
		}
		eaDestroy( &doc->eaContextNames );
	}
	MEContextPop( "UGCDialogTreeEditor_Treechart" );
	MEContextPop( strContextName );
	eaiDestroy( &doc->promptsInUse );
	
	ui_WidgetQueueFreeAndNull( &doc->propertiesPane );
	ui_WidgetQueueFreeAndNull( &doc->propertiesSprite );
	ui_WidgetQueueFreeAndNull( &doc->pane );

	MEContextDestroyByName( strContextName );
	free( doc );
}

/// Set the editor to use the specified splits.
void ugcDialogTreeDocSetVisible( UGCDialogTreeDoc* doc )
{
	ugcEditorSetDocPane( doc->pane );
}

void ugcDialogTreeDocSetSelectedPromptAndAction( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, int promptID, int actionIndex )
{
	doc->selectedNodeType = UGC_DT_NODE_PROMPT;
	doc->selectedNodeID = promptID;
	doc->selectedNodeSubID = 0;

	doc->selectedNodeMakeVisible = true;
	ugcDialogTreeUpdateFocusForSelection( doc );
}

void* ugcDialogTreeIDAsPtrFromIDIndex( U32 componentID )
{
	return (void*)(size_t)componentID;
}

/// Return an ID that will be unique per-Dialog Tree editor.
void* ugcDialogTreeDocGetIDAsPtr( UGCDialogTreeDoc* doc )
{
	return ugcDialogTreeIDAsPtrFromIDIndex( doc->componentID );
}

/// Once per frame for the editor.  Do animation here.
void ugcDialogTreeDocOncePerFrame( UGCDialogTreeDoc* doc, bool isActive )
{
	UGCDialogTreeNodeGroup* selectedNode;
	
	ugcDialogTreePropertiesOncePerFrame( doc );

	// Focus and selection should be in sync.  If this is not the
	// case, then the user has specifically focused on a widget.
	// Therefore, clear the selection.
	selectedNode = ugcDialogTreeDocFindSelectedNode( doc );
	if( selectedNode ) {
		if( !isActive ) {
			ugcDialogTreeDocSetNoSelection( doc );
			ugcEditorQueueUIUpdate();
		} else if( doc->pane && g_ui_State.focused
				   && !ugcAssetLibraryPickerWindowOpen() && !ugcZeniPickerWindowOpen()
				   && !ugcDialogPromptPickerWindowOpen()
				   && !ui_IsFocused( selectedNode->parent ) && !ui_IsFocusedOrChildren( doc->propertiesPane )) {
			ugcDialogTreeDocSetNoSelection( doc );
			ugcEditorQueueUIUpdate();
		}
	}
}

/// UGC Action system callback.
void ugcDialogTreeDocHandleAction( UGCDialogTreeDoc *doc, UGCActionID action )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	switch( action ) {
		xcase UGC_ACTION_DIALOG_PLAY: {
			ugcEditorPlayDialogTree( doc->componentID, -1 );
		}
		
		xcase UGC_ACTION_DIALOG_DELETE: {
			UGCMissionObjective* objective = ugcObjectiveFindComponent( ugcProj->mission->objectives, doc->componentID );
			if( !objective ) {
				UGCComponent* component = ugcEditorFindComponentByID( doc->componentID );
				if( component ) {
					if( UIYes == ugcModalDialogMsg( "UGC_DialogTreeEditor.DeleteDialogTree", "UGC_DialogTreeEditor.DeleteDialogTreeDetails", UIYes | UINo )) {
						ugcComponentOpDelete( ugcProj, component, false );
						ugcEditorApplyUpdate();
					}
				}
			}
		}

		xcase UGC_ACTION_DIALOG_PLAY_SELECTION: {
			UGCDialogTreeNodeGroup* selectedGroup = ugcDialogTreeDocFindSelectedNode( doc );
			if( selectedGroup ) {
				switch( selectedGroup->type ) {
					xcase UGC_DT_NODE_PROMPT:
						ugcEditorPlayDialogTree( doc->componentID, selectedGroup->prompt.id );
						
					xcase UGC_DT_NODE_PROMPT_LINK:
						ugcEditorPlayDialogTree( doc->componentID, selectedGroup->promptLink.id );
				}
			}
		}

		xcase UGC_ACTION_DELETE: {
			UGCDialogTreeNodeGroup* selectedGroup = ugcDialogTreeDocFindSelectedNode( doc );
			if( selectedGroup ) {
				ugcDialogTreeTreechartNodeTrash( doc->treechart, doc, selectedGroup );
			}
		}
	}
}

/// UGC Action system callback.
bool ugcDialogTreeDocQueryAction( UGCDialogTreeDoc *doc, UGCActionID action, char** out_estr )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	switch( action ) {
		xcase UGC_ACTION_DIALOG_PLAY: {
			return true;
		}
		
		xcase UGC_ACTION_DIALOG_DELETE: {
			UGCMissionObjective* objective = ugcObjectiveFindComponent( ugcProj->mission->objectives, doc->componentID );
			if( !objective ) {
				return true;
			} else {
				estrPrintf( out_estr, "Can not delete dialogs used in Talk To Contact tasks" );
			}
		}

		xcase UGC_ACTION_DIALOG_PLAY_SELECTION: {
			UGCDialogTreeNodeGroup* selectedGroup = ugcDialogTreeDocFindSelectedNode( doc );
			if( selectedGroup ) {
				return true;
			}
		}

		xcase UGC_ACTION_DELETE: {
			if( doc->selectedNodeType == UGC_DT_NODE_PROMPT ) {
				UGCDialogTreeNodeGroup* group = ugcDialogTreeDocFindSelectedNode( doc );
				return group != NULL && group->prompt.id > 0;
			} else if( doc->selectedNodeType == UGC_DT_NODE_PROMPT_LINK ) {
				return true;
			}
		}
	}
	
	return false;
}

UGCDialogTreeBlock* ugcDialogTreeDocGetBlock( UGCDialogTreeDoc* doc, UGCComponent** out_component )
{
	UGCComponent* scratch;
	UGCComponent* component = NULL;

	if( !out_component ) {
		out_component = &scratch;
	}
	
	component = ugcEditorFindComponentByID( doc->componentID ); 
	if( !component ) {
		*out_component = NULL;
		return NULL;
	} else {
		*out_component = component;
	}

	return &component->dialogBlock;
}

UGCDialogTreePrompt* ugcDialogTreeDocGetPrompt( UGCDialogTreeDoc* doc, int promptID )
{
	UGCDialogTreeBlock* block = ugcDialogTreeDocGetBlock( doc, NULL );
	if( !block ) {
		return NULL;
	} else {
		return ugcDialogTreeGetPrompt( block, promptID );
	} 
}

/// Given the current data, refresh the UI for the editor.
///
/// *DO NOT* change the data here.  Doing that will break the undo
/// system, the diffs, and just generally cause problems.
void ugcDialogTreeDocRefresh( UGCDialogTreeDoc* doc )
{
	UGCDialogTreeBlock* dialogTree = ugcDialogTreeDocGetBlock( doc, NULL );
	const char* dialogMapName = NULL;
	char* estr = NULL;
	char strContextName[ 256 ];

	if( !dialogTree ) {
		return;
	}

	sprintf( strContextName, "UGCDialogTree_%d%s",
			 doc->componentID,
			 (doc->isPicker ? "_Picker" : "") );
	MEContextPush( strContextName, NULL, NULL, NULL );
	MEContextGetCurrent()->cbChanged = ugcEditorMEFieldChangedCB;

	doc->ignoreChanges = true;

	// Refresh toolbar
	{
		int x = 10;
		int y = 6;
		
		if( nullStr( dialogTree->initialPrompt.pcPromptBody )) {
			ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.DialogName_Default", STRFMT_END );
		} else {
			estrPrintf( &estr, "%s", dialogTree->initialPrompt.pcPromptBody );
		}
		MEExpanderRefreshButton( &doc->displayNameButton, estr, ugcEditorPopupChooserDialogsCB, NULL, 0, 0, 0, 1, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
		SET_HANDLE_FROM_STRING( "UISkin", "UGCComboButton", UI_WIDGET( doc->displayNameButton )->hOverrideSkin );
		ui_WidgetSetPosition( UI_WIDGET( doc->displayNameButton ), x, y );
		ui_ButtonResize( doc->displayNameButton );
		MIN1( UI_WIDGET( doc->displayNameButton )->width, 250 );
		ui_WidgetSetHeight( UI_WIDGET( doc->displayNameButton ), UGC_ROW_HEIGHT * 1.5 - 12 );
		x = ui_WidgetGetNextX( UI_WIDGET( doc->displayNameButton ));

		// Link button (lots of cases!)
		{
			UGCProjectData* ugcProj = ugcEditorGetProjectData();
			UGCDialogTreeUsage usage = ugcDialogTreeDocGetLink( doc );

			switch( usage.type ) {
				xcase UGC_USAGE_MAP_PLACEMENT: {
					UGCMap *map = ugcMapFindByName( ugcProj, usage.map_name );
					if( map ) {
						ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_ComponentOnMap",
											 STRFMT_STRING( "MapName", map->pcDisplayName ),
											 STRFMT_END );
						MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
												 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
						dialogMapName = usage.map_name;
					}
				}

				xcase UGC_USAGE_OBJECTIVE_START: {
					UGCMissionObjective* objective = ugcObjectiveFind( ugcProj->mission->objectives, usage.uid );
					UGCComponent* component = ugcEditorFindComponentByID( objective->componentID );

					if( objective && objective->type == UGCOBJ_COMPLETE_COMPONENT && component && component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP ) {
						UGCMap* map = ugcEditorGetMapByName( objective->strComponentInternalMapName );

						if( map ) {
							ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_MapStart",
												 STRFMT_STRING( "MapName", map->pcDisplayName ),
												 STRFMT_END );
							MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
													 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
						} else {
							ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_CrypticMapStart", STRFMT_END );
							MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
													 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
						}
					} else {
						ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_ObjectiveStart",
											 STRFMT_STRING( "ObjectiveName", ugcMissionObjectiveUIString( objective )),
											 STRFMT_END );
						MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
												 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
					}
				}

				xcase UGC_USAGE_OBJECTIVE_COMPLETE: {
					UGCMissionObjective* objective = ugcObjectiveFind( ugcProj->mission->objectives, usage.uid );
					ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_ObjectiveComplete",
										 STRFMT_STRING( "ObjectiveName", ugcMissionObjectiveUIString( objective )),
										 STRFMT_END );
					MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
											 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
				}

				xcase UGC_USAGE_OBJECTIVE_GOAL: {
					UGCMissionObjective* objective = ugcObjectiveFind( ugcProj->mission->objectives, usage.uid );
					ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_ObjectiveGoal",
										 STRFMT_STRING( "ObjectiveName", ugcMissionObjectiveUIString( objective )),
										 STRFMT_END );
					MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
											 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
				}

				xcase UGC_USAGE_MISSION_START: {
					ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_MissionStart", STRFMT_END );
					MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
											 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
				}

				xcase UGC_USAGE_COMPONENT_DEFAULT_TEXT: {
					UGCComponent* component = ugcEditorFindComponentByID( usage.uid );
					char buffer[ 256 ];
					ugcComponentGetDisplayName( buffer, ugcEditorGetProjectData(), component, false );

					if( component && component->sPlacement.bIsExternalPlacement ) {
						ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_ComponentContactDialog_CrypticMap",
											 STRFMT_STRING( "ComponentName", buffer ),
											 STRFMT_END );
					} else {
						UGCMap* map = ugcEditorGetComponentMap( component );
						ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.Link_ComponentContactDialog_ProjectMap",
											 STRFMT_STRING( "ComponentName", buffer ),
											 STRFMT_STRING( "MapName", ugcMapDisplayName( map )),
											 STRFMT_END );
					}
					MEExpanderRefreshButton( &doc->usageButton, estr, ugcDialogTreeDocNavigateToLink, doc,
											 x, y, 0, 0, UIUnitFixed, 0, UI_WIDGET( doc->pane ));
				}
				
				xdefault: {
					ui_WidgetQueueFreeAndNull( &doc->usageButton );
				}
			}
			if( doc->usageButton ) {
				ui_WidgetSetHeight( UI_WIDGET( doc->usageButton ), UGC_ROW_HEIGHT * 1.5 - 12 );
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Hyperlink", doc->usageButton->widget.hOverrideSkin );
			}
		}

		x = 4;
		if( !doc->deleteDialogTreeButton ) {
			doc->deleteDialogTreeButton = ugcEditorButtonCreate( UGC_ACTION_DIALOG_DELETE, true, true );
			ui_PaneAddChild( doc->pane, doc->deleteDialogTreeButton );
		}
		ui_WidgetSetPositionEx( UI_WIDGET( doc->deleteDialogTreeButton ), x, y, 0, 0, UITopRight );
		ui_WidgetSetDimensions( UI_WIDGET( doc->deleteDialogTreeButton ), UGC_ROW_HEIGHT * 1.5 - 12, UGC_ROW_HEIGHT * 1.5 - 12 );
		x = ui_WidgetGetNextX( UI_WIDGET( doc->deleteDialogTreeButton )) + 5;

		if( !doc->playDialogTreeButton ) {
			doc->playDialogTreeButton = ugcEditorButtonCreate( UGC_ACTION_DIALOG_PLAY, true, false );
			ui_PaneAddChild( doc->pane, doc->playDialogTreeButton );
		}
		ui_WidgetSetPositionEx( UI_WIDGET( doc->playDialogTreeButton ), x, y, 0, 0, UITopRight );
		ui_WidgetSetHeight( UI_WIDGET( doc->playDialogTreeButton ), UGC_ROW_HEIGHT * 1.5 - 12 );
		x = ui_WidgetGetNextX( UI_WIDGET( doc->playDialogTreeButton )) + 5;
	}

	ugcDialogTreeDocRefreshTreechart( doc );
	ugcDialogTreeDocRefreshProperties( doc );

	if( !doc->isPicker ) {
		ui_WidgetSetPaddingEx( UI_WIDGET( doc->treechart ), 0, 0, UGC_PANE_TOP_BORDER, 0 );
	}

	doc->ignoreChanges = false;

	MEContextPop( strContextName );
	estrDestroy( &estr );
}

void ugcDialogTreeDocRefreshTreechart( UGCDialogTreeDoc* doc )
{
	// Refresh the treechart
	if( !doc->treechart ) {
		if( doc->isPicker ) {
			doc->treechart = ui_TreechartCreate( doc, NULL, NULL, NULL, ugcDialogTreeTreechartNodeAnimate, NULL, NULL, NULL, NULL );
		} else {
			doc->treechart = ui_TreechartCreate( doc, NULL, ugcDialogTreeTreechartDragNodeArrow, ugcDialogTreeTreechartDragArrowNode, ugcDialogTreeTreechartNodeAnimate, ugcDialogTreeTreechartNodeTrash, NULL, ugcDialogTreeInsertionPlusCB, NULL );
		}
		ui_PaneAddChild( doc->pane, doc->treechart );
	}
	UI_WIDGET(doc->treechart)->sb->scrollBoundsX = UIScrollBounds_KeepContentsAtViewCenter;
	UI_WIDGET(doc->treechart)->sb->scrollBoundsY = UIScrollBounds_KeepContentsAtViewCenter;
	ui_WidgetSetDimensionsEx( UI_WIDGET( doc->treechart ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCDialogTreeEditor_Treechart", UI_WIDGET( doc->treechart )->hOverrideSkin );
	ui_ScrollAreaSetDraggable( UI_SCROLLAREA( doc->treechart ), true );
	ui_ScrollAreaSetNoCtrlDraggable( UI_SCROLLAREA( doc->treechart ), true );
	UI_SCROLLAREA( doc->treechart )->maxZoomScale = 1;
	UI_SCROLLAREA( doc->treechart )->minZoomScale = 1;
	doc->treechart->widget.tickF = ugcDialogTreeTreechartWithFocusTick;
	doc->treechart->widget.u64 = (U64)doc;
	if( !doc->isPicker ) {
		ui_WidgetGroupMove( &UI_WIDGET( doc->pane )->children, UI_WIDGET( doc->treechart ));
	}
	ui_WidgetSetDimensionsEx( UI_WIDGET( doc->treechart ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	
	// Rebuild the treechart
	{
		int groupIt = 0;
		const char** eaPrevContextNames = NULL;
		eaPushEArray( &eaPrevContextNames, &doc->eaContextNames );
		eaDestroy( &doc->eaContextNames );

		eaiClear( &doc->promptsInUse );

		MEContextPush( "Treechart", NULL, NULL, NULL );
		ui_TreechartBeginRefresh( doc->treechart );

		// dummy start group
		{
			UGCDialogTreeNodeGroup* group = ugcDialogTreeNodeGroupIntern( doc, &groupIt );
			group->type = UGC_DT_NODE_DUMMY;
			if( !group->dummy.label ) {
				group->dummy.label = ui_LabelCreate( "", 0, 0 );
			}

			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCDialogTreeEditor_NoDragLabel", UI_WIDGET( group->dummy.label )->hOverrideSkin );
			ui_LabelSetMessage( group->dummy.label, "UGC_DialogTreeEditor.DialogStart" );
			group->dummy.label->textFrom = UITop;
			ui_LabelSetWidthNoAutosize( group->dummy.label, 380, UIUnitFixed );
			ui_TreechartAddWidget( doc->treechart, NULL, UI_WIDGET( group->dummy.label ), NULL, group, TreeNode_NoDrag | TreeNode_NoSelect );
		}
		
		ugcDialogTreeNodeGroupRefreshPrompt( doc, &groupIt, NULL, NULL, -1 );
		ui_TreechartEndRefresh( doc->treechart );

		// Delete left over contets, no groups
		{
			int it;
			for( it = 0; it != eaSize( &eaPrevContextNames ); ++it ) {
				const char* prevContextName = eaPrevContextNames[ it ];
				if( eaFind( &doc->eaContextNames, prevContextName ) == -1 ) {
					MEContextDestroyByName( prevContextName );
				}
			}
			
			while( groupIt < eaSize( &doc->nodeGroups )) {
				devassert( doc->nodeGroups );
				ugcDialogTreeNodeGroupDestroy( doc->nodeGroups[ groupIt ]);
				eaRemove( &doc->nodeGroups, groupIt );
			}
		}
		MEContextPop( "Treechart" );
	}

	{
		UGCDialogTreeNodeGroup* selectedGroup = ugcDialogTreeDocFindSelectedNode( doc );

		if( selectedGroup ) {
			ui_TreechartSetSelectedChild( doc->treechart, UI_WIDGET( selectedGroup->parent ), doc->selectedNodeMakeVisible );
		} else {
			ui_TreechartSetSelectedChild( doc->treechart, NULL, doc->selectedNodeMakeVisible );
		}
		doc->selectedNodeMakeVisible = false;
	}
}

/// Refresh just the property pane.
void ugcDialogTreeDocRefreshProperties( UGCDialogTreeDoc* doc )
{
	UGCDialogTreeNodeGroup* selectedNode = ugcDialogTreeDocFindSelectedNode( doc );
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIScrollArea* scrollarea;
	UIPane* pane;

	if( !selectedNode || doc->isPicker ) {
		MEContextDestroyByName( "Properties" );
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
		UGCDialogTreePrompt* selectedPrompt = ugcDialogTreeGroupGetPrompt( selectedNode );
		UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_Header_Box" );
		const char* dialogMapName = ugcDialogTreeDocGetMapName( doc );
		MEFieldContext* uiCtx = MEContextPush( "Properties", selectedPrompt, selectedPrompt, parse_UGCDialogTreePrompt );
		UGCComponent* dialogTreeComponent;
		UGCComponent* contactComponent;
		UGCRuntimeErrorContext* errorCtx;
		UIWidget* buttonWidget;
		float closeButtonPadding;
		char* estr = NULL;

		ugcDialogTreeDocGetBlock( doc, &dialogTreeComponent );
		contactComponent = ugcEditorFindComponentByID( dialogTreeComponent->uActorID );
		errorCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp( dialogTreeComponent ), NULL, NULL );
		if( selectedPrompt != &dialogTreeComponent->dialogBlock.initialPrompt ) {
			char promptName[ 256 ];
			sprintf( promptName, "Prompt_%d", selectedPrompt->uid );
			errorCtx->prompt_name = StructAllocString( promptName );
		}

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
		MEContextSetErrorContext( errorCtx );
		MEContextSetErrorFunction( NULL );
		
		entry = MEContextAddButton( NULL, "UGC_icon_window_controls_close", ugcDialogTreeSetNoSelectionCB, doc, "CloseButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		closeButtonPadding = ui_WidgetGetWidth( widget );
		buttonWidget = widget;

		entry = MEContextAddLabelMsg( "Title", "UGC_DialogTreeEditor.Prompt", NULL );
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

		{
			int y = uiCtx->iYPos;

			entry = ugcMEContextAddEditorButton( UGC_ACTION_DIALOG_PLAY_SELECTION, false, false );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
			ui_WidgetSetDimensionsEx( widget, 0.5, UGC_PANE_BUTTON_HEIGHT, UIUnitPercentage, UIUnitFixed );
			ui_WidgetSetPadding( widget, 0, 0 );

			entry = ugcMEContextAddEditorButton( UGC_ACTION_DELETE, false, false );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, y, 0.5, 0, UITopLeft );
			ui_WidgetSetDimensionsEx( widget, 0.5, UGC_PANE_BUTTON_HEIGHT, UIUnitPercentage, UIUnitFixed );
			ui_WidgetSetPadding( widget, 0, 0 );

			uiCtx->iYPos = ui_WidgetGetNextY( widget ) + 5;
		}
		MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );

		scrollarea = MEContextPushScrollAreaParent( "ScrollArea" );
		ui_WidgetSetDimensionsEx( UI_WIDGET( scrollarea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		{
			pane = ugcMEContextPushPaneParentWithHeaderMsg( "Properties", "Header", "UGC_DialogTreeEditor.Properties", true );
			{
				if( contactComponent && contactComponent->eType == UGC_COMPONENT_TYPE_CONTACT ) {
					ugcMEContextAddResourcePickerMsg( "AIAnimList", "UGC_DialogTreeEditor.ContactAnimation_Default", "UGC_DialogTreeEditor.ContactAnimation_PickerTitle", true,
													  "PromptStyle", "UGC_DialogTreeEditor.ContactAnimation", "UGC_DialogTreeEditor.ContactAnimation.Tooltip" );
				}

				ugcMEContextAddMultilineTextMsg( "PromptBody", "UGC_DialogTreeEditor.PromptText", "UGC_DialogTreeEditor.PromptText.Tooltip" );
				ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
				UI_WIDGET( pane )->rightPad = 10;
			}
			MEContextPop( "Properties" );
			MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
			
			FOR_EACH_IN_EARRAY_FORWARDS( selectedPrompt->eaActions, UGCDialogTreePromptAction, action ) {
				char strActionContextName[ 256 ];
				int actionIndex = FOR_EACH_IDX( selectedPrompt->eaActions, action );
				errorCtx->prompt_action_index = actionIndex;

				sprintf( strActionContextName, "Action_%d", actionIndex );
				ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.PromptButtonProperties",
									 STRFMT_INT( "Number", actionIndex + 1 ),
									 STRFMT_END );				
				pane = ugcMEContextPushPaneParentWithHeader( strActionContextName, "Header", estr, true );
				MEContextPush( "Action", action, action, parse_UGCDialogTreePromptAction );
				{
					// Toolbar -- only appears if there are multiple actions
					if( eaSize( &selectedPrompt->eaActions ) > 1 ) {
						UIWidget** eaToolbarWidgets = NULL;
						int toolbarY = MEContextGetCurrent()->iYPos;
						
						entry = MEContextAddButtonMsg( "UGC_DialogTreeEditor.PromptButtonDelete", NULL, ugcDialogTreeGroupDeleteSelectedNodeAction, doc, "DeleteButton", NULL, "UGC_DialogTreeEditor.PromptButtonDelete.Tooltip" );
						widget = UI_WIDGET( ENTRY_BUTTON( entry ));
						widget->u64 = actionIndex;
						eaPush( &eaToolbarWidgets, widget );

						entry = MEContextAddButtonMsg( "UGC_DialogTreeEditor.PromptButtonMoveUp", NULL, ugcDialogTreeGroupMoveUpSelectedNodeAction, doc, "MoveUpButton", NULL, "UGC_DialogTreeEditor.PromptButtonMoveUp.Tooltip" );
						widget = UI_WIDGET( ENTRY_BUTTON( entry ));
						widget->u64 = actionIndex;
						ui_SetActive( widget, actionIndex > 0 );
						eaPush( &eaToolbarWidgets, widget );

						entry = MEContextAddButtonMsg( "UGC_DialogTreeEditor.PromptButtonMoveDown", NULL, ugcDialogTreeGroupMoveDownSelectedNodeAction, doc, "MoveDownButton", NULL, "UGC_DialogTreeEditor.PromptButtonMoveUp.Tooltip" );
						widget = UI_WIDGET( ENTRY_BUTTON( entry ));
						widget->u64 = actionIndex;
						ui_SetActive( widget, actionIndex + 1 < eaSize( &selectedPrompt->eaActions ));
						eaPush( &eaToolbarWidgets, widget );

						{
							int toolbarIt;
							float buttonWidth = 1.0f / eaSize( &eaToolbarWidgets );

							for( toolbarIt = 0; toolbarIt != eaSize( &eaToolbarWidgets ); ++toolbarIt ) {
								widget = eaToolbarWidgets[ toolbarIt ];
								ui_WidgetSetPositionEx( widget, 0, toolbarY, buttonWidth * toolbarIt, 0, UITopLeft );
								ui_WidgetSetWidthEx( widget, buttonWidth, UIUnitPercentage );
							}
							MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );
						}

						eaDestroy( &eaToolbarWidgets );
					}
					
					MEContextAddText( false, ugcDialogTreePromptButtonText( NULL, 0 ), "Text", TranslateMessageKey( "UGC_DialogTreeEditor.PromptButton" ), TranslateMessageKey( "UGC_DialogTreeEditor.PromptButton.Tooltip" ));
					MEContextPush( "ActionStyle", action, action, parse_UGCDialogTreePromptAction );
					MEContextGetCurrent()->bDontSortComboEnums = true;
					{
						MEContextAddEnumMsg( kMEFieldType_Combo, UGCDialogTreePromptActionStyleEnum, "Style", "UGC_DialogTreeEditor.Style", "UGC_DialogTreeEditor.Style.Tooltip" );
					}
					MEContextPop( "ActionStyle" );
					
					ugcMEContextAddCheckedAttribPickerMsg( UGCINPR_CHECKED_ATTRIB_ITEMS | UGCINPR_CHECKED_ATTRIB_SKILLS, "UGC_DialogTreeEditor.PromptButtonRequirement_Default", "UGC_DialogTreeEditor.PromptButtonRequirement_Title", "EnabledCheckedAttrib", "UGC_DialogTreeEditor.PromptButtonRequirement", "UGC_DialogTreeEditor.PromptButtonRequirement.Tooltip" );
					if( dialogMapName ) {
						ugcMEContextAddWhenPickerMsg( dialogMapName, action->pShowWhen, "ShowWhen", 0, false, UGCWhenTypeNormalEnum, "UGC_DialogTreeEditor.ShowWhen", "UGC_DialogTreeEditor.ShowWhen.Tooltip" );
						ugcMEContextAddWhenPickerMsg( dialogMapName, action->pHideWhen, "HideWhen", 0, false, UGCWhenTypeHideEnum, "UGC_DialogTreeEditor.HideWhen", "UGC_DialogTreeEditor.HideWhen.Tooltip" );
					} else {
						ugcMEContextAddWhenPickerMsg( dialogMapName, action->pShowWhen, "ShowWhen", 0, false, UGCWhenTypeNoMapEnum, "UGC_DialogTreeEditor.ShowWhen", "UGC_DialogTreeEditor.ShowWhen.Tooltip" );
						ugcMEContextAddWhenPickerMsg( dialogMapName, action->pHideWhen, "HideWhen", 0, false, UGCWhenTypeNoMapEnum, "UGC_DialogTreeEditor.HideWhen", "UGC_DialogTreeEditor.HideWhen.Tooltip" );
					}
					ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
					UI_WIDGET( pane )->rightPad = 10;
				}
				MEContextPop( "Action" );
				MEContextPop( strActionContextName );
				MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
			} FOR_EACH_END;
			errorCtx->prompt_action_index = -1;

			// Make the add button double height
			{
				float y = MEContextGetCurrent()->iYPos;
				entry = MEContextAddButtonMsg( "UGC_DialogTreeEditor.PromptButtonAdd", NULL, ugcDialogTreeGroupAddAction, selectedNode, "AddButton", NULL, "UGC_DialogTreeEditor.PromptButtonAdd.Tooltip" );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ui_WidgetSetPosition( widget, 0, y );
				ui_WidgetSetDimensionsEx( widget, 1, uiCtx->iYStep, UIUnitPercentage, UIUnitFixed );
				ui_WidgetSetPaddingEx( widget, 0, 10, 0, 0 );
			}
		}
		MEContextPop( "ScrollArea" );
		MEContextPop( "Properties" );

		estrDestroy( &estr );
		StructDestroySafe( parse_UGCRuntimeErrorContext, &errorCtx );
	}
}

void ugcDialogTreePropertiesOncePerFrame( UGCDialogTreeDoc* doc )
{
	UGCDialogTreeNodeGroup* selectedNode = ugcDialogTreeDocFindSelectedNode( doc );
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

UGCDialogTreeUsage ugcDialogTreeDocGetLink( UGCDialogTreeDoc* doc )
{
	UGCComponent* component;
	UGCDialogTreeBlock* dialogTree = ugcDialogTreeDocGetBlock( doc, &component );

	if( !dialogTree || !component ) {
		UGCDialogTreeUsage usage = { 0 };
		return usage;
	}

	if (component->sPlacement.uRoomID != GENESIS_UNPLACED_ID)
	{
		UGCDialogTreeUsage usage = { UGC_USAGE_MAP_PLACEMENT };
		usage.map_name = allocAddString(component->sPlacement.pcMapName); 
		return usage;
	}

	if( ugcComponentStartWhenType( component ) == UGCWHEN_MISSION_START ) {
		UGCDialogTreeUsage usage = { UGC_USAGE_MISSION_START };
		return usage;
	} else if( ea32Size( &component->eaObjectiveIDs )) {
		switch( ugcComponentStartWhenType( component )) {
			xcase UGCWHEN_OBJECTIVE_START: {
				UGCDialogTreeUsage usage = { UGC_USAGE_OBJECTIVE_START };
				usage.uid = component->eaObjectiveIDs[ 0 ];
				return usage;
			}

			xcase UGCWHEN_OBJECTIVE_COMPLETE: {
				UGCDialogTreeUsage usage = { UGC_USAGE_OBJECTIVE_COMPLETE };
				usage.uid = component->eaObjectiveIDs[ 0 ];
				return usage;
			}

			xcase UGCWHEN_OBJECTIVE_IN_PROGRESS: {
				UGCDialogTreeUsage usage = { UGC_USAGE_OBJECTIVE_GOAL };
				usage.uid = component->eaObjectiveIDs[ 0 ];
				return usage;
			}
		}
	}

	if( component->bIsDefault && component->uActorID ) {
		UGCDialogTreeUsage usage = { UGC_USAGE_COMPONENT_DEFAULT_TEXT };
		usage.uid = component->uActorID;
		return usage;
	}

	{
		UGCDialogTreeUsage usage = { 0 };
		return usage;
	}
}

const char* ugcDialogTreeDocGetMapName( UGCDialogTreeDoc* doc )
{
	UGCDialogTreeUsage link = ugcDialogTreeDocGetLink( doc );
	switch( link.type ) {
		xcase UGC_USAGE_COMPONENT_DEFAULT_TEXT: {
			UGCComponent* component = ugcEditorFindComponentByID( link.uid );

			if( component && !component->sPlacement.bIsExternalPlacement ) {
				return component->sPlacement.pcMapName;
			} else {
				return NULL;
			}
		}
		xcase UGC_USAGE_MAP_PLACEMENT:
			return link.map_name;
		xdefault:
			return NULL;
	}
}

void ugcDialogTreeDocNavigateToLink( UIButton* button, UserData rawDoc )
{
	UGCDialogTreeDoc* doc = rawDoc;
	UGCDialogTreeUsage usage = ugcDialogTreeDocGetLink( doc );

	switch( usage.type ) {
		xcase UGC_USAGE_MAP_PLACEMENT:
			ugcEditorEditMapComponent( usage.map_name, doc->componentID, false, true );
		xcase UGC_USAGE_OBJECTIVE_START: case UGC_USAGE_OBJECTIVE_COMPLETE: {
			ugcEditorEditMissionDialogTreeBlock( doc->componentID );
		}
		xcase UGC_USAGE_OBJECTIVE_GOAL: {
			char buffer[ 256 ];
			strcpy( buffer, ugcMissionObjectiveIDLogicalNameTemp( usage.uid ));
			ugcEditorEditMissionObjective( buffer );
		}

		xcase UGC_USAGE_MISSION_START: {
			ugcEditorEditMission();
		}

		xcase UGC_USAGE_COMPONENT_DEFAULT_TEXT: {
			UGCComponent* component = ugcEditorFindComponentByID( usage.uid );
			UGCMap* map = ugcEditorGetComponentMap( component );
			if( map ) {
				ugcEditorEditMapComponent( map->pcName, component->uID, false, true );
			}
		}
	}
}

void ugcDialogTreeTreechartAddGroup( UGCDialogTreeDoc* doc, UGCDialogTreeNodeGroup* prevGroup, UGCDialogTreeNodeGroup* parentGroup, UGCDialogTreeNodeGroup* group, const char* iconName, UITreechartNodeFlags flags )
{
	group->parentGroup = parentGroup;
	group->prevGroup = prevGroup;
	if( prevGroup ) {
		prevGroup->nextGroup = group;
	}
	if( !prevGroup && parentGroup ) {
		eaPush( &parentGroup->childGroups, group );
	}
	
	if( prevGroup ) {
		ui_TreechartAddWidget( doc->treechart, UI_WIDGET( prevGroup->parent ), UI_WIDGET( group->parent ), iconName, group, flags );
	} else {
		if( parentGroup ) {
			ui_TreechartAddChildWidget( doc->treechart, UI_WIDGET( parentGroup->parent ), UI_WIDGET( group->parent ), iconName, group, flags );
		} else {
			ui_TreechartAddWidget( doc->treechart, NULL, UI_WIDGET( group->parent ), iconName, group, flags );
		}
	}
}

static bool ugcDialogTreePromptIDInUse( UGCDialogTreeDoc* doc, int promptID )
{
	return eaiFind( &doc->promptsInUse, promptID ) >= 0;
}

UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupRefreshPrompt( 
				UGCDialogTreeDoc* doc, int* pGroupIt, UGCDialogTreeNodeGroup* parentGroup, 
				UGCDialogTreeNodeGroup* prevGroup, int promptID )
{
	UGCComponent* component = NULL;
	UGCDialogTreeBlock* dialogTree = ugcDialogTreeDocGetBlock( doc, &component );
	UGCDialogTreePrompt* prompt = ugcDialogTreeDocGetPrompt( doc, promptID );
	char contextName[ 256 ];
	UGCDialogTreeNodeGroup* group;
	MEFieldContextEntry* entry;
	UIWidget* widget;

	if( ugcDialogTreePromptIDInUse( doc, promptID )) {
		return ugcDialogTreeNodeGroupRefreshPromptLink( doc, pGroupIt, parentGroup, prevGroup, promptID );
	}
	eaiPush( &doc->promptsInUse, promptID );

	sprintf( contextName, "Prompt_%d", promptID );
	group = ugcDialogTreeNodeGroupIntern( doc, pGroupIt );
	group->type = UGC_DT_NODE_PROMPT;
	group->prompt.id = promptID;

	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetSetPosition( UI_WIDGET( group->parent ), prompt->dialogEditorPos[ 0 ], prompt->dialogEditorPos[ 1 ]);
	if( promptID == -1 ) {
		ui_PaneSetStyle( group->parent, "UGC_Button_NoGradient_Darkbg_Idle_NoPadding", true, false );
	} else {
		ui_PaneSetStyle( group->parent, "UGC_Button_Default_Darkbg_Idle_NoPadding", true, false );
	}
	group->parent->widget.drawF = ugcDialogTreePaneWithLinkDraw;
	group->parent->widget.tickF = ugcDialogTreePaneWithFocusTick;
	group->parent->widget.u64 = (U64)group;
	if( eaSize( &prompt->eaActions ) > 1 ) {
		ui_WidgetSetPaddingEx( UI_WIDGET( group->parent ), 52, 53, 0, 0 );
	} else {
		ui_WidgetSetPaddingEx( UI_WIDGET( group->parent ), 0, 0, 0, 0 );
	}

	{
		MEFieldContext* ctx = MEContextPush( contextName, NULL, NULL, NULL );
		char* estr = NULL;
		const char* bodyText;
		float textPaddingAccum = 0;

		eaPush( &doc->eaContextNames, allocAddString( contextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));

		if( !nullStr( prompt->pcPromptBody )) {
			bodyText = prompt->pcPromptBody;
		} else {
			bodyText = TranslateMessageKey( "UGC_DialogTreeEditor.PromptNode_Default" );
		}

		entry = MEContextAddSprite( "UGC_Icons_Story_Objectives_Dialog_Tilted", "Icon", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		widget->uClickThrough = true;
		ENTRY_SPRITE( entry )->tint = colorFromRGBA( 0x3e3e3e81 );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
		textPaddingAccum = MAX( textPaddingAccum, ui_WidgetGetNextX( widget ));
		
		if( promptID != -1 ) {
			UIWidget* moverWidget;
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
			textPaddingAccum = MAX( textPaddingAccum, ui_WidgetGetNextX( widget ));
		}

		entry = ugcMEContextAddErrorSpriteForDialogTreePrompt( ugcComponentGetLogicalNameTemp( component ), promptID );
		widget = ENTRY_WIDGET( entry );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomRight );

		entry = MEContextAddLabel( "PromptBody", bodyText, NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ENTRY_LABEL( entry )->textFrom = UITop;
		ui_WidgetSetFont( widget, "UGC_Header_Large" );
		ui_WidgetSetPositionEx( widget, 0, 10, 0, 0, UITop );
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ui_WidgetSetPaddingEx( widget, textPaddingAccum, textPaddingAccum, 0, 0 );

		eaClear( &group->prompt.eaResponseWidgets );
		{
			int it;
			for( it = 0; it != eaSize( &prompt->eaActions ); ++it ) {
				UGCDialogTreePromptAction* action = prompt->eaActions[ it ];
				const char* responseText;

				if( !nullStr( action->pcText )) {
					responseText = action->pcText;
				} else {
					responseText = TranslateMessageKey( "UGC_DialogTreeEditor.PromptButtonNode_Default" );
				}

				ugcFormatMessageKey( &estr, "UGC_DialogTreeEditor.PromptButtonNode",
									 STRFMT_STRING( "ButtonText", responseText ),
									 STRFMT_INT( "ButtonNumber", it + 1 ),
									 STRFMT_END );
				entry = MEContextAddLabelIndex( "Response", it, estr, NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				ENTRY_LABEL( entry )->textFrom = UITop;
				ui_WidgetSetPosition( widget, -200, 0 );
				ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 200, UIUnitFixed );
				eaPush( &group->prompt.eaResponseWidgets, widget );
			}
		}

		ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 275, 60 );
		MEContextPop( contextName );
		
		estrDestroy( &estr );
	}

	ugcDialogTreeTreechartAddGroup( doc, prevGroup, parentGroup, group, "ugc_icon_dialogtree", (promptID == -1 ? TreeNode_NoDrag : 0) );

	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcDialogTreeWidgetFocus, group );

	if( eaSize( &prompt->eaActions ) == 1 ) {
		UGCDialogTreePromptAction* action = prompt->eaActions[ 0 ];
		UGCDialogTreePrompt* nextPrompt = ugcDialogTreeDocGetPrompt( doc, action->nextPromptID );
		if( nextPrompt ) {
			ugcDialogTreeNodeGroupRefreshPrompt( doc, pGroupIt, parentGroup, group, action->nextPromptID );
		} else {
			ugcDialogTreeNodeGroupRefreshEndAction( doc, pGroupIt, parentGroup, group, 0 );
		}
	} else {
		int it;
		for( it = 0; it != eaSize( &prompt->eaActions ); ++it ) {
			UGCDialogTreePromptAction* action = prompt->eaActions[ it ];
			UGCDialogTreePrompt* nextPrompt = ugcDialogTreeDocGetPrompt( doc, action->nextPromptID );
			if( nextPrompt ) {
				ugcDialogTreeNodeGroupRefreshPrompt( doc, pGroupIt, group, NULL, action->nextPromptID );
			} else {
				ugcDialogTreeNodeGroupRefreshEndAction( doc, pGroupIt, group, NULL, it );
			}
		}
	}

	return group;
}

static StaticDefineInt UGCDialogTreeResultEnum[] = {
	DEFINE_INT
	{ "UGC_DialogTreeEditor.EndAction_Pass", false },
	{ "UGC_DialogTreeEditor.EndAction_Fail", true },
	DEFINE_END
};

UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupRefreshEndAction( UGCDialogTreeDoc* doc, int* pGroupIt, UGCDialogTreeNodeGroup* parentGroup, UGCDialogTreeNodeGroup* prevGroup, int actionIndex )
{
	UGCDialogTreeNodeGroup* group = ugcDialogTreeNodeGroupIntern( doc, pGroupIt );
	UGCDialogTreeUsage usage = ugcDialogTreeDocGetLink( doc );
	bool allowResultEditing = (usage.type == UGC_USAGE_MAP_PLACEMENT || usage.type == UGC_USAGE_OBJECTIVE_GOAL || usage.type == UGC_USAGE_UNKNOWN);
	UGCDialogTreePromptAction* action;
	char contextName[ 256 ];
	UITextureAssembly* texas;
	MEFieldContextEntry* entry;
	UIWidget* widget;

	if( prevGroup ) {
		UGCDialogTreePrompt* prompt = ugcDialogTreeGroupGetPrompt( prevGroup );
		action = prompt->eaActions[ actionIndex ];
		sprintf( contextName, "Prompt%d_EndAction%d", prompt->uid, actionIndex );
	} else {
		UGCDialogTreePrompt* prompt = ugcDialogTreeGroupGetPrompt( parentGroup );
		action = prompt->eaActions[ actionIndex ];
		sprintf( contextName, "Prompt%d_EndAction%d", prompt->uid, actionIndex );
	}
	group->type = UGC_DT_NODE_END_ACTION;
	group->endAction.actionIndex = actionIndex;	

	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	if( allowResultEditing && action->bDismissAction ) {
		texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Story_Flow_Block_Red" );
	} else {
		texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Story_Flow_Block" );
	}
	ui_PaneSetStyle( group->parent, texas->pchName, true, false );
	group->parent->widget.drawF = ugcDialogTreePaneWithLinkDraw;
	group->parent->widget.tickF = ugcDialogTreePaneWithFocusTick;
	group->parent->widget.u64 = (U64)group;

	{
		MEFieldContext* ctx = MEContextPush( contextName, action, action, parse_UGCDialogTreePromptAction );
		float y;
		eaPush( &doc->eaContextNames, allocAddString( contextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));
		y = 0;

		entry = MEContextAddLabelMsg( "Text", "UGC_DialogTreeEditor.DialogEnd", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		SET_HANDLE_FROM_STRING( g_ui_FontDict, "UGC_Important_Alternate", widget->hOverrideFont );
		ui_LabelResize( ENTRY_LABEL( entry ));
		ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ENTRY_LABEL( entry )->textFrom = UITop;
		y = ui_WidgetGetNextY( widget ) + 5;

		if( allowResultEditing ) {
			entry = MEContextAddSimpleMsg( kMEFieldType_Check, "DismissAction", NULL, NULL );
			widget = ENTRY_FIELD( entry )->pUIWidget;
			SET_HANDLE_FROM_STRING( g_ui_FontDict, "UGC_Important_Alternate", widget->hOverrideFont );
			ui_WidgetSetTextMessage( widget, "UGC_DialogTreeEditor.EndAction" );
			ui_WidgetSetTooltipMessage( widget, "UGC_DialogTreeEditor.EndAction.Tooltip" );
			ui_CheckButtonResize( ENTRY_FIELD( entry )->pUICheck );
			ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
			y = ui_WidgetGetNextY( widget );
		}

		ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 380, ui_WidgetGetNextY( widget ) + ui_TextureAssemblyHeight( texas ));
		MEContextPop( contextName );
	}
	
	ugcDialogTreeTreechartAddGroup( doc, prevGroup, parentGroup, group, "", TreeNode_NoDrag | TreeNode_DragArrowBefore | TreeNode_AlternateArrowBefore );

	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcDialogTreeWidgetFocus, group );

	return group;
}

UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupRefreshPromptLink( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, SA_PARAM_NN_VALID int* pGroupIt, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* parentGroup, SA_PARAM_OP_VALID UGCDialogTreeNodeGroup* prevGroup, int promptID )
{
	UGCDialogTreePrompt* prompt = ugcDialogTreeDocGetPrompt( doc, promptID );
	UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Story_Flow_Block" );
	char contextName[ 256 ];
	UGCDialogTreeNodeGroup* group;
	MEFieldContextEntry* entry;
	UIWidget* widget;

	sprintf( contextName, "PromptLink_%d", *pGroupIt );
	group = ugcDialogTreeNodeGroupIntern( doc, pGroupIt );
	group->type = UGC_DT_NODE_PROMPT_LINK;
	group->promptLink.id = promptID;

	assert( prompt );

	if( !group->parent ) {
		group->parent = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_PaneSetStyle( group->parent, texas->pchName, true, false );
	group->parent->widget.drawF = ugcDialogTreePaneWithLinkDraw;
	group->parent->widget.tickF = ugcDialogTreePaneWithFocusTick;
	group->parent->widget.u64 = (U64)group;

	{
		MEFieldContext* ctx = MEContextPush( contextName, NULL, NULL, NULL );
		float y;
		eaPush( &doc->eaContextNames, allocAddString( contextName ));
		MEContextSetParent( UI_WIDGET( group->parent ));
		y = 0;

		entry = MEContextAddLabelMsg( "Label", "UGC_DialogTreeEditor.PromptLink", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		SET_HANDLE_FROM_STRING( g_ui_FontDict, "UGC_Important_Alternate", widget->hOverrideFont );
		ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
		ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
		ENTRY_LABEL( entry )->textFrom = UITop;
		y = ui_WidgetGetNextY( widget ) + 5;

		entry = MEContextAddLabel( "Target", prompt->pcPromptBody, NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		SET_HANDLE_FROM_STRING( g_ui_FontDict, "UGC_Important_Alternate", widget->hOverrideFont );
		ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
		ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
		ENTRY_LABEL( entry )->textFrom = UITop;
		y = ui_WidgetGetNextY( widget ) + 5;

		ui_WidgetSetDimensions( UI_WIDGET( group->parent ), 380, ui_WidgetGetNextY( widget ) + ui_TextureAssemblyHeight( texas ));
		MEContextPop( contextName );
	}
	
	ugcDialogTreeTreechartAddGroup( doc, prevGroup, parentGroup, group, "ugc_icon_dialogtree", TreeNode_NoDrag | TreeNode_NoSelect );

	ugcWidgetTreeSetFocusCallback( UI_WIDGET( group->parent ), ugcDialogTreeWidgetFocus, group );

	return group;
}


UGCDialogTreeNodeGroup* ugcDialogTreeNodeGroupIntern( UGCDialogTreeDoc* doc, int* pGroupIt )
{
	UGCDialogTreeNodeGroup* group;
	
	if( *pGroupIt >= eaSize( &doc->nodeGroups )) {
		group = calloc( 1, sizeof( *group ));
		group->doc = doc;
		eaPush( &doc->nodeGroups, group );
	}

	assert( doc->nodeGroups );
	assert( *pGroupIt < eaSize( &doc->nodeGroups ));
	group = doc->nodeGroups[ *pGroupIt ];
	++*pGroupIt;

	group->parentGroup = NULL;
	group->prevGroup = NULL;
	group->nextGroup = NULL;
	eaDestroy( &group->childGroups );

	return group;
}

void ugcDialogTreeNodeGroupDestroy( UGCDialogTreeNodeGroup* group )
{
	eaDestroy( &group->prompt.eaResponseWidgets );
	ui_WidgetForceQueueFree( UI_WIDGET( group->dummy.label ));
	ui_WidgetForceQueueFree( UI_WIDGET( group->parent ));
	free( group );
}

static int ugcDialogTreePromptFindPromptAction( UGCDialogTreePrompt* prompt, U32 actionPromptID )
{
	int it = eaSize( &prompt->eaActions ) - 1;
	while( it >= 0 ) {
		UGCDialogTreePromptAction* action = prompt->eaActions[ it ];

		if( action->nextPromptID == actionPromptID ) {
			return it;
		}

		--it;
	}

	return it;
}

UGCDialogTreePrompt* ugcDialogTreeGroupGetPrompt( UGCDialogTreeNodeGroup* group )
{
	int id = ugcDialogTreeGroupGetPromptID( group );

	if( !id ) {
		return NULL;
	} else {
		return ugcDialogTreeDocGetPrompt( group->doc, id );
	}
}

UGCDialogTreeNodeGroup* ugcDialogTreeNodeGetChildGroup( UGCDialogTreeNodeGroup* group, int index )
{
	if( index == 0 && !eaSize( &group->childGroups )) {
		return group->nextGroup;
	} else {
		return eaGet( &group->childGroups, index );
	}
}

int ugcDialogTreeGroupGetPromptID( UGCDialogTreeNodeGroup* group )
{
	if( !group ) {
		return 0;
	}

	switch( group->type ) {
		xcase UGC_DT_NODE_PROMPT:
			return group->prompt.id;

		xcase UGC_DT_NODE_END_ACTION: {
			UGCDialogTreeNodeGroup* prevGroup = (group->prevGroup ? group->prevGroup : group->parentGroup);
			assert( prevGroup && prevGroup->type == UGC_DT_NODE_PROMPT );
			return prevGroup->prompt.id;
		}

		xcase UGC_DT_NODE_PROMPT_LINK:
			return group->promptLink.id;
	}

	return 0;
}

UGCDialogTreeNodeGroup* ugcDialogTreePromptGetGroup( UGCDialogTreeDoc* doc, UGCDialogTreePrompt* prompt )
{
	if( !prompt ) {
		return NULL;
	}

	{
		int it;
		for( it = 0; it != eaSize( &doc->nodeGroups ); ++it ) {
			UGCDialogTreeNodeGroup* group = doc->nodeGroups[ it ];
			if( group->type == UGC_DT_NODE_PROMPT && (U32)group->prompt.id == prompt->uid ) {
				return group;
			}
		}
	}

	return NULL;
}

int ugcDialogTreeGroupGetActionIndex( UGCDialogTreeNodeGroup* group )
{
	if( !group || (!group->prevGroup && !group->parentGroup) ) {
		return -1;
	} else {
		UGCDialogTreeNodeGroup* prevGroup = (group->prevGroup ? group->prevGroup : group->parentGroup);
		UGCDialogTreePrompt* prevPrompt = ugcDialogTreeGroupGetPrompt( prevGroup );
		assert( prevGroup->type == UGC_DT_NODE_PROMPT );

		switch( group->type ) {
			xcase UGC_DT_NODE_PROMPT: case UGC_DT_NODE_PROMPT_LINK: {
				int id;
				int it;
				if( group->type == UGC_DT_NODE_PROMPT ) {
					id = group->prompt.id;
				} else {
					id = group->promptLink.id;
				}
				
				for( it = 0; it != eaSize( &prevPrompt->eaActions ); ++it ) {
					if( prevPrompt->eaActions[ it ]->nextPromptID == (U32)id ) {
						return it;
					}
				}
			}

			xcase UGC_DT_NODE_END_ACTION:
				return group->endAction.actionIndex;
		} 
	}
	
	return -1;
}

void ugcDialogTreeGroupDeleteSelectedNodeAction( UIButton* button, UGCDialogTreeDoc* doc )
{
	UGCDialogTreeNodeGroup* selectedNode = ugcDialogTreeDocFindSelectedNode( doc );
	UGCDialogTreePrompt* selectedPrompt = ugcDialogTreeGroupGetPrompt( selectedNode );
	int actionIndex = (int)UI_WIDGET( button )->u64;

	if( selectedPrompt ) {
		UGCDialogTreePromptAction* action = eaGet( &selectedPrompt->eaActions, actionIndex );
		if( action ) {
			eaRemove( &selectedPrompt->eaActions, actionIndex );
			StructDestroySafe( parse_UGCDialogTreePromptAction, &action );
		}
	}

	ugcEditorQueueApplyUpdate();
}

void ugcDialogTreeGroupMoveUpSelectedNodeAction( UIButton* button, UGCDialogTreeDoc* doc )
{
	UGCDialogTreeNodeGroup* selectedNode = ugcDialogTreeDocFindSelectedNode( doc );
	UGCDialogTreePrompt* selectedPrompt = ugcDialogTreeGroupGetPrompt( selectedNode );
	int actionIndex = (int)UI_WIDGET( button )->u64;

	if( selectedPrompt ) {
		eaSwap( &selectedPrompt->eaActions, actionIndex, actionIndex - 1 );
	}
	
	ugcEditorQueueApplyUpdate();
}

void ugcDialogTreeGroupMoveDownSelectedNodeAction( UIButton* button, UGCDialogTreeDoc* doc )
{
	UGCDialogTreeNodeGroup* selectedNode = ugcDialogTreeDocFindSelectedNode( doc );
	UGCDialogTreePrompt* selectedPrompt = ugcDialogTreeGroupGetPrompt( selectedNode );
	int actionIndex = (int)UI_WIDGET( button )->u64;

	if( selectedPrompt ) {
		eaSwap( &selectedPrompt->eaActions, actionIndex, actionIndex + 1 );
	}
	
	ugcEditorQueueApplyUpdate();
}

UGCRuntimeErrorContext* ugcDialogTreeNodeMakeTempErrorContext( UGCDialogTreeNodeGroup* group )
{
	UGCDialogTreeDoc* doc = group->doc;
	UGCComponent* component = NULL;
	UGCDialogTreeBlock* dialogTree = ugcDialogTreeDocGetBlock( doc, &component );
	UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL );
	int promptID = ugcDialogTreeGroupGetPromptID( group );
	
	if( promptID >= 0 ) {
		char promptName[ 256 ];
		sprintf( promptName, "Prompt_%d", promptID );
		ctx->prompt_name = StructAllocString( promptName );
	}

	return ctx;
}

void ugcDialogTreeGroupAddAction( UIButton* ignored, UserData rawGroup )
{
	UGCDialogTreeNodeGroup* group = rawGroup;
	UGCDialogTreePrompt* prompt = ugcDialogTreeGroupGetPrompt( group );
	assert( prompt );

	eaPush( &prompt->eaActions, StructCreate( parse_UGCDialogTreePromptAction ));

	ugcEditorQueueApplyUpdate();
}

void ugcDialogTreeSetNoSelectionCB( UIButton* ignored, UGCDialogTreeDoc* doc )
{
	ugcDialogTreeDocSetNoSelection( doc );
}

static void ugcDialogTreeNodeRemove( UGCDialogTreeNodeGroup* group )
{
	UGCDialogTreePrompt* prompt = ugcDialogTreeGroupGetPrompt( group );
	UGCDialogTreePrompt* prevPrompt = ugcDialogTreeGroupGetPrompt( group->prevGroup ? group->prevGroup : group->parentGroup );
	int actionIndex;
	assert( prompt && prevPrompt ); //< the initial prompt is not draggable
				
	actionIndex = ugcDialogTreePromptFindPromptAction( prevPrompt, group->prompt.id );
	assert( actionIndex >= 0 );
	StructDestroySafe( parse_UGCDialogTreePromptAction, &prevPrompt->eaActions[ actionIndex ]);
	eaRemove( &prevPrompt->eaActions, actionIndex );
	eaInsertEArray( &prevPrompt->eaActions, &prompt->eaActions, actionIndex );
	eaDestroy( &prompt->eaActions );
}

static int ugcDialogTreeDocPromptIDEq( const UGCDialogTreePrompt* prompt, const void* rawID )
{
	U32 id = (U32)(uintptr_t)rawID;

	return prompt->uid == id;
}

bool ugcDialogTreeTreechartDragNodeArrow( UITreechart* treechart, UserData rawDoc, bool isCommit, UserData rawSrcGroup, UserData rawBeforeDestGroup, UserData rawAfterDestGroup )
{
	UGCDialogTreeDoc* doc = (UGCDialogTreeDoc*)rawDoc;
	UGCDialogTreeBlock* block = ugcDialogTreeDocGetBlock( doc, NULL );
	UGCDialogTreeNodeGroup* srcGroup = (UGCDialogTreeNodeGroup*)rawSrcGroup;
	UGCDialogTreeNodeGroup* beforeDestGroup = (UGCDialogTreeNodeGroup*)rawBeforeDestGroup;
	UGCDialogTreeNodeGroup* afterDestGroup = (UGCDialogTreeNodeGroup*)rawAfterDestGroup;

	assert( srcGroup && (beforeDestGroup || afterDestGroup) );
	
	if( srcGroup->type == UGC_DT_NODE_PROMPT_LINK ) {
		return false;
	}

	if( !beforeDestGroup ) {
		beforeDestGroup = afterDestGroup->prevGroup;
	}
	if( !beforeDestGroup ) {
		beforeDestGroup = afterDestGroup->parentGroup;
	}
	if( !afterDestGroup ) {
		UGCDialogTreeNodeGroup* firstChildGroup = eaGet( &beforeDestGroup->childGroups, 0 );
		afterDestGroup = (firstChildGroup ? firstChildGroup : beforeDestGroup->nextGroup);
	}

	if( beforeDestGroup->type == UGC_DT_NODE_DUMMY ) {
		return false;
	} 

	if( !isCommit || !block || srcGroup == beforeDestGroup || srcGroup == afterDestGroup ) {
		return true;
	}

	switch( srcGroup->type ) {
		xcase UGC_DT_NODE_PROMPT: case UGC_DT_NODE_PROMPT_LINK: {
			UGCDialogTreePrompt* srcPrompt;
			UGCDialogTreePrompt* beforeDestPrompt = ugcDialogTreeGroupGetPrompt( beforeDestGroup );
			int afterDestActionIndex = ugcDialogTreeGroupGetActionIndex( afterDestGroup );

			if( !beforeDestGroup ) {
				beforeDestGroup = afterDestGroup->parentGroup;
				beforeDestPrompt = ugcDialogTreeGroupGetPrompt( beforeDestGroup );
			}
			assert( beforeDestPrompt && afterDestActionIndex >= 0 );

			if( srcGroup == &doc->newGroup ) {
				// Add a new group, add to the list
				srcPrompt = StructCreate( parse_UGCDialogTreePrompt );
				{
					U32 id = 1;
					while( eaFindCmp( &block->prompts, (const void*)(uintptr_t)id, ugcDialogTreeDocPromptIDEq ) >= 0 ) {
						++id;
					}
					srcPrompt->uid = id;
				}
				eaPush( &block->prompts, srcPrompt );
			} else {
				// Remove the old prompt group, move its actions to the top
				srcPrompt = ugcDialogTreeGroupGetPrompt( srcGroup );				
				ugcDialogTreeNodeRemove( srcGroup );
			}
			eaSetSizeStruct( &srcPrompt->eaActions, parse_UGCDialogTreePromptAction, 1 );

			// Insert into the new position
			{
				UGCDialogTreePromptAction* beforeDestAction = beforeDestPrompt->eaActions[ afterDestActionIndex ];

				if( afterDestGroup->type == UGC_DT_NODE_PROMPT ) {
					srcPrompt->eaActions[ 0 ]->nextPromptID = afterDestGroup->prompt.id;
				} else {
					srcPrompt->eaActions[ 0 ]->nextPromptID = 0;
				}
				beforeDestAction->nextPromptID = srcPrompt->uid;
			}

			ugcEditorQueueApplyUpdate();
			return true;
		}
			
		xcase UGC_DT_NODE_END_ACTION:
			;
	}
	
	return false;
}

bool ugcDialogTreeTreechartDragArrowNode( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, bool isCommit, UserData rawBeforeSrcGroup, UserData rawAfterSrcGroup, UserData rawDestGroup )
{
	UGCDialogTreeDoc* doc = (UGCDialogTreeDoc*)rawDoc;
	UGCDialogTreeNodeGroup* destGroup = (UGCDialogTreeNodeGroup*)rawDestGroup;
	UGCDialogTreeNodeGroup* beforeSrcGroup = (UGCDialogTreeNodeGroup*)rawBeforeSrcGroup;
	UGCDialogTreeNodeGroup* afterSrcGroup = (UGCDialogTreeNodeGroup*)rawAfterSrcGroup;
	UGCDialogTreePromptAction* srcAction;

	assert( afterSrcGroup && afterSrcGroup->type == UGC_DT_NODE_END_ACTION );
	srcAction = eaGet( &ugcDialogTreeGroupGetPrompt( afterSrcGroup )->eaActions, afterSrcGroup->endAction.actionIndex );
	assert( srcAction );
	
	switch( destGroup->type ) {
		xcase UGC_DT_NODE_PROMPT: case UGC_DT_NODE_PROMPT_LINK: {
			UGCDialogTreePrompt* destPrompt = ugcDialogTreeGroupGetPrompt( destGroup );
			if( !isCommit || !destPrompt ) {
				return true;
			}

			srcAction->nextPromptID = destPrompt->uid;
			ugcEditorQueueApplyUpdate();
			return true;
		}

		xcase UGC_DT_NODE_END_ACTION:
			;
	}
	
	return false;
}

void ugcDialogTreeTreechartNodeAnimate( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, UserData rawGroup, float x, float y )
{
	UGCDialogTreeDoc* doc = (UGCDialogTreeDoc*)rawDoc;
	UGCDialogTreeNodeGroup* group = (UGCDialogTreeNodeGroup*)rawGroup;

	if( !group ) {
		return;
	}

	switch( group->type ) {
		xcase UGC_DT_NODE_PROMPT: {
			UGCDialogTreePrompt* prompt = ugcDialogTreeDocGetPrompt( doc, group->prompt.id );
			assert( prompt );
			prompt->dialogEditorPos[ 0 ] = x;
			prompt->dialogEditorPos[ 1 ] = y;

			{
				int it;
				for( it = 0; it != eaSize( &prompt->eaActions ); ++it ) {
					UIWidget* responseWidget = eaGet( &group->prompt.eaResponseWidgets, it );
					UGCDialogTreeNodeGroup* nextGroup = ugcDialogTreeNodeGetChildGroup( group, it );

					if( responseWidget ) {
						if( nextGroup ) {
							ui_WidgetSetPositionEx( responseWidget,
													(nextGroup->parent->widget.x - (x + group->parent->widget.leftPad)
													 + (nextGroup->parent->widget.width - responseWidget->width) / 2),
													10, 0, 0, UIBottomLeft );
						} else {
							ui_WidgetSetPositionEx( responseWidget, -100, 0, 0, 0, UITopLeft );
						}
					}
				}
			}
		}
			
		xcase UGC_DT_NODE_END_ACTION: {
			// not yet implemented
		}

		xcase UGC_DT_NODE_PROMPT_LINK: {
			// not yet implemented
		}
	}
}

void ugcDialogTreeTreechartNodeTrash( SA_PARAM_NN_VALID UITreechart* treechart, UserData rawDoc, UserData rawGroup )
{
	UGCDialogTreeDoc* doc = (UGCDialogTreeDoc*)rawDoc;
	UGCDialogTreeBlock* block = ugcDialogTreeDocGetBlock( doc, NULL );
	UGCDialogTreeNodeGroup* group = (UGCDialogTreeNodeGroup*)rawGroup;
	
	switch( group->type ) {
		xcase UGC_DT_NODE_PROMPT: {
			if( group->prompt.id > 0 ) {
				UGCDialogTreePrompt* prompt = ugcDialogTreeGroupGetPrompt( group );
				ugcDialogTreeNodeRemove( group );
				eaFindAndRemove( &block->prompts, prompt );
				StructDestroySafe( parse_UGCDialogTreePrompt, &prompt );

				ugcEditorApplyUpdate();
			}
		}

		xcase UGC_DT_NODE_END_ACTION: {
			// nothing to do -- can't be done
		}

		xcase UGC_DT_NODE_PROMPT_LINK: {
			UGCDialogTreePrompt* prompt = ugcDialogTreeGroupGetPrompt( group->prevGroup ? group->prevGroup : group->parentGroup );
			int actionIndex = ugcDialogTreeGroupGetActionIndex( group );

			if( actionIndex < eaSize( &prompt->eaActions )) {
				prompt->eaActions[ actionIndex ]->nextPromptID = 0;
				ugcEditorApplyUpdate();
			}
		}
	}
}

bool ugcDialogTreeInsertionPlusCB(UITreechart* chart, UserData rawDoc, bool isCommit, UserData prevNode, UserData nextNode)
{
	UGCDialogTreeDoc* doc = (UGCDialogTreeDoc*)rawDoc;
	UGCDialogTreeNodeGroup* beforeDestGroup = (UGCDialogTreeNodeGroup*)prevNode;

	if( beforeDestGroup && beforeDestGroup->type == UGC_DT_NODE_DUMMY ) {
		return false;
	}

	if( !isCommit ) {
		return true;
	}

	if( prevNode || nextNode ) {
		doc->newGroup.type = UGC_DT_NODE_PROMPT;
		ugcDialogTreeTreechartDragNodeArrow( doc->treechart, doc, true, &doc->newGroup,
			prevNode, nextNode);
	}

	return true;
}

/// Pane drawF override function for drawing links
void ugcDialogTreePaneWithLinkDraw( UIPane* pane, UI_PARENT_ARGS )
{
	UGCDialogTreeNodeGroup* group = (UGCDialogTreeNodeGroup*)pane->widget.u64;
	UGCDialogTreeDoc* doc = group->doc;
	UGCDialogTreeNodeGroup* linkedGroup = ugcDialogTreePromptGetGroup( doc, ugcDialogTreeGroupGetPrompt( group ));
	U32 selectedID = 0;
	
	UI_GET_COORDINATES( pane );

	switch( doc->selectedNodeType ) {
		xcase UGC_DT_NODE_PROMPT:
			selectedID = doc->selectedNodeID;
		xcase UGC_DT_NODE_PROMPT_LINK:
			selectedID = doc->selectedNodeSubID;
	}
	
	ui_PaneDraw( pane, UI_PARENT_VALUES );

	if(   group->type == UGC_DT_NODE_PROMPT_LINK && linkedGroup
		  && ((ui_IsHovering( UI_WIDGET( pane )) && !mouseIsDown( MS_LEFT ) && !mouseIsDown( MS_MID ))
			  || (U32)group->promptLink.id == selectedID)) {
		Vec2 start;
		Vec2 end;

		start[ 0 ] = x + w / 2;
		start[ 1 ] = y + h / 2;
		end[ 0 ] = (linkedGroup->parent->widget.x + linkedGroup->parent->widget.width / 2) * scale + doc->treechart->lastDrawX;
		end[ 1 ] = (linkedGroup->parent->widget.y + UGC_PANE_TOP_BORDER / 2) * scale + doc->treechart->lastDrawY;

		ui_TreechartDrawExtraArrowAngled( doc->treechart, start, end, true, scale );
	}
}

/// Treechart tickF override function for handling selection.
void ugcDialogTreeTreechartWithFocusTick( UITreechart* treechart, UI_PARENT_ARGS )
{
	UGCDialogTreeDoc* doc = (UGCDialogTreeDoc*)treechart->widget.u64;
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
		ugcDialogTreeDocSetNoSelection( doc );
	}
	
	if ((treechart->widget.state & kWidgetModifier_Hovering) && (!treechart->widget.uClickThrough))
		inpHandled();
}

/// Pane tickF override function for handling selection
void ugcDialogTreePaneWithFocusTick( UIPane *pane, UI_PARENT_ARGS )
{
	UGCDialogTreeNodeGroup* group = (UGCDialogTreeNodeGroup*)pane->widget.u64;
	UGCDialogTreeDoc* doc = group->doc;
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
	
	if(   !doc->treechart->draggingNode && mouseClickHit( MS_LEFT, &box )
		  && group->type == UGC_DT_NODE_PROMPT ) {
		ui_SetFocus( pane );
		ugcDialogTreeDocSetSelectedGroup( group );
	}
	
	if ((pane->widget.state & kWidgetModifier_Hovering) && (!pane->widget.uClickThrough))
		inpHandled();
}

void ugcDialogTreeWidgetFocus( UIWidget* ignored, UserData rawGroup )
{
	UGCDialogTreeNodeGroup* group = rawGroup;
	ugcDialogTreeDocSetSelectedGroup( group );
}

/// Select nothing (the background).
void ugcDialogTreeDocSetNoSelection( UGCDialogTreeDoc* doc )
{
	doc->selectedNodeType = UGC_DT_NODE_DUMMY;
	doc->selectedNodeID = -1;
	doc->selectedNodeSubID = -1;

	ugcEditorQueueUIUpdate();
	ugcDialogTreeUpdateFocusForSelection( doc );
}

/// Make the specified group be the selected group.
void ugcDialogTreeDocSetSelectedGroup( UGCDialogTreeNodeGroup* group )
{
	UGCDialogTreeDoc* doc = group->doc;
	switch( group->type ) {
		xcase UGC_DT_NODE_PROMPT: {
			doc->selectedNodeType = UGC_DT_NODE_PROMPT;
			doc->selectedNodeID = group->prompt.id;
			doc->selectedNodeSubID = -1;
		}

		xcase UGC_DT_NODE_END_ACTION: {
			doc->selectedNodeType = UGC_DT_NODE_DUMMY;
			doc->selectedNodeID = -1;
			doc->selectedNodeSubID = -1;
		}

		xcase UGC_DT_NODE_PROMPT_LINK: {
			UGCDialogTreeNodeGroup* prevGroup = (group->prevGroup ? group->prevGroup : group->parentGroup);
			if( prevGroup && prevGroup->type == UGC_DT_NODE_PROMPT ) {
				doc->selectedNodeType = UGC_DT_NODE_PROMPT_LINK;
				doc->selectedNodeID = prevGroup->prompt.id;
				doc->selectedNodeSubID = group->promptLink.id;
			}
		}
	}
	ugcDialogTreeUpdateFocusForSelection( doc );

	{
		UGCDialogTreeNodeGroup* selectedGroup = ugcDialogTreeDocFindSelectedNode( doc );

		if( selectedGroup ) {
			ui_TreechartSetSelectedChild( doc->treechart, UI_WIDGET( selectedGroup->parent ), false );
		} else {
			ui_TreechartSetSelectedChild( doc->treechart, NULL, false );
		}
	}
	ugcEditorQueueUIUpdate();

	if( doc->selectionFn ) {
		doc->selectionFn( doc->selectionData );
	}
}

UGCDialogTreeNodeGroup* ugcDialogTreeDocFindSelectedNode( UGCDialogTreeDoc* doc )
{
	if( UGC_DT_NODE_DUMMY == doc->selectedNodeType ) {
		return NULL;
	} else {	
		int it;
		for( it = 0; it != eaSize( &doc->nodeGroups ); ++it ) {
			UGCDialogTreeNodeGroup* group = doc->nodeGroups[ it ];
			if( group->type == doc->selectedNodeType ) {
				switch( group->type ) {
					xcase UGC_DT_NODE_PROMPT: {
						if( group->prompt.id == doc->selectedNodeID ) {
							return group;
						}
					}

					xcase UGC_DT_NODE_END_ACTION: {
						UGCDialogTreeNodeGroup* prevGroup = (group->prevGroup ? group->prevGroup : group->parentGroup);

						if( prevGroup && prevGroup->type == UGC_DT_NODE_PROMPT ) {
							if( group->endAction.actionIndex == doc->selectedNodeSubID && prevGroup->prompt.id == doc->selectedNodeID ) {
								return group;
							}
						}
					}

					xcase UGC_DT_NODE_PROMPT_LINK: {
						UGCDialogTreeNodeGroup* prevGroup = (group->prevGroup ? group->prevGroup : group->parentGroup);

						if( prevGroup && prevGroup->type == UGC_DT_NODE_PROMPT ) {
							if( group->promptLink.id == doc->selectedNodeSubID && prevGroup->prompt.id == doc->selectedNodeID ) {
								return group;
							}
						}
					}
				}
			}
		}
		return NULL;
	}
}

void ugcDialogTreeUpdateFocusForSelection( UGCDialogTreeDoc* doc )
{
	UGCDialogTreeNodeGroup* selectedGroup = ugcDialogTreeDocFindSelectedNode( doc );
	if( SAFE_MEMBER( selectedGroup, parent )) {
		ui_SetFocus( selectedGroup->parent );
	}
}

UGCDialogTreeDoc* ugcDialogTreeDocCreateForPicker( U32 componentID, UGCDialogTreeDocSelectionFn fn, UserData data )
{
	UGCComponent* component = NULL;

	component = ugcEditorFindComponentByID( componentID );
	if( !component ) {
		return NULL;
	}

	{
		UGCDialogTreeDoc* accum = calloc( 1, sizeof( *accum ));

		accum->isPicker = true;
		accum->selectionFn = fn;
		accum->selectionData = data;
		accum->componentID = componentID;

		accum->pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
		accum->propertiesPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );

		// Setup the newGroup group
		accum->newGroup.doc = accum;
		
		ugcDialogTreeDocRefresh( accum );
	
		return accum;
	}
}

UITreechart* ugcDialogTreeDocTreechartForPicker( UGCDialogTreeDoc* doc )
{
	assert( doc->isPicker );
	return doc->treechart;
}

void ugcDialogTreeDocSetDialogTreeForPicker( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, U32 dialogTreeID )
{
	int oldComponentID = doc->componentID;
	assert( doc->isPicker );

	doc->componentID = dialogTreeID;
	ugcDialogTreeDocSetSelectedPromptAndAction( doc, -1, 0 );
	ugcDialogTreeDocRefresh( doc );
	
	if( doc->selectionFn ) {
		doc->selectionFn( doc->selectionData );
	}

	if( oldComponentID ) {
		char strContextName[ 256 ];
		sprintf( strContextName, "UGCDialogTree_%d%s",
				 oldComponentID,
				 (doc->isPicker ? "_Picker" : "") );
		MEContextDestroyByName( strContextName );
	}
}

int ugcDialogTreeDocGetSelectedPromptForPicker( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc )
{
	if( doc->selectedNodeType == UGC_DT_NODE_PROMPT ) {
		return doc->selectedNodeID;
	} else if( doc->selectedNodeType == UGC_DT_NODE_PROMPT_LINK ) {
		return doc->selectedNodeSubID;
	} else {
		return 0;
	}
}

UGCNoDialogTreesDoc* ugcNoDialogTreesDocCreate( void )
{
	UGCNoDialogTreesDoc* pDoc = calloc( 1, sizeof( *pDoc ));
	pDoc->pRootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	return pDoc;
}

void ugcNoDialogTreesDocDestroy( UGCNoDialogTreesDoc** ppDoc )
{
	MEContextDestroyByName( "UGCNoDialogTreesEditor" );
	SAFE_FREE( *ppDoc );
}


void ugcNoDialogTreesDocRefresh( UGCNoDialogTreesDoc* pDoc )
{
	UIPane* pane;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	
	MEContextPush( "UGCNoDialogTreesEditor", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( pDoc->pRootPane ));

	pane = MEContextPushPaneParent( "FTUE" );
	{
		entry = MEContextAddLabelMsg( "Text", "UGC_DialogTreeEditor.FTUEAddDialog", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ENTRY_LABEL( entry )->textFrom = UITop;
		ui_WidgetSetFont( widget, "UGC_Important_Alternate" );
		ui_WidgetSetPositionEx( widget, 0, -UGC_ROW_HEIGHT / 2, 0, 0.5, UITop );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
	}
	MEContextPop( "FTUE" );
	ui_PaneSetStyle( pane, "UGC_Story_BackgroundArea", true, false );
	ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );

	MEContextPop( "UGCNoDialogTreesEditor" );
}

void ugcNoDialogTreesDocSetVisible( UGCNoDialogTreesDoc* pDoc )
{
	ugcEditorSetDocPane( pDoc->pRootPane );
}

void ugcNoDialogTreesDocOncePerFrame( UGCNoDialogTreesDoc* pDoc )
{
	// nothing to do
}

void ugcNoDialogTreesDocHandleAction( UGCNoDialogTreesDoc* pDoc, UGCActionID action )
{
	// nothing to do
}

bool ugcNoDialogTreesDocQueryAction( SA_PARAM_NN_VALID UGCNoDialogTreesDoc* pDoc, UGCActionID action, SA_PARAM_NN_VALID char** out_estr )
{
	return false;
}

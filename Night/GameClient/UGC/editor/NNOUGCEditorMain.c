//// Main entry point for all UGC editing
////
//// Creates the basic UI infrastructure for all the UGC editors.
//// Handles loading/saving of projects, undo, UI updates, etc.
#include "NNOUGCEditorPrivate.h"

#include "GfxConsole.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCostumeEditor.h"
#include "NNOUGCDialogTreeEditor.h"
#include "NNOUGCInteriorCommon.h"
#include "NNOUGCItemEditor.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCProjectEditor.h"
#include "NNOUGCResource.h"
#include "NNOUGCUIResourcePreview.h"
#include "ReferenceSystem_Internal.h"
#include "Regex.h"
#include "ResourceSearch.h"
#include "StringFormat.h"
#include "UGCBugReport.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UGCProjectChooser.h"
#include "UGCProjectUtils.h"
#include "encounter_common.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCMissionEditor.h"
#include "NNOUGCDialogPromptPicker.h"
#include "NNOUGCPlayingEditor.h"

#include "UGCAchievements.h"

#include "../GraphicsLib/GfxSky.h"
#include "../WorldLib/StaticWorld/WorldGridLoadPrivate.h"
#include "../WorldLib/StaticWorld/WorldGridPrivate.h"
#include "Color.h"
#include "CostumeCommon.h"
#include "EditLibUiUtil.h"
#include "EditLibUndo.h"
#include "EditorManager.h"
#include "EditorPrefs.h"
#include "FolderCache.h"
#include "GameClientLib.h"
#include "GclCommandParse.h"
#include "GclEntity.h"
#include "GclUGC.h"
#include "GfxClipper.h"
#include "GfxDebug.h"
#include "gclResourceSnap.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GimmeDLLWrapper.h"
#include "GlobalStateMachine.h"
#include "MultiEditField.h"
#include "MultiEditFieldContext.h"
#include "ObjectLibrary.h"
#include "Player.h"
#include "Prefs.h"
#include "ResourceInfo.h"
#include "ResourceManager.h"
#include "ResourceManagerUI.h"
#include "StateMachine.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "TextFilter.h"
#include "TimedCallback.h"
#include "Timing.h"
#include "UIDialog.h"
#include "UILabel.h"
#include "UIMenu.h"
#include "UIMinimap.h"
#include "UISprite.h"
#include "UITooltips.h"
#include "UITree.h"
#include "UIWindow.h"
#include "UIChat.h"
#include "UUID.h"
#include "UtilitiesLib.h"
#include "WorldGrid.h"
#include "cmdparse.h"
#include "interaction_common.h"
#include "contact_common.h"
#include "continuousBuilderSupport.h"
#include "error.h"
#include "file.h"
#include "fileUtil2.h"
#include "gclBaseStates.h"
#include "gclLogin.h"
#include "gclPatchStreaming.h"
#include "gclUGC.h"
#include "gclUtils.h"
#include "gfxTexAtlas.h"
#include "soundLib.h"
#include "trivia.h"
#include "UGCError.h"
#include "wlState.h"
#include "wlUGC.h"
#include "zutils.h"
#include "TokenStore.h"
#include "NNOUGCZeniPicker.h"
#include "MultiEditFieldContext.h"
#include "CharacterCreationUI.h"
#include "species_common.h"
#include "CostumeCommonTailor.h"
#include "CostumeCommon_h_ast.h"
#include "gclFriendsIgnore.h"
#include "gclClientChat.h"
#include "UICore.h"
#include "NNOUGCUIStarRating.h"
#include "UITextureAssembly.h"
#include "hoglib.h"

#include "AutoGen/ugcprojectcommon_h_ast.h"
#include "AutoGen/GameServerLib_autogen_ServerCmdWrappers.h"
#include "AutoGen/ChatRelay_autogen_GenericServerCmdWrappers.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););
AUTO_RUN_ANON(memBudgetAddMapping("NNOUGCEditorPrivate.h", BUDGET_Editors););

typedef struct ClientPlayerStruct ClientPlayerStruct;

// Each object in here is flashing indicating a change
typedef struct UGCChangeFlash
{
	U32 flash_timer;
	void **objects;
} UGCChangeFlash;

typedef struct UGCChatWindow
{
	char* strHandleName;
	bool hasUnreadMessages;

	// UI -- refreshed only in ugcEditorChatWindowRefresh:
	UIWindow* window;
} UGCChatWindow;

AUTO_STRUCT;
typedef struct UGCRuntimeErrorOrHeader {
	UGCRuntimeError* error;			NO_AST
	char* strHeaderEditorName;		AST( NAME(HeaderEditorName) )
} UGCRuntimeErrorOrHeader;
extern ParseTable parse_UGCRuntimeErrorOrHeader[];
#define TYPE_parse_UGCRuntimeErrorOrHeader UGCRuntimeErrorOrHeader

// Editor global struct

typedef struct UGCEditorDoc
{
	EditUndoStack *edit_undo_stack;
	UGCProjectData *data;
	UGCProjectData *last_save_data;
	UGCProjectData *last_edit_data;

	UGCRuntimeStatus *runtime_status;
	UGCRuntimeError** runtime_status_all_errors;
	UGCRuntimeError** runtime_status_unhandled_errors;
	UGCRuntimeErrorOrHeader** runtime_status_sorted_for_model;
	char *error_prefix;

	bool queued_update;
	bool queued_ui_update;

	UGCProjectData **data_delete_queue;
	int delete_queue_time;

	U32 autosave_timer;

	// Chat data
	ClientPlayerStruct** eaFriends;
	char** eastrChatHandlesActive;

	// Backlink caches
	UGCBacklinkTable* pBacklinkTable;

	// Root UI
	UGCEditorMenus *menus;
	UIPane* pToolbarPane;
	UIPane* pDocPane;
	UIPane* pWindowManagerPane;

	// Popup menus (from the toolbars)
	UIMenu* pMapsListMenu;
	UIMenu* pDialogsListMenu;
	UIMenu* pCostumesListMenu;
	UIMenu* pItemsListMenu;
	UIMenu* pDevModeMenu;

	// Floating windows
	UGCChatWindow* chat_window;
	UGCChatWindow** eaPrivateChatWindows;
	UIWindow* friends_window;
	UIMenu* friends_context_menu;
	UIWindow* errors_window;
	UITree* errors_window_tree;

	// Project UI
	UGCProjectInfo *last_project_info;
	UIWindow *modal_dialog;
	TimedCallback *modal_timeout;
	TimedCallback *job_timeout;
	NOCONST(UGCProjectReviews) *pCachedRecentReviews;
	int iCachedRecentReviewsPageNumber;
	UGCProjectStatusQueryInfo* pCachedStatus;

	// Saved split panel sizes to restore splits after being hidden
	F32 fSavedSplitSize_Prop;
	F32 fSavedSplitSize_Library;

	// Active editor
	UGCEditorView activeView;
	const char* strLastActiveMap;
	const char* strLastActiveDialogTree;
	const char* strLastActiveCostume;

	// Properties
	UIPane *properties_pane;

	// Editors
	UGCProjectEditorDoc* project_editor;
	UGCMissionDoc *mission_editor;
	UGCMapEditorDoc **map_editors;
	UGCNoMapsEditorDoc* no_maps_editor;
	UGCCostumeEditorDoc **costume_editors;
	UGCNoCostumesEditorDoc* no_costumes_editor;
	UGCDialogTreeDoc **dialog_editors;
	UGCNoDialogTreesDoc* no_dialogs_editor;
	UGCItemEditorDoc *item_editor;

	// Modal Map-Chooser Dialog
	UIWindow* modalChooseMapWindow;
	UIList* modalChooseMapList;
	UGCComponent** unplacedComponentsList;
	
	// Animated headshot management (you only get one)
	bool bAnimatedHeadshotThisFrame;

	// Change flash
	UGCChangeFlash **flashes;

	UGCEditorCloseOption bQueueClose;

	bool bLastShouldShowRestoreButtons;

	bool bPublishingEnabled;
} UGCEditorDoc;

typedef struct UGCDialogTreeHeadshotGroup {
	UserData userData;
	bool isPicker;

	UGCUIResourcePreview* headshotPreview;

	UILabel* costumeLabel;
	UIButton* costumeButton;
	UISprite* costumeError;

	UILabel* styleLabel;
	UIButton* styleButton;
} UGCDialogTreeHeadshotGroup;

typedef enum UGCEditorObjectType {
	UGCOBJECT_NONE,
	
	UGCOBJECT_ROOM,
	UGCOBJECT_PATH,
	UGCOBJECT_COMPONENT,
	UGCOBJECT_DIALOG_TREE,
	UGCOBJECT_OBJECTIVE,
	UGCOBJECT_MAPTRANSITION,
	UGCOBJECT_MAP,
} UGCEditorObjectType;

typedef struct UGCWhenAndIndex {
	char* strMapName;
	UGCWhen* when;
	int index;
} UGCWhenAndIndex;

static void ugcEditorSetActiveDoc(UGCEditorViewMode mode, const char *name);
static UGCProjectEditorDoc* ugcEditorGetOrCreateProjectDoc( void );
static UGCMapEditorDoc *ugcEditorGetOrCreateMapDoc( const char* map_name );
static UGCNoMapsEditorDoc* ugcEditorGetOrCreateNoMapsDoc( void );
static UGCCostumeEditorDoc* ugcEditorGetOrCreateCostumeDoc(const char* costume_name);
static UGCNoCostumesEditorDoc* ugcEditorGetOrCreateNoCostumesDoc( void );
static UGCItemEditorDoc* ugcEditorGetOrCreateItemDoc( void );
static UGCDialogTreeDoc* ugcEditorGetOrCreateDialogTreeDoc( U32 dialog_id );
static UGCNoDialogTreesDoc* ugcEditorGetOrCreateNoDialogTreesDoc( void );
static void errorGetUGCField( const UGCRuntimeError* error, UGCEditorType* editorType, const char** editorName, UGCEditorObjectType* objectType, const char** objectName, int* promptActionIndex, const char** fieldName );
static UGCMission *ugcEditorLoadAndGetMission( void );
static int ugcEditorSortErrorOrHeader( const UGCRuntimeErrorOrHeader** ppRow1, const UGCRuntimeErrorOrHeader** ppRow2, const void* ignored );
static void ugcEditorErrorEditorName(UGCRuntimeError* error, char **estrOutput);

void ugcEditorRefreshGlobalUI();
void ugcEditMode(int enabled);
void ugcEditorUpdateWindowVisibility();
void ugcEditorImportProject(char *filename);
void ugcEditorExportProject(char *filename);

#define INIT_PROJECT UGCProjectInfo *project = g_UGCEditorDoc->data ? g_UGCEditorDoc->data->project : NULL
#define UGC_JOB_QUERY_INTERVAL 20 // Update every 20 seconds
#define STANDARD_ROW_HEIGHT  26
#define STANDARD_SPACING_HEIGHT 4
#define UGC_WINDOW_MANAGER_HEIGHT 30

static char **s_eaRegions = NULL;
static const char *s_pcGround = NULL;
static const char *s_pcSpace = NULL;
static bool sIgnoreChanges = false;
static UGCEditorDoc *g_UGCEditorDoc = NULL;

static bool ugcPerfErrorHasFired = false;
static bool ugcPerfKnownStallyOperation = false;

UGCProjectData *gStartupProjectData = NULL;
UGCProjectAutosaveData *gStartupAutosaveData = NULL;
UGCEditorCopyBuffer *ugcCopyBuffer = NULL;
bool gQueueReturnToEditor = false;

/// Control what mode the UGC editor's UI is in.
int g_ugcEditorMode = 0;

// These fields all exist so that minidumps have some extra debugging info about the project being edited.
static char ugcRecentOpsForMiniDump[ 20 * 1024 ];

//if true, then report a fake GSM state to the controller when the editor is loaded and ready to go.
//Used for automated testing
static bool sbInformControllerWhenUGCProjectIsLoaded = false;
AUTO_CMD_INT(sbInformControllerWhenUGCProjectIsLoaded, InformControllerWhenUGCProjectIsLoaded) ACMD_COMMANDLINE;

// MJF TODO: Change this back to false once the mission editor rewrite is done.
static bool gDisableUnhandledErrorCheck = true;
AUTO_CMD_INT(gDisableUnhandledErrorCheck, DisableUnhandledErrorCheck) ACMD_COMMANDLINE;

AUTO_COMMAND ACMD_I_AM_THE_ERROR_FUNCTION_FOR( ugcPerfDebug );
void printUgcPerfDebug( CmdContext *cmd )
{
	// Make it look just like an AUTO_CMD_INT
	estrPrintf( cmd->output_msg, "ugcPerfDebug %d", ugcPerfDebug );
}

AUTO_COMMAND ACMD_NAME( ugcPerfDebug );
void setUgcPerfDebug( int value )
{
	if( value ) {
		ugcPerfDebug = true;
		globCmdParse( "use_load_perfinfo_timers 1" );
	} else {
		ugcPerfDebug = false;
		globCmdParse( "use_load_perfinfo_timers 0" );
	}
}

/// This function records all UGC related trivia data so that crash
/// dumps have useful info.
///
/// See Also: ugcRecentOpsForMiniDump
static void ugcEditorUpdateTrivia( void )
{
	if( ugcEditorIsActive() && ugcEditorGetNamespace() ) {
		triviaPrintf( "UGCProjNamespace", "%s", ugcEditorGetNamespace() );
	} else {
		triviaRemoveEntry( "UGCProjNamespace" );
	}
}

void ugcEditorStartup(void)
{
	ugcGetDefaults();
	ugcEditorGetDefaults();
}

bool ugcEditorIsIgnoringChanges()
{
	return sIgnoreChanges;
}

void ugcEditorSetIgnoreChanges(bool bIgnoreChanges)
{
	sIgnoreChanges = bIgnoreChanges;
}

void ugcEditorUserResourceChanged(void)
{
	int i;

	// Find library panes that need updating
	for(i=eaSize(&g_UGCEditorDoc->map_editors)-1; i>=0; --i) {
		UGCMapEditorDoc *pDoc = g_UGCEditorDoc->map_editors[i];
		if( pDoc->libraryEmbeddedPicker ) {
			UGCAssetLibraryPane *pPane = pDoc->libraryEmbeddedPicker;
			const char* strTagTypeName = ugcAssetLibraryPaneGetTagTypeName( pPane ); 
			UGCAssetTagType *pTagType = RefSystem_ReferentFromString( "TagType", strTagTypeName );
			if (pTagType && (stricmp("PlayerCostume", pTagType->pcDictName) == 0)) {
				ugcAssetLibraryPaneRefreshLibraryModel(pPane);
			}
		}
	}
}

void ugcEditorProjectAllegianceChanged(void)
{
	int i;

	// Find library panes that need updating
	for(i=eaSize(&g_UGCEditorDoc->map_editors)-1; i>=0; --i) {
		UGCMapEditorDoc *pDoc = g_UGCEditorDoc->map_editors[i];
		if( pDoc->libraryEmbeddedPicker ) {
			UGCAssetLibraryPane *pPane = pDoc->libraryEmbeddedPicker;
			ugcAssetLibraryPaneRefreshLibraryModel(pPane);
		}
	}
}

bool ugcEditorObjectRestrictionSetIsValid(WorldUGCRestrictionProperties* props)
{
	UGCProjectData *data = ugcEditorGetProjectData();
	WorldUGCRestrictionProperties* projRestrictions = data ? data->project->pRestrictionProperties : NULL;
	WorldUGCRestrictionProperties restrictions = { 0 };

	if (!data)
		return true;

	if (ugcIsAllegianceEnabled())
	{
		// Only validate allegiance, since that's what the editor lets you choose.
		eaCopyStructs( &projRestrictions->eaFactions, &restrictions.eaFactions, parse_WorldUGCFactionRestrictionProperties );
	}
	if (projRestrictions && ugcIsFixedLevelEnabled())
	{
		restrictions.iMinLevel = projRestrictions->iMinLevel;
		restrictions.iMaxLevel = projRestrictions->iMaxLevel;
	}

	ugcRestrictionsIntersect( &restrictions, props );

	{
		bool result = ugcRestrictionsIsValid( &restrictions );
		StructReset( parse_WorldUGCRestrictionProperties, &restrictions );
		return result;
	}
}

bool ugcEditorEncObjFilter( const char* zmName, ZoneMapEncounterObjectInfo* object, UserData ignored )
{
	if(!zeniObjIsUGC(object)){
		return false;
	}
	
	if( !ugcEditorObjectRestrictionSetIsValid( &object->restrictions )) {
		return false;
	}

	return true;
}

//// Data handling functions

// Clear all the loaded data
static void ugcEditorClearData()
{
	StructReset(parse_UGCEditorView, &g_UGCEditorDoc->activeView);
	g_UGCEditorDoc->activeView.mode = UGC_VIEW_PROJECT_INFO;
	StructReset(parse_UGCRuntimeStatus, g_UGCEditorDoc->runtime_status);
	eaDestroy(&g_UGCEditorDoc->runtime_status_all_errors);
	eaDestroy(&g_UGCEditorDoc->runtime_status_unhandled_errors);
	eaDestroyStruct(&g_UGCEditorDoc->runtime_status_sorted_for_model, parse_UGCRuntimeErrorOrHeader);
	
	ugcProjectEditor_Close(&g_UGCEditorDoc->project_editor);
	ugcMissionDocClose(&g_UGCEditorDoc->mission_editor);
	eaDestroyEx(&g_UGCEditorDoc->map_editors, ugcMapEditorFreeDoc);
	ugcNoMapsEditorFreeDoc( &g_UGCEditorDoc->no_maps_editor );
	eaDestroyEx(&g_UGCEditorDoc->costume_editors, ugcCostumeEditor_Close);
	eaDestroyEx(&g_UGCEditorDoc->dialog_editors, ugcDialogTreeDocDestroy);
	ugcItemEditorDestroy( &g_UGCEditorDoc->item_editor );
	ugcNoCostumesEditor_Close( &g_UGCEditorDoc->no_costumes_editor );
	ugcNoDialogTreesDocDestroy( &g_UGCEditorDoc->no_dialogs_editor );
	EditUndoStackClear(g_UGCEditorDoc->edit_undo_stack);
	ugcBacklinkTableDestroy(&g_UGCEditorDoc->pBacklinkTable);
}

// Load data for a project, or request the resources from the server
static void ugcEditorLoadData(UGCProjectData *pData)
{
	if (!g_UGCEditorDoc) {
		return;
	}

	StructDestroyNoConstSafe(parse_UGCProjectReviews, &g_UGCEditorDoc->pCachedRecentReviews);
	StructDestroySafe(parse_UGCProjectStatusQueryInfo, &g_UGCEditorDoc->pCachedStatus);

	assert(g_UGCEditorDoc);

	sIgnoreChanges = true;
	ugcEditorClearData();
	eaPush(&g_UGCEditorDoc->data_delete_queue, g_UGCEditorDoc->data);
	g_UGCEditorDoc->data = NULL;
	eaPush(&g_UGCEditorDoc->data_delete_queue, g_UGCEditorDoc->last_save_data);
	g_UGCEditorDoc->last_save_data = NULL;
	eaPush(&g_UGCEditorDoc->data_delete_queue, g_UGCEditorDoc->last_edit_data);
	g_UGCEditorDoc->last_edit_data = NULL;
	g_UGCEditorDoc->delete_queue_time = 5;
	g_UGCEditorDoc->strLastActiveMap = NULL;
	g_UGCEditorDoc->strLastActiveDialogTree = NULL;
	g_UGCEditorDoc->strLastActiveCostume = NULL;

	if (pData)
	{
		ugcResourceInfoPopulateDictionary();

		g_UGCEditorDoc->data = StructClone(parse_UGCProjectData, pData);
		eaIndexedEnable(&g_UGCEditorDoc->data->components->eaComponents, parse_UGCComponent);

		ugcEditorFixupPostEdit(false);

		g_UGCEditorDoc->last_save_data = StructClone(parse_UGCProjectData, g_UGCEditorDoc->data);

		if (gStartupAutosaveData)
		{
			if ( UIYes == ugcModalDialogMsg( "UGC.AutosaveRecoverDialogTitle", "UGC.AutosaveRecoverDialogContents", UIYes | UINo ))
			{
				StructCopy(parse_UGCProjectData, gStartupAutosaveData->pData, g_UGCEditorDoc->data, 0, 0, 0);
				ugcEditorFixupPostEdit(false);
				ugcModalDialogMsg( "UGC_Editor.AutosaveRecovered", "UGC_Editor.AutosaveRecoveredDetails", UIOk );
			}
			else
			{
				ServerCmd_DeleteAutosaveUGCProject(g_UGCEditorDoc->data->ns_name, UGC_EDITOR_CLOSE_NONE);
			}
			StructDestroySafe(parse_UGCProjectAutosaveData, &gStartupAutosaveData);
		}

		g_UGCEditorDoc->last_edit_data = StructClone(parse_UGCProjectData, g_UGCEditorDoc->data);

		assert(g_UGCEditorDoc->last_edit_data);
		assert(g_UGCEditorDoc->last_save_data);

		if (sbInformControllerWhenUGCProjectIsLoaded)
		{
			gclReportStateToController("FakeState_UGCProjectLoaded");
		}

		{
			ContainerID uProjectID = UGCProject_GetContainerIDFromUGCNamespace(pData->ns_name);
			ServerCmd_QueryUGCProjectStatus();
			gclUGC_RequestReviewsForPage( uProjectID, /*uSeriesID=*/0, 0 );
		}
	}

	ugcPerfKnownStallyOperation = true;
	ugcEditorUpdateUI();
	sIgnoreChanges = false;
}

void ugcEditorSetStartupData(UGCProjectData *project_data, UGCProjectAutosaveData *autosave_data)
{
	if (project_data)
		gStartupProjectData = StructClone(parse_UGCProjectData, project_data);
	if (autosave_data && autosave_data->pData)
		gStartupAutosaveData = StructClone(parse_UGCProjectAutosaveData, autosave_data);
}



//// Copy/Paste

UGCEditorCopyBuffer *ugcEditorStartCopy(UGCEditorCopyType type)
{
	StructDestroySafe(parse_UGCEditorCopyBuffer, &ugcCopyBuffer);
	ugcCopyBuffer = StructCreate(parse_UGCEditorCopyBuffer);
	ugcCopyBuffer->eType = type;
	return ugcCopyBuffer;
}

UGCEditorCopyBuffer *ugcEditorCurrentCopy(void)
{
	return ugcCopyBuffer;
}

void ugcEditorAbortCopy(void)
{
	StructDestroySafe(parse_UGCEditorCopyBuffer, &ugcCopyBuffer);
}

//// Getters


UGCProjectData *ugcEditorGetProjectData(void)
{
	return SAFE_MEMBER(g_UGCEditorDoc, data);
}

UGCProjectData *ugcEditorGetLastSaveData(void)
{
	return SAFE_MEMBER(g_UGCEditorDoc, last_save_data);
}

UGCBacklinkTable* ugcEditorGetBacklinkTable(void)
{
	return SAFE_MEMBER(g_UGCEditorDoc, pBacklinkTable);
}

const char *ugcEditorGetNamespace(void)
{
	return SAFE_MEMBER2(g_UGCEditorDoc, data, ns_name);
}

UGCMap **ugcEditorGetMapsList(void)
{
	return SAFE_MEMBER2(g_UGCEditorDoc, data, maps);
}

UGCMap* ugcEditorGetMapByName(const char* name)
{
	if(!SAFE_MEMBER2(g_UGCEditorDoc, data, maps))
		return NULL;

	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->maps, UGCMap, map)
	{
		if (resNamespaceBaseNameEq(map->pcName, name))
			return map;
	}
	FOR_EACH_END

	return NULL;
}

UGCMap* ugcEditorGetComponentMap(const UGCComponent *component)
{
	if (!component)
		return NULL;

	if(component->sPlacement.bIsExternalPlacement)
		return NULL;

	return ugcEditorGetMapByName(component->sPlacement.pcMapName);
}

UGCComponentList *ugcEditorGetComponentList(void)
{
	return SAFE_MEMBER2(g_UGCEditorDoc, data, components);
}

UGCComponent* ugcEditorFindComponentByID(U32 id)
{
	UGCComponentList* list = ugcEditorGetComponentList();
	if( !list ) {
		return NULL;
	} else {
		return ugcComponentFindByID(list, id);
	}
}

UGCMission *ugcEditorGetMission(void)
{
	return SAFE_MEMBER2(g_UGCEditorDoc, data, mission);
}

UGCCostume **ugcEditorGetCostumesList(void)
{
	return SAFE_MEMBER2(g_UGCEditorDoc, data, costumes);
}

UGCCostume* ugcEditorGetCostumeByName(const char* name)
{
	if(!SAFE_MEMBER2(g_UGCEditorDoc, data, costumes))
		return NULL;

	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->costumes, UGCCostume, costume)
	{
		if (resNamespaceBaseNameEq(costume->astrName, name))
			return costume;
	}
	FOR_EACH_END

	return NULL;
}

UGCCostumeEditorDoc *ugcEditorGetCostumeDoc(const char *costume_name)
{
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, doc)
	{
		if (resNamespaceBaseNameEq(doc->astrName, costume_name))
			return doc;
	}
	FOR_EACH_END;
	return NULL;
}

UGCItem **ugcEditorGetItemsList(void)
{
	return SAFE_MEMBER2(g_UGCEditorDoc, data, items);
}

UGCItem* ugcEditorGetItemByName(const char* name)
{
	if(!SAFE_MEMBER(g_UGCEditorDoc, data))
		return NULL;

	return ugcItemFindByName( g_UGCEditorDoc->data, name );
}

UGCItemEditorDoc *ugcEditorGetItemDoc(void)
{
	return g_UGCEditorDoc->item_editor;
}

UGCMapEditorDoc *ugcEditorGetMapDoc(const char *map_name)
{
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, doc)
	{
		if (resNamespaceBaseNameEq(ugcMapEditorGetName(doc), map_name))
			return doc;
	}
	FOR_EACH_END;
	return NULL;
}

UGCMapEditorDoc *ugcEditorGetComponentMapDoc(const UGCComponent *component)
{
	if(component->sPlacement.bIsExternalPlacement)
		return NULL;

	return ugcEditorGetMapDoc(component->sPlacement.pcMapName);
}

//// Errors

UGCRuntimeStatus *ugcEditorGetRuntimeStatus(void)
{
	return g_UGCEditorDoc->runtime_status;
}

UGCRuntimeError** ugcEditorGetRuntimeErrors(void)
{
	return g_UGCEditorDoc->runtime_status_all_errors;
}

void ugcEditorSetErrorPrefix(const char *pcPrefixFormat, ...)
{
	estrDestroy(&g_UGCEditorDoc->error_prefix);

	if (pcPrefixFormat)
	{
		va_list ap;
		va_start( ap, pcPrefixFormat );
		estrConcatfv(&g_UGCEditorDoc->error_prefix, pcPrefixFormat, ap);
		va_end( ap );
	}
}

bool ugcEditorMEFieldErrorCB(UGCRuntimeErrorContext*pErrorContext, const char *pchFieldName, int iFieldIndex, char **estrToolTip_out)
{
	return ugcEditorErrorPrint( ugcEditorGetRuntimeStatus(), pErrorContext, pchFieldName, iFieldIndex, estrToolTip_out );
}

static void ugcErrorAddKeys( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx, const char* fieldName )
{
	char** fieldNameParsed = NULL;
	int stageIt;
	int errorIt;

	DivideString( fieldName, " ", &fieldNameParsed, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];

			if(   (eaSize( &fieldNameParsed ) == 0 || eaFindString( &fieldNameParsed, error->field_name ) >= 0)
				  && StructCompare( parse_UGCRuntimeErrorContext, error->context, ctx, 0, 0, 0 ) == 0
				  && stricmp( error->message_key, "UGC.Component_NotPlaced" ) != 0
				  && stricmp( error->message_key, "UGC.Component_NoMaps" ) != 0 ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled(error);
			}
		}
	}
	eaDestroyEx( &fieldNameParsed, NULL );
}

bool ugcEditorErrorPrint(const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* pErrorContext, const char *pchFieldName, int iFieldIndex, char **estrToolTip_out)
{
	const UGCRuntimeError** errors = NULL;
	
	{
		char** fieldNameParsed = NULL;
		int stageIt;
		int errorIt;
		DivideString( pchFieldName, " ", &fieldNameParsed, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );

		for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
			const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
			for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
				const UGCRuntimeError* error = stage->errors[ errorIt ];
				char error_field[256] = { 0 };
				if (!nullStr(SAFE_MEMBER(g_UGCEditorDoc, error_prefix)))
				{
					if (!strStartsWith(error->field_name, g_UGCEditorDoc->error_prefix))
						continue;
					strcpy(error_field, &error->field_name[strlen(g_UGCEditorDoc->error_prefix)]);
				}
				else if (error->field_name)
				{
					strcpy(error_field, error->field_name);
				}

				if(   (eaSize( &fieldNameParsed ) == 0 || eaFindString( &fieldNameParsed, error_field ) >= 0)
					  && StructCompare( parse_UGCRuntimeErrorContext, error->context, pErrorContext, 0, 0, 0 ) == 0
					  && stricmp( error->message_key, "UGC.Component_NotPlaced" ) != 0
					  && stricmp( error->message_key, "UGC.Component_NoMaps" )) {
					eaPush( &errors, error );
					ugcEditorErrorHandled( error );
				}
			}
		}
	
		eaDestroyEx( &fieldNameParsed, NULL );
	}

	if( eaSize( &errors ) > 0 ) {
		int it;
		estrPrintf( estrToolTip_out, "<b><font scale=1.2>%d Task%s</font></b>", eaSize( &errors ), eaSize( &errors ) != 1 ? "s" : "");

		for( it = 0; it != eaSize( &errors ); ++it ) {
			const UGCRuntimeError* error = errors[ it ];

			if( error->extraText ) {
				estrConcatf( estrToolTip_out, "<p>%s<br>%s</p>",
							 TranslateMessageKey( error->message_key ),
							 error->extraText );
			} else {
				estrConcatf( estrToolTip_out, "<p>%s</p>", TranslateMessageKey( error->message_key ));
			}
			
		}
	}
	eaDestroy( &errors );

	if (estrLength(estrToolTip_out) > 0)
		return true;
	return false;
}

void ugcEditorErrorPrint2( const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx1, const char* fieldName1, const UGCRuntimeErrorContext* ctx2, const char* fieldName2, char** estrTooltip_out )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeys( &errorKeys, runtimeStatus, ctx1, fieldName1 );
	ugcErrorAddKeys( &errorKeys, runtimeStatus, ctx2, fieldName2 );

	if( eaSize( &errorKeys ) > 0 ) {
		int it;
		estrPrintf( estrTooltip_out, "<b><font scale=1.2>%d Task%s</font></b>", eaSize( &errorKeys ), eaSize( &errorKeys ) != 1 ? "s" : "");

		for( it = 0; it != eaSize( &errorKeys ); ++it ) {
			estrConcatf( estrTooltip_out, "<p>%s</p>", TranslateMessageKey( errorKeys[ it ] ));
		}
	}
	eaDestroy( &errorKeys );
}

void ugcErrorButtonRefreshInternal( UISprite** icon, const char** errorKeys )
{
	char* errorText = NULL;
	
	if( !*icon ) {
		*icon = ui_SpriteCreate( 0, 0, 1, 1, "" );
	}

	
	if( eaSize( &errorKeys ) > 0 ) {
		int it;
		estrPrintf( &errorText, "<b><font scale=1.2>%d Task%s</font></b>", eaSize( &errorKeys ), eaSize( &errorKeys ) != 1 ? "s" : "");

		for( it = 0; it != eaSize( &errorKeys ); ++it ) {
			estrConcatf( &errorText, "<p>%s</p>", TranslateMessageKey( errorKeys[ it ] ));
		}
	}

	if( errorText ) {
		ui_SpriteSetTexture( *icon, "ugc_icons_labels_alert" );
		ui_SpriteResize( *icon );
	} else {
		ui_SpriteSetTexture( *icon, "alpha8x8" );
	}
	ui_WidgetSetTooltipString( UI_WIDGET( *icon ), errorText );
	estrDestroy( &errorText );
}

void ugcErrorButtonRefresh( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx, const char* fieldName, F32 y, UIWidget* parent )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeys( &errorKeys, runtimeStatus, ctx, fieldName );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
	
	if( parent ) {
		ui_WidgetRemoveFromGroup( UI_WIDGET( *icon ));
		ui_WidgetAddChild( parent, UI_WIDGET( *icon ));
	}
	ui_WidgetSetPositionEx( UI_WIDGET( *icon ), 0, y, 0, 0, UITopRight );
}

static void ugcErrorAddKeysForEditor( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, UGCEditorType editor )
{
	int stageIt;
	int errorIt;
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			UGCEditorType errorEditor;
			errorGetUGCField( error, &errorEditor, NULL, NULL, NULL, NULL, NULL );

			if( errorEditor == editor ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				// Intentionally do *NOT* call ugcEditorErrorHandled.
				// Each error still needs to appear on each field.
			}
		}
	}
}

void ugcErrorButtonRefreshForEditor( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, UGCEditorType editor )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeysForEditor( &errorKeys, runtimeStatus, editor );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
}

static void ugcErrorAddKeysForMissionMapTransition( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, const char* strNextObjectiveName )
{
	int stageIt;
	int errorIt;
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			UGCEditorType errorEditor;
			UGCEditorObjectType errorObjectType;
			const char* errorObjectName;
			errorGetUGCField( error, &errorEditor, NULL, &errorObjectType, &errorObjectName, NULL, NULL );

			if(   errorEditor == UGCEDITOR_MISSION && errorObjectType == UGCOBJECT_MAPTRANSITION
				  && stricmp( errorObjectName, strNextObjectiveName ) == 0 ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled( error );
			}
		}
	}
}

void ugcErrorButtonRefreshForMissionMapTransition( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const char* strNextObjectiveName )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeysForMissionMapTransition( &errorKeys, runtimeStatus, strNextObjectiveName );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
}

static void ugcErrorAddKeysForMissionMap( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, const char* strNextMapName, const char* strNextObjectiveName )
{
	int stageIt;
	int errorIt;
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			UGCEditorType errorEditor;
			const char* errorEditorName;
			UGCEditorObjectType errorObjectType;
			const char* errorObjectName;
			errorGetUGCField( error, &errorEditor, &errorEditorName, &errorObjectType, &errorObjectName, NULL, NULL );

			// NOTE: MissionEditor Map nodes duplicate some fields from the
			// corresponding map editor.  This list MUST be kept in sync with
			// ugcMissionNodeGroupRefreshMapProperties()'s display!
			if(   (errorEditor == UGCEDITOR_MISSION && errorObjectType == UGCOBJECT_MAP
				   && stricmp( errorObjectName, strNextObjectiveName ) == 0)
				  || (errorEditor == UGCEDITOR_MAP && stricmp( errorEditorName, strNextMapName ) == 0
					  && stricmp( error->field_name, "DisplayName" ) == 0 )) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled( error );
			}
		}
	}
}

void ugcErrorButtonRefreshForMissionMap( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const char* strNextMapName, const char* strNextObjectiveName )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeysForMissionMap( &errorKeys, runtimeStatus, strNextMapName, strNextObjectiveName );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
}

static void ugcErrorAddKeysForMissionObjective( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, const char* strObjectiveName )
{
	UGCMissionObjective* objective = ugcObjectiveFindByLogicalName( ugcEditorGetMission()->objectives, strObjectiveName );
	int stageIt;
	int errorIt;

	if( !objective ) {
		return;
	}
	
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			UGCEditorType errorEditor;
			const char* errorEditorName;
			UGCEditorObjectType errorObjectType;
			const char* errorObjectName;
			int errorPromptActionIndex;
			errorGetUGCField( error, &errorEditor, &errorEditorName, &errorObjectType, &errorObjectName, &errorPromptActionIndex, NULL );

			// NOTE: MissionEditor Objective nodes duplicate the text field from the
			// corresponding DialogTree editor.  This list MUST be kept in sync with
			// ugcMissionNodeGroupRefreshObjectivePropertiesDialogTree()'s display!
			if(   errorEditor == UGCEDITOR_MISSION && errorObjectType == UGCOBJECT_OBJECTIVE
				  && stricmp( errorObjectName, strObjectiveName ) == 0 ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled( error );
			} else if( errorEditor == UGCEDITOR_DIALOG_TREE && errorPromptActionIndex == -1
					   && -1 == ugcPromptNameGetID( errorObjectName )
					   && objective->componentID == ugcComponentNameGetID( errorEditorName )
					   && (stricmp( error->field_name, "PromptBody" ) == 0
						   || stricmp( error->field_name, "PromptStyle" ) == 0) ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled( error );
			}
		}
	}
}

void ugcErrorButtonRefreshForMissionObjective( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const char* strObjectiveName )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeysForMissionObjective( &errorKeys, runtimeStatus, strObjectiveName );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
}

static void ugcErrorAddKeysForMapComponent( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, const char* strComponentName )
{
	int stageIt;
	int errorIt;
	
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			UGCEditorType errorEditor;
			const char* errorEditorName;
			UGCEditorObjectType errorObjectType;
			const char* errorObjectName;
			int errorPromptActionIndex;
			errorGetUGCField( error, &errorEditor, &errorEditorName, &errorObjectType, &errorObjectName, &errorPromptActionIndex, NULL );

			if(   errorEditor == UGCEDITOR_MAP && errorObjectType == UGCOBJECT_COMPONENT
				  && stricmp( errorObjectName, strComponentName ) == 0 ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled( error );
			}
		}
	}
}

void ugcErrorButtonRefreshForMapComponent( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const char* strComponentName )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeysForMapComponent( &errorKeys, runtimeStatus, strComponentName );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
}

static void ugcErrorAddKeysForDialogTreePrompt( const char*** out_peaErrorKeys, const UGCRuntimeStatus* runtimeStatus, const char* strDialogTreeName, U32 promptID )
{
	int stageIt;
	int errorIt;
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			UGCEditorType errorEditor;
			const char* errorEditorName;
			const char* errorObjectName;
			errorGetUGCField( error, &errorEditor, &errorEditorName, NULL, &errorObjectName, NULL, NULL );

			if(   errorEditor == UGCEDITOR_DIALOG_TREE && ugcPromptNameGetID( errorObjectName ) == promptID
				  && stricmp( errorEditorName, strDialogTreeName ) == 0 ) {
				eaPushUnique( out_peaErrorKeys, error->message_key );
				ugcEditorErrorHandled( error );
			}
		}
	}
}

void ugcErrorButtonRefreshForDialogTreePrompt( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const char* strDialogTreeName, U32 promptID )
{
	const char** errorKeys = NULL;
	ugcErrorAddKeysForDialogTreePrompt( &errorKeys, runtimeStatus, strDialogTreeName, promptID );
	ugcErrorButtonRefreshInternal( icon, errorKeys );
	eaDestroy( &errorKeys );
}

int ugcErrorCount( const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx )
{
	int errorCount = 0;

	int stageIt;
	int errorIt;
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			
			if( StructCompare( parse_UGCRuntimeErrorContext, error->context, ctx, 0, 0, 0 ) == 0 ) {
				++errorCount;
			}
		}
	}

	return errorCount;
}

const UGCRuntimeError **ugcErrorList( const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx )
{
	int errorCount = 0;
	const UGCRuntimeError **ret = NULL;

	int stageIt;
	int errorIt;
	for( stageIt = 0; stageIt != eaSize( &runtimeStatus->stages ); ++stageIt ) {
		const UGCRuntimeStage* stage = runtimeStatus->stages[ stageIt ];
		for( errorIt = 0; errorIt != eaSize( &stage->errors ); ++errorIt ) {
			const UGCRuntimeError* error = stage->errors[ errorIt ];
			
			if( StructCompare( parse_UGCRuntimeErrorContext, error->context, ctx, 0, 0, 0 ) == 0 ) {
				eaPush(&ret, error);
			}
		}
	}

	return ret;
}

//// UI utilities

const char* ugcItemSpecifierGetDisplayName( const char* itemName )
{
	if( nullStr( itemName )) {
		return NULL;
	} else {
		UGCItem* pUGCItem = ugcItemFindByName(ugcEditorGetProjectData(), itemName);
		if (pUGCItem) {
			return pUGCItem->strDisplayName;
		} else {
			return NULL;
		}
	}
}

const char* ugcResourceGetDisplayName(const char *object_type, const char *object_name, const char *default_label)
{
	const char *label = NULL;
	static char buffer[256];

	if (object_name && strlen(object_name) > 0)
	{
		if (stricmp(object_type, "PlayerCostume") == 0)
			label = ugcCostumeSpecifierGetDisplayName( ugcEditorGetProjectData(), object_name );
		else if (stricmp(object_type, "ItemDef") == 0)
			label = ugcItemSpecifierGetDisplayName( object_name );
		else if (stricmp(object_type, "CheckedAttrib") == 0) {
			char messageKey[ RESOURCE_NAME_MAX_SIZE ];
			sprintf( messageKey, "UGC.CheckedAttrib_%s", object_name );
			label = TranslateMessageKey( messageKey );

			if( !label ) {
				sprintf( buffer, "%s (UNTRANSLATED)", object_name );
				label = buffer;
			}
		} else if (stricmp(object_type, "TrapPower") == 0) {
			char messageKey[ RESOURCE_NAME_MAX_SIZE ];
			sprintf( messageKey, "UGC.TrapPowerGroup.%s", object_name );
			label = TranslateMessageKey( messageKey );

			if( !label ) {
				sprintf( buffer, "%s (UNTRANSLATED)", object_name );
				label = buffer;
			}
		} else {
			const WorldUGCProperties* ugcProps = ugcResourceGetUGCProperties(object_type, object_name);
			if (ugcProps)
			{
				label = TranslateDisplayMessage(ugcProps->dVisibleName);
				if (!label)
				{
					sprintf(buffer, "%s (UNTRANSLATED)", object_name);
					label = buffer;
				}
			}
			else if (stricmp(object_type, "ObjectLibrary") == 0)
			{
				GroupDef *def = objectLibraryGetGroupDefByName(object_name, false);
				if (def)
					label = def->name_str;
				else
					label = object_name;
			}
			else
				label = object_name;
		}
	}

	if (!label)
		label = default_label;

	return label;
}

const char* ugcTrapGetDisplayName( int objlibID, const char* trapPower, const char* defaultName )
{
	static char buffer[ 512 ];

	if( objlibID && !nullStr( trapPower )) {
		char objlibIDAsText[ 256 ];
		sprintf( objlibIDAsText, "%d", objlibID );
		sprintf( buffer, "%s, %s",
				 ugcResourceGetDisplayName( "ObjectLibrary", objlibIDAsText, "UNNAMED" ),
				 ugcResourceGetDisplayName( "TrapPower", trapPower, "UNNAMED" ));
		return buffer;
	} else {
		return defaultName;
	}
}

// MJF Feb/2/2013 -- This currently uses the map editor convention
// that 0 = up and angles increase clockwise.  This convention was
// taken from the UGCMapEditor, the main user of this function at the
// time it was written.
void ugcEditorSetCursorForRotation( float rot )
{
	if( rot > PI * 7 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_Bottom" );
	} else if( rot > PI * 5 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_BottomRight" );
	} else if( rot > PI * 3 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_Right" );
	} else if( rot > PI * 1 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_TopRight" );
	} else if( rot > -PI * 1 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_Top" );
	} else if( rot > -PI * 3 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_TopLeft" );
	} else if( rot > -PI * 5 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_Left" );
	} else if( rot > -PI * 7 / 8.0 ) {
		ui_SetCursorByName( "UGC_Cursors_Rotate_BottomLeft" );
	} else {
		ui_SetCursorByName( "UGC_Cursors_Rotate_Bottom" );
	}
}

// NOTE: None of the data here needs to be copied.
typedef struct UGCPickerData {
	void* data;
	ParseTable* pti;
	int fieldColumn;
	int fieldColumn2;
	int index;
	const char* strPickerTitle;
	const char* strPickerNote;
	
	MEFieldChangeCallback changedFn;
	UserData changedData;

	// Only for resource pickers:
	const char* objectType;

	// Only for component pickers:
	const char* defaultMapName;
	UGCZeniPickerFilterType forceFilterType;

	// Only for location pickers:
	UGCMapLocation* defaultMapLocation;

	// Only for costume pickers
	bool allowProjectCostumes;

	// Only for checked attrib pickers:
	UGCInteractPropertiesFlags attribFlags;

	// If set, this function will get called to do extra filtering
	UGCAssetLibraryCustomFilterFn filterFn;
	UserData filterData;
} UGCPickerData;

static void ugcMEContextAddResourcePickerSetCB( UGCAssetLibraryPane* pane, UGCPickerData* pickerData, UGCAssetLibraryRow* row )
{
	if( TOK_GET_TYPE( pickerData->pti[ pickerData->fieldColumn ].type ) == TOK_STRING_X ) {
		TokenStoreSetString( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, row->pcName, NULL, NULL, NULL, NULL );
	} else if( TOK_GET_TYPE( pickerData->pti[ pickerData->fieldColumn ].type ) == TOK_REFERENCE_X ) {
		TokenStoreSetRef( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, row->pcName, NULL, NULL );
	} else {
		int value = atoi( row->pcName );
		if( value ) {
			TokenStoreSetIntAuto( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, value, NULL, NULL );
		}
	}
	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddResourcePickerClearCB( UIButton* ignored, UGCPickerData* pickerData )
{
	if( TOK_GET_TYPE( pickerData->pti[ pickerData->fieldColumn ].type ) == TOK_STRING_X ) {
		TokenStoreClearString( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, NULL );
	} else if( TOK_GET_TYPE( pickerData->pti[ pickerData->fieldColumn ].type ) == TOK_REFERENCE_X ) {
		TokenStoreClearRef( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, false, NULL );
	} else {
		TokenStoreSetIntAuto( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, 0, NULL, NULL );
	}
	
	ugcEditorQueueApplyUpdate();
}

static const char* ugcMEContextResourcePickerGetString( ParseTable* pti, int column, void* data, char* intNameBuffer, int intNameBuffer_size )
{
	// For object library pieces, it's possible the column is an
	// integer.  In this case it's an object library ID, and the
	// resource name is its string representation.
	if( TOK_GET_TYPE( pti[ column ].type ) == TOK_STRING_X ) {
		return TokenStoreGetString( pti, column, data, 0, NULL );
	} else if( TOK_GET_TYPE( pti[ column ].type ) == TOK_REFERENCE_X ) {
		return TokenStoreGetRefString( pti, column, data, 0, NULL );
	} else {
		int intValue = TokenStoreGetIntAuto( pti, column, data, 0, NULL );
		if( intValue ) {
			sprintf_s( SAFESTR2( intNameBuffer ), "%d", intValue );
			return intNameBuffer;
		} else {
			return NULL;
		}
	}
}

static void ugcMEContextAddResourcePickerShowCB( UIButton* ignored, UGCPickerData* pickerData )
{
	char intNameBuffer[ 20 ];
	const char* strValue = ugcMEContextResourcePickerGetString( pickerData->pti, pickerData->fieldColumn, pickerData->data, SAFESTR( intNameBuffer ));
	
	UGCAssetLibraryPane* pane = ugcAssetLibraryShowPicker(
			pickerData, true, pickerData->strPickerTitle, pickerData->strPickerNote, pickerData->objectType, strValue, ugcMEContextAddResourcePickerSetCB );
	if( pickerData->filterFn ) {
		ugcAssetLibraryPaneSetExtraFilter( pane, pickerData->filterFn, pickerData->filterData );
	}
}

MEFieldContextEntry* ugcMEContextAddResourcePicker(const char* strObjectType, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddResourcePickerEx( strObjectType, NULL, NULL, strDefaultText, strPickerTitle, allowTrashButton, NULL, strFieldName, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddResourcePickerEx(const char* strObjectType, UGCAssetLibraryCustomFilterFn filterFn, UserData filterData, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strPickerNote, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	MEFieldContext* ctx = MEContextGetCurrent();
	ParseTable* pti = MEContextGetParseTable();
	void* data = MEContextGetData();
	UGCAssetTagType* tagType = RefSystem_ReferentFromString( "TagType", strObjectType );
	int column;

	assert( pti && data && tagType );
	if( ParserFindColumn( pti, strFieldName, &column )) {
		char intNameBuffer[20];
		const char* strValue = ugcMEContextResourcePickerGetString( pti, column, data, SAFESTR( intNameBuffer ));
		UGCPickerData* pickerData;
		MEFieldContextEntry* entry;

		pickerData = MEContextAllocMem( strFieldName, sizeof( *pickerData ), NULL, false );
		pickerData->data = data;
		pickerData->pti = pti;
		pickerData->fieldColumn = column;
		pickerData->index = -1;
		pickerData->changedFn = ctx->cbChanged;
		pickerData->changedData = ctx->pChangedData;
		
		pickerData->objectType = strObjectType;
		pickerData->strPickerTitle = strPickerTitle;
		pickerData->strPickerNote = strPickerNote;
		pickerData->filterFn = filterFn;
		pickerData->filterData = filterData;
		
		entry = MEContextAddButton( ugcResourceGetDisplayName( tagType->pcDictName, strValue, strDefaultText ), NULL,
									ugcMEContextAddResourcePickerShowCB, pickerData,
									strFieldName, strDisplayName, strTooltip );
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_PickerButton", UI_WIDGET( ENTRY_BUTTON( entry ))->hOverrideSkin );
		if( !nullStr( strValue ) && allowTrashButton ) {
			MEContextEntryAddActionButton( entry, NULL, "UGC_Icons_Labels_Delete", ugcMEContextAddResourcePickerClearCB, pickerData, ctx->iWidgetHeight, NULL );
		}
		return entry;
	} else {
		assert( false );
	}
}

MEFieldContextEntry* ugcMEContextAddResourcePickerMsg(const char* strObjectType, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddResourcePickerExMsg( strObjectType, NULL, NULL, strDefaultText, strPickerTitle, allowTrashButton, NULL, strFieldName, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddResourcePickerExMsg(const char* strObjectType, UGCAssetLibraryCustomFilterFn filterFn, UserData filterData, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strPickerNote, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddResourcePickerEx( strObjectType,
											filterFn,
											filterData,
											TranslateMessageKey( strDefaultText ),
											TranslateMessageKey( strPickerTitle ),
											allowTrashButton,
											TranslateMessageKey( strPickerNote ),
											strFieldName,
											TranslateMessageKey( strDisplayName ),
											TranslateMessageKey( strTooltip ));
}

static void ugcMEContextAddCostumePickerSetCostumeCB( UserData ignored, UGCPickerData* pickerData, UGCAssetLibraryRow* row )
{
	int columnCostume = pickerData->fieldColumn;
	int columnPetCostume = pickerData->fieldColumn2;
	
	if( row && row->pcName ) {
		TokenStoreSetString( pickerData->pti, columnCostume, pickerData->data, pickerData->index, row->pcName, NULL, NULL, NULL, NULL );
		TokenStoreClearRef( pickerData->pti, columnPetCostume, pickerData->data, pickerData->index, false, NULL );
	}

	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddCostumePickerSetContactCB( const char* zmapName, const char* logicalName, UGCPickerData* pickerData )
{
	int columnCostume = pickerData->fieldColumn;
	int columnPetCostume = pickerData->fieldColumn2;

	if( zmapName && logicalName ) {
		ZoneMapEncounterObjectInfo* zeniObj = zeniObjectFind( zmapName, logicalName );

		if( zeniObj ) {
			TokenStoreSetString( pickerData->pti, columnCostume, pickerData->data, pickerData->index, REF_STRING_FROM_HANDLE( zeniObj->ugcContactCostume ), NULL, NULL, NULL, NULL );
			TokenStoreClearRef( pickerData->pti, columnPetCostume, pickerData->data, pickerData->index, false, NULL );
		}
	}

	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddCostumePickerSetPetCostumeCB(UserData ignored, UGCPickerData* pickerData, UGCAssetLibraryRow* row )
{
	int columnCostume = pickerData->fieldColumn;
	int columnPetCostume = pickerData->fieldColumn2;
	
	if( row && row->pcName ) {
		TokenStoreClearString( pickerData->pti, columnCostume, pickerData->data, pickerData->index, NULL );
		TokenStoreSetRef( pickerData->pti, columnPetCostume, pickerData->data, pickerData->index, row->pcName, NULL, NULL );
	}

	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddCostumePickerShowCB( UIButton* ignored, UGCPickerData* pickerData )
{
	int columnCostume = pickerData->fieldColumn;
	int columnPetCostume = pickerData->fieldColumn2;
	
	const char* strCostume = TokenStoreGetString( pickerData->pti, columnCostume, pickerData->data, 0, NULL );
	const char* strPetCostume = TokenStoreGetRefString( pickerData->pti, columnPetCostume, pickerData->data, 0, NULL );
		
	ugcAssetLibraryShowCostumePicker( pickerData, pickerData->allowProjectCostumes,
									  pickerData->strPickerTitle,
									  strCostume, strPetCostume,
									  ugcMEContextAddCostumePickerSetCostumeCB,
									  ugcMEContextAddCostumePickerSetContactCB,
									  ugcMEContextAddCostumePickerSetPetCostumeCB );
}

MEFieldContextEntry* ugcMEContextAddCostumePicker( bool allowProjectCostumes, const char* strDefaultText, const char* strPickerTitle, const char* strCostumeFieldName, const char* strPetCostumeFieldName, const char* strDisplayName, const char* strTooltip )
{
	MEFieldContext* ctx = MEContextGetCurrent();
	ParseTable* pti = MEContextGetParseTable();
	void* data = MEContextGetData();
	int columnCostume;
	int columnPetCostume;

	assert( pti && data );
	if( ParserFindColumn( pti, strCostumeFieldName, &columnCostume ) && ParserFindColumn( pti, strPetCostumeFieldName, &columnPetCostume )) {
		const char* strCostume = TokenStoreGetString( pti, columnCostume, data, 0, NULL );
		const char* strPetCostume = TokenStoreGetRefString( pti, columnPetCostume, data, 0, NULL );
		UGCPickerData* pickerData;
		
		pickerData = MEContextAllocMem( strCostumeFieldName, sizeof( *pickerData ), NULL, false );
		pickerData->data = data;
		pickerData->pti = pti;
		pickerData->fieldColumn = columnCostume;
		pickerData->fieldColumn2 = columnPetCostume;
		pickerData->index = -1; 
		pickerData->changedFn =  ctx->cbChanged;
		pickerData->changedData = ctx->pChangedData;

		pickerData->strPickerTitle = strPickerTitle;
		pickerData->allowProjectCostumes = allowProjectCostumes;

		if( !nullStr( strCostume )) {
			MEFieldContextEntry* entry = MEContextAddButton( ugcResourceGetDisplayName( "PlayerCostume", strCostume, strDefaultText ), NULL,
															 ugcMEContextAddCostumePickerShowCB, pickerData,
															 strCostumeFieldName, strDisplayName, strTooltip );
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_PickerButton", UI_WIDGET( ENTRY_BUTTON( entry ))->hOverrideSkin );
			return entry;
		} else {
			MEFieldContextEntry* entry = MEContextAddButton( ugcResourceGetDisplayName( "PetContactList", strPetCostume, strDefaultText ), NULL,
															 ugcMEContextAddCostumePickerShowCB, pickerData,
															 strCostumeFieldName, strDisplayName, strTooltip );
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_PickerButton", UI_WIDGET( ENTRY_BUTTON( entry ))->hOverrideSkin );
			return entry;
		}
	} else {
		assert( false );
	}
}

static void ugcMEContextAddCheckedAttribPickerSetCB(UserData ignored, UGCPickerData* pickerData, UGCAssetLibraryRow* row )
{
	UGCCheckedAttrib* attrib = TokenStoreGetPointer( pickerData->pti, pickerData->fieldColumn, pickerData->data, 0, NULL );
	if( !attrib ) {
		attrib = StructCreate( parse_UGCCheckedAttrib );
		TokenStoreSetPointer( pickerData->pti, pickerData->fieldColumn, pickerData->data, 0, attrib, NULL );
	}

	StructReset( parse_UGCCheckedAttrib, attrib );
	
	if( row->astrType == allocAddString( "UGCItem" )) {
		attrib->astrSkillName = NULL;
		attrib->astrItemName = allocAddString( row->pcName );
	} else if( row->astrType == allocAddString( "CheckedAttrib" )) {
		attrib->astrSkillName = allocAddString( row->pcName );
		attrib->astrItemName = NULL;
	}
	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddCheckedAttribPickerClearCB( UIButton* ignored, UGCPickerData* pickerData )
{
	UGCCheckedAttrib* attrib = TokenStoreGetPointer( pickerData->pti, pickerData->fieldColumn, pickerData->data, 0, NULL );
	if( attrib ) {
		StructDestroy( parse_UGCCheckedAttrib, attrib );
		TokenStoreSetPointer( pickerData->pti, pickerData->fieldColumn, pickerData->data, 0, NULL, NULL );
	}

	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddCheckedAttribPickerShowCB( UIButton* ignored, UGCPickerData* pickerData )
{
	ugcAssetLibraryShowCheckedAttribPicker( pickerData, true,
											((pickerData->attribFlags & UGCINPR_CHECKED_ATTRIB_SKILLS) != 0),
											((pickerData->attribFlags & UGCINPR_CHECKED_ATTRIB_ITEMS) != 0),
											pickerData->strPickerTitle,
											ugcMEContextAddCheckedAttribPickerSetCB );
}

MEFieldContextEntry* ugcMEContextAddCheckedAttribPicker( UGCInteractPropertiesFlags flags, const char* strDefaultText, const char* strPickerTitle, const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	MEFieldContext* ctx = MEContextGetCurrent();
	ParseTable* pti = MEContextGetParseTable();
	void* data = MEContextGetData();
	int column;

	assert( pti && data );
	if( ParserFindColumn( pti, strFieldName, &column )) {
		UGCCheckedAttrib* attrib = TokenStoreGetPointer( pti, column, data, 0, NULL );
		MEFieldContextEntry* entry;
		UGCPickerData* pickerData;
		char buffer[ 512 ];
		
		pickerData = MEContextAllocMem( strFieldName, sizeof( *pickerData ), NULL, false );
		pickerData->data = data;
		pickerData->pti = pti;
		pickerData->fieldColumn = column;
		pickerData->index = -1; 
		pickerData->changedFn =  ctx->cbChanged;
		pickerData->changedData = ctx->pChangedData;

		pickerData->strPickerTitle = strPickerTitle;
		pickerData->attribFlags = flags;

		if( !attrib ) {
			sprintf( buffer, "%s", strDefaultText );
		} else if( !nullStr( attrib->astrItemName )) {
			UGCItem *pItem = ugcEditorGetItemByName(attrib->astrItemName);
			sprintf( buffer, "%s", ugcResourceGetDisplayName( "MissionItem", pItem->strDisplayName, strDefaultText ));
		} else {
			sprintf( buffer, "%s", ugcResourceGetDisplayName( "CheckedAttrib", attrib->astrSkillName, strDefaultText ));
		}

		entry = MEContextAddButton( buffer, NULL,
								   ugcMEContextAddCheckedAttribPickerShowCB, pickerData,
								   strFieldName, strDisplayName, strTooltip );
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_PickerButton", UI_WIDGET( ENTRY_BUTTON( entry ))->hOverrideSkin );
		if( attrib ) {
			MEContextEntryAddActionButton( entry, NULL, "UGC_Icons_Labels_Delete", ugcMEContextAddCheckedAttribPickerClearCB, pickerData, ctx->iWidgetHeight, NULL );
		}
		return entry;
	} else {
		assert( false );
	}
}

MEFieldContextEntry* ugcMEContextAddCheckedAttribPickerMsg( UGCInteractPropertiesFlags flags, const char* strDefaultText, const char* strPickerTitle, const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	return ugcMEContextAddCheckedAttribPicker( flags, TranslateMessageKey( strDefaultText ), TranslateMessageKey( strPickerTitle ), strFieldName, TranslateMessageKey( strDisplayName ), TranslateMessageKey( strTooltip ));
}

static void ugcMEContextAddComponentPickerSetCB( const char* zmapName, const char* logicalName, const float* overworldPos, const char* overworldIcon, UGCPickerData* pickerData )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();

	assert( !overworldPos && !overworldIcon );
	if( nullStr( zmapName ) || nullStr( logicalName )) {
		return;
	}
	
	if( strchr( zmapName, ':' )) {
		// is in this project
		UGCComponent* component = ugcComponentFindByLogicalName( ugcProj->components, logicalName );
		if( component ) {
			TokenStoreSetInt( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, component->uID, NULL, NULL );
		}
	} else {
		// is external object -- create a new component to hold it
		UGCComponent* component = ugcComponentOpExternalObjectFindOrCreate( ugcProj, zmapName, logicalName );
		if( component ) {
			TokenStoreSetInt( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, component->uID, NULL, NULL );
		}
	}
	
	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static UGCZeniPickerFilterType ugcComponentEncounterObjectType( UGCComponentType type )
{
	switch( type ) {
		case UGC_COMPONENT_TYPE_KILL: return UGCZeniPickerType_Encounter;
		case UGC_COMPONENT_TYPE_CONTACT: return UGCZeniPickerType_Contact;
		case UGC_COMPONENT_TYPE_SPAWN: return UGCZeniPickerType_Spawn;
		case UGC_COMPONENT_TYPE_EXTERNAL_DOOR: return UGCZeniPickerType_Door;
		case UGC_COMPONENT_TYPE_OBJECT: return UGCZeniPickerType_Clickie;
		case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED: return UGCZeniPickerType_Clickie;
		case UGC_COMPONENT_TYPE_ROOM_DOOR: return UGCZeniPickerType_Door;
		case UGC_COMPONENT_TYPE_FAKE_DOOR: return UGCZeniPickerType_Door;
		case UGC_COMPONENT_TYPE_DESTRUCTIBLE: return UGCZeniPickerType_Destructible;
		case UGC_COMPONENT_TYPE_ROOM_MARKER: return UGCZeniPickerType_Volume;
		case UGC_COMPONENT_TYPE_PLANET: return UGCZeniPickerType_Volume;
		case UGC_COMPONENT_TYPE_WHOLE_MAP: return UGCZeniPickerType_Volume;
		case UGC_COMPONENT_TYPE_REWARD_BOX: return UGCZeniPickerType_Reward_Box;
		default: return UGCZeniPickerType_None;
	}
}

static void ugcMEContextAddComponentPickerShowCB( UIButton* ignored, UGCPickerData* pickerData )
{
	int componentID = TokenStoreGetInt( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, NULL );
	UGCComponent* component = ugcEditorFindComponentByID( componentID );

	if( !nullStr( pickerData->defaultMapName ) || component ) {
		if( !component || !component->sPlacement.bIsExternalPlacement ) {
			char logicalName[256];
			strcpy(logicalName, ugcComponentGetLogicalNameTemp( component ));
			if( !ugcZeniPickerShow( ugcMapEditorBuildEncounterInfos( pickerData->defaultMapName, pickerData->forceFilterType ),
									pickerData->forceFilterType,
									pickerData->defaultMapName, logicalName,
									ugcEditorEncObjFilter, NULL, ugcMEContextAddComponentPickerSetCB, pickerData )) {
				UGCMap* map = ugcEditorGetMapByName( pickerData->defaultMapName );
				if( map->pUnitializedMap ) {
					ugcModalDialogMsg( "UGC_Editor.NoComponents_IncompleteMap", "UGC_Editor.NoComponents_IncompleteMapDetails", UIOk );
				} else {
					ugcModalDialogMsg( "UGC_Editor.NoComponents", "UGC_Editor.NoComponentsDetails", UIOk );
				}
			}
		} else {
			char defaultMap[ RESOURCE_NAME_MAX_SIZE ];
			sprintf( defaultMap, "%s", component->sPlacement.pcExternalMapName ? component->sPlacement.pcExternalMapName : "" );

			if( !ugcZeniPickerShow( NULL,
									pickerData->forceFilterType,
									defaultMap, component->sPlacement.pcExternalObjectName,
									ugcEditorEncObjFilter, NULL, ugcMEContextAddComponentPickerSetCB, pickerData )) {
				ugcModalDialogMsg( "UGC_Editor.NoComponents_Cryptic", "UGC_Editor.NoComponents_CrypticDetails", UIOk );
			}
		}
	}
}

MEFieldContextEntry* ugcMEContextAddComponentPicker(const char* strDefaultText, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexEx( strDefaultText, NULL, UGCZeniPickerType_None, strFieldName, -1, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddComponentPickerIndex(const char* strDefaultText, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexEx( strDefaultText, NULL, UGCZeniPickerType_None, strFieldName, index, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddComponentPickerEx(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexEx( strDefaultText, strDefaultMapName, forceFilterType, strFieldName, -1, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddComponentPickerIndexEx(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip)
{
	MEFieldContext* ctx = MEContextGetCurrent(); 
	ParseTable* pti = MEContextGetParseTable();
	void* data = MEContextGetData();
	int column;

	assert( pti && data );
	if( ParserFindColumn( pti, strFieldName, &column )) {
		U32 componentID = TokenStoreGetInt( pti, column, data, index, NULL );
		UGCComponent* component = ugcEditorFindComponentByID( componentID );
		if( component || strDefaultMapName ) {
			char componentDisplayName[ 256 ];
			UGCPickerData* pickerData = MEContextAllocMemIndex( strFieldName, index, sizeof( *pickerData ), NULL, false );

			if( component ) {
				ugcComponentGetDisplayName( componentDisplayName, ugcEditorGetProjectData(), component, false );
			} else {
				strcpy( componentDisplayName, "" );
			}
			
			pickerData->data = data;
			pickerData->pti = pti;
			pickerData->fieldColumn = column;
			pickerData->index = index;
			pickerData->changedFn = ctx->cbChanged;
			pickerData->changedData = ctx->pChangedData;

			if( strDefaultMapName ) {
				pickerData->defaultMapName = strDefaultMapName;
			} else {
				pickerData->defaultMapName = component->sPlacement.pcMapName;
			}

			if (forceFilterType==UGCZeniPickerType_None)
			{
				pickerData->forceFilterType = ugcComponentEncounterObjectType( component->eType );
			}
			else
			{
				pickerData->forceFilterType = forceFilterType;
			}

			{
				MEFieldContextEntry* entry = MEContextAddButtonIndex(
						(!component || component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ? strDefaultText : componentDisplayName),
						NULL,
						ugcMEContextAddComponentPickerShowCB, pickerData,
						strFieldName, index, strDisplayName, strTooltip );
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_PickerButton", UI_WIDGET( ENTRY_BUTTON( entry ))->hOverrideSkin );
				return entry;
			}
		}

		return NULL;
	} else {
		assert( false );
		return NULL;
	}
}

MEFieldContextEntry* ugcMEContextAddComponentPickerMsg(const char* strDefaultText, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexExMsg( strDefaultText, NULL, UGCZeniPickerType_None, strFieldName, -1, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddComponentPickerIndexMsg(const char* strDefaultText, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexExMsg( strDefaultText, NULL, UGCZeniPickerType_None, strFieldName, index, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddComponentPickerExMsg(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexExMsg( strDefaultText, strDefaultMapName, forceFilterType, strFieldName, -1, strDisplayName, strTooltip );
}

MEFieldContextEntry* ugcMEContextAddComponentPickerIndexExMsg(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip)
{
	return ugcMEContextAddComponentPickerIndexEx( TranslateMessageKey( strDefaultText ), strDefaultMapName, forceFilterType, strFieldName, index, TranslateMessageKey( strDisplayName ), TranslateMessageKey( strTooltip ));
}

static void ugcMEContextAddLocationPickerSetCB( const char* zmapName, const char* logicalName, const float* overworldPos, const char* overworldIcon, UGCPickerData* pickerData )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	assert( !zmapName && !logicalName );

	if( overworldPos && overworldIcon ) {
		UGCMapLocation* mapLocation = TokenStoreGetPointer( pickerData->pti, pickerData->fieldColumn2, pickerData->data, pickerData->index, NULL );
		if( !mapLocation ) {
			mapLocation = StructCreate( parse_UGCMapLocation );
			TokenStoreSetPointer( pickerData->pti, pickerData->fieldColumn2, pickerData->data, pickerData->index, mapLocation, NULL );
		}
		mapLocation->positionX = overworldPos[ 0 ];
		mapLocation->positionY = overworldPos[ 1 ];
		mapLocation->astrIcon = allocAddString( overworldIcon );
	}
	
	pickerData->changedFn( NULL, true, pickerData->changedData );
}

static void ugcMEContextAddLocationPickerShowCB( UIButton* ignored, UGCPickerData* pickerData )
{
	Vec2 mapPosition = { SAFE_MEMBER( pickerData->defaultMapLocation, positionX ),
						 SAFE_MEMBER( pickerData->defaultMapLocation, positionY )};
	ResourceSearchResult* result;
	const char** iconNames = NULL;
	ResourceSearchRequest request = { 0 };
	
	request.eSearchMode = SEARCH_MODE_TAG_SEARCH;
	request.pcSearchDetails = "UGC, OverworldMapIcon,";
	request.pcName = NULL;
	request.pcType = "Texture";
	request.iRequest = 1;
	
	result = ugcResourceSearchRequest( &request );
	{
		int it;
		for( it = 0; it != eaSize( &result->eaRows ); ++it ) {
			eaPush( &iconNames, result->eaRows[ it ]->pcName );
		}
	}

	ugcZeniPickerOverworldMapShow( iconNames,
								   (pickerData->defaultMapLocation ? mapPosition : NULL),
								   SAFE_MEMBER( pickerData->defaultMapLocation, astrIcon ),
								   ugcMEContextAddLocationPickerSetCB, pickerData );
	StructDestroySafe( parse_ResourceSearchResult, &result );
	eaDestroy( &iconNames );
}

MEFieldContextEntry* ugcMEContextAddLocationPickerMsg(const char* strFieldName, const char* strDisplayName, const char* strTooltip)
{
	MEFieldContext* ctx = MEContextGetCurrent(); 
	ParseTable* pti = MEContextGetParseTable();
	void* data = MEContextGetData();
	int locationColumn;

	assert( pti && data );
	if( ParserFindColumn( pti, strFieldName, &locationColumn )) {
		UGCMapLocation* mapLocation = TokenStoreGetPointer( pti, locationColumn, data, -1, NULL );
		UGCPickerData* pickerData = MEContextAllocMemIndex( strFieldName, -1, sizeof( *pickerData ), NULL, false );
			
		pickerData->data = data;
		pickerData->pti = pti;
		pickerData->fieldColumn2 = locationColumn;
		pickerData->index = -1;
		pickerData->changedFn = ctx->cbChanged;
		pickerData->changedData = ctx->pChangedData;
		
		pickerData->defaultMapLocation = mapLocation;

		{
			MEFieldContextEntry* entry = MEContextAddButtonMsg(
					"UGC_Editor.ChooseLocation", NULL,
					ugcMEContextAddLocationPickerShowCB, pickerData,
					strFieldName, strDisplayName, strTooltip );
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_PickerButton", UI_WIDGET( ENTRY_BUTTON( entry ))->hOverrideSkin );
			return entry;
		}

		return NULL;
	} else {
		assert( false );
		return NULL;
	}
}

static void ugcWhenAddComponentCB( UIButton* ignored, UGCWhen* when )
{
	eaiPush( &when->eauComponentIDs, -1 );
	ugcEditorQueueApplyUpdate();
}

static void ugcWhenAddDialogCB( UIButton* ignored, UGCWhen* when )
{
	UGCWhenDialogPrompt* newPrompt = StructCreate( parse_UGCWhenDialogPrompt );
	eaPush( &when->eaDialogPrompts, newPrompt );
	ugcEditorQueueApplyUpdate();
}

static void ugcWhenAddObjectiveCB( UIButton* ignored, UGCWhen* when )
{
	eaiPush( &when->eauObjectiveIDs, -1 );
	ugcEditorQueueApplyUpdate();
}

static void ugcWhenSetDialogPromptCB( U32 dialogID, int promptID, UGCWhenAndIndex* data )
{
	UGCWhenDialogPrompt* dialogPrompt = eaGet( &data->when->eaDialogPrompts, data->index );
	if( dialogPrompt ) {
		dialogPrompt->uDialogID = dialogID;
		dialogPrompt->iPromptID = promptID;
		ugcEditorQueueApplyUpdate();
	}
}

static void ugcWhenSelectDialogPromptCB( UIButton* ignored, UGCWhenAndIndex* data )
{
	UGCWhenDialogPrompt* dialogPrompt = eaGet( &data->when->eaDialogPrompts, data->index );
	if( dialogPrompt ) {
		ugcShowDialogPromptPicker( ugcWhenSetDialogPromptCB, data, data->strMapName );
	}
}

static void ugcWhenClearComponentIndexCB( UIButton* ignored, UGCWhenAndIndex* data )
{
	eaiRemove( &data->when->eauComponentIDs, data->index );
	ugcEditorQueueApplyUpdate();
}

static void ugcWhenClearDialogPromptIndexCB( UIButton* ignored, UGCWhenAndIndex* data )
{
	StructDestroySafe( parse_UGCWhenDialogPrompt, &data->when->eaDialogPrompts[ data->index ]);
	eaRemove( &data->when->eaDialogPrompts, data->index );
	ugcEditorQueueApplyUpdate();
}

static void ugcWhenClearObjectiveIndexCB( UIButton* ignored, UGCWhenAndIndex* data )
{
	eaiRemove( &data->when->eauObjectiveIDs, data->index );
	ugcEditorQueueApplyUpdate();
}

static void ugcWhenAndIndexReset( UGCWhenAndIndex* data )
{
	SAFE_FREE( data->strMapName );
}

void ugcMEContextAddWhenPickerMsg( const char* mapName, UGCWhen* when, const char* ctxName, int whenIdx, bool hasObjective,
								   StaticDefineInt* enumModel, const char* label, const char* tooltip )
{
	MEFieldContext* whenUICtx;
	MEFieldContextEntry* entry;
	char errorField[ 256 ];
	int it;

	whenUICtx = MEContextPush( ctxName, when, when, parse_UGCWhen );
	sprintf( errorField, "When.%d.Type", whenIdx );
	entry = MEContextAddEnumMsg( kMEFieldType_Combo, enumModel, "Type", label, tooltip );
	MEContextSetEntryErrorForField( entry, errorField );

	whenUICtx->iYDataStart = 0;
	setVec2( whenUICtx->iErrorIconOffset, 0, 3 );
	whenUICtx->iYStep = UGC_ROW_HEIGHT;
	for( it = 0; it != ea32Size( &when->eauComponentIDs ); ++it ) {
		sprintf( errorField, "When.%d.ComponentID.%d", whenIdx, it );
		entry = ugcMEContextAddComponentPickerIndexExMsg( "UGC_Editor.When_Component_Default", mapName,
														  (when->eType == UGCWHEN_COMPONENT_REACHED ? UGCZeniPickerType_Usable_For_ComponentReached : UGCZeniPickerType_Usable_For_ComponentComplete),
													   "ComponentID", it, NULL, "UGC_Editor.When_Component.Tooltip" );
		MEContextSetEntryErrorForField( entry, errorField );
		if( ea32Size( &when->eauComponentIDs ) > 1 ) {
			UGCWhenAndIndex* data = MEContextAllocMemIndex( "ComponentIDDelete", it, sizeof( *data ), ugcWhenAndIndexReset, true );
			data->strMapName = strdup( mapName );
			data->when = when;
			data->index = it;
			MEContextEntryAddActionButtonMsg( entry, NULL, "UGC_Icons_Labels_Delete", ugcWhenClearComponentIndexCB, data, whenUICtx->iWidgetHeight, "UGC_Editor.When_Component_Delete.Tooltip" );
		}
	}
	for( it = 0; it != eaSize( &when->eaDialogPrompts ); ++it ) {
		UGCComponent* dialogPrompt = ugcEditorFindComponentByID( when->eaDialogPrompts[ it ]->uDialogID );
		UGCDialogTreePrompt* prompt = dialogPrompt ? ugcDialogTreeGetPrompt( &dialogPrompt->dialogBlock, when->eaDialogPrompts[ it ]->iPromptID ) : NULL;
		UGCWhenAndIndex* data = MEContextAllocMemIndex( "DialogPromptDelete", it, sizeof( *data ), ugcWhenAndIndexReset, true );
		data->strMapName = strdup( mapName );
		data->when = when;
		data->index = it;
		sprintf( errorField, "When.%d.ComponentID.%d", whenIdx, it );
		entry = MEContextAddButtonIndex( (prompt ? prompt->pcPromptBody : TranslateMessageKey( "UGC_Editor.When_DialogPrompt_Default" )),
										 NULL, ugcWhenSelectDialogPromptCB, data,
										 "DialogPrompt", it, NULL, TranslateMessageKey( "UGC_Editor.When_DialogPrompt.Tooltip" ));
		MEContextSetEntryErrorForField( entry, errorField );
		if( eaSize( &when->eaDialogPrompts ) > 1 ) {
			MEContextEntryAddActionButtonMsg( entry, NULL, "UGC_Icons_Labels_Delete", ugcWhenClearDialogPromptIndexCB, data, whenUICtx->iWidgetHeight, "UGC_Editor.When_DialogPrompt_Delete.Tooltip" );
		}
	}
	if( !hasObjective && !nullStr( mapName )) {
		UGCMapEditorDoc* mapDoc = ugcEditorGetMapDoc( mapName );
		if( mapDoc ) {
			MEFieldContext* noSortCtx = MEContextPush( "NoSort", when, when, parse_UGCWhen );
			noSortCtx->bDontSortComboEnums = true;
			for( it = 0; it != ea32Size( &when->eauObjectiveIDs ); ++it ) {
				sprintf( errorField, "When.%d.ObjectiveID.%d", whenIdx, it );
				entry = MEContextAddEnumIndex( kMEFieldType_Combo, mapDoc->beaObjectivesModel, "ObjectiveID", it, NULL, TranslateMessageKey( "UGC_Editor.When_Objective.Tooltip" ));
				MEContextSetEntryErrorForField( entry, errorField );
				ui_WidgetSetTextMessage( ENTRY_FIELD( entry )->pUIWidget, "UGC_Editor.When_Objective_Default" );
				if( ea32Size( &when->eauObjectiveIDs ) > 1 ) {
					UGCWhenAndIndex* data = MEContextAllocMemIndex( "ObjectiveIDDelete", it, sizeof( *data ), ugcWhenAndIndexReset, true );
					data->strMapName = strdup( mapName );
					data->when = when;
					data->index = it;
					MEContextEntryAddActionButtonMsg( entry, NULL, "UGC_Icons_Labels_Delete", ugcWhenClearObjectiveIndexCB, data, whenUICtx->iWidgetHeight, "UGC_Editor.When_Objective_Delete.Tooltip" );
				}
			}
			MEContextPop( "NoSort" );
		}
	}

	switch( when->eType ) {
		xcase UGCWHEN_COMPONENT_COMPLETE: case UGCWHEN_COMPONENT_REACHED:
			MEContextAddButtonMsg( "UGC_Editor.When_Component_Add", NULL, ugcWhenAddComponentCB, when, "AddButton", NULL, "UGC_Editor.When_Component_Add.Tooltip" );
		xcase UGCWHEN_DIALOG_PROMPT_REACHED:
			MEContextAddButtonMsg( "UGC_Editor.When_DialogPrompt_Add", NULL, ugcWhenAddDialogCB, when, "AddButton", NULL, "UGC_Editor.When_DialogPrompt_Add.Tooltip" );
		xcase UGCWHEN_OBJECTIVE_IN_PROGRESS: case UGCWHEN_OBJECTIVE_COMPLETE:
			if( !hasObjective ) {
				MEContextAddButtonMsg( "UGC_Editor.When_Objective_Add", NULL, ugcWhenAddObjectiveCB, when, "AddButton", NULL, "UGC_Editor.When_Objective_Add.Tooltip" );
			}
	}

	MEContextPop( ctxName );
}

static void ugcMEContextAddBooleanMsgSetCB( UIRadioButtonGroup* group, UGCPickerData* pickerData )
{
	UIRadioButton* activeButton = ui_RadioButtonGroupGetActive( group );
	bool newValue = activeButton->widget.u64;

	TokenStoreSetIntAuto( pickerData->pti, pickerData->fieldColumn, pickerData->data, pickerData->index, newValue, NULL, NULL );
	pickerData->changedFn( NULL, true, pickerData->changedData );
}

MEFieldContextEntry* ugcMEContextAddBooleanMsg( const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	MEFieldContext* ctx = MEContextGetCurrent();
	ParseTable* pti = MEContextGetParseTable();
	void* data = MEContextGetData();
	int column;

	assert( pti && data );
	if( ParserFindColumn( pti, strFieldName, &column )) {
		int value = TokenStoreGetIntAuto( pti, column, data, -1, NULL );
		MEFieldContextEntry* entry = MEContextAddCustom( strFieldName );
		UGCPickerData* pickerData = MEContextAllocMem( strFieldName, sizeof( *pickerData ), NULL, false );
		UILabel* label;
		UIRadioButton* trueButton;
		UIRadioButton* falseButton;

		pickerData->data = data;
		pickerData->pti = pti;
		pickerData->fieldColumn = column;
		pickerData->index = -1;
		pickerData->changedFn = ctx->cbChanged;
		pickerData->changedData = ctx->pChangedData;

		if( !ENTRY_LABEL( entry )) {
			ENTRY_LABEL( entry ) = ui_LabelCreate( "", 0, 0 );
		}
		if( !entry->pRadioGroup ) {
			entry->pRadioGroup = ui_RadioButtonGroupCreate();
		}
		if( !entry->pRadioButton1 ) {
			entry->pRadioButton1 = ui_RadioButtonCreate( 0, 0, "", entry->pRadioGroup );
		}
		if( !entry->pRadioButton2 ) {
			entry->pRadioButton2 = ui_RadioButtonCreate( 0, 0, "", entry->pRadioGroup );
		}

		label = ENTRY_LABEL( entry );
		trueButton = entry->pRadioButton1;
		falseButton = entry->pRadioButton2;

		ui_WidgetSetTextMessage( UI_WIDGET( label ), strDisplayName );
		ui_WidgetSetTooltipMessage( UI_WIDGET( label ), strTooltip );
		ui_WidgetSetPositionEx( UI_WIDGET( label ), ctx->iXPos + ctx->iXLabelStart, ctx->iYPos + ctx->iYLabelStart, ctx->fXPosPercentage, 0, UITopLeft );
		ui_LabelSetWidthNoAutosize( label, 1, UIUnitPercentage );
		ui_WidgetGroupMove( &ctx->pUIContainer->children, UI_WIDGET( label ));
		
		ui_WidgetSetTextMessage( UI_WIDGET( trueButton ), "UGC.True" );
		ui_WidgetSetTooltipMessage( UI_WIDGET( trueButton ), strTooltip );
		trueButton->widget.u64 = true;
		ui_WidgetSetPositionEx( UI_WIDGET( trueButton ), ctx->iXPos + ctx->iXDataStart, ctx->iYPos + ctx->iYDataStart, 0, 0, UITopLeft );
		ui_WidgetSetWidth( UI_WIDGET( trueButton ), 70 );
		ui_WidgetGroupMove( &ctx->pUIContainer->children, UI_WIDGET( trueButton ));

		ui_WidgetSetTextMessage( UI_WIDGET( falseButton ), "UGC.False" );
		ui_WidgetSetTooltipMessage( UI_WIDGET( falseButton ), strTooltip );
		falseButton->widget.u64 = false;
		ui_WidgetSetPositionEx( UI_WIDGET( falseButton ), ctx->iXPos + ctx->iXDataStart + 75, ctx->iYPos + ctx->iYDataStart, 0, 0, UITopLeft );
		ui_WidgetSetWidth( UI_WIDGET( falseButton ), 70 );
		ui_WidgetGroupMove( &ctx->pUIContainer->children, UI_WIDGET( falseButton ));

		ui_RadioButtonGroupSetActive( entry->pRadioGroup, value ? trueButton : falseButton );
		ui_RadioButtonGroupSetToggledCallback( entry->pRadioGroup, ugcMEContextAddBooleanMsgSetCB, pickerData );
		ctx->iYPos += ctx->iYStep;

		return entry;
	}

	return NULL;
}

MEFieldContextEntry* ugcMEContextAddMultilineText( const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry;
	F32 yStart = context->iYPos;
	
	entry = MEContextAddSimple( kMEFieldType_TextArea, strFieldName, strDisplayName, strTooltip );
	ui_TextAreaSetCollapse( ENTRY_FIELD( entry )->pUITextArea, true );
	ui_TextAreaSetCollapseHeight( ENTRY_FIELD( entry )->pUITextArea, STANDARD_ROW_HEIGHT * 2.5 - 2 );
	ui_TextAreaSetSMFEdit( ENTRY_FIELD( entry )->pUITextArea, ugcEditorGetDefaults()->eaTextShortcutTabs );
	ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_MULTI_LINE_MAX_LENGTH );
	ENTRY_FIELD( entry )->pUITextArea->trimWhitespace = true;
	ui_WidgetSetHeight( ENTRY_FIELD( entry )->pUIWidget, 200 );
	
	context->iYPos = yStart + STANDARD_ROW_HEIGHT * 2.5;

	return entry;
}

MEFieldContextEntry* ugcMEContextAddMultilineTextMsg( const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry;
	F32 yStart = context->iYPos;
	
	entry = MEContextAddSimpleMsg( kMEFieldType_TextArea, strFieldName, strDisplayName, strTooltip );
	ui_TextAreaSetCollapse( ENTRY_FIELD( entry )->pUITextArea, true );
	ui_TextAreaSetCollapseHeight( ENTRY_FIELD( entry )->pUITextArea, STANDARD_ROW_HEIGHT * 2.5 - 2 );
	ui_TextAreaSetSMFEdit( ENTRY_FIELD( entry )->pUITextArea, ugcEditorGetDefaults()->eaTextShortcutTabs );
	ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_MULTI_LINE_MAX_LENGTH );
	ENTRY_FIELD( entry )->pUITextArea->trimWhitespace = true;
	ui_WidgetSetHeight( ENTRY_FIELD( entry )->pUIWidget, 200 );
	
	context->iYPos = yStart + context->iYDataStart + STANDARD_ROW_HEIGHT * 2.5;

	return entry;
}

MEFieldContextEntry* ugcMEContextAddProjectEditorMultilineTextMsg( const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry;
	F32 yStart = context->iYPos;
	
	entry = MEContextAddSimpleMsg( kMEFieldType_TextArea, strFieldName, strDisplayName, strTooltip );
	ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_MULTI_LINE_MAX_LENGTH );
	ENTRY_FIELD( entry )->pUITextArea->trimWhitespace = true;
	ui_WidgetSetHeight( ENTRY_FIELD( entry )->pUIWidget, STANDARD_ROW_HEIGHT * 3.5 - 2 );
	
	context->iYPos = yStart + STANDARD_ROW_HEIGHT * 3.5;

	return entry;
}

MEFieldContextEntry* ugcMEContextAddStarRating( float rating, const char* strFieldName )
{
	return ugcMEContextAddStarRatingIndex( rating, strFieldName, -1 );
}

MEFieldContextEntry* ugcMEContextAddStarRatingIndex( float rating, const char* strFieldName, int index )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustomIndex( strFieldName, index );
	UGCUIStarRating* ratingWidget;
	
	if( !ENTRY_WIDGET( entry )) {
		ratingWidget = ugcui_StarRatingCreate();
		ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( ratingWidget ));
		ENTRY_WIDGET( entry ) = UI_WIDGET( ratingWidget );
	} else {
		ratingWidget = (UGCUIStarRating*)ENTRY_WIDGET( entry );
	}
	ui_WidgetRemoveFromGroup( UI_WIDGET( ratingWidget ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( ratingWidget ));

	ui_WidgetSetPositionEx( UI_WIDGET( ratingWidget ), context->iXPos + context->iXDataStart, context->iYPos, context->fXPosPercentage, 0, UITopLeft );
	ugcui_StarRatingSet( ratingWidget, rating );
	MEContextStepDown();

	return entry;
}

MEFieldContextEntry* ugcMEContextAddBarGraphMsg( int value, int max, const char* strFieldName, const char* strDisplayName, const char* strTooltip )
{
	char countBuffer[ 256 ];
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry;
	UIProgressBar* bar;

	sprintf( countBuffer, "%d", value );
	entry = MEContextAddTwoLabelsMsg( strFieldName, strDisplayName, NULL, strTooltip );
	ui_LabelSetText( ENTRY_LABEL2( entry ), countBuffer );
	UI_WIDGET( ENTRY_LABEL2( entry ))->x = 0;
	UI_WIDGET( ENTRY_LABEL2( entry ))->offsetFrom = UITopRight;
	ui_WidgetSetPaddingEx( UI_WIDGET( ENTRY_LABEL( entry )), 0, 0, 0, 0 );
	ui_WidgetSetPaddingEx( UI_WIDGET( ENTRY_LABEL2( entry )), 0, 0, 0, 0 );
	
	ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 50, UIUnitFixed );
	ui_LabelSetWidthNoAutosize( ENTRY_LABEL2( entry ), 30, UIUnitFixed );
	
	if( !ENTRY_WIDGET( entry )) {
		bar = ui_ProgressBarCreate( 0, 0, 1 );
		ENTRY_WIDGET( entry ) = UI_WIDGET( bar );
	} else {
		bar = (UIProgressBar*)ENTRY_WIDGET( entry );
	}
	ui_WidgetRemoveFromGroup( UI_WIDGET( bar ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( bar ));

	ui_WidgetSetPositionEx( UI_WIDGET( bar ), 0, context->iYPos, context->fXPosPercentage, 0, UITopLeft );
	ui_WidgetSetWidthEx( UI_WIDGET( bar ), 1 - context->fXPosPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( bar ), 60, 34, 0, 0 );
	if( max == 0 ) {
		ui_ProgressBarSet( bar, 0 );
	} else {
		ui_ProgressBarSet( bar, (float)value / max );
	}
	MEContextStepDown();

	return entry;
}

MEFieldContextEntry* ugcMEContextAddList( UIModel peaModel, int rowHeight, UICellDrawFunc drawFn, UserData drawData, const char* strName )
{
	MEFieldContextEntry* entry = MEContextAddCustom( strName );
	UIList* list;

	if( !ENTRY_WIDGET( entry )) {
		list = ui_ListCreate( NULL, NULL, 1 );
		ENTRY_WIDGET( entry ) = UI_WIDGET( list );
	} else {
		list = (UIList*)ENTRY_WIDGET( entry );
	}
	ui_WidgetRemoveFromGroup( UI_WIDGET( list ));
	ui_WidgetAddChild( MEContextGetCurrent()->pUIContainer, UI_WIDGET( list ));

	list->fHeaderHeight = 0;
	list->fRowHeight = rowHeight;
	ui_ListSetModel( list, NULL, peaModel );
	ui_ListDestroyColumns( list );
	ui_ListAppendColumn( list, ui_ListColumnCreateCallback( "", drawFn, drawData ));

	return entry;
}

MEFieldContextEntry* ugcMEContextAddErrorSpriteForEditor( UGCEditorType editor, const char* strName )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustom( strName );
	UISprite** ppEntrySprite = (UISprite**)&ENTRY_WIDGET( entry );
	ugcErrorButtonRefreshForEditor( ppEntrySprite, ugcEditorGetRuntimeStatus(), editor );

	UI_WIDGET( *ppEntrySprite )->priority = 128;
	ui_WidgetRemoveFromGroup( UI_WIDGET( *ppEntrySprite ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( *ppEntrySprite ));
	
	return entry;
}

MEFieldContextEntry* ugcMEContextAddErrorSpriteForMissionMapTransition( const char* strNextObjectiveName )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustom( strNextObjectiveName );
	UISprite** ppEntrySprite = (UISprite**)&ENTRY_WIDGET( entry );
	ugcErrorButtonRefreshForMissionMapTransition( ppEntrySprite, ugcEditorGetRuntimeStatus(), strNextObjectiveName );

	UI_WIDGET( *ppEntrySprite )->priority = 128;
	ui_WidgetRemoveFromGroup( UI_WIDGET( *ppEntrySprite ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( *ppEntrySprite ));
	
	return entry;
}

MEFieldContextEntry* ugcMEContextAddErrorSpriteForMissionMap( const char* strNextMapName, const char* strNextObjectiveName )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustom( strNextObjectiveName );
	UISprite** ppEntrySprite = (UISprite**)&ENTRY_WIDGET( entry ); 
	ugcErrorButtonRefreshForMissionMap( ppEntrySprite, ugcEditorGetRuntimeStatus(), strNextMapName, strNextObjectiveName );

	UI_WIDGET( *ppEntrySprite )->priority = 128;
	ui_WidgetRemoveFromGroup( UI_WIDGET( *ppEntrySprite ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( *ppEntrySprite ));
	
	return entry;
}

MEFieldContextEntry* ugcMEContextAddErrorSpriteForMissionObjective( const char* strObjectiveName )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustom( strObjectiveName );
	UISprite** ppEntrySprite = (UISprite**)&ENTRY_WIDGET( entry ); 
	ugcErrorButtonRefreshForMissionObjective( ppEntrySprite, ugcEditorGetRuntimeStatus(), strObjectiveName );

	UI_WIDGET( *ppEntrySprite )->priority = 128;
	ui_WidgetRemoveFromGroup( UI_WIDGET( *ppEntrySprite ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( *ppEntrySprite ));
	
	return entry;
}

MEFieldContextEntry* ugcMEContextAddErrorSpriteForMapComponent( const char* strComponentName )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustom( strComponentName );
	UISprite** ppEntrySprite = (UISprite**)&ENTRY_WIDGET( entry ); 
	ugcErrorButtonRefreshForMapComponent( ppEntrySprite, ugcEditorGetRuntimeStatus(), strComponentName );

	UI_WIDGET( *ppEntrySprite )->priority = 128;
	ui_WidgetRemoveFromGroup( UI_WIDGET( *ppEntrySprite ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( *ppEntrySprite ));
	
	return entry;
}

MEFieldContextEntry* ugcMEContextAddErrorSpriteForDialogTreePrompt( const char* strDialogTreeName, U32 promptID )
{
	MEFieldContext* context = MEContextGetCurrent();
	MEFieldContextEntry* entry = MEContextAddCustom( strDialogTreeName );
	UISprite** ppEntrySprite = (UISprite**)&ENTRY_WIDGET( entry ); 
	ugcErrorButtonRefreshForDialogTreePrompt( ppEntrySprite, ugcEditorGetRuntimeStatus(), strDialogTreeName, promptID );

	UI_WIDGET( *ppEntrySprite )->priority = 128;
	ui_WidgetRemoveFromGroup( UI_WIDGET( *ppEntrySprite ));
	ui_WidgetAddChild( context->pUIContainer, UI_WIDGET( *ppEntrySprite ));
	
	return entry;
}

UIPane* ugcMEContextPushPaneParentWithHeader( const char* pchUID, const char* pchHeaderUID, const char* headerName, bool bWhiteBackground )
{
	MEFieldContext* pParentContext = MEContextGetCurrent();
	const char* headerTexas;
	const char* headerBoxTexas;
	UIPane* pane;
	UIPane* headerPane;
	MEFieldContextEntry* entry;
	UIWidget* widget;

	if( bWhiteBackground ) {
		headerTexas = "UGC_Details_Header_Box_Cover";
		headerBoxTexas = "UGC_Details_Header_Box";
	} else {
		headerTexas = "UGC_Pane_Light_Header_Box_Cover";
		headerBoxTexas = "UGC_Pane_Light_Header_Box";
	}

	pane = MEContextPushPaneParent( pchUID );
	ui_PaneSetStyle( pane, headerBoxTexas, true, false );

	// Place the header widget
	headerPane = MEContextPushPaneParent( pchHeaderUID );
	ui_PaneSetStyle( headerPane, headerTexas, true, false );
	{
		UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", headerTexas );
		
		entry = MEContextAddLabel( "Header", headerName, NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
		ui_WidgetSetPosition( widget, 0, 0 );
		ui_LabelResize( ENTRY_LABEL( entry ));
		
		ui_WidgetSetDimensions( UI_WIDGET( headerPane ),
								widget->width + ui_TextureAssemblyWidth( texas ),
								widget->height + ui_TextureAssemblyHeight( texas ));
	}
	MEContextPop( pchHeaderUID );
	ui_WidgetSetPositionEx( UI_WIDGET( headerPane ), 0, pParentContext->iYPos, 0, 0, UITop );
	ui_WidgetGroupMove( &pParentContext->pUIContainer->children, UI_WIDGET( headerPane ));

	UI_WIDGET( pane )->y += UI_WIDGET( headerPane )->height / 2;
	MEContextGetCurrent()->iYPos = UI_WIDGET( headerPane )->height / 2;
	UI_WIDGET( pane )->uClickThrough = true;
	UI_WIDGET( headerPane )->uClickThrough = true;

	return pane;
}

UIPane* ugcMEContextPushPaneParentWithHeaderMsg( const char* pchUID, const char* pchHeaderUID, const char* headerName, bool bWhiteBackground )
{
	return ugcMEContextPushPaneParentWithHeader( pchUID, pchHeaderUID, TranslateMessageKey( headerName ), bWhiteBackground );
}

UIPane* ugcMEContextPushPaneParentWithBooleanCheckMsg( const char* pchUID, const char* pchHeaderUID, const char* headerName, bool bState, bool bActive, UIActivationFunc toggledCB, UserData toggledData )
{
	UIPane* headerPane;
	UITextureAssembly* headerTexas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box_Cover" );
	MEFieldContextEntry* entry;
	MEFieldContext* pParentContext = MEContextGetCurrent();
	
	UIPane* pane = MEContextPushPaneParent( pchUID );
	ui_PaneSetStyle( pane, "UGC_Details_Header_Box", true, false );

	// Place the header widget
	headerPane = MEContextPushPaneParent( pchHeaderUID );
	ui_PaneSetStyle( headerPane, headerTexas->pchName, true, false );
	{
		// Unlike normal UI refresh, do the layout after setting up
		// the widgets.  This is because the layout of these two
		// widgets is tied to each others height.
		UILabel* label;
		UICheckButton* check;
		float maxHeight;
		
		entry = MEContextAddLabelMsg( "HeaderLabel", headerName, NULL );
		label = ENTRY_LABEL( entry );
		UI_SET_STYLE_FONT_NAME( UI_WIDGET( label )->hOverrideFont, "UGC_Header_Large" );
		ui_LabelResize( label );

		entry = MEContextAddCustom( "HeaderCheck" );
		if( !ENTRY_WIDGET( entry )) {
			ENTRY_WIDGET( entry ) = UI_WIDGET( ui_CheckButtonCreate( 0, 0, "", false ));
		}
		check = (UICheckButton*)ENTRY_WIDGET( entry );
		ui_CheckButtonSetState( check, bState );
		ui_SetActive( UI_WIDGET( check ), bActive );
		ui_CheckButtonSetToggledCallback( check, toggledCB, toggledData );
		ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, UI_WIDGET( check ));

		maxHeight = MAX( UI_WIDGET( label )->height, UI_WIDGET( check )->height );

		// Okay, now layout
		ui_WidgetSetPosition( UI_WIDGET( label ), 0,
							  (maxHeight - UI_WIDGET( label )->height) / 2 );
		ui_WidgetSetPosition( UI_WIDGET( check ), UI_WIDGET( label )->width + 5,
							  (maxHeight - UI_WIDGET( check )->height) / 2 );
		
		ui_WidgetSetDimensions( UI_WIDGET( headerPane ),
								ui_WidgetGetNextX( UI_WIDGET( check )) + ui_TextureAssemblyWidth( headerTexas ),
								maxHeight + ui_TextureAssemblyHeight( headerTexas ));
	}

	MEContextPop( pchHeaderUID );
	ui_WidgetSetPositionEx( UI_WIDGET( headerPane ), 0, pParentContext->iYPos, 0, 0, UITop );
	ui_WidgetGroupMove( &pParentContext->pUIContainer->children, UI_WIDGET( headerPane ));

	UI_WIDGET( pane )->y += UI_WIDGET( headerPane )->height / 2;
	MEContextGetCurrent()->iYPos = UI_WIDGET( headerPane )->height / 2;
	UI_WIDGET( pane )->uClickThrough = true;
	UI_WIDGET( headerPane )->uClickThrough = true;

	return pane;
}

void ugcRefreshColorCombo(UIColorCombo **ppCombo, int x, int y, UIColorSet *pColorSet, U8 vColor[4], UserData pUserData, UIWidget *pParent, UIActivationFunc changeCallback, UIColorHoverFunc hoverCallback)
{
	Vec4 v4Color;

	copyVec4(vColor, v4Color);

	if (!*ppCombo) {
		*ppCombo = ui_ColorComboCreate(x, y, pColorSet, v4Color);
		ui_WidgetSetDimensions(UI_WIDGET(*ppCombo), 20, 20);
	} else {
		ui_ColorComboSetColor(*ppCombo, v4Color);
		ui_ColorComboSetColorSet(*ppCombo, pColorSet);
		ui_WidgetRemoveFromGroup(UI_WIDGET(*ppCombo));
	}

	ui_WidgetSetPositionEx(UI_WIDGET(*ppCombo), x, y, 0, 0, UITopLeft);
	ui_ColorComboSetChangedCallback(*ppCombo, changeCallback, pUserData);
	ui_ColorComboSetHoverCallback(*ppCombo, hoverCallback, pUserData);
	ui_WidgetAddChild(pParent, UI_WIDGET(*ppCombo));
}


#define UGC_ATTRIB_TYPE_EVERYBODY		allocAddString("Everybody")
#define UGC_ATTRIB_TYPE_ITEM			allocAddString("Players with item")
#define UGC_ATTRIB_TYPE_SKILL			allocAddString("Players with skill")
#define UGC_ATTRIB_TYPE_SKILL_NOT		allocAddString("Players without skill")

void ugcRefreshInteractProperties(UGCInteractProperties *properties, UGCMapType map_type, UGCInteractPropertiesFlags flags)
{
	MEFieldContext* pContext = MEContextPush("InteractProperties", properties, properties, parse_UGCInteractProperties);
	UGCInteractPropertiesGroup* pGroup;
	MEFieldContextEntry* entry;

	pGroup = MEContextAllocMem("Properties", sizeof(UGCInteractPropertiesGroup), NULL, true);
	pGroup->pStruct = properties;
	pGroup->eMapType = map_type;

	//// General interaction properties
	if( flags & UGCINPR_BASIC ) {
		entry = MEContextAddTextMsg( false, NULL, "InteractText", "UGC_MapEditor.InteractText", "UGC_MapEditor.InteractText.Tooltip" );
		ui_EditableSetDefaultString( ENTRY_FIELD( entry )->pUIEditable, ugcInteractPropsInteractText( NULL ));
	}

	if( flags & UGCINPR_CUSTOM_ANIM_AND_DURATION ) {
		ugcMEContextAddResourcePickerMsg( "AIAnimList", "UGC_MapEditor.Animation_Default", "UGC_MapEditor.Animation_PickerTitle", true,
										  "InteractAnim", "UGC_MapEditor.Animation", "UGC_MapEditor.Animation.Tooltip" );
		MEContextAddEnumMsg( kMEFieldType_Combo, UGCInteractDurationEnum, "InteractDuration", "UGC_MapEditor.Duration", "UGC_MapEditor.Duration.Tooltip" );
	}

	if( (flags & UGCINPR_CHECKED_ATTRIB_ITEMS) && ugcIsMissionItemsEnabled() ) {
		ugcMEContextAddResourcePickerMsg( "MissionItem", "UGC_MapEditor.DropItem_Default", "UGC_MapEditor.DropItem_PickerTitle", true,
										  "DropItem", "UGC_MapEditor.DropItem", "UGC_MapEditor.DropItem.Tooltip" );
	}

	/// Succeed condition
	if( ugcDefaultsIsItemEditorEnabled() && (flags & (UGCINPR_CHECKED_ATTRIB_SKILLS | UGCINPR_CHECKED_ATTRIB_ITEMS)) ) {
		ugcMEContextAddCheckedAttribPickerMsg( flags & (UGCINPR_CHECKED_ATTRIB_SKILLS | UGCINPR_CHECKED_ATTRIB_ITEMS),
											   "UGC_MapEditor.RequiredItem_Default", "UGC_MapEditor.RequiredItem_PickerTitle",
											   "SucceedCheckedAttrib", "UGC_MapEditor.RequiredItem", "UGC_MapEditor.RequiredItem.Tooltip" );
		
		entry = MEContextAddSimpleMsg( kMEFieldType_TextEntry, "InteractFailureText", "UGC_MapEditor.FailureText", "UGC_MapEditor.FailureText.Tooltip" );

		entry = ugcMEContextAddBooleanMsg( "InteractTakesItem", "UGC_MapEditor.ConsumeItem", "UGC_MapEditor.ConsumeItem.Tooltip" );
	}

	MEContextPop("InteractProperties");
}

void ugcEditorPlacePropertiesWidgetsForBoxes( UIPane* propertiesPane, UISprite* propertiesSprite, CBox* screenBox, CBox* selectedBox, bool paneIsDocked, bool forAnimateToCenter )
{
	AtlasTex* spriteTex = atlasFindTexture( "UGC_Kits_Details_Arrow" );
	float paneWidth = 300;
	float paneHeight = MIN( CBoxHeight( screenBox ) - 100, 620 );
	float spriteY = CLAMP( (selectedBox->ly + selectedBox->hy) / 2,
						   screenBox->ly + 5, screenBox->hy - 5 ) - spriteTex->height / 2;
	float paneY = CLAMP( (screenBox->ly + screenBox->hy - paneHeight) / 2,
						 spriteY + spriteTex->height - paneHeight, spriteY );

	ui_PaneSetStyle( propertiesPane, "UGC_Details_Popup_Window_Vertical", true, false );
	ui_WidgetSetDimensions( UI_WIDGET( propertiesPane ), paneWidth, paneHeight );

	ui_SpriteSetTexture( propertiesSprite, spriteTex->name );
	ui_SpriteResize( propertiesSprite );

	if( paneIsDocked ) {
		propertiesSprite->bFlipX = false;
		ui_WidgetSetPosition( UI_WIDGET( propertiesSprite ),
							  MAX( selectedBox->lx - spriteTex->width, screenBox->lx + 5 ),
							  spriteY );
		ui_WidgetSetPosition( UI_WIDGET( propertiesPane ),
							  screenBox->lx,
							  (screenBox->ly + screenBox->hy - paneHeight) / 2 );
	} else {
		if( forAnimateToCenter ) {
			propertiesSprite->bFlipX = true;
			ui_WidgetSetPosition( UI_WIDGET( propertiesSprite ),
								  MIN( selectedBox->hx, screenBox->hx - 5 ),
								  spriteY );
			ui_WidgetSetPosition( UI_WIDGET( propertiesPane ),
								  ui_WidgetGetNextX( UI_WIDGET( propertiesSprite )),
								  (screenBox->ly + screenBox->hy - paneHeight) / 2 );
		} else if( selectedBox->lx > (screenBox->lx + screenBox->hx) / 2 ) {
			propertiesSprite->bFlipX = false;
			ui_WidgetSetPosition( UI_WIDGET( propertiesSprite ),
								  MAX( selectedBox->lx - spriteTex->width, screenBox->lx + 5 ),
								  spriteY );
			ui_WidgetSetPosition( UI_WIDGET( propertiesPane ),
								  UI_WIDGET( propertiesSprite )->x - paneWidth,
								  paneY );
		} else {
			propertiesSprite->bFlipX = true;
			ui_WidgetSetPosition( UI_WIDGET( propertiesSprite ),
								  MIN( selectedBox->hx, screenBox->hx - 5 ),
								  spriteY );
			ui_WidgetSetPosition( UI_WIDGET( propertiesPane ),
								  ui_WidgetGetNextX( UI_WIDGET( propertiesSprite )),
								  paneY );
		}
	}
}

//// Project data cache

void ugcEditorGetCachedProjectData(UGCProjectReviews** out_pReviews, int* out_pPageNumber, UGCProjectStatusQueryInfo** out_pStatus)
{
	*out_pReviews = CONTAINER_RECONST( UGCProjectReviews, g_UGCEditorDoc->pCachedRecentReviews );
	*out_pPageNumber = g_UGCEditorDoc->iCachedRecentReviewsPageNumber;
	*out_pStatus = g_UGCEditorDoc->pCachedStatus;
}

//// Undo / Update handling

void ugcEditorErrorHandled(const UGCRuntimeError *error) {
	if( g_UGCEditorDoc ) {
		eaFindAndRemoveFast(&g_UGCEditorDoc->runtime_status_unhandled_errors, error);
	}
}

void ugcEditorUpdateUIEx( void )
{
	static bool sNoRecurse = false;
	U32 startMs = timerCpuMs();
	if (sNoRecurse)
		return;
	sNoRecurse = true;
	sIgnoreChanges = true;

	ugcEditorUpdateTrivia();
	
	// Make sure all editors are open
	if( g_UGCEditorDoc->data ) {
		ugcEditorGetOrCreateProjectDoc();

		ugcEditorLoadAndGetMission();

		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->maps, UGCMap, map)
		{
			ugcEditorGetOrCreateMapDoc(map->pcName);
		}
		FOR_EACH_END;
		ugcEditorGetOrCreateNoMapsDoc();

		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->costumes, UGCCostume, costume)
		{
			ugcEditorGetOrCreateCostumeDoc(costume->astrName);
		}
		FOR_EACH_END;
		ugcEditorGetOrCreateNoCostumesDoc();

		ugcEditorGetOrCreateItemDoc();

		UGC_FOR_EACH_COMPONENT_OF_TYPE(g_UGCEditorDoc->data->components, UGC_COMPONENT_TYPE_DIALOG_TREE, component)
		{
			ugcEditorGetOrCreateDialogTreeDoc( component->uID );
		}
		UGC_FOR_EACH_COMPONENT_END;
		ugcEditorGetOrCreateNoDialogTreesDoc();
	}

	// Dump undo stack
	EditUndoPrintToBuffer(g_UGCEditorDoc->edit_undo_stack, SAFESTR(ugcRecentOpsForMiniDump));
	
	// redo validation
	ugcLoadStart_printf( "Validation..." );
	if( g_UGCEditorDoc->data ) {
		StructReset( parse_UGCRuntimeStatus, g_UGCEditorDoc->runtime_status );
		ugcSetStageAndAdd( g_UGCEditorDoc->runtime_status, "UGC Validate" );
		ugcValidateProject( g_UGCEditorDoc->data );
		ugcClearStage();
	}

	// ALL ERRORS SHOULD BE GENERATED NOW
	{
		int it;

		eaSetSize( &g_UGCEditorDoc->runtime_status_all_errors, 0 );
		eaSetSize( &g_UGCEditorDoc->runtime_status_unhandled_errors, 0 );
		for( it = 0; it != eaSize( &g_UGCEditorDoc->runtime_status->stages ); ++it ) {
			eaPushEArray( &g_UGCEditorDoc->runtime_status_all_errors, &g_UGCEditorDoc->runtime_status->stages[ it ]->errors );
			eaPushEArray( &g_UGCEditorDoc->runtime_status_unhandled_errors, &g_UGCEditorDoc->runtime_status->stages[ it ]->errors );
		}

		// Create the error model, sort -- then insert header rows
		eaSetSizeStruct( &g_UGCEditorDoc->runtime_status_sorted_for_model, parse_UGCRuntimeErrorOrHeader, 0 );
		for( it = 0; it != eaSize( &g_UGCEditorDoc->runtime_status_all_errors ); ++it ) {
			UGCRuntimeErrorOrHeader* modelRow = StructCreate( parse_UGCRuntimeErrorOrHeader );
			modelRow->error = g_UGCEditorDoc->runtime_status_all_errors[ it ];
			eaPush( &g_UGCEditorDoc->runtime_status_sorted_for_model, modelRow );
		}
		eaStableSort( g_UGCEditorDoc->runtime_status_sorted_for_model, NULL, ugcEditorSortErrorOrHeader );
		{
			// Insert header rows between each change
			UGCEditorType prevEditorType = -1;
			char* prevEditorName = 0;
			for( it = 0; it != eaSize( &g_UGCEditorDoc->runtime_status_sorted_for_model ); ++it ) {
				UGCRuntimeError* error = g_UGCEditorDoc->runtime_status_sorted_for_model[ it ]->error;
				UGCEditorType editorType;
				char* editorName;
				errorGetUGCField( error, &editorType, &editorName, NULL, NULL, NULL, NULL );

				if( editorType != prevEditorType || editorName != prevEditorName ) {
					UGCRuntimeErrorOrHeader* row = StructCreate( parse_UGCRuntimeErrorOrHeader );
					char* estr = NULL;
					ugcEditorErrorEditorName( error, &estr );
					row->strHeaderEditorName = StructAllocString( estr );
					eaInsert( &g_UGCEditorDoc->runtime_status_sorted_for_model, row, it );
					++it;
					estrDestroy( &estr );
				}
				prevEditorType = editorType;
				prevEditorName = editorName;
			}
		}
	}

	ugcLoadEnd_printf( "done. (%d errors)", eaSize( &g_UGCEditorDoc->runtime_status_all_errors ));

	ugcLoadStart_printf( "UI Refresh..." );

	// Refresh the backlink table
	ugcBacklinkTableRefresh( g_UGCEditorDoc->data, &g_UGCEditorDoc->pBacklinkTable );

	ugcLoadStart_printf( "Project Editor..." );
	if (g_UGCEditorDoc->project_editor) {
		ugcProjectEditor_Refresh(g_UGCEditorDoc->project_editor);
	}
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Maps..." );
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, map_editor)
	{
		UGCMap* map = ugcEditorGetMapByName( map_editor->doc_name );
		if( !map ) {
			ugcMapEditorFreeDoc(map_editor);
			eaRemove( &g_UGCEditorDoc->map_editors, FOR_EACH_IDX( g_UGCEditorDoc->map_editors, map_editor ));
		} else {
			ugcMapEditorDocRefresh(map_editor);
		}
	}
	FOR_EACH_END;
	if( g_UGCEditorDoc->no_maps_editor ) {
		ugcNoMapsEditorDocRefresh( g_UGCEditorDoc->no_maps_editor );
	}
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Mission..." );
	if( g_UGCEditorDoc->mission_editor )
	{
		ugcMissionDocRefresh(g_UGCEditorDoc->mission_editor );
	}
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Costume..." );
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, costume_editor)
	{
		UGCCostume* costume = ugcEditorGetCostumeByName( costume_editor->astrName );
		if( !costume ) {
			ugcCostumeEditor_Close(costume_editor);
			eaRemove( &g_UGCEditorDoc->costume_editors, FOR_EACH_IDX( g_UGCEditorDoc->costume_editors, costume_editor ));
		} else {
			ugcCostumeEditor_UpdateUI(costume_editor);
		}
	}
	FOR_EACH_END;
	if( g_UGCEditorDoc->no_costumes_editor ) {
		ugcNoCostumesEditor_UpdateUI( g_UGCEditorDoc->no_costumes_editor );
	}
	if( g_UGCEditorDoc->no_dialogs_editor ) {
		ugcNoDialogTreesDocRefresh( g_UGCEditorDoc->no_dialogs_editor );
	}
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Items..." );
	if( g_UGCEditorDoc->item_editor ) {
		ugcItemEditorRefresh(g_UGCEditorDoc->item_editor);
	}
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Dialogs..." );
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->dialog_editors, UGCDialogTreeDoc, dialog_editor)
	{
		if( ugcDialogTreeDocGetBlock( dialog_editor, NULL )) {
			ugcDialogTreeDocRefresh(dialog_editor);
		} else {
			int index = FOR_EACH_IDX( g_UGCEditorDoc->dialog_editors, dialog_editor );
			void* activeDoc = ugcDialogTreeDocGetIDAsPtr( dialog_editor );
			ugcDialogTreeDocDestroy( dialog_editor );
			eaRemove( &g_UGCEditorDoc->dialog_editors, index );
			if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_DIALOG_TREE && g_UGCEditorDoc->activeView.name == activeDoc ) {
				UGCDialogTreeDoc* prevDoc = eaGet( &g_UGCEditorDoc->dialog_editors, index - 1 );
				UGCDialogTreeDoc* nextDoc = eaGet( &g_UGCEditorDoc->dialog_editors, index );

				if( nextDoc ) {
					ugcEditorSetActiveDoc( UGC_VIEW_DIALOG_TREE, ugcDialogTreeDocGetIDAsPtr( nextDoc ));
				} else if( prevDoc ) {
					ugcEditorSetActiveDoc( UGC_VIEW_DIALOG_TREE, ugcDialogTreeDocGetIDAsPtr( prevDoc ));
				} else {
					ugcEditorSetActiveDoc( UGC_VIEW_NO_DIALOG_TREES, NULL );
				}
			}
		}
	}
	FOR_EACH_END;
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Playing Editor..." );
	ugcPlayingEditorRefresh();
	ugcLoadEnd_printf( "done." );

	ugcLoadStart_printf( "Misc..." );
	ugcEditorRefreshGlobalUI();
	if( g_UGCEditorDoc->errors_window_tree ) {
		ui_TreeRefresh( g_UGCEditorDoc->errors_window_tree );
	}
	ugcLoadEnd_printf( "done." );
	sNoRecurse = false;

	if (g_UGCEditorDoc->data)
	{
		UGCMapEditorDoc *map_doc = ugcEditorGetActiveMapDoc();
		UGCComponent *component = NULL;
		if (map_doc && eaiSize(&map_doc->selected_components) == 1)
			component = ugcEditorFindComponentByID(map_doc->selected_components[0]);

		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->runtime_status_unhandled_errors, UGCRuntimeError, error)
		{
			// Remove all unhandled errors relating to components or items that are not selected
			if (error->context->scope == UGC_SCOPE_CHALLENGE &&
				(!component || stricmp(error->context->challenge_name, ugcComponentGetLogicalNameTemp( component )) != 0))
			{
				eaRemove(&g_UGCEditorDoc->runtime_status_unhandled_errors, FOR_EACH_IDX(g_UGCEditorDoc->runtime_status_unhandled_errors, error));
			}
			// Remove all unhandled errors relating to maps that are not selected
			else if (error->context->scope == UGC_SCOPE_MAP &&
				(!map_doc || component || !resNamespaceBaseNameEq(error->context->map_name, map_doc->map_data->pcName)))
			{
				eaRemove(&g_UGCEditorDoc->runtime_status_unhandled_errors, FOR_EACH_IDX(g_UGCEditorDoc->runtime_status_unhandled_errors, error));
			}
			else if (error->context->scope == UGC_SCOPE_INTERNAL_CODE)
			{
				eaRemove(&g_UGCEditorDoc->runtime_status_unhandled_errors, FOR_EACH_IDX(g_UGCEditorDoc->runtime_status_unhandled_errors, error));
			}
			// Specific errors that we don't expect to show in the UI
			else if (stricmp(error->message_key, "UGC.Mission_NoObjectives") == 0
					 || stricmp(error->message_key, "UGC.MissionEndsOnInternal") == 0
					 || stricmp(error->message_key, "UGC.Room_Colliding") == 0
					 || stricmp( error->message_key, "UGC.Project_TooManyDialogTreePrompts" ) == 0 )
			{
				eaRemove(&g_UGCEditorDoc->runtime_status_unhandled_errors, FOR_EACH_IDX(g_UGCEditorDoc->runtime_status_unhandled_errors, error));
			}
		}
		FOR_EACH_END;

		// Check to make sure all errors have been handled
		if( eaSize( &g_UGCEditorDoc->runtime_status_unhandled_errors ) > 0 ) {
			if( gDisableUnhandledErrorCheck ) {
				Errorf( "UGC error has not been displayed in the UI.  Get Jared immediately." );
			} else {
				devassertmsgf( 0, "UGC error has not been displayed in the UI." );
			}
		}
	}

	ugcLoadEnd_printf( "done." );

	if( (!ugcPerfErrorHasFired || ugcPerfDebug) && !ugcPerfKnownStallyOperation ) {
		U32 endMs = timerCpuMs();

		if( endMs - startMs > 200 ) {
			Errorf( "UGC Editor refresh taking over 0.2 sec (5 fps)." );
			ugcPerfErrorHasFired = true;
		} else if( endMs - startMs > 100 ) {
			Errorf( "UGC Editor refresh taking over 0.1 sec (10 fps)." );
			ugcPerfErrorHasFired = true;
		}
		
		if( ugcPerfDebug ) {
			printf( "UGC: Refresh took %d ms\n", endMs - startMs );
		}
	}

	// Check all components for non-finite numbers
	if( SAFE_MEMBER( g_UGCEditorDoc->data, components )) {
		int it;
		for( it = 0; it != eaSize( &g_UGCEditorDoc->data->components->eaComponents ); ++it ) {
			UGCComponent* component = g_UGCEditorDoc->data->components->eaComponents[ it ];
			assert( FINITEVEC3( component->sPlacement.vPos ));
		}
	}

	ugcPerfKnownStallyOperation = false;
	sIgnoreChanges = false;
	g_UGCEditorDoc->bLastShouldShowRestoreButtons = gfxShouldShowRestoreButtons();
}

static void ugcEditorUndoCB(EditorObject *unused, void *unused2)
{
	sIgnoreChanges = true;
	FixupStructLeafFirst( parse_UGCProjectData, g_UGCEditorDoc->data, FIXUPTYPE_POST_TEXT_READ, NULL );
	ugcEditorUpdateUI();
	StructCopy(parse_UGCProjectData, g_UGCEditorDoc->data, g_UGCEditorDoc->last_edit_data, 0, 0, 0);
	sIgnoreChanges = false;
	printf("Undo/redo completed.\n");
}

void ugcEditorQueueUIUpdate(void)
{
	g_UGCEditorDoc->queued_ui_update = true;
}

void ugcEditorQueueApplyUpdate(void)
{
	g_UGCEditorDoc->queued_update = true;
}

void ugcEditorMEFieldChangedCB(MEField *pField, bool bFinished, UserData unused)
{
	if (bFinished && !sIgnoreChanges)
		ugcEditorQueueApplyUpdate();
}

void ugcEditorWidgetChangedCB(UIWidget* ignored, UserData ignored2)
{
	ugcEditorQueueApplyUpdate();
}

void ugcEditorApplyUpdateEx(void)
{
	bool update_data = false;
	if (sIgnoreChanges)
		return;
	sIgnoreChanges = true;

	ugcEditorUpdateTrivia();

	if (StructCompare(parse_UGCProjectData, g_UGCEditorDoc->data, g_UGCEditorDoc->last_edit_data, 0, 0, 0) != 0)
	{
		printf("Applying update...\n");
		if( !ugcEditorFixupPostEdit(true)) {
			printf( "User canceled fixup.\n" );
			StructCopy( parse_UGCProjectData, g_UGCEditorDoc->last_edit_data, g_UGCEditorDoc->data, 0, 0, 0 );
			
		} else if(g_UGCEditorDoc->last_edit_data) {
			ugcLoadStart_printf( "Creating Undo Data..." );
			EditCreateUndoStruct(g_UGCEditorDoc->edit_undo_stack, NULL, g_UGCEditorDoc->last_edit_data, g_UGCEditorDoc->data, parse_UGCProjectData, ugcEditorUndoCB);
			EditUndoSetExtraData(g_UGCEditorDoc->edit_undo_stack, parse_UGCEditorView, StructClone( parse_UGCEditorView, &g_UGCEditorDoc->activeView ));
			ugcLoadEnd_printf( "done." );

			update_data = true;

			if (!g_UGCEditorDoc->autosave_timer)
			{
				g_UGCEditorDoc->autosave_timer = timerAlloc();
				timerStart(g_UGCEditorDoc->autosave_timer);
			}
		}
	}
	else
	{
		printf("No change.\n");
	}

	sIgnoreChanges = false;

	// MJF TODO: Should the entire delete doc logic be in sIgnoreChanges = true as well?
	sIgnoreChanges = true;
	ugcEditorUpdateUIEx();
	sIgnoreChanges = false;
	if (update_data)
		StructCopy(parse_UGCProjectData, g_UGCEditorDoc->data, g_UGCEditorDoc->last_edit_data, 0, 0, 0);
}

bool ugcEditorHasUnsavedChanges(void)
{
	if (!g_UGCEditorDoc || !g_UGCEditorDoc->data || !g_UGCEditorDoc->last_save_data)
		return false;

	if (StructCompare(parse_UGCProjectData, g_UGCEditorDoc->data, g_UGCEditorDoc->last_save_data, 0, 0, 0) != 0)
		return true;

	return false;
}

void ugcEditorUndo(void)
{
	if (!g_UGCEditorDoc)
		return;

	EditUndoLast(g_UGCEditorDoc->edit_undo_stack);
}

void ugcEditorRedo(void)
{
	if (!g_UGCEditorDoc)
		return;

	EditRedoLast(g_UGCEditorDoc->edit_undo_stack);
}

//// Flashing updates

void ugcEditorStartObjectFlashing(void *object)
{
	UGCChangeFlash *new_flash;
	if (eaSize(&g_UGCEditorDoc->flashes) > 0 &&
		g_UGCEditorDoc->flashes[0]->flash_timer == 0)
	{
		// Flash has just been created this frame; append to it
		eaPush(&g_UGCEditorDoc->flashes[0]->objects, object);
		return;
	}
	// Create a new flash
	new_flash = calloc(1, sizeof(UGCChangeFlash));
	eaPush(&new_flash->objects, object);
	eaInsert(&g_UGCEditorDoc->flashes, new_flash, 0);
}

F32 ugcEditorGetObjectFlashingTime(void *object)
{
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->flashes, UGCChangeFlash, flash)
	{
		FOR_EACH_IN_EARRAY(flash->objects, void, obj)
		{
			if (obj == object)
			{
				if (flash->flash_timer == 0)
					return 0.f;
				else
					return timerElapsed(flash->flash_timer);
			}
		}
		FOR_EACH_END;
	}
	FOR_EACH_END;
	return -1.f;
}

static void ugcEditorProcessFlashingObjects(void)
{
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->flashes, UGCChangeFlash, flash)
	{
		if (flash->flash_timer == 0)
		{
			flash->flash_timer = timerAlloc();
			timerStart(flash->flash_timer);
		}
		else if (timerElapsed(flash->flash_timer) > 5.f)
		{
			timerFree(flash->flash_timer);
			eaDestroy(&flash->objects);
			SAFE_FREE(flash);
			eaRemove(&g_UGCEditorDoc->flashes, FOR_EACH_IDX(g_UGCEditorDoc->flashes, flash));
		}
	}
	FOR_EACH_END;
}

//// Play mode

static void ugcEditorPlayTimeout(TimedCallback *callback, F32 timeSinceLastCallback, UserData userData)
{
	wl_state.stop_map_transfer = false;
	if (g_UGCEditorDoc->modal_dialog)
	{
		ugcModalDialogMsg( "UGC_Editor.PlayMapError_Timeout", "UGC_Editor.PlayMapError_TimeoutDetails", UIOk );
		ui_WindowClose(g_UGCEditorDoc->modal_dialog);
	}
	g_UGCEditorDoc->modal_dialog = NULL;

	if (g_UGCEditorDoc->modal_timeout)
		TimedCallback_Remove(g_UGCEditorDoc->modal_timeout);
	g_UGCEditorDoc->modal_timeout = NULL;
}

static void ugcEditorSwitchToPlayModeFinish(void* ignored)
{
	if (g_UGCEditorDoc->modal_dialog)
		ui_WindowClose(g_UGCEditorDoc->modal_dialog);
	g_UGCEditorDoc->modal_dialog = NULL;
	if (g_UGCEditorDoc->modal_timeout)
		TimedCallback_Remove(g_UGCEditorDoc->modal_timeout);
	g_UGCEditorDoc->modal_timeout = NULL;

	ugcEditorRefreshGlobalUI();
	ugcEditMode(0);

	// UI will detect rather to show edit preview UI
}

static void ugcEditorSwitchToEditMode(void)
{
	{
		int it;
		for( it = 0; it != eaSize( &g_UGCEditorDoc->map_editors ); ++it ) {
			ugcMapEditorSwitchToEditMode( g_UGCEditorDoc->map_editors[ it ]);
		}
	}
	
	ugcEditMode(1);
}

void UGCEditorDoUGCPublishEnabled( bool bUGCPublishEnabled )
{
	if(g_UGCEditorDoc)
		g_UGCEditorDoc->bPublishingEnabled = bUGCPublishEnabled;
}

void gclUGCDoProcessPlayResult(UGCPlayResult *result)
{
	bool bCloseDialog = true;

	wl_state.stop_map_transfer = false;
	ugcPlayingEditorMapChanged( NULL );

	if (!g_UGCEditorDoc->modal_dialog)
		return;

	if (!result)
	{
		ugcModalDialogMsg( "UGC_Editor.PlayMapError_InternalServer", "UGC_Editor.PlayMapError_InternalServerDetails", UIOk );
	}
	else if (result->eStatus != UGC_PLAY_SUCCESS)
	{
		switch (result->eStatus)
		{
			xcase UGC_PLAY_WRONG_FACTION:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_WrongFaction", "UGC_Editor.PlayMapError_WrongFactionDetails", UIOk );
			xcase UGC_PLAY_NO_OBJECTIVE_MAP:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_NoObjectiveMap", "UGC_Editor.PlayMapError_NoObjectiveMapDetails", UIOk );
			xcase UGC_PLAY_GENESIS_GENERATION_ERROR:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_GenesisGenerationError", "UGC_Editor.PlayMapError_GenesisGenerationErrorDetails", UIOk );
			xcase UGC_PLAY_NO_DIALOG_TREE:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_NoDialogTree", "UGC_Editor.PlayMapError_NoDialogTreeDetails", UIOk );
			xcase UGC_PLAY_NO_PROJECT:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_NoProject", "UGC_Editor.PlayMapError_NoProjectDetails", UIOk );
			xcase UGC_PLAY_BUDGET_ERROR:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_Budget", "UGC_Editor.PlayMapError_BudgetDetails", UIOk );
			xdefault:
				ugcModalDialogMsg( "UGC_Editor.PlayMapError_UnknownError", "UGC_Editor.PlayMapError_UnknownErrorDetails", UIOk );
		}
	}
	else
	{
		ugcPlayingEditorMapChanged( result->eaComponentData );	
	
		result->pInfo->filename = allocAddFilename(result->fstrFilename);
		resFreePreviews();
		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, doc)
		{
			ugcMapEditorSwitchToPlayMode( doc );
		}
		FOR_EACH_END;

		// Get UGC layer data
		ugcLayerCacheClear();
		FOR_EACH_IN_EARRAY(result->eaLayerDatas, UGCPlayLayerData, layer_data)
		{
			LibFileLoad *lib_file_data = StructCreate(parse_LibFileLoad);
			lib_file_data->filename = StructAllocString(layer_data->filename);
			lib_file_data->defs = layer_data->eaDefs;
			layer_data->eaDefs = NULL;
			ugcLayerCacheAddLayerData(lib_file_data);
		}
		FOR_EACH_END;
		
		FOR_EACH_IN_EARRAY(result->eaSkyDefs, char, sky_def)
		{
			SkyInfo *old_sky;
			SkyInfo *new_sky = StructCreate(parse_SkyInfo);
			ParserReadText(sky_def, parse_SkyInfo, new_sky, 0);
			old_sky = RefSystem_ReferentFromString("SkyInfo", new_sky->filename_no_path);
			if (old_sky)
				RefSystem_RemoveReferent(old_sky, false);
			RefSystem_AddReferent("SkyInfo", new_sky->filename_no_path, new_sky);
		}
		FOR_EACH_END;

		if (worldLoadZoneMapAsync(result->pInfo, ugcEditorSwitchToPlayModeFinish, NULL))
		{
			bCloseDialog = false;
			return;
		}
		else
		{
			ugcModalDialogMsg( "UGC.PlayMapError_InternalClient", "UGC.PlayMapError_InternalClientDetails", UIOk );
		}
	}

	if( bCloseDialog ) {
		ui_WindowClose(g_UGCEditorDoc->modal_dialog);
		g_UGCEditorDoc->modal_dialog = NULL;
	}

	if (g_UGCEditorDoc->modal_timeout)
		TimedCallback_Remove(g_UGCEditorDoc->modal_timeout);
	g_UGCEditorDoc->modal_timeout = NULL;
}

void ugcEditorPlay(const char* map_name, U32 objective_id, bool is_mission, Vec3 spawn_pos, Vec3 spawn_rot)
{
	UILabel* label;
	
	// We don't want the server to send us anywhere until we've received the gclUGCProcessPlayResult callback
	wl_state.stop_map_transfer = true;

	ServerCmd_gslUGCPlay( ugcEditorGetProjectData(), map_name, objective_id, spawn_pos, spawn_rot );

	g_UGCEditorDoc->modal_dialog = ui_WindowCreate("", 0, 0, 200, 50);
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor", g_UGCEditorDoc->modal_dialog->widget.hOverrideSkin );
	ui_WidgetSetTextMessage( UI_WIDGET( g_UGCEditorDoc->modal_dialog ), "UGC_Editor.PreviewTitle" );
	label = ui_LabelCreate("", 0, 0);
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor", label->widget.hOverrideSkin );
	ui_WidgetSetTextMessage( UI_WIDGET( label ), "UGC_Editor.PreviewDetails" );
	ui_WindowAddChild(g_UGCEditorDoc->modal_dialog, label);
	ui_WindowSetClosable(g_UGCEditorDoc->modal_dialog, false);
	ui_WindowSetModal(g_UGCEditorDoc->modal_dialog, true);
	ui_WindowSetResizable(g_UGCEditorDoc->modal_dialog, false);
	elUICenterWindow(g_UGCEditorDoc->modal_dialog);
	ui_WindowShowEx(g_UGCEditorDoc->modal_dialog, true);

	g_UGCEditorDoc->modal_timeout = TimedCallback_Add(ugcEditorPlayTimeout, NULL, UGC_PLAY_TIMEOUT);
}

void ugcEditorPlayFromStart(void)
{
	UGCProjectData *data = ugcEditorGetProjectData();
	UGCMissionObjective *objective;
	U32 objective_id = 0;
	const char *map_name;
	bool map_is_internal = false;

	if (!data || !data->mission)
		return;

	if (eaSize(&data->mission->objectives) == 0)
	{
		ugcModalDialogMsg( "UGC_Editor.PlayMapError_NoObjectives", "UGC_Editor.PlayMapError_NoObjectivesDetails", UIOk );
		return;
	}
	objective = data->mission->objectives[0];
	objective_id = objective->id;
	do {
		map_name = ugcObjectiveMapName(data, objective, &map_is_internal);
		if (!map_name && eaSize(&objective->eaChildren) > 0)
			objective = objective->eaChildren[0];
		else
			objective = NULL;
	} while (objective && !map_name);
	if (map_name && map_is_internal)
	{
		UGCMissionMapLink *link = ugcMissionFindLink(data->mission, data->components, map_name, NULL);
		if (link)
		{
			UGCComponent *component = ugcEditorFindComponentByID(link->uDoorComponentID);
			if (component && component->sPlacement.bIsExternalPlacement)
			{
				map_name = component->sPlacement.pcExternalMapName;
			}
			else
			{
				map_name = NULL;
			}
		}
		else
		{
			map_name = NULL;
		}
	}
	if (!map_name)
	{
		UGCPerAllegianceDefaults* defaults = ugcGetAllegianceDefaults(data);
		if( defaults )
			map_name = defaults->pcDefaultCrypticMap;
		else
			map_name = "Kfr_Sol_Starbase_Ground";
	}
	if (map_name)
	{
		ugcEditorPlay(map_name, objective_id, true, NULL, NULL);
	}
	else
	{
		ugcModalDialogMsg( "UGC_Editor.PlayMapError_NoStartMap", "UGC_Editor.PlayMapError_NoStartMapDetails", UIOk );
	}
}

void ugcEditorPlayDialogTree(U32 componentID, int promptID)
{
	ServerCmd_gslUGCPlayDialogTree( ugcEditorGetProjectData(), componentID, promptID );

	g_UGCEditorDoc->modal_dialog = ui_WindowCreate("Play Dialog", 0, 0, 200, 50);
	ui_WindowAddChild(g_UGCEditorDoc->modal_dialog, ui_LabelCreate("Loading dialog...", 0, 0));
	ui_WindowSetClosable(g_UGCEditorDoc->modal_dialog, false);
	ui_WindowSetModal(g_UGCEditorDoc->modal_dialog, true);
	ui_WindowSetResizable(g_UGCEditorDoc->modal_dialog, false);
	elUICenterWindow(g_UGCEditorDoc->modal_dialog);
	ui_WindowShowEx(g_UGCEditorDoc->modal_dialog, true);

	g_UGCEditorDoc->modal_timeout = TimedCallback_Add(ugcEditorPlayTimeout, NULL, UGC_PLAY_TIMEOUT);
}

AUTO_COMMAND;
void UGCPlayMap(ACMD_NAMELIST("ZoneMap",REFDICTIONARY) char *map_name)
{
	ServerCmd_gslUGCPlay(ugcEditorGetProjectData(), map_name, 0, NULL, NULL);
}

//// Active editor switching

static void ugcEditorSetActiveDoc(UGCEditorViewMode mode, const char *name)
{
	if (mode == UGC_VIEW_MAP_EDITOR || mode == UGC_VIEW_NO_MAPS)
	{
		if( mode == UGC_VIEW_NO_MAPS || !ugcEditorGetMapDoc( name )) {
			if( eaSize( &g_UGCEditorDoc->data->maps )) {
				mode = UGC_VIEW_MAP_EDITOR;
				name = g_UGCEditorDoc->data->maps[ 0 ]->pcName;
			} else {
				mode = UGC_VIEW_NO_MAPS;
				name = NULL;
			}
		}
	}
	if (mode == UGC_VIEW_DIALOG_TREE || mode == UGC_VIEW_NO_DIALOG_TREES)
	{
		if( !ugcEditorFindComponentByID( (int)name )) {
			if( eaSize( &g_UGCEditorDoc->dialog_editors )) {
				mode = UGC_VIEW_DIALOG_TREE;
				name = ugcDialogTreeDocGetIDAsPtr( g_UGCEditorDoc->dialog_editors[ 0 ]);
			} else {
				mode = UGC_VIEW_NO_DIALOG_TREES;
				name = NULL;
			}
		}
	}
	if (mode == UGC_VIEW_COSTUME || mode == UGC_VIEW_NO_COSTUMES)
	{
		if( mode == UGC_VIEW_NO_COSTUMES || !ugcEditorGetCostumeDoc( name )) {
			if( eaSize( &g_UGCEditorDoc->data->costumes )) {
				mode = UGC_VIEW_COSTUME;
				name = g_UGCEditorDoc->data->costumes[ 0 ]->astrName;
			} else {
				mode = UGC_VIEW_NO_COSTUMES;
				name = NULL;
			}
		}
	}
	
	g_UGCEditorDoc->activeView.mode = mode;
	g_UGCEditorDoc->activeView.name = name;
	ugcEditorUpdateWindowVisibility();

	// Update the Last Active list
	switch( mode ) {
		xcase UGC_VIEW_MAP_EDITOR:
			g_UGCEditorDoc->strLastActiveMap = name;
		xcase UGC_VIEW_NO_MAPS:
			g_UGCEditorDoc->strLastActiveMap = NULL;
		xcase UGC_VIEW_DIALOG_TREE:
			g_UGCEditorDoc->strLastActiveDialogTree = name;
		xcase UGC_VIEW_NO_DIALOG_TREES:
			g_UGCEditorDoc->strLastActiveDialogTree = NULL;
		xcase UGC_VIEW_COSTUME:
			g_UGCEditorDoc->strLastActiveCostume = name;
		xcase UGC_VIEW_NO_COSTUMES:
			g_UGCEditorDoc->strLastActiveCostume = NULL;
	}
}

void ugcEditorSetActivePlayingEditorMap( const char* mapName )
{
	g_UGCEditorDoc->activeView.playingEditorMap = mapName;
}

const char* ugcEditorGetActivePlayingEditorMap( void )
{
	return SAFE_MEMBER( g_UGCEditorDoc, activeView.playingEditorMap );
}

//// Utility functions

typedef void* UGCEditorGetResourceFn( char* name );
static void ugcEditorCreateResourceName(char **estrNameOut, UGCEditorGetResourceFn fn )
{
	U32 obj_id = timeSecondsSince2000();

	estrCreate(estrNameOut);
	estrPrintf(estrNameOut, "%u", obj_id);

	while( fn( *estrNameOut )) {
		++obj_id;
		estrPrintf(estrNameOut, "%u", obj_id);
	}
}

//// UGCProject functions

UGCProjectEditorDoc* ugcEditorGetOrCreateProjectDoc( void )
{
	INIT_PROJECT;
	if( !project ) {
		return NULL;
	}
	if( !g_UGCEditorDoc->project_editor ) {
		g_UGCEditorDoc->project_editor = ugcProjectEditor_Open();
	}
	return g_UGCEditorDoc->project_editor;
}


//// UGCMission functions

bool ugcEditorMissionIsLoaded(void)
{
	return g_UGCEditorDoc->mission_editor != NULL;
}

UGCMission *ugcEditorLoadAndGetMission()
{
	INIT_PROJECT;
	if (!project)
		return NULL;
	if (!g_UGCEditorDoc->mission_editor)
	{
		g_UGCEditorDoc->mission_editor = ugcMissionDocLoad();
	}
	return ugcMissionGetMission(g_UGCEditorDoc->mission_editor);
}

void ugcEditorEditMissionObjective( const char* objective_name )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedObjectiveByName( g_UGCEditorDoc->mission_editor, objective_name );
	ugcEditorUpdateUI();
}

void ugcEditorEditMissionDialogTreeBlock( U32 dialog_id )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedDialogTreeBlock( g_UGCEditorDoc->mission_editor, dialog_id );
	ugcEditorUpdateUI();
}

static void ugcEditorEditMissionMapTransition( U32 map_transition_objective_id )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedMapTransition( g_UGCEditorDoc->mission_editor, map_transition_objective_id );
	ugcEditorUpdateUI();
}

static void ugcEditorEditMissionMap( U32 map_objective_id )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedMap( g_UGCEditorDoc->mission_editor, map_objective_id );
	ugcEditorUpdateUI();
}

static void ugcEditorEditMissionComponent( const char* component_name )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedObjectiveByComponentName( g_UGCEditorDoc->mission_editor, component_name );
	ugcEditorUpdateUI();
}

static void ugcEditorEditMissionDialogTree( const char* dialog_tree_name )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedObjectiveByDialogName( g_UGCEditorDoc->mission_editor, dialog_tree_name );
	ugcEditorUpdateUI();
}

void ugcEditorEditMissionMapTransitionByMapLink( UGCMissionMapLink* mapLink )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedMapTransitionByMapLink( g_UGCEditorDoc->mission_editor, mapLink );
	ugcEditorUpdateUI();
}

void ugcEditorEditMissionNode( UGCMissionNodeGroup* group )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedGroupAndMakeVisible( group );
	ugcEditorUpdateUI();
}

void ugcEditorEditMission( void )
{
	ugcEditorLoadAndGetMission();
	ugcEditorSetActiveDoc(UGC_VIEW_MISSION, NULL);
	ugcMissionSetSelectedObjectiveByName( g_UGCEditorDoc->mission_editor, "" );
	ugcEditorUpdateUI();
}

//// UGCDialogTreeBlock functions

static UGCDialogTreeDoc* ugcEditorGetOrCreateDialogTreeDoc( U32 dialog_id )
{
	{
		int docIt;
		for( docIt = 0; docIt != eaSize( &g_UGCEditorDoc->dialog_editors ); ++docIt ) {
		
			if( ugcDialogTreeDocGetIDAsPtr( g_UGCEditorDoc->dialog_editors[ docIt ]) == ugcDialogTreeIDAsPtrFromIDIndex( dialog_id )) {
				return g_UGCEditorDoc->dialog_editors[ docIt ];
			}
		}
	}

	{
		UGCDialogTreeDoc* doc = ugcDialogTreeDocCreate( dialog_id );
		if( doc ) {
			eaPush( &g_UGCEditorDoc->dialog_editors, doc );
			return doc;
		}
	}

	return NULL;
}

static UGCNoDialogTreesDoc* ugcEditorGetOrCreateNoDialogTreesDoc( void )
{
	if( g_UGCEditorDoc->no_dialogs_editor ) {
		return g_UGCEditorDoc->no_dialogs_editor;
	}

	if( g_UGCEditorDoc->data ) {
		g_UGCEditorDoc->no_dialogs_editor = ugcNoDialogTreesDocCreate();
	}

	return g_UGCEditorDoc->no_dialogs_editor;
}

void ugcEditorEditDialogTreeBlock( U32 dialog_id, int promptID, int actionIndex )
{
	INIT_PROJECT;
	UGCDialogTreeDoc* doc = ugcEditorGetOrCreateDialogTreeDoc( dialog_id );
	
	if( doc ) {
		ugcEditorSetActiveDoc(UGC_VIEW_DIALOG_TREE, ugcDialogTreeDocGetIDAsPtr( doc ));
		ugcDialogTreeDocSetSelectedPromptAndAction( doc, promptID, actionIndex );
		ugcEditorUpdateUI();
	}
}

void ugcEditorEditDialogTree( UIButton* ignored, UGCComponent* dialog )
{
	ugcEditorEditDialogTreeBlock( dialog->uID, 0, 0 );
}

//// UGCMap functions

UGCMapEditorDoc *ugcEditorGetActiveMapDoc()
{
	if (g_UGCEditorDoc->activeView.mode != UGC_VIEW_MAP_EDITOR)
		return NULL;
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, doc)
	{
		if (g_UGCEditorDoc->activeView.name == ugcMapEditorGetName(doc))
			return doc;
	}
	FOR_EACH_END;
	return NULL;
}

UGCMapEditorDoc *ugcEditorGetOrCreateMapDoc( const char* map_name )
{
	INIT_PROJECT;
	UGCMapEditorDoc *new_doc = NULL;
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, doc)
	{
		if (resNamespaceBaseNameEq(ugcMapEditorGetName(doc), map_name))
		{
			new_doc = doc;
			break;
		}
	}
	FOR_EACH_END;
	if (!new_doc && g_UGCEditorDoc->data)
	{
		UGCMap* map = ugcEditorGetMapByName( map_name );
		if( map ) {
			new_doc = ugcMapEditorLoadDoc( map_name );
			eaPush(&g_UGCEditorDoc->map_editors, new_doc);
		}
	}
	return new_doc;
}

static UGCNoMapsEditorDoc* ugcEditorGetOrCreateNoMapsDoc( void )
{
	if( g_UGCEditorDoc->no_maps_editor ) {
		return g_UGCEditorDoc->no_maps_editor;
	}

	if( g_UGCEditorDoc->data ) {
		g_UGCEditorDoc->no_maps_editor = ugcNoMapsEditorLoadDoc();
	}

	return g_UGCEditorDoc->no_maps_editor;
}

UGCMapEditorDoc *ugcEditorEditMapComponent( const char* map_name, U32 component_id, bool select_unplaced, bool refresh_ui )
{
	INIT_PROJECT;
	UGCMapEditorDoc *new_doc = ugcEditorGetOrCreateMapDoc(map_name);
	if (new_doc)
	{
		ugcEditorSetActiveDoc(UGC_VIEW_MAP_EDITOR, ugcMapEditorGetName(new_doc));

		if (!new_doc->map_data->pUnitializedMap) {
			if (component_id != UGC_NONE)
				ugcMapEditorSetSelectedComponent(new_doc, component_id, 0, true, true);
			else
				ugcMapEditorClearSelection(new_doc);

			if( select_unplaced ) {
				ugcMapEditorSelectUnplacedTab( new_doc );
			}
		}

		if (refresh_ui)
			ugcEditorRefreshGlobalUI();
	}
	return new_doc;
}

static void ugcEditorEditMap(UIButton *button, const char *map_name)
{
	ugcEditorEditMapComponent( map_name, UGC_NONE, false, true );
}

static void ugcEditorInitMap(UGCMap* map, const char *new_name, const char* new_notes, UGCMapType new_type, const char *prefab_map_name, bool bNewMapInMissionEditor)
{
	UGCPerProjectDefaults *defaults = ugcGetDefaults();

	if( !map ) {
		char map_full_name[ 256 ], map_filename[ MAX_PATH ];
		char *map_name;
		
		ugcEditorCreateResourceName( &map_name, ugcEditorGetMapByName );
		sprintf( map_full_name, "%s:%s", g_UGCEditorDoc->data->ns_name, map_name );
		sprintf( map_filename, "ns/%s/UGC/%s.ugcmap", g_UGCEditorDoc->data->ns_name, map_name );
		map = StructCreate( parse_UGCMap );
		map->pcName = allocAddFilename( map_full_name );
		map->pcFilename = allocAddFilename( map_filename );
		
		eaPush(&g_UGCEditorDoc->data->maps, map);
		estrDestroy( &map_name );
	}

	StructCopyString(&map->pcDisplayName, new_name);
	StructCopyString(&map->strNotes, new_notes);

	StructDestroySafe( parse_UGCGenesisSpace, &map->pSpace );
	StructDestroySafe( parse_UGCGenesisPrefab, &map->pPrefab );
	StructDestroySafe( parse_UGCUnitializedMap, &map->pUnitializedMap );
	switch (new_type)
	{
	case UGC_MAP_TYPE_SPACE:
		map->pSpace = StructCreate(parse_UGCGenesisSpace);
	case UGC_MAP_TYPE_PREFAB_SPACE:
		map->pPrefab = StructCreate(parse_UGCGenesisPrefab);
		map->pPrefab->map_name = allocAddString(prefab_map_name);
		break;
	case UGC_MAP_TYPE_PREFAB_GROUND:
		map->pPrefab = StructCreate(parse_UGCGenesisPrefab);
		map->pPrefab->map_name = allocAddString(prefab_map_name);
		break;
	case UGC_MAP_TYPE_INTERIOR:
	case UGC_MAP_TYPE_PREFAB_INTERIOR:
		map->pPrefab = StructCreate(parse_UGCGenesisPrefab);
		if (new_type == UGC_MAP_TYPE_INTERIOR)
		{
			map->pPrefab->map_name = allocAddString(defaults->pcCustomInteriorMap);
			map->pPrefab->customizable = true;
		}
		else
		{
			map->pPrefab->map_name = allocAddString(prefab_map_name);
		}
		break;
	}

	if( bNewMapInMissionEditor ) {
		ugcMissionDocHandleNewMap( g_UGCEditorDoc->mission_editor, map );
	} else {
		ugcEditorFixupPostEdit(true);
		ugcEditorApplyUpdate();

		ugcEditorEditMapComponent( map->pcName, UGC_NONE, false, false );
	}
}

static StaticDefineInt UGCMapTypePickerEnum[] =
{
	DEFINE_INT
	{ "UGC.MapType_Space", UGC_MAP_TYPE_SPACE },
	{ "UGC.MapType_Ground", UGC_MAP_TYPE_GROUND },
	{ "UGC.MapType_Interior", UGC_MAP_TYPE_INTERIOR },
	DEFINE_END
};

static StaticDefineInt UGCMapTypePickerWithoutSpaceEnum[] =
{
	DEFINE_INT
	{ "UGC.MapType_Interior", UGC_MAP_TYPE_INTERIOR },
	{ "UGC.MapType_Ground", UGC_MAP_TYPE_GROUND },
	DEFINE_END
};

StaticDefineInt* ugcEditorGetPickerEnum( void )
{
	if( ugcIsSpaceEnabled() ) {
		return UGCMapTypePickerEnum;
	} else {
		return UGCMapTypePickerWithoutSpaceEnum;
	}
}

AUTO_STRUCT;
typedef struct UGCNewMapUIState
{
	char* name;					AST( NAME(Name) )
	char* notes;				AST( NAME(Notes) )
	UGCMapType mapType;			AST( NAME(MapType) )
} UGCNewMapUIState;
extern ParseTable parse_UGCNewMapUIState[];
#define TYPE_parse_UGCNewMapUIState UGCNewMapUIState

typedef struct UGCNewMapUI
{
	// The map being edited, or NULL if it is a new map
	UGCMap* map;

	// The window this UI is in
	UIWindow* rootWindow;
	
	// The context holding all the other widgets
	MEFieldContext* rootContext;

	// If set, we are updating UI and so we don't want to respond to changed callbacks.
	bool ignoreChanges;

	// The asset library
	UGCAssetLibraryPane* libraryPicker;

	// Set this in refresh.  On new creation, this should get the focus
	UIWidget* defaultFocusWidget;

	// The editable state
	UGCNewMapUIState state;

	// If set, then this UI was created by the mission editor for
	// mission-editor behavior.  Call back to the mission editor when
	// done.
	bool newMapInMissionEditor;

	// The errors
	MEFieldGenericErrorList errorList;
} UGCNewMapUI;

static void ugcEditorNewMapUIDestroy( UIButton* ignored, UGCNewMapUI* ui )
{
	ui_SetFocus( NULL );
	
	ugcAssetLibraryPaneDestroy( ui->libraryPicker );
	MEContextDestroyExternalContext( ui->rootContext );
	ui_WidgetQueueFree( UI_WIDGET( ui->rootWindow ));
	StructReset( parse_UGCNewMapUIState, &ui->state );
	StructReset( parse_MEFieldGenericErrorList, &ui->errorList );
	SAFE_FREE( ui );
}

static bool ugcEditorNewMapUIWindowClosedCB( UIWindow* window, UGCNewMapUI* ui )
{
	ugcEditorNewMapUIDestroy( NULL, ui );
	return false;
}

static bool ugcEditorNewMapUINameTaken( UGCNewMapUI* ui )
{
	FOR_EACH_IN_EARRAY( g_UGCEditorDoc->data->maps, UGCMap, map ) {
		if(   stricmp( ui->state.name, map->pcDisplayName ) == 0
			  && stricmp( ui->state.notes, map->strNotes ) == 0 ) {
			return true;
		}
	} FOR_EACH_END;

	return false;
}

static void ugcEditorNewMapUIValidate( UGCNewMapUI* ui )
{
	StructReset( parse_MEFieldGenericErrorList, &ui->errorList );

	if( !ui->libraryPicker || !ugcAssetLibraryPaneGetSelected( ui->libraryPicker )) {
		MEContextGenericErrorAdd( &ui->errorList, "MapName", -1, "UGC_Errors.NewMap_NoMap" );
	}
	if( nullStr( ui->state.name )) {
		MEContextGenericErrorAdd( &ui->errorList, "Name", -1, "UGC_Errors.NewMap_MapName_TooShort" );
	}
	if( IsAnyProfane( ui->state.name )) {
		MEContextGenericErrorAdd( &ui->errorList, "Name", -1, "UGC_Errors.NewMap_MapName_Profane" );
	}
	if( ugcEditorNewMapUINameTaken( ui )) {
		MEContextGenericErrorAdd( &ui->errorList, "Notes", -1, "UGC_Errors.NewMap_MapNameAndNotes_NotUnique" );
	}
}

static void ugcEditorDoCreateNewMap(UIButton *button, UGCNewMapUI *ui)
{
	UGCMapType mapType = ui->state.mapType;
	const char* name = ui->state.name;
	const char* notes = ui->state.notes;
	const char* externalMapName = NULL;

	ui_SetFocus( NULL );

	{
		UGCAssetLibraryRow *row = ugcAssetLibraryPaneGetSelected( ui->libraryPicker );
		if( !row ) {
			return;
		}

		externalMapName = row->pcName;
		if( stricmp( externalMapName, "UGC_Custom_Interior" ) == 0 ) {
			externalMapName = NULL;
			mapType = UGC_MAP_TYPE_INTERIOR;
		} else {
			if( mapType == UGC_MAP_TYPE_SPACE ) {
				mapType = UGC_MAP_TYPE_PREFAB_SPACE;
			} else if( mapType == UGC_MAP_TYPE_GROUND ) {
				mapType = UGC_MAP_TYPE_PREFAB_GROUND;
			} else if( mapType == UGC_MAP_TYPE_INTERIOR ) {
				mapType = UGC_MAP_TYPE_PREFAB_INTERIOR;
			}
		}
	}

	{
		UGCAchievementEvent *event = StructCreate(parse_UGCAchievementEvent);
		event->uUGCAuthorID = entGetAccountID(entActivePlayerPtr());
		event->ugcAchievementClientEvent = StructCreate(parse_UGCAchievementClientEvent);
		event->ugcAchievementClientEvent->ugcMapCreatedEvent = StructCreate(parse_UGCMapCreatedEvent);
		event->ugcAchievementClientEvent->ugcMapCreatedEvent->type = mapType;
		gclUGC_SendAchievementEvent(event);
		StructDestroy(parse_UGCAchievementEvent, event);
	}

	ugcEditorInitMap( ui->map, name, notes, mapType, externalMapName, ui->newMapInMissionEditor );
	ugcEditorNewMapUIDestroy( NULL, ui );
}

static void ugcEditorNewMapUIRefresh( UGCNewMapUI* ui );

static void ugcEditorNewMapUISetTab( UIButton* button, UGCNewMapUI* ui )
{
	ui->state.mapType = button->widget.u64;
	ugcEditorNewMapUIRefresh( ui );
}

static void ugcEditorNewMapUIRefresh( UGCNewMapUI* ui )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_Header_Box" );
	MEFieldContextEntry* entry;
	UIPane* pane;
	UIWidget* widget;
	float tabY;

	ugcEditorNewMapUIValidate( ui );
	
	// UI refresh
	ui->ignoreChanges = true;
	MEContextPushExternalContext( ui->rootContext, NULL, &ui->state, parse_UGCNewMapUIState );
	MEContextSetParent( UI_WIDGET( ui->rootWindow ));
	ui->rootContext->iXPos = 0;
	ui->rootContext->iYPos = 0;
	ui->rootContext->iXDataStart = 100;
	ui->rootContext->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;

	pane = ugcMEContextPushPaneParentWithHeaderMsg( "PropertiesPane", "Header", "UGC_MapEditor.NewMapProperties", false );
	ui_PaneSetStyle( pane, paneAssembly->pchName, true, false );
	{
		MEContextSetErrorFunction( MEContextGenericErrorMsgFunction );
		MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
		setVec2( MEContextGetCurrent()->iErrorIconOffset, 0, 3 );
		MEContextGetCurrent()->iErrorIconSpaceWidth = atlasFindTexture( "ugc_icons_labels_alert" )->width + 5;
		MEContextSetErrorContext( &ui->errorList );
	
		entry = MEContextAddTextMsg( false, NULL, "Name", "UGC_MapEditor.NewMapName", "UGC_MapEditor.NewMapName.Tooltip" );
		ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_SINGLE_LINE_MAX_LENGTH );
		
		entry = MEContextAddTextMsg( false, NULL, "Notes", "UGC_MapEditor.NewMapNotes", "UGC_MapEditor.NewMapNotes.Tooltip" );
		ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_SINGLE_LINE_MAX_LENGTH );

		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	}
	MEContextPop( "PropertiesPane" );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	
	pane = MEContextPushPaneParent( "HeaderPane" );
	ui_PaneSetStyle( pane, "UGC_Pane_Light_Header_Box_Cover", true, false );
	MEContextGetCurrent()->astrOverrideSkinName = allocAddString( "UGCAssetLibrary" );
	{
		tabY = 0;
		
		entry = MEContextAddButtonMsg( "UGC_MapEditor.NewMapInterior", NULL, ugcEditorNewMapUISetTab, ui, "IndoorTab", NULL, "UGC_MapEditor.NewMapInterior.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		widget->u64 = UGC_MAP_TYPE_INTERIOR;
		ENTRY_BUTTON( entry )->textOffsetFrom = UILeft;
		ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopRight );
		ui_WidgetSetWidth( widget, 120 );
		if( ui->state.mapType == UGC_MAP_TYPE_INTERIOR ) {
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Picker_Active", widget->hOverrideSkin );
		}

		entry = MEContextAddButtonMsg( "UGC_MapEditor.NewMapExterior", NULL, ugcEditorNewMapUISetTab, ui, "OutdoorTab", NULL, "UGC_MapEditor.NewMapExterior.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		widget->u64 = UGC_MAP_TYPE_GROUND;
		ENTRY_BUTTON( entry )->textOffsetFrom = UILeft;
		ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopLeft );
		ui_WidgetSetWidth( widget, 120 );
		if( ui->state.mapType == UGC_MAP_TYPE_GROUND ) {
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Picker_Active", widget->hOverrideSkin );
		}
		tabY = ui_WidgetGetNextY( widget );
	}
	MEContextPopNoIncrementYPos( "HeaderPane" );
	ui_WidgetSetDimensions( UI_WIDGET( pane ), 260, tabY );
	
	if( !ui->libraryPicker ) {
		ui->libraryPicker = ugcAssetLibraryPaneCreate( UGCAssetLibrary_NewMapWindow, true, NULL, NULL, NULL );
	}
	switch( ui->state.mapType ) {
		xcase UGC_MAP_TYPE_SPACE:
			ugcAssetLibraryPaneSetTagTypeName( ui->libraryPicker, "PrefabSpaceMap" );
		xcase UGC_MAP_TYPE_GROUND:
			ugcAssetLibraryPaneSetTagTypeName( ui->libraryPicker, "PrefabGroundMap" );
		xcase UGC_MAP_TYPE_INTERIOR:
			ugcAssetLibraryPaneSetTagTypeName( ui->libraryPicker, "PrefabInteriorMap" );
		xdefault:
			assert( "ecase" );
	}
	ugcAssetLibraryPaneSetHeaderWidget( ui->libraryPicker, UI_WIDGET( pane ));
	widget = UI_WIDGET( ugcAssetLibraryPaneGetUIPane( ui->libraryPicker ));
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 0, 0, MEContextGetCurrent()->iYPos, UGC_ROW_HEIGHT );
	ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, widget );

	{
		float x = 0;
		entry = MEContextAddButtonMsg( "UGC.Create", NULL, ugcEditorDoCreateNewMap, ui, "CreateButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_SetActive( widget, eaSize( &ui->errorList.eaErrors ) == 0 );
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_DialogDefault", widget->hOverrideSkin );
		ui_WidgetSetPositionEx( widget, x, 0, 0, 0, UIBottomRight );
		ui_WidgetSetWidth( widget, 80 );
		x = ui_WidgetGetNextX( widget );

		entry = MEContextAddButtonMsg( "UGC.Cancel", NULL, ugcEditorNewMapUIDestroy, ui, "CancelButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, x, 0, 0, 0, UIBottomRight );
		ui_WidgetSetWidth( widget, 80 );
		x = ui_WidgetGetNextX( widget );
	}

	MEContextPopExternalContext( ui->rootContext );
	ui->ignoreChanges = false;
}

static void ugcEditorNewMapUIMEFieldChangedCB( MEField *pField, bool bFinished, UGCNewMapUI* ui )
{
	if( ui->ignoreChanges ) {
		return;
	}
	
	if( bFinished ) {
		ugcEditorNewMapUIRefresh( ui );
	}
}

void ugcEditorFinishCreateMap( UGCMap* map, bool bNewMapInMissionEditor )
{
	UGCNewMapUI *ui = calloc(1, sizeof(UGCNewMapUI));
	if( map ) {
		assert( map->pUnitializedMap );
	}
	ui->map = map;
	ui->newMapInMissionEditor = bNewMapInMissionEditor;

	{
		int w = g_ui_State.screenWidth;
		int h = g_ui_State.screenHeight;
		ui->rootWindow = ui_WindowCreate( NULL, 0, 0, UGC_LIBRARY_PANE_WIDTH + 10, 610 );
		ui_WidgetSetTextMessage( UI_WIDGET( ui->rootWindow ), "UGC_MapEditor.NewMapTitle" );
		ui_WidgetSetPosition( UI_WIDGET( ui->rootWindow ), w / 2 + 100, (h - UI_WIDGET( ui->rootWindow )->height) / 2 );
	}
	ui_WindowSetModal( ui->rootWindow, true );
	ui_WindowSetResizable( ui->rootWindow, false );
	ui_WindowSetMovable( ui->rootWindow, false );
	ui->rootWindow->bDimensionsIncludesNonclient = true;
	ui_WindowSetCloseCallback( ui->rootWindow, ugcEditorNewMapUIWindowClosedCB, ui );

	ui->rootContext = MEContextCreateExternalContext( "NewMapUI" );
	ui->rootContext->cbChanged = ugcEditorNewMapUIMEFieldChangedCB;
	ui->rootContext->pChangedData = ui;

	ui->state.mapType = UGC_MAP_TYPE_INTERIOR;
	ugcEditorNewMapUIRefresh( ui );

	ui_WindowShowEx( ui->rootWindow, true );
	if( ui->defaultFocusWidget ) {
		ui_SetFocus( ui->defaultFocusWidget );
	}
}

static void ugcEditorCheckCreateNewMap(UIDialogResponseCallback confirm_cb, UserData data)
{
	UGCMap **maps = ugcEditorGetMapsList();
	UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_MAP, 0);
	if (budget && (eaSize(&maps)+1) > budget->iHardLimit)
	{
		ugcModalDialogMsg( "UGC.Error", "UGC_MapEditor.TooManyMaps", UIOk );
		return;
	}
	if (budget && (eaSize(&maps)+1) > budget->iSoftLimit)
	{
		if( ugcModalDialogMsg( "UGC.Error", "UGC_MapEditor.TooManyMapsWarning", UIYes | UINo ) != UIYes ) {
			return;
		}
	}
	confirm_cb(NULL, UIYes, data);
}

static bool ugcEditorCreateNewMapConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData rawNewMapInMissionEditor)
{
	bool bNewMapInMissionEditor = (bool)rawNewMapInMissionEditor;
	if (eButton == UIYes)
	{
		ugcEditorFinishCreateMap( NULL, bNewMapInMissionEditor );
	}
	return true;
}

void ugcEditorCreateNewMap(bool bNewMapInMissionEditor)
{
	ugcEditorCheckCreateNewMap(ugcEditorCreateNewMapConfirmCB, (UserData)(intptr_t)bNewMapInMissionEditor);
}

void ugcEditorDeleteMap(UGCMapEditorDoc* doc)
{
	UGCProjectData* ugcProj = g_UGCEditorDoc->data;
	UGCMissionObjective* objective = NULL;

	if( g_UGCEditorDoc->activeView.mode ) {
		objective = ugcObjectiveFindOnMap( ugcProj->mission->objectives, ugcProj->components, doc->doc_name );
	}
	
	if (UIYes != ugcModalDialogMsg("UGC_MapEditor.DeleteMap", (objective ? "UGC_MapEditor.DeleteMapDetails_UsedInStory" : "UGC_MapEditor.DeleteMapDetails"), UIYes | UINo)) {
		return;
	}

	if( objective ) {
		// Make the map uninitialized
		FOR_EACH_IN_EARRAY(ugcProj->maps, UGCMap, map)
		{
			if (map->pcName == doc->doc_name)
			{
				UGCMapType mapType = ugcMapGetTypeEx( map, true );
				map->pUnitializedMap = StructCreate( parse_UGCUnitializedMap );
				map->pUnitializedMap->eType = mapType;
				StructDestroySafe( parse_UGCGenesisSpace, &map->pSpace );
				StructDestroySafe( parse_UGCGenesisPrefab, &map->pPrefab );
				break;
			}
		}
		FOR_EACH_END;
	} else {
		// Remove the map from the project
		FOR_EACH_IN_EARRAY(ugcProj->maps, UGCMap, map)
		{
			if (map->pcName == doc->doc_name)
			{
				eaRemove(&ugcProj->maps, FOR_EACH_IDX(ugcProj->maps, map));
				StructDestroy(parse_UGCMap, map);
				break;
			}
		}
		FOR_EACH_END;
	}

	// Process the change
	ugcEditorQueueApplyUpdate();
}

void ugcEditorDuplicateMapEx(UGCMapEditorDoc* doc)
{
	UGCMap *map_to_duplicate = doc ? doc->map_data : NULL;
	UGCMap *new_map;
	int new_idx = 2;
	char *map_name = NULL;
	char map_full_name[256], map_filename[MAX_PATH];
	UGCEditorCopyBuffer *copy_buffer;
	UGCMapEditorDoc *new_doc;

	assert(g_UGCEditorDoc->data);

	if (!map_to_duplicate)
	{
		return;
	}

	new_map = StructClone(parse_UGCMap, map_to_duplicate);
	{
		char* estrNewDisplayName = NULL;
		ugcFormatMessageKey( &estrNewDisplayName, "UGC_MapEditor.DuplicateMapName",
							 STRFMT_STRING( "MapName", NULL_TO_EMPTY( map_to_duplicate->pcDisplayName )),
							 STRFMT_END );
		StructCopyString( &new_map->pcDisplayName, estrNewDisplayName );
		estrDestroy( &estrNewDisplayName );
	}
	assert(new_map);

	// Update internal name
	ugcEditorCreateResourceName(&map_name, ugcEditorGetMapByName);
	sprintf(map_full_name, "%s:%s", g_UGCEditorDoc->data->ns_name, map_name);
	sprintf(map_filename, "ns/%s/UGC/%s.ugcmap", g_UGCEditorDoc->data->ns_name, map_name);
	new_map->pcName = allocAddString(map_full_name);
	new_map->pcFilename = allocAddFilename(map_filename);
	estrDestroy(&map_name);

	eaPush(&g_UGCEditorDoc->data->maps, new_map);

	// Create doc
	new_doc = ugcMapEditorLoadDoc(new_map->pcName);
	new_doc->map_data = ugcEditorGetMapByName( new_doc->doc_name );
	eaPush(&g_UGCEditorDoc->map_editors, new_doc);

	// Copy all components from old map to new map
	copy_buffer = StructCreate(parse_UGCEditorCopyBuffer);
	copy_buffer->eType = UGC_COPY_COMPONENT;
	copy_buffer->eSourceMapType = ugcMapGetType(map_to_duplicate);
	UGC_FOR_EACH_COMPONENT_ON_MAP(g_UGCEditorDoc->data->components, map_to_duplicate->pcName, component)
	{
		if (component->uParentID == 0 && component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP)
		{
			eaPush(&copy_buffer->eaComponents, StructClone(parse_UGCComponent, component));
			ugcMapEditorCopyChildrenRecurse(copy_buffer, component);
		}
	}
	UGC_FOR_EACH_COMPONENT_END;

	ugcMapEditorPaste(new_doc, copy_buffer, true, false);

	StructDestroy(parse_UGCEditorCopyBuffer, copy_buffer);

	ugcEditorFixupPostEdit(true);
	ugcEditorApplyUpdate();

	ugcEditorEditMapComponent( new_map->pcName, UGC_NONE, false, true );
}

static bool ugcEditorDuplicateMapConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData rawDoc)
{
	UGCMapEditorDoc* doc = rawDoc;
	if (eButton == UIYes)
	{
		ugcEditorDuplicateMapEx(doc);
	}
	return true;
}

void ugcEditorDuplicateMap(UGCMapEditorDoc* doc)
{
	UGCMap *map_to_duplicate = doc->map_data;
	if (!map_to_duplicate)
		return;

	ugcEditorCheckCreateNewMap(ugcEditorDuplicateMapConfirmCB, doc);
}

//// UGCCostume functions

static UGCCostumeEditorDoc* ugcEditorGetOrCreateCostumeDoc(const char* costume_name)
{
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, editor)
	{
		if (resNamespaceBaseNameEq( editor->astrName, costume_name ))
		{
			return editor;
		}
	}
	FOR_EACH_END;

	
	if (g_UGCEditorDoc->data)
	{
		int i;
		for (i = eaSize(&g_UGCEditorDoc->data->costumes)-1; i >= 0; --i)
		{
			if (resNamespaceBaseNameEq( g_UGCEditorDoc->data->costumes[i]->astrName, costume_name ))
			{
				UGCCostumeEditorDoc* pDoc = ugcCostumeEditor_Open(g_UGCEditorDoc->data->costumes[i]);
				eaPush(&g_UGCEditorDoc->costume_editors, pDoc);
				return pDoc;
			}
		}
	}

	return NULL;
}

static UGCNoCostumesEditorDoc* ugcEditorGetOrCreateNoCostumesDoc( void )
{
	if( g_UGCEditorDoc->no_costumes_editor ) {
		return g_UGCEditorDoc->no_costumes_editor;
	}

	if( g_UGCEditorDoc->data ) {
		g_UGCEditorDoc->no_costumes_editor = ugcNoCostumesEditor_Open();
	}

	return g_UGCEditorDoc->no_costumes_editor;
}

void ugcEditorEditCostume(UIWidget* ignored, const char *costume_name)
{
	INIT_PROJECT;
	UGCCostumeEditorDoc *pDoc = ugcEditorGetOrCreateCostumeDoc(costume_name);
	
	if (pDoc)
	{
		ugcEditorSetActiveDoc(UGC_VIEW_COSTUME, pDoc->astrName);
		ugcEditorRefreshGlobalUI();
	}
}

static void ugcEditorMakeBlankCostume( UGCCostume* ugcCostume, bool bSpace )
{
	WorldUGCFactionRestrictionProperties** factions = SAFE_MEMBER2( ugcEditorGetProjectData(), project->pRestrictionProperties, eaFactions );
	WorldUGCFactionRestrictionProperties* faction = eaGet( &factions, 0 );
	NOCONST(PlayerCostume)* pCostume = StructCreateNoConst( parse_PlayerCostume );
	pCostume->eCostumeType = kPCCostumeType_UGC;

	// Get a valid species
	if( faction ) {
		SpeciesDef** eaSpecies = NULL;
		SpeciesDef* pSpecies;
		CharacterCreation_FillSpeciesList(&eaSpecies, faction->pcFaction, bSpace, true);
		pSpecies = eaGet( &eaSpecies, 0 );
		eaDestroy( &eaSpecies );

		if( pSpecies ) {
			SET_HANDLE_FROM_REFERENT("SpeciesDef", pSpecies, pCostume->hSpecies);
			SET_HANDLE_FROM_REFERENT("PCSkeletonDef", GET_REF(pSpecies->hSkeleton), pCostume->hSkeleton);
		}
	}

	if ( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR ) {
		pCostume->eDefaultColorLinkAll = false;
	}
	
	// Force the costume to be a legal member of the species
	costumeTailor_FillAllBones(pCostume, GET_REF(pCostume->hSpecies), NULL, NULL, true, false, true);
	costumeTailor_MakeCostumeValid(pCostume, GET_REF(pCostume->hSpecies), NULL, NULL, true, false, false, NULL, true, NULL, false, NULL);

	StructDestroySafe( parse_PlayerCostume, &ugcCostume->pPlayerCostume );
	ugcCostume->pPlayerCostume = CONTAINER_RECONST( PlayerCostume, pCostume );
}

UGCCostume* ugcEditorCreateCostume(const char* displayName, const char* presetCostumeName, U32 region)
{
	char res_full_name[RESOURCE_NAME_MAX_SIZE];
	char res_filename[MAX_PATH];
	char *res_name = NULL;
	UGCCostume *new_costume;

	ugcEditorCreateResourceName(&res_name, ugcEditorGetCostumeByName);
	sprintf(res_full_name, "%s:%s", g_UGCEditorDoc->data->ns_name, res_name);
	sprintf(res_filename, "ns/%s/UGC/%s.ugccostume", g_UGCEditorDoc->data->ns_name, res_name);
	new_costume = StructCreate(parse_UGCCostume);
	new_costume->astrName = allocAddString(res_full_name);
	new_costume->fstrFilename = allocAddFilename(res_filename);
	new_costume->pcDisplayName = StructAllocString(displayName);

	eaPush(&g_UGCEditorDoc->data->costumes, new_costume);

	estrDestroy(&res_name);

	switch( ugcDefaultsCostumeEditorStyle() ) {
		xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR: {
			int isSpace = (region == (U32)StaticDefineIntGetIntDefault( CharClassTypesEnum, "Space", -1 ));
			ugcEditorMakeBlankCostume( new_costume, isSpace );
		}
			
		xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER:
			ugcCostumeRevertToPreset( new_costume, presetCostumeName );
	}
	new_costume->eRegion = region;
	{
		WorldUGCFactionRestrictionProperties** factions = SAFE_MEMBER2( ugcEditorGetProjectData(), project->pRestrictionProperties, eaFactions );
		WorldUGCFactionRestrictionProperties* faction = eaGet( &factions, 0 );
		if( faction ) {
			AllegianceDef* allegiance = RefSystem_ReferentFromString( g_hAllegianceDict, faction->pcFaction );
			if( allegiance ) {
				SET_HANDLE_FROM_REFERENT( g_hAllegianceDict, allegiance, new_costume->hAllegiance );
			}
		}
	}

	ugcEditorQueueApplyUpdate(); // Add to undo stack

	// Notify of new costume so it gets added to search results
	ugcEditorUserResourceChanged();

	return new_costume;
}

AUTO_STRUCT;
typedef struct UGCNewCostumeState
{
	char* displayName;			AST( NAME(DisplayName) )
	char* description;			AST( NAME(Description) )
} UGCNewCostumeState;
extern ParseTable parse_UGCNewCostumeState[];
#define TYPE_parse_UGCNewCostumeState UGCNewCostumeState

typedef struct UGCNewCostumeUI
{
	// The window this UI is in
	UIWindow* rootWindow;

	// The context holding all the other widgets
	MEFieldContext* rootContext;

	// If set, we are updating UI and so we don't want to respond to changed callbacks.
	bool ignoreChanges;

	// The asset library
	UGCAssetLibraryPane* libraryPicker;

	// Set this in refresh.  On new creation, this should get the focus
	UIWidget* defaultFocusWidget;

	// The editable state
	UGCNewCostumeState state;

	// The errors
	MEFieldGenericErrorList errorList;
} UGCNewCostumeUI;

static void ugcEditorCancelCreateNewCostume( UIWidget* ignored, UGCNewCostumeUI *ui)
{
	// Clear the focus before destroying things, so that we don't get
	// changed CBs in the middle of teardown.
	ui_SetFocus( NULL );

	ugcAssetLibraryPaneDestroy( ui->libraryPicker );
	MEContextDestroyExternalContext( ui->rootContext );
	ui_WidgetQueueFree( UI_WIDGET( ui->rootWindow ));
	StructReset( parse_UGCNewCostumeState, &ui->state );
	StructReset( parse_MEFieldGenericErrorList, &ui->errorList );
	SAFE_FREE( ui );
}

static bool ugcEditorNewCostumeUIWindowClosedCB( UIWindow* window, UGCNewCostumeUI* ui )
{
	ugcEditorCancelCreateNewCostume( NULL, ui );
	return false;
}

static void ugcEditorCreateNewCostumeAssetLibCB( UGCAssetLibraryPane* libraryPane, UGCNewCostumeUI* ui, UGCAssetLibraryRow* row )
{
	INIT_PROJECT;
	char *estrTrimmedName = NULL;
	const char* presetCostumeName = NULL;
	PlayerCostume* presetCostume = NULL;

	if (!project)
		return;
	
	ui_SetFocus(NULL);

	// Trim the name
	if( ui->state.displayName )
	{
		estrCopy2(&estrTrimmedName, ui->state.displayName );
		estrTrimLeadingAndTrailingWhitespace(&estrTrimmedName);
	}

	// Complain if no actual name
	if (!ui->state.displayName)
	{
		ugcModalDialogMsg( "UGC.Error", "UGC_CostumeEditor.NewCostumeError_MustSpecifyName", UIOk );
		return;
	}

	if (estrTrimmedName)
	{
		ANALYSIS_ASSUME(estrTrimmedName);
		if (strlen(estrTrimmedName) < 1)
		{
			ugcModalDialogMsg( "UGC.Error", "UGC_CostumeEditor.NewCostumeError_MustSpecifyName", UIOk );
			return;
		}
	}

	// Look for duplicate name
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->costumes, UGCCostume, costume)
	{
		if (stricmp(estrTrimmedName, costume->pcDisplayName) == 0)
		{
			ugcModalDialogMsg( "UGC.Error", "UGC_CostumeEditor.NewCostumeError_DuplicateName", UIOk );
			return;
		}
	}
	FOR_EACH_END;

	if( row ) {
		presetCostumeName = row->pcName;
	}

	if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_NEVERWINTER && !presetCostumeName ) {
		ugcModalDialogMsg( "UGC.Error", "UGC_CostumeEditor.NewCostumeError_MustSpecifyPreset", UIOk );
		return;
	}

	// Create the costume
	{
		UGCCostume* newCostume = ugcEditorCreateCostume(estrTrimmedName, presetCostumeName, 0 );
		if( newCostume ) {
			ugcEditorEditCostume(NULL, newCostume->astrName);
		}
	}
	ugcEditorCancelCreateNewCostume( NULL, ui );

	estrDestroy(&estrTrimmedName);
}

static void ugcEditorDoCreateNewCostume( UIWidget* ignored, UGCNewCostumeUI *ui)
{
	ugcEditorCreateNewCostumeAssetLibCB( ui->libraryPicker, ui,
										 (ui->libraryPicker ? ugcAssetLibraryPaneGetSelected( ui->libraryPicker ) : NULL) );
}

static bool ugcEditorNewCostumeUINameTaken( UGCNewCostumeUI* ui )
{
	FOR_EACH_IN_EARRAY( g_UGCEditorDoc->data->costumes, UGCCostume, costume ) {
		if(   stricmp( ui->state.displayName, costume->pcDisplayName ) == 0
			  && stricmp( ui->state.description, costume->pcDescription ) == 0 ) {
			return true;
		}
	} FOR_EACH_END;

	return false;
}

static void ugcEditorNewCostumeUIValidate( UGCNewCostumeUI* ui )
{
	StructReset( parse_MEFieldGenericErrorList, &ui->errorList );

	if( !ui->libraryPicker || !ugcAssetLibraryPaneGetSelected( ui->libraryPicker )) {
		MEContextGenericErrorAdd( &ui->errorList, "CostumeName", -1, "UGC_Errors.NewCostume_NoCostume" );
	}
	if( nullStr( ui->state.displayName )) {
		MEContextGenericErrorAdd( &ui->errorList, "DisplayName", -1, "UGC_Errors.NewCostume_DisplayName_TooShort" );
	}
	if( IsAnyProfane( ui->state.displayName )) {
		MEContextGenericErrorAdd( &ui->errorList, "DisplayName", -1, "UGC_Errors.NewCostume_DisplayName_Profane" );
	}
	if( ugcEditorNewCostumeUINameTaken( ui )) {
		MEContextGenericErrorAdd( &ui->errorList, "Description", -1, "UGC_Errors.NewCostume_DisplayNameAndDescription_NotUnique" );
	}
}

void ugcEditorNewCostumeUIRefresh( UGCNewCostumeUI* ui )
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_Header_Box" );
	MEFieldContextEntry* entry;
	UIPane* pane;
	UIWidget* widget;
	
	ugcEditorNewCostumeUIValidate( ui );

	ui->ignoreChanges = true;
	MEContextPushExternalContext( ui->rootContext, &ui->state, &ui->state, parse_UGCNewCostumeState );
	MEContextSetParent( UI_WIDGET( ui->rootWindow ));
	ui->rootContext->iXPos = 0;
	ui->rootContext->iYPos = 0;
	ui->rootContext->iXDataStart = 100;
	ui->rootContext->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;

	pane = ugcMEContextPushPaneParentWithHeaderMsg( "PropertiesPane", "Header", "UGC_CostumeEditor.NewCostumeProperties", false );
	ui_PaneSetStyle( pane, paneAssembly->pchName, true, false );
	{
		MEContextSetErrorFunction( MEContextGenericErrorMsgFunction );
		MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
		setVec2( MEContextGetCurrent()->iErrorIconOffset, 0, 3 );
		MEContextGetCurrent()->iErrorIconSpaceWidth = atlasFindTexture( "ugc_icons_labels_alert" )->width + 5;
		MEContextSetErrorContext( &ui->errorList );
		
		entry = MEContextAddTextMsg( false, "UGC_CostumeEditor.NewCostumeName_Default", "DisplayName", "UGC_CostumeEditor.NewCostumeName", "UGC_CostumeEditor.NewCostumeName.Tooltip" );
		widget = ENTRY_FIELD( entry )->pUIWidget;
		ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_SINGLE_LINE_MAX_LENGTH );

		entry = MEContextAddTextMsg( false, NULL, "Description", "UGC_CostumeEditor.NewCostumeDescription", "UGC_CostumeEditor.NewCostumeDescription.Tooltip" );
		widget = ENTRY_FIELD( entry )->pUIWidget;
		ui_EditableSetMaxLength( ENTRY_FIELD( entry )->pUIEditable, UGC_TEXT_SINGLE_LINE_MAX_LENGTH );

		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	}
	MEContextPop( "PropertiesPane" );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;

	if( !ui->libraryPicker ) {
		ui->libraryPicker = ugcAssetLibraryPaneCreate( UGCAssetLibrary_GenericWindow, false, NULL, ugcEditorCreateNewCostumeAssetLibCB, ui );
	}
	ugcAssetLibraryPaneSetTagTypeName( ui->libraryPicker, "Costume" );
	widget = UI_WIDGET( ugcAssetLibraryPaneGetUIPane( ui->libraryPicker ));
	ui_WidgetSetPosition( widget, 0, MEContextGetCurrent()->iYPos );
	ui_WidgetSetPaddingEx( widget, 0, 0, 0, UGC_ROW_HEIGHT );
	ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, widget );

	{
		float x = 0;
		entry = MEContextAddButtonMsg( "UGC.Create", NULL, ugcEditorDoCreateNewCostume, ui, "CreateButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_SetActive( widget, eaSize( &ui->errorList.eaErrors ) == 0 );
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_DialogDefault", widget->hOverrideSkin );
		ui_WidgetSetPositionEx( widget, x, 0, 0, 0, UIBottomRight );
		ui_WidgetSetWidth( widget, 80 );
		x = ui_WidgetGetNextX( widget );

		entry = MEContextAddButtonMsg( "UGC.Cancel", NULL, ugcEditorCancelCreateNewCostume, ui, "CancelButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, x, 0, 0, 0, UIBottomRight );
		ui_WidgetSetWidth( widget, 80 );
		x = ui_WidgetGetNextX( widget );
	}

	MEContextPopExternalContext( ui->rootContext );
	ui->ignoreChanges = false;
}

static void ugcEditorNewCostumeUIMEFieldChangedCB( MEField *pField, bool bFinished, UGCNewCostumeUI* ui )
{
	if( ui->ignoreChanges ) {
		return;
	}
	
	if( bFinished ) {
		ugcEditorNewCostumeUIRefresh( ui );
	}
}



void ugcEditorShowCreateNewCostumeWindow(void)
{
	UGCNewCostumeUI *ui = calloc(1, sizeof(UGCNewCostumeUI));

	{
		int w = g_ui_State.screenWidth;
		int h = g_ui_State.screenHeight;
		ui->rootWindow = ui_WindowCreate("", 0, 0, 325, 100);
		ui_WidgetSetTextMessage( UI_WIDGET( ui->rootWindow ), "UGC_CostumeEditor.NewCostumeTitle" );
		ui_WidgetSetDimensions( UI_WIDGET( ui->rootWindow ), UGC_LIBRARY_PANE_WIDTH + 10, 610 );
		ui_WidgetSetPosition( UI_WIDGET( ui->rootWindow ), w / 2 + 100, (h - UI_WIDGET( ui->rootWindow )->height) / 2 );
	}
	ui_WindowSetModal( ui->rootWindow, true );
	ui_WindowSetResizable( ui->rootWindow, false );
	ui_WindowSetMovable( ui->rootWindow, false );
	ui->rootWindow->bDimensionsIncludesNonclient = true;
	ui_WindowSetCloseCallback( ui->rootWindow, ugcEditorNewCostumeUIWindowClosedCB, ui );

	ui->rootContext = MEContextCreateExternalContext( "NewCostumeUI" );
	ui->rootContext->cbChanged = ugcEditorNewCostumeUIMEFieldChangedCB;
	ui->rootContext->pChangedData = ui;

	ugcEditorNewCostumeUIRefresh( ui );
	ui_WindowShowEx( ui->rootWindow, true );
	if( ui->defaultFocusWidget ) {
		ui_SetFocus( ui->defaultFocusWidget );
	}
}

typedef bool UGCEditorNewCostumeConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData pResponseData);

void ugcEditorCheckCreateNewCostume(UGCEditorNewCostumeConfirmCB *confirm_cb)
{
	UGCCostume **costumes = ugcEditorGetCostumesList();
	UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_COSTUME, 0);
	if (budget && (eaSize(&costumes)+1) > budget->iHardLimit)
	{
		ugcModalDialogMsg( "UGC.Error", "UGC_CostumeEditor.NewCostumeError_TooManyCostumes", UIOk );
		return;
	}
	if (budget && (eaSize(&costumes)+1) > budget->iSoftLimit)
	{
		if( ugcModalDialogMsg( "UGC.Warning", "UGC_CostumeEditor.NewCostumeError_TooManyCostumesWarning", UIYes | UINo ) != UIYes ) {
			return;
		}
	}
	confirm_cb(NULL, kUIDialogButton_Yes, NULL);
}

static bool ugcEditorCreateNewCostumeConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData pResponseData)
{
	if (eButton == kUIDialogButton_Yes)
	{
		ugcEditorShowCreateNewCostumeWindow();
	}
	return true;
}

void ugcEditorCreateNewCostume(UIWidget* ignored, UserData ignored2)
{
	ugcEditorCheckCreateNewCostume(ugcEditorCreateNewCostumeConfirmCB);
}

static bool ugcEditorCostumeDisplayNameTaken(const char *new_name)
{
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->costumes, UGCCostume, costume)
	{
		if (stricmp(new_name, costume->pcDisplayName) == 0)
		{
			return true;
		}
	}
	FOR_EACH_END;
	return false;
}

static bool ugcEditorDuplicateCostumeConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData pResponseData)
{
	INIT_PROJECT;
	if (eButton == kUIDialogButton_Yes)
	{
		char new_name[256];
		char *delim;
		int new_idx = 1;
		char *estrTrimmedName = NULL;
		UGCCostumeEditorDoc *costume_to_duplicate = (g_UGCEditorDoc->activeView.mode == UGC_VIEW_COSTUME) ? ugcEditorGetCostumeDoc(g_UGCEditorDoc->activeView.name) : NULL;

		if (!costume_to_duplicate)
			return true;

		if (!project)
			return true;

		strcpy(new_name, costume_to_duplicate->pUGCCostume->pcDisplayName);

		while (ugcEditorCostumeDisplayNameTaken(new_name))
		{
			delim = strrchr(new_name, '#');
			if (delim)
				*delim = '\0';
			strcatf(new_name, "#%d", new_idx++);
		}
		// Trim the name
		if (new_name)
		{
			estrCopy2(&estrTrimmedName, new_name);
			estrTrimLeadingAndTrailingWhitespace(&estrTrimmedName);
		}

		// Create the costume
		{
			UGCCostume *new_costume = StructClone( parse_UGCCostume, costume_to_duplicate->pUGCCostume );
			char res_full_name[RESOURCE_NAME_MAX_SIZE];
			char res_filename[MAX_PATH];
			char *res_name = NULL;

			ugcEditorCreateResourceName(&res_name, ugcEditorGetCostumeByName);
			sprintf(res_full_name, "%s:%s", g_UGCEditorDoc->data->ns_name, res_name);
			sprintf(res_filename, "ns/%s/UGC/%s.ugccostume", g_UGCEditorDoc->data->ns_name, res_name);
			new_costume->astrName = allocAddString(res_full_name);
			new_costume->fstrFilename = allocAddFilename(res_filename);
			new_costume->pcDisplayName = StructAllocString(new_name);

			eaPush(&g_UGCEditorDoc->data->costumes, new_costume);

			estrDestroy(&res_name);

			ugcEditorEditCostume(NULL, new_costume->astrName);
			ugcEditorApplyUpdate(); // Add to undo stack

			// Notify of new costume so it gets added to search results
			ugcEditorUserResourceChanged();
		}

		estrDestroy(&estrTrimmedName);
	}
	return true;
}

void ugcEditorDuplicateCostume(void)
{
	UGCCostumeEditorDoc *costume_to_duplicate = (g_UGCEditorDoc->activeView.mode == UGC_VIEW_COSTUME) ? ugcEditorGetCostumeDoc(g_UGCEditorDoc->activeView.name) : NULL;
	if (!costume_to_duplicate)
		return;

	ugcEditorCheckCreateNewCostume(ugcEditorDuplicateCostumeConfirmCB);
}

void ugcEditorDeleteCostume(void)
{
	if (UIYes != ugcModalDialogMsg("UGC_Editor.DeleteCostume", "UGC_Editor.DeleteCostumeDetails", UIYes | UINo)) {
		return;
	} 
	
	if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_COSTUME)
	{
		// Close the relevant editor
		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, doc)
		{
			if (ugcCostumeEditor_GetCostume(doc)->astrName == g_UGCEditorDoc->activeView.name)
			{
				ugcCostumeEditor_Close(doc);
				eaRemove(&g_UGCEditorDoc->costume_editors, FOR_EACH_IDX(g_UGCEditorDoc->costume_editors, doc));
				break;
			}
		}
		FOR_EACH_END;

		// Remove the costume from the project
		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->costumes, UGCCostume, costume)
		{
			if (costume->astrName == g_UGCEditorDoc->activeView.name)
			{
				eaRemove(&g_UGCEditorDoc->data->costumes, FOR_EACH_IDX(g_UGCEditorDoc->data->costumes, costume));
				StructDestroy(parse_UGCCostume, costume);
				ugcEditorUserResourceChanged();
				break;
			}
		}
		FOR_EACH_END;

		// Create undo op
		ugcEditorApplyUpdate();
	}
}

//// UGCItem functions

UGCItemEditorDoc* ugcEditorGetOrCreateItemDoc(void)
{
	if( g_UGCEditorDoc->item_editor ) {
		return g_UGCEditorDoc->item_editor;
	}
	if (g_UGCEditorDoc->data) {
		g_UGCEditorDoc->item_editor = ugcItemEditorCreate();
		return g_UGCEditorDoc->item_editor;
	}

	return NULL;
}

static void ugcEditorEditItem(UIButton *button, const char *item_name)
{
	INIT_PROJECT;
	ugcEditorSetActiveDoc(UGC_VIEW_ITEM, NULL);
	ugcEditorRefreshGlobalUI();
}

static void ugcEditorCreateItem(UGCProjectInfo *project, const char *new_name)
{
	char *res_name = NULL;
	UGCItem *new_item;

	ugcEditorCreateResourceName(&res_name, ugcEditorGetItemByName);
	new_item = StructCreate(parse_UGCItem);
	new_item->astrName = allocAddString(res_name);
	new_item->strDisplayName = StructAllocString(new_name);

	eaPush(&g_UGCEditorDoc->data->items, new_item);

	estrDestroy(&res_name);

	ugcEditorEditItem(NULL, new_item->astrName);

	ugcEditorApplyUpdate(); // Add to undo stack

	// Notify of new costume so it gets added to search results
	ugcEditorUserResourceChanged();
}

typedef struct UGCNewItemUI
{
	UIWindow *win;
	UITextEntry *name_entry;
} UGCNewItemUI;

static void ugcEditorCancelCreateNewItem(UIButton *button, UGCNewItemUI *ui)
{
	ui_WidgetQueueFree(UI_WIDGET(ui->win));
	SAFE_FREE(ui);
}

static void ugcEditorDoCreateNewItem(UIButton *button, UGCNewItemUI *ui)
{
	INIT_PROJECT;
	const char *new_name = ui_TextEntryGetText(ui->name_entry);
	char *estrTrimmedName = NULL;
	CharClassTypes eRegion = 0;

	if (!project)
		return;
	
	ui_SetFocus(NULL);

	// Trim the name
	if (new_name)
	{
		estrCopy2(&estrTrimmedName, new_name);
		estrTrimLeadingAndTrailingWhitespace(&estrTrimmedName);
	}

	// Complain if no actual name
	if (!new_name)
	{
		ui_DialogPopup("Error", "Must specify a costume name.");
		return;
	}

	if (estrTrimmedName)
	{
		ANALYSIS_ASSUME(estrTrimmedName);
		if (strlen(estrTrimmedName) < 1)
		{
			ui_DialogPopup("Error", "Must specify a costume name.");
			return;
		}
	}

	// Look for duplicate name
	FOR_EACH_IN_EARRAY(g_UGCEditorDoc->data->items, UGCItem, item)
	{
		if (stricmp(estrTrimmedName, item->strDisplayName) == 0)
		{
			ui_DialogPopup("Error", "Item with this name already exists in project.");
			return;
		}
	}
	FOR_EACH_END;

	// Create the item
	ugcEditorCreateItem(project, estrTrimmedName);
	ugcEditorCancelCreateNewItem(button, ui);

	estrDestroy(&estrTrimmedName);
}

// Wrapper function for handling text entry "enter" to call the same function as the button
static void ugcEditorDoCreateNewItem2(UITextEntry *entry, UGCNewItemUI *ui)
{
	ugcEditorDoCreateNewItem(NULL, ui);
}

void ugcEditorShowCreateNewItemWindow(void)
{
	UGCNewItemUI *ui = calloc(1, sizeof( *ui ));
	int x, y;
	UILabel *label;
	UIButton *button;

	ui->win = ui_WindowCreate("", 0, 0, 325, 100);
	ui_WidgetSetTextMessage( UI_WIDGET( ui->win ), "UGC_ItemEditor.NewItemTitle" );
	elUICenterWindow(ui->win);
	ui_WindowSetModal(ui->win, true);
	ui_WindowSetResizable(ui->win, false);

	label = ui_LabelCreate("", 5, 5);
	ui_WidgetSetTextMessage( UI_WIDGET( label ), "UGC_ItemEditor.NewItemName" );
	ui_LabelResize( label );
	ui_WidgetSetTooltipMessage( UI_WIDGET( label ), "UGC_ItemEditor.NewItemName.Tooltip" );
	ui_WindowAddChild(ui->win, UI_WIDGET(label));
	x = elUINextX(label);
	y = elUINextY(label);
	
	ui->name_entry = ui_TextEntryCreate("", x+5, 5);
	ui_WidgetSetDimensionsEx(UI_WIDGET(ui->name_entry), 1, 24, UIUnitPercentage, UIUnitFixed);
	ui_EditableSetDefaultMessage(UI_EDITABLE(ui->name_entry), "UGC_ItemEditor.NewItemName_Default");
	ui_EditableSetMaxLength(UI_EDITABLE(ui->name_entry), 40);
	ui_TextEntrySetEnterCallback(ui->name_entry, ugcEditorDoCreateNewItem2, ui);
	ui_TextEntrySetSimpleOnly(ui->name_entry);
	ui_WindowAddChild(ui->win, UI_WIDGET(ui->name_entry));

	x = 0;
	button = ui_ButtonCreate("", 0, 0, ugcEditorDoCreateNewItem, ui);
	ui_WidgetSetTextMessage( UI_WIDGET( button ), "UGC.Create" );
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WidgetSetWidth(UI_WIDGET(button), 80);
	ui_WindowAddChild(ui->win, UI_WIDGET(button));
	x = ui_WidgetGetNextX( UI_WIDGET( button ));

	button = ui_ButtonCreate("", 0, 0, ugcEditorCancelCreateNewItem, ui);
	ui_WidgetSetTextMessage( UI_WIDGET( button ), "UGC.Cancel" );
	ui_WidgetSetWidth(UI_WIDGET(button), 80);
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WindowAddChild(ui->win, UI_WIDGET(button));

	ui_WindowShowEx(ui->win, true);
	ui_SetFocus(ui->name_entry);
}

typedef bool UGCEditorNewItemConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData pResponseData);

void ugcEditorCheckCreateNewItem(UGCEditorNewItemConfirmCB *confirm_cb)
{
	UGCItem **items = ugcEditorGetItemsList();
	UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_ITEM, 0);
	if (budget && (eaSize(&items)+1) > budget->iHardLimit)
	{
		ugcModalDialogMsg( "UGC.Error", "UGC_ItemEditor.NewItemError_TooManyItems", UIOk );
		return;
	}
	if (budget && (eaSize(&items)+1) > budget->iSoftLimit)
	{
		if( ugcModalDialogMsg( "UGC.Warning", "UGC_ItemEditor.NewItemError_TooManyItemsWarning", UIYes | UINo ) != UIYes ) {
			return;
		}
	}
	confirm_cb(NULL, kUIDialogButton_Yes, NULL);
}

static bool ugcEditorCreateNewItemConfirmCB(UIDialog *pDialog, UIDialogButton eButton, UserData pResponseData)
{
	if (eButton == kUIDialogButton_Yes)
	{
		ugcEditorShowCreateNewItemWindow();
	}
	return true;
}

void ugcEditorCreateNewItem(UIWidget* ignored, UserData ignored2 )
{
	if( !ugcDefaultsIsItemEditorEnabled() ) {
		return;
	}

	ugcEditorCheckCreateNewItem(ugcEditorCreateNewItemConfirmCB);
}

//// Window visibility

void ugcEditorSetDocPane( UIPane* pane )
{
	eaClearEx( &UI_WIDGET( g_UGCEditorDoc->pDocPane )->children, ui_WidgetRemoveFromGroup );
	if( pane ) {
		ui_WidgetAddChild( UI_WIDGET( g_UGCEditorDoc->pDocPane ), UI_WIDGET( pane ));
		ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	}
}

void ugcEditorUpdateWindowVisibility()
{
	bool found = false;
	
	ugcEditorMenusRefresh(g_UGCEditorDoc->menus, g_ui_State.bInUGCEditor, g_UGCEditorDoc->activeView.mode);

	ugcEditorSetDocPane( NULL );
	
	if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_MISSION && g_UGCEditorDoc->mission_editor) {
		ugcMissionSetVisible(g_UGCEditorDoc->mission_editor);
	} else if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_MAP_EDITOR) {
		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, doc) {
			if (ugcMapEditorGetName(doc) == g_UGCEditorDoc->activeView.name) {
				ugcMapEditorSetVisible(doc);
				break;
			}
		} FOR_EACH_END;
	} else if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_MAPS ) {
		ugcNoMapsEditorSetVisible( g_UGCEditorDoc->no_maps_editor );
	} else if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_COSTUME) {
		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, editor) {
			if (ugcCostumeEditor_GetCostume(editor)->astrName == g_UGCEditorDoc->activeView.name) {
				ugcCostumeEditor_SetVisible(editor);
				break;
			}
		} FOR_EACH_END;
	} else if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_COSTUMES ) {
		ugcNoCostumesEditor_SetVisible( g_UGCEditorDoc->no_costumes_editor );
	} else if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_ITEM) {
		ugcItemEditorSetVisible(g_UGCEditorDoc->item_editor);
	} else if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_DIALOG_TREE) {
		FOR_EACH_IN_EARRAY(g_UGCEditorDoc->dialog_editors, UGCDialogTreeDoc, editor) {
			if (ugcDialogTreeDocGetIDAsPtr(editor) == g_UGCEditorDoc->activeView.name) {
				ugcDialogTreeDocSetVisible(editor);
				break;
			}
		} FOR_EACH_END;
	} else if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_DIALOG_TREES ) {
		ugcNoDialogTreesDocSetVisible( g_UGCEditorDoc->no_dialogs_editor );
	} else if (g_UGCEditorDoc->activeView.mode == UGC_VIEW_PROJECT_INFO && g_UGCEditorDoc->project_editor) {
		ugcProjectEditor_SetVisible(g_UGCEditorDoc->project_editor);
	}

	if (g_ui_State.bInUGCEditor) {
		ui_WidgetAddToDevice(UI_WIDGET(g_UGCEditorDoc->pToolbarPane), NULL);
		ui_WidgetAddToDevice(UI_WIDGET(g_UGCEditorDoc->pDocPane), NULL);
		ui_WidgetAddToDevice(UI_WIDGET(g_UGCEditorDoc->pWindowManagerPane), NULL);
	} else {
		ui_WidgetRemoveFromGroup(UI_WIDGET(g_UGCEditorDoc->pToolbarPane));
		ui_WidgetRemoveFromGroup(UI_WIDGET(g_UGCEditorDoc->pDocPane));
		ui_WidgetRemoveFromGroup(UI_WIDGET(g_UGCEditorDoc->pWindowManagerPane));
	}
}

void UGCEditorDoAutosaveDeletionCompleted(int iAutosaveType)
{
	if (iAutosaveType == UGC_EDITOR_QUIT)
	{
		ugcEditorLoadData(NULL);
		utilitiesLibSetShouldQuit(true);
	}
	else if (iAutosaveType == UGC_EDITOR_CLOSE_EDITOR)
	{
		ugcEditorLoadData(NULL);
		ClientGoToCharacterSelect();
	}
	else if (iAutosaveType == UGC_EDITOR_CLOSE_PROJECT)
	{
		ugcEditorLoadData(NULL);
		ClientGoToCharacterSelectAndChoosePreviousForUGC();
	}
}

//// Save project

void UGCEditorDoUpdateSaveStatus(bool succeeded, const char *error)
{
	if (g_UGCEditorDoc->modal_dialog && g_UGCEditorDoc->data && g_UGCEditorDoc->last_save_data)
	{
		ui_WidgetQueueFreeAndNull(&g_UGCEditorDoc->modal_dialog);
		if (succeeded)
		{
			StructCopy(parse_UGCProjectData, g_UGCEditorDoc->data, g_UGCEditorDoc->last_save_data, 0, 0, 0);
			assert(g_UGCEditorDoc->last_save_data);
			printf("Project save completed.\n");

			ugcEditorRefreshGlobalUI();

			if (g_UGCEditorDoc->bQueueClose)
			{
				ugcEditorLoadData(NULL);

				if (UGC_EDITOR_QUIT == g_UGCEditorDoc->bQueueClose)
					utilitiesLibSetShouldQuit(true);
				else if (UGC_EDITOR_CLOSE_EDITOR == g_UGCEditorDoc->bQueueClose)
					ClientGoToCharacterSelect();
				else if (UGC_EDITOR_CLOSE_PROJECT == g_UGCEditorDoc->bQueueClose)
					ClientGoToCharacterSelectAndChoosePreviousForUGC();

				g_UGCEditorDoc->bQueueClose = UGC_EDITOR_CLOSE_NONE;
			}
		}
		else
		{
			char* estr = NULL;
			ugcFormatMessageKey( &estr, "UGC_Editor.SaveError",
								 STRFMT_STRING( "Error", error ),
								 STRFMT_END );
			ugcModalDialog( TranslateMessageKey( "UGC.Error" ), estr, UIOk );
			estrDestroy( &estr );
		}
	}
	if (g_UGCEditorDoc->modal_timeout)
		TimedCallback_Remove(g_UGCEditorDoc->modal_timeout);
	g_UGCEditorDoc->modal_timeout = NULL;
	g_UGCEditorDoc->bQueueClose = UGC_EDITOR_CLOSE_NONE;
}

static void UGCEditorSaveStatusTimeout(TimedCallback *callback, F32 timeSinceLastCallback, UserData userData)
{
	UGCEditorDoUpdateSaveStatus(false, TranslateMessageKey( "UGC_Editor.SaveError_SaveTimedOut" ));
}

void ugcEditorSave()
{
	UILabel* label;
	if (!g_UGCEditorDoc || !g_UGCEditorDoc->data || !g_UGCEditorDoc->data->project)
		return;

	// Clear autosave
	if (g_UGCEditorDoc->autosave_timer)
	{
		timerFree(g_UGCEditorDoc->autosave_timer);
		g_UGCEditorDoc->autosave_timer = 0;
	}

	EditUndoStackClear(g_UGCEditorDoc->edit_undo_stack);

	g_UGCEditorDoc->modal_dialog = ui_WindowCreate( "", 0, 0, 200, 50);
	ui_WidgetSetTextMessage( UI_WIDGET( g_UGCEditorDoc->modal_dialog ), "UGC_Editor.SavingTitle" );
	label = ui_LabelCreate("", 0, 0);
	ui_LabelSetMessage( label, "UGC_Editor.SavingDetails" );
	ui_WindowAddChild(g_UGCEditorDoc->modal_dialog, label);
	ui_WindowSetClosable(g_UGCEditorDoc->modal_dialog, false);
	ui_WindowSetModal(g_UGCEditorDoc->modal_dialog, true);
	ui_WindowSetResizable(g_UGCEditorDoc->modal_dialog, false);
	elUICenterWindow(g_UGCEditorDoc->modal_dialog);
	ui_WindowShowEx(g_UGCEditorDoc->modal_dialog, true);

	g_UGCEditorDoc->modal_timeout = TimedCallback_Add(UGCEditorSaveStatusTimeout, NULL, UGC_SAVE_STATUS_TIMEOUT);

	ServerCmd_SaveUGCProject(g_UGCEditorDoc->data);
}

//// UI routines

static UGCComponent** ugcEditorGetDialogs( void )
{
	UGCComponent** eaAccum = NULL;
	int componentIt;
	for( componentIt = 0; componentIt != eaSize( &g_UGCEditorDoc->data->components->eaComponents ); ++componentIt ) {
		UGCComponent* component = g_UGCEditorDoc->data->components->eaComponents[ componentIt ];
		if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			continue;
		}

		eaPush( &eaAccum, component );
	}

	return eaAccum;
}

static void ConcatStringFromUGCRuntimeError(UGCRuntimeError* error, char **estrOutput, bool bDumpEntireContext)
{
	if( error->message_key ) {
		const char *tr = TranslateMessageKey( error->message_key );
		if( tr ) {
			estrConcatf( estrOutput, "%s", tr );
		} else {
			estrConcatf( estrOutput, "%s [UNTRANSLATED!]", error->message );
		}
	} else {
		estrConcatf( estrOutput, "%s [NO MESSAGE KEY!]", error->message );
	}

	if (bDumpEntireContext && error)
	{
		ParserWriteText(estrOutput, parse_UGCRuntimeError, error, 0, 0, 0);
	}
}

// This "tree" is actually supposed to look like a list... so peek into the list's skinning.
static void ugcEditorErrorsListRowDrawCB( UITreeNode* pNode, UserData ignored, UI_MY_ARGS, F32 z )
{
	UGCRuntimeErrorOrHeader* errorOrHeader = pNode->contents;
	CBox box;

	BuildCBox( &box, x + 4, y, w - 8, h );
	clipperPushRestrict( &box );
	
	if( errorOrHeader->error ) {
		UGCRuntimeError* error = errorOrHeader->error;
		AtlasTex* alertIcon = atlasFindTexture( "ugc_icons_labels_alert" );
		bool bSelected = ui_TreeIsNodeSelected( pNode->tree, pNode );
		UIStyleFont* pFont = ui_ListItemGetFontFromSkinAndWidget( UI_GET_SKIN( pNode->tree ), UI_WIDGET( pNode->tree ), bSelected, false );

		display_sprite( alertIcon, box.lx, (box.ly + box.hy - alertIcon->height) / 2, z, 1, 1, -1 );
		box.lx += alertIcon->width + 2;

		{
			char* estr = NULL;
			ConcatStringFromUGCRuntimeError( error, &estr, false );
			ui_StyleFontUse( pFont, false, 0 );
			ui_DrawTextInBoxSingleLine( pFont, estr, true, &box, z, scale, UILeft );
			estrDestroy( &estr );
		}
	} else {
		UIStyleFont* pFont = ui_ListHeaderGetFontFromSkinAndWidget( UI_GET_SKIN(pNode->tree ), UI_WIDGET( pNode->tree ));
		AtlasTex* white = atlasFindTexture( "white" );
		UIDrawingDescription desc = { 0 };
		desc.textureAssemblyName = UI_GET_SKIN( pNode->tree )->astrListHeaderStyle;
		//display_sprite_box( white, &box, z, 0x404040FF );
		ui_DrawingDescriptionDraw( &desc, &box, scale, z, 255, ColorBlack, ColorBlack );
		ui_StyleFontUse( pFont, false, 0 );
		ui_DrawTextInBoxSingleLine( pFont, errorOrHeader->strHeaderEditorName, true, &box, z, scale, UINoDirection );
	}

	clipperPop();
}

void errorGetUGCField( const UGCRuntimeError* error, UGCEditorType* editorType, const char** editorName, UGCEditorObjectType* objectType, const char** objectName, int* promptActionIndex, const char** fieldName )
{
	UGCRuntimeErrorContext* context = error->context;
	UGCEditorType defaultEditor;
	UGCEditorObjectType defaultObj;
	int defaultInt;
	const char* defaultStr;
	if( !editorType ) { editorType = &defaultEditor; }
	if( !editorName ) { editorName = &defaultStr; }
	if( !objectType ) { objectType = &defaultObj; }
	if( !objectName ) { objectName = &defaultStr; }
	if( !promptActionIndex ) { promptActionIndex = &defaultInt; }
	if( !fieldName ) { fieldName = &defaultStr; }

	*editorType = UGCEDITOR_NONE;
	*editorName = NULL;
	*objectType = UGCOBJECT_NONE;
	*objectName = NULL;
	*promptActionIndex = 0;
	*fieldName = NULL;
	
	switch( context->scope ) {
		xcase UGC_SCOPE_DEFAULT: case UGC_SCOPE_INTERNAL_CODE:
			*editorType = UGCEDITOR_PROJECT;

		xcase UGC_SCOPE_MAP:
			*editorType = UGCEDITOR_MAP;
			*editorName = allocAddString( context->map_name );
			
		xcase UGC_SCOPE_LAYOUT:
			*editorType = UGCEDITOR_MAP;
			*editorName = allocAddString( context->map_name );
			
		xcase UGC_SCOPE_ROOM:
			*editorType = UGCEDITOR_MAP;
			*editorName = allocAddString( context->map_name );
			*objectType = UGCOBJECT_ROOM;
			*objectName = context->location_name;

		xcase UGC_SCOPE_PATH:
			*editorType = UGCEDITOR_MAP;
			*editorName = allocAddString( context->map_name );
			*objectType = UGCOBJECT_PATH;
			*objectName = context->location_name;

		xcase UGC_SCOPE_CHALLENGE: {
			UGCComponent* component = ugcComponentFindByLogicalName( ugcEditorGetComponentList(), context->challenge_name );
			if (component)
			{
				if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
					*editorType = UGCEDITOR_DIALOG_TREE;
					*editorName = context->challenge_name;
					if( context->prompt_name ) {
						*objectName = context->prompt_name;
					} else {
						*objectName = "Prompt_-1";
					}
					*promptActionIndex = context->prompt_action_index;
				} else {
					bool isForMapEditor = true;

					if(   component->sPlacement.bIsExternalPlacement
						  || stricmp_safe( error->message_key, "UGC.Component_UsedMultipleTimes" ) == 0 ) {
						isForMapEditor = false;
					} else {
						isForMapEditor = true;
					}
					
					if( isForMapEditor ) {
						*editorType = UGCEDITOR_MAP;

						if( !nullStr( component->sPlacement.pcMapName )) {
							char mapName[ RESOURCE_NAME_MAX_SIZE ];
							sprintf( mapName, "%s:%s", ugcEditorGetNamespace(), component->sPlacement.pcMapName );
							*editorName = allocAddString( mapName );
						} else {
							*editorName = NULL;
						}
					
						*objectType = UGCOBJECT_COMPONENT;
						*objectName = context->challenge_name;
					} else {
						*editorType = UGCEDITOR_MISSION;
						*objectType = UGCOBJECT_COMPONENT;
						*objectName = context->challenge_name;
					}
				}
			}
			else
			{
				*editorType = UGCEDITOR_NONE;
			}
		}
			
		xcase UGC_SCOPE_PORTAL:
			*editorType = UGCEDITOR_MAP;
			*editorName = allocAddString( context->map_name );
			*objectType = UGCOBJECT_COMPONENT;
			*objectName = context->portal_name;

		xcase UGC_SCOPE_OBJECTIVE:
			*editorType = UGCEDITOR_MISSION;
			*objectType = UGCOBJECT_OBJECTIVE;
			*objectName = context->objective_name;

		xcase UGC_SCOPE_MAP_TRANSITION:
			if( stricmp( error->field_name, "DoorMapLocation" ) == 0 ) {
				*editorType = UGCEDITOR_MISSION;
				*objectType = UGCOBJECT_MAP;
				*objectName = context->objective_name;
			} else {
				*editorType = UGCEDITOR_MISSION;
				*objectType = UGCOBJECT_MAPTRANSITION;
				*objectName = context->objective_name;
			}

		xcase UGC_SCOPE_MISSION:
			*editorType = UGCEDITOR_MISSION;

		xcase UGC_SCOPE_UGC_ITEM:
			*editorType = UGCEDITOR_ITEM;
			*editorName = allocAddString(context->ugc_item_name);

		xdefault:
			devassert( false );
	}
}

static void ugcEditorErrorEditorName(UGCRuntimeError* error, char **estrOutput)
{
	UGCEditorType editorType;
	const char* editorName;
	UGCEditorObjectType objectType;
	const char* objectName;
	
	errorGetUGCField( error, &editorType, &editorName, &objectType, &objectName, NULL, NULL );

	switch( editorType ) {
		case UGCEDITOR_PROJECT:
			ugcFormatMessageKey( estrOutput, "UGC_ProjectEditor.EditorName", STRFMT_END );
			
		xcase UGCEDITOR_MAP: {
			UGCMap* map = ugcEditorGetMapByName(editorName);
			ugcFormatMessageKey( estrOutput, "UGC_MapEditor.EditorName",
								 STRFMT_STRING( "MapName", ugcMapDisplayName( map )),
								 STRFMT_END );
		}

		xcase UGCEDITOR_MISSION:
			ugcFormatMessageKey( estrOutput, "UGC_MissionEditor.EditorName", STRFMT_END );

		xcase UGCEDITOR_DIALOG_TREE: {
			UGCComponent* component = ugcComponentFindByLogicalName( ugcEditorGetComponentList(), editorName );

			if( !nullStr( component->dialogBlock.initialPrompt.pcPromptBody )) {
				ugcFormatMessageKey( estrOutput, "UGC_DialogTreeEditor.EditorName",
									 STRFMT_STRING( "DialogName", component->dialogBlock.initialPrompt.pcPromptBody ),
									 STRFMT_END );
			} else {
				ugcFormatMessageKey( estrOutput, "UGC_DialogTreeEditor.EditorName",
									 STRFMT_STRING( "DialogName", TranslateMessageKey( TranslateMessageKey( "UGC_DialogTreeEditor.DialogName_Default" ))),
									 STRFMT_END );
			}
		}

		xcase UGCEDITOR_COSTUME: {
			UGCCostume *costume = ugcEditorGetCostumeByName(editorName);
			ugcFormatMessageKey( estrOutput, "UGC_CostumeEditor.EditorName",
								 STRFMT_STRING( "CostumeName", costume->pcDisplayName ),
								 STRFMT_END );
		}

		xcase UGCEDITOR_ITEM: {
			UGCItem *item = ugcEditorGetItemByName(editorName);
			ugcFormatMessageKey( estrOutput, "UGC_ItemEditor.EditorName",
								 STRFMT_STRING( "ItemName", item->strDisplayName ),
								 STRFMT_END );
		}
	}
}

static void navigateTo( UITree* pTree, UserData ignored )
{
	UITreeNode* selectedNode = ui_TreeGetSelected( pTree );
	UGCEditorType editorType;
	const char* editorName;
	UGCEditorObjectType objectType;
	const char* objectName;
	int promptActionIndex;

	UGCRuntimeErrorOrHeader* errorOrHeader = SAFE_MEMBER( selectedNode, contents );
	UGCRuntimeError* error = SAFE_MEMBER( errorOrHeader, error);
	if( !error ) {
		return;
	}	
	errorGetUGCField( error, &editorType, &editorName, &objectType, &objectName, &promptActionIndex, NULL );

	switch( editorType ) {
		case UGCEDITOR_PROJECT:
			ugcEditorSetActiveDoc(UGC_VIEW_PROJECT_INFO, NULL);
			ugcEditorRefreshGlobalUI();
			
		xcase UGCEDITOR_MAP:
			if( editorName ) {
				switch( objectType ) {
					xcase UGCOBJECT_COMPONENT:
					{
						UGCComponent *component = ugcComponentFindByLogicalName(ugcEditorGetComponentList(), objectName);
						if (component)
							ugcEditorEditMapComponent( editorName, component->uID, false, true );
					}
xdefault:
					ugcEditorEditMap( NULL, editorName );
				}
			}

		xcase UGCEDITOR_MISSION:
			switch( objectType ) {
				case UGCOBJECT_OBJECTIVE:
					ugcEditorEditMissionObjective( objectName );

				xcase UGCOBJECT_COMPONENT:
					ugcEditorEditMissionComponent( objectName );

				xcase UGCOBJECT_DIALOG_TREE: {
					UGCComponent* component = ugcComponentFindByLogicalName( ugcEditorGetComponentList(), objectName );
					if( component ) {
						ugcEditorEditMissionDialogTreeBlock( component->uID );
					}
				}
				
				xcase UGCOBJECT_MAPTRANSITION: {
					UGCMissionObjective* objective = ugcObjectiveFindByLogicalName( ugcEditorGetProjectData()->mission->objectives, objectName );
					if( objective ) {
						ugcEditorEditMissionMapTransition( objective->id );
					}
				}

				xcase UGCOBJECT_MAP: {
					UGCMissionObjective* objective = ugcObjectiveFindByLogicalName( ugcEditorGetProjectData()->mission->objectives, objectName );
					if( objective ) {
						ugcEditorEditMissionMap( objective->id );
					}
				}

				xdefault:
					ugcEditorEditMission();
					
			}

		xcase UGCEDITOR_DIALOG_TREE: {
			UGCComponent* component = ugcComponentFindByLogicalName( ugcEditorGetComponentList(), editorName );
			int promptID = 0;

			if( strStartsWith( objectName, "Prompt_" )) {
				promptID = atoi( objectName + 7 );
			}

			if( component ) {
				ugcEditorEditDialogTreeBlock( component->uID, promptID, promptActionIndex );
			}
		}

		xcase UGCEDITOR_COSTUME:
			ugcEditorEditCostume( NULL, editorName );

		xcase UGCEDITOR_ITEM:
			ugcEditorEditItem( NULL, editorName );
	}
}

int ugcEditorSortErrorOrHeader( const UGCRuntimeErrorOrHeader** ppRow1, const UGCRuntimeErrorOrHeader** ppRow2, const void* ignored )
{
	UGCEditorType editorType1;
	const char* editorName1;
	UGCEditorType editorType2;
	const char* editorName2;

	assert( (*ppRow1)->error && (*ppRow2)->error );
	
	errorGetUGCField( (*ppRow1)->error, &editorType1, &editorName1, NULL, NULL, NULL, NULL );
	errorGetUGCField( (*ppRow2)->error, &editorType2, &editorName2, NULL, NULL, NULL, NULL );

	if( editorType1 != editorType2 ) {
		return editorType1 - editorType2;
	}
	if( editorName1 != editorName2 ) {
		return editorName1 - editorName2;
	}
	return 0;
}

//forward declarations for ugcEditorRefreshGlobalUI helpers
static void ugcEditorRefreshRootUI(void);
static void ugcEditorRefreshToolbarPane(void);
static void ugcEditorRefreshWindowManagerPane(void);

void ugcEditorRefreshGlobalUI()
{
	INIT_PROJECT;

	assert(g_UGCEditorDoc);

	g_UGCEditorDoc->last_project_info = project;
	ugcEditorRefreshRootUI();
	
	if (project)
	{
		// Other Root UI stuff
		ugcEditorRefreshToolbarPane();
		ugcEditorRefreshWindowManagerPane();
	}

	// Refresh after edge panes have a chance to size themselves
	ui_WidgetSetPaddingEx( UI_WIDGET( g_UGCEditorDoc->pDocPane ), 0, 0,
						   ui_WidgetGetNextY( UI_WIDGET( g_UGCEditorDoc->pToolbarPane )),
						   ui_WidgetGetNextY( UI_WIDGET( g_UGCEditorDoc->pWindowManagerPane )));

	ugcEditorSetActiveDoc( g_UGCEditorDoc->activeView.mode, g_UGCEditorDoc->activeView.name );
}

static void ugcEditorRefreshRootUI(void)
{
	if (!g_UGCEditorDoc->menus)
		g_UGCEditorDoc->menus = ugcEditorCreateMenus();

	if( !g_UGCEditorDoc->pToolbarPane ) {
		g_UGCEditorDoc->pToolbarPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetSetPositionEx( UI_WIDGET( g_UGCEditorDoc->pToolbarPane ), 0, ugcEditorGetMenuHeight(g_UGCEditorDoc->menus), 0, 0, UITopLeft );
	ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCEditorDoc->pToolbarPane ), 1, 1, UIUnitPercentage, UIUnitFixed );
	ui_PaneSetStyle( g_UGCEditorDoc->pToolbarPane, "UGC_Pane_Dark", true, false );
	UI_WIDGET( g_UGCEditorDoc->pToolbarPane )->priority = 1;

	if( !g_UGCEditorDoc->pWindowManagerPane ) {
		g_UGCEditorDoc->pWindowManagerPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetSetPositionEx( UI_WIDGET( g_UGCEditorDoc->pWindowManagerPane ), 0, 0, 0, 0, UIBottomLeft );
	ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCEditorDoc->pWindowManagerPane ), 1, UGC_WINDOW_MANAGER_HEIGHT, UIUnitPercentage, UIUnitFixed );
		
	if (!g_UGCEditorDoc->pDocPane)
	{
		g_UGCEditorDoc->pDocPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		g_UGCEditorDoc->pDocPane->invisible = true;
	}
	ui_WidgetSetPosition( UI_WIDGET( g_UGCEditorDoc->pDocPane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCEditorDoc->pDocPane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
}

static void ugcEditorNavigateToProjectCB( UIButton* ignored, UserData ignored2 )
{
	ugcEditorSetActiveDoc( UGC_VIEW_PROJECT_INFO, NULL );
	ugcEditorRefreshGlobalUI();
}

static void ugcEditorNavigateToStoryCB( UIButton* ignored, UserData ignored2 )
{
	ugcEditorSetActiveDoc( UGC_VIEW_MISSION, NULL );
	ugcEditorRefreshGlobalUI();
}

static void ugcEditorNavigateToMapsCB( UIButton* ignored, UserData ignored2 )
{
	if( g_UGCEditorDoc->strLastActiveMap ) {
		ugcEditorSetActiveDoc( UGC_VIEW_MAP_EDITOR, g_UGCEditorDoc->strLastActiveMap );
	} else {
		ugcEditorSetActiveDoc( UGC_VIEW_NO_MAPS, NULL );
	}
	ugcEditorRefreshGlobalUI();
}

static void ugcEditorNavigateToDialogsCB( UIButton* ignored, UserData ignored2 )
{
	if( g_UGCEditorDoc->strLastActiveDialogTree ) {
		ugcEditorSetActiveDoc( UGC_VIEW_DIALOG_TREE, g_UGCEditorDoc->strLastActiveDialogTree );
	} else {
		ugcEditorSetActiveDoc( UGC_VIEW_NO_DIALOG_TREES, NULL );
	}
	ugcEditorRefreshGlobalUI();
}

static void ugcEditorNavigateToCostumesCB( UIButton* ignored, UserData ignored2 )
{
	if( g_UGCEditorDoc->strLastActiveCostume ) {
		ugcEditorSetActiveDoc( UGC_VIEW_COSTUME, g_UGCEditorDoc->strLastActiveCostume );
	} else {
		ugcEditorSetActiveDoc( UGC_VIEW_NO_COSTUMES, NULL );
	}
	ugcEditorRefreshGlobalUI();
}

static void ugcEditorNavigateToItemsCB( UIButton* ignored, UserData ignored2 )
{
	ugcEditorSetActiveDoc( UGC_VIEW_ITEM, NULL );
	ugcEditorRefreshGlobalUI();
}

void ugcEditorPopupChooserMapsCB( UIButton* ignored, UserData ignored2 )
{
	ui_MenuPopupAtCursorOrWidgetBox( g_UGCEditorDoc->pMapsListMenu );
}

void ugcEditorPopupChooserDialogsCB( UIButton* ignored, UserData ignored2 )
{
	ui_MenuPopupAtCursorOrWidgetBox( g_UGCEditorDoc->pDialogsListMenu );
}

void ugcEditorPopupChooserCostumesCB( UIButton* ignored, UserData ignored2 )
{
	ui_MenuPopupAtCursorOrWidgetBox( g_UGCEditorDoc->pCostumesListMenu );
}

void ugcEditorPopupChooserItemsCB( UIButton* ignored, UserData ignored2 )
{
	ui_MenuPopupAtCursorOrWidgetBox( g_UGCEditorDoc->pItemsListMenu );
}

static int ugcSortMapsByDisplayName( const UGCMap** ppMap1, const UGCMap** ppMap2 )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	return stricmp_safe( ugcMapGetDisplayName( ugcProj, (*ppMap1)->pcName ),
						 ugcMapGetDisplayName( ugcProj, (*ppMap2)->pcName ));
}

static int ugcSortDialogsByBodyText( const UGCComponent** ppDialog1, const UGCComponent** ppDialog2 )
{
	return stricmp_safe( (*ppDialog1)->dialogBlock.initialPrompt.pcPromptBody,
						 (*ppDialog2)->dialogBlock.initialPrompt.pcPromptBody );
}

static int ugcSortCostumesByDisplayName( const UGCCostume** ppCostume1, const UGCCostume** ppCostume2 )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	return stricmp_safe( ugcCostumeSpecifierGetDisplayName( ugcProj, (*ppCostume1)->astrName ),
						 ugcCostumeSpecifierGetDisplayName( ugcProj, (*ppCostume2)->astrName ));
}

static int ugcSortItemsByDisplayName( const UGCItem** ppItem1, const UGCItem** ppItem2 )
{
	return stricmp_safe( ugcItemSpecifierGetDisplayName( (*ppItem1)->astrName ),
						 ugcItemSpecifierGetDisplayName( (*ppItem2)->astrName ));
}

void ugcEditorRefreshToolbarPane(void)
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	MEFieldContext* uiCtx = MEContextPush( "UGCRoot_Toolbar", NULL, NULL, NULL );
	MEFieldContextEntry* entry;
	UIWidget* widget;
	float x = 0;
	float y = 8;
	float tabWidth = 1.0/6;
	float tabBottomAccum;
	
	MEContextSetParent( UI_WIDGET( g_UGCEditorDoc->pToolbarPane ));
	uiCtx->astrOverrideSkinName = allocAddString( "UGCToolbar" );

	entry = MEContextAddButtonMsg( "UGC_Toolbar.Project_Tab", NULL, ugcEditorNavigateToProjectCB, NULL, "Project_Tab", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ENTRY_BUTTON( entry )->bDrawBorderOutsideRect = true;
	ui_WidgetSetPositionEx( widget, 0, y, x, 0, UITopLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetWidthEx( widget, tabWidth, UIUnitPercentage );
	if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_PROJECT_INFO ) {
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Major_Active", widget->hOverrideSkin );
	}
	tabBottomAccum = ui_WidgetGetNextY( widget );
	
	entry = ugcMEContextAddErrorSpriteForEditor( UGCEDITOR_PROJECT, "Project_Error" );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetSetPositionEx( widget, 4, 2, x, 0, UITopLeft );
	x += tabWidth;	

	entry = MEContextAddButtonMsg( "UGC_Toolbar.Mission_Tab", NULL, ugcEditorNavigateToStoryCB, NULL, "Mission_Tab", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ENTRY_BUTTON( entry )->bDrawBorderOutsideRect = true;
	ui_WidgetSetPositionEx( widget, 0, y, x, 0, UITopLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetWidthEx( widget, tabWidth, UIUnitPercentage );
	if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_MISSION ) {
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Major_Active", widget->hOverrideSkin );
	}
	
	entry = ugcMEContextAddErrorSpriteForEditor( UGCEDITOR_MISSION, "Mission_Error" );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetSetPositionEx( widget, 4, 2, x, 0, UITopLeft );
	x += tabWidth;

	entry = MEContextAddButtonMsg( "UGC_Toolbar.Maps_Tab", NULL, ugcEditorNavigateToMapsCB, NULL, "Maps_Tab", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ENTRY_BUTTON( entry )->bDrawBorderOutsideRect = true;
	ui_WidgetSetPositionEx( widget, 0, y, x, 0, UITopLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetWidthEx( widget, tabWidth, UIUnitPercentage );
	if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_MAP_EDITOR || g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_MAPS ) {
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Major_Active", widget->hOverrideSkin );
	}

	entry = ugcMEContextAddErrorSpriteForEditor( UGCEDITOR_MAP, "Maps_Error" );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetSetPositionEx( widget, 4, 2, x, 0, UITopLeft );
	x += tabWidth;

	entry = MEContextAddButtonMsg( "UGC_Toolbar.Dialogs_Tab", NULL, ugcEditorNavigateToDialogsCB, NULL, "Dialogs_Tab", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ENTRY_BUTTON( entry )->bDrawBorderOutsideRect = true;
	ui_WidgetSetPositionEx( widget, 0, y, x, 0, UITopLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetWidthEx( widget, tabWidth, UIUnitPercentage );
	if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_DIALOG_TREE || g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_DIALOG_TREES ) {
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Major_Active", widget->hOverrideSkin );
	}

	entry = ugcMEContextAddErrorSpriteForEditor( UGCEDITOR_DIALOG_TREE, "Dialogs_Error" );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetSetPositionEx( widget, 4, 2, x, 0, UITopLeft );
	x += tabWidth;
	
	entry = MEContextAddButtonMsg( "UGC_Toolbar.Costumes_Tab", NULL, ugcEditorNavigateToCostumesCB, NULL, "Costumes_Tab", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ENTRY_BUTTON( entry )->bDrawBorderOutsideRect = true;
	ui_WidgetSetPositionEx( widget, 0, y, x, 0, UITopLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetWidthEx( widget, tabWidth, UIUnitPercentage );
	if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_COSTUME || g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_COSTUMES ) {
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Major_Active", widget->hOverrideSkin );
	}

	entry = ugcMEContextAddErrorSpriteForEditor( UGCEDITOR_COSTUME, "Costumes_Error" );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetSetPositionEx( widget, 4, 2, x, 0, UITopLeft );
	x += tabWidth;
	
	entry = MEContextAddButtonMsg( "UGC_Toolbar.Items_Tab", NULL, ugcEditorNavigateToItemsCB, NULL, "Items_Tab", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ENTRY_BUTTON( entry )->bDrawBorderOutsideRect = true;
	ui_WidgetSetPositionEx( widget, 0, y, x, 0, UITopLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetWidthEx( widget, tabWidth, UIUnitPercentage );
	if( g_UGCEditorDoc->activeView.mode == UGC_VIEW_ITEM ) {
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Major_Active", widget->hOverrideSkin );
	}

	entry = ugcMEContextAddErrorSpriteForEditor( UGCEDITOR_ITEM, "Item_Error" );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetSetPositionEx( widget, 4, 2, x, 0, UITopLeft );
	x += tabWidth;

	MEContextPop( "UGCRoot_Toolbar" );

	// Set the parent's size
	ui_WidgetSetHeight( UI_WIDGET( g_UGCEditorDoc->pToolbarPane ), tabBottomAccum - 2 );

	// Also, reinitialize the popup menus
	{
		UGCMap** eaMapsSortedByName = NULL;
		eaCopy( &eaMapsSortedByName, &ugcProj->maps );
		eaQSort( eaMapsSortedByName, ugcSortMapsByDisplayName );
		
		if( !g_UGCEditorDoc->pMapsListMenu ) {
			g_UGCEditorDoc->pMapsListMenu = ui_MenuCreate( NULL );
		}
		ui_MenuClear( g_UGCEditorDoc->pMapsListMenu );
		if( eaSize( &eaMapsSortedByName )) {
			FOR_EACH_IN_EARRAY_FORWARDS( eaMapsSortedByName, UGCMap, map ) {
				UIMenuItem* mapItem = ui_MenuItemCreate( ugcMapGetDisplayName( ugcProj, map->pcName ), UIMenuCallback, ugcEditorEditMap, (UserData)map->pcName, NULL );
				ui_MenuAppendItem( g_UGCEditorDoc->pMapsListMenu, mapItem );
			} FOR_EACH_END;

			ui_MenuAppendItem( g_UGCEditorDoc->pMapsListMenu, ui_MenuItemCreate( "---", UIMenuSeparator, NULL, NULL, NULL ));
			ui_MenuAppendItem( g_UGCEditorDoc->pMapsListMenu, ugcEditorMenuItemCreate( UGC_ACTION_MAP_CREATE ));
		}
		eaDestroy( &eaMapsSortedByName );
	}

	{
		UGCComponent** eaDialogsSortedByText = ugcEditorGetDialogs();
		eaQSort( eaDialogsSortedByText, ugcSortDialogsByBodyText );
		
		if( !g_UGCEditorDoc->pDialogsListMenu ) {
			g_UGCEditorDoc->pDialogsListMenu = ui_MenuCreate( NULL );
		}
		ui_MenuClear( g_UGCEditorDoc->pDialogsListMenu );
		if( eaSize( &eaDialogsSortedByText )) {
			FOR_EACH_IN_EARRAY_FORWARDS( eaDialogsSortedByText, UGCComponent, dialog ) {
				UIMenuItem* mapItem = ui_MenuItemCreate( dialog->dialogBlock.initialPrompt.pcPromptBody, UIMenuCallback, ugcEditorEditDialogTree, (UserData)dialog, NULL );
				ui_MenuAppendItem( g_UGCEditorDoc->pDialogsListMenu, mapItem );
			} FOR_EACH_END;
		}
		eaDestroy( &eaDialogsSortedByText );
	}

	{
		UGCCostume** eaCostumesSortedByName = NULL;
		eaCopy( &eaCostumesSortedByName, &ugcProj->costumes );
		eaQSort( eaCostumesSortedByName, ugcSortCostumesByDisplayName );
		
		if( !g_UGCEditorDoc->pCostumesListMenu ) {
			g_UGCEditorDoc->pCostumesListMenu = ui_MenuCreate( NULL );
		}
		ui_MenuClear( g_UGCEditorDoc->pCostumesListMenu );
		if( eaSize( &eaCostumesSortedByName )) {
			FOR_EACH_IN_EARRAY_FORWARDS( eaCostumesSortedByName, UGCCostume, costume ) {
				UIMenuItem* mapItem = ui_MenuItemCreate( ugcCostumeSpecifierGetDisplayName( ugcProj, costume->astrName ), UIMenuCallback, ugcEditorEditCostume, (UserData)costume->astrName, NULL );
				ui_MenuAppendItem( g_UGCEditorDoc->pCostumesListMenu, mapItem );
			} FOR_EACH_END;

			ui_MenuAppendItem( g_UGCEditorDoc->pCostumesListMenu, ui_MenuItemCreate( "---", UIMenuSeparator, NULL, NULL, NULL ));
			ui_MenuAppendItem( g_UGCEditorDoc->pCostumesListMenu, ugcEditorMenuItemCreate( UGC_ACTION_COSTUME_CREATE ));
		}
		eaDestroy( &eaCostumesSortedByName );
	}

	{
		UGCItem** eaItemsSortedByName = NULL;
		eaCopy( &eaItemsSortedByName, &ugcProj->items );
		eaQSort( eaItemsSortedByName, ugcSortItemsByDisplayName );
		if( !g_UGCEditorDoc->pItemsListMenu ) {
			g_UGCEditorDoc->pItemsListMenu = ui_MenuCreate( NULL );
		}
		ui_MenuClear( g_UGCEditorDoc->pItemsListMenu );
		if( eaSize( &eaItemsSortedByName )) {
			FOR_EACH_IN_EARRAY_FORWARDS( eaItemsSortedByName, UGCItem, item ) {
				UIMenuItem* mapItem = ui_MenuItemCreate( ugcItemSpecifierGetDisplayName( item->astrName ), UIMenuCallback, ugcEditorEditItem, (UserData)item->astrName, NULL );
				ui_MenuAppendItem( g_UGCEditorDoc->pItemsListMenu, mapItem );
			} FOR_EACH_END;

			ui_MenuAppendItem( g_UGCEditorDoc->pItemsListMenu, ui_MenuItemCreate( "---", UIMenuSeparator, NULL, NULL, NULL ));
			ui_MenuAppendItem( g_UGCEditorDoc->pItemsListMenu, ugcEditorMenuItemCreate( UGC_ACTION_ITEM_CREATE ));
		}
		eaDestroy( &eaItemsSortedByName );
	}
}

static void ugcEditorChatWindowChangedCB( UIChat* ignored, UserData rawChatWindow )
{
	UGCChatWindow* chatWindow = rawChatWindow;
	if( !ui_WindowIsVisible( chatWindow->window ) ) {
		chatWindow->hasUnreadMessages = true;
		ugcEditorQueueUIUpdate();
	}
}

/// Refresh a UGCChatWindow.  If STR-CHAT-HANDLE is NULL, set it to
/// the global chat window.
static void ugcEditorChatWindowRefresh( UGCChatWindow* chatWindow, const char* strChatHandle )
{
	if( chatWindow->window && stricmp( chatWindow->strHandleName, strChatHandle ) == 0 ) {
		return;
	} else {
		UIChat* chat;
		StructCopyString( &chatWindow->strHandleName, strChatHandle );
		
		if( !chatWindow->window ) {
			chatWindow->window = ui_WindowCreate( NULL, 0, 0, 400, 300 );
			ui_WindowSetMovable( chatWindow->window, true );
			ui_WindowSetCloseCallback( chatWindow->window, ui_WindowSetShowToFalseOnClose, NULL );
			
		}
		ui_WidgetGroupQueueFreeAndRemove( &UI_WIDGET( chatWindow->window )->children );

		if( chatWindow->strHandleName ) {
			char* estr = NULL;
			FormatMessageKey( &estr, "UGC.HandleChatFormat",
							  STRFMT_STRING( "ChatHandle", chatWindow->strHandleName ),
							  STRFMT_END );
			chat = ui_ChatCreateForPrivateMessages( chatWindow->strHandleName );
			ui_WidgetSetTextString( UI_WIDGET( chatWindow->window ), estr );
			ui_ChatSetChangedCallback( chat, ugcEditorChatWindowChangedCB, chatWindow );
			estrDestroy( &estr );
		} else {
			chat = ui_ChatCreate();
			ui_WidgetSetTextMessage( UI_WIDGET( chatWindow->window ), "UGC.GlobalChat" );
		}
		ui_WindowAddChild( chatWindow->window, UI_WIDGET( chat ));
		ui_WidgetSetDimensionsEx( UI_WIDGET( chat ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	}
}

static void ugcEditorChatWindowDestroy( UGCChatWindow** ppChatWindow )
{
	ui_WidgetQueueFreeAndNull( &(*ppChatWindow)->window );
	StructFreeStringSafe( &(*ppChatWindow)->strHandleName );
	SAFE_FREE( *ppChatWindow );
}

static void ugcEditorGlobalChatWindowShow( UIButton* ignored, UserData ignored2 )
{
	if( !g_UGCEditorDoc->chat_window ) {
		g_UGCEditorDoc->chat_window = calloc( 1, sizeof( *g_UGCEditorDoc->chat_window ));
		ugcEditorChatWindowRefresh( g_UGCEditorDoc->chat_window, NULL );
		ui_WindowPlaceAtCursorOrWidgetBox( g_UGCEditorDoc->chat_window->window );
	}

	ui_WindowPresentEx( g_UGCEditorDoc->chat_window->window, true );
}

static void ugcFriendsWindowAddFriendCB( UIWidget* ignored, UserData rawTextEntry )
{
	UITextEntry* entry = rawTextEntry;
	const char* text = ui_TextEntryGetText( entry );
	gclChat_FriendByName( (char*)text );
	ui_TextEntrySetText( entry, "" );
}

static void ugcEditorPrivateChatWindowShow( UIButton* ignored, UserData rawIndex )
{
	int index = (int)rawIndex;
	const char* chatHandle = eaGet( &g_UGCEditorDoc->eastrChatHandlesActive, index );
	if( !chatHandle ) {
		return;
	}

	if( eaSize( &g_UGCEditorDoc->eaPrivateChatWindows ) < index + 1 ) {
		eaSetSize( &g_UGCEditorDoc->eaPrivateChatWindows, index + 1 );
	}
	if( !g_UGCEditorDoc->eaPrivateChatWindows[ index ]) {
		UGCChatWindow* privateChatWindow = calloc( 1, sizeof( *privateChatWindow ));
		g_UGCEditorDoc->eaPrivateChatWindows[ index ] = privateChatWindow;
		ugcEditorChatWindowRefresh( privateChatWindow, chatHandle );
		elUICenterWindow( privateChatWindow->window );
	}
	ui_WindowPresentEx( g_UGCEditorDoc->eaPrivateChatWindows[ index ]->window, true );
	g_UGCEditorDoc->eaPrivateChatWindows[ index ]->hasUnreadMessages = false;
	ugcEditorQueueUIUpdate();
}

static void ugcEditorPrivateChatWindowClose( UIButton* ignored, UserData rawIndex )
{
	int index = (int)rawIndex;
	const char* chatHandle = eaGet( &g_UGCEditorDoc->eastrChatHandlesActive, index );
	eaRemove( &g_UGCEditorDoc->eastrChatHandlesActive, index );
	SAFE_FREE( chatHandle );
	ugcEditorQueueUIUpdate();
}

static void ugcFriendWindowStartChatCB( UIList* list, UserData ignored )
{
	ClientPlayerStruct* selected = ui_ListGetSelectedObject( list );
	if( selected ) {
		char* handleToPush;
		char* handleAt = strchr( selected->pchHandle, '@' );
		if( handleAt ) {
			handleToPush = handleAt + 1;
		} else {
			handleToPush = selected->pchHandle;
		}
		if( eaFindString( &g_UGCEditorDoc->eastrChatHandlesActive, handleToPush ) < 0 ) {
			int newIndex = eaSize( &g_UGCEditorDoc->eastrChatHandlesActive );
			handleToPush = strdup( handleToPush );
			eaPush( &g_UGCEditorDoc->eastrChatHandlesActive, handleToPush );
			ugcEditorPrivateChatWindowShow( NULL, (UserData)(intptr_t)newIndex );				
			ugcEditorQueueUIUpdate();
		}
	}
}

static void ugcFriendWindowRemoveIgnoreCB( UIWidget* ignored, UserData rawAccountID )
{
	U32 accountID = (U32)rawAccountID;
	GServerCmd_crRemoveIgnore( GLOBALTYPE_CHATRELAY, accountID, NULL );
}

static void ugcFriendWindowAddIgnoreCB( UIWidget* ignored, UserData rawAccountID )
{
	U32 accountID = (U32)rawAccountID;
	GServerCmd_crAddIgnore( GLOBALTYPE_CHATRELAY, accountID, NULL, false, NULL );
}

static void ugcFriendWindowRemoveFriendCB( UIWidget* ignored, UserData rawAccountID )
{
	U32 accountID = (U32)rawAccountID;
	GServerCmd_crRemoveFriend( GLOBALTYPE_CHATRELAY, accountID, NULL, true );
}

static void ugcFriendWindowDeclineFriendInviteCB( UIWidget* ignored, UserData rawAccountID )
{
	U32 accountID = (U32)rawAccountID;
	GServerCmd_crRemoveFriend( GLOBALTYPE_CHATRELAY, accountID, NULL, true );
}

static void ugcFriendWindowAcceptFriendInviteCB( UIWidget* ignored, UserData rawAccountID )
{
	U32 accountID = (U32)rawAccountID;
	GServerCmd_crAddFriend( GLOBALTYPE_CHATRELAY, accountID, NULL, true );
}

static void ugcFriendWindowAddFriendCB( UIWidget* ignored, UserData rawAccountID )
{
	U32 accountID = (U32)rawAccountID;
	GServerCmd_crAddFriend( GLOBALTYPE_CHATRELAY, accountID, NULL, false );
}

static void ugcFriendWindowContextCB( UIList* list, int column, int row, float mouseX, float mouseY, CBox* pBox, UserData ignored )
{
	ClientPlayerStruct* selected;

	ui_ListSetSelectedRowColExAndCallback( list, row, column, false, false );
	selected = ui_ListGetSelectedObject( list );
	if( selected ) {
		CBox screenBox = *pBox;
		screenBox.lx += list->lastDrawBox.lx;
		screenBox.hx += list->lastDrawBox.lx;
		screenBox.ly += list->lastDrawBox.ly + list->fHeaderHeight;
		screenBox.hy += list->lastDrawBox.ly + list->fHeaderHeight;
		
		if( !g_UGCEditorDoc->friends_context_menu ) {
			g_UGCEditorDoc->friends_context_menu = ui_MenuCreateMessage( "UGC.Menu_Context" );
		} else {
			ui_MenuClearAndFreeItems( g_UGCEditorDoc->friends_context_menu );
		}

		if( ClientChat_IsIgnoredAccount( selected->accountID )) {
			ui_MenuAppendItem( g_UGCEditorDoc->friends_context_menu, ui_MenuItemCreateMessage( "EntityMenu_RemoveIgnore", UIMenuCallback, ugcFriendWindowRemoveIgnoreCB, (UserData)selected->accountID, NULL ));
		} else {
			ui_MenuAppendItem( g_UGCEditorDoc->friends_context_menu, ui_MenuItemCreate( "EntityMenu_AddIgnore", UIMenuCallback, ugcFriendWindowAddIgnoreCB, (UserData)selected->accountID, NULL ));
		}

		if( ClientChat_IsFriendAccount( selected->accountID )) {
			ui_MenuAppendItem( g_UGCEditorDoc->friends_context_menu, ui_MenuItemCreate( "EntityMenu_RemoveFriend", UIMenuCallback, ugcFriendWindowRemoveFriendCB, (UserData)selected->accountID, NULL ));
		} else if( ClientChat_FriendAccountRequestReceived( selected->accountID )) {
			ui_MenuAppendItem( g_UGCEditorDoc->friends_context_menu, ui_MenuItemCreate( "EntityMenu_DeclineFriendRequest", UIMenuCallback, ugcFriendWindowDeclineFriendInviteCB, (UserData)selected->accountID, NULL ));
			ui_MenuAppendItem( g_UGCEditorDoc->friends_context_menu, ui_MenuItemCreate( "EntityMenu_AcceptFriendRequest", UIMenuCallback, ugcFriendWindowAcceptFriendInviteCB, (UserData)selected->accountID, NULL ));
		} else if( !ClientChat_FriendAccountRequestPending( selected->accountID )) {
			ui_MenuAppendItem( g_UGCEditorDoc->friends_context_menu, ui_MenuItemCreate( "EntityMenu_AddFriend", UIMenuCallback, ugcFriendWindowAddFriendCB, (UserData)selected->accountID, NULL ));
		}

		ui_MenuPopupAtBox( g_UGCEditorDoc->friends_context_menu, &screenBox );
	}
}

static void ugcEditorFriendsWindowShow( UIButton* ignored, UserData ignored2 )
{
	if( !g_UGCEditorDoc->friends_window ) {
		UIListColumn* column;
		g_UGCEditorDoc->friends_window = ui_WindowCreate( "", 0, 0, 400, 300 );
		ui_WidgetSetTextMessage( UI_WIDGET( g_UGCEditorDoc->friends_window ), "UGC.FriendsList" );

		{
			UITextEntry* entry = ui_TextEntryCreate( "", 0, 0 );
			UIButton* button = ui_ButtonCreate( "", 0, 0, ugcFriendsWindowAddFriendCB, entry );
			ui_ButtonSetMessageAndResize( button, "UGC.AddFriend" );
			ui_WindowAddChild( g_UGCEditorDoc->friends_window, UI_WIDGET( entry ));
			ui_WindowAddChild( g_UGCEditorDoc->friends_window, UI_WIDGET( button ));
			ui_TextEntrySetEnterCallback( entry, ugcFriendsWindowAddFriendCB, entry );

			ui_WidgetSetPosition( UI_WIDGET( entry ), 0, 0 );
			ui_WidgetSetWidthEx( UI_WIDGET( entry ), 1, UIUnitPercentage );
			entry->widget.rightPad = UI_WIDGET( button )->width;
			ui_WidgetSetPositionEx( UI_WIDGET( button ), 0, 0, 0, 0, UITopRight );
		}

		{
			UIList* list = ui_ListCreate( parse_ClientPlayerStruct, &g_UGCEditorDoc->eaFriends, 20 );
			ui_WindowAddChild( g_UGCEditorDoc->friends_window, UI_WIDGET( list ));

			column = ui_ListColumnCreateParseName( TranslateMessageKey( "UGC.FriendOnlineStatus" ), "OnlineStatus", NULL );
			ui_ListColumnSetWidth( column, 1, true );
			ui_ListAppendColumn( list, column );
			
			column = ui_ListColumnCreateParseName( TranslateMessageKey( "UGC.FriendHandle" ), "Handle", NULL );
			ui_ListColumnSetWidth( column, 1, true );
			ui_ListAppendColumn( list, column );

			column = ui_ListColumnCreateParseName( TranslateMessageKey( "UGC.FriendLocation" ), "Location", NULL );
			ui_ListColumnSetWidth( column, 1, true );
			ui_ListAppendColumn( list, column );
			
			ui_WidgetSetDimensionsEx( UI_WIDGET( list ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			ui_WidgetSetPaddingEx( UI_WIDGET( list ), 0, 0, UGC_ROW_HEIGHT, 0 );

			ui_ListSetActivatedCallback( list, ugcFriendWindowStartChatCB, NULL );
			ui_ListSetCellContextCallback( list, ugcFriendWindowContextCB, NULL );
		}

		ui_WindowPlaceAtCursorOrWidgetBox( g_UGCEditorDoc->friends_window );
	}

	ui_WindowPresentEx( g_UGCEditorDoc->friends_window, true );
}

void ugcEditorErrorsWindowShow( UIWidget* ignored, UserData ignored2 )
{
	if( !g_UGCEditorDoc->errors_window ) {
		UILabel* label = NULL;
		g_UGCEditorDoc->errors_window = ui_WindowCreate( "", 0, 0, 400, 350 );
		ui_WidgetSetTextMessage( UI_WIDGET( g_UGCEditorDoc->errors_window ), "UGC.Errors_Long" );

		label = ui_LabelCreate( NULL, 0, 0 );
		ui_WidgetSetTextMessage( UI_WIDGET( label ), "UGC.Errors_BlockPublishDescription" );
		ui_LabelSetWordWrap( label, true );
		ui_LabelSetWidthNoAutosize( label, 1, UIUnitPercentage );
		ui_WidgetSetHeight( UI_WIDGET( label ), UGC_ROW_HEIGHT * 2 );
		ui_WindowAddChild( g_UGCEditorDoc->errors_window, UI_WIDGET( label ));
		
		ugcEditorErrorsWidgetRefresh( &g_UGCEditorDoc->errors_window_tree, false );
		ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCEditorDoc->errors_window_tree ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( g_UGCEditorDoc->errors_window_tree ), 0, 0, UGC_ROW_HEIGHT * 2, 0 );
		ui_WindowAddChild( g_UGCEditorDoc->errors_window, UI_WIDGET( g_UGCEditorDoc->errors_window_tree ));

		elUICenterWindow( g_UGCEditorDoc->errors_window );
	}

	ui_WindowPresentEx( g_UGCEditorDoc->errors_window, true );
}

static void ugcEditorErrorsFillCB( UITreeNode* parent, UserData ignored )
{
	int errorsIt = 0;
	FOR_EACH_IN_EARRAY_FORWARDS( g_UGCEditorDoc->runtime_status_sorted_for_model, UGCRuntimeErrorOrHeader, errorOrHeader ) {
		UITreeNode* node;
		if( !errorOrHeader->error ) {
			node = ui_TreeNodeCreate( parent->tree, 0, NULL, errorOrHeader, NULL, NULL, ugcEditorErrorsListRowDrawCB, NULL, 20 );
			node->allow_selection = false;
		} else {
			node = ui_TreeNodeCreate( parent->tree, errorsIt, NULL, errorOrHeader, NULL, NULL, ugcEditorErrorsListRowDrawCB, NULL, 20 );
			++errorsIt;
		}
		ui_TreeNodeAddChild( parent, node );
	} FOR_EACH_END;
}

void ugcEditorErrorsWidgetRefresh( UITree** ppTree, bool navigateToErrorOnDoubleClick )
{
	UITree* tree = *ppTree;

	if( !tree ) {
		tree = ui_TreeCreate( 0, 0, 1, 1 );
		ui_TreeNodeSetFillCallback( &tree->root, ugcEditorErrorsFillCB, NULL );
		*ppTree = tree;
	}

	ui_WidgetSetDimensionsEx( UI_WIDGET( tree ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetTextMessage( UI_WIDGET( tree ), "UGC.Errors_NoErrors" );
	ui_TreeRefresh( tree );

	if( !navigateToErrorOnDoubleClick ) {
		ui_TreeSetSelectedCallback( tree, navigateTo, NULL );
		ui_TreeSetActivatedCallback( tree, NULL, NULL );
	} else {
		ui_TreeSetSelectedCallback( tree, NULL, NULL );
		ui_TreeSetActivatedCallback( tree, navigateTo, NULL );
	}
}

static void ugcEditorShowDevModeMenuCB( UIButton* ignored, UserData ignored2 )
{
	if( isDevelopmentMode() && g_UGCEditorDoc->pDevModeMenu ) {
		ui_MenuPopupAtCursorOrWidgetBox( g_UGCEditorDoc->pDevModeMenu );
	}
}

static void ugcEditorImportProject_FileSelectedCB( const char* path, const char* name, UserData ignored )
{
	char buffer[ MAX_PATH ];
	sprintf( buffer, "%s/%s", path, name );
	ugcEditorImportProject( buffer );

	ui_FileBrowserFree();
}

static void ugcEditorImportProjectCB( UIWidget* ignored, UserData ignored2 )
{
	static const char** eastrTopDirs = NULL;
	static const char** eastrExts = NULL;
	UIWindow* window;
	if( !eastrTopDirs ) {
		eaPush( &eastrTopDirs, "c:/" );
	}
	if( !eastrExts ) {
		eaPush( &eastrExts, ".ugcproject" );
		eaPush( &eastrExts, ".gz" );
	}
	
	window = ui_FileBrowserCreateEx(
			"Import project", "Import", UIBrowseExisting, UIBrowseFiles, false,
			eastrTopDirs, "c:/", NULL, eastrExts, NULL, NULL,
			ugcEditorImportProject_FileSelectedCB, NULL,
			NULL, NULL, NULL, false );
	ui_WindowSetModal( window, true );
	ui_WindowShowEx( window, true );
}

static void ugcEditorExportProject_FileSelectedCB( const char* path, const char* name, UserData ignored )
{
	char buffer[ MAX_PATH ];
	sprintf( buffer, "%s/%s", path, name );
	ugcEditorExportProject( buffer );

	ui_FileBrowserFree();
}

static void ugcEditorExportProjectCB( UIWidget* ignored, UserData ignored2 )
{
	UIWindow* window = ui_FileBrowserCreate(
			"Export project", "Export", UIBrowseNewOrExisting, UIBrowseFiles, false,
			"c:/", "c:/", NULL, ".ugcproject", NULL, NULL,
			ugcEditorExportProject_FileSelectedCB, NULL );
	ui_WindowSetModal( window, true );
	ui_WindowShowEx( window, true );
}

void ugcEditorRefreshWindowManagerPane(void)
{
	MEFieldContextEntry* entry;
	UIWidget* widget;
	int x = 2;
	
	MEContextPush( "UGCRoot_WindowManager", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( g_UGCEditorDoc->pWindowManagerPane ));

	entry = MEContextAddButtonMsg( "UGC.GlobalChat", NULL, ugcEditorGlobalChatWindowShow, NULL, "Chat_Window", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, 2 );
	ui_WidgetSetDimensionsEx( widget, 80, 1, UIUnitFixed, UIUnitPercentage );
	x += 82;

	entry = MEContextAddButtonMsg( "UGC.FriendsList", NULL, ugcEditorFriendsWindowShow, NULL, "Friends_Window", NULL, NULL );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, 2 );
	ui_WidgetSetDimensionsEx( widget, 80, 1, UIUnitFixed, UIUnitPercentage );
	x += 82;

	FOR_EACH_IN_EARRAY_FORWARDS( g_UGCEditorDoc->eastrChatHandlesActive, char, strChatHandle ) {
		int index = FOR_EACH_IDX( g_UGCEditorDoc->eastrChatHandlesActive, strChatHandle );
		{
			UGCChatWindow* chatWindow = eaGet( &g_UGCEditorDoc->eaPrivateChatWindows, index );
			char* estr = NULL;
			FormatMessageKey( &estr, SAFE_MEMBER( chatWindow, hasUnreadMessages ) ? "UGC.HandleChatFormat_Unread" : "UGC.HandleChatFormat",
							  STRFMT_STRING( "ChatHandle", chatWindow->strHandleName ),
							  STRFMT_END );
			entry = MEContextAddButtonIndex( estr, NULL, ugcEditorPrivateChatWindowShow, (UserData)(intptr_t)index, "Handle_Window", index, NULL, NULL );
			estrDestroy( &estr );
		}
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, 2 );
		ui_WidgetSetDimensionsEx( widget, 150, 1, UIUnitFixed, UIUnitPercentage );
		x += 150;

		entry = MEContextAddButtonIndex( "X", NULL, ugcEditorPrivateChatWindowClose, (UserData)(intptr_t)index, "Handle_Close", index, NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, 2 );
		ui_WidgetSetDimensionsEx( widget, 20, 1, UIUnitFixed, UIUnitPercentage );
		x += 25;

		// Refresh the associated window, if necessary
		{
			UGCChatWindow* chatWindow = eaGet( &g_UGCEditorDoc->eaPrivateChatWindows, index );
			if( chatWindow ) {
				ugcEditorChatWindowRefresh( chatWindow, strChatHandle );
			}
		}
	} FOR_EACH_END;
	while( eaSize( &g_UGCEditorDoc->eaPrivateChatWindows ) > eaSize( &g_UGCEditorDoc->eastrChatHandlesActive )) {
		UGCChatWindow* window = eaPop( &g_UGCEditorDoc->eaPrivateChatWindows );
		ugcEditorChatWindowDestroy( &window );
	}

	if( isDevelopmentMode() ) {
		char buffer[ 256 ];
		UGCProjectData* ugcProj = ugcEditorGetProjectData();
		
		entry = MEContextAddButton( "<DEV MODE>", "UGC_Icons_Labels_Dev", ugcEditorShowDevModeMenuCB, NULL, "DevModeButton", NULL, NULL );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 2, 2, 0, 0, UITopRight );
		ui_WidgetSetHeightEx( widget, 1, UIUnitPercentage );

		if( !g_UGCEditorDoc->pDevModeMenu ) {
			g_UGCEditorDoc->pDevModeMenu = ui_MenuCreate( NULL );
		}
		ui_MenuClear( g_UGCEditorDoc->pDevModeMenu );

		sprintf( buffer, "NS=%s", ugcProj->ns_name );
		ui_MenuAppendItem( g_UGCEditorDoc->pDevModeMenu,
						   ui_MenuItemCreate( buffer, UIMenuCallback, NULL, NULL, NULL ));
		if( SAFE_MEMBER( g_UGCEditorDoc->pCachedStatus, pFeatured )) {
			ui_MenuAppendItem( g_UGCEditorDoc->pDevModeMenu,
							   ui_MenuItemCreate( "Is Featured", UIMenuCallback, NULL, NULL, NULL ));
		}

		ui_MenuAppendItems( g_UGCEditorDoc->pDevModeMenu,
							ui_MenuItemCreate( "---", UIMenuSeparator, NULL, NULL, NULL ),
							ugcEditorMenuItemCreate( UGC_ACTION_FREEZE ),
							ui_MenuItemCreate( "Import project...", UIMenuCallback, ugcEditorImportProjectCB, NULL, NULL ),
							ui_MenuItemCreate( "Export project...", UIMenuCallback, ugcEditorExportProjectCB, NULL, NULL ),
							NULL );
	}

	MEContextPop( "UGCRoot_WindowManager" );
}

//// Basic UI lifecycle functions

static UGCEditorDoc *ugcEditorInit()
{
	UGCEditorDoc *new_doc = calloc(1, sizeof(UGCEditorDoc));

	new_doc->edit_undo_stack = EditUndoStackCreate();
	new_doc->runtime_status = StructCreate( parse_UGCRuntimeStatus );

	g_UGCEditorDoc = new_doc;

	if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_NEVERWINTER ) {
		//resRequestAllResourcesInDictionary("PlayerCostume");
	}

	ugcEditorLoadData(NULL);
	
	if( !s_eaRegions ) {
		// Set up region choice
		s_pcGround = TranslateMessageKey("UGC.Editor.Costume.Region.Ground");
		s_pcSpace = TranslateMessageKey("UGC.Editor.Costume.Region.Space");
		eaPush(&s_eaRegions, (char*)s_pcGround);
		eaPush(&s_eaRegions, (char*)s_pcSpace);
	}

	return new_doc;
}

static bool ugcEditorDoQuitCB(UIDialog *pDialog, UIDialogButton eButton, UserData pResponseData)
{
	if(UGC_EDITOR_CLOSE_CANCEL != eButton)
	{
		if(UGC_EDITOR_SAVE & eButton)
		{
			g_UGCEditorDoc->bQueueClose = (eButton & ~UGC_EDITOR_SAVE);

			ugcEditorSave();
		}
		else if(eButton)
			ServerCmd_DeleteAutosaveUGCProject(g_UGCEditorDoc->data->ns_name, eButton);
	}

	return true;
}

bool ugcEditorQueryLogout(bool quit, bool choosePrevious)
{
	if(isProductionEditMode())
	{
		if (g_UGCEditorDoc)
		{
			if (!g_ui_State.bInUGCEditor)
			{
				gQueueReturnToEditor = true;
				return false;
			}
			else
			{
				if (ugcEditorHasUnsavedChanges())
				{
					UGCEditorCloseOption close_option = quit ? UGC_EDITOR_QUIT : (choosePrevious ? UGC_EDITOR_CLOSE_PROJECT : UGC_EDITOR_CLOSE_EDITOR);
					UIDialog *pDialog = ui_DialogCreateEx( TranslateMessageKey( "UGC_Editor.UnsavedChangesTitle" ),
														   TranslateMessageKey( "UGC_Editor.UnsavedChangesDetails" ),
														   ugcEditorDoQuitCB, NULL, NULL,
														   TranslateMessageKey( "UGC.Cancel" ), UGC_EDITOR_CLOSE_CANCEL,
														   TranslateMessageKey( "UGC_Editor.UnsavedChanges_DontSaveAndQuit"), close_option,
														   TranslateMessageKey( "UGC_Editor.UnsavedChanges_SaveAndQuit" ), (UGC_EDITOR_SAVE | close_option),
														   NULL );
					ui_WindowSetModal(UI_WINDOW(pDialog), true);
					ui_WindowShowEx(UI_WINDOW(pDialog), true);
					return false;
				}
			}
			ugcEditorLoadData(NULL);
		}
	}
	return true;
}

bool bLoaded = false;
static UIWindow *pLoadingWindow = NULL;

void UGCEditorDoWaitForResourcesComplete(void)
{
	ui_WidgetQueueFreeAndNull(&pLoadingWindow);
	bLoaded = true;
	ugcEditorUpdateUIEx();
}


static void ugcPromptUnsavedChanges()
{
	UIDialog *pDialog;
	pDialog = ui_DialogCreateEx("Quit with unsaved changes?",
		"You have unsaved changes that will be lost if the editor is closed. Are you sure you want to close?",
		ugcEditorDoQuitCB, NULL, NULL,
		"Cancel", UGC_EDITOR_CLOSE_CANCEL,
		"Exit & lose changes", UGC_EDITOR_QUIT, 
		"Save & Exit", UGC_EDITOR_SAVE | UGC_EDITOR_QUIT,
		NULL);
	ui_WindowSetModal(UI_WINDOW(pDialog), true);
	ui_WindowShowEx(UI_WINDOW(pDialog), true);
}

static void ugcCommitEditModeChange(int enabled);

static void ugcSwitchToUGCEditModeAndWarnUnsavedCB(void * data)
{
	ugcCommitEditModeChange(1);
	ugcPromptUnsavedChanges();
}

void ugcEditorOncePerFrame(void)
{

	if (gQueueReturnToEditor)
	{
		ugcEditorSwitchToEditMode();
		gQueueReturnToEditor = false;
	}

	if (g_UGCEditorDoc)
	{
		INIT_PROJECT;

		if (!bLoaded && !pLoadingWindow && !resClientAreTherePendingRequests())
		{
			pLoadingWindow = ui_WindowCreate("Loading", 0, 0, 180, 80);
			ui_WindowAddChild(pLoadingWindow, ui_LabelCreate("Loading resources...", 0, 0));
			ui_WindowSetModal(pLoadingWindow, true);
			ui_WindowSetClosable(pLoadingWindow, false);
			elUICenterWindow(pLoadingWindow);
			ui_WindowShowEx(pLoadingWindow, true);

			ServerCmd_gslUGC_WaitForResources();
		}

		if( bLoaded ) {
			if (!project && gStartupProjectData)
			{
				ugcEditorLoadData(gStartupProjectData);
				StructDestroySafe(parse_UGCProjectData, &gStartupProjectData);
			}
			else if (project != g_UGCEditorDoc->last_project_info)
			{
				ugcEditorRefreshGlobalUI();
			}

			if (g_UGCEditorDoc->queued_update)
			{
				ugcEditorApplyUpdate();
				g_UGCEditorDoc->queued_update = false;
			}
			if (g_UGCEditorDoc->queued_ui_update || g_UGCEditorDoc->bLastShouldShowRestoreButtons != gfxShouldShowRestoreButtons() )
			{
				ugcEditorUpdateUI();
				g_UGCEditorDoc->queued_ui_update = false;
			}

			
			FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, sub_doc)
			{
				ugcMapEditorOncePerFrame( sub_doc,
										  (g_UGCEditorDoc->activeView.mode == UGC_VIEW_MAP_EDITOR
										   && ugcMapEditorGetName( sub_doc ) == g_UGCEditorDoc->activeView.name) );
			} FOR_EACH_END;
			if( g_UGCEditorDoc->no_maps_editor ) {
				ugcNoMapsEditorOncePerFrame( g_UGCEditorDoc->no_maps_editor, g_UGCEditorDoc->activeView.mode == UGC_VIEW_NO_MAPS );
			}
			FOR_EACH_IN_EARRAY(g_UGCEditorDoc->dialog_editors, UGCDialogTreeDoc, editor) {
				ugcDialogTreeDocOncePerFrame( editor,
											  (g_UGCEditorDoc->activeView.mode == UGC_VIEW_DIALOG_TREE
											   && ugcDialogTreeDocGetIDAsPtr(editor) == g_UGCEditorDoc->activeView.name) );
			} FOR_EACH_END;
			if( g_UGCEditorDoc->mission_editor ) {
				ugcMissionDocOncePerFrame(g_UGCEditorDoc->mission_editor, g_UGCEditorDoc->activeView.mode == UGC_VIEW_MISSION );
			}

			switch( g_UGCEditorDoc->activeView.mode ) {
				xcase UGC_VIEW_COSTUME:
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, editor)
					{
						if (ugcCostumeEditor_GetCostume(editor)->astrName == g_UGCEditorDoc->activeView.name)
						{
							ugcCostumeEditor_OncePerFrame(editor);
						}
					}
					FOR_EACH_END;

				xcase UGC_VIEW_NO_COSTUMES:
					ugcNoCostumesEditor_OncePerFrame(g_UGCEditorDoc->no_costumes_editor);

				xcase UGC_VIEW_NO_DIALOG_TREES:
					ugcNoDialogTreesDocOncePerFrame( g_UGCEditorDoc->no_dialogs_editor );
				
				xcase UGC_VIEW_ITEM:
					if( g_UGCEditorDoc->item_editor ) {
						ugcItemEditorOncePerFrame( g_UGCEditorDoc->item_editor );
					}					
			}

			if (g_UGCEditorDoc->delete_queue_time > 0)
				g_UGCEditorDoc->delete_queue_time--;
			else
				eaDestroyStruct(&g_UGCEditorDoc->data_delete_queue, parse_UGCProjectData);

			ugcEditorProcessFlashingObjects();

			ugcEditorMenuOncePerFrame(g_UGCEditorDoc->menus);

			if (g_UGCEditorDoc->autosave_timer && timerElapsed(g_UGCEditorDoc->autosave_timer) > UGC_AUTOSAVE_DURATION)
			{
				if (project)
					ServerCmd_UpdateUGCProjectServerCopy(g_UGCEditorDoc->data);
				timerFree(g_UGCEditorDoc->autosave_timer);
				g_UGCEditorDoc->autosave_timer = 0;
			}

			// Update friends list
			gclChat_FillFriendListStructs( &g_UGCEditorDoc->eaFriends, entActivePlayerPtr() );
		}

		if (utilitiesLibShouldQuit() && project)
		{
			if (ugcEditorHasUnsavedChanges())
			{
				utilitiesLibSetShouldQuit(false);
				if (!g_ui_State.bInUGCEditor)
					emQueueFunctionCallEx(ugcSwitchToUGCEditModeAndWarnUnsavedCB, NULL, 0);
				else
					ugcPromptUnsavedChanges();
			}
		}
		
		g_UGCEditorDoc->bAnimatedHeadshotThisFrame = false;
	}

	ugcEditorUpdateTrivia();
	ugc_ReportBugOncePerFrame();
	ugcPlayingEditorOncePerFrame();
}

void ugcEditorDrawGhosts( void )
{
	ugcPlayingEditorDrawGhosts();
}

bool ugcEditorIsActive(void)
{
	return g_ui_State.bInUGCEditor;
}

void ugcEditorToggleSkin(bool bEnabled)
{
	if(bEnabled)
	{
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor", g_ui_State.hActiveSkin );
		g_ui_State.minScreenWidth = 1024;
		g_ui_State.minScreenHeight = 720;
		ui_SetGlobalValuesFromActiveSkin();
	}
	else
	{
		REMOVE_HANDLE( g_ui_State.hActiveSkin );
		g_ui_State.minScreenWidth = 0;
		g_ui_State.minScreenHeight = 0;
		ui_SetGlobalValuesFromActiveSkin();
	}
}

void ugcLoadEditingData(Entity *pEnt)
{
	objectLibraryLoad();
	ugcLoadDictionaries();
	resSubscribeToInfoIndex(UGC_DICTIONARY_RESOURCE_INFO, true);
}

void ugcEditorSetupPostUI(void)
{
	// Grab any game prefs here
}

static void ugcCommitEditModeChange(int enabled)
{
	Entity *pEnt = entActivePlayerPtr();
	if (!areEditorsAllowed() && !g_ui_State.bInUGCEditor)
	{
		return;
	}
	if( g_UGCEditorDoc ) {
		ugcEditorQueueUIUpdate();
	}

	if (!!enabled == g_ui_State.bInUGCEditor)
	{
		return;
	}
	g_ui_State.bInUGCEditor = !!enabled;
	ui_TooltipsSetNoDelay( g_ui_State.bInUGCEditor );
	
	if (pEnt && pEnt->pPlayer)
	{		
		// halt camera movement
		globCmdParse("Camera.halt");

		if (enabled)
		{
			gclPatchStreamingFastMode();

			ugcLoadingUpdateState(UGC_LOAD_NONE, 0);
			
			ugcLoadEditingData(pEnt);

			ugcResourceLoadLibrary();
			
			if (isDevelopmentMode() && !!enabled) {
				ugcTakeObjectPreviewPhotos();
			}

			if (g_ui_State.bInEditor)
				CommandEditMode(0); // Leave normal edit mode

			// Stop interaction. In case we are leaving from preview
			interaction_ClearPlayerInteractState(pEnt);	
			
			ServerCmd_gslUGCEnterEditor();

			ui_GameUIHide(UI_GAME_HIDER_UGC);
			
			keybind_PushProfileName("UGCEditor");

			if (!g_UGCEditorDoc)
				ugcEditorInit();

			// Query publish disabled status on each edit mode enable
			g_UGCEditorDoc->bPublishingEnabled = false;
			ServerCmd_gslIsUGCPublishDisabled();

			ugcEditorRefreshGlobalUI();  // Initial tick to create or update everything
			ugcEditorSetupPostUI();

			ugcEditorUpdateWindowVisibility();

			if (isDevelopmentMode())
			{
				resClientRequestEditingLogin(gimmeDLLQueryUserName(), true);

				if (!FolderCacheUpdatesLikelyWorking())
				{
					Errorf("Filesystem updates not working.  DO NOT EDIT FILES.  Checkouts will not work correctly, and you may lose data.  Please restart the client/server and if the problem persists, restart your computer.");
				}
			}

			// disable position displays
			globCmdParse("showcampos 0");
			stringCacheDisableWarnings(); // Don't care about string cache resizing in edit mode

			// unload project chooser UI
			ugcProjectChooser_FinishedLoading();

			//wl_state.stop_map_transfer = true;
		}

		//emSetEditorMode(!!enabled);
		//gclSetEditorCameraActive(!!enabled);
		gfxDebugClearAccessLevelCmdWarnings(); // Reset the flag, so that editMode 0 doesn't trigger a warning
	}

	if (enabled)
	{
		//TODO(UGC/AM): Fix this hackery when MHenry makes his sounds not looping oneshots.
		sndKillAll();
		g_ui_State.forceHideWindowButtons = true;
	}
	else
	{
		resFreePreviews();

		keybind_PopProfileName("UGCEditor");

		ui_WindowCloseAll();

		ui_GameUIShow(UI_GAME_HIDER_UGC);
		
		
		ugcEditorUpdateWindowVisibility();
		ui_SetFocus(NULL);

		//wl_state.stop_map_transfer = false;
		ugcPerfErrorHasFired = false;
		
		g_ui_State.forceHideWindowButtons = false;
	}

	if (!enabled)
	{
		keybind_PushProfileName("UGCGamePlay");
	}
	else
	{
		keybind_PopProfileName("UGCGamePlay");
	}
	
	// Going back into the editor, disable any components
	if( enabled ) {
		ugcPlayingEditorMapChanged( NULL );
	}

	ugcPlayingEditorEditModeChanged( enabled );
}

static void ugcCommitEditModeDisableCB(void * data)
{
	ugcCommitEditModeChange(0);
}

static void ugcCommitEditModeEnableCB(void * data)
{
	ugcCommitEditModeChange(1);
}

AUTO_COMMAND ACMD_NAME(UGCEditMode);
void ugcEditMode(int enabled)
{
	emQueueFunctionCallEx(enabled ? ugcCommitEditModeEnableCB : ugcCommitEditModeDisableCB, NULL, 0);
}

AUTO_COMMAND ACMD_ACCESSLEVEL(0) ACMD_HIDE;
void ugcEditorExportProject(char *filename)
{
	if (g_UGCEditorDoc && g_UGCEditorDoc->data)
	{
		ParserWriteTextFile(filename, parse_UGCProjectData, g_UGCEditorDoc->data, 0, 0);
	}
	else
	{
		Errorf("Failed to export project: No project loaded");
	}
}

static char *spFileNameForSafeProjectExport = NULL;
static char *spTextBufferForSafeProjectExport = NULL;
static int sSafeProjectExportID = 0;
static int sSafeProjectImportID = 0;
static char *spTextBufferForSafeProjectImport = NULL;

#define MAX_SAFE_EXPORT_BUF_SIZE 10000000


AUTO_COMMAND ACMD_ACCESSLEVEL(0);
void ugcEditorExportProjectSafe(char *filename)
{
	sSafeProjectExportID++;
	estrDestroy(&spFileNameForSafeProjectExport);
	estrDestroy(&spTextBufferForSafeProjectExport);

	if (g_UGCEditorDoc && g_UGCEditorDoc->data)
	{
		estrCopy2(&spFileNameForSafeProjectExport, filename);
		ParserWriteText(&spTextBufferForSafeProjectExport, parse_UGCProjectData, g_UGCEditorDoc->data, 0, 0, 0);

		estrInsertf(&spTextBufferForSafeProjectExport, 0, "%s\n", UGCProject_GetTimestampPlusShardNameStringEscaped());

		ServerCmd_GetCryptKeyForSafeProjectExport(spTextBufferForSafeProjectExport, sSafeProjectExportID);
	}
}

void DoReceiveCryptKeyForSafeProjectExport(char *pKey, int iExportID)
{
	void *pCompressedBuffer = NULL;
	int iCompressedSize;
	FILE *pFile;
	int *pBufAsInts;

	if (iExportID != sSafeProjectExportID)
	{
		//FAIL another export started before the previous one could complete
		return;
	}

	if (!pKey[0])
	{
		//FAIL server didn't like the string we sent up for some reason, probably someone trying to hack the timestamp
		return;
	}

	estrInsertf(&spTextBufferForSafeProjectExport, 0, "%s\n", pKey);

	pBufAsInts = pCompressedBuffer = zipData(spTextBufferForSafeProjectExport, estrLength(&spTextBufferForSafeProjectExport) + 1, &iCompressedSize);

	pBufAsInts[0] = reversibleHash(pBufAsInts[0], false);
	pBufAsInts[1] = reversibleHash(pBufAsInts[1], false);
	pBufAsInts[2] = reversibleHash(pBufAsInts[2], false);


	pFile = fopen(spFileNameForSafeProjectExport, "wb");
	if (!pFile)
	{
		//FAIL can't write file
	}
	else
	{
		int iUncompressedSize = estrLength(&spTextBufferForSafeProjectExport) + 1;
		if (iUncompressedSize > MAX_SAFE_EXPORT_BUF_SIZE)
		{
			//FAIL buffer too big
		}
		else
		{
			iUncompressedSize = reversibleHash(iUncompressedSize, false);
			fwrite(&iUncompressedSize, sizeof(int), 1, pFile);
			fwrite(pCompressedBuffer, iCompressedSize, 1, pFile);
			fclose(pFile);
		}
	}

	SAFE_FREE(pCompressedBuffer);
	estrDestroy(&spTextBufferForSafeProjectExport);
	estrDestroy(&spFileNameForSafeProjectExport);
}

bool ImportProjectInternal(UGCProjectData *new_data)
{
	if (g_UGCEditorDoc && g_UGCEditorDoc->data)
	{
		char ns_name[RESOURCE_NAME_MAX_SIZE];

		strcpy(ns_name, g_UGCEditorDoc->data->ns_name);

		ugcEditorClearData();

		StructDestroy(parse_UGCProjectData, g_UGCEditorDoc->data);
		g_UGCEditorDoc->data = new_data;

		ugcProjectDataNameSpaceChange(g_UGCEditorDoc->data, ns_name);

		ugcEditorFixupPostEdit(true);

		StructDestroy(parse_UGCProjectData, g_UGCEditorDoc->last_save_data);
		StructDestroy(parse_UGCProjectData, g_UGCEditorDoc->last_edit_data);
		g_UGCEditorDoc->last_save_data = StructClone(parse_UGCProjectData, g_UGCEditorDoc->data);
		g_UGCEditorDoc->last_edit_data = StructClone(parse_UGCProjectData, g_UGCEditorDoc->data);

		ugcPerfKnownStallyOperation = true;
		ugcEditorQueueUIUpdate();
		return true;
	}
	else
	{
		Errorf("Failed to import project: No project loaded");
		StructDestroy(parse_UGCProjectData, new_data);
		return false;
	}
}


AUTO_COMMAND ACMD_ACCESSLEVEL(0);
void ugcEditorImportProjectSafe(char *filename)
{
	int iBufSize;
	char *pBuf = fileAlloc(filename, &iBufSize);
	sSafeProjectImportID++;

	estrDestroy(&spTextBufferForSafeProjectImport);

	if (!pBuf)
	{
		//FAIL can't read file
		return;
	}
	else
	{
		int	iUncompressedSize;
		int* pBufAsInts = (int*)pBuf;
			
		pBufAsInts[0] = reversibleHash(pBufAsInts[0], true);
		pBufAsInts[1] = reversibleHash(pBufAsInts[1], true);
		pBufAsInts[2] = reversibleHash(pBufAsInts[2], true);
		pBufAsInts[3] = reversibleHash(pBufAsInts[3], true);

		iUncompressedSize = *pBufAsInts;
		if (iUncompressedSize > MAX_SAFE_EXPORT_BUF_SIZE)
		{
			//FAIL corrupt file
		}
		else
		{
			estrSetSize(&spTextBufferForSafeProjectImport, iUncompressedSize - 1);
			if (unzipData(spTextBufferForSafeProjectImport, &iUncompressedSize, pBuf + sizeof(int), iBufSize - sizeof(int)))
			{
				//FAIL corrupt file
			}
			else
			{
				ServerCmd_CheckBufferForSafeImport(spTextBufferForSafeProjectImport, sSafeProjectImportID);
			}
		}
	}

	SAFE_FREE(pBuf);
}
		
void DoSafeImportBufferResult(int iID, int iResult)
{
	char *pReadBuf;
	UGCProjectData *new_data = NULL;
	if (iID != sSafeProjectImportID)
	{
		//FAIL another import started
		estrDestroy(&spTextBufferForSafeProjectImport);
		return;
	}

	if (!iResult)
	{
		//FAIL crypto non-match
		estrDestroy(&spTextBufferForSafeProjectImport);
		return;
	}

	pReadBuf = strchr(spTextBufferForSafeProjectImport, '\n');
	if (!pReadBuf)
	{
		//FAIL corruption
		estrDestroy(&spTextBufferForSafeProjectImport);
		return;
	}

	pReadBuf++;
	pReadBuf = strchr(pReadBuf + 1, '\n');
	if (!pReadBuf)
	{
		//FAIL corruption
		estrDestroy(&spTextBufferForSafeProjectImport);
		return;
	}

	pReadBuf++;



	new_data = StructCreate(parse_UGCProjectData);

	if (!ParserReadText(pReadBuf, parse_UGCProjectData, new_data, 0))
	{
		//FAIL corruption or data format change
		StructDestroy(parse_UGCProjectData, new_data);
		estrDestroy(&spTextBufferForSafeProjectImport);
		return;
	}

	if (!ImportProjectInternal(new_data))
	{
		//FAIL project read failure
		estrDestroy(&spTextBufferForSafeProjectImport);
		return;
	}
}

AUTO_COMMAND ACMD_ACCESSLEVEL(4);
char *ugcEditorTransferProjectOwnershipToUserByIDWithName(ContainerID uUGCAccountID, const char *pcAccountName)
{
	if(isProductionEditMode() && g_UGCEditorDoc && g_ui_State.bInUGCEditor) // if we are in Foundry and are not previewing
	{
		if(uUGCAccountID && !nullStr(pcAccountName)) // if params are valid
		{
			if(!ugcEditorHasUnsavedChanges()) // if we have no unsaved changes
			{
				ServerCmd_gslUGC_TransferProjectOwnershipToUserByIDWithName(uUGCAccountID, pcAccountName); // Transfer ownership

				ClientGoToCharacterSelectAndChoosePreviousForUGC(); // Go to UGC Project chooser to see project is gone from list
				return "Successfully transferred UGC Project. Logging out of Foundry now.";
			}
			else
				return "You have unsaved changes. Please save the UGC Project and run the command again.";
		}
		else
			return "Invalid params. You must specify the Account ID and Account Name of the person you are transferring this UGC Project to.";
	}
	else
		return "Please run ugcEditorTransferProjectOwnershipToUserByIDWithName from the Foundry with the project open that you want to transfer.";
}

AUTO_COMMAND ACMD_ACCESSLEVEL(4);
void ugcEditorImportProject(char *filename)
{
	UGCProjectData *new_data = StructCreate(parse_UGCProjectData);
	char *pErrorString = NULL;
	int iRetVal;

	if (g_isContinuousBuilder)
	{
		ErrorfPushCallback(EstringErrorCallback, (void*)&pErrorString);
	}

	if (strEndsWith(filename, ".gz"))
	{
		iRetVal = ParserReadZippedTextFile(filename, parse_UGCProjectData, new_data, 0);
	}
	else
	{
		iRetVal = ParserReadTextFile(filename, parse_UGCProjectData, new_data, 0);
	}
		


	if (!iRetVal)
	{
		if (g_isContinuousBuilder)
		{
			assertmsgf(0, "Failed to import project %s, it may be out of date. Errors: %s\n", filename, pErrorString);
		}
	

		ErrorFilenamef(filename, "Failed to read project data!");
		StructDestroy(parse_UGCProjectData, new_data);
		return;
	}

	if (g_isContinuousBuilder)
	{
		ErrorfPopCallback();
	}

	if (!ImportProjectInternal(new_data))
	{
		//something went wrong
	}
}

AUTO_COMMAND ACMD_ACCESSLEVEL(4);
void ugcEditorImportProjectFromDir(char *dir)
{
	UGCProjectData* data = ugcProjectLoadFromDir( dir );
	
	if (!data || !ImportProjectInternal(data))
	{
		//something went wrong
	}
}

char *spFolderForZippedProjectImport = NULL;
AUTO_CMD_ESTRING(spFolderForZippedProjectImport, FolderForZippedProjectImport) ACMD_ACCESSLEVEL(4) ACMD_COMMANDLINE;

//special CSR command to make it super easy for people who are downloading zipped 
//project files from servermonitor into a download directory
AUTO_COMMAND ACMD_ACCESSLEVEL(4) ACMD_NAME(izp) ACMD_CATEGORY(ugc, csr);
char *UGCProjectImportNewestZipped(void)
{
	int iCount;
	int i;
	char *pNewestName = NULL;
	U32 iNewestModTime = 0;
	static char *pRetString = NULL;
	char **ppFileList = NULL;

	if (!estrLength(&spFolderForZippedProjectImport))
	{
		return "No directory specified... you must specify one with -FolderForZippedProjectImport";
	}

	ppFileList = fileScanDirNoSubdirRecurse(spFolderForZippedProjectImport);
    iCount = eaSize( &ppFileList );

	for (i = 0 ; i < iCount; i++)
	{
		if (strEndsWith(ppFileList[i], ".gz"))
		{
			U32 iModTime = fileLastChangedSS2000(ppFileList[i]);
			if (iModTime > iNewestModTime)
			{
				iNewestModTime = iModTime;
				pNewestName = ppFileList[i];
			}
		}
	}

	if (!iNewestModTime)
	{
		fileScanDirFreeNames(ppFileList);
		return "No files found";
	}
	
	ugcEditorImportProject(pNewestName);
	estrPrintf(&pRetString, "Attempted to import %s", pNewestName);

	fileScanDirFreeNames(ppFileList);

	return pRetString;
}

void UGCEditorDoUpdatePublishStatus(bool succeeded, const char *pDisplayString)
{
	if (g_UGCEditorDoc->modal_dialog)
		ui_WidgetQueueFreeAndNull(&g_UGCEditorDoc->modal_dialog);
	if (g_UGCEditorDoc->modal_timeout)
		TimedCallback_Remove(g_UGCEditorDoc->modal_timeout);
	g_UGCEditorDoc->modal_timeout = NULL;

	if (succeeded)
	{
		StructDestroySafe( parse_UGCProjectStatusQueryInfo, &g_UGCEditorDoc->pCachedStatus );
		g_UGCEditorDoc->pCachedStatus = StructCreate( parse_UGCProjectStatusQueryInfo );
		
		g_UGCEditorDoc->pCachedStatus->bCurrentlyPublishing = true;
		g_UGCEditorDoc->pCachedStatus->bLastPublishSucceeded = false;
		g_UGCEditorDoc->pCachedStatus->iCurPlaceInQueue = -1;
		UGCEditorQueryJobStatus();
		ugcEditorRefreshGlobalUI();
		ugcProjectEditor_Refresh( g_UGCEditorDoc->project_editor );
	}
	else
	{
		StructDestroySafe( parse_UGCProjectStatusQueryInfo, &g_UGCEditorDoc->pCachedStatus );
		g_UGCEditorDoc->pCachedStatus = StructCreate( parse_UGCProjectStatusQueryInfo );
		
		g_UGCEditorDoc->pCachedStatus->bCurrentlyPublishing = false;
		g_UGCEditorDoc->pCachedStatus->bLastPublishSucceeded = false;
		ui_DialogPopup("Error", STACK_SPRINTF("Error during publish. Publish failed: %s", pDisplayString));
		ugcEditorRefreshGlobalUI();
		ugcProjectEditor_Refresh( g_UGCEditorDoc->project_editor );
	}
}

static void ugcEditorPublishStatusTimeout(TimedCallback *callback, F32 timeSinceLastCallback, UserData userData)
{
	UGCEditorUpdatePublishStatus(false, "");
}

void ugcEditorDoSaveAndPublishProject(void)
{
	UGCProjectData *data;
	UILabel* label;
	if (!isProductionEditMode())
		return;

	data = ugcEditorGetProjectData();
	if (eaSize(&g_UGCEditorDoc->runtime_status_all_errors))
	{
		if (g_isContinuousBuilder)
		{
			char *pErrorString = NULL;
			int i;

			for (i=0; i < eaSize(&g_UGCEditorDoc->runtime_status_all_errors); i++)
			{
				ConcatStringFromUGCRuntimeError(g_UGCEditorDoc->runtime_status_all_errors[i], &pErrorString, true);
				estrConcatf(&pErrorString, "\n");
			}

			assertmsgf(0, "Couldn't publish project because of unfinished tasks:\n%s", pErrorString);

		}
		ugcModalDialog( "Cannot Publish", "Cannot publish a project until all unfinished tasks are completed.", UIOk);
		return;
	}
	
	// Clear autosave
	if (g_UGCEditorDoc->autosave_timer)
	{
		timerFree(g_UGCEditorDoc->autosave_timer);
		g_UGCEditorDoc->autosave_timer = 0;
	}

	EditUndoStackClear(g_UGCEditorDoc->edit_undo_stack);

	g_UGCEditorDoc->modal_dialog = ui_WindowCreate("", 0, 0, 200, 50);
	ui_WidgetSetTextMessage( UI_WIDGET( g_UGCEditorDoc->modal_dialog ), "UGC_Editor.StartingPublish" );
	label = ui_LabelCreate("", 0, 0);
	ui_WidgetSetTextMessage( UI_WIDGET( label ), "UGC_Editor.StartingPublishDetails" );
	ui_WindowAddChild(g_UGCEditorDoc->modal_dialog, label);
	ui_WindowSetClosable(g_UGCEditorDoc->modal_dialog, false);
	ui_WindowSetModal(g_UGCEditorDoc->modal_dialog, true);
	ui_WindowSetResizable(g_UGCEditorDoc->modal_dialog, false);
	elUICenterWindow(g_UGCEditorDoc->modal_dialog);
	ui_WindowShowEx(g_UGCEditorDoc->modal_dialog, true);

	g_UGCEditorDoc->modal_timeout = TimedCallback_Add(ugcEditorPublishStatusTimeout, NULL, UGC_PUBLISH_STATUS_TIMEOUT);

	ServerCmd_SaveAndPublishUGCProject(data);
}

void ugcEditorShutdown(void)
{
	if (g_UGCEditorDoc)
	{
		if (g_UGCEditorDoc->modal_dialog)
			ui_WindowClose(g_UGCEditorDoc->modal_dialog);
		g_UGCEditorDoc->modal_dialog = NULL;

		if (g_UGCEditorDoc->modal_timeout)
			TimedCallback_Remove(g_UGCEditorDoc->modal_timeout);
		g_UGCEditorDoc->modal_timeout = NULL;

		if (g_UGCEditorDoc->job_timeout)
			TimedCallback_Remove(g_UGCEditorDoc->job_timeout);
		g_UGCEditorDoc->job_timeout = NULL;
	}
	StructDestroySafe(parse_UGCEditorCopyBuffer, &ugcCopyBuffer);

	if (isProductionMode())
	{
		// Used to clear any edit modes set by the editor
		// Only called when in real production mode.  Don't call in dev mode!
		resClearAllDictionaryEditModes();
	}

	ugcEditorLoadData(NULL);
	ugcEditMode(0);
	memset( ugcRecentOpsForMiniDump, 0, sizeof( ugcRecentOpsForMiniDump ));

	// Pop any keybinds that may have been set
	keybind_PopProfileName("UGCEditor");
	keybind_PopProfileName("UGCGamePlay");
}

// Don't remove: UGCPublishTest uses command "UGC.Publish".
AUTO_COMMAND ACMD_NAME("UGC.Publish") ACMD_ACCESSLEVEL(2);
void ugcEditorSaveAndPublishProjectCmd(void)
{
	ugcEditorExecuteCommandByID( UGC_ACTION_PUBLISH );
}

void ugcEditorSaveAndPublishProject( void )
{
	if(!isProductionEditMode()) return;

	if (g_isContinuousBuilder)
	{
		ugcEditorDoSaveAndPublishProject();
	}
	else if (eaSize(&g_UGCEditorDoc->runtime_status_all_errors))
	{
		ugcEditorErrorsWindowShow( NULL, NULL );
	}
	else
	{
		if( ugcModalDialogMsg( "UGC_Editor.SaveAndPublishTitle", "UGC_Editor.SaveAndPublishDetails", UIYes | UINo ) == UIYes ) {
			ugcEditorDoSaveAndPublishProject();
		}
	}
}

static void ugcEditorFreezeProjectCancel(UIButton *button, UserData ignored)
{
	MEContextDestroyByName("UGCEditorFreezeProjectDialog");
}

void ugcEditorDoAuthorAllowsFeaturedChanged( bool bSucceeded )
{
	if( g_UGCEditorDoc ) {
		if( g_UGCEditorDoc->modal_timeout ) {
			TimedCallback_Remove( g_UGCEditorDoc->modal_timeout );
		}
		g_UGCEditorDoc->modal_timeout = NULL;
		ui_WindowClose(g_UGCEditorDoc->modal_dialog);

		if( bSucceeded ) {
			if( ugcDefaultsAuthorAllowsFeaturedBlocksEditing() ) {
				// Clear unsaved changes on this code path.  They just
				// disabled editing so the data isn't useful.
				if( g_UGCEditorDoc->last_save_data ) {
					StructCopy( parse_UGCProjectData, g_UGCEditorDoc->data, g_UGCEditorDoc->last_save_data, 0, 0, 0 );
				}
			
				ClientGoToCharacterSelectAndChoosePreviousForUGC();
			}
		} else {
			ugcModalDialogMsg( "UGC.ErrorDialogTitle", "UGC.ErrorSetAuthorAllowsFeatured", UIOk );
		}
	}
}

static void ugcEditorSetAuthorAllowsFeaturedTimeout( TimedCallback *callback, F32 timeSinceLastCallback, UserData userData )
{
	if (g_UGCEditorDoc->modal_dialog) {
		ugcEditorDoAuthorAllowsFeaturedChanged( false );
	}
}

static void ugcEditorSetAuthorAllowsFeatured( bool bValue )
{
	if (!isProductionEditMode())
		return;

	g_UGCEditorDoc->modal_dialog = ui_WindowCreate( "", 0, 0, 200, 50);
	{
		char* estr = NULL;
		ugcFormatMessageKey( &estr, "UGC.SetProjectAuthorAllowsFeaturedDialogTitle", STRFMT_END );
		ui_WindowAddChild(g_UGCEditorDoc->modal_dialog, ui_LabelCreate(estr, 0, 0));
		estrDestroy( &estr );
	}
	ui_WindowSetClosable(g_UGCEditorDoc->modal_dialog, false);
	ui_WindowSetModal(g_UGCEditorDoc->modal_dialog, true);
	ui_WindowSetResizable(g_UGCEditorDoc->modal_dialog, false);
	elUICenterWindow(g_UGCEditorDoc->modal_dialog);
	ui_WindowShowEx(g_UGCEditorDoc->modal_dialog, true);

	g_UGCEditorDoc->modal_timeout = TimedCallback_Add( ugcEditorSetAuthorAllowsFeaturedTimeout, NULL, UGC_AUTHOR_ALLOWS_FEATURED_TIMEOUT);

	ServerCmd_gslUGC_SetAuthorAllowsFeatured( bValue );
}

void ugcEditorShowSetAuthorAllowsFeaturedDialog(void)
{
	if( ugcModalDialogMsg( "UGC.SetProjectAuthorAllowsFeaturedDialogTitle", "UGC.SetProjectAuthorAllowsFeaturedEULA", UIYes | UINo ) == UIYes ) {
		ugcEditorSetAuthorAllowsFeatured( true );
	}
}

static void ugcEditorFreezeProjectOK(UIButton *button, UGCFreezeProjectInfo *pInfo)
{
	ServerCmd_FreezeUGCProject(ugcEditorGetProjectData(), pInfo);
	ugcEditorFreezeProjectCancel(button, NULL);
}

void ugcEditorFreezeProjectRefresh(bool bPartialRefresh);

void ugcEditorFreezeProjectCB(MEField *pField, bool bFinished, void *pUserData)
{
	if (MEContextExists())
		return;

	if (bFinished)
	{
		ugcEditorFreezeProjectRefresh(false);
	}
	else
	{
		ugcEditorFreezeProjectRefresh(true);
	}
}

void ugcEditorFreezeProjectValidate(UGCFreezeProjectInfo *pInfo, MEFieldGenericErrorList *pErrorList)
{
	char filename[256];

	if (!pInfo->pcProjectPrefix || !pInfo->pcProjectPrefix[0])
	{
		MEContextGenericErrorAdd(pErrorList, "ProjectPrefix", 0, "Must specify a project prefix.");
	}

	sprintf(filename, "Maps\\%s", pInfo->pcProjectPrefix);
	StructCopyString(&pInfo->pcProjectDirectory, filename);
	if (dirExists(pInfo->pcProjectDirectory))
	{
		MEContextGenericErrorAdd(pErrorList, "ProjectDirectory", 0, "Project directory %s already exists.", pInfo->pcProjectDirectory);
	}

	sprintf(filename, "%s\\%s.Mission", pInfo->pcProjectDirectory, pInfo->pcProjectPrefix);
	StructCopyString(&pInfo->pcMissionFile, filename);
	if (fileExists(pInfo->pcMissionFile))
	{
		MEContextGenericErrorAdd(pErrorList, "MissionFile", 0, "File %s already exists.", pInfo->pcMissionFile);
	}
}

void ugcEditorFreezeProjectValidateMap(UGCFreezeProjectInfo *pInfo, UGCFreezeProjectMapInfo *pMapInfo, MEFieldGenericErrorList *pErrorList)
{
	char filename[256];

	if (!pMapInfo->pcOutMapName || !pMapInfo->pcOutMapName[0])
	{
		MEContextGenericErrorAdd(pErrorList, "OutMapName", 0, "Must specify a map logical name.");
	}

	sprintf(filename, "%s\\%s", pInfo->pcProjectDirectory, pMapInfo->pcOutMapName);
	StructCopyString(&pMapInfo->pcMapDirectory, filename);
	if (dirExists(pMapInfo->pcMapDirectory))
	{
		MEContextGenericErrorAdd(pErrorList, "MapDirectory", 0, "Map directory %s already exists.", pMapInfo->pcMapDirectory);
	}

	sprintf(filename, "%s\\%s.zone", pMapInfo->pcMapDirectory, pMapInfo->pcOutMapName);
	StructCopyString(&pMapInfo->pcMapFile, filename);
	if (fileExists(pMapInfo->pcMapFile))
	{
		MEContextGenericErrorAdd(pErrorList, "MapFile", 0, "File %s already exists.", pMapInfo->pcMapFile);
	}

	sprintf(filename, "%s\\Default.layer", pMapInfo->pcMapDirectory);
	StructCopyString(&pMapInfo->pcMapLayerFile, filename);
	if (fileExists(pMapInfo->pcMapLayerFile))
	{
		MEContextGenericErrorAdd(pErrorList, "MapLayerFile", 0, "File %s already exists.", pMapInfo->pcMapLayerFile);
	}

	sprintf(filename, "%s\\Missions\\%s_Mission_Ugc_Openmission.Mission", pMapInfo->pcMapDirectory, pMapInfo->pcOutMapName);
	StructCopyString(&pMapInfo->pcMapMissionFile1, filename);
	if (fileExists(pMapInfo->pcMapMissionFile1))
	{
		MEContextGenericErrorAdd(pErrorList, "MapMissionFile1", 0, "File %s already exists.", pMapInfo->pcMapMissionFile1);
	}

	sprintf(filename, "%s\\Missions\\%s_Mission_Ugc_Completed_Openmission.Mission", pMapInfo->pcMapDirectory, pMapInfo->pcOutMapName);
	StructCopyString(&pMapInfo->pcMapMissionFile2, filename);
	if (fileExists(pMapInfo->pcMapMissionFile2))
	{
		MEContextGenericErrorAdd(pErrorList, "MapMissionFile2", 0, "File %s already exists.", pMapInfo->pcMapMissionFile2);
	}
}

void ugcEditorFreezeProjectValidateCostume(UGCFreezeProjectInfo *pInfo, UGCFreezeProjectCostumeInfo *pCostumeInfo, MEFieldGenericErrorList *pErrorList)
{
	int iField;
	char *estrOutName = NULL;
	char filename[256];
	UGCPerProjectDefaults *pDefaults = ugcGetDefaults();
	for (iField = 0; iField < eaSize(&pDefaults->eaCostumeNamingConventionFields); iField++)
	{
		UGCCostumeNamingConventionField *pField = pDefaults->eaCostumeNamingConventionFields[iField];
		if (stricmp(pField->pcFieldName, "UGC_MissionName") == 0)
		{
			if (estrOutName)
				estrAppend2(&estrOutName, "_");
			estrAppend2(&estrOutName, "Msn_");
			estrAppend2(&estrOutName, pInfo->pcProjectPrefix);
		}
		else
		{
			char *pName = eaGet(&pCostumeInfo->eaCostumeNameParts, iField);
			if (!pName || pName[0] == '\0')
			{
				if (!pField->bOptional)
					MEContextGenericErrorAdd(pErrorList, "CostumeNamePart", iField, "Field %s must be filled in.", pField->pcFieldName);
			}
			else
			{
				if (estrOutName)
					estrAppend2(&estrOutName, "_");
				estrAppend2(&estrOutName, pName);
			}
		}
	}

	StructCopyString(&pCostumeInfo->pcOutCostumeName, estrOutName);
	estrDestroy(&estrOutName);

	sprintf(filename, "%s\\Costumes\\%s.Costume", pInfo->pcProjectDirectory, pCostumeInfo->pcOutCostumeName);
	StructCopyString(&pCostumeInfo->pcCostumeFile, filename);
	if (fileExists(pCostumeInfo->pcCostumeFile))
	{
		MEContextGenericErrorAdd(pErrorList, "CostumeFile", 0, "Costume file %s already exists.", pCostumeInfo->pcCostumeFile);
	}
}

void ugcEditorFreezeProjectRefresh(bool bPartialRefresh)
{
	UGCFreezeProjectInfo *pInfo;
	UGCProjectData *data;
	MEFieldContext *pContext;
	MEFieldContextEntry *pWindowEntry, *pEntry;
	MEFieldGenericErrorList *pErrorList;
	UGCPerProjectDefaults *pDefaults = ugcGetDefaults();
	char buffer[256];
	bool bHaveErrors = false;

	data = ugcEditorGetProjectData();

	if (isProductionMode() || !data)
		return;

	pContext = MEContextPush("UGCEditorFreezeProjectDialog", NULL, NULL, NULL);
	pContext->cbChanged = ugcEditorFreezeProjectCB;

	pWindowEntry = MEContextCreateWindowParent("Commit Quest to layers", 800, 600, true, "CommitProjectWindow");
	pInfo = MEContextAllocStruct("FreezeProjectInfo", parse_UGCFreezeProjectInfo, false);
	pErrorList = MEContextAllocStruct("FreezeProjectErrors", parse_MEFieldGenericErrorList, true);

	// Fixup pInfo
	FOR_EACH_IN_EARRAY(data->maps, UGCMap, map)
	{
		UGCFreezeProjectMapInfo *found_info = NULL;
		FOR_EACH_IN_EARRAY(pInfo->eaMaps, UGCFreezeProjectMapInfo, map_info)
		{
			if (map_info->astrInternalMapName == map->pcName)
			{
				found_info = map_info;
				break;
			}
		}
		FOR_EACH_END;
		if (!found_info)
		{
			found_info = StructCreate(parse_UGCFreezeProjectMapInfo);
			found_info->astrInternalMapName = map->pcName;
			eaPush(&pInfo->eaMaps, found_info);
		}

		StructCopyString(&found_info->pcDisplayName, map->pcDisplayName);

		if (!found_info->pcOutMapName)
		{
			char *estrNameOut = NULL;
			if (resFixName(map->pcDisplayName, &estrNameOut))
				found_info->pcOutMapName = StructAllocString(estrNameOut);
			else
				found_info->pcOutMapName = StructAllocString(map->pcDisplayName);
		}
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(data->costumes, UGCCostume, costume)
	{
		UGCFreezeProjectCostumeInfo *found_info = NULL;
		FOR_EACH_IN_EARRAY(pInfo->eaCostumes, UGCFreezeProjectCostumeInfo, costume_info)
		{
			if (costume_info->astrInternalCostumeName == costume->astrName)
			{
				found_info = costume_info;
				break;
			}
		}
		FOR_EACH_END;
		if (!found_info)
		{
			found_info = StructCreate(parse_UGCFreezeProjectCostumeInfo);
			found_info->astrInternalCostumeName = costume->astrName;
			eaPush(&pInfo->eaCostumes, found_info);
		}

		StructCopyString(&found_info->pcDisplayName, costume->pcDisplayName);

		while (eaSize(&found_info->eaCostumeNameParts) < eaSize(&pDefaults->eaCostumeNamingConventionFields))
		{
			eaPush(&found_info->eaCostumeNameParts, StructAllocString(""));
		}
	}
	FOR_EACH_END;

	ugcEditorFreezeProjectValidate(pInfo, pErrorList);
	if (eaSize(&pErrorList->eaErrors) > 0)
		bHaveErrors = true;

	MEContextSetErrorFunction(MEContextGenericErrorFunction);
	MEContextSetErrorContext(pErrorList);
	MEContextSetErrorIcon("ugc_icons_labels_alert", -1, -1);

	MEContextPush("UGCEditorFreezeProjectDialog_Info", pInfo, pInfo, parse_UGCFreezeProjectInfo);
	MEContextCreateScrollAreaParent(500, "CommitProjectScroll");

	MEContextAddLabel("DescriptionLabel", "You are saving resources from this project to the game data directory.", NULL);
	MEContextAddSimple(kMEFieldType_TextEntry, "ProjectPrefix", "Mission Name", "This is the unique top-level name for the project.");

	pEntry = MEContextAddTwoLabels("ProjectLabel", "DIRECTORY", pInfo->pcProjectDirectory, "The name of the root directory that all the files will be written to.");
	MEContextSetEntryErrorForField(pEntry, "ProjectDirectory");

	pEntry = MEContextAddTwoLabels("MissionLabel", "FILE", pInfo->pcMissionFile, "The name of the file that the Mission will be written to.");
	MEContextSetEntryErrorForField(pEntry, "MissionFile");

	FOR_EACH_IN_EARRAY(pInfo->eaMaps, UGCFreezeProjectMapInfo, pMapInfo)
	{
		char context_name[256];
		char error_list_name[256];
		MEFieldGenericErrorList *pMapErrorList;
		
		sprintf(error_list_name, "FreezeProjectErrors_Map_%d", FOR_EACH_IDX(pInfo->eaMaps, pMapInfo));
		pMapErrorList = MEContextAllocStruct(error_list_name, parse_MEFieldGenericErrorList, true);
		ugcEditorFreezeProjectValidateMap(pInfo, pMapInfo, pMapErrorList);
		if (eaSize(&pMapErrorList->eaErrors) > 0)
			bHaveErrors = true;

		pContext->iYPos += 5;

		sprintf(context_name, "UGCEditorFreezeProjectDialog_Map_%d", FOR_EACH_IDX(pInfo->eaMaps, pMapInfo));
		MEContextPush(context_name, pMapInfo, pMapInfo, parse_UGCFreezeProjectMapInfo);
		MEContextSetErrorContext(pMapErrorList);

		sprintf(buffer, "For map \"%s\":", pMapInfo->pcDisplayName);
		MEContextAddLabel("MapLabel", buffer, NULL);

		MEContextIndentRight();

		MEContextAddSimple(kMEFieldType_TextEntry, "OutMapName", "Exported Map Name", "This is the new name for the exported map.");

		pEntry = MEContextAddTwoLabels("MapDirectory",		"DIRECTORY",	pMapInfo->pcMapDirectory,		"The name of the directory that all the files for this map will be written to.");
		pEntry = MEContextAddTwoLabels("MapFile",			"FILE",			pMapInfo->pcMapFile,			"The name of the file that the ZoneMap will be written to.");
		pEntry = MEContextAddTwoLabels("MapLayerFile",		"FILE",			pMapInfo->pcMapLayerFile,		"The name of the file that the map layer will be written to.");
		pEntry = MEContextAddTwoLabels("MapMissionFile1",	"FILE",			pMapInfo->pcMapMissionFile1,	"The name of the file that the open mission will be written to.");
		pEntry = MEContextAddTwoLabels("MapMissionFile2",	"FILE",			pMapInfo->pcMapMissionFile2,	"The name of the file that the open mission will be written to.");

		MEContextIndentLeft();

		MEContextPop(context_name);
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(pInfo->eaCostumes, UGCFreezeProjectCostumeInfo, pCostumeInfo)
	{
		int iField;
		char context_name[256];
		char error_list_name[256];
		MEFieldGenericErrorList *pCostumeErrorList;
		
		sprintf(error_list_name, "FreezeProjectErrors_Costume_%d", FOR_EACH_IDX(pInfo->eaCostumes, pCostumeInfo));
		pCostumeErrorList = MEContextAllocStruct(error_list_name, parse_MEFieldGenericErrorList, true);
		ugcEditorFreezeProjectValidateCostume(pInfo, pCostumeInfo, pCostumeErrorList);
		if (eaSize(&pCostumeErrorList->eaErrors) > 0)
			bHaveErrors = true;

		pContext->iYPos += 5;

		sprintf(context_name, "UGCEditorFreezeProjectDialog_Costume_%d", FOR_EACH_IDX(pInfo->eaCostumes, pCostumeInfo));
		MEContextPush(context_name, pCostumeInfo, pCostumeInfo, parse_UGCFreezeProjectCostumeInfo);
		MEContextSetErrorContext(pCostumeErrorList);

		sprintf(buffer, "For costume \"%s\":", pCostumeInfo->pcDisplayName);
		MEContextAddLabel("CostumeLabel", buffer, NULL);

		MEContextIndentRight();

		for (iField = 0; iField < eaSize(&pDefaults->eaCostumeNamingConventionFields); iField++)
		{
			UGCCostumeNamingConventionField *pField = pDefaults->eaCostumeNamingConventionFields[iField];
			if (stricmp(pField->pcFieldName, "UGC_MissionName") != 0)
			{
				MEContextAddIndex(kMEFieldType_TextEntry, "CostumeNamePart", iField, pField->pcFieldName, pField->pcTooltip);
			}
		}

		pEntry = MEContextAddTwoLabels("OutCostumeName", "Costume Name", pCostumeInfo->pcOutCostumeName, "The final name of the costume resource that will be created.");
		pEntry = MEContextAddTwoLabels("CostumeFile", "FILE", pCostumeInfo->pcCostumeFile, "The name of the file that the costume will be written to.");

		MEContextIndentLeft();

		MEContextPop(context_name);
	}
	FOR_EACH_END;

	MEContextPop("UGCEditorFreezeProjectDialog_Info");

	MEContextSetEnabled( !bHaveErrors );
	if (bHaveErrors)
		pEntry = MEContextAddButton("Commit to layers", NULL, ugcEditorFreezeProjectOK, pInfo, "OKButton", NULL, "Write out the project resources to the game data directory. [DISABLED: See errors above.]");
	else
		pEntry = MEContextAddButton("Commit to layers", NULL, ugcEditorFreezeProjectOK, pInfo, "OKButton", NULL, "Write out the project resources to the game data directory.");
	ui_WidgetSetPositionEx(UI_WIDGET(ENTRY_BUTTON(pEntry)), 10, 10, 0, 0, UIBottomRight);
	ui_WidgetSetWidth(UI_WIDGET(ENTRY_BUTTON(pEntry)), 200);
	MEContextSetEnabled( true );

	pEntry = MEContextAddButton("Cancel", NULL, ugcEditorFreezeProjectCancel, NULL, "CancelButton", NULL, "Close this dialog.");
	ui_WidgetSetPositionEx(UI_WIDGET(ENTRY_BUTTON(pEntry)), 220, 10, 0, 0, UIBottomRight);
	ui_WidgetSetWidth(UI_WIDGET(ENTRY_BUTTON(pEntry)), 200);

	MEContextPop("UGCEditorFreezeProjectDialog");
}

void ugcEditorFreezeProject(void)
{
	if(!isProductionEditMode()) return;

	ugcEditorFreezeProjectRefresh(false);
}

bool ugcEditorQueryCommandByID(UGCActionID command_id, char** out_estr)
{
	UGCActionDescription *desc = ugcEditorGetAction(command_id);
	bool ret = false;
	if (command_id == UGC_ACTION_LOGOUT)
		return true;
	if (!g_UGCEditorDoc || !g_UGCEditorDoc->data || !g_UGCEditorDoc->data->project)
		return false;
	if (desc)
	{
		switch (command_id)
		{
		xcase UGC_ACTION_SAVE:
			// The undo stack is cleared on save.  This detects if we
			// have made any changes.
			return EditCanUndoLast( g_UGCEditorDoc->edit_undo_stack ) || EditCanRedoLast( g_UGCEditorDoc->edit_undo_stack );
		xcase UGC_ACTION_PUBLISH:
			if( g_UGCEditorDoc->pCachedStatus && !g_UGCEditorDoc->pCachedStatus->bCurrentlyPublishing && g_UGCEditorDoc->bPublishingEnabled )
			{
				desc->strDescriptionMessage = "UGC.Action_Publish_Tooltip";

				return true;
			}
			else
			{
				if(!g_UGCEditorDoc->bPublishingEnabled)
					desc->strDescriptionMessage = "UGC.Action_Publish_Disabled_Tooltip";

				return false;
			}
		xcase UGC_ACTION_WITHDRAW:
			if( g_UGCEditorDoc->pCachedStatus && !g_UGCEditorDoc->pCachedStatus->bCurrentlyPublishing && g_UGCEditorDoc->pCachedStatus->bLastPublishSucceeded ) {
				return true;
			} else {
				return false;
			}
				
		xcase UGC_ACTION_FREEZE:
			return isDevelopmentMode();
		xcase UGC_ACTION_CLOSE:
			return true;
		xcase UGC_ACTION_PLAY_MISSION:
			return SAFE_MEMBER( g_UGCEditorDoc->data, mission ) && eaSize( &g_UGCEditorDoc->data->mission->objectives ) > 0;
		xcase UGC_ACTION_UNDO:
			if( !nullStr( g_UGCEditorDoc->activeView.playingEditorMap )) {
				UGCEditorView* opView = EditUndoLastExtraData( g_UGCEditorDoc->edit_undo_stack );
				if(   !opView || nullStr( opView->playingEditorMap )
					  || !resNamespaceBaseNameEq( opView->playingEditorMap, g_UGCEditorDoc->activeView.playingEditorMap )) {
					return false;
				}
			}
			return EditCanUndoLast(g_UGCEditorDoc->edit_undo_stack);
		xcase UGC_ACTION_REDO:
			if( !nullStr( g_UGCEditorDoc->activeView.playingEditorMap )) {
				UGCEditorView* opView = EditRedoLastExtraData( g_UGCEditorDoc->edit_undo_stack );
				if(   !opView || nullStr( opView->playingEditorMap )
					  || !resNamespaceBaseNameEq( opView->playingEditorMap, g_UGCEditorDoc->activeView.playingEditorMap )) {
					return false;
				}
			}
			return EditCanRedoLast(g_UGCEditorDoc->edit_undo_stack);

		xcase UGC_ACTION_MAP_CREATE:
		case UGC_ACTION_MAP_IMPORT:
		case UGC_ACTION_COSTUME_CREATE:
		case UGC_ACTION_ITEM_CREATE:
		case UGC_ACTION_VIEW_EULA:
		case UGC_ACTION_REPORT_BUG:
			return true;
		}

		if( !ugcEditorIsActive() ) {
			ret = ugcPlayingEditorQueryAction( command_id, out_estr );
		} else {
			switch( g_UGCEditorDoc->activeView.mode ) {
				xcase UGC_VIEW_MAP_EDITOR: {
					UGCMapEditorDoc* pSubDoc=NULL;
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, sub_doc)
					{
						if (ugcMapEditorGetName(sub_doc) == g_UGCEditorDoc->activeView.name)
						{
							pSubDoc=sub_doc;
						}
					}
					FOR_EACH_END;
					// Do an action query check even if the doc is NULL so that the query action contains
					//   all of the validity checking rather than the NULL case being handled by fallthrough
					//   out of this function
					ret = ugcMapEditorQueryAction(pSubDoc, command_id, out_estr);
				}

				xcase UGC_VIEW_NO_MAPS:
					ugcNoMapsEditorQueryAction( g_UGCEditorDoc->no_maps_editor, command_id, out_estr );
				

				xcase UGC_VIEW_MISSION:
					ret = ugcMissionDocQueryAction(g_UGCEditorDoc->mission_editor, command_id, out_estr);
				
				xcase UGC_VIEW_COSTUME:
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, editor)
					{
						if (ugcCostumeEditor_GetCostume(editor)->astrName == g_UGCEditorDoc->activeView.name)
							ret = ugcCostumeEditor_QueryAction(editor, command_id, out_estr);
					}
					FOR_EACH_END;

				xcase UGC_VIEW_NO_COSTUMES:
					ret = ugcNoCostumesEditor_QueryAction(g_UGCEditorDoc->no_costumes_editor, command_id, out_estr);

				xcase UGC_VIEW_NO_DIALOG_TREES:
					ret = ugcNoDialogTreesDocQueryAction( g_UGCEditorDoc->no_dialogs_editor, command_id, out_estr );
				
				xcase UGC_VIEW_ITEM:
					ret = ugcItemEditorQueryAction(g_UGCEditorDoc->item_editor, command_id, out_estr);

				xcase UGC_VIEW_DIALOG_TREE:
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->dialog_editors, UGCDialogTreeDoc, editor)
					{
						if (ugcDialogTreeDocGetIDAsPtr(editor) == g_UGCEditorDoc->activeView.name)
							ret = ugcDialogTreeDocQueryAction(editor, command_id, out_estr);
					}
					FOR_EACH_END;
			}
		}
	}
	return ret;
}

void ugcEditorExecuteCommandByID(UGCActionID command_id)
{
	const UGCActionDescription *desc = ugcEditorGetAction(command_id);
	if (desc)
	{
		char* estr = NULL;
		if (!desc->bIsCheckBox && !ugcEditorQueryCommandByID(command_id, &estr)) {
			estrDestroy( &estr );
			return;
		}
		estrDestroy( &estr );

		printf("Executing %s...\n", StaticDefineIntRevLookup( UGCActionIDEnum, desc->eID ));

		switch( command_id ) {
			xcase UGC_ACTION_SAVE:
				ugcEditorSave();
				return;

			xcase UGC_ACTION_PUBLISH:
				ugcEditorSaveAndPublishProject();
				return;

			xcase UGC_ACTION_FREEZE:
				ugcEditorFreezeProject();
				return;

			xcase UGC_ACTION_CLOSE:
				ClientGoToCharacterSelectAndChoosePreviousForUGC();
				return;

			xcase UGC_ACTION_LOGOUT:
				ClientGoToCharacterSelect();
				return;
			
			xcase UGC_ACTION_UNDO:
				ugcEditorUndo();
				return;

			xcase UGC_ACTION_REDO:
				ugcEditorRedo();
				return;

			xcase UGC_ACTION_WITHDRAW:
				ServerCmd_gslWithdrawUGCProject();
				return;

			xcase UGC_ACTION_REPORT_BUG:
				ugc_ReportBug();
				return;

			xcase UGC_ACTION_COSTUME_CREATE:
				ugcEditorCreateNewCostume( NULL, NULL );
				return;

			xcase UGC_ACTION_COSTUME_DUPLICATE:
				ugcEditorDuplicateCostume();
				return;

			xcase UGC_ACTION_COSTUME_DELETE:
				ugcEditorDeleteCostume();
				return;

			xcase UGC_ACTION_VIEW_EULA:
				ugcEditorShowEULA(NULL);
				return;

			xcase UGC_ACTION_MAP_CREATE:
				ugcEditorCreateNewMap( false );
				return;

			xcase UGC_ACTION_ITEM_CREATE:
				ugcEditorCreateNewItem( NULL, NULL );
				return;
		}

		
		if( !ugcEditorIsActive() ) {
			ugcPlayingEditorHandleAction( command_id );
		} else {
			switch( g_UGCEditorDoc->activeView.mode ) {
				xcase UGC_VIEW_MAP_EDITOR:
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->map_editors, UGCMapEditorDoc, sub_doc)
					{
						if (ugcMapEditorGetName(sub_doc) == g_UGCEditorDoc->activeView.name)
							ugcMapEditorHandleAction(sub_doc, command_id);
					}
					FOR_EACH_END;

				xcase UGC_VIEW_NO_MAPS:
					ugcNoMapsEditorHandleAction( g_UGCEditorDoc->no_maps_editor, command_id );

				xcase UGC_VIEW_MISSION:
					ugcMissionDocHandleAction(g_UGCEditorDoc->mission_editor, command_id);
				
				xcase UGC_VIEW_COSTUME:
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->costume_editors, UGCCostumeEditorDoc, editor)
					{
						if (ugcCostumeEditor_GetCostume(editor)->astrName == g_UGCEditorDoc->activeView.name)
							ugcCostumeEditor_HandleAction(editor, command_id);
					}
					FOR_EACH_END;

				xcase UGC_VIEW_NO_COSTUMES:
					ugcNoCostumesEditor_HandleAction(g_UGCEditorDoc->no_costumes_editor, command_id);

				xcase UGC_VIEW_NO_DIALOG_TREES:
					ugcNoDialogTreesDocHandleAction( g_UGCEditorDoc->no_dialogs_editor, command_id );
				
				xcase UGC_VIEW_ITEM:
					ugcItemEditorHandleAction(g_UGCEditorDoc->item_editor, command_id);

				xcase UGC_VIEW_DIALOG_TREE:
					FOR_EACH_IN_EARRAY(g_UGCEditorDoc->dialog_editors, UGCDialogTreeDoc, editor)
					{
						if (ugcDialogTreeDocGetIDAsPtr(editor) == g_UGCEditorDoc->activeView.name)
							ugcDialogTreeDocHandleAction(editor, command_id);
					}
					FOR_EACH_END;
			}
		}
	}
}

AUTO_COMMAND ACMD_NAME("UGC.Do") ACMD_ACCESSLEVEL(0);
void ugcEditorExecuteCommand(ACMD_NAMELIST(UGCActionIDEnum,STATICDEFINE) char *command_name)
{
	UGCActionID command_id = StaticDefineIntGetInt(UGCActionIDEnum, command_name);
	ugcEditorExecuteCommandByID(command_id);
}

AUTO_COMMAND ACMD_NAME("ugc_ShowEditor") ACMD_ACCESSLEVEL(2);
void ugcEditorShowEditor(void)
{
	if(!isProductionEditMode()) return;

	ugcEditorQueueApplyUpdate();
	gQueueReturnToEditor = true;
}

AUTO_COMMAND ACMD_NAME("ugc_ResetMap") ACMD_ACCESSLEVEL(2);
void ugcEditorResetMap( void )
{
	Entity *pEnt = entActivePlayerPtr();
	
	if( !isProductionEditMode() ) {
		return;
	}

	if (pEnt!=NULL)
	{
		// Stop interaction. In case we have a dialog up or something
		interaction_ClearPlayerInteractState(pEnt);
	}

	ServerCmd_gslUGCResetMap( ugcEditorGetProjectData() );
}

AUTO_COMMAND;
void ugcFixupUgcInfo(char *filename)
{
	int total;
	char *buf;
	char located_name[256];
	FILE *fIn;
	if (!fileLocateRead(filename, located_name))
	{
		Alertf("Cannot find input file %s!", filename);
		return;
	}
	fIn = fopen(located_name, "rt");
	if (!fIn)
	{
		Alertf("Cannot read input file %s!", located_name);
		return;
	}
	fseek(fIn, 0, SEEK_END);
	total = ftell(fIn);
	fseek(fIn, 0, SEEK_SET);

	buf = malloc(total+1);
	total = (int)fread(buf, 1, total, fIn);
	buf[total] = 0;
	fclose(fIn);

	if (buf)
	{
		char map_name[64];
		char obj_name[128];
		char msg_string[2048];
		int i;
		char new_filename[CRYPTIC_MAX_PATH], out_filename[CRYPTIC_MAX_PATH];
		FILE *fMsgOut;
		FILE *fFileOut;
		changeFileExt(filename, ".ugcinfo", new_filename);
		if (stricmp(filename, new_filename) == 0)
		{
			Alertf("Output file is the same as input file; don't use .ugcinfo!");
			return;
		}
		fileLocateWrite(new_filename, out_filename);
		fFileOut = fopen(out_filename, "wt");
		if (!fFileOut)
		{
			Alertf("Cannot open message file %s!", out_filename);
			return;
		}
		changeFileExt(filename, ".ugcinfo.ms", new_filename);
		fileLocateWrite(new_filename, out_filename);
		fMsgOut = fopen(out_filename, "wt");
		if (!fMsgOut)
		{
			Alertf("Cannot open message file %s!", out_filename);
			return;
		}
		for (i = 0; i < total; i++)
		{
			if (strStartsWith(&buf[i], "MapName"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(map_name, &buf[i+8], end-&buf[i+8]);
				fwrite(&buf[i], 1, 1, fFileOut);
			}
			else if (strStartsWith(&buf[i], "UGCObject"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(obj_name, &buf[i+10], end-&buf[i+10]);
				fwrite(&buf[i], 1, 1, fFileOut);
			}
			else if (strStartsWith(&buf[i], "DisplayNameString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+18], end-&buf[i+18]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.%s.%s.Name\n\tDefaultstring \"%s\"\n}\n\n", map_name, obj_name, msg_string);
				fprintf(fFileOut, "DisplayName UGC.%s.%s.Name\n", map_name, obj_name);
				i += (end-&buf[i]);
			}
			else if (strStartsWith(&buf[i], "DisplayDetailsString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+21], end-&buf[i+21]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.%s.%s.Desc\n\tDefaultstring \"%s\"\n}\n\n", map_name, obj_name, msg_string);
				fprintf(fFileOut, "DisplayDetails UGC.%s.%s.Desc\n", map_name, obj_name);
				i += (end-&buf[i]);
			}
			else
			{
				fwrite(&buf[i], 1, 1, fFileOut);
			}
		}
		fclose(fFileOut);
		fclose(fMsgOut);
		SAFE_FREE(buf);
	}
	else
	{
		Alertf("File not found!");
		return;
	}
}

AUTO_COMMAND;
void ugcFixupAnimlist(char *filename)
{
	int total;
	char *buf;
	char located_name[256];
	FILE *fIn;
	if (!fileLocateRead(filename, located_name))
	{
		Alertf("Cannot find input file %s!", filename);
		return;
	}
	fIn = fopen(located_name, "rt");
	if (!fIn)
	{
		Alertf("Cannot read input file %s!", located_name);
		return;
	}
	fseek(fIn, 0, SEEK_END);
	total = ftell(fIn);
	fseek(fIn, 0, SEEK_SET);

	buf = malloc(total+1);
	total = (int)fread(buf, 1, total, fIn);
	buf[total] = 0;
	fclose(fIn);

	if (buf)
	{
		char al_name[256];
		char msg_string[2048];
		int i;
		char new_filename[CRYPTIC_MAX_PATH], out_filename[CRYPTIC_MAX_PATH];
		FILE *fMsgOut;
		FILE *fFileOut;
		changeFileExt(filename, ".al", new_filename);
		if (stricmp(filename, new_filename) == 0)
		{
			Alertf("Output file is the same as input file; change input file to .al.input!");
			return;
		}
		fileLocateWrite(new_filename, out_filename);
		fFileOut = fopen(out_filename, "wt");
		if (!fFileOut)
		{
			Alertf("Cannot open message file %s!", out_filename);
			return;
		}
		changeFileExt(filename, ".al.ms", new_filename);
		fileLocateWrite(new_filename, out_filename);
		fMsgOut = fopen(out_filename, "wt");
		if (!fMsgOut)
		{
			Alertf("Cannot open message file %s!", out_filename);
			return;
		}
		for (i = 0; i < total; i++)
		{
			if (strStartsWith(&buf[i], "AIAnimList"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(al_name, &buf[i+11], end-&buf[i+11]);
				fwrite(&buf[i], 1, 1, fFileOut);
			}
			else if (strStartsWith(&buf[i], "DisplayNameString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+18], end-&buf[i+18]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.AIAnimList.%s.Name\n\tDefaultstring \"%s\"\n}\n\n", al_name, msg_string);
				fprintf(fFileOut, "VisibleName UGC.AIAnimList.%s.Name\n", al_name);
				i += (end-&buf[i]);
			}
			else if (strStartsWith(&buf[i], "DisplayDetailsString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+21], end-&buf[i+21]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.AIAnimList.%s.Desc\n\tDefaultstring \"%s\"\n}\n\n", al_name, msg_string);
				fprintf(fFileOut, "Description UGC.AIAnimList.%s.Desc\n", al_name);
				i += (end-&buf[i]);
			}
			else
			{
				fwrite(&buf[i], 1, 1, fFileOut);
			}
		}
		fclose(fFileOut);
		fclose(fMsgOut);
		SAFE_FREE(buf);
	}
	else
	{
		Alertf("File not found!");
		return;
	}
}

AUTO_COMMAND;
void ugcFixupUGCResInfo(char *filename, char* dictname)
{
	int total;
	char *buf;
	char located_name[256];
	FILE *fIn;
	if (!fileLocateRead(filename, located_name))
	{
		Alertf("Cannot find input file %s!", filename);
		return;
	}
	fIn = fopen(located_name, "rt");
	if (!fIn)
	{
		Alertf("Cannot read input file %s!", located_name);
		return;
	}
	fseek(fIn, 0, SEEK_END);
	total = ftell(fIn);
	fseek(fIn, 0, SEEK_SET);

	buf = malloc(total+1);
	total = (int)fread(buf, 1, total, fIn);
	buf[total] = 0;
	fclose(fIn);

	if (buf)
	{
		char al_name[256];
		char var_name[256];
		char msg_string[2048];
		int i;
		char new_filename[CRYPTIC_MAX_PATH], out_filename[CRYPTIC_MAX_PATH];
		FILE *fMsgOut;
		FILE *fFileOut;
		changeFileExt(filename, ".ugcresinfo", new_filename);
		if (stricmp(filename, new_filename) == 0)
		{
			Alertf("Output file is the same as input file; change input file to .ugcresinfo_input!");
			return;
		}
		fileLocateWrite(new_filename, out_filename);
		fFileOut = fopen(out_filename, "wt");
		if (!fFileOut)
		{
			Alertf("Cannot open message file %s!", out_filename);
			return;
		}
		changeFileExt(filename, ".ugcresinfo.ms", new_filename);
		fileLocateWrite(new_filename, out_filename);
		fMsgOut = fopen(out_filename, "wt");
		if (!fMsgOut)
		{
			Alertf("Cannot open message file %s!", out_filename);
			return;
		}
		for (i = 0; i < total; i++)
		{
			if (strStartsWith(&buf[i], "Resource"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(al_name, &buf[i+9], end-&buf[i+9]);
				fwrite(&buf[i], 1, 1, fFileOut);
			}
			else if (strStartsWith(&buf[i], "VisibleNameString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+18], end-&buf[i+18]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.ResInfo.%s.%s.Name\n\tDefaultstring \"%s\"\n}\n\n", dictname, al_name, msg_string);
				fprintf(fFileOut, "VisibleName UGC.ResInfo.%s.%s.Name\n", dictname, al_name);
				i += (end-&buf[i]);
			}
			else if (strStartsWith(&buf[i], "DefaultNameString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+18], end-&buf[i+18]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.ResInfo.%s.%s.DefaultName\n\tDefaultstring \"%s\"\n}\n\n", dictname, al_name, msg_string);
				fprintf(fFileOut, "DefaultName UGC.ResInfo.%s.%s.DefaultName\n", dictname, al_name);
				i += (end-&buf[i]);
			}
			else if (strStartsWith(&buf[i], "DescriptionString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+18], end-&buf[i+18]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.ResInfo.%s.%s.Desc\n\tDefaultstring \"%s\"\n}\n\n", dictname, al_name, msg_string);
				fprintf(fFileOut, "Description UGC.ResInfo.%s.%s.Desc\n", dictname, al_name);
				i += (end-&buf[i]);
			}

			// Support for FSM ExternVars
			else if (strStartsWith(&buf[i], "ExternVar"))
			{
				char* end = strchr(&buf[i], '\n');
				strncpy(var_name, &buf[i+10], end-&buf[i+10]);
				fwrite(&buf[i], 1, 1, fFileOut);
			}
			else if (strStartsWith(&buf[i], "VarNameString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+14], end-&buf[i+14]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.ExternVar.%s.%s.Name\n\tDefaultstring \"%s\"\n}\n\n", al_name, var_name, msg_string);
				fprintf(fFileOut, "DisplayName UGC.ExternVar.%s.%s.Name\n", al_name, var_name);
				i += (end-&buf[i]);
			}
			else if (strStartsWith(&buf[i], "VarTooltipString"))
			{
				char *end = strchr(&buf[i], '\n');
				strncpy(msg_string, &buf[i+17], end-&buf[i+17]);

				fprintf(fMsgOut, "Message\n{\n\tMessageKey UGC.ExternVar.%s.%s.Tooltip\n\tDefaultstring \"%s\"\n}\n\n", al_name, var_name, msg_string);
				fprintf(fFileOut, "Tooltip UGC.ExternVar.%s.%s.Tooltip\n", al_name, var_name);
				i += (end-&buf[i]);
			}

			// Default behavior
			else
			{
				fwrite(&buf[i], 1, 1, fFileOut);
			}
		}
		fclose(fFileOut);
		fclose(fMsgOut);
		SAFE_FREE(buf);
	}
	else
	{
		Alertf("File not found!");
		return;
	}
}

AUTO_COMMAND;
void ugcFixupCSVResInfo( char* filename, char* dictName )
{
	int bufSize;
	char* buf = fileAlloc( filename, &bufSize );
	char* lineIt;
	char* context = NULL;
	FILE* resOut = NULL;
	FILE* messageOut = NULL;

	if( !buf ) {
		Alertf( "Could not read input file %s!", filename );
		return;
	}
	
	{
		char newFilename[ CRYPTIC_MAX_PATH ];
		char outFilename[ CRYPTIC_MAX_PATH ];
		changeFileExt( filename, ".ugcresinfo", newFilename );
		if( stricmp( filename, newFilename ) == 0 ) {
			Alertf( "Output file is the same as input file; change input file to .csv!" );
		}
		fileLocateWrite( newFilename, outFilename );
		resOut = fopen( outFilename, "wt" );
		if( !resOut ) {
			Alertf( "Cannot open output file %s!", newFilename );
		}

		changeFileExt( filename, ".ugcresinfo.ms", newFilename );
		fileLocateWrite( newFilename, outFilename );
		messageOut = fopen( outFilename, "wt" );
		if( !messageOut ) {
			Alertf( "Cannot open output file %s!", newFilename );
		}
	}

	if( !resOut || !messageOut ) {
		fclose( resOut );
		fclose( messageOut );
		return;
	}

	lineIt = strtok_s( buf, "\n", &context );
	do {
		char** lineFields = NULL;
		// Skip the byte order marks
		if( strStartsWith( lineIt, "\xEF\xBB\xBF" )) {
			lineIt += 3;
		}
		// Detect comment lines
		if( strStartsWith( lineIt, "//" )) {
			continue;
		}
		DivideString( lineIt, "\t", &lineFields, DIVIDESTRING_RESPECT_SIMPLE_QUOTES );

		{
			const char* resName = eaGet( &lineFields, 0 );
			const char* displayName = eaGet( &lineFields, 1 );
			const char* defaultName = eaGet( &lineFields, 2 );
			const char* displayDesc = eaGet( &lineFields, 3 );
			char* tags = NULL;
			char displayNameKey[ RESOURCE_NAME_MAX_SIZE ];
			char defaultNameKey[ RESOURCE_NAME_MAX_SIZE ];
			char displayDescKey[ RESOURCE_NAME_MAX_SIZE ];
			{
				int tagIt;
				for( tagIt = 3; tagIt < eaSize( &lineFields ); ++tagIt ) {
					char* tag = lineFields[ tagIt ];
					if( StringIsAllWhiteSpace( tag )) {
						continue;
					}
					estrConcatf( &tags, "%s, ", tag );
				}
			}

			sprintf( displayNameKey, "UGC.ResInfo.%s.%s.Name", dictName, resName );
			sprintf( defaultNameKey, "UGC.ResInfo.%s.%s.DefaultName", dictName, resName );
			sprintf( displayDescKey, "UGC.ResInfo.%s.%s.Desc", dictName, resName );

			fprintf( resOut, "\nResource %s\n{\n\tDictionary %s\n", resName, dictName );
			if( !nullStr( tags )) {
				fprintf( resOut, "\tTags \"%s\"\n", tags );
			}
			if( !nullStr( displayName )) {
				fprintf( resOut, "\tVisibleName %s\n", displayNameKey );
				fprintf( messageOut, "\nMessage\n{\n\tMessageKey %s\n\tDefaultString \"%s\"\n}\n",
						 displayNameKey, displayName );
			}
			if( !nullStr( defaultName )) {
				fprintf( resOut, "\tDefaultName %s\n", defaultNameKey );
				fprintf( messageOut, "\nMessage\n{\n\tMessageKey %s\n\tDefaultString \"%s\"\n}\n",
					defaultNameKey, defaultName );
			}
			if( !nullStr( displayDesc )) {
				fprintf( resOut, "\tDescription %s\n", displayDescKey );
				fprintf( messageOut, "\nMessage\n{\n\tMessageKey %s\n\tDefaultString \"%s\"\n}\n",
						 displayDescKey, displayDesc );
			}
			fprintf( resOut, "}\n" );
			
			estrDestroy( &tags );
		}
		
		eaDestroyEx( &lineFields, NULL );
	} while( lineIt = strtok_s( NULL, "\n", &context ));

	free( buf );
	fclose( resOut );
	fclose( messageOut );
}

AUTO_COMMAND;
void ugcFixupSoundFile(char *filename)
{
	int bufSize;
	char* buf = fileAlloc( filename, &bufSize );
	char* lineIt;
	char* context = NULL;
	FILE* resOut = NULL;

	if( !buf ) {
		Alertf( "Could not read input file %s!", filename );
		return;
	}
	
	{
		char newFilename[ CRYPTIC_MAX_PATH ];
		char outFilename[ CRYPTIC_MAX_PATH ];
		changeFileExt( filename, ".out", newFilename );
		if( stricmp( filename, newFilename ) == 0 ) {
			Alertf( "Output file is the same as input file; change input file to .txt!" );
			return;
		}
		fileLocateWrite( newFilename, outFilename );
		resOut = fopen( outFilename, "wt" );
		if( !resOut ) {
			Alertf( "Cannot open output file %s!", newFilename );
		}
	}

	if( !resOut ) {
		return;
	}

	lineIt = strtok_s( buf, "\r\n", &context );
	do {
		char lineWithoutSlash[ 1024 ];

		// Skip the byte order marks
		if( strStartsWith( lineIt, "\xEF\xBB\xBF" )) {
			lineIt += 3;
		}
		// Detect comment lines
		if( strStartsWith( lineIt, "//" )) {
			continue;
		}

		strcpy( lineWithoutSlash, lineIt );
		{
			char* loc;
			while( loc = strchr( lineWithoutSlash, '/' )) {
				*loc = '_';
			}
		}
		fprintf( resOut,
				 "UGCSound %s\n"
				 "{\n"
				 "\tSoundName \"%s\"\n"
				 "}\n"
				 "\n",
				 lineWithoutSlash, lineIt );
	} while( lineIt = strtok_s( NULL, "\r\n", &context ));

	free( buf );
	fclose( resOut );
}

const char** ugcComponentAnimationDefaultVals( UGCMapType map_type )
{
	const char** defaultVals = NULL;

	if (ugcIsSpaceEnabled())
	{
		static const char** groundVals = NULL;
		static const char** spaceVals = NULL;

		if( !groundVals ) {
			eaPush( &groundVals, "Region_Ground" );
		}
		if( !spaceVals ) {
			eaPush( &spaceVals, "Region_Space" );
		}

		switch( map_type ) {
			case UGC_MAP_TYPE_INTERIOR: case UGC_MAP_TYPE_PREFAB_INTERIOR:
			case UGC_MAP_TYPE_GROUND: case UGC_MAP_TYPE_PREFAB_GROUND:
				defaultVals = groundVals;
			xcase UGC_MAP_TYPE_SPACE: case UGC_MAP_TYPE_PREFAB_SPACE:
				defaultVals = spaceVals;
		}
	}

	return defaultVals;
}

/// Traverse the widget tree starting at ROOT, set FN, DATA as the
/// focus function.
void ugcWidgetTreeSetFocusCallback( UIWidget* root, UIActivationFunc fn, UserData data )
{
	int it;
	
	assert( root->onFocusF == NULL || root->onFocusF == fn );
	ui_WidgetSetFocusCallback( root, fn, data );
	for( it = 0; it != eaSize( &root->children ); ++it ) {
		ugcWidgetTreeSetFocusCallback( root->children[ it ], fn, data );
	}
}

UGCMissionObjective* ugcEditorObjectiveCreate( UGCComponentType type, UGCMissionLibraryModelType modelType, U32 id, const char* mapName )
{
	UGCProjectData* data = ugcEditorGetProjectData();
	UGCMissionObjective* accum = StructCreate( parse_UGCMissionObjective );

	switch( modelType ) {
		xcase UGCMIMO_NEW_OBJECTIVE:
			accum->type = UGCOBJ_COMPLETE_COMPONENT;
			
		xcase UGCMIMO_NEW_TALK_TO_OBJECTIVE:
			accum->type = UGCOBJ_COMPLETE_COMPONENT;
			
		xcase UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE:
			accum->type = UGCOBJ_UNLOCK_DOOR;
	
		xdefault:
			FatalErrorf( "Unsupported type to create objective" );
	}

	accum->id = id;
	if( type == UGC_COMPONENT_TYPE_OBJECT ) {
		accum->waypointMode = UGC_WAYPOINT_POINTS;
	} else {
		accum->waypointMode = UGC_WAYPOINT_AREA;
	}
	StructCopyString( &accum->strComponentInternalMapName, mapName );
	if( modelType == UGCMIMO_NEW_TALK_TO_OBJECTIVE ) {
		UGCComponent* component = ugcComponentOpCreate( data, UGC_COMPONENT_TYPE_DIALOG_TREE, 0 );
		UGCComponent* contactComponent = ugcComponentOpCreate( data, type, 0 );

		if( mapName ) {
			UGCMap* map = ugcEditorGetMapByName( mapName );
			UGCMapType mapType = ugcMapGetType( map );
			ugcComponentOpReset( data, component, mapType, false );
			ugcComponentOpReset( data, contactComponent, mapType, false );
			ugcComponentOpSetPlacement(data,contactComponent,map,GENESIS_UNPLACED_ID);
		}
		else
		{
			contactComponent->sPlacement.bIsExternalPlacement = true;
			contactComponent->sPlacement.pcExternalMapName = StructAllocString( ugcGetDefaultMapName( data ));
		}

		accum->componentID = component->uID;
		ea32Push( &component->eaObjectiveIDs, accum->id );
		component->uActorID = contactComponent->uID;

	} else {
		UGCComponent* component = ugcComponentOpCreate( data, type, 0 );
		accum->componentID = component->uID;
		
		if( mapName ) {
			UGCMap* map = ugcEditorGetMapByName( mapName );
			UGCMapType mapType = ugcMapGetType( map );
			ugcComponentOpReset( data, component, mapType, false );
			ugcComponentOpSetPlacement(data,component,map,GENESIS_UNPLACED_ID);
		}
		else
		{
			component->sPlacement.bIsExternalPlacement = true;
			component->sPlacement.pcExternalMapName = StructAllocString( ugcGetDefaultMapName( data ));
		}
	}

	return accum;
}

UGCMap* ugcEditorUninitializedMapCreate( UGCMapType type )
{
	UGCMap* new_map = NULL;
	char map_full_name[256], map_filename[MAX_PATH];
	char *map_name = NULL;
	
	ugcEditorCreateResourceName(&map_name, ugcEditorGetMapByName);
	sprintf(map_full_name, "%s:%s", g_UGCEditorDoc->data->ns_name, map_name);
	sprintf(map_filename, "ns/%s/UGC/%s.ugcmap", g_UGCEditorDoc->data->ns_name, map_name);
	estrDestroy(&map_name);
		
	new_map = StructCreate(parse_UGCMap);
	new_map->pcName = allocAddString(map_full_name);
	new_map->pcFilename = allocAddFilename(map_filename);
	new_map->pUnitializedMap = StructCreate( parse_UGCUnitializedMap );
	new_map->pUnitializedMap->eType = type;

	eaPush(&g_UGCEditorDoc->data->maps, new_map);

	// Also, create the whole-map component for that map
	{
		UGCComponent *new_com = ugcComponentOpCreate(g_UGCEditorDoc->data, UGC_COMPONENT_TYPE_WHOLE_MAP, 0);
		assert(new_com);
		ugcComponentOpSetPlacement(g_UGCEditorDoc->data, new_com, new_map, UGC_TOPLEVEL_ROOM_ID);
		setVec3(new_com->sPlacement.vPos, 0, 0, 0);
	}
	
	return new_map;
}

CharClassTypes ugcGetRegionTypeFromName(const char *pcName)
{
	// Not using static define lookup since the name passed in here is a localized one that
	// does not actually match the static define
	if (stricmp(pcName, s_pcGround) == 0) {
		return StaticDefineIntGetInt(CharClassTypesEnum, "Ground");
	} else {
		return StaticDefineIntGetInt(CharClassTypesEnum, "Space");
	}
}


const char *ugcGetRegionNameFromType(CharClassTypes eType)
{
	// Not using static define lookup since the name returned  here is a localized one that
	// does not actually match the static define
	if ((eType == 0) || (eType == StaticDefineIntGetInt(CharClassTypesEnum, "Ground"))) {
		return s_pcGround;
	} else {
		return s_pcSpace;
	}
}

void ugcEditorReceiveMoreReviews(U32 iProjectID, U32 iSeriesID, int iPageNumber, UGCProjectReviews *pReviews)
{
	if(g_UGCEditorDoc && g_UGCEditorDoc->data && g_UGCEditorDoc->data->ns_name
	   && UGCProject_GetContainerIDFromUGCNamespace( g_UGCEditorDoc->data->ns_name ) == iProjectID)
	{
		if (!g_UGCEditorDoc->pCachedRecentReviews)
		{
			g_UGCEditorDoc->pCachedRecentReviews = StructCreateNoConst(parse_UGCProjectReviews);
			g_UGCEditorDoc->iCachedRecentReviewsPageNumber = -1;
		}
		
		ugcEditorUpdateReviews(g_UGCEditorDoc->pCachedRecentReviews, &g_UGCEditorDoc->iCachedRecentReviewsPageNumber, pReviews, iPageNumber);
		ugcProjectEditor_Refresh( g_UGCEditorDoc->project_editor );
	}
}

void ugcEditorGetMoreReviews(UIButton *button, UserData ignored)
{
	ContainerID uProjectID = UGCProject_GetContainerIDFromUGCNamespace( g_UGCEditorDoc->data->ns_name );
	int iPageNumber = g_UGCEditorDoc->iCachedRecentReviewsPageNumber + 1;

	gclUGC_RequestReviewsForPage(uProjectID, /*uSeriesID=*/0, iPageNumber);
	ugcEditorUpdateUI();
}

static void UGCProjectJobQueryCB(TimedCallback *callback, F32 timeSinceLastCallback, UserData userData)
{
	if( g_UGCEditorDoc ) {
		TimedCallback_Remove(g_UGCEditorDoc->job_timeout);
		g_UGCEditorDoc->job_timeout = NULL;
		ServerCmd_QueryUGCProjectStatus();
	}
}

void DoUGCProjectJobStatus(UGCProjectStatusQueryInfo *pInfo)
{
	if(g_UGCEditorDoc)
	{
		if (!g_UGCEditorDoc->pCachedStatus)
		{
			g_UGCEditorDoc->pCachedStatus = StructCreate( parse_UGCProjectStatusQueryInfo );
		}
		StructCopyAll( parse_UGCProjectStatusQueryInfo, pInfo, g_UGCEditorDoc->pCachedStatus );

		if (pInfo->bCurrentlyPublishing)
		{
			g_UGCEditorDoc->job_timeout = TimedCallback_Add(UGCProjectJobQueryCB, NULL, UGC_JOB_QUERY_INTERVAL);
		}

		if( g_UGCEditorDoc->project_editor ) {
			ugcProjectEditor_Refresh( g_UGCEditorDoc->project_editor );
		}
	}
}

void ugcEditorDoForceUpdateAutosave( void )
{
	if( !g_UGCEditorDoc ) {
		return;
	}

	ServerCmd_gslUGC_UpdateAutosave( ugcEditorGetProjectData() );
}

AUTO_COMMAND;
void ugcProjectEditorCheckJobStatus(void)
{
	ServerCmd_QueryUGCProjectStatus();
}

void UGCEditorQueryJobStatus(void)
{
	if( g_UGCEditorDoc && !g_UGCEditorDoc->job_timeout )
		g_UGCEditorDoc->job_timeout = TimedCallback_Add(UGCProjectJobQueryCB, NULL, UGC_JOB_QUERY_INTERVAL);
}



static UGCEditorDefaults *g_UGCEditorDefaults = NULL;

UGCEditorDefaults *ugcEditorGetDefaults(void)
{
	if (!g_UGCEditorDefaults)
	{
		g_UGCEditorDefaults = StructCreate(parse_UGCEditorDefaults);
		ParserLoadFiles(NULL, "genesis/ugc_editor_defaults.txt", "UGCEditorDefaults.bin", 0, parse_UGCEditorDefaults, g_UGCEditorDefaults);
	}
	return g_UGCEditorDefaults;
}

void* ugcEditorGetObject( const char* dictName, const char* objName )
{
	UGCProjectData* projData = ugcEditorGetProjectData();
	
	// If searching for costumes, iterate the costumes
	if (stricmp(dictName, "PlayerCostume") == 0) {
		FOR_EACH_IN_EARRAY(projData->costumes, UGCCostume, costume) {
			if (resNamespaceBaseNameEq(costume->astrName, objName)) {
				return costume->pCachedPlayerCostume;
			}
		} FOR_EACH_END;
	}

	return NULL;
}

static int sortResourceSnapDescs( const ResourceSnapDesc** ppDesc1, const ResourceSnapDesc** ppDesc2 )
{
	int val;

	val = stricmp( (*ppDesc1)->astrDictName, (*ppDesc2)->astrDictName );
	if( val != 0 ) {
		return val;
	}

	if( (*ppDesc1)->astrDictName == allocAddString( "ObjectLibrary" )) {
		GroupDef* def1 = objectLibraryGetGroupDefByName( (*ppDesc1)->astrResName, false );
		GroupDef* def2 = objectLibraryGetGroupDefByName( (*ppDesc2)->astrResName, false );

		val = stricmp( def1->name_str, def2->name_str );
	} else {
		val = stricmp( (*ppDesc1)->astrResName, (*ppDesc2)->astrResName );
	}
	
	if( val != 0 ) {
		return val;
	}

	return 0;
}

void ugcTakeObjectPreviewPhotos(void)
{
	ResourceSnapDescList resources = { 0 };

	loadstart_printf("Taking Object Photos...");
	ugcResourceInfoPopulateDictionary();
	
	ParserLoadFiles( NULL, "defs/config/ExtraResourceSnap.def", NULL, PARSER_OPTIONALFLAG, parse_ResourceSnapDescList, &resources );

	FOR_EACH_IN_REFDICT( UGC_DICTIONARY_RESOURCE_INFO, UGCResourceInfo, info ) {
		if( stricmp( info->pResInfo->resourceDict, "ObjectLibrary" ) == 0 ) {
			GroupDef* def = objectLibraryGetGroupDefByName( info->pResInfo->resourceName, false );
			ResourceSnapDesc* desc;

			if( !def ) {
				continue;
			}
			
			// If the object is a child of some object library piece, then
			// we don't want to process it.
			if( def->root_id ) {
				continue;
			}
			
			if( ugcHasTagType( info->pResInfo->resourceTags, "ObjectLibrary", "Teleporter" )) {
				// Teleporters should have a top down view for each of
				// the children, and a 3D view for itself.
				int childIt;
				for( childIt = 0; childIt != eaSize( &def->children ); ++childIt ) {
					char resNameBuffer[ RESOURCE_NAME_MAX_SIZE ];

					desc = StructCreate( parse_ResourceSnapDesc );
					desc->astrDictName = allocAddString( "ObjectLibrary" );
					sprintf( resNameBuffer, "%d", def->children[ childIt ]->name_uid );
					desc->astrResName = allocAddString( resNameBuffer );
					desc->objectIsTopDownView = true;
					eaPush( &resources.eaResources, desc );
				}

				desc = StructCreate( parse_ResourceSnapDesc );
				desc->astrDictName = allocAddString( "ObjectLibrary" );
				desc->astrResName = allocAddString( info->pResInfo->resourceName );
				eaPush( &resources.eaResources, desc );

				// need to make sure an editor copy is created, so
				// that when the headshots get taken, the children are
				// already in the Editing dictionary.
				objectLibraryGetGroupDefByName( info->pResInfo->resourceName, true );
			} else {
				desc = StructCreate( parse_ResourceSnapDesc );
				desc->astrDictName = allocAddString( "ObjectLibrary" );
				desc->astrResName = allocAddString( info->pResInfo->resourceName );
				eaPush( &resources.eaResources, desc );

				desc = StructCreate( parse_ResourceSnapDesc );
				desc->astrDictName = allocAddString( "ObjectLibrary" );
				desc->astrResName = allocAddString( info->pResInfo->resourceName );
				desc->objectIsTopDownView = true;
				eaPush( &resources.eaResources, desc );
			}
		} else if( stricmp( info->pResInfo->resourceDict, "PlayerCostume" ) == 0 ) {
			ResourceSnapDesc* desc = StructCreate( parse_ResourceSnapDesc );
			desc->astrDictName = allocAddString( "PlayerCostume" );
			desc->astrResName = allocAddString( info->pResInfo->resourceName );
			desc->astrHeadshotStyleDef = NULL;
			eaPush( &resources.eaResources, desc );
		}
	} FOR_EACH_END;

	loadstart_printf( "Sorting photo descriptions..." );
	eaQSort( resources.eaResources, sortResourceSnapDescs );
	loadend_printf( "done." );

	gclResourceSnapTakePhotos( resources.eaResources );
	StructReset( parse_ResourceSnapDescList, &resources );

	loadend_printf(" done.");
}

bool ugcAnimatedHeadshotThisFrame( void )
{
	return SAFE_MEMBER( g_UGCEditorDoc, bAnimatedHeadshotThisFrame );
}

void ugcSetAnimatedHeadshotThisFrame( void )
{
	if( g_UGCEditorDoc ) {
		g_UGCEditorDoc->bAnimatedHeadshotThisFrame = true;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN GOTHIC EXPORT COMMANDS
///////////////////////////////////////////////////////////////////////////////////////////////

static bool gothicFilename_s(const char *filename, char *out_filename, size_t out_filename_size)
{
	char csv_filename[CRYPTIC_MAX_PATH];

	if(!isProductionEditMode())
	{
		conPrintf("Must be connected to a Foundry editing GameServer to run gothic export commands.");
		return false;
	}

	if(strEndsWith(filename, ".csv"))
		strcpy(csv_filename, filename);
	else
		changeFileExt(filename, ".csv", csv_filename);

	fileLocateWrite_s(csv_filename, out_filename, out_filename_size);

	return true;
}
#define gothicFilename(filename, csv_filename) gothicFilename_s(filename, SAFESTR(csv_filename))

static FILE *gothicFile(const char *filename)
{
	char out_filename[CRYPTIC_MAX_PATH];
	FILE *file = NULL;

	if(!gothicFilename(filename, out_filename))
		return NULL;

	file = fopen(out_filename, "wt");
	if(!file)
	{
		conPrintf("Cannot open gothic output file %s.", out_filename);
		return NULL;
	}

	conPrintf("Writing to gothic: %s ...", out_filename);

	return file;
}

#define FOR_EACH_INFO_IN_REFDICT(hDict, name) { RefDictIterator i##name##Iter; ReferentInfoStruct *p; RefSystem_InitRefDictIterator(hDict, &i##name##Iter); while ((p = RefSystem_GetNextReferentInfoFromIterator(&i##name##Iter))) { const char *name = p->pStringRefData;

#define FOR_GOTHIC_FILE(filename, file)	{ FILE *file = gothicFile(filename); if(file) {
#define END_GOTHIC_FILE(file)			conPrintf("...Done."); fclose(file); } }

AUTO_COMMAND ACMD_CATEGORY(UGC);
void ugcExportGothicForSound(const char *filename)
{
	const char **sndEventList = *sndGetEventListStatic();
	FOR_GOTHIC_FILE(filename, file)
	{
		fprintf(file, "FMOD Sound Event,Looping/OneShot,UGCSound (if tagged)\n");
		FOR_EACH_IN_EARRAY_FORWARDS(sndEventList, const char, event_name)
		{
			if(event_name)
			{
				UGCSound *ugcSound = NULL;
				FOR_EACH_IN_REFDICT("UGCSound", UGCSound, ugcSoundIter)
				{
					if(stricmp(ugcSoundIter->strSoundName, event_name) == 0)
					{
						ugcSound = ugcSoundIter;
						break;
					}
				}
				FOR_EACH_END
				fprintf(file, "%s,%s,%s\n", event_name, sndEventIsOneShot(event_name) ? "OneShot" : "Looping", NULL_TO_EMPTY(SAFE_MEMBER(ugcSound, astrName)));
			}
		}
		FOR_EACH_END
	}
	END_GOTHIC_FILE(file)
}

AUTO_COMMAND ACMD_CATEGORY(UGC);
void ugcExportGothicForSoundDSP(const char *filename)
{
	FOR_GOTHIC_FILE(filename, file)
	{
		fprintf(file, "FMOD SoundDSP,UGCSoundDSP (if tagged)\n");
		FOR_EACH_INFO_IN_REFDICT("SoundDSPs", name)
		{
			UGCSoundDSP *ugcSoundDSP = NULL;
			FOR_EACH_IN_REFDICT("UGCSoundDSP", UGCSoundDSP, ugcSoundDSPIter)
			{
				if(stricmp(ugcSoundDSPIter->strSoundDSPName, name) == 0)
				{
					ugcSoundDSP = ugcSoundDSPIter;
					break;
				}
			}
			FOR_EACH_END
			fprintf(file, "%s,%s\n", name, NULL_TO_EMPTY(SAFE_MEMBER(ugcSoundDSP, astrName)));
		}
		FOR_EACH_END
	}
	END_GOTHIC_FILE(file)
}

AUTO_COMMAND ACMD_CATEGORY(UGC);
void ugcExportGothicForObjLib(const char *filename, const char *filename_regex, bool exclude_resinfos)
{
	FOR_GOTHIC_FILE(filename, file)
	{
		fprintf(file, "Object,Filename\n");
		FOR_EACH_IN_REFDICT("ObjectLibrary", GroupDef, def)
		{
			if(RegExSimpleMatch(def->filename, filename_regex))
				if(!exclude_resinfos || !ugcResourceGetInfoInt("ObjectLibrary", def->name_uid))
					fprintf(file, "%s,%s\n", def->name_str, def->filename);
		}
		FOR_EACH_END
	}
	END_GOTHIC_FILE(file)
}

AUTO_COMMAND ACMD_CATEGORY(UGC);
void ugcExportGothicForEncounters(const char* filename, const char *filename_regex, bool exclude_resinfos)
{
	ServerCmd_gslUGC_ExportGothicForEncounterTemplates(filename, filename_regex, exclude_resinfos);
}

AUTO_COMMAND ACMD_CATEGORY(UGC);
void ugcExportGothicForCostumes(const char* filename, const char *filename_regex, bool exclude_resinfos)
{
	ServerCmd_gslUGC_ExportGothicForPlayerCostumes(filename, filename_regex, exclude_resinfos);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// END GOTHIC EXPORT COMMANDS
///////////////////////////////////////////////////////////////////////////////////////////////

static void ugcRegionPreviewRefresh(UIButton* ignored, UIWindow* window)
{
	float w = UI_WIDGET( window )->width;
	float h = UI_WIDGET( window )->height;
	UGCOverworldMapRegion** mapRegions = ugcGetDefaults()->eaMapRegions;
	int itRegion;
	int itRect;
	
	ui_WidgetGroupQueueFree( &UI_WIDGET( window )->children );

	ui_WindowAddChild(window, UI_WIDGET(ui_SpriteCreate(0,0,w,h, "World_Map")));
	for(itRegion = eaSize( &mapRegions ) - 1; itRegion >= 0; itRegion--) {
		UGCOverworldMapRegion * region = mapRegions[itRegion];
		for( itRect = 0; itRect != eaSize( &region->eaRects ); ++itRect ) {
			UGCRect* rect = region->eaRects[ itRect ];
			UISprite * sprite = ui_SpriteCreate(rect->x * w, rect->y * h, 
												rect->w * w, rect->h * h, "white8x8");
			sprite->tint.r = 255 * (itRegion % 4) / 4;
			sprite->tint.g = 255 - 255 * ((itRegion + 5) % 6) / 6;
			sprite->tint.b = 255 * ((itRegion + 3) % 5) / 5;
			sprite->tint.a = 200;
			ui_WindowAddChild(window, UI_WIDGET(sprite));
		}
	}
}

//////////////////////////////////////////////////////////////////////
// UGC Loading
static char* s_ugcLoadingProjectName = NULL;
static bool s_ugcLoadingOldSkipPressAnyKey;
static UIWindow* s_ugcLoadingModalDialog;
static UILabel* s_ugcLoadingLabel;
static UGCLoadingState s_ugcLoadingCurState;

static void ugcLoadingCancelLogin( UIButton* ignored, UserData ignored2 )
{
	GSM_SwitchToState_Complex(GCL_BASE "/" GCL_LOGIN);
	ugcProjectChooserFree();
	
	if( s_ugcLoadingModalDialog ) {
		ui_WindowHide( s_ugcLoadingModalDialog );
	}
}

static void ugcLoadingAnimateSprite( UISprite* sprite, UI_PARENT_ARGS )
{
	sprite->rot += g_ui_State.timestep * 6;
	while( sprite->rot > TWOPI ) {
		sprite->rot -= TWOPI;
	}
	ui_SpriteTick( sprite, UI_PARENT_VALUES );
}

static void ugcLoadingDrawSprite( UISprite* sprite, UI_PARENT_ARGS )
{
	float rot = sprite->rot;
	//sprite->rot = rot - fmodf( rot, QUARTERPI );
	ui_SpriteDraw( sprite, UI_PARENT_VALUES );
	sprite->rot = rot;
}

void ugcLoadingUpdateState(UGCLoadingState state, int queuePos)
{
	//printfColor( COLOR_BRIGHT|COLOR_BLUE, "UGC Loading: %d, %d\n", state, queuePos );

	if( state ) {
		if( !s_ugcLoadingModalDialog ) {			
			s_ugcLoadingModalDialog = ui_WindowCreate( NULL, 0, 0, 300, 150 );
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_WindowNoFooter", UI_WIDGET( s_ugcLoadingModalDialog )->hOverrideSkin );
			ui_WidgetSetTextMessage( UI_WIDGET( s_ugcLoadingModalDialog ), "UGC.LoadingEditor" );
			ui_WindowSetModal( s_ugcLoadingModalDialog, true );
			ui_WindowSetClosable( s_ugcLoadingModalDialog, false );
			ui_WindowSetResizable( s_ugcLoadingModalDialog, false );			
			elUICenterWindow( s_ugcLoadingModalDialog );

			s_ugcLoadingLabel = ui_LabelCreate( "<PLACEHOLDER>", 0, 0 );
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser", UI_WIDGET( s_ugcLoadingLabel )->hOverrideSkin );
			ui_LabelSetWordWrap( s_ugcLoadingLabel, true );
			ui_WidgetSetWidthEx( UI_WIDGET( s_ugcLoadingLabel ), 1, UIUnitPercentage );
			ui_WindowAddChild( s_ugcLoadingModalDialog, s_ugcLoadingLabel );

			{
				UIButton* button = ui_ButtonCreate( NULL, 0, 0, ugcLoadingCancelLogin, NULL );
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser", UI_WIDGET( button )->hOverrideSkin );
				ui_ButtonSetMessage( button, "UGC.Cancel" );
				ui_WidgetSetPositionEx( UI_WIDGET( button ), 0, 0, 0, 0, UIBottomRight );
				ui_WindowAddChild( s_ugcLoadingModalDialog, button );
			}

			{
				UISprite* sprite = ui_SpriteCreate( 0, 0, 1, 1, "PatchingIndicator" );
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser", UI_WIDGET( sprite )->hOverrideSkin );
				sprite->widget.tickF = ugcLoadingAnimateSprite;
				sprite->widget.drawF = ugcLoadingDrawSprite;
				ui_WidgetSetPositionEx( UI_WIDGET( sprite ), 0, 25, 0, 0, UIBottom );
				ui_SpriteResize( sprite );
				ui_WindowAddChild( s_ugcLoadingModalDialog, sprite );
			}

			s_ugcLoadingOldSkipPressAnyKey = gGCLState.bSkipPressAnyKey;
			gGCLState.bSkipPressAnyKey = true;
		}

		if( !ui_WindowIsVisible( s_ugcLoadingModalDialog )) {
			ui_WindowShowEx( s_ugcLoadingModalDialog, true );
		}
	} else {
		if( s_ugcLoadingModalDialog ) {
			ui_WidgetQueueFreeAndNull( &s_ugcLoadingModalDialog );
			s_ugcLoadingLabel = NULL;
			
			gGCLState.bSkipPressAnyKey = s_ugcLoadingOldSkipPressAnyKey;
			s_ugcLoadingOldSkipPressAnyKey = false;
		}
	}
	s_ugcLoadingCurState = state;

	if( s_ugcLoadingModalDialog && s_ugcLoadingLabel ) {
		switch( state ) {
			case UGC_LOAD_INIT:
				ui_LabelSetText( s_ugcLoadingLabel, TranslateMessageKey( "UGC.InitializingProject" ));
			
			xcase UGC_LOAD_WAITING_IN_QUEUE: {
				char *text = NULL;
				FormatMessageKey(&text, "UGC.WaitingInQueue",
								 STRFMT_INT("QueuePosition", queuePos + 1),
								 STRFMT_END );
				ui_LabelSetText( s_ugcLoadingLabel, text );
				estrDestroy( &text );
			}
			
			xcase UGC_LOAD_WAITING_FOR_SERVER:
				ui_LabelSetText( s_ugcLoadingLabel, TranslateMessageKey( "UGC.WaitingToEdit" ));
		}
	}
}

//shows a preview for the map_regions setup in ugc_defaults.txt
AUTO_COMMAND ACMD_NAME(showRegionWindow);
void ugcRegionPreviewShow(){
	int w = 1000, h = 750;
	UIWindow* window = ui_WindowCreate( "region previewer", 0, 0, w, h);

	ugcRegionPreviewRefresh(NULL, window);
	ui_WindowAddChild(window, UI_WIDGET(ui_ButtonCreate("refresh", 0,0, ugcRegionPreviewRefresh, window)));
	ui_WindowSetCloseCallback( window, ui_WindowFreeOnClose, NULL );
	ui_WindowShow(window);
}

AUTO_COMMAND ACMD_CATEGORY(UGC) ACMD_ACCESSLEVEL(4);
char *ugcValidatePublishedProjectsInExportHogg(const char* hogg_filename)
{
	char *result = NULL;
	int err_return = 0;
	HogFile *hogg = hogFileRead(hogg_filename, NULL, PIGERR_PRINTF, &err_return, HOG_NOCREATE|HOG_READONLY);
	if(!hogg)
	{
		char *estrError = NULL;
		estrPrintf(&estrError, "ERROR: Couldn't open \"%s\" for reading: %s!\n", hogg_filename, hogFileGetArchiveFileName(hogg));
		result = StructAllocString(estrError);
		estrDestroy(&estrError);
	}
	else
	{
		UGCSearchResult *pUGCSearchResult = StructCreate(parse_UGCSearchResult);
		if(PARSERESULT_ERROR == ParserReadTextFileFromHogg("UGCSearchResult", parse_UGCSearchResult, pUGCSearchResult, hogg))
		{
			char *estrError = NULL;
			estrPrintf(&estrError, "ERROR: Could not find UGCSearchResult in hogg %s.\n", hogFileGetArchiveFileName(hogg));
			result = StructAllocString(estrError);
			estrDestroy(&estrError);
		}
		else
		{
			int projectsValidated = 0;
			int projectsWithValidationErrors = 0;

			int index;
			for(index = 0; index < eaSize(&pUGCSearchResult->eaResults); index++)
			{
				UGCProject *pUGCProject = StructCreate(parse_UGCProject);
				char filename[1024];

				sprintf(filename, "UGCProject/%d/UGCProject.con", pUGCSearchResult->eaResults[index]->iUGCProjectID);

				if(PARSERESULT_ERROR != ParserReadTextFileFromHogg(filename, parse_UGCProject, pUGCProject, hogg))
				{
					UGCProjectVersion *pUGCProjectVersionPublished = NULL;
					int i;
					for(i = eaSize(&pUGCProject->ppProjectVersions) - 1; i >= 0; i--)
					{
						UGCProjectVersionState eState = ugcProjectGetVersionStateConst(pUGCProject->ppProjectVersions[i]);
						if(UGC_PUBLISHED == eState || UGC_REPUBLISHING == eState || UGC_NEEDS_REPUBLISHING == eState)
							pUGCProjectVersionPublished = pUGCProject->ppProjectVersions[i];

						if(pUGCProjectVersionPublished)
							break;
					}

					if(pUGCProjectVersionPublished)
					{
						UGCProjectData *pUGCProjectData = StructCreate(parse_UGCProjectData);
						sprintf(filename, "data/ns/%s/project/%s", pUGCProjectVersionPublished->pNameSpace, pUGCProject->pIDString);

						if(PARSERESULT_ERROR != ParserReadTextFileFromHogg(filename, parse_UGCProjectData, pUGCProjectData, hogg))
						{
							// Validate the project
							UGCRuntimeStatus *pUGCRuntimeStatus = StructCreate(parse_UGCRuntimeStatus);

							int numDialogsDeleted = 0, numCostumesReset = 0, numObjectivesReset = 0;
							ugcEditorFixupProjectData(pUGCProjectData, &numDialogsDeleted, &numCostumesReset, &numObjectivesReset, NULL);
							ugcProjectFixupDeprecated(pUGCProjectData, true);

							ugcSetStageAndAdd(pUGCRuntimeStatus, "UGC Validate");
							ugcValidateProject(pUGCProjectData);
							ugcClearStage();

							projectsValidated++;

							if(ugcStatusHasErrors(pUGCRuntimeStatus, UGC_ERROR))
							{
								projectsWithValidationErrors++;

								FOR_EACH_IN_EARRAY(pUGCRuntimeStatus->stages, UGCRuntimeStage, stage)
								{
									if(eaSize(&pUGCRuntimeStatus->stages[0]->errors) > 0)
									{
										printf("\nPROJECT %s HAS %d VALIDATION ERRORS:\n",
											pUGCProjectVersionPublished->pNameSpace, eaSize(&pUGCRuntimeStatus->stages[0]->errors));
										FOR_EACH_IN_EARRAY(pUGCRuntimeStatus->stages[0]->errors, UGCRuntimeError, error)
										{
											char *buf = NULL;
											estrCreate(&buf);
											ParserWriteText(&buf, parse_UGCRuntimeError, error, 0, 0, 0);
											printf("%s\n", buf);
											estrDestroy(&buf);
										}
										FOR_EACH_END;
									}
								}
								FOR_EACH_END;
								printf("\n");
							}
							StructDestroy(parse_UGCRuntimeStatus, pUGCRuntimeStatus);
						}

						StructDestroy(parse_UGCProjectData, pUGCProjectData);
					}
				}

				StructDestroy(parse_UGCProject, pUGCProject);
			}

			if(projectsWithValidationErrors)
				printf("\n%d PROJECTS HAD ERRORS OUT OF %d PROJECTS VALIDATED!\n", projectsWithValidationErrors, projectsValidated);
			else
				printf("\n%d projects validated with no errors.\n", projectsValidated);
		}

		StructDestroy(parse_UGCSearchResult, pUGCSearchResult);
	}

	hogFileDestroy(hogg, true);

	return result;
}

StaticDefineInt UGCWhenTypeNormalEnum[] =
{
	DEFINE_INT
	{ "UGC_When.MapStart", UGCWHEN_MAP_START},
	{ "UGC_When.ComponentComplete", UGCWHEN_COMPONENT_COMPLETE},
	{ "UGC_When.ComponentReached", UGCWHEN_COMPONENT_REACHED},
	{ "UGC_When.DialogPromptReached", UGCWHEN_DIALOG_PROMPT_REACHED },
	{ "UGC_When.ObjectiveInProgress", UGCWHEN_OBJECTIVE_IN_PROGRESS},
	{ "UGC_When.ObjectiveComplete", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeHideEnum[] =
{
	DEFINE_INT
	{ "UGC_When.CurrentComponentComplete", UGCWHEN_CURRENT_COMPONENT_COMPLETE},
	{ "UGC_When.ComponentComplete", UGCWHEN_COMPONENT_COMPLETE},
	{ "UGC_When.ComponentReached", UGCWHEN_COMPONENT_REACHED},
	{ "UGC_When.DialogPromptReached", UGCWHEN_DIALOG_PROMPT_REACHED },
	{ "UGC_When.ObjectiveInProgress", UGCWHEN_OBJECTIVE_IN_PROGRESS},
	{ "UGC_When.ObjectiveComplete", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeContactHideEnum[] =
{
	DEFINE_INT
	{ "UGC_When.ComponentComplete", UGCWHEN_COMPONENT_COMPLETE},
	{ "UGC_When.ComponentReached", UGCWHEN_COMPONENT_REACHED},
	{ "UGC_When.DialogPromptReached", UGCWHEN_DIALOG_PROMPT_REACHED },
	{ "UGC_When.ObjectiveInProgress", UGCWHEN_OBJECTIVE_IN_PROGRESS},
	{ "UGC_When.ObjectiveComplete", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeRoomDoorHideEnum[] =
{
	DEFINE_INT
	{ "UGC_When.ComponentComplete", UGCWHEN_COMPONENT_COMPLETE},
	{ "UGC_When.ComponentReached", UGCWHEN_COMPONENT_REACHED},
	{ "UGC_When.DialogPromptReached", UGCWHEN_DIALOG_PROMPT_REACHED },
	{ "UGC_When.ObjectiveInProgress", UGCWHEN_OBJECTIVE_IN_PROGRESS},
	{ "UGC_When.ObjectiveComplete", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeDialogShowEnum[] =
{
	DEFINE_INT
	{ "UGC_When.MapStart", UGCWHEN_MAP_START},
	{ "UGC_When.ComponentComplete", UGCWHEN_COMPONENT_COMPLETE},
	{ "UGC_When.ComponentReached", UGCWHEN_COMPONENT_REACHED},
//  WOLF[10Apr12] According to Jared, these options are not really valid. For now don't do any fix up of old projects. Whatever functionality they currently
//      have we would like to keep. But we won't allow any new projects to use them.
//	{ "Objective In Progress", UGCWHEN_OBJECTIVE_IN_PROGRESS},
//	{ "Objective Complete", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeObjectiveShowEnum[] =
{
	DEFINE_INT
	{ "UGC_When.MapStart", UGCWHEN_MAP_START},
	{ "UGC_When.ObjectiveInProgress_ComponentHasObjective", UGCWHEN_OBJECTIVE_IN_PROGRESS},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeObjectiveHideEnum[] =
{
	DEFINE_INT
	{ "UGC_When.CurrentComponentComplete", UGCWHEN_CURRENT_COMPONENT_COMPLETE},
	{ "UGC_When.ObjectiveComplete_ComponentHasObjective", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeObjectiveContactHideEnum[] =
{
	DEFINE_INT
	{ "UGC_When.ObjectiveComplete_ComponentHasObjective", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeObjectiveRoomDoorHideEnum[] =
{
	DEFINE_INT
	{ "UGC_When.ObjectiveComplete_ComponentHasObjective", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

StaticDefineInt UGCWhenTypeNoMapEnum[] =
{
	DEFINE_INT
	{ "UGC_When.MapStart", UGCWHEN_MAP_START },
	{ "UGC_When.ObjectiveInProgress", UGCWHEN_OBJECTIVE_IN_PROGRESS},
	{ "UGC_When.ObjectiveComplete", UGCWHEN_OBJECTIVE_COMPLETE},
	{ "UGC_When.Manual", UGCWHEN_MANUAL},
	DEFINE_END
};

#include "NNOUGCEditorPrivate_h_ast.c"
#include "NNOUGCEditorMain_c_ast.c"

#include "NNOUGCEditorPrivate.h"

#include "CharacterClass.h"
#include "Color.h"
#include "GfxDebug.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCCommon.h"
#include "NNOUGCModalDialog.h"
#include "Prefs.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UGCProjectCommon.h"
#include "UITextureAssembly.h"
#include "cmdparse.h"
#include "earray.h"
#include "file.h"
#include "inputKeyBind.h"
#include "utilitiesLib.h"

#include "AutoGen/GameServerLib_autogen_ServerCmdWrappers.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

//// Actions

static UGCActionDescription g_UGCActionDescriptions[] = {
/// ** Globals **
//	  ID						Name						Icon						Disabled Icon						Dialog?	Description
	{ UGC_ACTION_SAVE,			"UGC.Action_Save",			"UGC_Icons_Labels_Save",	"UGC_Icons_Labels_Save_Disabled",	false,	"UGC.Action_Save_Tooltip" },
	// "UGC.Action_Publish_Tooltip" message key string was c because we dynamically swap it with "UGC.Action_Publish_Disabled_Tooltip" per [COR-17087]
	{ UGC_ACTION_PUBLISH,		"UGC.Action_Publish",		"UGC_Icon_Labels_Publish",	"UGC_Icon_Labels_Publish_Disabled",	false,	"UGC.Action_Publish_Tooltip" },
	{ UGC_ACTION_WITHDRAW,		"UGC.Action_Withdraw",		"UGC_Icon_Labels_Unpublish","UGC_Icon_Labels_Unpublish_Disabled",false,	"UGC.Action_Withdraw_Tooltip" },
	{ UGC_ACTION_FREEZE,		"UGC.Action_Freeze",		NULL,						NULL,								true,	"UGC.Action_Freeze_Tooltip" },
	{ UGC_ACTION_CLOSE,			"UGC.Action_Close",			NULL,						NULL,								false,	"UGC.Action_Close_Tooltip" },
	{ UGC_ACTION_LOGOUT,		"UGC.Action_Logout",		NULL,						NULL,								false,	"UGC.Action_Logout_Tooltip" },
	{ UGC_ACTION_UNDO,			"UGC.Action_Undo",			"UGC_Icons_Labels_Undo",	"UGC_Icons_Labels_Undo_Disabled",	false,	"UGC.Action_Undo_Tooltip" },
	{ UGC_ACTION_REDO,			"UGC.Action_Redo",			"UGC_Icons_Labels_Redo",	"UGC_Icons_Labels_Redo_Disabled",	false,	"UGC.Action_Redo_Tooltip" },
	{ UGC_ACTION_CUT,			"UGC.Action_Cut",			"UGC_Icons_Labels_Cut",		"UGC_Icons_Labels_Cut_Disabled",	false,	"UGC.Action_Cut_Tooltip" },
	{ UGC_ACTION_COPY,			"UGC.Action_Copy",			"UGC_Icons_Labels_Duplicate","UGC_Icons_Labels_Duplicate_Disabled",false,"UGC.Action_Copy_Tooltip" },
	{ UGC_ACTION_PASTE,			"UGC.Action_Paste",			"UGC_Icons_Labels_Paste",	"UGC_Icons_Labels_Paste_Disabled",	false,	"UGC.Action_Paste_Tooltip" },
	{ UGC_ACTION_DELETE,		"UGC.Action_Delete",		"UGC_Icons_Labels_Delete",	"UGC_Icons_Labels_Delete_Disabled",	false,	"UGC.Action_Delete_Tooltip" },
	{ UGC_ACTION_DUPLICATE,		"UGC.Action_Duplicate",		"UGC_Icons_Labels_Duplicate","UGC_Icons_Labels_Duplicate_Disabled",false,"UGC.Action_Duplicate_Tooltip" },
	{ UGC_ACTION_REPORT_BUG,	"UGC.Action_Report_Bug",	NULL,						NULL,								false,	"UGC.Action_Report_Bug_Tooltip" },
	{ UGC_ACTION_VIEW_EULA,		"UGC.Action_View_EULA",		NULL,						NULL,								false,	"UGC.Action_View_EULA_Tooltip" },
	{ UGC_ACTION_PLAY_MISSION,	"UGC.Action_Play_Mission",	"UGC_Icons_Labels_Play",	NULL,								false,	"UGC.Action_Play_Mission_Tooltip" },
	{ UGC_ACTION_DESELECT_ALL,	"UGC.Action_Deselect_All",	NULL,						NULL,								false,	"UGC.Action_Deselect_All_Tooltip" },
	{ UGC_ACTION_MOVE_UP,		"UGC.Action_Move_Up",		NULL,						NULL,								false,	"UGC.Action_Move_Up_Tooltip" },
	{ UGC_ACTION_MOVE_DOWN,		"UGC.Action_Move_Down",		NULL,						NULL,								false,	"UGC.Action_Move_Down_Tooltip" },
	{ UGC_ACTION_MOVE_LEFT,		"UGC.Action_Move_Left",		NULL,						NULL,								false,	"UGC.Action_Move_Left_Tooltip" },
	{ UGC_ACTION_MOVE_RIGHT,	"UGC.Action_Move_Right",	NULL,						NULL,								false,	"UGC.Action_Move_Right_Tooltip" },

/// ** Maps **
//	  ID						Name						Icon						Disabled Icon						Dialog?	Description
	{ UGC_ACTION_MAP_CREATE,	"UGC.Action_Map_Create",	"UGC_Icons_Labels_New",		"UGC_Icons_Labels_New_Disabled",	true,	"UGC.Action_Map_Create_Tooltip" },
	{ UGC_ACTION_MAP_DELETE,	"UGC.Action_Map_Delete",	"UGC_Icons_Labels_Delete",	"UGC_Icons_Labels_Delete_Disabled",	false,	"UGC.Action_Map_Delete_Tooltip" },
	{ UGC_ACTION_MAP_DUPLICATE,	"UGC.Action_Map_Duplicate",	"UGC_Icons_Labels_Duplicate","UGC_Icons_Labels_Duplicate_Disabled",false,"UGC.Action_Map_Duplicate_Tooltip" },
	{ UGC_ACTION_MAP_SEARCH_COMPONENT,"UGC.Action_Map_Search_Component","UGC_Icons_Labels_TreeView",NULL,					false,  "UGC.Action_Map_Search_Component_Tooltip" },
	{ UGC_ACTION_MAP_EDIT_NAME,	"UGC.Action_Map_Edit_Name",	NULL,						NULL,								false,	"UGC.Action_Map_Edit_Name_Tooltip" },
	{ UGC_ACTION_MAP_EDIT_BACKDROP,"UGC.Action_Map_Edit_Backdrop",NULL,					NULL,								false,  "UGC.Action_Map_Edit_Backdrop_Tooltip" },
	{ UGC_ACTION_MAP_SET_LAYOUT_MODE,"UGC.Action_Map_Set_Layout_Mode",NULL,				NULL,								false,	"UGC.Action_Map_Set_Layout_Mode_Tooltip" },
	{ UGC_ACTION_MAP_SET_DETAIL_MODE,"UGC.Action_Map_Set_Detail_Mode",NULL,				NULL,								false,	"UGC.Action_Map_Set_Detail_Mode_Tooltip" },

	{ UGC_ACTION_SET_BACKDROP,	"UGC.Action_Set_Backdrop",	"UGC_Icon_Object",			NULL,								true,	"UGC.Action_Set_Backdrop_Tooltip" },
	{ UGC_ACTION_CREATE_MARKER,	"UGC.Action_Create_Marker",	"UGC_Icon_Marker",			NULL,								false,	"UGC.Action_Create_Marker_Tooltip" },
	{ UGC_ACTION_CREATE_RESPAWN,"UGC.Action_Create_Respawn","UGC_Icon_Spawn_Map",		NULL,								false,	"UGC.Action_Create_Respawn_Tooltip" },
	{ UGC_ACTION_PLAY_MAP,		"UGC.Action_Play_Map",		"UGC_Icons_Labels_Play",	NULL,								false,	"UGC.Action_Play_Map_Tooltip" },
	{ UGC_ACTION_PLAY_MAP_FROM_LOCATION,"UGC.Action_Play_Map_From_Location","UGC_Icons_Labels_Play",NULL,					false,	"UGC.Action_Play_Map_From_Location_Tooltip" },
	{ UGC_ACTION_ROOM_CLEAR,	"UGC.Action_Room_Clear",	"UGC_Icon_Object",			NULL,								false,	"UGC.Action_Room_Clear_Tooltip" },
	{ UGC_ACTION_ROOM_POPULATE,	"UGC.Action_Room_Populate",	"UGC_Icon_Object",			NULL,								true,	"UGC.Action_Room_Populate_Tooltip" },

/// ** Mission **
//	ID												Name								Icon						Disabled Icon						Dialog? Description
	{ UGC_ACTION_MISSION_PLAY_SELECTION,			"UGC.Action_Mission_Play_Selection",NULL,						NULL,								false,	"UGC.Action_Mission_Play_Selection_Tooltip" },
	{ UGC_ACTION_MISSION_DELETE,					"UGC.Action_Mission_Delete",		NULL,						NULL,								false,	"UGC.Action_Mission_Delete_Tooltip" },
	{ UGC_ACTION_MISSION_CREATE_CLICKIE_OBJECTIVE,	"UGC.Action_Mission_Create_Clickie_Objective","ugc_icon_clickie",NULL,								false,	"UGC.Action_Mission_Create_Clickie_Objective_Tooltip" },
	{ UGC_ACTION_MISSION_CREATE_KILL_OBJECTIVE,		"UGC.Action_Mission_Create_Kill_Objective","ugc_icon_encounter",NULL,								false,	"UGC.Action_Mission_Create_Kill_Objective_Tooltip" },	
	{ UGC_ACTION_MISSION_CREATE_DIALOG_OBJECTIVE,	"UGC.Action_Mission_Create_Dialog_Objective","ugc_icon_contact",NULL,								false,	"UGC.Action_Mission_Create_Dialog_Objective_Tooltip" },
	{ UGC_ACTION_MISSION_CREATE_DIALOG,				"UGC.Action_Mission_Create_Dialog",	"ugc_icon_dialogtree",		NULL,								false,	"UGC.Action_Mission_Create_Dialog_Tooltip" },
	{ UGC_ACTION_MISSION_CREATE_MARKER_OBJECTIVE,	"UGC.Action_Mission_Create_Marker_Objective","ugc_icon_marker",	NULL,								false,	"UGC.Action_Mission_Create_Marker_Objective_Tooltip" },
	{ UGC_ACTION_MISSION_CREATE_UNLOCK_OBJECTIVE,	"UGC.Action_Mission_Create_Unlock_Objective","ugc_icon_clickie",NULL,								false,	"UGC.Action_Mission_Create_Unlock_Objective_Tooltip" },

/// ** Dialogs **
//	  ID											Name								Icon						Disabled Icon						Dialog?	Description
	{ UGC_ACTION_DIALOG_PLAY,						"UGC.Action_Dialog_Play",			"UGC_Icons_Labels_Play",	NULL,								false,	"UGC.Action_Dialog_Play_Tooltip" },
	{ UGC_ACTION_DIALOG_DELETE,						"UGC.Action_Dialog_Delete",			"UGC_Icons_Labels_Delete",	"UGC_Icons_Labels_Delete_Disabled",	false,	"UGC.Action_Dialog_Delete_Tooltip" },
	{ UGC_ACTION_DIALOG_PLAY_SELECTION,				"UGC.Action_Dialog_Play_Selection",	"UGC_Icons_Labels_Play",	NULL,								false,	"UGC.Action_Dialog_Play_Selection_Tooltip" },

/// ** Costumes **
//	  ID											Name								Icon						Disabled Icon						Dialog?	Description
	{ UGC_ACTION_COSTUME_CREATE,					"UGC.Action_Costume_Create",		"UGC_Icons_Labels_New",		"UGC_Icons_Labels_New_Disabled",	true,	"UGC.Action_Costume_Create_Tooltip" },
	{ UGC_ACTION_COSTUME_DUPLICATE,					"UGC.Action_Costume_Duplicate",		"UGC_Icons_Labels_Duplicate","UGC_Icons_Labels_Duplicate_Disabled",false,"UGC.Action_Costume_Duplicate_Tooltip" },
	{ UGC_ACTION_COSTUME_DELETE,					"UGC.Action_Costume_Delete",		"UGC_Icons_Labels_Delete",	"UGC_Icons_Labels_Delete_Disabled",	false,	"UGC.Action_Costume_Delete_Tooltip" },
	{ UGC_ACTION_COSTUME_EDIT_NAME,					"UGC.Action_Costume_Edit_Name",		NULL,						NULL,								false,	"UGC.Action_Costume_Edit_Name_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_ALL,				"UGC.Action_Randomize_All",			NULL,						NULL,								false,	"UGC.Action_Randomize_All_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION0,			"UGC.Action_Randomize_Region0",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region0_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION1,			"UGC.Action_Randomize_Region1",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region1_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION2,			"UGC.Action_Randomize_Region2",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region2_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION3,			"UGC.Action_Randomize_Region3",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region3_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION4,			"UGC.Action_Randomize_Region4",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region4_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION5,			"UGC.Action_Randomize_Region5",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region5_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION6,			"UGC.Action_Randomize_Region6",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region6_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION7,			"UGC.Action_Randomize_Region7",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region7_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION8,			"UGC.Action_Randomize_Region8",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region8_Tooltip" },
	{ UGC_ACTION_COSTUME_RANDOMIZE_REGION9,			"UGC.Action_Randomize_Region9",		NULL,						NULL,								false,	"UGC.Action_Randomize_Region9_Tooltip" },

/// ** Items **
//	  ID											Name								Icon						Disabled Icon						Dialog?	Description
	{ UGC_ACTION_ITEM_CREATE,						"UGC.Action_Item_Create",			"UGC_Icons_Labels_New",		"UGC_Icons_Labels_New_Disabled",	true,	"UGC.Action_Item_Create_Tooltip" },
	{ UGC_ACTION_ITEM_SORT_BY_NAME,					"UGC.Action_Item_Sort_By_Name",		NULL,						NULL,								false,	"UGC.Action_Item_Sort_By_Name_Tooltip" },
	{ UGC_ACTION_ITEM_SORT_BY_ICON,					"UGC.Action_Item_Sort_By_Icon",		NULL,						NULL,								false,	"UGC.Action_Item_Sort_By_Icon_Tooltip" },
/// ** Playing **
//	  ID									Name									Icon							Disabled Icon							Dialog?	Description
	{ UGC_ACTION_PLAYING_RESET_MAP,			"UGC.Action_Playing_Reset_Map",			NULL,							NULL,									false,	"UGC.Action_Playing_Reset_Map_Tooltip" },
	{ UGC_ACTION_PLAYING_KILL_TARGET,		"UGC.Action_Playing_Kill_Target",		NULL,							NULL,									false,	"UGC.Action_Playing_Kill_Target_Tooltip" },
	{ UGC_ACTION_PLAYING_FULL_HEAL,			"UGC.Action_Playing_Full_Heal",			NULL,							NULL,									false,	"UGC.Action_Playing_Full_Heal_Tooltip" },
	{ UGC_ACTION_PLAYING_TOGGLE_EDIT_MODE,	"UGC.Action_Playing_Toggle_Edit_Mode",	NULL,							NULL,									false,	"UGC.Action_Playing_Toggle_Edit_Mode_Tooltip" },
	{ UGC_ACTION_PLAYING_TRANSLATE_MODE,	NULL,									"UGC_Icons_Labels_Translate",	"UGC_Icons_Labels_Translate_Disabled",	false,	"UGC.Action_Playing_Translate_Mode_Tooltip" },
	{ UGC_ACTION_PLAYING_ROTATE_MODE,		NULL,									"UGC_Icons_Labels_Rotate",		"UGC_Icons_Labels_Rotate_Disabled",		false,	"UGC.Action_Playing_Rotate_Mode_Tooltip" },
	{ UGC_ACTION_PLAYING_SLIDE_MODE,		NULL,									"UGC_Icons_Labels_Slide",		"UGC_Icons_Labels_Slide_Disabled",		false,	"UGC.Action_Playing_Slide_Mode_Tooltip" },
	{ UGC_ACTION_PLAYING_UNDO,				"UGC.Action_Playing_Undo",				"UGC_Icons_Labels_Undo",		"UGC_Icons_Labels_Undo_Disabled",			false,	"UGC.Action_Playing_Undo_Tooltip" },
	{ UGC_ACTION_PLAYING_REDO,				"UGC.Action_Playing_Redo",				"UGC_Icons_Labels_Redo",		"UGC_Icons_Labels_Redo_Disabled",		false,	"UGC.Action_Playing_Redo_Tooltip" },
};

UIMenu *g_UGCEditorContextMenu = NULL;
F32 g_UGCEditorContextPosition[2];

UGCActionDescription *ugcEditorGetAction(UGCActionID id)
{
	int i;
	for (i = sizeof(g_UGCActionDescriptions)/sizeof(UGCActionDescription)-1; i >= 0; --i)
	{
		if (g_UGCActionDescriptions[i].eID == id)
			return &g_UGCActionDescriptions[i];
	}
	return NULL;
}

void ugcEditorExecuteAction(UIButton *button, UserData data)
{
	UGCActionID id = (intptr_t)data;
	const UGCActionDescription *desc = ugcEditorGetAction(id);
	if (desc)
	{
		ugcEditorExecuteCommandByID(id);
	}
}

static const char *ugcEditorFindKeybind(const char *command_str)
{
	KeyBindProfileIterator iter;
	KeyBindProfile *profile;
	int i;
	bool found = false;
	keybind_NewProfileIterator(&iter);
	while (!found && (profile = keybind_ProfileIteratorNext(&iter)))
	{
		for (i = 0; i < eaSize(&profile->eaBinds); i++)
		{
			KeyBind *bind = profile->eaBinds[i];
			if (bind->pchCommand && strcmpi(bind->pchCommand, command_str) == 0)
			{
				return keybind_GetDisplayName( bind->pchKey, false );
			}
		}
	}
	return NULL;
}

static void ugcEditorActionButtonTick(SA_PARAM_NN_VALID UIButton *button, UI_PARENT_ARGS)
{
	UGCActionID id = (UGCActionID)(intptr_t)button->clickedData;
	const UGCActionDescription* desc = ugcEditorGetAction(id);
	char* statusEstr = NULL;
	bool enabled;

	enabled = ugcEditorQueryCommandByID(id, &statusEstr);
	ui_SetActive(UI_WIDGET(button), enabled);
	if( estrLength( &statusEstr )) {
		estrInsertf( &statusEstr, 0, "<b>(" );
		estrConcatf( &statusEstr, ")</b>" );
	}

	if( desc ) {
		const char *bind_str;
		char buf[128];

		sprintf(buf, "UGC.Do %s", StaticDefineIntRevLookup(UGCActionIDEnum, desc->eID));

		bind_str = ugcEditorFindKeybind(buf);
		if (bind_str)
		{
			estrInsertf( &statusEstr, 0, "%s [%s]%s", TranslateMessageKey( desc->strDescriptionMessage ), bind_str, (estrLength(&statusEstr) ? "<br>" : ""));
		}
		else
		{
			estrInsertf( &statusEstr, 0, "%s%s", TranslateMessageKey( desc->strDescriptionMessage ), (estrLength(&statusEstr) ? "<br>" : ""));
		}
		
		ui_WidgetSetTooltipString(UI_WIDGET(button), statusEstr);
		estrDestroy( &statusEstr );
	}
	
	ui_ButtonTick(button, UI_PARENT_VALUES);
}

UIButton *ugcEditorButtonCreate(UGCActionID id, bool show_icon, bool icon_only)
{
	const UGCActionDescription *desc = ugcEditorGetAction(id);
	if (desc)
	{
		UIButton *button;
		
		if (!show_icon) {
			button = ui_ButtonCreate(NULL, 0, 0, ugcEditorExecuteAction, (void*)(intptr_t)desc->eID);
			ui_ButtonSetMessage( button, desc->strNameMessage );
		} else if (icon_only) {
			button = ui_ButtonCreateImageOnly(desc->strIcon, 0, 0, ugcEditorExecuteAction, (void*)(intptr_t)desc->eID);
		} else {
			button = ui_ButtonCreateWithIcon("", desc->strIcon, ugcEditorExecuteAction, (void*)(intptr_t)desc->eID);
			ui_ButtonSetMessage( button, desc->strNameMessage );
		}
		if( desc->strIconDisabled ) {
			ui_ButtonSetImageEx( button, desc->strIcon, desc->strIcon, desc->strIcon, desc->strIcon, desc->strIconDisabled );
		}
		ui_ButtonResize( button );
		
		(UI_WIDGET(button))->tickF = ugcEditorActionButtonTick;
		

		return button;
	}
	return NULL;
}

MEFieldContextEntry* ugcMEContextAddEditorButton(UGCActionID id, bool show_icon, bool icon_only)
{
	const UGCActionDescription *desc = ugcEditorGetAction(id);
	if (desc)
	{
		char button_id[256];
		MEFieldContextEntry *pEntry;

		sprintf(button_id, "UGCCommandButton_%d", id);
		
		if (!show_icon)
			pEntry = MEContextAddButtonMsg(desc->strNameMessage, NULL, ugcEditorExecuteAction, (void*)(intptr_t)desc->eID, allocAddString(button_id), NULL, desc->strDescriptionMessage);
		else if (icon_only)
			pEntry = MEContextAddButtonMsg(NULL, desc->strIcon, ugcEditorExecuteAction, (void*)(intptr_t)desc->eID, allocAddString(button_id), NULL, desc->strDescriptionMessage);
		else
			pEntry = MEContextAddButtonMsg(desc->strNameMessage, desc->strIcon, ugcEditorExecuteAction, (void*)(intptr_t)desc->eID, allocAddString(button_id), NULL, desc->strDescriptionMessage);

		if(show_icon && desc->strIconDisabled ) {
			ui_ButtonSetImageEx( ENTRY_BUTTON( pEntry ), desc->strIcon, desc->strIcon, desc->strIcon, desc->strIcon, desc->strIconDisabled );
		}
		ui_ButtonResize( ENTRY_BUTTON( pEntry ));
		ui_WidgetSetPaddingEx( UI_WIDGET( ENTRY_BUTTON( pEntry )), 0, 0, 0, 0 );
		
		(UI_WIDGET(ENTRY_BUTTON(pEntry)))->tickF = ugcEditorActionButtonTick;

		return pEntry;
	}
	return NULL;
}

static UIMenuItem *ugcEditorMenuItemCreateInternal(UGCActionID id, UIMenuItem ***refresh_list)
{
	const UGCActionDescription *desc = ugcEditorGetAction(id);
	if (desc)
	{
		char name_buf[128], cmd_buf[128];
		UIMenuItem *ret;
		const char *bind_str;

		if (desc->bOpensDialog)
			sprintf(name_buf, "%s...", TranslateMessageKey( desc->strNameMessage ));
		else
			strcpy(name_buf, TranslateMessageKey( desc->strNameMessage ));

		sprintf(cmd_buf, "UGC.Do %s", StaticDefineIntRevLookup(UGCActionIDEnum, id));
		ret = ui_MenuItemCreate(name_buf, UIMenuCommand, NULL, cmd_buf, NULL);
		ret->data.voidPtr = (void*)(intptr_t)id;

		if (refresh_list)
			eaPush(refresh_list, ret);

		bind_str = ugcEditorFindKeybind(cmd_buf);
		if (bind_str)
		{
			ui_MenuItemSetRightText(ret, bind_str);
		}
		return ret;
	}
	return ui_MenuItemCreate("INVALID COMMAND", UIMenuCommand, NULL, NULL, NULL);
}

UIMenuItem* ugcEditorMenuItemCreate(UGCActionID id)
{
	return ugcEditorMenuItemCreateInternal( id, NULL );
}

static UIMenuItem *ugcEditorCheckMenuItemCreate(UGCActionID id, UIMenuItem ***refresh_list)
{
	const UGCActionDescription *desc = ugcEditorGetAction(id);
	if (desc)
	{
		char name_buf[128], cmd_buf[128];
		UIMenuItem *ret;
		const char *bind_str;

		assert( !desc->bOpensDialog );
		strcpy(name_buf, TranslateMessageKey( desc->strNameMessage ));

		sprintf(cmd_buf, "UGC.Do %s", StaticDefineIntRevLookup(UGCActionIDEnum, id));
		ret = ui_MenuItemCreate(name_buf, UIMenuCheckButton, MenuItemCmdParseCallback, strdup(cmd_buf), NULL);

		if (refresh_list)
			eaPush(refresh_list, ret);

		bind_str = ugcEditorFindKeybind(cmd_buf);
		if (bind_str)
		{
			ui_MenuItemSetRightText(ret, bind_str);
		}
		return ret;
	}
	return ui_MenuItemCreate("INVALID COMMAND", UIMenuCommand, NULL, NULL, NULL);
}

void ugcEditorSetClassAndLevelCB( UIMenuItem* menuItem, UserData rawClass )
{
	UGCRespecClass* class = rawClass;
	UGCRespecClassLevel* level = menuItem->data.voidPtr;
	CharacterPath* classPath = GET_REF( class->hRespecClassName );
	char* estr = NULL;

	FormatMessageKey( &estr, "UGC.Query_Respec",
					  STRFMT_DISPLAYMESSAGE( "ClassPath", classPath->pDisplayName ),
					  STRFMT_INT( "Level", level->iLevel ),
					  STRFMT_END );

	if( UIYes != ugcModalDialog( TranslateMessageKey( "UGC.Query_Respec_Title" ), estr, UIYes | UINo )) {
		estrDestroy( &estr );
		return;
	}

	ServerCmd_gslUGC_RespecCharacter( ugcGetAllegianceDefaultsIndex( ugcEditorGetProjectData() ), REF_STRING_FROM_HANDLE( class->hRespecClassName ), level->iLevel );
	estrDestroy( &estr );
}

static void ugcEditorMinimizeCB( UIWidget* ignored, UserData ignored2 )
{
	gfxWindowMinimize();
}

static void ugcEditorRestoreCB( UIWidget* ignored, UserData ignored2 )
{
	gfxWindowRestoreToggle();
}

static void ugcEditorCloseCB( UIWidget* ignored, UserData ignored2 )
{
	if (!ugcEditorQueryLogout(true, false)) {
		return;
	}

   	utilitiesLibSetShouldQuit(true);
}

UGCEditorMenus *ugcEditorCreateMenus()
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	UGCEditorMenus *menus = calloc(1, sizeof(UGCEditorMenus));
	menus->root_pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitFixed, 0 );

	// We need this pane to be toplevel so its menu gets ticked first.
	menus->root_pane->widget.priority = UI_HIGHEST_PRIORITY;
	ui_PaneSetStyle( menus->root_pane, "UGC_Pane_Light_With_Inset", true, false );

	{
		UISprite* sprite = ui_SpriteCreate( 0, 0, -1, -1, "Icons_Foundry_Logo" );
		ui_PaneAddChild( menus->root_pane, UI_WIDGET( sprite ));
		menus->foundry_sprite = sprite;
	}
	
	menus->menu_bar = ui_MenuBarCreate(NULL);
	ui_PaneAddChild( menus->root_pane, menus->menu_bar );

	{
		UIMenu* file_menu = ui_MenuCreateMessage("UGC.Menu_File");
		ui_MenuBarAppendMenu(menus->menu_bar, file_menu);
		
		{
			UIMenu* file_create_menu = ui_MenuCreateMessage("UGC.Menu_File");
			ui_MenuAppendItem(file_menu, ui_MenuItemCreateMessage("UGC.Menu_New", UIMenuSubmenu, NULL, NULL, file_create_menu));
			
			ui_MenuAppendItems(file_create_menu,
							   ugcEditorMenuItemCreateInternal(UGC_ACTION_MAP_CREATE, &menus->refresh_list),
							   ugcEditorMenuItemCreateInternal(UGC_ACTION_COSTUME_CREATE, &menus->refresh_list),
							   ugcEditorMenuItemCreateInternal(UGC_ACTION_ITEM_CREATE, &menus->refresh_list),
							   NULL);
		}

		ui_MenuAppendItems(file_menu,
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_PLAY_MISSION, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_SAVE, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_PUBLISH, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_CLOSE, &menus->refresh_list),
						   ui_MenuItemCreate("", UIMenuSeparator, NULL, NULL, NULL),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_LOGOUT, &menus->refresh_list),
						   NULL);
	}

	{
		UIMenu* edit_menu = ui_MenuCreateMessage("UGC.Menu_Edit");
		ui_MenuBarAppendMenu(menus->menu_bar, edit_menu);
		
		ui_MenuAppendItems(edit_menu,
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_UNDO, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_REDO, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_CUT, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_COPY, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_PASTE, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_DELETE, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_DESELECT_ALL, &menus->refresh_list),
						   NULL);
	}

	{
		menus->help_menu = ui_MenuCreateMessage("UGC.Menu_Help");
		ui_MenuBarAppendMenu(menus->menu_bar, menus->help_menu);
		
		ui_MenuAppendItems(menus->help_menu,
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_REPORT_BUG, &menus->refresh_list),
						   ugcEditorMenuItemCreateInternal(UGC_ACTION_VIEW_EULA, &menus->refresh_list),
						   NULL);
	}

	/// Dynamicly shown menus:
	menus->map_editor_menu = ui_MenuCreateMessage("UGC.Menu_Map");

	ui_MenuAppendItems(menus->map_editor_menu,
					   ugcEditorMenuItemCreateInternal(UGC_ACTION_SET_BACKDROP, &menus->refresh_list),
					   ui_MenuItemCreate("", UIMenuSeparator, NULL, NULL, NULL),
					   ugcEditorMenuItemCreateInternal(UGC_ACTION_CREATE_MARKER, &menus->refresh_list),
					   ugcEditorMenuItemCreateInternal(UGC_ACTION_CREATE_RESPAWN, &menus->refresh_list),
					   ugcEditorMenuItemCreateInternal(UGC_ACTION_PLAY_MAP, &menus->refresh_list),
					   NULL);

	menus->mission_editor_menu = ui_MenuCreateMessage("UGC.Menu_Mission");
	ui_MenuAppendItems(menus->mission_editor_menu,
					   ugcEditorMenuItemCreateInternal(UGC_ACTION_PLAY_MISSION, &menus->refresh_list),
					   NULL);
	
	{
		int it;
		for( it = 0; it != eaSize( &defaults->allegiance ); ++it ) {
			UGCPerAllegianceDefaults* allegianceDefault = defaults->allegiance[ it ];
			if( eaSize( &allegianceDefault->respecClasses )) {
				UIMenu* menu = ui_MenuCreateMessage( "UGC.Menu_Respec" );
				int classIt;
				int levelIt;
				eaPush( &menus->allegiance_menus, menu );

				for( classIt = 0; classIt != eaSize( &allegianceDefault->respecClasses ); ++classIt ) {
					UGCRespecClass* class = allegianceDefault->respecClasses[ classIt ];
					UIMenu* classMenu = ui_MenuCreate( REF_STRING_FROM_HANDLE( class->hRespecClassName ));
					CharacterPath* classPath = GET_REF( class->hRespecClassName );
					ui_MenuAppendItem( menu, ui_MenuItemCreate( TranslateDisplayMessage( classPath->pDisplayName ), UIMenuSubmenu, NULL, NULL, classMenu ));
					
					for( levelIt = 0; levelIt != eaSize( &class->eaLevels ); ++levelIt ) {
						UGCRespecClassLevel* level = class->eaLevels[ levelIt ];
						char* estr = NULL;

						FormatMessageKey( &estr, "UGC.Menu_Level_Num",
										  STRFMT_INT( "Level", level->iLevel ),
										  STRFMT_END );
						ui_MenuAppendItem( classMenu, ui_MenuItemCreate( estr, UIMenuCallback, ugcEditorSetClassAndLevelCB, class, level ));
						estrDestroy( &estr );
					}
				}
			} else {
				eaPush( &menus->allegiance_menus, NULL );
			}
		}
	}

	menus->separator_after_menu_bar = ui_SeparatorCreate( UIVertical );
	ui_PaneAddChild( menus->root_pane, menus->separator_after_menu_bar );

	menus->save_button = ugcEditorButtonCreate( UGC_ACTION_SAVE, true, false );
	SET_HANDLE_FROM_STRING( "UISkin", "UGCEditor", UI_WIDGET( menus->save_button )->hOverrideSkin );
	ui_PaneAddChild( menus->root_pane, menus->save_button );
	
	menus->publish_button = ugcEditorButtonCreate( UGC_ACTION_PUBLISH, true, false );
	SET_HANDLE_FROM_STRING( "UISkin", "UGCEditor", UI_WIDGET( menus->publish_button )->hOverrideSkin );
	ui_PaneAddChild( menus->root_pane, menus->publish_button );

	menus->errors_button = ui_ButtonCreateWithIcon( "xxx", "ugc_icons_labels_alert", ugcEditorErrorsWindowShow, NULL );
	ui_ButtonSetImageEx( menus->errors_button, "ugc_icons_labels_alert", "ugc_icons_labels_alert", "ugc_icons_labels_alert", "ugc_icons_labels_alert", "ugc_icons_labels_alert_disabled" );
	SET_HANDLE_FROM_STRING( "UISkin", "UGCEditor", UI_WIDGET( menus->errors_button )->hOverrideSkin );
	ui_PaneAddChild( menus->root_pane, menus->errors_button );

	// Do not add these to the pane, they are added / removed dynamically
	menus->separator_before_window_buttons = ui_SeparatorCreate( UIVertical );
	menus->minimize_button = ui_ButtonCreateImageOnly( "ugc_icon_window_controls_minimize", 0, 0, ugcEditorMinimizeCB, NULL );
	SET_HANDLE_FROM_STRING( "UISkin", "UGCButton_Light", UI_WIDGET( menus->minimize_button )->hOverrideSkin );
	menus->restore_button = ui_ButtonCreateImageOnly( "ugc_icon_window_controls_restore", 0, 0, ugcEditorRestoreCB, NULL );
	SET_HANDLE_FROM_STRING( "UISkin", "UGCButton_Light", UI_WIDGET( menus->restore_button )->hOverrideSkin );
	menus->close_button = ui_ButtonCreateImageOnly( "ugc_icon_window_controls_close", 0, 0, ugcEditorCloseCB, NULL );
	SET_HANDLE_FROM_STRING( "UISkin", "UGCButton_Light", UI_WIDGET( menus->close_button )->hOverrideSkin );

	{
		UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_With_Inset" );
		ui_WidgetSetHeight( UI_WIDGET( menus->root_pane ), ui_WidgetGetHeight( UI_WIDGET( menus->menu_bar )) + ui_TextureAssemblyHeight( texas ));
	}

	return menus;
}

void ugcEditorShowContextMenu(UGCActionID *actions)
{
	int i = 0;

	if (!g_UGCEditorContextMenu)
		g_UGCEditorContextMenu = ui_MenuCreateMessage( "UGC.Menu_Context" );
	else
		ui_MenuClearAndFreeItems(g_UGCEditorContextMenu);

	while (actions[i] != 0)
	{
		UIMenuItem *item = ugcEditorMenuItemCreateInternal(actions[i], NULL);
		ui_MenuAppendItem(g_UGCEditorContextMenu, item);
		i++;
	}

	g_UGCEditorContextPosition[0] = g_ui_State.mouseX;
	g_UGCEditorContextPosition[1] = g_ui_State.mouseY;
	ui_MenuPopupAtCursor(g_UGCEditorContextMenu);

	i = 0;
	while (actions[i] != 0)
	{
		UIMenuItem *item = g_UGCEditorContextMenu->items[i];
		char* estr = NULL;
		item->active = ugcEditorQueryCommandByID(actions[i], &estr);
		estrDestroy( &estr );
		i++;
	}
}

bool ugcEditorGetContextMenuPosition(F32 *out_x, F32 *out_y)
{
	devassertmsg(out_x && out_y, "Must provide output x and y parameters to ugcEditorGetContextMenuPosition.");
	if (g_UGCEditorContextMenu && g_UGCEditorContextMenu->opened)
	{
		*out_x = g_UGCEditorContextPosition[0];
		*out_y = g_UGCEditorContextPosition[1];
		return true;
	}
	return false;
}

bool ugcEditorHasContextMenuPosition()
{
	return g_UGCEditorContextMenu && g_UGCEditorContextMenu->opened;
}

void ugcEditorMenusRefresh(UGCEditorMenus *menus, bool visible, UGCEditorViewMode mode)
{
	if (visible)
	{
		int allegianceIt = ugcGetAllegianceDefaultsIndex( ugcEditorGetProjectData() );

		ui_WidgetRemoveFromGroup(UI_WIDGET(menus->root_pane));
		ui_WidgetAddToDevice(UI_WIDGET(menus->root_pane), NULL);

		{
			int it;
			for( it = 0; it != eaSize( &menus->allegiance_menus ); ++it ) {
				ui_MenuBarRemoveMenu( menus->menu_bar, menus->allegiance_menus[ it ]);
			}
		}
		ui_MenuBarRemoveMenu(menus->menu_bar, menus->map_editor_menu);
		ui_MenuBarRemoveMenu(menus->menu_bar, menus->mission_editor_menu);
		ui_MenuBarRemoveMenu(menus->menu_bar, menus->help_menu);

		if( allegianceIt >= 0 && menus->allegiance_menus[ allegianceIt ]) {
			ui_MenuBarAppendMenu( menus->menu_bar, menus->allegiance_menus[ allegianceIt ]);
		}
		switch (mode)
		{
		xcase UGC_VIEW_MAP_EDITOR:
			ui_MenuBarAppendMenu(menus->menu_bar, menus->map_editor_menu);
		xcase UGC_VIEW_MISSION:
			ui_MenuBarAppendMenu(menus->menu_bar, menus->mission_editor_menu);
		};
		ui_MenuBarAppendMenu(menus->menu_bar, menus->help_menu);
		ui_MenuBarResize( menus->menu_bar );

		{
			UGCRuntimeError** errors = ugcEditorGetRuntimeErrors();
			char* estr = NULL;

			FormatMessageKey( &estr, "UGC.Errors_Button",
							  STRFMT_INT( "NumErrors", eaSize( &errors )),
							  STRFMT_END );
			ui_ButtonSetTextAndResize( menus->errors_button, estr );
			ui_SetActive( UI_WIDGET( menus->errors_button ), eaSize( &errors ) > 0 );
			estrDestroy( &estr );
		}

		{
			UGCProjectStatusQueryInfo* pStatus = NULL;
			UGCProjectReviews* pReviews;
			int iReviewsPageNumber;

			ugcEditorGetCachedProjectData( &pReviews, &iReviewsPageNumber, &pStatus );
			ui_SetActive( UI_WIDGET( menus->publish_button ), !SAFE_MEMBER( pStatus, bCurrentlyPublishing ));
		}

		// Now lay out all the items
		{
			float x = 0;
			
			ui_WidgetSetPositionEx( UI_WIDGET( menus->foundry_sprite ), x, 0, 0, 0, UILeft );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->foundry_sprite ));
			x += 10;
			ui_WidgetSetPositionEx( UI_WIDGET( menus->menu_bar ), x, 0, 0, 0, UILeft );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->menu_bar ));
			
			x += 2;
			ui_WidgetSetPositionEx( UI_WIDGET( menus->separator_after_menu_bar ), x, 0, 0, 0, UITopLeft );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->separator_after_menu_bar ));
			x += 2;
			
			ui_WidgetSetPositionEx( UI_WIDGET( menus->save_button ), x, 0, 0, 0, UILeft );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->save_button ));
			ui_WidgetSetPositionEx( UI_WIDGET( menus->publish_button ), x, 0, 0, 0, UILeft );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->publish_button ));
			ui_WidgetSetPositionEx( UI_WIDGET( menus->errors_button ), x, 0, 0, 0, UILeft );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->errors_button ));
		}

		ui_WidgetRemoveFromGroup( UI_WIDGET( menus->minimize_button ));
		ui_WidgetRemoveFromGroup( UI_WIDGET( menus->restore_button ));
		ui_WidgetRemoveFromGroup( UI_WIDGET( menus->close_button ));
		ui_WidgetRemoveFromGroup( UI_WIDGET( menus->separator_before_window_buttons ));
		if( gfxShouldShowRestoreButtons() ) {
			float x = 0;

			ui_WidgetSetPositionEx( UI_WIDGET( menus->close_button ), x, 0, 0, 0, UIRight );
			ui_PaneAddChild( menus->root_pane, menus->close_button );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->close_button ));

			ui_WidgetSetPositionEx( UI_WIDGET( menus->restore_button ), x, 0, 0, 0, UIRight );
			ui_PaneAddChild( menus->root_pane, menus->restore_button );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->restore_button ));

			ui_WidgetSetPositionEx( UI_WIDGET( menus->minimize_button ), x, 0, 0, 0, UIRight );
			ui_PaneAddChild( menus->root_pane, menus->minimize_button );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->minimize_button ));

			x += 2;
			ui_WidgetSetPositionEx( UI_WIDGET( menus->separator_before_window_buttons ), x, 0, 0, 0, UITopRight );
			ui_PaneAddChild( menus->root_pane, menus->separator_before_window_buttons );
			x = ui_WidgetGetNextX( UI_WIDGET( menus->separator_before_window_buttons ));
			x += 2;
		}
	}
	else
	{
		ui_WidgetRemoveFromGroup(UI_WIDGET(menus->root_pane));
	}
}

F32 ugcEditorGetMenuHeight(UGCEditorMenus *menus)
{
	return ui_WidgetGetHeight(UI_WIDGET(menus->root_pane));
}

void ugcEditorMenuOncePerFrame(UGCEditorMenus *menus)
{
	FOR_EACH_IN_EARRAY(menus->refresh_list, UIMenuItem, menu)
	{
		UGCActionID id = (intptr_t)menu->data.voidPtr;
		char* estr = NULL;
		if( menu->type == UIMenuCheckButton ) {
			char* command = menu->clickedData;
			id = StaticDefineIntGetInt( UGCActionIDEnum, command + strlen( "UGC.Do " ));
			ui_MenuItemSetCheckState( menu, ugcEditorQueryCommandByID(id, &estr));
		} else {
			menu->active = ugcEditorQueryCommandByID(id, &estr);
		}
		estrDestroy(&estr);
	}
	FOR_EACH_END;
}

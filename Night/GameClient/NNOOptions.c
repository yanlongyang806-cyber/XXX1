/***************************************************************************



***************************************************************************/

#include "NNOOptions.h"
#include "gclOptions.h"
#include "UIGen.h"
#include "gclEntity.h"
#include "gclKeyBind.h"
#include "AutoGen/GameServerLib_autogen_ServerCmdWrappers.h"

#include "NNOOptions_h_ast.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_EngineMisc););

///////////////////////////////////////////////////////////////////////////
//Gameplay Options

static void NWOptions_GetControlOptions(OptionSetting ***peaSettings, bool bIncludeControlScheme)
{
	static const char **s_eaBasicOptionNames = NULL;
	static const char **s_eaControlOptionNames = NULL;

	if (!s_eaBasicOptionNames)
	{
		eaPush(&s_eaBasicOptionNames, "Invert Camera X");
		eaPush(&s_eaBasicOptionNames, "Invert Camera Y");
		//eaPush(&s_eaBasicOptionNames, "Enable Rumble");
	}
	gclOptions_GetSettingsForCategoryNameAndOptionName(peaSettings, "Basic", &s_eaBasicOptionNames);

	if (!s_eaControlOptionNames)
	{
		eaPush(&s_eaControlOptionNames, "CamMouseLookFactor");
		//eaPush(&s_eaControlOptionNames, "CamControllerLookFactor");
		eaPush(&s_eaControlOptionNames, "DoubleTapDirToRoll");
		eaPush(&s_eaControlOptionNames, "PowerCursorActivation");
		eaPush(&s_eaControlOptionNames, "CameraShake");
	}
	if (bIncludeControlScheme)
	{
		gclOptions_GetSettingsForCategoryNameAndOptionName(peaSettings, "ControlScheme", &s_eaControlOptionNames);
	}
}

static void NWOptions_GetGameplayOptions(const char *pchSubgroup, OptionSetting ***peaSettings)
{
	if (stricmp(pchSubgroup, "General") == 0)
	{
		NWOptions_GetControlOptions(peaSettings, true);
	}
}

///////////////////////////////////////////////////////////////////////////
//Audio Options

static void NWOptions_GetVoiceOptions(OptionSetting ***peaSettings)
{
	gclOptions_GetSettingsForCategoryName(peaSettings, "Voice");
}

static void NWOptions_GetGeneralAudioOptions(OptionSetting ***peaSettings)
{
	static const char **s_eaAudioOptionExcludeNames = NULL;
	if (!s_eaAudioOptionExcludeNames)
	{
		eaPush(&s_eaAudioOptionExcludeNames, "Media Player");
	}
	gclOptions_GetSettingsForCategoryNameExcludingOptionName(peaSettings, "Audio", &s_eaAudioOptionExcludeNames);
}

static void NWOptions_GetAudioOptions(const char *pchSubgroup, OptionSetting ***peaSettings)
{
	if (stricmp(pchSubgroup, "General") == 0)
	{
		NWOptions_GetGeneralAudioOptions(peaSettings);
	}
	else if (stricmp(pchSubgroup, "Voice") == 0)
	{
		NWOptions_GetVoiceOptions(peaSettings);
	}
}

///////////////////////////////////////////////////////////////////////////
//UI Options

static void NWOptions_GetHUDOptions(OptionSetting ***peaSettings)
{
	static const char **s_eaHUDOptionExcludeNames = NULL;
	if (!s_eaHUDOptionExcludeNames)
	{
		eaPush(&s_eaHUDOptionExcludeNames, "Hudregion");
		eaPush(&s_eaHUDOptionExcludeNames, "ShowReticlesAs");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Enemy_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Friendlynpc_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Friend_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Supergroup_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Team_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Pet_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_EnemyPlayer_Reticle");
		eaPush(&s_eaHUDOptionExcludeNames, "Show_Player_Reticle");
	}
	gclOptions_GetSettingsForCategoryNameExcludingOptionName(peaSettings, "HUDOptions", &s_eaHUDOptionExcludeNames);
}

static void NWOptions_GetChatOptions(OptionSetting ***peaSettings)
{
	gclOptions_GetSettingsForCategoryName(peaSettings, "Chat");
}

static void NWOptions_GetGeneralInterfaceOptions(OptionSetting ***peaSettings)
{
	static const char **s_eaBasicOptionNames = NULL;

	if (!s_eaBasicOptionNames)
	{
		eaPush(&s_eaBasicOptionNames, "Uiscale");
		eaPush(&s_eaBasicOptionNames, "Tooltipdelay");
	}
	gclOptions_GetSettingsForCategoryNameAndOptionName(peaSettings, "Basic", &s_eaBasicOptionNames);
}

static void NWOptions_GetInterfaceOptions(const char *pchSubgroup, OptionSetting ***peaSettings)
{
	if (stricmp(pchSubgroup, "HUD") == 0)
	{
		NWOptions_GetHUDOptions(peaSettings);
	}
	else if (stricmp(pchSubgroup, "Chat") == 0)
	{
		NWOptions_GetChatOptions(peaSettings);
	}
	else if (stricmp(pchSubgroup, "General") == 0)
	{
		NWOptions_GetGeneralInterfaceOptions(peaSettings);
	}
}

///////////////////////////////////////////////////////////////////////////
//General Options Expressions

extern ParseTable parse_OptionSetting[];

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NWOptionsGetSettingsForGroup);
void NWOptionsExpr_GetSettingsForGroup(SA_PARAM_NN_VALID UIGen *pGen, const char *pchGroup, const char *pchSubgroup)
{
	static OptionSetting **s_eaSettings = NULL;
	eaClear(&s_eaSettings);

	if (stricmp(pchGroup, "Gameplay") == 0)
	{
		NWOptions_GetGameplayOptions(pchSubgroup, &s_eaSettings);
	}
	else if (stricmp(pchGroup, "Audio") == 0)
	{
		NWOptions_GetAudioOptions(pchSubgroup, &s_eaSettings);
	}
	else if (stricmp(pchGroup, "Video") == 0)
	{
		//These are handled separately for now
	}
	else if (stricmp(pchGroup, "Interface") == 0)
	{
		NWOptions_GetInterfaceOptions(pchSubgroup, &s_eaSettings);
	}
	else
	{
		ErrorFilenamef(pGen->pchFilename, "NWOptionsGetSettingsForGroup expression called with invalid group name: %s", pchGroup);
	}

	ui_GenSetListSafe(pGen, &s_eaSettings, OptionSetting);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NWOptionsGetGroupList);
void NWOptionsExpr_GetGroupList(SA_PARAM_NN_VALID UIGen *pGen)
{
	static NWOptionsGroup **s_eaGroups = NULL;

	if (!s_eaGroups)
	{
		NWOptionsGroup *pGameplayGroup = StructCreate(parse_NWOptionsGroup);
		NWOptionsGroup *pAudioGroup = StructCreate(parse_NWOptionsGroup);
		NWOptionsGroup *pVideoGroup = StructCreate(parse_NWOptionsGroup);
		NWOptionsGroup *pInterfaceGroup = StructCreate(parse_NWOptionsGroup);

		pGameplayGroup->pchName = "Gameplay";
		SET_HANDLE_FROM_STRING("Message", "NWOptions_Gameplay", pGameplayGroup->hDisplayName);
		eaPush(&s_eaGroups, pGameplayGroup);

		pAudioGroup->pchName = "Audio";
		SET_HANDLE_FROM_STRING("Message", "NWOptions_Audio", pAudioGroup->hDisplayName);
		eaPush(&s_eaGroups, pAudioGroup);

		pVideoGroup->pchName = "Video";
		SET_HANDLE_FROM_STRING("Message", "NWOptions_Video", pVideoGroup->hDisplayName);
		eaPush(&s_eaGroups, pVideoGroup);

		pInterfaceGroup->pchName = "Interface";
		SET_HANDLE_FROM_STRING("Message", "NWOptions_Interface", pInterfaceGroup->hDisplayName);
		eaPush(&s_eaGroups, pInterfaceGroup);
	}

	ui_GenSetList(pGen, &s_eaGroups, parse_NWOptionsGroup);
}

///////////////////////////////////////////////////////////////////////////
//Default Settings

static void NWOptions_RestoreDefaultsInternal(OptionSetting ***peaSettings)
{
	S32 i;

	for (i = 0; i < eaSize(peaSettings); ++i)
	{
		OptionSetting *pSetting = (*peaSettings)[i];
		S32 iValue = pSetting->iIntValue;
		if (pSetting->cbRestoreDefaults)
		{
			pSetting->cbRestoreDefaults(pSetting);
		}
		else
		{
			// If we can't restore defaults, at least restore it to what it was
			// when the dialog opened.
			pSetting->iIntValue = pSetting->iOrigIntValue;
		}

		if (pSetting->iIntValue != iValue)
		{
			OptionSettingChanged(pSetting, true);
			OptionSettingConfirm(pSetting);
		}
	}
}

static void NWOptions_RestoreGameplayDefaults(void)
{
	OptionSetting **eaSettings = NULL;

	gclKeyBindUnbindAll();

	NWOptions_GetControlOptions(&eaSettings, false);

	NWOptions_RestoreDefaultsInternal(&eaSettings);

	ServerCmd_schemes_Reset();

	eaDestroy(&eaSettings);
}

static void NWOptions_RestoreAudioDefaults(void)
{
	OptionSetting **eaSettings = NULL;

	NWOptions_GetGeneralAudioOptions(&eaSettings);

	NWOptions_RestoreDefaultsInternal(&eaSettings);

	eaClear(&eaSettings);

	NWOptions_GetVoiceOptions(&eaSettings);

	NWOptions_RestoreDefaultsInternal(&eaSettings);

	eaDestroy(&eaSettings);
}

static void NWOptions_RestoreVideoDefaults(void)
{
	Options_RestoreDefaultsFor("Graphics");
}

static void NWOptions_RestoreInterfaceDefaults(void)
{
	OptionSetting **eaSettings = NULL;

	NWOptions_GetChatOptions(&eaSettings);

	NWOptions_RestoreDefaultsInternal(&eaSettings);

	Options_RestoreDefaultsFor("HUDOptions");

	eaClear(&eaSettings);

	NWOptions_GetGeneralInterfaceOptions(&eaSettings);

	NWOptions_RestoreDefaultsInternal(&eaSettings);

	eaDestroy(&eaSettings);
}

AUTO_COMMAND ACMD_ACCESSLEVEL(0) ACMD_NAME("NWOptions_RestoreDefaultsForGroup") ACMD_HIDE;
void NWOptions_RestoreDefaultsFor(const char *pchGroup)
{
	if (stricmp(pchGroup, "Gameplay") == 0)
	{
		NWOptions_RestoreGameplayDefaults();
	}
	else if (stricmp(pchGroup, "Audio") == 0)
	{
		NWOptions_RestoreAudioDefaults();
	}
	else if (stricmp(pchGroup, "Video") == 0)
	{
		NWOptions_RestoreVideoDefaults();
	}
	else if (stricmp(pchGroup, "Interface") == 0)
	{
		NWOptions_RestoreInterfaceDefaults();
	}
}

#include "AutoGen/NNOOptions_h_ast.c"
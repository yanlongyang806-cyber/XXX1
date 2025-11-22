#include "Character.h"
#include "cmdParse.h"
#include "../../libs/WorldLib/dynFx.h"
#include "../../libs/WorldLib/wlState.h"
#include "file.h"
#include "dynFxInterface.h"
#include "GameClientLib.h"
#include "GameStringFormat.h"
#include "gclBaseStates.h"
#include "gclOptions.h"
#include "gclUIGen.h"
#include "GfxLCD.h"
#include "GfxLightOptions.h"
#include "gfxSettings.h"
#include "GfxMapSnap.h"
#include "GlobalTypes.h"
#include "GlobalStateMachine.h"
#include "GraphicsLib.h"
#include "RenderLib.h"
#include "RdrShader.h"
#include "RdrState.h"
#include "GenericMesh.h"
#include "gclEntity.h"
#include "../common/NNOCharacterBackground.h"
#include "GameAccountDataCommon.h"
#include "ItemCommon.h"
#include "Autogen/itemCommon_h_ast.h"
#include "ItemEnums.h"
#include "Autogen/itemEnums_h_ast.h"
#include "inventoryCommon.h"
#include "inputGamepad.h"
#include "inputJoystick.h"

#ifndef _XBOX
#include "resource1.h"
#endif

#define CLIENT_MELEE_RANGE 10.5	//aiGlobalSettings.meleeMaximumDistance isn't available on the client, so I had
								// to add a duplicate definition. Not ideal, I know.Slightly  less than 11 to
								// make up for client/server disagreement about where the player is actually standing.

extern int gClickSize;
extern int gDoubleClickSize;

struct 
{
	// The FX applied on the target point
	dtFx hActionPointFullFX;

} s_NNOClientStateInfo;

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_GameSystems););


AUTO_RUN_FIRST;
void InitGlobalConfig(void)
{
#ifndef _XBOX
	gGCLState.logoResource = IDR_CRYPTIC_LOGO;
#endif
	gGCLState.iconResource = IDI_ICON1;
	gGCLState.bAudioListenerModeShortBoom = 1;
	gGCLState.bEnableStereoscopic = 1;	// Enabling stereoscopic api by default for Neverwinter.  This should not be moved off of tip.

	gfxSetTargetHighlight(true);

	gfxSetFeatures(GFEATURE_POSTPROCESSING|GFEATURE_DOF|GFEATURE_SHADOWS|GFEATURE_WATER);

	gfxSetDefaultLightingOptions();
	gfx_lighting_options.enableDOFCameraFade = true;
	gfx_lighting_options.disableHighBloomQuality = true;
	gfx_lighting_options.disableHighBloomIntensity = true;

	gfxSettingsSetScattering(GSCATTER_LOW_QUALITY);
	gfxSettingsSetHDRMaxLuminanceMode(true);
	rdrSetDisableToneCurve10PercentBoost(true);

	gConf.bHideChatWindow = 1;
	gConf.iFontSmallSize = 12;
	gConf.iFontMediumSize = 16;
	gConf.iFontLargeSize = 20;

	gClickSize = 12;
	gDoubleClickSize = 4;

	wlSetInteractibleUseCharacterLighting();
	simplygonSetEnabled(false);
}

AUTO_RUN_LATE;
void OverrideConfig(void)
{
	rdrShaderSetGlobalDefine(0, "TINT_SHADOW"); // Allows shadow color tinting
	//rdrShaderSetGlobalDefine(2, "SIDE_AS_RIMLIGHT"); // Allows a harsh, directional rimlight driven by the atmospheric side light color

	gfxEnableDiffuseWarp();
	gfxEnableBloomToneCurve(false);

	g_bMapSnapUseSunIndoors = true;
	
	rdr_state.alphaInDOF = 1;
	rdr_state.fastParticlesInPreDOF = 1;

	// don't want the extra shaders, and we don't want the material editor to reflect the expense
	gfx_lighting_options.bRequireProjectorLights = false;
	gfxLightingLockSpecularToDiffuseColor(true);
}


static bool showAlways(OptionSetting *setting)
{
	return false;
}

void OVERRIDE_LATELINK_schemes_ControlSchemeOptionInit(const char* pchCategory)
{
	// always show
	OptionCategoryMoveToPosition("ControlScheme", 1);
	options_HideOption(pchCategory, "AutoAttackDelay", showAlways);
	options_HideOption(pchCategory, "DelayAutoAttackUntilCombat", showAlways);
	options_UnhideOption(pchCategory, "DoubleTapDirToRoll");

	gamepadSetEnabled(false);
	joystickSetEnabled(false);

}


void OVERRIDE_LATELINK_gclOncePerFrame_GameSpecific(F32 elapsed)
{
	bool bKillFX = true;

	if (GSM_IsStateActive(GCL_GAMEPLAY) && !gbNoGraphics)
	{
		Entity *pEnt = entActivePlayerPtr();
		if (pEnt)
		{
			if (exprEntGetPowerPercent(pEnt) >= 1.f)
			{
				bKillFX = false;
				if (!s_NNOClientStateInfo.hActionPointFullFX)
				{
					s_NNOClientStateInfo.hActionPointFullFX = dtAddFx(	pEnt->dyn.guidFxMan, "FX_Alienware_ActionPoints_Full", 
						NULL, 0, 0, 0.f, 0, NULL, eDynFxSource_UI, NULL, NULL);
				}
			}
		}
	}

	if (bKillFX && s_NNOClientStateInfo.hActionPointFullFX)
	{
		dtFxKill(s_NNOClientStateInfo.hActionPointFullFX);
		s_NNOClientStateInfo.hActionPointFullFX = 0;
	}

	return;
}

void OVERRIDE_LATELINK_gclUpdateLCD(void)

{
	Entity *pEnt = entActivePlayerPtr();
	static char *s_pch;

	if(!gfxLCDIsEnabled()){
		return;
	}

	if ((pEnt && pEnt->pChar && pEnt->pChar->pattrBasic))
	{
		GameAccountDataExtract *pExtract = entity_GetCachedGameAccountDataExtract(pEnt);
		
		if(gfxLCDIsQVGA()){
			//Action Points
			estrClear(&s_pch);
			FormatGameMessageKey(&s_pch, "LCD_POWER", STRFMT_FLOAT("Power", pEnt->pChar->pattrBasic->fPower), STRFMT_END);
			gfxLCDAddText(s_pch, 0xFFFFFFff);

			//Open Inventory Slots
			estrClear(&s_pch);
			FormatGameMessageKey(&s_pch, "LCD_INVENTORY",
				STRFMT_INT("Open", inv_ent_AvailableSlots(pEnt,InvBagIDs_Inventory, pExtract)), STRFMT_END);
			gfxLCDAddText(s_pch, 0xFFFFFFff);
		}

		//Health
		estrClear(&s_pch);
		FormatGameMessageKey(&s_pch, "LCD_HP",
			STRFMT_INT("Current", pEnt->pChar->pattrBasic->fHitPoints),
			STRFMT_INT("Max", pEnt->pChar->pattrBasic->fHitPointsMax),
			STRFMT_END);
		gfxLCDAddMeter(s_pch, (S32)pEnt->pChar->pattrBasic->fHitPoints, 0, (S32)pEnt->pChar->pattrBasic->fHitPointsMax, 0xFFFFFFff, 0xFF0000ff, 0xFFFF00ff, 0x00FF00ff);

		//Experience
		estrClear(&s_pch);
		FormatGameMessageKey(&s_pch, "LCD_XP", STRFMT_END);
		gfxLCDAddMeter(s_pch, entity_ExpOfNextExpLevel(pEnt) - entity_ExpToNextExpLevel(pEnt),
			entity_ExpOfCurrentExpLevel(pEnt),
			entity_ExpOfNextExpLevel(pEnt),
			0xFFFFFFff, 0x000044ff, 0x0000AAff, 0x0000FFff);

		//Level
		if(gfxLCDIsQVGA()){
			estrClear(&s_pch);
			FormatGameMessageKey(&s_pch, "LCD_LEVEL", STRFMT_INT("Level", entity_GetSavedExpLevel(pEnt)), STRFMT_END);
			gfxLCDAddText(s_pch, 0xFFFF00ff);
		}
	}
	if (pEnt)
	{
		//Name
		gfxLCDAddText(entGetLocalName(pEnt), 0xFF8800ff);
	}
}


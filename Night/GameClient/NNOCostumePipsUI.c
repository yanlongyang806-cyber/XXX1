
#include "CharacterCreationUI.h"
#include "CostumeCommon.h"
#include "CostumeCommonLoad.h"
#include "CostumeCommonRandom.h"
#include "CostumeCommonTailor.h"
#include "GameAccountDataCommon.h"
#include "gclBaseStates.h"
#include "gclCostumeLineUI.h"
#include "gclCostumeUI.h"
#include "gclCostumeUIState.h"
#include "gclEntity.h"
#include "GlobalStateMachine.h"
#include "Guild.h"
#include "Player.h"
#include "ReferenceSystem.h"
#include "species_common.h"
#include "StringCache.h"
#include "UIGen.h"
#include "GameClientLib.h"
#include "NNOCostumePipsUI_c_ast.h"
#include "editors\gclCostumeView.h"


AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_UISystem););

AUTO_STRUCT;
typedef struct CostumeUIPip
{
	F32 x;
	F32 y;
	S32 OffsetX;
	S32 OffsetY;
	S32 WiggleY; // To avoid overlaps with other pips
	S32 iLineTypeMask;
	char* pcBoneMask;
	char* pcAddRegions;
	char* pcScaleGroups;
	char* pcBodyScales;
	const char* pcPositionBone; AST(POOL_STRING)
	const char* pcNameMessage; AST(POOL_STRING)
	U32 iLinesRequired;
} CostumeUIPip;

static CostumeUIPip** s_eaQueuedPips = NULL;
static bool s_bResetRequested = false;

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_ClearPips");
void exprCostumeCreator_ClearPips()
{
	s_bResetRequested = true;
	eaClearFast(&s_eaQueuedPips);
}

void CostumeCreator_PositionPipRelativeToUIGen(CostumeUIPip* pPip, SA_PARAM_NN_VALID UIGen* pSelf)
{
	GfxCameraController * pCam = costumeView_GetCamera();
	if (!pPip || !pSelf)
		return;
 	CostumeCreation_GetBoneScreenLocation(pPip->pcPositionBone, &pPip->x, &pPip->y);
	pPip->x -= pSelf->ScreenBox.left;
	pPip->y -= pSelf->ScreenBox.top;
	pPip->x += pPip->OffsetX * 15/pCam->camdist;
	pPip->y += pPip->OffsetY * 15/pCam->camdist;
}

U32 costumeLineUI_CountLinesRequired(CostumeUIPip* pPip);

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_AddRegionPip");
void exprCostumeCreator_AddRegionPip(const char* pcRegions, const char* pcPositionBone, const char* pcNameMessage, int xOffset, int yOffset)
{
	PCBoneDef* pBone = RefSystem_ReferentFromString(g_hCostumeBoneDict, pcPositionBone);
	if (pBone)
	{
		CostumeUIPip* pNewPip = StructCreate(parse_CostumeUIPip);
		pNewPip->OffsetX = xOffset;
		pNewPip->OffsetY = yOffset;
 		pNewPip->pcAddRegions = strdup(pcRegions);
		pNewPip->pcPositionBone = allocAddString(pBone->pcClickBoneName);
		pNewPip->pcNameMessage = allocAddString(pcNameMessage);
		pNewPip->iLinesRequired = 0; //costumeLineUI_CountLinesRequired(pNewPip);
		eaPush(&s_eaQueuedPips, pNewPip);
	}
}


AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_AddBonePip");
void exprCostumeCreator_AddBonePip(const char* pcBoneMask, const char* pcPositionBone, const char* pcScaleGroups, const char* pcBodyScales, const char* pcNameMessage, int xOffset, int yOffset, bool bRequireOwnerRegion)
{
	PCBoneDef* pBone = RefSystem_ReferentFromString(g_hCostumeBoneDict, pcPositionBone);
	if (pBone)
	{
		CostumeUIPip* pNewPip = NULL;
		if (bRequireOwnerRegion)
		{
			int i;
			bool bFound = false;
			for (i = 0; i < eaSize(&g_CostumeEditState.eaFindRegions); i++)
			{
				if (REF_COMPARE_HANDLES(pBone->hRegion,g_CostumeEditState.eaFindRegions[i]->hRegion) == 0)
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
				return;
		}
		pNewPip = StructCreate(parse_CostumeUIPip);
		pNewPip->OffsetX = xOffset;
		pNewPip->OffsetY = yOffset;
		pNewPip->pcBoneMask = strdup(pcBoneMask);
		pNewPip->pcScaleGroups = strdup(pcScaleGroups);
		pNewPip->pcBodyScales = strdup(pcBodyScales);
		pNewPip->pcPositionBone = allocAddString(pBone->pcClickBoneName);
		pNewPip->pcNameMessage = allocAddString(pcNameMessage);
		pNewPip->iLinesRequired = 0; //costumeLineUI_CountLinesRequired(pNewPip);
		eaPush(&s_eaQueuedPips, pNewPip);
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_AddRegionAndBonePip");
void exprCostumeCreator_AddRegionAndBonePip(const char* pcRegions, const char* pcBoneMask, const char* pcPositionBone, const char* pcScaleGroups, const char* pcBodyScales, const char* pcNameMessage, int xOffset, int yOffset, bool bRequireOwnerRegion)
{
	PCBoneDef* pBone = RefSystem_ReferentFromString(g_hCostumeBoneDict, pcPositionBone);
	if (pBone)
	{
		CostumeUIPip* pNewPip = NULL;
		if (bRequireOwnerRegion)
		{
			int i;
			bool bFound = false;
			for (i = 0; i < eaSize(&g_CostumeEditState.eaFindRegions); i++)
			{
				if (REF_COMPARE_HANDLES(pBone->hRegion,g_CostumeEditState.eaFindRegions[i]->hRegion) == 0)
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
				return;
		}
		pNewPip = StructCreate(parse_CostumeUIPip);
		pNewPip->OffsetX = xOffset;
		pNewPip->OffsetY = yOffset;
		pNewPip->pcAddRegions = strdup(pcRegions);
		pNewPip->pcBoneMask = strdup(pcBoneMask);
		pNewPip->pcScaleGroups = strdup(pcScaleGroups);
		pNewPip->pcBodyScales = strdup(pcBodyScales);
		pNewPip->pcPositionBone = allocAddString(pBone->pcClickBoneName);
		pNewPip->pcNameMessage = allocAddString(pcNameMessage);
		pNewPip->iLinesRequired = 0; //costumeLineUI_CountLinesRequired(pNewPip);
		eaPush(&s_eaQueuedPips, pNewPip);
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_AddMaskedRegionAndBonePip");
void exprCostumeCreator_AddMaskedRegionAndBonePip(const char* pcRegions, const char* pcBoneMask, const char* pcPositionBone, const char* pcScaleGroups, const char* pcBodyScales, const char* pcNameMessage, int xOffset, int yOffset, bool bRequireOwnerRegion, S32 LineTypeMask)
{
	PCBoneDef* pBone = RefSystem_ReferentFromString(g_hCostumeBoneDict, pcPositionBone);
	if (pBone)
	{
		CostumeUIPip* pNewPip = NULL;
		if (bRequireOwnerRegion)
		{
			int i;
			bool bFound = false;
			for (i = 0; i < eaSize(&g_CostumeEditState.eaFindRegions); i++)
			{
				if (REF_COMPARE_HANDLES(pBone->hRegion,g_CostumeEditState.eaFindRegions[i]->hRegion) == 0)
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
				return;
		}
		pNewPip = StructCreate(parse_CostumeUIPip);
		pNewPip->OffsetX = xOffset;
		pNewPip->OffsetY = yOffset;
		pNewPip->pcAddRegions = strdup(pcRegions);
		pNewPip->pcBoneMask = strdup(pcBoneMask);
		pNewPip->pcScaleGroups = strdup(pcScaleGroups);
		pNewPip->pcBodyScales = strdup(pcBodyScales);
		pNewPip->pcPositionBone = allocAddString(pBone->pcClickBoneName);
		pNewPip->pcNameMessage = allocAddString(pcNameMessage);
		pNewPip->iLineTypeMask = LineTypeMask;
		pNewPip->iLinesRequired = 0; //costumeLineUI_CountLinesRequired(pNewPip);
		eaPush(&s_eaQueuedPips, pNewPip);
	}
}

// How much to move A to avoid the collision with B.
bool overlapPips(CostumeUIPip* pA, CostumeUIPip* pB, int iPipHeight, F32* pOverlap)
{
	F32 fBMinusA = (pB->y + pB->WiggleY) - (pA->y + pA->WiggleY);
	bool bResult = false;
	if( fBMinusA >= 0 && fBMinusA <= iPipHeight )
	{
		*pOverlap = -fBMinusA;
		if( fBMinusA < ((F32)iPipHeight) / 2.0f )
		{
			*pOverlap += iPipHeight;
		}
		bResult = true;
	}
	else
	{
		*pOverlap = 0.0f;
	}
	return bResult;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_GetPips");
void exprCostumeCreator_GetPips(SA_PARAM_NN_VALID UIGen* pSelf, int iPipHeight)
{
	//int i,j;
	//F32 fMag, fOverlap;
	//bool bMoved;
	CostumeUIPip ***peaPips = ui_GenGetManagedListSafe(pSelf, CostumeUIPip);

	if (s_bResetRequested)
	{
		eaDestroyStruct(peaPips, parse_CostumeUIPip);
		s_bResetRequested = false;
	}

	eaPushEArray(peaPips, &s_eaQueuedPips);
	eaClearFast(&s_eaQueuedPips);
	
	/*
	// The following does some cool stuff with moving the pips so they line up with the associated bone, but don't overlap each other.
	// We're no longer presenting the pips this way, so this is wasted work, and I'm commenting it out.
	for (i = eaSize(peaPips)-1; i >= 0; i--)
	{
		CostumeCreator_PositionPipRelativeToUIGen((*peaPips)[i], pSelf);
	}

	for (i = eaSize(peaPips)-1; i >= 0; i--)
	{
		(*peaPips)[i]->WiggleY = 0;
	}

	// Now move them a bit to avoid overlapping
	fMag = 1.0f;
	bMoved = true;
	while(bMoved && fMag > 0.5f)
	{
		bMoved = false;
		for (i = eaSize(peaPips)-1; i >= 0; i--)
		{
			for (j = eaSize(peaPips)-1; j >= 0; j--)
			{
				if( (i != j) && ((*peaPips)[i]->OffsetX == (*peaPips)[j]->OffsetX) && overlapPips((*peaPips)[i], (*peaPips)[j], iPipHeight, &fOverlap) )
				{
					// Move each one a bit more than half of the overlap. 
					// This tends to keep the algorithm working when fMag gets small.
					(*peaPips)[i]->WiggleY -= 0.6f * fMag * fOverlap;
					(*peaPips)[j]->WiggleY += 0.6f * fMag * fOverlap;
					bMoved = true;
				}
			}
		}
		fMag *= 0.95f;
	}
	*/
	ui_GenSetManagedListSafe(pSelf, peaPips, CostumeUIPip, true);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_SetRegions");
void exprCostumeCreator_SetRegions(const char* pcRegions)
{
	char* context = NULL;
	char* pchString = strdup(pcRegions);
	char* pTok = strtok_s(pchString, " ", &context);
	CostumeCreator_ClearRegionList();
	while(pTok)
	{
		CostumeCreator_AddRegion(pTok);
		pTok = strtok_s(NULL, " ", &context);
	}
	free(pchString);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("CostumeCreator_SetScaleGroups");
void exprCostumeCreator_SetScaleGroups(const char* pcScaleGroups)
{
	char* context = NULL;
	char* pchString = strdup(pcScaleGroups);
	char* pTok = strtok_s(pchString, " ", &context);
	CostumeCreator_ClearScaleGroupList();
	while(pTok)
	{
		CostumeCreator_AddScaleGroup(pTok);
		pTok = strtok_s(NULL, " ", &context);
	}
	free(pchString);
}

extern bool g_MirrorSelectMode;
extern bool g_GroupSelectMode;
extern bool g_bCountNone;
extern bool g_bOmitHasOnlyOne;

// Hey, this is slow, but changing costumeLineUI_UpdateLines() would be MUCH worse.
U32 costumeLineUI_CountLinesRequired(CostumeUIPip* pPip)
{
	unsigned int iCount = 0;
	static CostumeEditLine ** s_eaEditLine = NULL;

	// Apply pPip, just like it had been clicked.
	{
		costumeLineUI_SetBodyScalesRule(kCostumeUIBodyScaleRule_AfterRegions);
		CostumeCreator_ClearRegionList();
		CostumeCreator_ClearScaleGroupList();
		CostumeCreator_AddScaleGroup("Face");
		CostumeCreator_FilterBodyScaleList(NULL, "Face", "");
		if( pPip->pcAddRegions && *pPip->pcAddRegions )
		{
			exprCostumeCreator_SetRegions(pPip->pcAddRegions);
		}
		exprCostumeCreator_SetScaleGroups(pPip->pcScaleGroups);

		if( pPip->pcBodyScales && *pPip->pcBodyScales )
		{
			costumeLineUI_SetBodyScalesRule(kCostumeUIBodyScaleRule_AfterRegions);
			// HumanPreset MUST be included if any filters are set, otherwise the bodyscale preset dropdown won't populate.
			CostumeCreator_FilterBodyScaleList(NULL, pPip->pcBodyScales, ""); //  + ", HumanPreset"
		}
		else
		{
			costumeLineUI_SetBodyScalesRule(kCostumeUIBodyScaleRule_Disabled);
		}

		if( !pPip->iLineTypeMask )
		{
			costumeLineUI_SetAllowedLineTypes(
				kCostumeEditLineType_Divider
				| kCostumeEditLineType_Region
				| kCostumeEditLineType_Category
				| kCostumeEditLineType_Geometry
				| kCostumeEditLineType_Material
				| kCostumeEditLineType_Texture0
				| kCostumeEditLineType_Texture1
				| kCostumeEditLineType_Texture2
				| kCostumeEditLineType_Texture3
				| kCostumeEditLineType_TextureScale
				| kCostumeEditLineType_Scale
				| kCostumeEditLineType_BodyScale);
		}
		else
		{
			costumeLineUI_SetAllowedLineTypes(pPip->iLineTypeMask);
		}
	}

	costumeLineUI_UpdateLines(g_CostumeEditState.pCostume, &s_eaEditLine,
			GET_REF(g_CostumeEditState.hSpecies), GET_REF(g_CostumeEditState.hSkeleton),
			g_CostumeEditState.eFindTypes, g_CostumeEditState.iBodyScalesRule,
			&g_CostumeEditState.eaFindRegions, g_CostumeEditState.eaFindScaleGroup,
			g_CostumeEditState.eaBodyScalesInclude, g_CostumeEditState.eaBodyScalesExclude,
			g_CostumeEditState.eaIncludeBones, g_CostumeEditState.eaExcludeBones,
			g_CostumeEditState.pSlotType, g_CostumeEditState.pchCostumeSet, g_CostumeEditState.bLineListHideMirrorBones, g_CostumeEditState.bUnlockAll, g_MirrorSelectMode, g_GroupSelectMode, g_bCountNone, g_bOmitHasOnlyOne, g_CostumeEditState.bCombineLines,
			g_CostumeEditState.bTextureLinesForCurrentPartValuesOnly, g_CostumeEditState.eaUnlockedCostumes, g_CostumeEditState.eaPowerFXBones);
	if( eaSize(&s_eaEditLine) > 0 )
	{
		CostumeEditLine *pEditLine = s_eaEditLine[0];
		iCount = costumeLineUI_GetCostumeEditSubLineListSizeInternal(pEditLine, pEditLine->iType);
	}
	costumeLineUI_DestroyLines(&s_eaEditLine);
	eaClear(&s_eaEditLine);

	return iCount;
}

#include "NNOCostumePipsUI_c_ast.c"
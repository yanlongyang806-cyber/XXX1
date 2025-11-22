#include "NNOGatewayGameMapping.h"
#include "AutoGen/NNOGatewayGameMapping_h_ast.h"
#include "NNOGatewayMappedEntity_c_ast.h"
#include "Entity.h"
#include "GlobalTypes.h"
#include "Gateway/gslGatewayGame.h"
#include "SuperCritterPet.h"
#include "itemEnums.h"
#include "AutoGen/itemEnums_h_ast.h"
#include "NNOGatewayCommonMappings.h"
#include "SuperCritterPet.h"

#include "NNOGatewayCommonMappings_h_ast.h"
#include "Gateway/gslGatewaySession.h"

void SubscribeGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams)
{
	char achID[24];
	Entity *pLoginEntity = session_GetLoginEntity(psess);

	if(pLoginEntity)
	{
		itoa(pLoginEntity->myContainerID, achID, 10);
		RefSystem_SetHandleFromString(GlobalTypeToCopyDictionaryName(GLOBALTYPE_GATEWAYGAMEDATA),achID,REF_HANDLEPTR(ptracker->hGatewayGameData));
		ptracker->phRef = (RefTo *)OFFSET_PTR(ptracker, ptracker->pMapping->offReference);
	}
}

bool IsReadyGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker)
{
	if(GET_REF(ptracker->hGatewayGameData) == NULL)
	{
		if(REF_IS_REFERENT_SET_BY_SOURCE_FROM_HANDLE(ptracker->hGatewayGameData))
		{
			const char *pchID = REF_STRING_FROM_HANDLE(ptracker->hGatewayGameData);

			if(pchID)
			{
				U32 iID	= atoi(pchID);

				if(iID)
					gslGatewayGame_CreateContainerForID(iID);
			}
		}
		return false;
	}

	return true;
}

void *CreateMappedGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	Entity *pLoginEntity = session_GetLoginEntity(psess);
	MappedGatewayGameData *pMapped = StructCreate(parse_MappedGatewayGameData);
	InvBagIDs bagIDs[] = {InvBagIDs_SuperCritterPets, StaticDefineInt_FastStringToInt(InvBagIDsEnum,"InactivePets",InvBagIDs_None)};
	GatewayGameData *pSrc = GET_REF(ptracker->hGatewayGameData);
	EntitySavedSCPData* pSCPData = scp_GetEntSCPDataStruct(pLoginEntity);
	int i;

	if(pSrc)
	{
		pMapped->pSCA = StructCreate(parse_MappedSCA);

		if(pSrc->pchSaveState)
			pMapped->pSCA->pchSaveState = StructAllocString(pSrc->pchSaveState);

		if(pSrc->pRewardBag)
		{
			pMapped->pSCA->pQueuedRewardBag = StructCreate(parse_MappedRewardBag);

			for(i=0;i<eaSize(&pSrc->pRewardBag->ppIndexedInventorySlots);i++)
			{
				if(pSrc->pRewardBag->ppIndexedInventorySlots[i]->pItem)
				{
					MappedInvSlot *pSlot = NULL;

					pSlot = createMappedInvSlotFromItem(pLoginEntity,psess->lang,pSrc->pRewardBag->ppIndexedInventorySlots[i]->pItem);

					eaPush(&pMapped->pSCA->pQueuedRewardBag->ppSlots,pSlot);
				}
			}

			if(pSrc->pLastRewardBag)
			{
				pMapped->pSCA->pLastRewardBag = StructCreate(parse_MappedRewardBag);

				for(i=0;i<eaSize(&pSrc->pLastRewardBag->ppIndexedInventorySlots);i++)
				{
					MappedInvSlot *pSlot = NULL;

					pSlot = createMappedInvSlotFromItem(pLoginEntity,psess->lang,pSrc->pLastRewardBag->ppIndexedInventorySlots[i]->pItem);

					eaPush(&pMapped->pSCA->pLastRewardBag->ppSlots,pSlot);
				}
			}

			if(pSrc->pLastQueuedRewardBag)
			{
				pMapped->pSCA->pLastQueuedRewardBag = StructCreate(parse_MappedRewardBag);

				for(i=0;i<eaSize(&pSrc->pLastQueuedRewardBag->ppIndexedInventorySlots);i++)
				{
					MappedInvSlot *pSlot = NULL;

					pSlot = createMappedInvSlotFromItem(pLoginEntity,psess->lang,pSrc->pLastQueuedRewardBag->ppIndexedInventorySlots[i]->pItem);

					eaPush(&pMapped->pSCA->pLastQueuedRewardBag->ppSlots,pSlot);
				}
			}
		}

		if(pLoginEntity)
		{
			int iBag;

			for(iBag=0;iBag<ARRAY_SIZE(bagIDs);iBag++)
			{
				BagIterator *pIter = bagiterator_Create();

				bagiterator_SetBagByID(pLoginEntity,bagIDs[iBag], pIter, NULL);

				while(bagiterator_Next(pIter))
				{
					ItemDef *pItemDef = bagiterator_GetDef(pIter);

					if(pItemDef && IS_HANDLE_ACTIVE(pItemDef->hSCPdef))
					{
						Item *pItem = (Item*)bagiterator_GetItem(pIter);
						SuperCritterPetDef *pSCPDef = GET_REF(pItemDef->hSCPdef);
						MappedSCP *pSCP = StructCreate(parse_MappedSCP);
						SCPAltCostumeDef *pCostume = NULL;

						pSCP->uID = pItem->id;
						pSCP->eQuality = item_GetQuality(pItem);
						RefSystem_CopyHandle(REF_HANDLEPTR(pSCP->hItemName),REF_HANDLEPTR(pItemDef->displayNameMsg.hMessage));

						pSCP->uXP = pItem->pSpecialProps->pSuperCritterPet->uXP;
						pSCP->uLevel = pItem->pSpecialProps->pSuperCritterPet->uLevel;
						pSCP->pchName = StructAllocString(pItem->pSpecialProps->pSuperCritterPet->pchName);
						RefSystem_CopyHandle(REF_HANDLEPTR(pSCP->hSCPDef),REF_HANDLEPTR(pItem->pSpecialProps->pSuperCritterPet->hPetDef));

						pSCP->uLastLevel = scp_GetPetLevelAfterTraining(pItem);
						pSCP->uNextLevel = CLAMP(pSCP->uLastLevel + 1,0,scp_MaxLevel(pItem));

						pSCP->uXPNextLevel = scp_GetTotalXPRequiredForLevel(pSCP->uLastLevel+1,pItem);
						pSCP->uXPLastLevel = scp_GetTotalXPRequiredForLevel(pSCP->uLastLevel,pItem);
						
						if(bagIDs[iBag] == InvBagIDs_SuperCritterPets)
							pSCP->uTrainingEndTime = scp_GetActivePetTrainingTimeEnding(pLoginEntity,pIter->i_cur);
						else
							pSCP->uTrainingEndTime = 0;

						pSCP->bTraining = pSCP->uTrainingEndTime > 0;

						pCostume = scp_GetPetCostumeDef(pItem);

						if(pCostume)
						{
							PlayerCostume *pPlayerCostume = GET_REF(pCostume->hCostume);

							pSCP->pchCostume = pPlayerCostume->pcName;
						}

						eaPush(&pMapped->pSCA->ppCritterPets,pSCP);
					}
				}

				bagiterator_Destroy(pIter);
			}
		}
	}

	return pMapped;
}

void DestroyMappedGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker, MappedGatewayGameData *pdata)
{
	StructDestroySafe(parse_MappedGatewayGameData,&pdata);
}

#include "AutoGen/NNOGatewayGameMapping_h_ast.c"
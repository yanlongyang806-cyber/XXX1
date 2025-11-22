/***************************************************************************
 
 
 
 *
 ***************************************************************************/

#include "NNOGatewayMailMapping.h"
#include "AutoGen/NNOGatewayMailMapping_h_ast.h"

#include "Entity.h"
#include "MailCommon.h"
#include "itemCommon.h"
#include "inventoryCommon.h"
#include "Player.h"
#include "Gateway/gslGatewaySession.h"
#include "Gateway/gslGatewayContainerMapping.h"

#include "MailCommon_h_ast.h"

#include "gslMailNPC.h"
#include "gslMail.h"


void SubscribeMailList(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams)
{
	if(psess)
	{
		ContainerMapping *pmapBank = FindContainerMapping(GLOBALTYPE_ENTITYSHAREDBANK);
		char achID[24];
		
		itoa(psess->idAccount, achID, 10);

		session_GetContainerEx(psess, pmapBank, achID, NULL, false);
	}
}

bool IsModifiedMailList(GatewaySession *psess, ContainerTracker *ptracker)
{
	MappedMailList *pList = ptracker->pMapped;
	static EmailV3UIMessage **ppMessages = NULL;
	Entity *pEnt = session_GetLoginEntity(psess);
	char achID[24];

	itoa(psess->idAccount, achID, 10);

	eaClearStruct(&ppMessages,parse_EmailV3UIMessage);

	if(pEnt && pList)
	{
		ContainerTracker *pBankTracker = session_FindContainerTracker(psess,GLOBALTYPE_ENTITYSHAREDBANK,achID);
		Entity *pSharedBank = NULL;

		if(pBankTracker)
			pSharedBank = GET_REF(pBankTracker->hEntity);

		EmailV3_RebuildMailList(pEnt,pSharedBank,&ppMessages,0);

		if(eaSize(&pList->ppUIMessages) != eaSize(&ppMessages))
			return true;
	}
	return false;
}

bool CheckModifiedMailList(GatewaySession *psess, ContainerTracker *ptracker)
{
	return false;
}

bool IsReadyMailList(GatewaySession *psess, ContainerTracker *ptracker)
{
	Entity *pent = session_GetLoginEntity(psess);
	ContainerTracker *pBankTracker = NULL;
	Entity *psharedbank = NULL;
	char achID[24];

	itoa(psess->idAccount, achID, 10);


	pBankTracker = session_FindContainerTracker(psess,GLOBALTYPE_ENTITYSHAREDBANK,achID);
	psharedbank = pBankTracker ? GET_REF(pBankTracker->hEntity) : NULL;

	return pent != NULL && psharedbank != NULL;
}

MappedMailHeader *CreateMappedMailHeader(EmailV3UIMessage *pMessage)
{
	MappedMailHeader *pHeader = StructCreate(parse_MappedMailHeader);

	estrCopy2(&pHeader->estrFrom,pMessage->fromName);
	estrCopy2(&pHeader->estrSubject,pMessage->subject);
	estrCopy2(&pHeader->estrFromHandle,pMessage->fromHandle);

	pHeader->uID = pMessage->uID;
	pHeader->bRead = pMessage->bRead;
	pHeader->sent = pMessage->sent;
	pHeader->eType = pMessage->eSenderType;

	return pHeader;
}

void *CreateMappedMailList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	MappedMailList *pReturn = StructCreate(parse_MappedMailList);
	Entity *pEnt = session_GetLoginEntity(psess);
	ContainerTracker *pBankTracker = NULL;
	Entity* pSharedBank = NULL;
	ContainerTracker **ppMailDetailTrackers = NULL;
	char achID[24];
	int i;

	if(!pEnt)
		return pReturn;

	itoa(psess->idAccount, achID, 10);

	pBankTracker = session_FindContainerTracker(psess,GLOBALTYPE_ENTITYSHAREDBANK,achID);

	if(pBankTracker)
		pSharedBank = GET_REF(pBankTracker->hEntity);

	EmailV3_RebuildMailList(pEnt,pSharedBank,&pReturn->ppUIMessages,0);

	for(i=0;i<eaSize(&psess->ppContainers);i++)
	{
		if(psess->ppContainers[i]->gatewaytype == GW_GLOBALTYPE_MAILDETAIL)
		{
			eaPush(&ppMailDetailTrackers,psess->ppContainers[i]);
		}
	}

	for(i=0;i<eaSize(&pReturn->ppUIMessages);i++)
	{
		EmailV3UIMessage *pMessage = pReturn->ppUIMessages[i];
		int n;

		for(n=0;n<eaSize(&ppMailDetailTrackers);n++)
		{
			U32 MailId;
			EMailV3SenderType eType;

			GetMailKeysFromStringKey(ppMailDetailTrackers[n]->estrID,&eType,&MailId);

			if(pMessage->eSenderType == eType && MailId == pMessage->uID)
			{
				eaRemoveFast(&ppMailDetailTrackers,n);
				break;
			}
		}
		
		eaPush(&pReturn->ppMessages,CreateMappedMailHeader(pMessage));
	}

	for(i=eaSize(&ppMailDetailTrackers)-1;i>=0;i--)
	{
		session_ReleaseContainer(psess,ppMailDetailTrackers[i]->gatewaytype,ppMailDetailTrackers[i]->estrID);
	}

	return pReturn;
}

void DestroyMappedMailList(GatewaySession *psess, ContainerTracker *ptracker, MappedMailList *pvObj)
{
	eaDestroyStruct(&pvObj->ppUIMessages, parse_EmailV3UIMessage);
	StructDestroy(parse_MappedMailList, pvObj);
}

void SubscribeMailDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams)
{
	if(psess)
	{
		ContainerMapping *pmapBank = FindContainerMapping(GLOBALTYPE_ENTITYSHAREDBANK);
		char achID[24];

		itoa(psess->idAccount, achID, 10);

		session_GetContainerEx(psess, pmapBank, achID, NULL, false);
	}
}

bool IsModifiedMailDetail(GatewaySession *psess, ContainerTracker *ptracker)
{
	return false;
}

bool CheckModifiedMailDetail(GatewaySession *psess, ContainerTracker *ptracker)
{
	return false;
}

bool IsReadyMailDetail(GatewaySession *psess, ContainerTracker *ptracker)
{
	Entity *psharedbank = NULL;
	int i;
	Entity *pent = session_GetLoginEntity(psess);

	for(i=0;i<eaSize(&psess->ppContainers);i++)
	{
		if(psess->ppContainers[i]->gatewaytype == GLOBALTYPE_ENTITYSHAREDBANK)
		{
			psharedbank = GET_REF(psess->ppContainers[i]->hEntity);
		}
	}

	return pent != NULL && psharedbank != NULL;
}

MappedMailItem *CreateMappedMailItem(ItemDef *pDef, U32 iCount)
{
	if(pDef)
	{
		MappedMailItem *pItem = StructCreate(parse_MappedMailItem);

		SET_HANDLE_FROM_REFERENT(g_hItemDict,pDef,pItem->hItem);
		pItem->iCount = iCount;

		return pItem;
	}

	return NULL;
}

void GetMailKeysFromStringKey(char *pchStringKey, EMailV3SenderType *eTypeOut, U32 *uiIDOut)
{
	char **ppchIDs = NULL;

	DivideString(pchStringKey,":",&ppchIDs,DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);

	if(eaSize(&ppchIDs) != 2)
	{
		devassert(0);
		return;
	}


	(*eTypeOut) = StaticDefineIntGetInt(EMailV3SenderTypeEnum,ppchIDs[0]);
	(*uiIDOut) = atoi(ppchIDs[1]);

	eaDestroy(&ppchIDs);
}

EmailV3UIMessage *Gateway_FindEmailV3Message(GatewaySession *psess, EMailV3SenderType eType, U32 MailId)
{
	ContainerTracker *pMailList = session_FindContainerTracker(psess,GW_GLOBALTYPE_MAILLIST,"MailList");

	if(pMailList && pMailList->pMapped)
	{
		MappedMailList *pList = pMailList->pMapped;
		int i;

		for(i=0;i<eaSize(&pList->ppUIMessages);i++)
		{
			if(pList->ppUIMessages[i]->uID == MailId && pList->ppUIMessages[i]->eSenderType == eType)
			{
				return pList->ppUIMessages[i];
			}
		}
	}

	return NULL;
}

void *CreateMappedMailDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	
	U32 MailId;
	MappedMailDetail *pReturn = StructCreate(parse_MappedMailDetail);
	EMailV3SenderType eType;

	GetMailKeysFromStringKey(ptracker->estrID,&eType,&MailId);

	if(MailId)
	{
		EmailV3UIMessage *pMessage = Gateway_FindEmailV3Message(psess,eType,MailId);

		if(pMessage)
		{
			MappedMailBody *pBody = StructCreate(parse_MappedMailBody);
			int n;

			pReturn->pHeader = CreateMappedMailHeader(pMessage);
			pReturn->pBody = pBody;

			estrCopy2(&pBody->estrBody,pMessage->body);

			if(pMessage->pNPCMessage)
			{
				for(n=0;n<eaSize(&pMessage->pNPCMessage->ppItemSlot);n++)
				{
					ItemDef *pDef = GET_REF(pMessage->pNPCMessage->ppItemSlot[n]->pItem->hItem);
					if(pDef)
					{
						eaPush(&pBody->ppItems,CreateMappedMailItem(pDef,pMessage->pNPCMessage->ppItemSlot[n]->pItem->count));
					}
				}
			}
			else if(pMessage->pMessage)
			{
				for(n=0;n<eaSize(&pMessage->pMessage->ppItems);n++)
				{
					ItemDef *pDef = SAFE_GET_REF(pMessage->pMessage->ppItems[n],hItem);
					if(pDef)
					{
						eaPush(&pBody->ppItems,CreateMappedMailItem(pDef,pMessage->pMessage->ppItems[n]->count));
					}
				}
			}
		}
	}
	return pReturn;
}

void DestroyMappedMailDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	StructDestroy(parse_MappedMailDetail, pvObj);
}

MappedSharedBank *CreateMappedSharedBank(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	MappedSharedBank *pReturn = StructCreate(parse_MappedSharedBank);

	pReturn->iID = psess->idAccount;
	
	return pReturn;
}

void DestroyMappedSharedBank(GatewaySession *psess, ContainerTracker *ptracker, MappedSharedBank *psharedbank)
{
	StructDestroy(parse_MappedSharedBank,psharedbank);
}

#include "AutoGen/NNOGatewayMailMapping_h_ast.c"
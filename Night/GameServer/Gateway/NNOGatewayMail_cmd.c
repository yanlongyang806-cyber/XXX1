/***************************************************************************
 
 
 
 *
 ***************************************************************************/

#include "Gateway/NNOGatewayMailMapping.h"

#include "Entity.h"
#include "Player.h"
#include "Gateway/gslGatewaySession.h"
#include "gslMail.h"
#include "gslMailNPC.h"
#include "chatCommonStructs.h"
#include "gslFriendsIgnore.h"

static void DeleteMailAndReport(Entity *pEnt, char *pchID, char *pchSpammer)
{
	EMailV3SenderType eType = 0;
	U32 uiID = 0;
	EmailV3UIMessage *pUIMessage = NULL;
	GatewaySession *psess = wgsFindSessionForAccountId(SAFE_MEMBER2(pEnt,pPlayer,accountID));

	if(!psess)
		return;

	GetMailKeysFromStringKey(pchID, &eType, &uiID);

	pUIMessage = Gateway_FindEmailV3Message(psess,eType,uiID);

	if(!pUIMessage)
		return;

	if(pchSpammer)
	{
		gslChat_AddIgnoreSpammer(pEnt,pchSpammer);
	}

	switch (eType)
	{
	case kEmailV3Type_Player:
		EmailV3_DeleteMessage(pEnt, pUIMessage->uID);
		break;
	default:
		gslMailNPC_DeleteMailCompleteLog(pEnt, pUIMessage->uID, pUIMessage->iNPCEMailID, pUIMessage->uLotID);
	}
}

AUTO_COMMAND ACMD_ACCESSLEVEL(0) ACMD_NAME(GatewayMail_Send) ACMD_LIST(gGatewayCmdList);
void GatewayMail_Send(Entity *pEnt, const char *pchTo, const char *pchSubject, const char *pchBody)
{
	EmailV3_SendPlayerEmail(pchSubject,pchBody,pEnt,pchTo,NULL);
}

AUTO_COMMAND ACMD_ACCESSLEVEL(0) ACMD_NAME(GatewayMail_Delete) ACMD_LIST(gGatewayCmdList);
void GatewayMail_Delete(Entity *pEnt, char *pchID)
{
	DeleteMailAndReport(pEnt,pchID,NULL);
}

AUTO_COMMAND ACMD_ACCESSLEVEL(0) ACMD_NAME(GatewayMail_ReportSpam) ACMD_LIST(gGatewayCmdList);
void GatewayMail_ReportSpam(Entity *pEnt, char *pchSpammer, char *pchID)
{
	DeleteMailAndReport(pEnt,pchID,pchSpammer);
}
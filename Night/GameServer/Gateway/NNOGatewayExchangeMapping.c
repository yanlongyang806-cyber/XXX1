
#include "NNOGatewayExchangeMapping.h"
#include "CurrencyExchangeCommon.h"
#include "accountnet.h"
#include "GameAccountData/GameAccountData.h"
#include "AutoGen/CurrencyExchangeCommon_h_ast.h"
#include "AutoGen/NNOGatewayExchangeMapping_h_ast.h"
#include "fastAtoi.h"
#include "gslCurrencyExchange.h"
#include "timing_profiler.h"

#include "Gateway/gslGatewaySession.h"
#include "Gateway/gslGatewayContainerMapping.h"

void SubscribeExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams)
{
	char achID[24];

	itoa(psess->idAccount, achID, 10);

	RefSystem_SetHandleFromString(GlobalTypeToCopyDictionaryName(GLOBALTYPE_CURRENCYEXCHANGE), achID, REF_HANDLEPTR(ptracker->hExchangeAccountData));

	ptracker->phRef = (RefTo *)OFFSET_PTR(ptracker, ptracker->pMapping->offReference);
}

void *CreateMappedExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	MappedExchangeAccountData *pData = StructCreate(parse_MappedExchangeAccountData);
	CurrencyExchangeAccountData *pSrc = GET_REF(ptracker->hExchangeAccountData);
	AccountProxyKeyValueInfoContainer *pPair = NULL;
	GameAccountData *pGameAccountData = GET_REF(psess->hGameAccountData);
	CurrencyExchangeGlobalUIData *pGlobalUIData = gslCurrencyExchange_GetGlobalUIData();
	
	int i;

	PERFINFO_AUTO_START_FUNC();

	pData->uForSaleEscrow = pSrc->forSaleEscrowTC;
	pData->uReadyToClaimEscrow = pSrc->readyToClaimEscrowTC;

	pPair = eaIndexedGetUsingString(&pGameAccountData->eaAccountKeyValues, CurrencyExchangeConfig_GetMtcReadyToClaimEscrowAccountKey());

	if(pPair && pPair->pValue)
	{
		pData->uReadyToClaimMTC = atoi64(pPair->pValue);
	}
	else
	{
		pData->uReadyToClaimMTC = 0;
	}

	for(i=0;i<eaSize(&pSrc->logEntries);i++)
	{
		MappedExchangeLogEntry *pMappedLog = StructCreate(parse_MappedExchangeLogEntry);
		CurrencyExchangeLogEntry *pSrcLog = pSrc->logEntries[i];

		pMappedLog->logType = pSrcLog->logType;
		pMappedLog->orderID = pSrcLog->orderID;
		pMappedLog->orderType = pSrcLog->orderType;
		pMappedLog->quantityMTC = pSrcLog->quantityMTC;
		pMappedLog->quantityTC = pSrcLog->quantityTC;
		pMappedLog->date = pSrcLog->time;
		pMappedLog->characterName = pSrcLog->characterName;

		eaPush(&pData->ppLogEntries,pMappedLog);
	}

	for(i=0;i<eaSize(&pSrc->openOrders);i++)
	{
		CurrencyExchangeOpenOrder *pSrcOrder = pSrc->openOrders[i];

		if(pSrcOrder && pSrcOrder->orderType != OrderType_None) //These entries are allowed to be null
		{
			MappedExchangeOpenOrder *pMappedOrder = StructCreate(parse_MappedExchangeOpenOrder);

			pMappedOrder->consumedMTC = pSrcOrder->consumedMTC;
			pMappedOrder->consumedTC = pSrcOrder->consumedTC;
			pMappedOrder->listingTime = pSrcOrder->listingTime;
			pMappedOrder->orderID = pSrcOrder->orderID;
			pMappedOrder->orderType = pSrcOrder->orderType;
			pMappedOrder->price = pSrcOrder->price;
			pMappedOrder->quantity = pSrcOrder->quantity;
			pMappedOrder->totalTC = pSrcOrder->quantity * pSrcOrder->price;

			if(pMappedOrder->orderType == OrderType_Sell)
				pMappedOrder->quantityRemaining = pSrcOrder->quantity - pSrcOrder->consumedMTC;
			else if(pMappedOrder->orderType == OrderType_Buy)
				pMappedOrder->quantityRemaining = pSrcOrder->quantity - (pSrcOrder->consumedTC / pSrcOrder->price);
			
			eaPush(&pData->ppOpenOrders,pMappedOrder);
		}
	}

	if(pGlobalUIData)
		pData->pGlobalData = StructClone(parse_CurrencyExchangeGlobalUIData,pGlobalUIData);

	PERFINFO_AUTO_STOP();

	return pData;
}

void DestroyMappedExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	StructDestroySafe(parse_MappedExchangeAccountData, &(MappedExchangeAccountData*)pvObj);
}

#include "AutoGen/NNOGatewayExchangeMapping_h_ast.c"
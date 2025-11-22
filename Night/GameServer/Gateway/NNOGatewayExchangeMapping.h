/***************************************************************************
 
 
 
 *
 ***************************************************************************/

#ifndef GSLGATEWAYEXCHANGEMAPPING_H__
#define GSLGATEWAYEXCHANGEMAPPING_H__
#pragma once
GCC_SYSTEM

#include "GlobalTypes.h"
#include "referencesystem.h"


typedef struct GatewaySession GatewaySession;
typedef struct ContainerTracker ContainerTracker;
typedef struct CurrencyExchangeAccountData CurrencyExchangeAccountData;
typedef struct CurrencyExchangeGlobalUIData CurrencyExchangeGlobalUIData;

typedef enum CurrencyExchangeOrderType CurrencyExchangeOrderType;
typedef enum CurrencyExchangeOperationType CurrencyExchangeOperationType;

AUTO_STRUCT;
typedef struct MappedExchangeOpenOrder
{
	U32 orderID;

	CurrencyExchangeOrderType orderType;

	U32 quantity;

	U32 quantityRemaining;

	U32 totalTC;

	U32 price;

	U32 listingTime;

	U32 consumedTC;

	U32 consumedMTC;
}MappedExchangeOpenOrder;

AUTO_STRUCT;
typedef struct MappedExchangeLogEntry
{
	CurrencyExchangeOperationType logType;

	// The type of order. Used by CreateOrder, FulfillOrder, WithdrawOrder and ExpireOrder.
	CurrencyExchangeOrderType orderType; 

	// The ID of the order. Used by CreateOrder, FulfillOrder, WithdrawOrder and ExpireOrder.
	U32 orderID;                        

	// The quantity of MTC involved in the logged operation. Used by CreateOrder, FulfillOrder, WithdrawOrder, ExpireOrder and ClaimMTC.
	U32 quantityMTC;                    

	// The quantity of TC involved in the logged operation. Used by CreateOrder, FulfillOrder, WithdrawOrder, ExpireOrder and ClaimTC.
	U32 quantityTC;                       

	// The time that the logged operation occurred. Used by all log types.
	U32 date;										AST(FORMATSTRING(JSON_SECS_TO_RFC822=1))                        

	// The name of the character that performed the operation, or "website"
	const char *characterName;						AST(POOL_STRING)
}MappedExchangeLogEntry;

AUTO_STRUCT;
typedef struct MappedExchangeAccountData
{
	U32 uForSaleEscrow;								AST(NAME(ForSaleEscrow) DEFAULT(-1))

	U32 uReadyToClaimEscrow;						AST(NAME(ReadyToClaimEscrow) DEFAULT(-1))

	U32 uReadyToClaimMTC;							AST(NAME(ReadyToClaimMTC) DEFAULT(-1))

	MappedExchangeOpenOrder **ppOpenOrders;			AST(NAME(OpenOrders))

	MappedExchangeLogEntry **ppLogEntries;			AST(NAME(LogEntries))

	CurrencyExchangeGlobalUIData *pGlobalData;		AST(NAME(GlobalData))
}MappedExchangeAccountData;

void SubscribeExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams);
bool IsModifiedExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker);
bool CheckModifiedExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker);
bool IsReadyExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker);
void *CreateMappedExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedExchangeAccountData(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);


#endif
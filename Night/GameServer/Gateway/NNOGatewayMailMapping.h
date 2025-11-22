/***************************************************************************
 
 
 
 *
 ***************************************************************************/

#ifndef GSLGATEWAYMAILMAPPING_H__
#define GSLGATEWAYMAILMAPPING_H__
#pragma once
GCC_SYSTEM

#include "GlobalTypes.h"
#include "referencesystem.h"

typedef struct GatewaySession GatewaySession;
typedef struct ContainerTracker ContainerTracker;
typedef struct EmailV3UIMessage EmailV3UIMessage;
typedef struct Entity Entity;
typedef struct ItemDef ItemDef;

typedef enum EMailV3SenderType EMailV3SenderType;

AUTO_STRUCT;
typedef struct MappedMailHeader
{
	char *estrFrom;			AST(ESTRING NAME(From))
	char *estrFromHandle;	AST(ESTRING NAME(FromHandle))
	char *estrSubject;		AST(ESTRING NAME(Subject))
	U32 bRead;				AST(NAME(Read) DEFAULT(-1))
	U32 uID;				AST(NAME(ID))
	U32 sent;				AST(NAME(Sent) FORMATSTRING(JSON_SECS_TO_RFC822=1))
	EMailV3SenderType eType;		AST(NAME(Type) SUBTABLE(EMailV3SenderTypeEnum) DEFAULT(-1))
}MappedMailHeader;

AUTO_STRUCT;
typedef struct MappedMailList
{
	EmailV3UIMessage **ppUIMessages;	NO_AST
	MappedMailHeader **ppMessages;
}MappedMailList;

AUTO_STRUCT;
typedef struct MappedMailItem
{
	REF_TO(ItemDef) hItem;		AST(NAME(ItemDef))
	U32 iCount;					AST(NAME(Count))
}MappedMailItem;

AUTO_STRUCT;
typedef struct MappedMailBody
{
	char *estrBody;				AST(ESTRING NAME(Body))
	MappedMailItem **ppItems;
}MappedMailBody;

AUTO_STRUCT;
typedef struct MappedMailDetail
{
	MappedMailHeader *pHeader;
	MappedMailBody *pBody;
	
}MappedMailDetail;

AUTO_STRUCT;
typedef struct MappedSharedBank
{
	int iID;
}MappedSharedBank;

void SubscribeMailList(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams);
bool IsModifiedMailList(GatewaySession *psess, ContainerTracker *ptracker);
bool CheckModifiedMailList(GatewaySession *psess, ContainerTracker *ptracker);
bool IsReadyMailList(GatewaySession *psess, ContainerTracker *ptracker);
void *CreateMappedMailList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedMailList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);

void SubscribeMailDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams);
bool IsModifiedMailDetail(GatewaySession *psess, ContainerTracker *ptracker);
bool CheckModifiedMailDetail(GatewaySession *psess, ContainerTracker *ptracker);
bool IsReadyMailDetail(GatewaySession *psess, ContainerTracker *ptracker);
void *CreateMappedMailDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedMailDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);

MappedSharedBank *CreateMappedSharedBank(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedSharedBank(GatewaySession *psess, ContainerTracker *ptracker, MappedSharedBank *psharedbank);

void GetMailKeysFromStringKey(char *pchStringKey, EMailV3SenderType *eTypeOut, U32 *uiIDOut);
EmailV3UIMessage *Gateway_FindEmailV3Message(GatewaySession *psess, EMailV3SenderType eType, U32 MailId);

#endif
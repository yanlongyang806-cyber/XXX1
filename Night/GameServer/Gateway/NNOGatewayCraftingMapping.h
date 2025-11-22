/***************************************************************************
 
 
 
 *
 ***************************************************************************/

#ifndef GSLGATEWAYCRAFTINGMAPPING_H__
#define GSLGATEWAYCRAFTINGMAPPING_H__
#pragma once
GCC_SYSTEM

#include "GlobalTypes.h"
#include "referencesystem.h"
#include "ItemAssignments.h"

typedef struct GatewaySession GatewaySession;
typedef struct ContainerTracker ContainerTracker;
typedef struct ItemAssignmentDef ItemAssignmentDef;
typedef struct ItemAssignmentSlotUI ItemAssignmentSlotUI;
typedef struct ItemAssignmentOutcomeRewardRequest ItemAssignmentOutcomeRewardRequest;
typedef struct ItemDef ItemDef;
typedef struct InvRewardRequest InvRewardRequest;
typedef struct ItemAssignmentOutcomeUI ItemAssignmentOutcomeUI;
typedef U32 ContainerID;

AUTO_STRUCT;
typedef struct MappedCraftingItem
{
	REF_TO(ItemDef) hDef;
	int iCount;
	int iRequired;
	bool bFillsRequirements;	AST(DEFAULT(-1))
	U64 uID;
}MappedCraftingItem;

AUTO_STRUCT;
typedef struct MappedCraftingSlot
{
	char *pchIcon;						AST(ESTRING)
	bool bFillsRequirements;			AST(DEFAULT(-1))
	char *pchCategories;				AST(ESTRING)
}MappedCraftingSlot;

AUTO_STRUCT;
typedef struct MappedCraftingEntry
{
	REF_TO(ItemAssignmentDef) hDef;
	char *pchFailedRequirementsReasons;	AST(ESTRING)
	char *pchHeader;					AST(ESTRING)
	MappedCraftingSlot **ppRequired;
	MappedCraftingItem **ppConsumables;
	MappedCraftingItem **ppRewards;
	bool bIsHeader;							AST(DEFAULT(-1))
	bool bFailsLevelRequirements;			AST(DEFAULT(-1))
	bool bFailsLevelRequirementsFilter;		AST(DEFAULT(-1))
	bool bFailsResourcesRequirements;		AST(DEFAULT(-1))
	U32 uNextUpdateTime;					AST(FORMATSTRING(JSON_SECS_TO_RFC822=1))
}MappedCraftingEntry;

AUTO_STRUCT;
typedef struct MappedCraftingList
{
	DisplayMessage DisplayName;			AST(NAME(DisplayName) STRUCT(parse_DisplayMessage))  
	MappedCraftingEntry **ppEntries;	AST(NAME(Entries))
	U32	uLastPersonalUpdateTime;		NO_AST
	U32 uLastUpdateTime;				NO_AST

	const char *pchAvailabilityEvent;	AST(NAME(AvailabilityEvent) POOL_STRING)
	U32 uAvailabilityTime;				AST(NAME(AvailabilityTime) FORMATSTRING(JSON_SECS_TO_RFC822=1))

}MappedCraftingList;

AUTO_STRUCT;
typedef struct ItemAssignmentGatewaySlotData
{
	ItemAssignmentSlotUI *pSlot;
	MappedCraftingItem **ppItems;
	int index;
}ItemAssignmentGatewaySlotData;

AUTO_STRUCT;
typedef struct MappedCraftingRewardRank
{
	InvRewardRequest* pData;
	ItemAssignmentOutcomeUI *pChanceData;
}MappedCraftingRewardRank;

AUTO_STRUCT;
typedef struct MappedCraftingDetail
{
	REF_TO(ItemAssignmentDef) hDef;

	MappedCraftingItem **ppConsumables;
	ItemAssignmentGatewaySlotData **ppRequired;
	ItemAssignmentGatewaySlotData **ppOptional;
	MappedCraftingRewardRank **ppRewardOutcomes;

	char *pchFailedRequirementsReasons;	AST(ESTRING)
	bool bFailsRequirements;			AST(DEFAULT(-1))
	ItemAssignmentFailsRequiresReason eFailsRequiresReasons;
	U32 bfInvalidSlots[1];				AST(NAME(SlotRequirement))
	U32 uDuration;						AST(NAME(Duration))
}MappedCraftingDetail;

AUTO_STRUCT;
typedef struct ParamsCraftingList
{
	int iEmpty;
} ParamsCraftingList;

AUTO_STRUCT;
typedef struct ParamsCraftingDetail
{
	int iEmpty;
}ParamsCraftingDetail;


void SubscribeCraftingList(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams);
bool IsModifiedCraftingList(GatewaySession *psess, ContainerTracker *ptracker);
bool CheckModifiedCraftingList(GatewaySession *psess, ContainerTracker *ptracker);
bool IsReadyCraftingList(GatewaySession *psess, ContainerTracker *ptracker);
void *CreateMappedCraftingList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedCraftingList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);

void SubscribeCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams);
bool IsModifiedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker);
bool CheckModifiedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker);
bool IsReadyCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker);
void *CreateMappedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);

bool CraftingMappingItemDefCheck(ItemDef *pDef);

#endif
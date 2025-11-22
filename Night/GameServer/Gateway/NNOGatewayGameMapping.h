#ifndef NNOGATEWAYGAMEMAPPING_H__
#define NNOGATEWAYGAMEMAPPING_H__

#include "referencesystem.h"

typedef struct GatewaySession GatewaySession;
typedef struct ContainerTracker ContainerTracker;
typedef struct ItemDef ItemDef;
typedef struct SuperCritterPetDef SuperCritterPetDef;
typedef struct Message Message;
typedef struct MappedInvSlot MappedInvSlot;
typedef U32 ItemQuality;

AUTO_STRUCT;
typedef struct MappedRewardBag
{
	MappedInvSlot **ppSlots;					AST(NAME(Rewards))
}MappedRewardBag;

AUTO_STRUCT;
typedef struct MappedSCP
{
	REF_TO(Message) hItemName;				AST(NAME(ItemName))
	U64 uID;								AST(NAME(ID))//Item ID
	ItemQuality eQuality;					AST(NAME(Quality))

	const char* pchName;					AST(NAME(Name))
	U32 uXP;								AST(NAME(XP))
	
	U32 uLevel;								AST(NAME(Level))
	U32 uLastLevel;							AST(NAME(LastLevel))
	U32 uNextLevel;							AST(NAME(NextLevel))

	U32 uXPNextLevel;						AST(NAME(NextXP))
	U32 uXPLastLevel;						AST(NAME(LastXP))
	const char* pchCostume;					AST(POOL_STRING NAME(Costume))
	REF_TO(SuperCritterPetDef) hSCPDef;		AST(NAME(SCPDef))

	U32 uTrainingEndTime;					AST(NAME(TrainingEndTime) FORMATSTRING(JSON_SECS_TO_RFC822=1))
	bool bTraining;							AST(NAME(isTraining))
}MappedSCP;

AUTO_STRUCT;
typedef struct MappedSCA
{
	MappedSCP **ppCritterPets;				AST(NAME(Companions))
	MappedRewardBag *pQueuedRewardBag;		AST(NAME(QueuedRewardBag))
	MappedRewardBag *pLastQueuedRewardBag;	AST(NAME(LastQueuedRewardBag))
	MappedRewardBag *pLastRewardBag;		AST(NAME(LastRewardBag))
	const char *pchSaveState;				AST(NAME(SaveState))
}MappedSCA;

	AUTO_STRUCT;
typedef struct MappedGatewayGameData
{
	MappedSCA *pSCA;						AST(NAME(SCA))
}MappedGatewayGameData;

void SubscribeGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams);
bool IsReadyGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker);
void *CreateMappedGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj);
void DestroyMappedGatewayGameData(GatewaySession *psess, ContainerTracker *ptracker, MappedGatewayGameData *pdata);

#endif
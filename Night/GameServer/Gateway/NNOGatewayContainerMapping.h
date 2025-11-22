/***************************************************************************
 
 
 
 *
 ***************************************************************************/
#include "GlobalTypes.h"

typedef struct MappedEntity MappedEntity;
typedef struct MappedGuild MappedGuild;
typedef struct ContainerTracker ContainerTracker;

// From NNOGatewayMappedEntity.c
extern MappedEntity *CreateMappedEntity(GatewaySession *psess, ContainerTracker *ptracker, MappedEntity *pent);
extern void DestroyMappedEntity(GatewaySession *psess, ContainerTracker *ptracker, MappedEntity *pent);
extern void MappedEntityInit(void);

// From NNOGatewayMappedGuild.c
extern MappedGuild *CreateMappedGuild(GatewaySession *psess, ContainerTracker *ptracker, MappedGuild *pent);
extern void DestroyMappedGuild(GatewaySession *psess, ContainerTracker *ptracker, MappedGuild *pent);

// End of File

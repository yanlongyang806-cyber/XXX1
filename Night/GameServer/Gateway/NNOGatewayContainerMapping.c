/***************************************************************************
 
 
 
 *
 ***************************************************************************/
#include "GlobalTypes.h"

#include "Gateway/gslGatewayContainerMapping.h"
#include "Gateway/gslGatewaySession.h"
#include "Gateway/gslGatewayAuction.h"
#include "Gateway/gslGatewayCStore.h"
#include "Gateway/gslGatewayVendor.h"
#include "NNOGatewayContainerMapping.h"
#include "NNOGatewayCraftingMapping.h"
#include "NNOGatewayMailMapping.h"
#include "Gateway/NNOGatewayExchangeMapping.h"
#include "Gateway/NNOGatewayGameMapping.h"

#include "NNOGatewayCraftingMapping_h_ast.h"
#include "NNOGatewayMailMapping_h_ast.h"
#include "NNOGatewayExchangeMapping_h_ast.h"

#include "Entity.h"
#include "Player.h"
#include "gslItemAssignments.h"
#include "gslSuperCritterPet.h"

extern ParseTable parse_MappedLoginEntity[];

extern ParseTable parse_Entity[];
extern ParseTable parse_MappedEntity[];

extern ParseTable parse_Guild[];
extern ParseTable parse_MappedGuild[];

extern ParseTable parse_AuctionSearch[];
extern ParseTable parse_ParamsAuctionSearch[];
extern ParseTable parse_MappedAuctionSearch[];
extern ParseTable parse_ItemAssignmentList[];
extern ParseTable parse_ParamsCraftingList[];
extern ParseTable parse_ParamsCarftingDetail[];
extern ParseTable parse_MappedMailList[];
extern ParseTable parse_MappedMailDetail[];
extern ParseTable parse_MappedCStore[];
extern ParseTable parse_MappedVendor[];
extern ParseTable parse_ExchangeAccountData[];
extern ParseTable parse_MappedGatewayGameData[];

ContainerMapping *OVERRIDE_LATELINK_GetContainerMappings(void)
{
	static GatewayGlobalType types[GATEWAY_CONTAINER_MAX_DEPS] = {GW_GLOBALTYPE_CRAFTING_LIST, GW_GLOBALTYPE_CRAFTING_DETAIL, GLOBALTYPE_GATEWAYGAMEDATA};
	static ContainerMapping s_aContainerMappings[] =
	{
		CONTAINER_MAPPING_LOGIN_ENTITY(types),

		CONTAINER_MAPPING_SUBSCRIBE("Entity", GLOBALTYPE_ENTITYPLAYER, Entity),
		CONTAINER_MAPPING_SUBSCRIBE("Pet", GLOBALTYPE_ENTITYSAVEDPET, Entity),
		CONTAINER_MAPPING_SUBSCRIBE("Guild", GLOBALTYPE_GUILD, Guild),
		{ "EntitySharedBank", GLOBALTYPE_ENTITYSHAREDBANK, GLOBALTYPE_ENTITYSHAREDBANK, NULL, parse_MappedSharedBank, false, SubscribeDBContainer, NULL, NULL, IsReadyDBContainer, CreateMappedSharedBank, DestroyMappedSharedBank, offsetof(ContainerTracker, hEntity)  },
		CONTAINER_MAPPING("AuctionSearch", GW_GLOBALTYPE_AUCTION_SEARCH, AuctionSearch),
		CONTAINER_MAPPING_NOPARAMS("CraftingList", GW_GLOBALTYPE_CRAFTING_LIST, CraftingList),
		CONTAINER_MAPPING_NOPARAMS("CraftingDetail", GW_GLOBALTYPE_CRAFTING_DETAIL, CraftingDetail),
		CONTAINER_MAPPING_NOPARAMS("MailList", GW_GLOBALTYPE_MAILLIST, MailList),
		CONTAINER_MAPPING_NOPARAMS("MailDetail", GW_GLOBALTYPE_MAILDETAIL, MailDetail),
		CONTAINER_MAPPING_NOPARAMS("CStore", GW_GLOBALTYPE_CSTORE, CStore),
		CONTAINER_MAPPING_NOPARAMS("Vendor", GW_GLOBALTYPE_VENDOR, Vendor),
		{ "ExchangeAccountData", GLOBALTYPE_CURRENCYEXCHANGE, GLOBALTYPE_CURRENCYEXCHANGE, NULL, parse_MappedExchangeAccountData, false, SubscribeExchangeAccountData, NULL, NULL, IsReadyDBContainer, CreateMappedExchangeAccountData, DestroyMappedExchangeAccountData, offsetof(ContainerTracker, hExchangeAccountData)  },
		{ "GatewayGameData", GLOBALTYPE_GATEWAYGAMEDATA, GLOBALTYPE_GATEWAYGAMEDATA, NULL, parse_MappedGatewayGameData, false, SubscribeGatewayGameData, NULL, NULL, IsReadyGatewayGameData, CreateMappedGatewayGameData, DestroyMappedGatewayGameData, offsetof(ContainerTracker, hGatewayGameData)  },

		// { pchName, gatewaytype, globaltype, tpiParams, tpiDest, bAlwaysFullUpdate, Subscribe, IsModified, CheckModified, IsReady, CreateMapped, DestroyMapped, offset to href},

		CONTAINER_MAPPING_END
	};

	return s_aContainerMappings;
}

void wgsUpdateItemAssignments(Entity *pent)
{
	ItemAssignmentPersistedData *pdata = SAFE_MEMBER2(pent,pPlayer,pItemAssignmentPersistedData);
	int i;

	PERFINFO_AUTO_START_FUNC();
	
	pent->iPartitionIdx_UseAccessor = 1;

	if(pdata)
	{
		for (i = eaSize(&pdata->eaActiveAssignments)-1; i >= 0; i--)
		{
			ItemAssignment* pAssignment = pdata->eaActiveAssignments[i];

			gslItemAssignments_CompleteAssignment(pent, pAssignment, NULL, false, false);
		}

		PERFINFO_AUTO_START("gslUpdatePersistedItemAssignmentList", 1);
		gslUpdatePersistedItemAssignmentList(pent, NULL);
		PERFINFO_AUTO_STOP();

		gslItemAssignments_UpdatePlayerAssignments(pent);
	}

	PERFINFO_AUTO_STOP();
}


void OVERRIDE_LATELINK_GatewayEntityTick(Entity *pEnt)
{
	PERFINFO_AUTO_START_FUNC();
	if(pEnt)
	{
		wgsUpdateItemAssignments(pEnt);
		scp_CheckForFinishedTraining(pEnt);
	}
	PERFINFO_AUTO_STOP();
}

void OVERRIDE_LATELINK_ContainerMappingInit(void)
{
	MappedEntityInit();
}

// End of File

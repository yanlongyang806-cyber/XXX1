/***************************************************************************
 
 
 
 *
 ***************************************************************************/
#include "stdtypes.h"
#include "referencesystem.h"
#include "textparserJSON.h"
#include "Message.h"
#include "Powers.h"
#include "mission_common.h"
#include "CharacterClass.h"
#include "StringCache.h"
#include "ItemAssignments.h"
#include "ItemAssignments_h_ast.h"
#include "ItemCommon.h"
#include "itemCommon_h_ast.h"
#include "ItemEnums.h"
#include "itemEnums_h_ast.h"
#include "MicroTransactions.h"
#include "AuctionLot.h"
#include "AutoGen/AuctionLot_h_ast.h"

#include "Gateway/gslGatewayStructMapping.h"  // for structmap_Message
#include "gslGatewayStructMapping_c_ast.h"    // for parse_MappedMessage
#include "itemcommon_h_ast.h"                 // for parse_ItemSortTypes
#include "MicroTransactions_h_ast.h"          // for parse_MicroTransactionDef
#include "powers_h_ast.h"

#include "NNOGatewayStructMapping_c_ast.h"

/////////////////////////////////////////////////////////////////////////////

AUTO_STRUCT;
typedef struct MappedPowerDef
{
	const char *pchName;					AST( NAME(Name) POOL_STRING )

	REF_TO(Message) hDisplayName;			AST( NAME(DisplayName) REFDICT(Message) )
	REF_TO(Message) hDescription;			AST( NAME(Description) REFDICT(Message) )
	REF_TO(Message) hDescriptionLong;		AST( NAME(DescriptionLong) REFDICT(Message) )
	REF_TO(Message) hAutoDesc;				AST( NAME(AutoDesc) REFDICT(Message) )

	const char *pchIconName;				AST( NAME(Icon) POOL_STRING)

} MappedPowerDef;

void *structmap_PowerDef(StructMapping *pmap, void *pvSrc, Language lang)
{
	PowerDef *pPowerDef = (PowerDef *)pvSrc;
	MappedPowerDef *pdest;

	StructAllocIfNullVoid(parse_MappedPowerDef, pmap->pvScratch);
	pdest = (MappedPowerDef *)pmap->pvScratch;

	pdest->pchName = allocAddString(pPowerDef->pchName);

	COPY_HANDLE(pdest->hDisplayName, pPowerDef->msgDisplayName.hMessage);
	COPY_HANDLE(pdest->hDescription, pPowerDef->msgDescription.hMessage);
	COPY_HANDLE(pdest->hDescriptionLong, pPowerDef->msgDescriptionLong.hMessage);
	COPY_HANDLE(pdest->hAutoDesc, pPowerDef->msgAutoDesc.hMessage);

	pdest->pchIconName = allocAddString(pPowerDef->pchIconName);

	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

AUTO_STRUCT;
typedef struct MappedMissionDef
{
	const char *pchName;					AST( NAME(Name) POOL_STRING )

	REF_TO(Message) hDisplayName;			AST( NAME(DisplayName) REFDICT(Message) )
	REF_TO(Message) hDisplayLong;			AST( NAME(DisplayLong) REFDICT(Message) )
	REF_TO(Message) hSummary;				AST( NAME(Summary) REFDICT(Message) )

	const char *pchIconName;				AST( NAME(Icon) POOL_STRING)
	U32 iPerkPoints;                        AST( NAME(PerkPoints) )

} MappedMissionDef;

void *structmap_MissionDef(StructMapping *pmap, void *pvSrc, Language lang)
{
	MissionDef *pMissionDef = (MissionDef *)pvSrc;
	MappedMissionDef *pdest;

	StructAllocIfNullVoid(parse_MappedMissionDef, pmap->pvScratch);
	pdest = (MappedMissionDef *)pmap->pvScratch;

	pdest->pchName = allocAddString(pMissionDef->name);

	COPY_HANDLE(pdest->hDisplayName, pMissionDef->displayNameMsg.hMessage);
	COPY_HANDLE(pdest->hDisplayLong, pMissionDef->uiStringMsg.hMessage);
	COPY_HANDLE(pdest->hSummary, pMissionDef->summaryMsg.hMessage);

	pdest->pchIconName = allocAddString(pMissionDef->pchIconName);

	pdest->iPerkPoints = pMissionDef->iPerkPoints;

	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

AUTO_STRUCT;
typedef struct MappedCharacterClass
{
	const char *pchName;					AST( NAME(Name) POOL_STRING )

	REF_TO(Message) hDisplayName;			AST( NAME(DisplayName) REFDICT(Message) )

} MappedCharacterClass;

void *structmap_CharacterClass(StructMapping *pmap, void *pvSrc, Language lang)
{
	CharacterClass *pCharacterClass = (CharacterClass *)pvSrc;
	MappedCharacterClass *pdest;

	StructAllocIfNullVoid(parse_CharacterClass, pmap->pvScratch);
	pdest = (MappedCharacterClass *)pmap->pvScratch;

	pdest->pchName = allocAddString(pCharacterClass->pchName);
	COPY_HANDLE(pdest->hDisplayName, pCharacterClass->msgDisplayName.hMessage);

	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

AUTO_STRUCT;
typedef struct MappedItemDef
{
	const char *pchName;				AST( NAME(Name) POOL_STRING )

	REF_TO(Message) hDisplayName;		AST( NAME(displayNameMsg) REFDICT(Message) )
	REF_TO(Message) hDescription;		AST( NAME(descriptionMsg) REFDICT(Message) )
	REF_TO(Message) hDescriptionShort;	AST( NAME(descShortMsg) REFDICT(Message) )
	REF_TO(Message) hAutoDesc;			AST( NAME(msgAutoDesc) REFDICT(Message) )

	const char *pchIconName;			AST( NAME(Icon) POOL_STRING )
	ItemType eType;						AST( NAME(Type) )
	ItemQuality Quality;				AST( NAME(Quality))

	EARRAY_OF(ItemPowerDefRef) eaItemPowerDefRefs;	AST( NAME(ItemPowerDefRefs) )

} MappedItemDef;

void *structmap_ItemDef(StructMapping *pmap, void *pvSrc, Language lang)
{
	ItemDef *pItemDef = (ItemDef *)pvSrc;
	MappedItemDef *pdest;

	StructAllocIfNullVoid(parse_MappedItemDef, pmap->pvScratch);
	pdest = (MappedItemDef *)pmap->pvScratch;

	pdest->pchName = allocAddString(pItemDef->pchName);

	COPY_HANDLE(pdest->hDisplayName, pItemDef->displayNameMsg.hMessage);
	COPY_HANDLE(pdest->hDescription, pItemDef->descriptionMsg.hMessage);
	COPY_HANDLE(pdest->hDescriptionShort, pItemDef->descShortMsg.hMessage);
	COPY_HANDLE(pdest->hAutoDesc, pItemDef->msgAutoDesc.hMessage);

	pdest->pchIconName = allocAddString(pItemDef->pchIconName);
	pdest->eType = pItemDef->eType;
	pdest->Quality = pItemDef->Quality;

	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

AUTO_STRUCT;
typedef struct MappedItemAssignmentDef
{
	const char *pchName;			AST( NAME(Name)  POOL_STRING )

	const char* pchIconName;		AST( NAME(Icon) POOL_STRING )
	S32 iRequiredRank;				AST( NAME(RequiredRank) )
	U32 uDuration;					AST( NAME(Duration) )
	REF_TO(Message) hDisplayName;	AST( NAME(DisplayName) REFDICT(Message) )  
	REF_TO(Message) hDescription;	AST( NAME(Description) REFDICT(Message) )  

} MappedItemAssignmentDef;

void *structmap_ItemAssignmentDef(StructMapping *pmap, void *pvSrc, Language lang)
{
	ItemAssignmentDef *pdef = (ItemAssignmentDef *)pvSrc;
	MappedItemAssignmentDef *pdest;

	StructAllocIfNullVoid(parse_MappedItemAssignmentDef, pmap->pvScratch);
	pdest = (MappedItemAssignmentDef *)pmap->pvScratch;

	pdest->pchName = allocAddString(pdef->pchName);
	pdest->pchIconName = allocAddString(pdef->pchIconName);
	pdest->iRequiredRank = SAFE_MEMBER2(pdef, pRequirements, iRequiredNumericValue);
	pdest->uDuration = pdef->uDuration;
	COPY_HANDLE(pdest->hDisplayName, pdef->msgDisplayName.hMessage);
	COPY_HANDLE(pdest->hDescription, pdef->msgDescription.hMessage);

	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

typedef struct MappedSortType MappedSortType;

AUTO_STRUCT;
typedef struct MappedSortType
{
	int iIndex;									AST(NAME(index))
	REF_TO(Message) hDisplayName;				AST(STRUCT(parse_DisplayMessage) NAME(displayname))
	EARRAY_OF(MappedSortType) subSortTypes;		AST(NAME(subsorttypes))
	int itemSortType;							AST(NAME(type))
	const char *pchItemCategory;				AST(NAME(Category) POOL_STRING)
}MappedSortType;

AUTO_STRUCT;
typedef struct MappedAuctionDuration
{
	REF_TO(Message) hDisplayName;				AST(STRUCT(parse_DisplayMessage) NAME(displayname))
	int iDuration;								AST(NAME(duration))
	bool bDefaultDuration;						AST(NAME(defaultduration))
}MappedAuctionDuration;

AUTO_STRUCT;
typedef struct MappedAuctionSettings
{			
	EARRAY_OF(MappedSortType) SortTypes;	AST(NAME(sorttypes))
	EARRAY_OF(MappedAuctionDuration) AuctionDurations; AST(NAME(auctiondurations))
}MappedAuctionSettings;

static MappedSortType *createMappedSortType(ItemSortTypeCategory *pCategory)
{
	MappedSortType *pReturn = StructCreate(parse_MappedSortType);
	int i;

	COPY_HANDLE(pReturn->hDisplayName,pCategory->hNameMsg);
	pReturn->pchItemCategory = pCategory->pchName;

	for(i=0;i<ea32Size(&pCategory->eaiItemSortTypes);i++)
	{
		ItemSortType *pType = eaIndexedGetUsingInt(&gSortTypes.ppIndexedItemSortTypes,pCategory->eaiItemSortTypes[i]);
		
		if(pType)
		{
			MappedSortType *pSubType = StructCreate(parse_MappedSortType);

			COPY_HANDLE(pSubType->hDisplayName,pType->hNameMsg);
			pSubType->itemSortType = pType->iSortID;
			pSubType->pchItemCategory = pCategory->pchName;
			eaPush(&pReturn->subSortTypes,pSubType);
		}
	}

	return pReturn;
}

static MappedAuctionDuration *createMappedAuctionDuration(AuctionDurationOption *pSrc)
{
	MappedAuctionDuration *pReturn = StructCreate(parse_MappedAuctionDuration);

	pReturn->iDuration = pSrc->iDuration;
	pReturn->bDefaultDuration = pSrc->bUIDefaultDuration;
	COPY_HANDLE(pReturn->hDisplayName,pSrc->msgDisplayName.hMessage);

	return pReturn;
}

void *structmap_AuctionSettings(StructMapping *pmap, void *pvSrc, Language lang)
{
	AuctionConfig *pSrc = pvSrc;
	MappedAuctionSettings *pReturn = StructCreate(parse_MappedAuctionSettings);
	int i;

	if(pSrc)
	{
		for(i=0;i<eaSize(&pSrc->eaDurationOptions);i++)
		{
			eaPush(&pReturn->AuctionDurations,createMappedAuctionDuration(pSrc->eaDurationOptions[i]));
		}

		//Add the sort types from the global settings
		for(i=0;i<eaSize(&gSortTypes.ppItemSortTypeCategories);i++)
		{
			ItemSortTypeCategory *pCategory = gSortTypes.ppItemSortTypeCategories[i];
			MappedSortType *pNewType = createMappedSortType(pCategory);

			pNewType->iIndex = i + 1;

			eaPush(&pReturn->SortTypes,pNewType);
		}
	}
	
	return pReturn;
}

/////////////////////////////////////////////////////////////////////////////

AUTO_STRUCT;
typedef struct MappedMicroTransactionDef
{
	const char *pchName;						AST(POOL_STRING)

	REF_TO(Message) hDisplayName;				AST(NAME(DisplayName))
	REF_TO(Message) hUsageInfo;					AST(NAME(UsageInfo))
	REF_TO(Message) hDescription;				AST(NAME(Description))

	REF_TO(ItemDef) hItemDef;					AST(NAME(ItemDef))

	const char *pchIcon;						AST(NAME(Icon) POOL_STRING)
	const char *pchIconLarge;					AST(NAME(IconLarge) POOL_STRING)
	const char **eaPreviews;					AST(NAME(Previews) POOL_STRING)

} MappedMicroTransactionDef;


void *structmap_MicroTransactionDef(StructMapping *pmap, void *pvSrc, Language lang)
{
	MicroTransactionDef *psrc = (MicroTransactionDef *)pvSrc;
	MappedMicroTransactionDef *pdest = StructCreate(parse_MappedMicroTransactionDef);

	if(psrc)
	{
		pdest->pchName = allocAddString(psrc->pchName);

		COPY_HANDLE(pdest->hDisplayName, psrc->displayNameMesg.hMessage);
		COPY_HANDLE(pdest->hUsageInfo, psrc->descriptionShortMesg.hMessage);
		COPY_HANDLE(pdest->hDescription, psrc->descriptionLongMesg.hMessage);

		pdest->pchIcon = allocAddString(psrc->pchIconSmall);
		pdest->pchIconLarge = allocAddString(psrc->pchIconLarge);

		if(psrc->pchIconLargeSecond)
			eaPush(&pdest->eaPreviews, allocAddString(psrc->pchIconLargeSecond));
		if(psrc->pchIconLargeThird)
			eaPush(&pdest->eaPreviews, allocAddString(psrc->pchIconLargeThird));
	}

	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

//
// GetStructMappings
//
StructMapping *OVERRIDE_LATELINK_GetStructMappings(void)
{

	static StructMapping s_aStructMappings[] =
	{
		STRUCT_MAPPING_STANDARD(Message),
		STRUCT_MAPPING_STANDARD(PowerDef),
		STRUCT_MAPPING_STANDARD(MissionDef),
		STRUCT_MAPPING_STANDARD(CharacterClass),
		STRUCT_MAPPING_STANDARD(ItemDef),
		STRUCT_MAPPING_STANDARD(ItemAssignmentDef),
		STRUCT_MAPPING_STANDARD(MicroTransactionDef),
		{ "AuctionSettings", "AuctionSettings", parse_AuctionConfig, parse_MappedAuctionSettings, structmap_AuctionSettings, &gAuctionConfig },

		STRUCT_MAPPING_END
	};

	return s_aStructMappings;
}

/////////////////////////////////////////////////////////////////////////////

#include "NNOGatewayStructMapping_c_ast.c"

// End of File

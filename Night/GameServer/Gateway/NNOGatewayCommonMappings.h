#ifndef NNOGATEWAYCOMMONMAPPINGS_H__
#define NNOGATEWAYCOMMONMAPPINGS_H__

#include "referencesystem.h"
#include "itemEnums.h"

typedef struct PowerDef PowerDef;
typedef struct ItemDef ItemDef;
typedef struct Entity Entity;
typedef struct Item Item;
typedef enum ItemGemType ItemGemType;
typedef enum Language Language;

AUTO_STRUCT;
typedef struct MappedPower
{
	REF_TO(PowerDef) hPowerDef;	AST(NAME(PowerDef) REFDICT(PowerDef))
} MappedPower;

////////////////////////////////////////////////////////////////////////////
// Mapped Inventory
// Should include all inventory and equip bags. Companions are stored in another structure
//

AUTO_STRUCT;
typedef struct MappedGemSlot
{
	ItemGemType eType;		AST(NAME(TYPE) STRUCTPARAM FLAGS)
	bool bFilled;			AST(NAME(Filled) DEFAULT(-1))
}MappedGemSlot;

AUTO_STRUCT;
typedef struct MappedInvSlot
{
	int count;                  AST(NAME(Count))
	char *estrName;             AST(NAME(Name) ESTRING)
	REF_TO(ItemDef) hItemDef;   AST(NAME(ItemDef) REFDICT(ItemDef))
	EARRAY_OF(MappedPower) ppPowers;	AST(NAME(Powers))
	ItemQuality Quality;		AST(NAME(Rarity) DEFAULT(kItemQuality_None))
	bool bBoundToAccount;		AST(NAME(BoundToAccount) DEFAULT(-1))
	bool bBound;				AST(NAME(Bound) DEFAULT(-1))
	bool bSellable;				AST(NAME(Sellable) DEFAULT(-1))
	bool bDiscardable;			AST(NAME(Discardable) DEFAULT(-1))
	bool bIsUsable;				AST(NAME(IsUsable) DEFAULT(-1))
	bool bHasClass;				AST(NAME(HasClass) DEFAULT(-1))
	bool bMeetsLevelRequirements;		AST(NAME(MeetsLevel) DEFAULT(-1))
	bool bMeetsExpressionRequirements;	AST(NAME(MeetsExpr) DEFAULT(-1))
	bool bIsNew;				AST(NAME(IsNew) DEFAULT(-1))

	char *esBoundText;			AST(NAME(BoundText) ESTRING)
	char *esSortText;			AST(NAME(SortText) ESTRING)		// item type
	char *esRareText;			AST(NAME(RareText) ESTRING)	
	U32 iValue;					AST(NAME(Value) DEFAULT(-1))
	U64 uID;
	EARRAY_OF(MappedGemSlot) ppGemSlots; AST(NAME(GemSlots))
} MappedInvSlot;

AUTO_STRUCT;
typedef struct MappedInvBag
{
	InvBagIDs BagID;					AST(NAME(BagID))
	char *estrName;						AST(NAME(Name) ESTRING)
	char *estrIcon;						AST(NAME(Icon) ESTRING)
	EARRAY_OF(MappedInvSlot) ppSlots;	AST(NAME(Slots))
} MappedInvBag;

MappedInvSlot *createMappedInvSlotFromItem(Entity *pEnt, Language lang, Item *psrcItem);

#endif
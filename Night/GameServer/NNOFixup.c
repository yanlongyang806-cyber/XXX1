/***************************************************************************



***************************************************************************/

#include "NNOFixup.h"
#include "NNOCommon.h"

#include "WorldGrid.h"
#include "WorldLibEnums.h"
#include "objTransactions.h"
#include "AutoTransDefs.h"
#include "logging.h"
#include "ResourceInfo.h"
#include "gslHandleMsg.h"
#include "StringUtil.h"

#include "Character.h"
#include "CharacterClass.h"
#include "entCritter.h"
#include "Entity.h"
#include "EntityLib.h"
#include "EntitySavedData.h"
#include "GameAccountDataCommon.h"
#include "inventoryCommon.h"

#include "LoggedTransactions.h"

#include "Player.h"
#include "EntitySavedData.h"
#include "inventoryTransactions.h"
#include "EntityMailCommon.h"
#include "PowerTree.h"
#include "PowerTreeHelpers.h"
#include "ItemAssignments.h"
#include "itemEnums.h"

#include "StickerBookCommon.h"

#include "AutoGen/NNOGameServer_autotransactions_autogen_wrappers.h"
#include "AutoGen/EntitySavedData_h_ast.h"
#include "AutoGen/Player_h_ast.h"
#include "AutoGen/Character_h_ast.h"
#include "AutoGen/EntitySavedData_h_ast.h"
#include "AutoGen/Entity_h_ast.h"
#include "AutoGen/inventoryCommon_h_ast.h"
#include "AutoGen/itemCommon_h_ast.h"
#include "AutoGen/PowerTree_h_ast.h"
#include "AutoGen/itemenums_h_ast.h"
#include "AutoGen/itemassignments_h_ast.h"

typedef struct PlayerFixupState
{
    GlobalType entityType;
    ContainerID entityID;
    bool doRecent;
} PlayerFixupState;

static void
DoEntityVersionFixup(TransactionReturnVal *pReturn, Entity *playerEnt, bool doRecent);

static void FixupEntityVersion_PlayerFixupCB(TransactionReturnVal *returnVal, PlayerFixupState *fixupState)
{
	Entity *playerEnt;

	// Log result of player version migration
	if ( returnVal->eOutcome != TRANSACTION_OUTCOME_SUCCESS ) 
    {
		Errorf("Version migrate failed for player %d %d\n", fixupState->entityType, fixupState->entityID);
		log_printf(LOG_LOGIN, "FAILED NNO Fixup of player %d %d to entity version %d", fixupState->entityType, fixupState->entityID, NNO_ENTITYFIXUPVERSION);
	} 
    else 
    {
		log_printf(LOG_LOGIN, "NNO Fixup completed player %d %d to entity version %d", fixupState->entityType, fixupState->entityID, NNO_ENTITYFIXUPVERSION);
	}

	// If entity is still on this server, continue the login success process
	playerEnt = entFromContainerIDAnyPartition(fixupState->entityType, fixupState->entityID);
	if (playerEnt) 
    {
		if (playerEnt->pChar && playerEnt->pChar->bResetPowersArray) 
        {
			GameAccountDataExtract* pExtract = entity_GetCachedGameAccountDataExtract(playerEnt);
			character_ResetPowersArray(entGetPartitionIdx(playerEnt), playerEnt->pChar, pExtract);
			playerEnt->pChar->bResetPowersArray = false;
		}
		HandlePlayerLogin_Success(playerEnt, kLoginSuccess_Fixup);
	}

	if (playerEnt) 
    {
		// Need to set this dirty in case costume is changed by fixup
		entity_SetDirtyBit(playerEnt, parse_PlayerCostumeData, &playerEnt->pSaved->costumeData, true);
		entity_SetDirtyBit(playerEnt, parse_SavedEntityData, playerEnt->pSaved, true);
	}

	free(fixupState);
}

void OVERRIDE_LATELINK_gameSpecificFixup(Entity *playerEnt)
{
	PlayerFixupState *fixupState = calloc(1,sizeof(PlayerFixupState));
    TransactionReturnVal *returnVal;

    if ( playerEnt->myEntityType != GLOBALTYPE_ENTITYPLAYER )
    {
        ErrorDetailsf("entType=%u, entID=%u", entGetType(playerEnt), entGetContainerID(playerEnt));
        Errorf("Attempted to perform Neverwinter game specific fixup on a non-player entity");
    }

	// Set up the callback data
	fixupState->entityType = entGetType(playerEnt);
	fixupState->entityID = entGetContainerID(playerEnt);
    if ( playerEnt && playerEnt->pSaved )
    {
        // use the main player entity's version to decide whether to do recent or full fixup for all containers
        fixupState->doRecent = playerEnt->pSaved->uGameSpecificFixupVersion >= NNO_LASTFULLFIXUPVERSION;
    }
    else
    {
        fixupState->doRecent = false;
    }

    returnVal = LoggedTransactions_CreateManagedReturnValEnt("EntityFixup:Player", playerEnt, FixupEntityVersion_PlayerFixupCB, fixupState);
    DoEntityVersionFixup(returnVal, playerEnt, fixupState->doRecent);
}

AUTO_TRANS_HELPER;
void entity_trh_UpdateFixupVersion(ATH_ARG NOCONST(Entity) *playerEnt)
{
	playerEnt->pSaved->uGameSpecificFixupVersion = NNO_ENTITYFIXUPVERSION;
}

//
// This transaction only updates the fixup version of the entity.  It should only be called once we have determined that
//  any pending fixup would have no effect on the entity.
//

AUTO_TRANSACTION
ATR_LOCKS(playerEnt, ".Psaved.Ugamespecificfixupversion");
enumTransactionOutcome entity_tr_UpdateFixupVersion(ATR_ARGS, NOCONST(Entity) *playerEnt)
{
	if ( ISNULL(playerEnt) || ISNULL(playerEnt->pSaved) )
	{
		return TRANSACTION_OUTCOME_FAILURE;
	}

	entity_trh_UpdateFixupVersion(playerEnt);

	return TRANSACTION_OUTCOME_SUCCESS;
}

// Remove founders packs, which were mistakenly given out during alpha, from the player's NPC email.
AUTO_TRANS_HELPER;
void
RemoveFoundersPacksFromNPCEMail(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt)
{
    const char *itemNameFromEmail;
    int i, j;
    const char *heroBoxName = allocAddString("Hero_Of_The_North_Box_Collectors");
    const char *guardianBoxName = allocAddString("Neverwinters_Guardian_Box_Collectors");

    if ( ISNULL(playerEnt) || ISNULL(playerEnt->pPlayer) || ISNULL(playerEnt->pPlayer->pEmailV2) )
    {
        // Player doesn't have email.
        return;
    }

    for( i = eaSize(&playerEnt->pPlayer->pEmailV2->mail) - 1; i >= 0; i-- )		
    {
        NOCONST(NPCEMailData) *mailEntry = playerEnt->pPlayer->pEmailV2->mail[i];

        if ( NONNULL(mailEntry) )
        {
            for ( j = eaSize(&mailEntry->ppItemSlot) - 1; j >= 0; j-- )
            {
                NOCONST(InventorySlot) *inventorySlot = mailEntry->ppItemSlot[j];
                if ( NONNULL(inventorySlot) && NONNULL(inventorySlot->pItem) )
                {
                    itemNameFromEmail = REF_STRING_FROM_HANDLE(inventorySlot->pItem->hItem);
                    if ( heroBoxName == itemNameFromEmail || guardianBoxName == itemNameFromEmail )
                    {
                        // The item in email matches one of the founders packs, so remove the email.
                        StructDestroyNoConst(parse_NPCEMailData, mailEntry);
                        eaRemove(&playerEnt->pPlayer->pEmailV2->mail, i);
                        break;
                    }
                }
            }
        }
    }
}

static const char *s_foundersPackItems[] = {
    "Hero_Of_The_North_Box_Collectors",
    "Neverwinters_Guardian_Box_Collectors",
    "Primary_Control_Purple_Delzoun",
    "Primary_Devoted_Purple_Delzoun",
    "Primary_Trickster_Purple_Delzoun",
    "Primary_Greatweapon_Purple_Delzoun",
    "Primary_Guardian_Purple_Delzoun",
    "Flavor_Robe_Useless_Collectors",
    "Equipment_Primary_Delzoun_Treasure_Box_Collectors",
    "Mount_Giantspider_Armored_Collectors",
    "Pet_Panther_Purple_Collectors",
    "Pet_Wolf_Blue_Collectors",
    "Mount_Horse_Epic_Founder_Collectors",
    "Bag_Smallbagofholding_12_Collectors",
    "Equipment_Primary_Greycloaks_Legacy_Collectors",
    "Armor_Enhancement_Founders_Collectors",
    NULL
};

static const char **s_foundersPackItemsPooled = NULL;

AUTO_TRANS_HELPER;
void
RemoveFoundersPackItemsFromInventory(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt)
{
    int i, j, k;

    if ( s_foundersPackItemsPooled == NULL )
    {
        // The first time here, build the array of pooled item name strings.
        const char **pItemName = s_foundersPackItems;
        while ( *pItemName != NULL )
        {
            eaPush(&s_foundersPackItemsPooled, allocAddString(*pItemName));
            pItemName++;
        }
    }

    // Check all bags.
    for (i = eaSize(&playerEnt->pInventoryV2->ppInventoryBags)-1; i >= 0; i--)
    {
        NOCONST(InventoryBag)* pBag = playerEnt->pInventoryV2->ppInventoryBags[i];

        // Check every slot in the bag.
        for (j = eaSize(&pBag->ppIndexedInventorySlots)-1; j >= 0; j--)
        {
            NOCONST(InventorySlot)* pSlot = pBag->ppIndexedInventorySlots[j];
            ItemDef* pItemDef = NONNULL(pSlot->pItem) ? GET_REF(pSlot->pItem->hItem) : NULL;

            // Check the item name against all the founder's pack item names.
            for ( k = eaSize(&s_foundersPackItemsPooled) - 1; k >= 0; k--)
            {
                if (pItemDef && pItemDef->pchName == s_foundersPackItemsPooled[k])
                {
                    // If the item matches one of the founders pack items, then delete it.
                    StructDestroyNoConstSafe(parse_Item, &pSlot->pItem);
                    break;
                }
            }
        }
    }
}

//
// Give any existing character that is a Menzobaranzan Renegade Drow a free appearance change token.  This is so that characters that didn't
//  have access to the exclusive Drow tattoos at launch can get them on their character without having to pay for an appearance change.
//
AUTO_TRANS_HELPER;
bool FixupMaleDrowWithAppearanceChangeToken(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason)
{
    const char *speciesName;

    if ( NONNULL(playerEnt) && NONNULL(playerEnt->pChar) )
    {
        speciesName = REF_STRING_FROM_HANDLE(playerEnt->pChar->hSpecies);

        if ( ( stricmp(speciesName, "Drowrenegade_Male") == 0 ) || ( stricmp(speciesName, "Drowrenegade_Female") == 0 ) )
        {
            if(!inv_ent_trh_AddNumeric(ATR_PASS_ARGS, playerEnt, true, "Freecostumechange", 1, pReason))
            {
                return false;
            }
        }
    }

    return true;
}

// Replace any _MT "skill kit" items with non-MT equivalents, because they don't work.
AUTO_TRANS_HELPER;
bool ReplaceBrokenMTSkillKitItems(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason, GameAccountDataExtract *pExtract)
{
	const char* ppchBrokenItemNames[5] = {"Item_Consumable_Skill_Arcana_Mt",
		"Item_Consumable_Skill_Dungeoneering_Mt",
		"Item_Consumable_Skill_Nature_Mt",
		"Item_Consumable_Skill_Religion_Mt",
		"Item_Consumable_Skill_Thievery_Mt"};
	const char* ppchFixedItemNames[5] = {"Item_Consumable_Skill_Arcana",
		"Item_Consumable_Skill_Dungeoneering",
		"Item_Consumable_Skill_Nature",
		"Item_Consumable_Skill_Religion",
		"Item_Consumable_Skill_Thievery"};

	if ( NONNULL(playerEnt) && NONNULL(playerEnt->pChar) )
	{
		int i;
		for (i = 0; i < 5; i++)
		{
			int iRemoved = inv_trh_RemoveAllItemByDefName(ATR_PASS_ARGS, playerEnt, ppchBrokenItemNames[i], pReason, pExtract);
			if (iRemoved > 0)
			{
				if (!inv_ent_trh_AddItemFromDef(ATR_PASS_ARGS, playerEnt, NULL, InvBagIDs_Inventory, -1, ppchFixedItemNames[i], iRemoved, 0, NULL, ItemAdd_UseOverflow, pReason, pExtract))
					return false;
			}
		}
	}

	return true;
}

// Replace any _MT "skill kit" items with non-MT equivalents, because they don't work.
AUTO_TRANS_HELPER;
bool RepairBrokenProfessionAssets(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt, GameAccountDataExtract *pExtract)
{
	if ( NONNULL(playerEnt) && NONNULL(playerEnt->pInventoryV2) )
	{
		int iBagID = StaticDefineInt_FastStringToInt(InvBagIDsEnum, "CraftingInventory", 0);
		BagIterator* pIter = bagiterator_Create();
		NOCONST(ItemAssignment)*** peaActiveAssignments = NULL;
		bagiterator_trh_SetBagByID(ATR_PASS_ARGS, playerEnt, iBagID, pIter, pExtract);

		if (NONNULL(playerEnt->pPlayer->pItemAssignmentPersistedData))
		{
			peaActiveAssignments = &playerEnt->pPlayer->pItemAssignmentPersistedData->eaActiveAssignments;
		}

		while (bagiterator_Next(pIter))
		{
			bool bFound = false;
			NOCONST(Item)* pItem = bagiterator_GetItem(pIter);
			if (pItem && (pItem->flags & kItemFlag_SlottedOnAssignment))
			{
				if (peaActiveAssignments != NULL)
				{
					int i, j;
					for (i = 0; i < eaSize(peaActiveAssignments); i++)
					{
						NOCONST(ItemAssignment)* pAssignment = (*peaActiveAssignments)[i];
						for (j = 0; j < eaSize(&pAssignment->eaSlottedItems); j++)
						{
							if (pAssignment->eaSlottedItems[j]->uItemID == pItem->id)
							{
								bFound = true;
								break;
							}
						}
						if (bFound)
							break;
					}
				}
				
				if (!bFound)
					pItem->flags &= ~kItemFlag_SlottedOnAssignment;
			}
		}
		bagiterator_Destroy(pIter);
	}

	return true;
}


// Give a reward pack to all existing players at time of the fixup as compensation for rollbacks/downtime.
AUTO_TRANS_HELPER;
bool GiveCaturdayRewardPack(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason)
{
    MailCharacterItems* pMailItems = NULL;
    const char *sender;
    const char *subject;
    const char *body;
    NOCONST(Item)* pItem;
    
    // Create item
    pItem = inv_ItemInstanceFromDefName("Caturday_Survivor_Box_Promo", 0, 0, NULL, NULL, NULL, false, NULL);
    if ( pItem == NULL )
    {
        return false;
    }

    pMailItems = CharacterMailAddItem(NULL, CONTAINER_RECONST(Item, pItem));
    if ( pMailItems == NULL )
    {
        return false;
    }

    // Translate message for the player.
    sender = langTranslateMessageKeyDefault(playerEnt->pPlayer->langID, "NNO_Fixup_Caturday_Mail_Sender", "[UNTRANSLATED]Sender");
    subject = langTranslateMessageKeyDefault(playerEnt->pPlayer->langID, "NNO_Fixup_Caturday_Mail_Subject", "[UNTRANSLATED]Subject");
    body = langTranslateMessageKeyDefault(playerEnt->pPlayer->langID, "NNO_Fixup_Caturday_Mail_Body", "[UNTRANSLATED]Body");

    // Add the email to the player.
    return EntityMail_trh_NPCAddMail(ATR_PASS_ARGS, playerEnt, sender, subject, body, pMailItems, 0, kNPCEmailType_Default, pReason);
}

// Grant StickerBook items for participating Items in Player's inventory.
AUTO_TRANS_HELPER;
bool GrantStickerBookItems_Fixup_8(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt)
{
	if ( NONNULL(playerEnt) && NONNULL(playerEnt->pInventoryV2) )
	{
		FOR_EACH_IN_EARRAY(playerEnt->pInventoryV2->ppInventoryBags, NOCONST(InventoryBag), pInventoryBag)
		{
			FOR_EACH_IN_EARRAY(pInventoryBag->ppIndexedInventorySlots, NOCONST(InventorySlot), pInventorySlot)
			{
				if(NONNULL(pInventorySlot->pItem))
				{
					ItemDef *pItemDef = GET_REF(pInventorySlot->pItem->hItem);
					StickerBook_trh_MaybeRecentlyAcquiredItem(playerEnt, pItemDef);
				}
			}
			FOR_EACH_END;
		}
		FOR_EACH_END;
	}

	return true;
}

// Unbind any Mount_Owlbear items obtained via the lockbox released on 10/03/2013. We are doing this no matter where it is in one's inventory, even if it
// is equipped in the Mounts bag. Production is ok with this decision. It essentially gives players with this item equipped a free-be unequip and sell/trade of it.
AUTO_TRANS_HELPER;
bool RemoveOwlbearMountBoundFlag_Fixup_7(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt)
{
	static const char *pchMount_Owlbear = NULL;

	if(!pchMount_Owlbear) pchMount_Owlbear = allocAddString("Mount_Owlbear");

	if(NONNULL(playerEnt) && NONNULL(playerEnt->pInventoryV2))
	{
		FOR_EACH_IN_EARRAY(playerEnt->pInventoryV2->ppInventoryBags, NOCONST(InventoryBag), pInventoryBag)
		{
			FOR_EACH_IN_EARRAY(pInventoryBag->ppIndexedInventorySlots, NOCONST(InventorySlot), pInventorySlot)
			{
				if(NONNULL(pInventorySlot->pItem))
				{
					NOCONST(Item) *pItem = pInventorySlot->pItem;
					ItemDef *pItemDef = GET_REF(pItem->hItem);
					if(pItemDef && pItemDef->pchName == pchMount_Owlbear)
						pItem->flags &= ~kItemFlag_Bound;
				}
			}
			FOR_EACH_END;
		}
		FOR_EACH_END;
	}

	return true;
}

AUTO_TRANS_HELPER;
enumTransactionOutcome entity_trh_NNOFixupEntityVersionRecent(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason, GameAccountDataExtract *pExtract)
{
	// Note we say less than 7. 6 may or may not have been applied and we bumped 6 to 8. So, if less than 7, we are safe to apply 6 (which is now a noop), 7 (which is the Owlbear fix),
	// and 8 (which is the bumped sticker book fix).
	if(playerEnt->pSaved->uGameSpecificFixupVersion < 7)
	{
		if ( !RemoveOwlbearMountBoundFlag_Fixup_7(ATR_PASS_ARGS, playerEnt) )
		{
			return TRANSACTION_OUTCOME_FAILURE;
		}

		if ( !GrantStickerBookItems_Fixup_8(ATR_PASS_ARGS, playerEnt) )
		{
			return TRANSACTION_OUTCOME_FAILURE;
		}
	}
    return TRANSACTION_OUTCOME_SUCCESS;
}

AUTO_TRANSACTION
ATR_LOCKS(playerEnt, ".Psaved.Ugamespecificfixupversion, .Pinventoryv2.Ppinventorybags, .Pplayer.Eaastrrecentlyacquiredstickerbookitems");
enumTransactionOutcome entity_tr_NNOFixupEntityVersionRecent(ATR_ARGS, NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason, GameAccountDataExtract *pExtract)
{
	enumTransactionOutcome outcome;

	//
	// IMPORTANT NOTE: This function must not change any persisted data itself.  All changes to the Entities (except the fixup version)
	//  must be made in entity_trh_NNOFixupEntityVersionRecent() since that helper is also called outside of the transaction to 
	//  determine if the fixup will make any real changes to the Entity.  This function should only do the following:
	//    1) Validate inputs
	//    2) Call the helper to make the fixups
	//    3) Call the helper to update the fixup version
	//

    // This fixup only works on player entitites.
	if ( ISNULL(playerEnt) || ISNULL(playerEnt->pSaved) || playerEnt->myEntityType != GLOBALTYPE_ENTITYPLAYER )
	{
		return TRANSACTION_OUTCOME_FAILURE;
	}

	if ( playerEnt->pSaved->uGameSpecificFixupVersion < NNO_LASTFULLFIXUPVERSION )
	{
		// error and abort the transaction if someone is trying to do a "recent" fixup for an entity that hasn't had the previous fixups done
		ErrorDetailsf("debugName=%s, entFixupVersion=%d", playerEnt->debugName, playerEnt->pSaved->uGameSpecificFixupVersion);
		Errorf("Attempted 'Recent' entity version fixup for entity that is missing older fixups");

		return TRANSACTION_OUTCOME_FAILURE;
	}

	outcome = entity_trh_NNOFixupEntityVersionRecent(ATR_PASS_ARGS, playerEnt, pReason, pExtract);
	if ( outcome != TRANSACTION_OUTCOME_SUCCESS )
	{
		return outcome;
	}

	entity_trh_UpdateFixupVersion(playerEnt);

	return TRANSACTION_OUTCOME_SUCCESS;
}

AUTO_TRANS_HELPER;
enumTransactionOutcome entity_trh_NNOFixupEntityVersionFull(ATR_ARGS, ATH_ARG NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason, GameAccountDataExtract *pExtract)
{
    // Older fixups go here.
    if(playerEnt->pSaved->uGameSpecificFixupVersion < 1)
    {
        RemoveFoundersPacksFromNPCEMail(ATR_PASS_ARGS, playerEnt);
        RemoveFoundersPackItemsFromInventory(ATR_PASS_ARGS, playerEnt);
    }

    if(playerEnt->pSaved->uGameSpecificFixupVersion < 2)
    {
        if ( !FixupMaleDrowWithAppearanceChangeToken(ATR_PASS_ARGS, playerEnt, pReason) )
        {
            return TRANSACTION_OUTCOME_FAILURE;
        }
    }
    if(playerEnt->pSaved->uGameSpecificFixupVersion < 3)
    {
        if ( !ReplaceBrokenMTSkillKitItems(ATR_PASS_ARGS, playerEnt, pReason, pExtract) )
        {
            return TRANSACTION_OUTCOME_FAILURE;
        }
	}

	if(playerEnt->pSaved->uGameSpecificFixupVersion < 4)
	{
		if ( !GiveCaturdayRewardPack(ATR_PASS_ARGS, playerEnt, pReason) )
		{

			return TRANSACTION_OUTCOME_FAILURE;
		}
	}

	if(playerEnt->pSaved->uGameSpecificFixupVersion < 5)
	{
		if ( !RepairBrokenProfessionAssets(ATR_PASS_ARGS, playerEnt, pExtract) )
		{
			return TRANSACTION_OUTCOME_FAILURE;
		}
	}

	// Also do any "recent" fixups.
	return entity_trh_NNOFixupEntityVersionRecent(ATR_PASS_ARGS, playerEnt, pReason, pExtract);
}

AUTO_TRANSACTION
ATR_LOCKS(playerEnt, ".Pplayer.Pitemassignmentpersisteddata.Eaactiveassignments, .Pplayer.Pemailv2.Unextfuturedelivery, .Pplayer.Pemailv2.Bunreadmail, .Psaved.Ugamespecificfixupversion, .Pplayer.Pemailv2.Mail, .Pinventoryv2.Ppinventorybags, .Pchar.Hspecies, .Psaved.Ppallowedcritterpets, .Pplayer.Langid, .Psaved.Pscpdata.Isummonedscp, .Psaved.Pscpdata.Fcachedpetbonusxppct, .Psaved.Pscpdata.Erscp, .Hallegiance, .Hsuballegiance, .Pplayer.Pplayeraccountdata.Eagamepermissionmaxvaluenumerics, .Pchar.Ilevelexp, .Pinventoryv2.Pplitebags, .Pplayer.Pemailv2.Ilastusedid, .Psaved.Uiindexbuild, .Psaved.Ppbuilds, .Psaved.Conowner.Containerid, .Psaved.Conowner.Containertype, .Pinventoryv2.Peaowneduniqueitems, .Pplayer.Playertype, .Psaved.Ppownedcontainers, .Itemidmax, .Pchar.Uipoweridmax, .Psaved.Ipetid, .Psaved.Ppuppetmaster.Curid, .Psaved.Ppuppetmaster.Curtype, .Psaved.Ppuppetmaster.Curtempid, .Pplayer.Eaplayernumericthresholds, .Pplayer.Eaastrrecentlyacquiredstickerbookitems");
enumTransactionOutcome entity_tr_NNOFixupEntityVersionFull(ATR_ARGS, NOCONST(Entity)* playerEnt, const ItemChangeReason *pReason, GameAccountDataExtract *pExtract)
{
	enumTransactionOutcome outcome;

	//
	// IMPORTANT NOTE: This function must not change any persisted data itself.  All changes to the Entities (except the fixup version)
	//  must be made in entity_trh_NNOFixupEntityVersionFull() since that helper is also called outside of the transaction to 
	//  determine if the fixup will make any real changes to the Entity.  This function should only do the following:
	//    1) Validate inputs
	//    2) Call the helper do make the fixups
	//    3) Call the helper to update the fixup version
	//

	if ( ISNULL(playerEnt) || ISNULL(playerEnt->pSaved) )
	{
		return TRANSACTION_OUTCOME_FAILURE;
	}

	outcome = entity_trh_NNOFixupEntityVersionFull(ATR_PASS_ARGS, playerEnt, pReason, pExtract);
	if ( outcome != TRANSACTION_OUTCOME_SUCCESS )
	{
		return outcome;
	}

	entity_trh_UpdateFixupVersion(playerEnt);

	return TRANSACTION_OUTCOME_SUCCESS;
}

static void
DoEntityVersionFixup(TransactionReturnVal *returnVal, Entity *playerEnt, bool doRecent)
{
	GameAccountDataExtract *pExtract = NULL;
    ItemChangeReason reason = {0};

    PERFINFO_AUTO_START_FUNC();

    devassert(playerEnt->myEntityType == GLOBALTYPE_ENTITYPLAYER);

	if ( NONNULL(playerEnt) && NONNULL(playerEnt->pSaved) )
	{
        Entity *entCopy = NULL;
        int entDiffResult = 0;

        pExtract = entity_GetCachedGameAccountDataExtract(playerEnt);

        // Player entities are owned by the gameserver so we can perform an additional optimization, where we run the
        //  fixup outside of a transaction on a copy of the original entity, and compare the results to the original.  If nothing
        //  changes, then we can just update the fixup version with a really cheap transaction.
		//
		// The GatewayServer doesn't have a full copy, so we need to skip this optimization as well.
		//
		if(GetAppGlobalType() != GLOBALTYPE_GATEWAYSERVER)
		{
			PERFINFO_AUTO_START("DoEntityVersionFixup:Clone Entity", 1);
			entCopy = StructCloneWithComment(parse_Entity, playerEnt, "Temp entity for versioned entity fixup test");
			PERFINFO_AUTO_STOP();
		}

		// Run the fixup transaction helper on a copy of the entity, and then compare the copy to the original.
		// If there are no differences than we can safely skip the fixup, since it won't be making any changes, and just
		//  bump up the entity's fixup version number.
		if ( NONNULL(entCopy) && GetAppGlobalType() != GLOBALTYPE_GATEWAYSERVER)
		{
			// run the recent or full fixup helper on the copy entities
			if ( doRecent )
			{
                PERFINFO_AUTO_START("DoEntityVersionFixup:Recent Fixup Copy", 1);
				inv_FillItemChangeReason(&reason, playerEnt, "Fixup:Recent", playerEnt ? playerEnt->debugName : NULL);
				entity_trh_NNOFixupEntityVersionRecent(ATR_EMPTY_ARGS, CONTAINER_NOCONST(Entity, entCopy), &reason, pExtract);
                PERFINFO_AUTO_STOP();
			}
			else
			{
                PERFINFO_AUTO_START("DoEntityVersionFixup:Full Fixup Copy", 1);
				inv_FillItemChangeReason(&reason, playerEnt, "Fixup:Full", playerEnt ? playerEnt->debugName : NULL );
				entity_trh_NNOFixupEntityVersionFull(ATR_EMPTY_ARGS, CONTAINER_NOCONST(Entity, entCopy), &reason, pExtract);
                PERFINFO_AUTO_STOP();
			}

			// diff the main entity
            PERFINFO_AUTO_START("DoEntityVersionFixup:Compare Entity", 1);
			entDiffResult = StructCompare(parse_Entity, playerEnt, entCopy, 0, TOK_PERSIST, TOK_NO_TRANSACT);
            PERFINFO_AUTO_STOP();

			// free the copy
			StructDestroy(parse_Entity, entCopy);

			// if compare does not find differences, then we can do the super fast transaction that just updates the version number
			if ( entDiffResult == 0 )
			{
				entLog(LOG_GSL, playerEnt, "NNOEntityFixupVersionOnly", "Performing version only entity fixup.  oldVersion = %d, newVersion = %d, entityID=%d", playerEnt->pSaved->uGameSpecificFixupVersion, NNO_ENTITYFIXUPVERSION, entGetContainerID(playerEnt));
                AutoTrans_entity_tr_UpdateFixupVersion(returnVal, GetAppGlobalType(), entGetType(playerEnt), entGetContainerID(playerEnt));
                PERFINFO_AUTO_STOP();
				return;
			}
		}
		else
		{
			// free any copies in case things go wrong above
			if ( NONNULL(entCopy) )
			{
				StructDestroy(parse_Entity, entCopy);
			}
		}

		// if the entity version is new enough, we can do a cheaper "recent" fixup
		if ( doRecent )
		{
			entLog(LOG_GSL, playerEnt, "NNOEntityFixupVersionRecent", "Performing recent entity fixup.  oldVersion = %d, newVersion = %d, entityID=%d", playerEnt->pSaved->uGameSpecificFixupVersion, NNO_ENTITYFIXUPVERSION, entGetContainerID(playerEnt));
			inv_FillItemChangeReason(&reason, playerEnt, "Fixup:Recent", playerEnt ? playerEnt->debugName : NULL);
			AutoTrans_entity_tr_NNOFixupEntityVersionRecent(returnVal, GetAppGlobalType(), 
					entGetType(playerEnt), entGetContainerID(playerEnt),
					&reason, pExtract);
            PERFINFO_AUTO_STOP();
		}
        else
        {
            entLog(LOG_GSL, playerEnt, "NNOEntityFixupVersionFull", "Performing full entity fixup.  oldVersion = %d, newVersion = %d, entityID=%d", playerEnt->pSaved->uGameSpecificFixupVersion, NNO_ENTITYFIXUPVERSION, entGetContainerID(playerEnt));

            inv_FillItemChangeReason(&reason, playerEnt, "Fixup:Full", playerEnt ? playerEnt->debugName : NULL);

            AutoTrans_entity_tr_NNOFixupEntityVersionFull(returnVal, GetAppGlobalType(), 
                entGetType(playerEnt), entGetContainerID(playerEnt),
                &reason, pExtract);
        }
    }
	else
	{
		// The entity is not present on this gameserver.  Something is wrong.
		log_printf(LOG_GSL, "Failing entity fixup because entity is missing or invalid.");
	}

    PERFINFO_AUTO_STOP();
}

// 强制全地图PVP：覆盖worldRegionGetType函数，使所有区域类型都返回WRT_PvP
WorldRegionType OVERRIDE_LATELINK_worldRegionGetType(const WorldRegion *region)
{
	// 如果不是NULL，返回原始类型，但我们需要强制PVP
	// 为了实现全地图强制PVP，我们直接返回WRT_PvP
	return WRT_PvP;
}

bool OVERRIDE_LATELINK_ent_NeedsProjectSpecificPowersFixup(Entity *pEnt)
{
	NOCONST(Entity) *pEntNoConst = CONTAINER_NOCONST(Entity, pEnt);
	PowerTreeDef *pTreeDef = powertreedef_Find("AbilityScores_CharacterCreation");
	NOCONST(PowerTree) *pTree = entity_FindPowerTreeHelper(pEntNoConst, pTreeDef);
	
	// Enable this code for testing. It removes the AbilityScores_CharacterCreation node from the character,
	// and causes the fixup to happen.
//	if(pTreeDef && pTree && eaSize(&pTree->ppNodes) )
//	{
//		entity_PowerTreeNodeDecreaseRankHelper(pEntNoConst, pTreeDef->pchName, REF_STRING_FROM_HANDLE(pTree->ppNodes[0]->hDef), true, false, NULL);
//	}

	// If they have NO PTNodes in the AbilityScores tree, they need a fixup.
	return !pTree || (eaSize(&pTree->ppNodes) == 0);
}

void OVERRIDE_LATELINK_ent_FixupProjectSpecificPowers(Entity *pEnt)
{
	PowerTreeDef *pTreeDef;
	
	if(!pEnt)
		return;

	ErrorDetailsf("ContainerID:%d", pEnt->myContainerID);
	Errorf("ent_FixupProjectSpecificPowers - Running Project-specific Powers fixup on entity.");

	// Find the first AbilityScores_CharacterCreation tree node that the entity can use, and buy it.
	pTreeDef = powertreedef_Find("AbilityScores_CharacterCreation");
	if( pTreeDef )
	{
		NOCONST(Entity) *pEntNoConst = CONTAINER_NOCONST(Entity, pEnt);
		NOCONST(PowerTree) *pTree;
		int iGroup=0, iNode=0;

		// Loop through all the groups.
		pTree = entity_FindPowerTreeHelper(pEntNoConst, pTreeDef);
		for( iGroup=0; iGroup < eaSize(&pTreeDef->ppGroups); ++iGroup )
		{
			PTGroupDef *pGroupDef = pTreeDef->ppGroups[iGroup];
			NOCONST(PTNode) *pNode = NULL;
			// If we don't meet the requirements, continue to the next group.
			if (pGroupDef->pRequires && !EntityPTPurchaseReqsHelper(ATR_EMPTY_ARGS,pEntNoConst,pEntNoConst,pTree,pGroupDef->pRequires))
				continue;

			// Find a node we can use
			for( iNode=0; iNode < eaSize(&pGroupDef->ppNodes); ++iNode )
			{
				PTNodeDef *pPTNodeDef = pGroupDef->ppNodes[iNode];
				pNode = entity_PowerTreeNodeIncreaseRankHelper(PARTITION_CLIENT, pEntNoConst, NULL, pTreeDef->pchName, pPTNodeDef->pchNameFull, false, false, false, NULL);
				// If we were able to buy a node rank, we're done.
				if( pNode )
				{
					break;
				}
			}

			// Only need to buy 1 node, so quit after the first was successfully purchased
			if( pNode )
			{
				break;
			}
		}
	}

}
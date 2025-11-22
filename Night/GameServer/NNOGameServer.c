/***************************************************************************
 
 
 
 ***************************************************************************/
#include "NNOGameServer.h"

#include "Character.h"
#include "Entity.h"
#include "Player.h"

// Used to check if user experience logging should occur for this user
bool OVERRIDE_LATELINK_UserExp_ShouldLogThisUser(Entity *pEnt)
{
	if (!pEnt || 
		!pEnt->pPlayer ||
		!pEnt->pChar || 
		(pEnt->pChar->iLevelExp > 8)) {
		return false;
	}

	return true;
}

// For Neverwinter devs. Refill character hp, action points, remove injuries. based on Refill_HP_POW()
AUTO_COMMAND ACMD_ACCESSLEVEL(5);
void NWCureAll(Entity *e)
{
	int i;
	if(e && e->pChar)
	{
		U32 *eaiApplicationIDs = NULL;
		int eTag = StaticDefineIntGetInt(PowerTagsEnum, "injury");

		e->pChar->pattrBasic->fHitPoints = e->pChar->pattrBasic->fHitPointsMax;
		e->pChar->pattrBasic->fPower = e->pChar->pattrBasic->fPowerMax; 

		if(eTag != -1)
		{
			for (i = eaSize(&e->pChar->modArray.ppMods) - 1; i >= 0; i--)
			{
				AttribMod* pMod = e->pChar->modArray.ppMods[i];
				AttribModDef* pModDef = mod_GetDef(pMod);
				if (powertags_Check(&pModDef->tags, eTag))
				{
					eaiPushUnique(&eaiApplicationIDs, pMod->uiApplyID);
					modarray_Remove(&e->pChar->modArray, i);
				}
			}

			// find all the mods that came from the same application ID and remove those mods as well
			if (eaiSize(&eaiApplicationIDs) > 0)
			{
				for (i = eaSize(&e->pChar->modArray.ppMods) - 1; i >= 0; i--)
				{
					AttribMod* pMod = e->pChar->modArray.ppMods[i];
					
					if (eaiFind(&eaiApplicationIDs, pMod->uiApplyID) >= 0)
					{
						modarray_Remove(&e->pChar->modArray, i);
					}
				}
			}
		}

		eaiDestroy(&eaiApplicationIDs);
		character_DirtyAttribs(e->pChar);
	}
}

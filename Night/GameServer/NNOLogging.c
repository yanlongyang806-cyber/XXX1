#include "GlobalTypes.h"
#include "gslentity.h"
#include "error.h"
#include "errornet.h"
#include "ticketnet.h"
#include "GameServerLib.h"
#include "GameStringFormat.h"
#include "textparser.h"
#include "estring.h"
#include "utilitiesLib.h"
#include "OfficerCommon.h"
#include "Player.h"
#include "ServerLib.h"
#include "entity.h"
#include "allegiance.h"
#include "Character.h"
#include "CharacterClass.h"
#include "StringCache.h"

#include "inventoryCommon.h"

#include "Autogen/entity_h_ast.h"
#include "AutoGen/GameClientLib_autogen_ClientCmdWrappers.h"


char *OVERRIDE_LATELINK_entity_CreateProjSpecificLogString(Entity *entity)
{
    static const char *s_ScourgeWarlock = NULL;
    static const char *s_TricksterRogue = NULL;
    static const char *s_DevotedCleric = NULL;
    static const char *s_GreatWeaponFighter = NULL;
    static const char *s_GuardianFighter = NULL;
    static const char *s_ArcherRanger = NULL;
    static const char *s_ControlWizard = NULL;

	PERFINFO_AUTO_START_FUNC();
	{
		int Level = inv_GetNumericItemValue(entity, "Level");
        const char *classShortName;

        if ( s_ScourgeWarlock == NULL )
        {
            s_ScourgeWarlock = allocAddString("Player_Scourge");
            s_TricksterRogue = allocAddString("Player_Trickster");
            s_DevotedCleric = allocAddString("Player_Devoted");
            s_GreatWeaponFighter = allocAddString("Player_Greatweapon");
            s_GuardianFighter = allocAddString("Player_Guardian");
            s_ArcherRanger = allocAddString("Player_Archer");
            s_ControlWizard = allocAddString("Player_Controller");
        }

        if (!entity->estrProjSpecificLogString)
        {
            estrCreate(&entity->estrProjSpecificLogString);
        }

        if ( entity->myEntityType == GLOBALTYPE_ENTITYPLAYER )
        {
            CharacterClass *characterClass = GET_REF(entity->pChar->hClass);
            const char *classNamePooled = allocAddString(characterClass->pchName);
            int playedTime = 0;

            if ( classNamePooled == s_ScourgeWarlock )
            {
                classShortName = "SW";
            }
            else if ( classNamePooled == s_TricksterRogue )
            {
                classShortName = "TR";
            }
            else if ( classNamePooled == s_DevotedCleric )
            {
                classShortName = "DC";
            }
            else if ( classNamePooled == s_GreatWeaponFighter )
            {
                classShortName = "GW";
            }
            else if ( classNamePooled == s_GuardianFighter )
            {
                classShortName = "GF";
            }
            else if ( classNamePooled == s_ArcherRanger )
            {
                classShortName = "AR";
            }
            else if ( classNamePooled == s_ControlWizard )
            {
                classShortName = "CW";
            }
            else
            {
                classShortName = "XX";
                Errorf("A programmer needs to add a case for class %s to %s()", characterClass->pchName, __FUNCTION__);
            }

            if ( entity->pPlayer )
            {
                playedTime = (int)entity->pPlayer->fTotalPlayTime;
            }

            estrPrintf(&entity->estrProjSpecificLogString, "LEV %d,CL %s,PT %d", Level, classShortName, playedTime);
            entity->lastProjSpecificLogTime = timeSecondsSince2000();
        }
        else
        {
            estrPrintf(&entity->estrProjSpecificLogString, "LEV %d", Level);
        }

		if (entGetVirtualShardID(entity))
		{
			estrConcatf(&entity->estrProjSpecificLogString, ",VSH %d", entGetVirtualShardID(entity));
		}
	}
	PERFINFO_AUTO_STOP();

	return entity->estrProjSpecificLogString;
}
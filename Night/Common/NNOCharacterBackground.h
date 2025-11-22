/***************************************************************************



***************************************************************************/
#ifndef CHARACTERBACKGROUND_H__
#define CHARACTERBACKGROUND_H__
GCC_SYSTEM

#include "Message.h"

typedef struct Login2CharacterCreationData Login2CharacterCreationData;
typedef struct GameAccountDataExtract GameAccountDataExtract;
typedef struct NOCONST(Entity) NOCONST(Entity);

AUTO_STRUCT;
typedef struct CharacterBackgroundChoice
{	
	// The internal name of the background
	char *pchName;											AST(STRUCTPARAM POOL_STRING KEY)

	// Translated display name which players see
	DisplayMessage pDisplayName;							AST(STRUCT(parse_DisplayMessage) NAME(DisplayName))

	// Translated flavor text which players see
	DisplayMessage pFlavorText;								AST(STRUCT(parse_DisplayMessage) NAME(FlavorText))

	// Translated description which players see
	DisplayMessage pBiographyText;							AST(STRUCT(parse_DisplayMessage) NAME(BioText))

	const char** ppchGrantedItems;							AST(POOL_STRING NAME(GrantedItem) SERVER_ONLY RESOURCEDICT(ItemDef))
	const char** ppchPowerTreeNodeChoices;					AST(POOL_STRING NAME(PowerTreeNode) RESOURCEDICT(PTNodeDef))

} CharacterBackgroundChoice;

AUTO_STRUCT;
typedef struct CharacterBackgroundGroup
{
	// The internal name of the background grouping
	char *pchName;											AST(STRUCTPARAM POOL_STRING KEY)

	// Current file (required for reloading)
	const char *pchFile;									AST(CURRENTFILE)		
	
	// Translated display name which players see
	DisplayMessage pDisplayName;							AST(STRUCT(parse_DisplayMessage) NAME(DisplayName))

	// Translated flavor text which players see
	DisplayMessage pFlavorText;								AST(STRUCT(parse_DisplayMessage) NAME(FlavorText))

	// The image displayed to the players for this grouping.
	const char *pchTextureName;								AST(POOL_STRING NAME(Texture))

	const char **ppchGrantedItems;							AST(POOL_STRING NAME(GrantedItem) SERVER_ONLY RESOURCEDICT(ItemDef))
	//map coordinates
	float XPercent;
	float YPercent;

	CharacterBackgroundChoice** ppBackgrounds;
} CharacterBackgroundGroup;

void CharacterBackground_Load(void);
bool CharacterBackground_DoNewCharacterInitialization(NOCONST(Entity) *ent, Login2CharacterCreationData *characterCreationData, GameAccountDataExtract* pExtract);


#endif
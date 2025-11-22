/***************************************************************************



***************************************************************************/

#ifndef PRODUCTTYPES_H_
#define PRODUCTTYPES_H_

#include "GlobalTypes.h"

// This is the file that defines the project specific container types

// The Fixup Version is used for performing game-specific fixes on the Entity structures
// When incrementing this, document who changed it, when, and why
// Version  0: From the dawn of time
// Version  1: jweinstein	01/23/2013 - Remove founders pack items accidentally given out during alpha
// Version  2: jweinstein	04/25/2013 - Give Founders Pack Drow a free appearance change token
// Version  3: cmiller		04/25/2013 - Replace broken _MT skill kit items with non-MT equivalents
// Version  4: jweinstein	05/20/2013 - Give every existing character a Cat-urday reward pack as compensation for downtime/rollback
// Version  5: cmiller		08/15/2013 - Fix broken profession assets that have their "In use" flag permanently stuck on.
// Version  6: andrewa		09/17/2013 - Grant StickerBook items for participating and bound Items in Player's inventory. Bumped to fixup version 8.

// This fixup is a merge from release 2. 6 is safe to run multiple times but 7 is not.
// Version  7: andrewa		10/03/2013 - Fix to Owlbear item in lockbox having BindOnPickup flagged.

// Version  8: andrewa		10/03/2013 - Grant StickerBook items for participating and bound Items in Player's inventory. Bumped from fixup version 6.

#define NNO_LASTFULLFIXUPVERSION 5
#define NNO_ENTITYFIXUPVERSION 8

#endif

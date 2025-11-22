/***************************************************************************



***************************************************************************/

#pragma once

#include "ReferenceSystem.h"

typedef struct Message Message;

AUTO_STRUCT;
typedef struct NWOptionsGroup
{
	const char *pchName;

	REF_TO(Message) hDisplayName; AST(NAME(DisplayName))
} NWOptionsGroup;

#include "AutoGen/NNOOptions_h_ast.h"
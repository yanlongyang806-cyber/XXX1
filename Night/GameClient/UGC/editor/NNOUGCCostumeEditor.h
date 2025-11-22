#pragma once
GCC_SYSTEM

#include "CombatEnums.h"
#include "CostumeCommon.h"
#include "NNOUGCCommon.h"

typedef enum UGCActionID UGCActionID;
typedef struct CostumeEditLine CostumeEditLine;
typedef struct PlayerCostume PlayerCostume;
typedef struct UGCCostume UGCCostume;
typedef struct UGCCostumeEditorDoc UGCCostumeEditorDoc;
typedef struct UGCCostumeLineData UGCCostumeLineData;
typedef struct UGCUIAnimatedResourcePreview UGCUIAnimatedResourcePreview;
typedef struct UIMenu UIMenu;
typedef struct UIPane UIPane;
typedef struct UISlider UISlider;
typedef struct UITab UITab;
typedef struct UITabGroup UITabGroup;
typedef struct UIWindow UIWindow;
typedef struct WLCostume WLCostume;

typedef struct UGCCostumeColorData
{
	UGCCostumeEditorDoc* pDoc;
	UGCCostumeLineData* pParent;
	int iColorNum;
} UGCCostumeColorData;

typedef struct UGCCostumeLineData
{
	UGCCostumeEditorDoc* pDoc;
	CostumeEditLine *pLine;

	int iHoverRow;
	int iHoverColorIndex;
	Vec4 vHoverColor;

	UGCCostumeColorData colors[4];
} UGCCostumeLineData;

AUTO_STRUCT;
typedef struct UGCCostumeBasics
{
	REF_TO(SpeciesDef) hSpecies;	AST(NAME("Species"))
	REF_TO(SpeciesDef) hGender;		AST(NAME("Gender"))
	REF_TO(AllegianceDef) hAllegiance; AST(NAME("Allegiance"))
} UGCCostumeBasics;
extern ParseTable parse_UGCCostumeBasics[];
#define TYPE_parse_UGCCostumeBasics UGCCostumeBasics

typedef struct UGCCostumeEditorDoc
{
	const char *astrName;

	// Widget tree
	UIPane* pRootPane;
	UGCUIAnimatedResourcePreview* pCostumePreview;
	UIPane* pSidePane;

	// Components using this costume
	UGCComponent** eaComponentsUsingCostume;
	UIMenu* pComponentsUsingCostumeMenu;

	// Models used
	SpeciesDef** eaSpecies;
	SpeciesDef** eaGenders;
	AllegianceDef** eaAllegiance;
	PCStanceInfo** eaStances;
	const char** eaSlots[ 20 ];

	// Other misc widgets
	int activeRegionTabIndex;
	UGCCostumeBasics costumeBasics;
	PCRegion** eaCostumeRegions;
	UIWindow* pGlobalPropertiesWindow;
	
	UGCCostume *pUGCCostume;
	UGCCostumeOverride hoverData;

	// Preview data
	UGCCostume* pPreviewUGCCostume;
	UGCCostumeOverride previewHoverData;
	PlayerCostume *pPreviewCostume;
	bool bPreviewResetCamera;
	
	bool ignoreUpdates;

	// MJF (Aug/13/2012): The CharCreator-mode costume editor is
	// extremely slow, mainly due to costumeTailor_BoneHasValidGeo
	// being slow and called everywhere.  This prevents refreshing the
	// UI unnecessarily.
	//
	// NOTE: This assumes that the UGCCostume isn't ever changed
	// outside of this editor.  This is true for CharCreator mode.
	PlayerCostume* pCharCreatorLastRefreshCostume;
} UGCCostumeEditorDoc;

// This doc is shown if there are no costumes.
//
// Yeah, the name is hideous.  The me from now wants to hit the me
// from three years ago, when the naming convention for docs was
// created.
typedef struct UGCNoCostumesEditorDoc
{
	UIPane* pRootPane;
} UGCNoCostumesEditorDoc;

UGCCostumeEditorDoc *ugcCostumeEditor_Open(UGCCostume *pUGCCostume);
void ugcCostumeEditor_UpdateUI(UGCCostumeEditorDoc *pDoc);
void ugcCostumeEditor_SetVisible(UGCCostumeEditorDoc *pDoc);
void ugcCostumeEditor_OncePerFrame(UGCCostumeEditorDoc *pDoc);
void ugcCostumeEditor_Close(UGCCostumeEditorDoc *pDoc);
void ugcCostumeEditor_HandleAction(UGCCostumeEditorDoc *pDoc, UGCActionID action);
bool ugcCostumeEditor_QueryAction(UGCCostumeEditorDoc *pDoc, UGCActionID action, char** out_estr);

UGCCostume *ugcCostumeEditor_GetCostume(UGCCostumeEditorDoc *pDoc);

UGCNoCostumesEditorDoc *ugcNoCostumesEditor_Open( void );
void ugcNoCostumesEditor_UpdateUI(UGCNoCostumesEditorDoc *pDoc);
void ugcNoCostumesEditor_SetVisible(UGCNoCostumesEditorDoc *pDoc);
void ugcNoCostumesEditor_OncePerFrame(UGCNoCostumesEditorDoc *pDoc);
void ugcNoCostumesEditor_Close(UGCNoCostumesEditorDoc **ppDoc);
void ugcNoCostumesEditor_HandleAction(UGCNoCostumesEditorDoc *pDoc, UGCActionID action);
bool ugcNoCostumesEditor_QueryAction(UGCNoCostumesEditorDoc *pDoc, UGCActionID action, char** out_estr);

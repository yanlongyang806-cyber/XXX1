#include "NNOUGCCostumeEditor.h"

#include "Allegiance.h"
#include "CharacterCreationUI.h"
#include "CombatEnums.h"
#include "CostumeCommon.h"
#include "CostumeCommonGenerate.h"
#include "CostumeCommonLoad.h"
#include "CostumeCommonRandom.h"
#include "EditorManagerUI.h"
#include "GameClientLib.h"
#include "GfxSprite.h"
#include "MultiEditField.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "NNOUGCUIAnimatedResourcePreview.h"
#include "Prefs.h"
#include "ResourceSearch.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UILib.h"
#include "UITextureAssembly.h"
#include "gclCommandParse.h"
#include "gclCostumeCameraUI.h"
#include "gclCostumeLineUI.h"
#include "gclCostumeOnly.h"
#include "gclCostumeUI.h"
#include "gclCostumeUIState.h"
#include "gclCostumeUtil.h"
#include "gclCostumeView.h"
#include "gclEntity.h"
#include "gfxHeadshot.h"
#include "inputMouse.h"
#include "species_common.h"
#include "wlUGC.h"

#include "Allegiance_h_ast.h"
#include "CostumeCommon_h_ast.h"
#include "gclCostumeLineUI_h_ast.h"
#include "NNOUGCCostumeEditor_h_ast.h"



AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

static bool s_bForceCostumeRefresh = false;

typedef struct UGCCostumeSlotUI {
	UGCCostumeEditorDoc* pDoc;
	UGCCostumeSlotDef* slotDef;
} UGCCostumeSlotUI;

typedef struct UGCCostumeSlotColorUI {
	UGCCostumeEditorDoc* pDoc;
	UGCCostumeSlotDef* slotDef;
	int colorIndex;
} UGCCostumeSlotColorUI;

typedef struct UGCCostumePartUI {
	UGCCostumeEditorDoc* pDoc;
	UGCCostumePart* part;
} UGCCostumePartUI;

typedef struct UGCCostumePartColorUI {
	UGCCostumeEditorDoc* pDoc;
	UGCCostumePart* part;
	int colorIndex;
	bool colorIsSkin;
} UGCCostumePartColorUI;

/// Forward declarations
static void ugcCostumeEditor_DictionaryUpdate(enumResourceEventType eType, const char *pDictName, const char *pRefData, Referent pReferent, void *pUserData);
static void ugcCostumeEditor_RegionSetTabCB(UIButton* button, UGCCostumeEditorDoc* pDoc);
static void ugcCostumeEditor_FieldChangeCB(MEField *pField, bool bFinished, UGCCostumeEditorDoc *pDoc);
static void ugcCostumeEditor_CostumeBasicsChangeCB(MEField *pField, bool bFinished, UGCCostumeEditorDoc *pDoc);
static void ugcCostumeEditor_MetadataChangeCB(MEField *pField, bool bFinished, UGCCostumeEditorDoc *pDoc);
static void ugcCostumeEditor_GotoUsageCB(UIButton* ignored, UGCCostumeEditorDoc* pDoc);
static void ugcCostumeEditor_EditComponentCB( UIWidget* ignored, UGCComponent* component );
static void ugcCostumeEditor_RefreshRegionTab(UGCCostumeEditorDoc *pDoc, PCRegion* pRegion);
static void ugcCostumeEditor_RefreshBasics(UGCCostumeEditorDoc *pDoc);
static void ugcCostumeEditor_RefreshTitleBar(UGCCostumeEditorDoc *pDoc);
static void ugcCostumeEditor_RefreshSidePane(UGCCostumeEditorDoc *pDoc);

static SpeciesDef* ugcCostumeEditorCharCreator_FindMatchingSpecies(SpeciesDef **eaSpecies, SpeciesDef *pSpecies, bool bReturnDefault);
static SpeciesDef* ugcCostumeEditorCharCreator_FindMatchingGender(SpeciesDef **eaGenders, SpeciesDef *pGender);
static void ugcCostumeEditorCharCreator_FillAllegianceList(AllegianceDef ***peaAllegianceList);
static void ugcCostumeEditorCharCreator_LineChangeCB( MEField *pField, bool bFinished, UGCCostumeLineData* pData );
static void ugcCostumeEditorCharCreator_LineHoverCB( UIComboBox *pCombo, S32 iRow, UGCCostumeLineData* pData );
static void ugcCostumeEditorCharCreator_ColorChangeCB(UIColorCombo *pCombo, UGCCostumeColorData* pData);
static void ugcCostumeEditorCharCreator_ColorHoverCB(UIColorCombo *pCombo, bool bHover, Vec4 vColor, UGCCostumeColorData* pData);
static void ugcCostumeEditorCharCreator_RandomizeRegion( UGCCostumeEditorDoc* pDoc, PCRegion* pRegion );
static void ugcCostumeEditorCharCreator_RefreshLine(UGCCostumeEditorDoc *pDoc, PCRegion* pRegion, int index, CostumeEditLine *pLine, bool bIsSpace, bool prevTypeIsGeometry);

static void ugcCostumeEditorNeverwinter_SetBodyScale( UGCCostumeEditorDoc* pDoc, const char* name, float value );
static void ugcCostumeEditorNeverwinter_SetScale( UGCCostumeEditorDoc* pDoc, const char* name, float value );
static UGCCostumeScale* ugcCostumeEditorNeverwinter_GetScale( UGCCostumeEditorDoc* pDoc, const char* name );
static void ugcCostumeEditorNeverwinter_PartColorChangedCB( UIColorCombo *pCombo, UGCCostumePartColorUI* colorUI );
static void ugcCostumeEditorNeverwinter_PartColorHoverCB( UIColorCombo *pCombo, bool bHover, Vec4 vColor, UGCCostumePartColorUI* colorUI );
static void ugcCostumeEditorNeverwinter_GeometryHoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI );
static void ugcCostumeEditorNeverwinter_MaterialHoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI );
static void ugcCostumeEditorNeverwinter_Texture0HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI );
static void ugcCostumeEditorNeverwinter_Texture1HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI );
static void ugcCostumeEditorNeverwinter_Texture2HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI );
static void ugcCostumeEditorNeverwinter_Texture3HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI );
static void ugcCostumeEditorNeverwinter_RevertToPresetCB( UIWidget* ignored, UGCCostumeEditorDoc* pDoc );
static void ugcCostumeEditorNeverwinter_UpdateSlotCostumes( const char*** out_peaCostumes, const char* astrSlotName );
static void ugcCostumeEditorNeverwinter_SlotChangedCB( MEField *pField, bool bFinished, UGCCostumeSlotUI* slotUI );
static void ugcCostumeEditorNeverwinter_SlotHoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumeSlotUI* slotUI );
static void ugcCostumeEditorNeverwinter_SlotRevertCB( UIWidget* ignored, UGCCostumeSlotUI* slotUI );
static void ugcCostumeEditorNeverwinter_SlotColorChangedCB( UIColorCombo *pCombo, UGCCostumeSlotColorUI* slotUI );
static void ugcCostumeEditorNeverwinter_SlotColorHoverCB( UIColorCombo *pCombo, bool bHover, Vec4 vColor, UGCCostumeSlotColorUI* slotUI );
static void ugcCostumeEditorNeverwinter_RefreshSlotTab( UGCCostumeEditorDoc *pDoc );
static void ugcCostumeEditorNeverwinter_RefreshPartColor( UGCCostumeEditorDoc* pDoc, CostumeEditLine* pData, int lineIt, int colorIdx, int colorIt, bool onRight, bool isSkin );
static void ugcCostumeEditorNeverwinter_ToggleAdvancedMode( UIButton* ignored, UGCCostumeEditorDoc* pDoc );

bool bUGCCostumeEditorEnableEarlyOut = true;
AUTO_CMD_INT( bUGCCostumeEditorEnableEarlyOut, ugc_CostumeEditorEnableEarlyOut );

void ugcCostumeEditor_DictionaryUpdate(enumResourceEventType eType, const char *pDictName, const char *pRefData, Referent pReferent, void *pUserData)
{
	if (eType != RESEVENT_NO_REFERENCES) {
		ugcEditorQueueUIUpdate();
	}
}

void ugcCostumeEditor_RegionSetTabCB(UIButton* button, UGCCostumeEditorDoc* pDoc)
{
	int index = UI_WIDGET( button )->u64;
	pDoc->activeRegionTabIndex = index;
	ugcEditorQueueUIUpdate();
}

void ugcCostumeEditor_FieldChangeCB(MEField *pField, bool bFinished, UGCCostumeEditorDoc *pDoc)
{
	if( pDoc->ignoreUpdates ) {
		return;
	}
	
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	// Clear all the cached headshots that may have been made of these
	emClearPreviews();

	if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR ) {
		StructDestroySafe( parse_PlayerCostume, &pDoc->pCharCreatorLastRefreshCostume );
	}
	
	ugcEditorQueueApplyUpdate();
}


void ugcCostumeEditor_CostumeBasicsChangeCB(MEField *pField, bool bFinished, UGCCostumeEditorDoc *pDoc)
{
	if( !bFinished || pDoc->ignoreUpdates ) {
		return;
	}
	

	if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR ) {
		PlayerCostume *pCostume = pDoc->pUGCCostume->pPlayerCostume;
	
		// Check if allegiance changed
		{
			AllegianceDef *pAllegiance = GET_REF( pDoc->costumeBasics.hAllegiance );
			if( pAllegiance != GET_REF( pDoc->pUGCCostume->hAllegiance )) {
				SpeciesDef *pSpecies = GET_REF( pDoc->costumeBasics.hSpecies );
	
				SET_HANDLE_FROM_REFERENT( g_hAllegianceDict, pAllegiance, pDoc->pUGCCostume->hAllegiance );
		
				// Reset the species based on allegiance
				if( pSpecies && pAllegiance ) {
					bool bIsSpace = (pDoc->pUGCCostume->eRegion == (U32)StaticDefineIntGetIntDefault( CharClassTypesEnum, "Space", -1 ));
					CharacterCreation_FillSpeciesList( &pDoc->eaSpecies, pAllegiance->pcName, bIsSpace, true );
					pSpecies = ugcCostumeEditorCharCreator_FindMatchingSpecies( pDoc->eaSpecies, GET_REF( pDoc->costumeBasics.hSpecies ), IS_HANDLE_ACTIVE( pDoc->costumeBasics.hSpecies ));
					SET_HANDLE_FROM_REFERENT( "SpeciesDef", pSpecies, pDoc->costumeBasics.hSpecies );
				}
			}
		}

		// Check if species changed
		{
			SpeciesDef *pSpecies = GET_REF(pDoc->costumeBasics.hSpecies);
			if (pSpecies) {
				bool bIsSpace = (pDoc->pUGCCostume->eRegion == (U32)StaticDefineIntGetIntDefault(CharClassTypesEnum, "Space", -1));
				if (!bIsSpace) {
					CharacterCreation_FillGenderList(&pDoc->eaGenders, pSpecies, true);
					pSpecies = ugcCostumeEditorCharCreator_FindMatchingGender(pDoc->eaGenders, GET_REF(pDoc->costumeBasics.hGender));
				}
				SET_HANDLE_FROM_REFERENT("SpeciesDef", pSpecies, pDoc->costumeBasics.hGender);
			}
		}

		// Check if gender changed
		{
			SpeciesDef *pSpecies = GET_REF(pDoc->costumeBasics.hGender);

			if (pSpecies != GET_REF(pCostume->hSpecies)) {
				// Copy gender species choice to costume
				SET_HANDLE_FROM_REFERENT("SpeciesDef", pSpecies, pCostume->hSpecies);

				// Reset the skeleton to match the species
				if (pSpecies) {
					SET_HANDLE_FROM_REFERENT("PCSkeletonDef", GET_REF(pSpecies->hSkeleton), pCostume->hSkeleton);
				}
			}
		}
	}

	ugcCostumeEditor_FieldChangeCB( pField, bFinished, pDoc );
}


void ugcCostumeEditor_MetadataChangeCB(MEField *pField, bool bFinished, UGCCostumeEditorDoc *pDoc)
{
	char *estrTrimmedName = NULL;

	if (!bFinished || pDoc->ignoreUpdates ) {
		return;
	}

	ugcEditorUserResourceChanged();
	ugcCostumeEditor_FieldChangeCB(pField, bFinished, pDoc);
}

void ugcCostumeEditor_GotoUsageCB(UIButton* ignored, UGCCostumeEditorDoc* pDoc)
{
	ui_MenuPopupAtCursorOrWidgetBox( pDoc->pComponentsUsingCostumeMenu );
}

void ugcCostumeEditor_EditComponentCB( UIWidget* ignored, UGCComponent* component )
{
	if( component && !component->sPlacement.bIsExternalPlacement ) {
		ugcEditorEditMapComponent( component->sPlacement.pcMapName, component->uID, false, false );
	}
}

void ugcCostumeEditor_RefreshTitleBar(UGCCostumeEditorDoc *pDoc)
{
	MEFieldContextEntry* pEntry;
	UIWidget* pWidget;

	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	MEFieldContext* uiCtx = MEContextPush( "UGCCostumeEditor_TitleBar", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( pDoc->pRootPane ));
	uiCtx->iXDataStart = 80;
	uiCtx->iXPos = 15;
	uiCtx->iRightPad = 0;

	{
		float x = 10;
		float y = 6;

		pEntry = MEContextAddButton( pDoc->pUGCCostume->pcDisplayName, NULL, ugcEditorPopupChooserCostumesCB, NULL, "TitleButton", NULL, NULL );
		pWidget = UI_WIDGET( ENTRY_BUTTON( pEntry ));
		SET_HANDLE_FROM_STRING( "UISkin", "UGCComboButton", pWidget->hOverrideSkin );
		ui_WidgetSetPosition( pWidget, x, y );
		ui_ButtonResize( ENTRY_BUTTON( pEntry ));
		MIN1( UI_WIDGET( ENTRY_BUTTON( pEntry ))->width, 250 );
		ui_WidgetSetHeight( pWidget, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( pWidget );

		pEntry = ugcMEContextAddEditorButton( UGC_ACTION_COSTUME_EDIT_NAME, false, false );
		pWidget = UI_WIDGET( ENTRY_BUTTON( pEntry ));
		ui_WidgetSetPosition( pWidget, x, y );
		ui_WidgetSetHeight( pWidget, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( pWidget );

		pEntry = MEContextAddButtonMsg( "UGC_CostumeEditor.Links", NULL, ugcCostumeEditor_GotoUsageCB, pDoc, "UsedInLink", NULL, "UGC_CostumeEditor.Links.Tooltip" );
		pWidget = UI_WIDGET( ENTRY_BUTTON( pEntry ));
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Hyperlink", pWidget->hOverrideSkin );
		ui_ButtonResize( ENTRY_BUTTON( pEntry ));
		ui_WidgetSetPosition( pWidget, x, y );
		ui_WidgetSetHeight( pWidget, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( pWidget );

		x = 4;
		
		pEntry = ugcMEContextAddEditorButton( UGC_ACTION_COSTUME_DELETE, true, true );
		pWidget = UI_WIDGET( ENTRY_BUTTON( pEntry ));
		ui_WidgetSetPositionEx( pWidget, x, y, 0, 0, UITopRight );
		ui_WidgetSetDimensions( pWidget, UGC_ROW_HEIGHT*1.5-12, UGC_ROW_HEIGHT*1.5-12 );
		x = ui_WidgetGetNextX( pWidget ) + 5;

		pEntry = ugcMEContextAddEditorButton( UGC_ACTION_COSTUME_DUPLICATE, true, false );
		pWidget = UI_WIDGET( ENTRY_BUTTON( pEntry ));
		ui_WidgetSetPositionEx( pWidget, x, y, 0, 0, UITopRight );
		ui_ButtonResize( ENTRY_BUTTON( pEntry ));
		ui_WidgetSetHeight( pWidget, UGC_ROW_HEIGHT*1.5-12 );
	}
	
	MEContextPop( "UGCCostumeEditor_TitleBar" );
}

static int ugcCostumeEditor_AddRegionUIForMode( UGCCostumeEditorDoc* pDoc, PCRegion* pRegion, UGCCostumeNWRegionModeDef* modeDef )
{
	PlayerCostume* baseCostume = ugcResourceGetCostumeMetadata( pDoc->pUGCCostume->data.astrPresetCostumeName )->pFullCostume;
	PCSkeletonDef* pSkel = GET_REF( baseCostume->hSkeleton );
	int accum = 0;

	assert( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_NEVERWINTER );

	if( !modeDef ) {
		return accum;
	}

	// Display Costume Parts
	{
		PCRegionRef** eaRegionRefs = NULL;
		PCRegionRef singleRegionRef = { 0 };
		int partIt;
		SET_HANDLE_FROM_REFERENT( "PCRegion", pRegion, singleRegionRef.hRegion );
		eaPush( &eaRegionRefs, &singleRegionRef );

		for( partIt = 0; partIt != eaSize( &modeDef->eaParts ); ++partIt ) {
			UGCCostumeNWPartDef* partDef = modeDef->eaParts[ partIt ];
			CostumeEditLineType eFindTypes = 0;

			if( partDef->enableGeometry ) {
				eFindTypes |= kCostumeEditLineType_Geometry;
			}
			if( partDef->enableMaterial ) {
				eFindTypes |= kCostumeEditLineType_Material;
			}
			if( partDef->enableTextures ) {
				eFindTypes |= (kCostumeEditLineType_Texture0 | kCostumeEditLineType_Texture1
							   | kCostumeEditLineType_Texture2 | kCostumeEditLineType_Texture3);
			}
			if( eFindTypes ) {
				UGCCostumePart* part = ugcCostumeFindPart( pDoc->pUGCCostume, partDef->astrName );
				PlayerCostume* currentCostume = ugcCostumeGeneratePlayerCostume( pDoc->pUGCCostume, NULL, NULL );
				const char** eaBonesInclude = NULL;
				int lineIt;
				CostumeEditLine** eaCostumeEditLines = NULL;
				char buffer[ 256 ];

				if( !part ) {
					continue;
				}

				sprintf( buffer, "Line_%s", partDef->astrName );
				eaPush( &eaBonesInclude, partDef->astrName );
				costumeLineUI_UpdateLines(
						CONTAINER_NOCONST( PlayerCostume, currentCostume ), &eaCostumeEditLines, 
						NULL, pSkel, eFindTypes, kCostumeUIBodyScaleRule_Disabled, &eaRegionRefs,
						NULL, NULL, NULL, eaBonesInclude, NULL, NULL /* pSlotType */, NULL /* pcCostumeSet */,
						false /*bLineListHideMirrorBones*/ , true /*bUnlockAll*/, true /*bMirrorSelectMode*/, true /*bGroupSelectMode*/, true /*bCountNone*/, false /*bOmitHasOnlyOne*/, false /*bCombineLines*/, true /*bTextureLinesForCurrentPartValuesOnly*/,
						NULL /*eaUnlockedCostumes*/, NULL /*eaPowerFXBones*/);

				{
					MEFieldContext* partCtx = MEContextPush( allocAddString( buffer ), part, part, parse_UGCCostumePart );
					int partCtxRightPad = partCtx->iRightPad;
					for( lineIt = 0; lineIt != eaSize( &eaCostumeEditLines ); ++lineIt ) {
						CostumeEditLine* pLine = eaCostumeEditLines[ lineIt ];
						UGCCostumePartUI* partUI = MEContextAllocMem( "Data", sizeof( *partUI ), NULL, false );
						int numColors = 0;
						MEFieldContextEntry* entry;
	
						partUI->pDoc = pDoc;
						partUI->part = part;

						if( partDef->enableColors ) {
							if( pLine->bColor0Allowed ) {
								++numColors;
							}
							if( pLine->bColor1Allowed ) {
								++numColors;
							}
							if( pLine->bColor2Allowed ) {
								++numColors;
							}
							if( pLine->bColor3Allowed ) {
								++numColors;
							}
						}
						if( numColors == 1 ) {
							partCtx->iRightPad = partCtxRightPad + 25;
						} else {
							partCtx->iRightPad = partCtxRightPad;
						}
						
						switch( pLine->iType ) {
							xcase kCostumeEditLineType_Geometry: {
								entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCGeometryDef, &pLine->eaGeo, "Name",
																  "Geometry", TranslateDisplayMessage( pLine->displayNameMsg ), NULL );
								MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
								ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_GeometryHoverCB, partUI );
								ui_ComboBoxSetDefaultDisplayString( ENTRY_FIELD( entry )->pUICombo, TranslateMessageKey( "UGC_CostumeEditor.FromPreset" ));
								++accum;
							}
							xcase kCostumeEditLineType_Material: {
								entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCMaterialDef, &pLine->eaMat, "Name",
																  "Material", TranslateDisplayMessage( pLine->displayNameMsg ), NULL );
								MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
								ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_MaterialHoverCB, partUI );
								ui_ComboBoxSetDefaultDisplayString( ENTRY_FIELD( entry )->pUICombo, TranslateMessageKey( "UGC_CostumeEditor.FromPreset" ));
								++accum;
							}
							xcase kCostumeEditLineType_Texture0: {
								entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCTextureDef, &pLine->eaTex, "Name",
																  "Texture0", TranslateDisplayMessage( pLine->displayNameMsg ), NULL );
								MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
								ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_Texture0HoverCB, partUI );
								ui_ComboBoxSetDefaultDisplayString( ENTRY_FIELD( entry )->pUICombo, TranslateMessageKey( "UGC_CostumeEditor.FromPreset" ));
								++accum;
							}
							xcase kCostumeEditLineType_Texture1: {
								entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCTextureDef, &pLine->eaTex, "Name",
																  "Texture1", TranslateDisplayMessage( pLine->displayNameMsg ), NULL );
								MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
								ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_Texture1HoverCB, partUI );
								ui_ComboBoxSetDefaultDisplayString( ENTRY_FIELD( entry )->pUICombo, TranslateMessageKey( "UGC_CostumeEditor.FromPreset" ));
								++accum;
							}
							xcase kCostumeEditLineType_Texture2: {
								entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCTextureDef, &pLine->eaTex, "Name",
																  "Texture2", TranslateDisplayMessage( pLine->displayNameMsg ), NULL );
								MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
								ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_Texture2HoverCB, partUI );
								ui_ComboBoxSetDefaultDisplayString( ENTRY_FIELD( entry )->pUICombo, TranslateMessageKey( "UGC_CostumeEditor.FromPreset" ));
								++accum;
							}
							xcase kCostumeEditLineType_Texture3: {
								entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCTextureDef, &pLine->eaTex, "Name",
																  "Texture3", TranslateDisplayMessage( pLine->displayNameMsg ), NULL );
								MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
								ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_Texture3HoverCB, partUI );
								ui_ComboBoxSetDefaultDisplayString( ENTRY_FIELD( entry )->pUICombo, TranslateMessageKey( "UGC_CostumeEditor.FromPreset" ));
								++accum;
							}
						}

						if( numColors ) {
							int colorIt = 0;
							bool colorsOnRight = (numColors == 1);
							PCMaterialDef* ownerMat = GET_REF( pLine->hOwnerMat );

							if( colorsOnRight ) {
								MEContextStepBackUp();
							}
								
							if( pLine->bColor0Allowed ) {
								ugcCostumeEditorNeverwinter_RefreshPartColor( pDoc, pLine, lineIt, 0, colorIt, colorsOnRight, false );
								++colorIt;
							}
							if( pLine->bColor1Allowed ) {
								ugcCostumeEditorNeverwinter_RefreshPartColor( pDoc, pLine, lineIt, 1, colorIt, colorsOnRight, false );
								++colorIt;
							}
							if( pLine->bColor2Allowed ) {
								ugcCostumeEditorNeverwinter_RefreshPartColor( pDoc, pLine, lineIt, 2, colorIt, colorsOnRight, false );
								++colorIt;
							}
							if( pLine->bColor3Allowed ) {
								// MJF: costumeLineUI_UpdateLines() has a horrible hack for
								// editing skin color.  It stuffs the skin color into a line as
								// color 3.  Silly costumeLineUI_UpdateLines().
								bool isSkinColor = SAFE_MEMBER( ownerMat, bHasSkin );
									
								ugcCostumeEditorNeverwinter_RefreshPartColor( pDoc, pLine, lineIt, 3, colorIt, colorsOnRight, isSkinColor );
								++colorIt;
							}

							if( colorsOnRight ) {
								MEContextStepDown();
							} else {
								MEContextAddCustomSpacer( UGC_ROW_HEIGHT );
							}
						}
					}
					MEContextPop( allocAddString( buffer ));
				}

				eaDestroy( &eaCostumeEditLines );
				eaDestroy( &eaBonesInclude );
				StructDestroy( parse_PlayerCostume, currentCostume );
			}
		}

		REMOVE_HANDLE( singleRegionRef.hRegion );
		eaDestroy( &eaRegionRefs );
	}

	// Display Body Scales
	{
		int bodyScaleIt;
		for( bodyScaleIt = 0; bodyScaleIt != eaSize( &pDoc->pUGCCostume->data.eaBodyScales ); ++bodyScaleIt ) {
			UGCCostumeScale* bodyScale = pDoc->pUGCCostume->data.eaBodyScales[ bodyScaleIt ];
			if(   eaFind( &modeDef->eaBodyScales, bodyScale->astrName ) >= 0 ) {
				int bodyScaleIndex = costumeTailor_FindBodyScaleInfoIndexByName( baseCostume, bodyScale->astrName );
				PCBodyScaleInfo* bodyScaleDef = pSkel->eaBodyScaleInfo[ bodyScaleIndex ];
				char buffer[ 256 ];
				sprintf( buffer, "BodyScale_%s", bodyScale->astrName );
				MEContextPush( allocAddString( buffer ), bodyScale, bodyScale, parse_UGCCostumeScale );
				MEContextAddMinMax( kMEFieldType_Slider, 0, 100, 1, "Value", TranslateDisplayMessage( bodyScaleDef->displayNameMsg ), NULL );
				MEContextPop( allocAddString( buffer ));
				++accum;
			}
		}
	}

	// Display scales groups (from ScaleGroups)
	{
		int scaleIt;
		for( scaleIt = 0; scaleIt != eaSize( &modeDef->eaScaleInfos ); ++scaleIt ) {
			const char* scaleName = modeDef->eaScaleInfos[ scaleIt ];
			const PCScaleInfo* scaleDef = costumeTailor_FindScaleInfoByName( pSkel, scaleName );
			UGCCostumeScale* scale = ugcCostumeEditorNeverwinter_GetScale( pDoc, scaleDef->pcName );

			if( scale && scaleDef ) {
				char buffer[ 256 ];
				const char* displayMessage = TranslateDisplayMessage( scaleDef->displayNameMsg );
				char displayMessageBuffer[ 256 ];
				sprintf( buffer, "Scale_%s", scale->astrName );
				MEContextPush( allocAddString( buffer ), scale, scale, parse_UGCCostumeScale );

				if( displayMessage ) {
					strcpy( displayMessageBuffer, displayMessage );
				} else {
					sprintf( displayMessageBuffer, "%s [UNTRANSLATED]", scaleName );
				}
				MEContextAddMinMax( kMEFieldType_Slider, -100, +100, 1, "Value", displayMessageBuffer, NULL );
				MEContextPop( allocAddString( buffer ));
				++accum;
			}
		}
	}

	return accum;
}


void ugcCostumeEditor_RefreshRegionTab(UGCCostumeEditorDoc *pDoc, PCRegion* pRegion)
{
	bool bIsSpace = (pDoc->pUGCCostume->eRegion == (U32)StaticDefineIntGetIntDefault(CharClassTypesEnum, "Space", -1));
	MEFieldContext* uiCtx;
	char contextName[ 256 ];
	
	if (!pRegion) {
		return;
	}

	sprintf( contextName, "UGCCostumeEditor_Region%sTab", pRegion->pcName );
	uiCtx = MEContextPush( allocAddString( contextName ), NULL, NULL, NULL );
	uiCtx->iXLabelStart = 0;
	uiCtx->iYLabelStart = 0;
	uiCtx->bLabelPaddingFromData = false;
	uiCtx->iXDataStart = 20;
	uiCtx->iYDataStart = UGC_ROW_HEIGHT - 10;
	uiCtx->iYStep = UGC_ROW_HEIGHT * 2 - 10;
	MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
	setVec2( uiCtx->iErrorIconOffset, 0, UGC_ROW_HEIGHT - 10 + 3 );
	uiCtx->bErrorIconOffsetFromRight = false;
	uiCtx->iErrorIconSpaceWidth = 0;

	{
		UIScrollArea* area = MEContextPushScrollAreaParent( "ScrollParent" );
		ui_WidgetSetDimensionsEx( UI_WIDGET( area ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		area->autosize = true;
	}

	switch( ugcDefaultsCostumeEditorStyle() ) {
		xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR: {
			PlayerCostume *pCostume = pDoc->pUGCCostume->pPlayerCostume;
			UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( pRegion->pcName );
			CostumeUIScaleGroup** eaScaleGroups = NULL;
			PCBodyScaleInfo** eaBodyScalesInclude = NULL;
			PCBodyScaleInfo** eaBodyScalesExclude = NULL;
			CostumeEditLine **eaCostumeEditLines = NULL;
			CostumeEditLineType eFindTypes = (kCostumeEditLineType_Category | kCostumeEditLineType_Geometry
											  | kCostumeEditLineType_Material | kCostumeEditLineType_Texture0
											  | kCostumeEditLineType_Texture1 | kCostumeEditLineType_Texture2
											  | kCostumeEditLineType_Texture3 | kCostumeEditLineType_Texture4
											  | kCostumeEditLineType_BodyScale | kCostumeEditLineType_Scale);

			// Fill in the scale groups
			if( regionDef ) {
				PCSkeletonDef *pSkel = GET_REF(pCostume->hSkeleton);
				int i;
				for( i = 0; i != eaSize(&regionDef->eaScaleGroups); ++i ) {
					CostumeUIScaleGroup* pRef = StructCreate(parse_CostumeUIScaleGroup);
					pRef->pcName = allocAddString( regionDef->eaScaleGroups[ i ]);
					eaPush(&eaScaleGroups, pRef);
				}
				for( i = 0; i != eaSize(&pSkel->eaBodyScaleInfo); ++i ) {
					const char* scaleName = pSkel->eaBodyScaleInfo[ i ]->pcName;
					if( eaFindString( &regionDef->eaBodyScalesInclude, scaleName ) >= 0 ) {
						eaPush(&eaBodyScalesInclude, pSkel->eaBodyScaleInfo[ i ]);
					}
					if( eaFindString( &regionDef->eaBodyScalesExclude, scaleName ) >= 0 ) {
						eaPush(&eaBodyScalesExclude, pSkel->eaBodyScaleInfo[ i ]);
					}
				}
			}

			// Set up the required data for refreshing the lines
			{
				PCRegionRef **eaRegionRefs = NULL;
				PCRegionRef singleRegionRef = { 0 };
				const char** eaBonesInclude = SAFE_MEMBER( regionDef, eaBonesInclude );
				const char** eaBonesExclude = SAFE_MEMBER( regionDef, eaBonesExclude );
				SET_HANDLE_FROM_REFERENT( "PCRegion", pRegion, singleRegionRef.hRegion );
				eaPush( &eaRegionRefs, &singleRegionRef );

				// Update the lines information
				costumeLineUI_UpdateLines(CONTAINER_NOCONST(PlayerCostume, pCostume), &eaCostumeEditLines, 
										  GET_REF(pCostume->hSpecies), GET_REF(pCostume->hSkeleton), 
										  eFindTypes, kCostumeUIBodyScaleRule_AfterRegions, &eaRegionRefs,
										  eaScaleGroups, eaBodyScalesInclude, eaBodyScalesExclude,
										  eaBonesInclude, eaBonesExclude, NULL /* pSlotType */, NULL /* pcCostumeSet */,
										  bIsSpace /*bLineListHideMirrorBones*/ , true /*bUnlockAll*/, true /*bMirrorSelectMode*/, true /*bGroupSelectMode*/, true /*bCountNone*/, false /*bOmitHasOnlyOne*/, false /*bCombineLines*/, true /*bTextureLinesForCurrentPartValuesOnly*/,
										  NULL /*eaUnlockedCostumes*/, NULL /*eaPowerFXBones*/);
				REMOVE_HANDLE( singleRegionRef.hRegion );
				eaDestroy( &eaRegionRefs );
			}
		
			// Refresh the line entries
			{
				bool prevLineIsGeometry = false;
				int i;

				for(i=0; i<eaSize(&eaCostumeEditLines); ++i) {
					CostumeEditLine *pLine = eaCostumeEditLines[i];

					ugcCostumeEditorCharCreator_RefreshLine( pDoc, pRegion, i, pLine, bIsSpace, prevLineIsGeometry );
					MEContextAddCustomSpacer( 4 );

					prevLineIsGeometry = (pLine->iType == kCostumeEditLineType_Geometry);
				}
			}

			// Destroy array, but not structures.  Those get kept on the LineGroup.
			eaDestroy(&eaCostumeEditLines);
			eaDestroyStruct(&eaScaleGroups, parse_CostumeUIScaleGroup);
			eaDestroy(&eaBodyScalesInclude);
			eaDestroy(&eaBodyScalesExclude);
		}

		xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER: {
			UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( pRegion->pcName );
			int lines = 0;

			if( regionDef ) {
				lines += ugcCostumeEditor_AddRegionUIForMode( pDoc, pRegion, &regionDef->nwBasic );
				if( pDoc->pUGCCostume->data.isAdvanced ) {
					lines += ugcCostumeEditor_AddRegionUIForMode( pDoc, pRegion, &regionDef->nwAdvanced );
				}

				if( !lines && !pDoc->pUGCCostume->data.isAdvanced ) {
					MEFieldContextEntry* entry = MEContextAddLabelMsg( "SwitchToAdvancedHelp", "UGC_CostumeEditor.AdvancedModeHasMoreOptions", NULL );
					ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
					ENTRY_LABEL( entry )->bWrap = true;
				}
			} else {
				MEFieldContextEntry* entry = MEContextAddLabelMsg( "NoDefHelp", "UGC_CostumeEditor.NoOptions", NULL );
				ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
				ENTRY_LABEL( entry )->bWrap = true;
			}
		}
	}

	MEContextPop( "ScrollParent" );
	MEContextPop( allocAddString( contextName ));
}


void ugcCostumeEditor_RefreshBasics(UGCCostumeEditorDoc *pDoc)
{
	UITextureAssembly* paneAssembly = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Header_Box" );
	bool bIsSpace = (pDoc->pUGCCostume->eRegion == (U32)StaticDefineIntGetIntDefault( CharClassTypesEnum, "Space", -1 ));
	MEFieldContext* basicsUICtx = MEContextPush( "UGCCostumeEditor_Basics", &pDoc->costumeBasics, &pDoc->costumeBasics, parse_UGCCostumeBasics );
	char* estr = NULL;
	UIPane* pane;
	basicsUICtx->cbChanged = ugcCostumeEditor_CostumeBasicsChangeCB;
	basicsUICtx->pChangedData = pDoc;
	
	// Refresh the UI!
	pane = ugcMEContextPushPaneParentWithHeaderMsg( "Properties", "Header", "UGC_CostumeEditor.Properties", false );
	{
		switch( ugcDefaultsCostumeEditorStyle() ) {
			xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR: {
				PlayerCostume *pCostume = pDoc->pUGCCostume->pPlayerCostume;
				AllegianceDef* pAllegiance = GET_REF( pDoc->pUGCCostume->hAllegiance );

				// Init key data
				SET_HANDLE_FROM_REFERENT(g_hAllegianceDict, GET_REF( pDoc->pUGCCostume->hAllegiance ), pDoc->costumeBasics.hAllegiance);
				CharacterCreation_FillSpeciesList( &pDoc->eaSpecies, pAllegiance ? pAllegiance->pcName : NULL, bIsSpace, true );
				CharacterCreation_FillGenderList( &pDoc->eaGenders, GET_REF( pCostume->hSpecies ), true );
				ugcCostumeEditorCharCreator_FillAllegianceList( &pDoc->eaAllegiance );
		
				costumeTailor_GetValidStances( CONTAINER_NOCONST( PlayerCostume, pCostume ), GET_REF( pCostume->hSpecies ), NULL, &pDoc->eaStances, true, NULL, true );
				SET_HANDLE_FROM_REFERENT( "SpeciesDef", ugcCostumeEditorCharCreator_FindMatchingSpecies( pDoc->eaSpecies, GET_REF( pCostume->hSpecies ), IS_HANDLE_ACTIVE( pCostume->hSpecies )), pDoc->costumeBasics.hSpecies );
				SET_HANDLE_FROM_REFERENT( "SpeciesDef", GET_REF( pCostume->hSpecies ), pDoc->costumeBasics.hGender );
		
				if( !bIsSpace && eaSize( &pDoc->eaAllegiance ) > 0 ) {
					MEFieldContextEntry* entry = MEContextAddDataProvided(
							kMEFieldType_Combo, parse_AllegianceDef, &pDoc->eaAllegiance, "Name",
							"Allegiance", "Allegiance", "Select the allegiance" );
					MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayName", true );
				}
	
				// Species control
				if( bIsSpace ) {
					MEFieldContextEntry* entry = MEContextAddDataProvided(
							kMEFieldType_Combo, parse_SpeciesDef, &pDoc->eaSpecies, "Name",
							"Species", "Ship Class", "Select the ship class" );
					MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
				} else {
					MEFieldContextEntry* speciesEntry = MEContextAddDataProvided(
							kMEFieldType_Combo, parse_SpeciesDef, &pDoc->eaSpecies, "Name",
							"Species", "Species", "Select the species" );
					MEFieldContextEntry* genderEntry = MEContextAddDataProvided(
							kMEFieldType_Combo, parse_SpeciesDef, &pDoc->eaGenders, "Name",
							"Gender", "Gender", "Select the gender" );
					MEFieldSetDictField( ENTRY_FIELD( speciesEntry ), "Name", "DisplayNameMsg", true );
					MEFieldSetDictField( ENTRY_FIELD( genderEntry ), "Name", "GenderNameMsg", true );
				}
			}
			
			xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER: {
				PlayerCostume* baseCostume = ugcResourceGetCostumeMetadata( pDoc->pUGCCostume->data.astrPresetCostumeName )->pFullCostume;
				MEFieldContextEntry* entry;
				UIWidget* widget;

				eaClear( &pDoc->eaStances );
				if( baseCostume ) {
					costumeTailor_GetValidStances( CONTAINER_NOCONST( PlayerCostume, baseCostume ), NULL, NULL, &pDoc->eaStances, true, NULL, true );
				}

				if( !pDoc->pUGCCostume->data.isAdvanced ) {
					entry = MEContextAddButtonMsg( "UGC_CostumeEditor.ToggleAdvanced_ToAdvanced", NULL, ugcCostumeEditorNeverwinter_ToggleAdvancedMode, pDoc, "ToggleAdvanced", NULL, "UGC_CostumeEditor.ToggleAdvanced_ToAdvanced.Tooltip" );
				} else {
					entry = MEContextAddButtonMsg( "UGC_CostumeEditor.ToggleAdvanced_ToSimple", NULL, ugcCostumeEditorNeverwinter_ToggleAdvancedMode, pDoc, "ToggleAdvanced", NULL, "UGC_CostumeEditor.ToggleAdvanced_ToSimple.Tooltip" );
				}
				ENTRY_BUTTON( entry )->widget.x = MEContextGetCurrent()->iXPos;

				{
					const WorldUGCProperties* props = ugcResourceGetUGCProperties( "PlayerCostume", pDoc->pUGCCostume->data.astrPresetCostumeName );
					if( props ) {
						ugcFormatMessageKey( &estr, "UGC_CostumeEditor.PresetLabel",
											 STRFMT_DISPLAYMESSAGE( "CostumeName", props->dVisibleName ),
											 STRFMT_END );
						entry = MEContextAddLabel( "PresetLabel", estr, NULL );
						widget = UI_WIDGET( ENTRY_LABEL( entry ));
						MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );
					}
				}
				entry = MEContextAddButtonMsg( "UGC_CostumeEditor.RevertToPreset", NULL, ugcCostumeEditorNeverwinter_RevertToPresetCB, pDoc, "Preset", NULL, "UGC_CostumeEditor.RevertToPreset.Tooltip" );
				ENTRY_BUTTON( entry )->widget.x = MEContextGetCurrent()->iXPos;
		
				MEContextStepDown();
			}
		}

		if( !bIsSpace ) {
			switch( ugcDefaultsCostumeEditorStyle() ) {
				xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR:
				MEContextPush( "CostumeData", pDoc->pUGCCostume->pPlayerCostume, pDoc->pUGCCostume->pPlayerCostume, parse_PlayerCostume );
				xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER:
				MEContextPush( "CostumeData", &pDoc->pUGCCostume->data, &pDoc->pUGCCostume->data, parse_UGCCostumeData );	
			}
		
			{
				MEFieldContextEntry* stanceEntry = MEContextAddDataProvidedMsg(
						kMEFieldType_Combo, parse_PCStanceInfo, &pDoc->eaStances, "Name",
						"Stance", "UGC_CostumeEditor.Stance", "UGC_CostumeEditor.Stance.Tooltip" );
				MEFieldContextEntry* heightEntry = MEContextAddSimpleMsg(
						kMEFieldType_Slider, "Height",
						"UGC_CostumeEditor.Height", "UGC_CostumeEditor.Height.Tooltip" );
				float heightMin = 0;
				float heightMax = 0;

				MEFieldSetDictField( ENTRY_FIELD( stanceEntry ), "Name", "DisplayNameMsg", true );

				switch( ugcDefaultsCostumeEditorStyle() ) {
					xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR: {
						SpeciesDef* pSpecies = GET_REF( pDoc->pUGCCostume->pPlayerCostume->hSpecies );
						heightMin = costumeTailor_GetOverrideHeightMin( pSpecies, NULL );
						heightMax = costumeTailor_GetOverrideHeightMax( pSpecies, NULL );

						if( !heightMax ) {
							PCSkeletonDef* pSkel = GET_REF( pDoc->pUGCCostume->pPlayerCostume->hSkeleton );
							heightMin = pSkel->fPlayerMinHeight;
							heightMax = pSkel->fPlayerMaxHeight;
						}
					}
					
					xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER: {
						PlayerCostume* baseCostume = ugcResourceGetCostumeMetadata( pDoc->pUGCCostume->data.astrPresetCostumeName )->pFullCostume;
						if( baseCostume && pDoc->pUGCCostume->data.isAdvanced ) {
							PCSkeletonDef* pSkel = GET_REF( baseCostume->hSkeleton );

							heightMin = pSkel->fPlayerMinHeight;
							heightMax = pSkel->fPlayerMaxHeight;
						}
					}
				}
			
				if( heightMax ) {
					ui_SetActive( UI_WIDGET( ENTRY_FIELD( heightEntry )->pUISlider ), true );
					ui_SliderSetRange( ENTRY_FIELD( heightEntry )->pUISlider, heightMin, heightMax, 0 );
					MEFieldRefreshFromData( ENTRY_FIELD( heightEntry ));
				} else {
					ui_SetActive( UI_WIDGET( ENTRY_FIELD( heightEntry )->pUISlider ), false );
				}
			}
			MEContextPop( "CostumeData" );
		}
	}
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( paneAssembly ), UIUnitPercentage, UIUnitFixed );
	MEContextPop( "Properties" );
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane )) + 10;
	ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 10, 10, 0, 0 );
	
	MEContextPop( "UGCCostumeEditor_Basics" );

	estrDestroy( &estr );
}


void ugcCostumeEditor_RefreshSidePane(UGCCostumeEditorDoc *pDoc)
{
	MEFieldContext* uiCtx = MEContextPush( "UGCCostumeEditor_SidePane", NULL, NULL, NULL );
	MEFieldContextEntry* entry;
	UIPane* pane;
	UIWidget* widget;
	UITextureAssembly* texas;
	
	MEContextSetParent( UI_WIDGET( pDoc->pSidePane ));
	uiCtx->iXPos = 0;
	uiCtx->iYPos = 0;
	uiCtx->iXDataStart = 80;
	uiCtx->iXLabelStart = 0;
	uiCtx->cbChanged = ugcCostumeEditor_FieldChangeCB;
	uiCtx->pChangedData = pDoc;

	ui_PaneSetTitleHeight( pDoc->pSidePane, 24 );
	entry = MEContextAddLabelMsg( "Header", "UGC_CostumeEditor.SidePane_Header", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_WidgetSetFont( widget, "UGC_Header_Alternate" );
	ui_WidgetSetPosition( widget, 4, 2 );
	ui_LabelResize( ENTRY_LABEL( entry ));
	uiCtx->iYPos = 24 + 5;

	// Refresh basic properties
	ugcCostumeEditor_RefreshBasics( pDoc );

	// Refresh tabs
	{
		int extraTabs = (ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_NEVERWINTER ? 1 : 0);
		UIWidget* headerWidget;
		int tabX = 0;
		int tabY = 0;
		int it;

		pane = MEContextPushPaneParent( "HeaderPane" );
		ui_PaneSetStyle( pane, "UGC_Pane_Light_Header_Box_Cover", true, false );
		MEContextGetCurrent()->astrOverrideSkinName = allocAddString( "UGCAssetLibrary" );
		for( it = 0; it < eaSize( &pDoc->eaCostumeRegions ) + extraTabs; ++it ) {
			if( it < extraTabs ) {
				entry = MEContextAddButtonIndexMsg( "UGC_CostumeEditor.Slots", NULL, ugcCostumeEditor_RegionSetTabCB, pDoc, "RegionTab", it, NULL, NULL );
			} else {
				PCRegion* pRegion = pDoc->eaCostumeRegions[ it - extraTabs ];
				entry = MEContextAddButtonIndexMsg( REF_STRING_FROM_HANDLE( pRegion->displayNameMsg.hMessage ), NULL, ugcCostumeEditor_RegionSetTabCB, pDoc, "RegionTab", it, NULL, NULL );
			}
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ENTRY_BUTTON( entry )->textOffsetFrom = UILeft;
			widget->u64 = it;
			if( it == pDoc->activeRegionTabIndex ) {
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Picker_Active", widget->hOverrideSkin );
			}

			if( tabX == 0 ) {
				ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopRight );
			} else {
				ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopLeft );
			}
			ui_WidgetSetWidth( widget, 120 );

			++tabX;
			if( tabX >= 2 ) {
				tabX = 0;
				tabY = ui_WidgetGetNextY( widget );
			}
		}

		if( it % 2 != 0 ) {
			entry = MEContextAddButtonIndex( NULL, NULL, NULL, NULL, "RegionTabPlaceholder", it, NULL, NULL );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_SetActive( widget, false );

			ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopLeft );
			ui_WidgetSetWidth( widget, 120 );
			tabY = ui_WidgetGetNextY( widget );
		}
		MEContextPop( "HeaderPane" );
		ui_WidgetSetDimensions( UI_WIDGET( pane ), 260, tabY );
		headerWidget = UI_WIDGET( pane );

		pane = MEContextPushPaneParent( "ContentPane" );
		MEContextGetCurrent()->iYPos = headerWidget->height / 2;
		
		if( pDoc->activeRegionTabIndex < extraTabs ) {
			ugcCostumeEditorNeverwinter_RefreshSlotTab( pDoc );
		} else {
			ugcCostumeEditor_RefreshRegionTab( pDoc, pDoc->eaCostumeRegions[ pDoc->activeRegionTabIndex - extraTabs ]);
		}
		texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_Header_Box" );
		ui_PaneSetStyle( pane, texas->pchName, true, false );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 10, 10, 0, 0 );
		MEContextPop( "ContentPane" );

		ui_WidgetSetPosition( UI_WIDGET( pane ), 0, headerWidget->y + headerWidget->height / 2 );
		ui_WidgetSetPositionEx( headerWidget, 0, headerWidget->y, 0, 0, UITop );
		ui_WidgetGroupSteal( headerWidget->group, headerWidget );
	}

	MEContextPop( "UGCCostumeEditor_SidePane" );
}

void ugcCostumeEditor_UpdateUI(UGCCostumeEditorDoc *pDoc)
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	char contextName[ 256 ];

	// Early out for performance.  See the comment above pCharCreatorLastRefreshCostume.
	if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR && bUGCCostumeEditorEnableEarlyOut ) {
		if( pDoc->pCharCreatorLastRefreshCostume
			&& StructCompare( parse_PlayerCostume, pDoc->pUGCCostume->pPlayerCostume, pDoc->pCharCreatorLastRefreshCostume, 0, 0, 0 ) == 0 ) {
			return;
		}
	}
	
	sprintf( contextName, "UGCCostumeEditor_%s", pDoc->astrName );

	pDoc->ignoreUpdates = true;

	MEContextPush( contextName, NULL, NULL, NULL );
	{
		PlayerCostume* pCostume;
		SpeciesDef* pSpecies;

		switch( ugcDefaultsCostumeEditorStyle() ) {
			xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR:
				pCostume = pDoc->pUGCCostume->pPlayerCostume;
				pSpecies = GET_REF( pCostume->hSpecies );
				costumeTailor_FillAllBones(CONTAINER_NOCONST(PlayerCostume, pCostume), GET_REF(pCostume->hSpecies), NULL, NULL, true, false, true);
				costumeTailor_MakeCostumeValid(CONTAINER_NOCONST(PlayerCostume, pCostume), GET_REF(pCostume->hSpecies), NULL, NULL, true, true, false, NULL, true, NULL, false, NULL);
				
				StructDestroySafe( parse_PlayerCostume, &pDoc->pCharCreatorLastRefreshCostume );
				pDoc->pCharCreatorLastRefreshCostume = StructClone( parse_PlayerCostume, pCostume );
			xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER: {
				UGCCostumeMetadata* metadata = ugcResourceGetCostumeMetadata( pDoc->pUGCCostume->data.astrPresetCostumeName );
				pCostume = SAFE_MEMBER( metadata, pFullCostume );
				pSpecies = NULL;
			}
			xdefault:
				assert( 0 );
		}
		
		// Get the valid regions (needed for UI refresh)
		eaDestroy( &pDoc->eaCostumeRegions );
		if( pCostume ) {
			costumeTailor_GetValidRegions( CONTAINER_NOCONST( PlayerCostume, pCostume ), pSpecies, NULL, NULL, NULL, &pDoc->eaCostumeRegions, CGVF_UNLOCK_ALL );

			// Get rid of specific regions we know we don't want
			FOR_EACH_IN_EARRAY( pDoc->eaCostumeRegions, PCRegion, pRegion ) {
				UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( pRegion->pcName );
				if( !regionDef ) {
					eaRemove( &pDoc->eaCostumeRegions, FOR_EACH_IDX( pDoc->eaCostumeRegions, pRegion ));
				}
			} FOR_EACH_END;
		}

		// Get all the components using this costume (needed for UI refresh)
		{
			int it;
			eaClear( &pDoc->eaComponentsUsingCostume );
			if( !pDoc->pComponentsUsingCostumeMenu ) {
				pDoc->pComponentsUsingCostumeMenu = ui_MenuCreate( NULL );
			}
			ui_MenuClear( pDoc->pComponentsUsingCostumeMenu );
			
			for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
				UGCComponent* component = ugcProj->components->eaComponents[ it ];
				if( resNamespaceIsUGC( component->pcCostumeName ) && resNamespaceBaseNameEq( component->pcCostumeName, pDoc->astrName )) {
					char buffer[ 256 ];
					
					eaPush( &pDoc->eaComponentsUsingCostume, component );

					ugcComponentGetDisplayName( buffer, ugcProj, component, false );
					ui_MenuAppendItem( pDoc->pComponentsUsingCostumeMenu, ui_MenuItemCreate( buffer, UIMenuCallback, ugcCostumeEditor_EditComponentCB, component, NULL ));
				}
			}
		}

		ugcCostumeEditor_RefreshTitleBar(pDoc);
		ugcCostumeEditor_RefreshSidePane(pDoc);
	}
	MEContextPop( contextName );
		
	pDoc->ignoreUpdates = false;
}


// -------------------------------------------------------------------
// Graphics functions
// -------------------------------------------------------------------

void ugcCostumeEditor_SetVisible(UGCCostumeEditorDoc *pDoc)
{
	ugcEditorSetDocPane( pDoc->pRootPane );

	// Because all systems share one animated headshot, we need to
	// force updating it when the visible costume editor changes.
	s_bForceCostumeRefresh = true;
}

// This is run once per tick for drawing
void ugcCostumeEditor_OncePerFrame(UGCCostumeEditorDoc *pDoc)
{
	// No work if nothing has changed
	if(   StructCompare( parse_UGCCostumeData, SAFE_MEMBER_ADDR( pDoc->pUGCCostume, data ), SAFE_MEMBER_ADDR( pDoc->pPreviewUGCCostume, data ), 0, 0, 0 ) == 0
		  && StructCompare( parse_UGCCostumeOverride, &pDoc->hoverData, &pDoc->previewHoverData, 0, 0, 0 ) == 0
		  && !s_bForceCostumeRefresh ) {
		return;
	}
	s_bForceCostumeRefresh = false;
	StructDestroy( parse_UGCCostume, pDoc->pPreviewUGCCostume );
	pDoc->pPreviewUGCCostume = StructClone( parse_UGCCostume, pDoc->pUGCCostume );
	StructCopyAll( parse_UGCCostumeOverride, &pDoc->hoverData, &pDoc->previewHoverData );

	// Copy costume off UGC structure into preview costume
	StructDestroySafe( parse_PlayerCostume, &pDoc->pPreviewCostume );
	pDoc->pPreviewCostume = ugcCostumeGeneratePlayerCostume( pDoc->pUGCCostume, &pDoc->hoverData, NULL );
	if( pDoc->hoverData.type ) {
		gfxHeadshotAnimationIsPaused = true;
	} else {
		gfxHeadshotAnimationIsPaused = false;
	}
	
	if( !pDoc->pPreviewCostume ) {
		ugcui_AnimatedResourcePreviewSetResource( pDoc->pCostumePreview, NULL, NULL, true );
	} else {
		bool bIsSpace = (pDoc->pUGCCostume->eRegion == (U32)StaticDefineIntGetIntDefault( CharClassTypesEnum, "Space", -1 ));
		ugcui_AnimatedResourcePreviewSetCostume( pDoc->pCostumePreview, pDoc->pPreviewCostume, !bIsSpace );
		if( pDoc->bPreviewResetCamera ) {
			ugcui_AnimatedResourcePreviewResetCamera( pDoc->pCostumePreview );
			pDoc->bPreviewResetCamera = false;
		}
	}
}

// -------------------------------------------------------------------
// Lifecycle functions
// -------------------------------------------------------------------


UGCCostumeEditorDoc *ugcCostumeEditor_Open(UGCCostume *pUGCCostume)
{
	UGCCostumeEditorDoc *pDoc;
	bool bIsSpace = (pUGCCostume->eRegion == (U32)StaticDefineIntGetIntDefault( CharClassTypesEnum, "Space", -1 ));


	// Create the doc
	pDoc = calloc(1, sizeof(UGCCostumeEditorDoc));
	pDoc->pUGCCostume = pUGCCostume;
	pDoc->astrName = pUGCCostume->astrName;

	// Create the panes
	pDoc->pRootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );

	pDoc->pCostumePreview = ugcui_AnimatedResourcePreviewCreate();
	pDoc->bPreviewResetCamera = true;
	ui_WidgetSetDimensionsEx( UI_WIDGET( pDoc->pCostumePreview ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( pDoc->pCostumePreview ), 0, UGC_LIBRARY_PANE_WIDTH, UGC_PANE_TOP_BORDER, 0 );
	ui_PaneAddChild( pDoc->pRootPane, pDoc->pCostumePreview );

	pDoc->pSidePane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	ui_WidgetSetPositionEx( UI_WIDGET( pDoc->pSidePane ), 0, 0, 0, 0, UITopRight );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pDoc->pSidePane ), UGC_LIBRARY_PANE_WIDTH, 1, UIUnitFixed, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( pDoc->pSidePane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );
	ui_PaneAddChild( pDoc->pRootPane, pDoc->pSidePane );

	return pDoc;
}


void ugcCostumeEditor_Close(UGCCostumeEditorDoc *pDoc)
{
	// Reset the costume graphics view
	char contextName[ 256 ];
	sprintf( contextName, "UGCCostumeEditor_%s", pDoc->astrName );
	MEContextDestroyByName( contextName );
	sprintf( contextName, "UGCCostumeEditor_%s_GlobalProperties", pDoc->astrName );
	MEContextDestroyByName( contextName );
	
	ui_WidgetQueueFreeAndNull( &pDoc->pCostumePreview );
	ui_WidgetQueueFreeAndNull( &pDoc->pSidePane );
	ui_WidgetQueueFreeAndNull( &pDoc->pRootPane );
	ui_WidgetQueueFreeAndNull( &pDoc->pGlobalPropertiesWindow );

	// Free the data
	eaDestroy( &pDoc->eaSpecies );
	eaDestroy( &pDoc->eaGenders );
	eaDestroy( &pDoc->eaAllegiance );
	eaDestroy( &pDoc->eaStances );
	{
		int it;
		for( it = 0; it != ARRAY_SIZE( pDoc->eaSlots ); ++it ) {
			eaDestroy( &pDoc->eaSlots[ it ]);
		}
	}

	eaDestroy( &pDoc->eaComponentsUsingCostume );
	ui_WidgetQueueFreeAndNull( &pDoc->pComponentsUsingCostumeMenu );
	StructReset( parse_UGCCostumeBasics, &pDoc->costumeBasics );
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );
	StructDestroySafe( parse_PlayerCostume, &pDoc->pPreviewCostume );
	SAFE_FREE( pDoc );
}


UGCCostume *ugcCostumeEditor_GetCostume(UGCCostumeEditorDoc *pDoc)
{
	return pDoc->pUGCCostume;
}

static void ugcCostumeEditor_GlobalPropertiesWindowRefresh( UGCCostumeEditorDoc* pDoc )
{
	char strContextName[ 256 ];
	MEFieldContext* uiCtx;

	if( !pDoc->pGlobalPropertiesWindow ) {
		return;
	}

	ui_WidgetSetDimensions( UI_WIDGET( pDoc->pGlobalPropertiesWindow ), 300, 100 );
	ui_WidgetSetTextMessage( UI_WIDGET( pDoc->pGlobalPropertiesWindow ), "UGC_CostumeEditor.GlobalProperties" );
	ui_WindowSetResizable( pDoc->pGlobalPropertiesWindow, false );

	sprintf( strContextName, "UGCCostumeEditor_%s_GlobalProperties", pDoc->astrName );
	uiCtx = MEContextPush( strContextName, pDoc->pUGCCostume, pDoc->pUGCCostume, parse_UGCCostume );
	uiCtx->cbChanged = ugcCostumeEditor_MetadataChangeCB;
	uiCtx->pChangedData = pDoc;
	MEContextSetParent( UI_WIDGET( pDoc->pGlobalPropertiesWindow ));
	MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
	uiCtx->bTextEntryTrimWhitespace = true;
	uiCtx->iXPos = 0;
	uiCtx->iYPos = 0;
	uiCtx->iXLabelStart = 0;
	uiCtx->iYLabelStart = 0;
	uiCtx->bLabelPaddingFromData = false;
	uiCtx->iXDataStart = 20;
	uiCtx->iYDataStart = UGC_ROW_HEIGHT - 10;
	uiCtx->iYStep = UGC_ROW_HEIGHT * 2 - 10;
	MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
	setVec2( uiCtx->iErrorIconOffset, 0, UGC_ROW_HEIGHT - 10 + 3 );
	uiCtx->bErrorIconOffsetFromRight = false;
	uiCtx->iErrorIconSpaceWidth = 0;

	MEContextAddSimpleMsg( kMEFieldType_TextEntry, "DisplayName", "UGC_CostumeEditor.DisplayName", "UGC_CostumeEditor.DisplayName.Tooltip" );
	MEContextAddSimpleMsg( kMEFieldType_TextEntry, "Description", "UGC_CostumeEditor.Description", "UGC_CostumeEditor.Description.Tooltip" );

	MEContextPop( strContextName );
}

void ugcCostumeEditor_GlobalPropertiesWindowShow( UGCCostumeEditorDoc* pDoc )
{
	if( !pDoc->pGlobalPropertiesWindow ) {
		pDoc->pGlobalPropertiesWindow = ui_WindowCreate( "", 0, 0, 150, 100 );
	}

	ugcCostumeEditor_GlobalPropertiesWindowRefresh( pDoc );
	elUICenterWindow( pDoc->pGlobalPropertiesWindow );
	ui_WindowSetModal( pDoc->pGlobalPropertiesWindow, true );
	ui_WindowPresentEx( pDoc->pGlobalPropertiesWindow, true );
}

typedef struct CostumeImportData {
	UGCCostumeEditorDoc* pDoc;
	UITabGroup* tabGroup;
	UIList* costumeList;
	UGCAssetLibraryPane* costumePane;
} CostumeImportData;

static void ugcCostumeEditor_ImportCB( UIWidget* ignored, CostumeImportData* data )
{
	UGCCostumeEditorDoc* pDoc = data->pDoc;
			
	assert( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR );
			
	switch( ui_TabGroupGetActiveIndex( data->tabGroup )) {
		// IMPORT LIST
		xcase 0: {
			CostumeOnly_CostumeList* selectedCostume = ui_ListGetSelectedObject( data->costumeList );

			if( selectedCostume ) {
				PlayerCostume* pCostume = CostumeOnly_LoadCostume( selectedCostume->eFileName );
				if( pCostume ) {
					int it;
					StructCopyAll( parse_PlayerCostume, pCostume, pDoc->pUGCCostume->pPlayerCostume );

					// Find the correct allegiance
					ugcCostumeEditorCharCreator_FillAllegianceList(&pDoc->eaAllegiance);
					for( it = 0; it != eaSize( &pDoc->eaAllegiance ); ++it ) {
						SpeciesDef* pSpecies;
						CharacterCreation_FillSpeciesList(&pDoc->eaSpecies, pDoc->eaAllegiance[ it ]->pcName, false, true);
						pSpecies = ugcCostumeEditorCharCreator_FindMatchingSpecies(pDoc->eaSpecies, GET_REF( pCostume->hSpecies ), false);

						if( pSpecies ) {
							pDoc->pUGCCostume->eRegion = StaticDefineIntGetInt(CharClassTypesEnum, "Ground");
							SET_HANDLE_FROM_REFERENT( "SpeciesDef", pDoc->eaAllegiance[ it ], pDoc->pUGCCostume->hAllegiance );
							break;
						}

						if( ugcIsSpaceEnabled() ) {
							CharacterCreation_FillSpeciesList(&pDoc->eaSpecies, pDoc->eaAllegiance[ it ]->pcName, true, true);
							pSpecies = ugcCostumeEditorCharCreator_FindMatchingSpecies(pDoc->eaSpecies, GET_REF( pCostume->hSpecies), false);
					
							if( pSpecies ) {
								pDoc->pUGCCostume->eRegion = StaticDefineIntGetInt(CharClassTypesEnum, "Space");
								SET_HANDLE_FROM_REFERENT( "SpeciesDef", pDoc->eaAllegiance[ it ], pDoc->pUGCCostume->hAllegiance );
								break;
							}
						}
					}
					ugcCostumeEditor_FieldChangeCB(NULL, true, pDoc);
					StructDestroySafe( parse_PlayerCostume, &pCostume );
				}
			}
		}

		// COSTUME LIST
		xcase 1: {
			UGCAssetLibraryRow* row = ugcAssetLibraryPaneGetSelected( data->costumePane );
			PlayerCostume* pCostume = RefSystem_ReferentFromString( "PlayerCostume", row->pcName );
			if( pCostume ) {
				StructCopyAll( parse_PlayerCostume, pCostume, pDoc->pUGCCostume->pPlayerCostume );
				ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
			}
		}
	}
	
	ugcAssetLibraryPaneDestroy( data->costumePane );
	free( data );	
	ugcModalDialogClose( NULL, NULL );
}

static void ugcCostumeEditor_ImportAssetCB( UGCAssetLibraryPane* ignored, CostumeImportData* data, UGCAssetLibraryRow* row )
{
	ugcCostumeEditor_ImportCB( NULL, data );
}

static void ugcCostumeEditor_CancelCB( UIWidget* ignored, UserData rawData )
{
	CostumeImportData* data = rawData;

	free( data );	
	ugcModalDialogClose( NULL, NULL );
}


void ugcCostumeEditor_HandleAction(UGCCostumeEditorDoc *pDoc, UGCActionID action)
{
	switch (action)
	{
		xcase UGC_ACTION_COSTUME_EDIT_NAME:
			ugcCostumeEditor_GlobalPropertiesWindowShow( pDoc );
		
		xcase UGC_ACTION_COSTUME_RANDOMIZE_ALL:
			assert( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR );
			costumeRandom_RandomParts( CONTAINER_NOCONST( PlayerCostume, pDoc->pUGCCostume->pPlayerCostume ), GET_REF( pDoc->pUGCCostume->pPlayerCostume->hSpecies ), NULL, NULL, NULL, NULL, true, true, true, true, true );
			costumeRandom_RandomBoneScales( CONTAINER_NOCONST( PlayerCostume, pDoc->pUGCCostume->pPlayerCostume ), GET_REF( pDoc->pUGCCostume->pPlayerCostume->hSpecies ), NULL, "all" );

			// Force UI updates and such
			ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );

		xcase UGC_ACTION_COSTUME_RANDOMIZE_REGION0: case UGC_ACTION_COSTUME_RANDOMIZE_REGION1:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION2: case UGC_ACTION_COSTUME_RANDOMIZE_REGION3:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION4: case UGC_ACTION_COSTUME_RANDOMIZE_REGION5:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION6: case UGC_ACTION_COSTUME_RANDOMIZE_REGION7:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION8: case UGC_ACTION_COSTUME_RANDOMIZE_REGION9:
			if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR ) {
				int regionIdx = action - UGC_ACTION_COSTUME_RANDOMIZE_REGION0;
				if( regionIdx < eaSize( &pDoc->eaCostumeRegions )) {
					ugcCostumeEditorCharCreator_RandomizeRegion( pDoc, pDoc->eaCostumeRegions[ regionIdx ]);
				}
			}

		xdefault:
			break;
	}
}

bool ugcCostumeEditor_QueryAction(UGCCostumeEditorDoc *pDoc, UGCActionID action, char** out_estr)
{
	switch(action)
	{
		case UGC_ACTION_COSTUME_DELETE:
		case UGC_ACTION_COSTUME_DUPLICATE:
		case UGC_ACTION_COSTUME_EDIT_NAME:
			return true;

		case UGC_ACTION_COSTUME_RANDOMIZE_ALL:
			return true;

		case UGC_ACTION_COSTUME_RANDOMIZE_REGION0:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION1:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION2:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION3:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION4:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION5:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION6:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION7:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION8:
		case UGC_ACTION_COSTUME_RANDOMIZE_REGION9: {
			int regionIdx = action - UGC_ACTION_COSTUME_RANDOMIZE_REGION0;
			return regionIdx < eaSize( &pDoc->eaCostumeRegions );
		}

		xdefault:
			return false;
	}
}

//// CHARACTER CREATOR MODE UTILITIES
SpeciesDef* ugcCostumeEditorCharCreator_FindMatchingSpecies(SpeciesDef **eaSpecies, SpeciesDef *pSpecies, bool bReturnDefault)
{
	const char *pcName1, *pcName2;
	int i;

	if (pSpecies) {
		pcName1 = TranslateDisplayMessage(pSpecies->displayNameMsg);
		for(i=eaSize(&eaSpecies)-1; i>=0; --i) {
			pcName2 = TranslateDisplayMessage(eaSpecies[i]->displayNameMsg);
			if (pcName1 && pcName2 && (stricmp(pcName1,pcName2) == 0)) {
				return eaSpecies[i];
			}
		}
	}
	if (bReturnDefault && eaSize(&eaSpecies)) {
		return eaSpecies[0];
	} else {
		return NULL;
	}
}


SpeciesDef* ugcCostumeEditorCharCreator_FindMatchingGender(SpeciesDef **eaGenders, SpeciesDef *pGender)
{
	const char *pcName1, *pcName2;
	int i;

	if (pGender) {
		pcName1 = TranslateDisplayMessage(pGender->genderNameMsg);
		for(i=eaSize(&eaGenders)-1; i>=0; --i) {
			pcName2 = TranslateDisplayMessage(eaGenders[i]->genderNameMsg);
			if (pcName1 && pcName2 && (stricmp(pcName1,pcName2) == 0)) {
				return eaGenders[i];
			}
		}
	}
	if (eaSize(&eaGenders)) {
		return eaGenders[0];
	} else {
		return NULL;
	}
}


void ugcCostumeEditorCharCreator_FillAllegianceList(AllegianceDef ***peaAllegianceList)
{
	if( ugcIsAllegianceEnabled() ) {
		UGCPerProjectDefaults* defaults = ugcGetDefaults();
		int it;

		eaClear( peaAllegianceList );
		for( it = 0; it != eaSize( &defaults->allegiance ); ++it ) {
			UGCPerAllegianceDefaults* allegianceDefault = defaults->allegiance[ it ];
			if( allegianceDefault->allegianceName ) {
				eaPush( peaAllegianceList, RefSystem_ReferentFromString( g_hAllegianceDict, allegianceDefault->allegianceName ));
			}
		}
	}
}

void ugcCostumeEditorCharCreator_LineChangeCB( MEField *pField, bool bFinished, UGCCostumeLineData* pData )
{
	UGCCostumeEditorDoc* pDoc = pData->pDoc;
	CostumeEditLine *pLine = pData->pLine;
	NOCONST(PlayerCostume)* costumeToModify;
	
	if (pDoc->ignoreUpdates) {
		return;
	}

	if( bFinished ) {
		costumeToModify = CONTAINER_NOCONST( PlayerCostume, pDoc->pUGCCostume->pPlayerCostume );
	} else {
		StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_ENTIRE_COSTUME;
		pDoc->hoverData.entireCostume = StructClone( parse_PlayerCostume, pDoc->pUGCCostume->pPlayerCostume );
		costumeToModify = CONTAINER_NOCONST( PlayerCostume, pDoc->hoverData.entireCostume );
	}

	// Apply the line changes
	if( pField->eType == kMEFieldType_Slider ) {
		costumeLineUI_SetLineScaleInternal( costumeToModify, pLine, 0, pLine->fTempValue1 - pLine->fScaleMin1);
		if ((stricmp(pLine->pcName, "PositionX") == 0) ||
			(stricmp(pLine->pcName, "ScaleX") == 0) ||
			(pLine->iType == kCostumeEditLineType_Scale)) {
			costumeLineUI_SetLineScaleInternal( costumeToModify, pLine, 1, pLine->fTempValue2 - pLine->fScaleMin2);
		}
	} else {
		costumeLineUI_SetLineItemInternal( costumeToModify, GET_REF( pDoc->pUGCCostume->pPlayerCostume->hSpecies ),
										   pLine->pcSysName, pLine, pLine->iType, 
										   NULL /* eaUnlockedCostumes */, NULL /* eaPowerFXBones */, NULL /* pSlotType */, NULL /* pGuild */, NULL /* pGameAccountData */, 
										   true /* bUnlockAll */, true /* bMirrorMode */, true /* bGroupMode */);
	}

	if( bFinished ) {
		ugcCostumeEditor_FieldChangeCB( pField, bFinished, pDoc );
	}
}

static const char *GetLineValueForRow(CostumeEditLine *pLine, S32 iRow)
{
	const char *pcValue = NULL;
	if(iRow >= 0) {
		switch( pLine->iType ) {
			xcase kCostumeEditLineType_Category:
		if( iRow < eaSize(&pLine->eaCat) )
			pcValue = pLine->eaCat[ iRow ]->pcName;
		xcase kCostumeEditLineType_Geometry:
		if( iRow < eaSize(&pLine->eaGeo) )
			pcValue = pLine->eaGeo[ iRow ]->pcName;
		xcase kCostumeEditLineType_Material:
		if( iRow < eaSize(&pLine->eaMat) )
			pcValue = pLine->eaMat[ iRow ]->pcName;
		xcase kCostumeEditLineType_Texture0: case kCostumeEditLineType_Texture1:
		case kCostumeEditLineType_Texture2: case kCostumeEditLineType_Texture3:
		case kCostumeEditLineType_Texture4:
			if( iRow < eaSize(&pLine->eaTex) )
				pcValue = pLine->eaTex[ iRow ]->pcName;
		}
	}
	return pcValue;
}

void ugcCostumeEditorCharCreator_LineHoverCB( UIComboBox *pCombo, S32 iRow, UGCCostumeLineData* pData )
{
	UGCCostumeEditorDoc* pDoc = pData->pDoc;
	CostumeEditLine *pLine = pData->pLine;
	
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );
	if( iRow >= 0 ) {
		const char* pcValue = GetLineValueForRow(pLine, iRow);

		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_ENTIRE_COSTUME;
		pDoc->hoverData.entireCostume = StructClone( parse_PlayerCostume, pDoc->pUGCCostume->pPlayerCostume );

		costumeLineUI_SetLineItemInternal( CONTAINER_NOCONST( PlayerCostume, pDoc->hoverData.entireCostume ),
										   GET_REF( pDoc->hoverData.entireCostume->hSpecies ),
										   pcValue, pLine, pLine->iType,
										   NULL, NULL, NULL, NULL, NULL, true, true, true );
	}
}


void ugcCostumeEditorCharCreator_ColorChangeCB(UIColorCombo *pCombo, UGCCostumeColorData* pData)
{
	UGCCostumeEditorDoc *pDoc = pData->pDoc;
 	NOCONST(PlayerCostume)* pCostume = CONTAINER_NOCONST( PlayerCostume, pDoc->pUGCCostume->pPlayerCostume );
	NOCONST(PCPart)* pPart = costumeTailor_GetPartByBone( pCostume, GET_REF( pData->pParent->pLine->hOwnerBone ), NULL );
 	Vec4 vColor;
	U8 color[4];
 
	// Get the color into the right format
 	ui_ColorComboGetColor(pCombo, vColor);
	VEC4_TO_COSTUME_COLOR(vColor, color);
 
	// Apply color change
	if (pPart) {
		if (costumeTailor_SetRealPartColor(pCostume, pPart, GET_REF(pCostume->hSpecies), 
										   pData->iColorNum, color, NULL /* SlotType */, true /* MirrorMode */)) {
			ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
		}
	}
}


void ugcCostumeEditorCharCreator_ColorHoverCB(UIColorCombo *pCombo, bool bHover, Vec4 vColor, UGCCostumeColorData* pData)
{
 	UGCCostumeEditorDoc *pDoc = pData->pDoc;
	U8 color[ 4 ];
	
	VEC4_TO_COSTUME_COLOR(vColor, color);
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );
 
	if( bHover && (pData->pParent->iHoverColorIndex != pData->iColorNum
				   || !sameVec4( vColor, pData->pParent->vHoverColor ))) {
		NOCONST(PlayerCostume)* pCostume;
		NOCONST(PCPart)* pPart;
		
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_ENTIRE_COSTUME;
		pDoc->hoverData.entireCostume = StructClone( parse_PlayerCostume, pDoc->pUGCCostume->pPlayerCostume );
		
		pCostume = CONTAINER_NOCONST( PlayerCostume, pDoc->hoverData.entireCostume );
		pPart = costumeTailor_GetPartByBone( pCostume, GET_REF( pData->pParent->pLine->hOwnerBone ), NULL );
		if( pPart ) {
			costumeTailor_SetRealPartColor( pCostume, pPart, GET_REF(pCostume->hSpecies), 
											pData->iColorNum, color, NULL /* SlotType */, true /* MirrorMode */);
		}
 	}
}


void ugcCostumeEditorCharCreator_RandomizeRegion( UGCCostumeEditorDoc* pDoc, PCRegion* pRegion )
{
	PlayerCostume* pCostume = pDoc->pUGCCostume->pPlayerCostume;
	assert( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR );
	
	// STO Hardcode to combine regions
	//   WOLF[15Jun2012] Release 4 STO only had a single "Body" randomize button that was evidently hooked up to "ST*_UpperBody".
	//   Since Release 5 has both upper body/lower body randomize buttons, we shouldn't need to deal with the hardcoded upper/lower combine that was
	//   happening here.
	if (pRegion && ((stricmp(pRegion->pcName, "STF_Head") == 0) || (stricmp(pRegion->pcName, "STM_Head") == 0)) ) {
		// Randomize skin along with the head
		costumeRandom_RandomParts( CONTAINER_NOCONST( PlayerCostume, pCostume ), GET_REF( pCostume->hSpecies ), NULL, pRegion, NULL, NULL, true, true, true, true, true /*Randomize skin*/ );
		costumeRandom_RandomBoneScales( CONTAINER_NOCONST( PlayerCostume, pCostume ), GET_REF( pCostume->hSpecies ), NULL, "Face" );				 
		costumeRandom_RandomBoneScales( CONTAINER_NOCONST( PlayerCostume, pCostume ), GET_REF( pCostume->hSpecies ), NULL, "Head" );				 
	} else {
		// Normal region so just do it
		costumeRandom_RandomParts( CONTAINER_NOCONST( PlayerCostume, pCostume ), GET_REF( pCostume->hSpecies ), NULL, pRegion, NULL, NULL, true, true, true, true, false );
	}

	// Force UI updates and such
	ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
}


static void ugcCostumeEditorCharCreator_RefreshLineColor( UGCCostumeEditorDoc* pDoc, UGCCostumeLineData* pData, NOCONST(PCPart) *pPart, int colorIdx, int colorIt, int colorCount, int iSingleColorY )
{
	MEFieldContext* uiCtx = MEContextGetCurrent();
	UIColorSet* pColorSet = costumeTailor_GetColorSetForPartConsideringSkin( CONTAINER_NOCONST( PlayerCostume, pDoc->pUGCCostume->pPlayerCostume ), GET_REF( pDoc->pUGCCostume->pPlayerCostume->hSpecies ), NULL, pPart, colorIdx );
	char entryName[ 256 ];
	U8 color[4];
	
	assert( colorIt < colorCount );
	costumeTailor_GetRealPartColor(CONTAINER_NOCONST( PlayerCostume, pDoc->pUGCCostume->pPlayerCostume ), pPart, colorIdx, color);

	sprintf( entryName, "LineColor%d", colorIt );

	if( pColorSet ) {
		MEFieldContextEntry* entry = MEContextAddCustom( allocAddString( entryName ));
		ugcRefreshColorCombo( (UIColorCombo**)&ENTRY_WIDGET( entry ), uiCtx->iXPos + uiCtx->iXDataStart + 25 * colorIt, uiCtx->iYPos, pColorSet, color, &pData->colors[ colorIdx ], uiCtx->pUIContainer, ugcCostumeEditorCharCreator_ColorChangeCB, ugcCostumeEditorCharCreator_ColorHoverCB );

		if( colorCount == 1 ) {
			ui_WidgetSetPositionEx( ENTRY_WIDGET( entry ), uiCtx->iRightPad - 25, iSingleColorY, 0, 0, UITopRight );
		}
	}
}

static void ugcCostumeEditorCharCreator_ResetLineData( UGCCostumeLineData* pData )
{
	if( pData->pLine ) {
		costumeLineUI_DestroyLine( pData->pLine );
		pData->pLine = NULL;
	}
}


void ugcCostumeEditorCharCreator_RefreshLine(UGCCostumeEditorDoc *pDoc, PCRegion* pRegion, int index, CostumeEditLine *pLine, bool bIsSpace, bool prevTypeIsGeometry)
{
	NOCONST(PlayerCostume) *pCostume = CONTAINER_NOCONST(PlayerCostume, pDoc->pUGCCostume->pPlayerCostume);
	NOCONST(PCPart) *pPart = NULL;
	char labelBuf[1024];
	int iNumColors;
	int iSingleColorY;
	char uiCtxName[ 256 ];
	MEFieldContext* uiCtx;
	UGCCostumeLineData* pData;
	
	sprintf( uiCtxName, "UGCCostumeEditor_Line%d", index );
	uiCtx = MEContextPush( uiCtxName, pLine, pLine, parse_CostumeEditLine );
	uiCtx->cbChanged = ugcCostumeEditorCharCreator_LineChangeCB;
	{
		int it;
		pData = MEContextAllocMem( "Data", sizeof( *pData ), ugcCostumeEditorCharCreator_ResetLineData, true );
		pData->pDoc = pDoc;
		pData->pLine = pLine;
		for( it = 0; it != 4; ++it ) {
			pData->colors[ it ].pDoc = pDoc;
			pData->colors[ it ].pParent = pData;
			pData->colors[ it ].iColorNum = it;
		}

		uiCtx->pChangedData = pData;
	}

	// Set up the label
	sprintf( labelBuf, "%s", TranslateDisplayMessage( pLine->displayNameMsg ));

	// Get the part (if any)
	if( GET_REF( pLine->hOwnerBone )) {
		pPart = costumeTailor_GetPartByBone(pCostume, GET_REF(pLine->hOwnerBone), NULL);
	}

	// Color counting
	iNumColors = 0;
	if( pLine->bColor0Allowed ) {
		++iNumColors;
	}
	if( pLine->bColor1Allowed ) {
		++iNumColors;
	}
	if( pLine->bColor2Allowed ) {
		++iNumColors;
	}
	if( pLine->bColor3Allowed ) {
		++iNumColors;
	}
	iSingleColorY = uiCtx->iYPos + uiCtx->iYStep;
	if( iNumColors == 1 ) {
		uiCtx->iRightPad += 25;
	}

	// Do line type specific logic
	if( pLine->iType == kCostumeEditLineType_Category ) {
		NOCONST(PCRegionCategory) *pRegCat = NULL;
		int i;

		// Find the right structure for the region/category
		for(i=eaSize(&pCostume->eaRegionCategories)-1; i>=0; --i) {
			if (GET_REF(pCostume->eaRegionCategories[i]->hRegion) == pRegion ) {
				pRegCat = pCostume->eaRegionCategories[i];
			}
		}

		MEContextAddLabel( "Title", labelBuf, NULL );
		
		if( pRegCat ) {
			MEFieldContextEntry* entry;
			MEContextPush( "Category", pRegCat, pRegCat, parse_PCRegionCategory );

			entry = MEContextAddDataProvided( kMEFieldType_Combo, parse_PCCategory, &pLine->eaCat, "Name",
											  "PCCategory", NULL, NULL );
			MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
			ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorCharCreator_LineHoverCB, pData );

			MEContextPop( "Category" );
		}
	} else if(pLine->iType == kCostumeEditLineType_Texture4) {
		MEContextAddLabel( "Title", labelBuf, NULL );
		
		if( pPart ) {
			MEFieldContextEntry* entry = NULL;
			
			pLine->pcSysName = pPart->pMovableTexture ? REF_STRING_FROM_HANDLE( pPart->pMovableTexture->hMovableTexture ) : NULL;
			pLine->fTempValue1 = SAFE_MEMBER2( pPart, pMovableTexture, fMovableValue );

			entry = MEContextAddDataProvided(
				kMEFieldType_Combo, parse_PCTextureDef, &pLine->eaTex, "Name",
				"SysName", NULL, NULL );
			MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
			ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorCharCreator_LineHoverCB, pData );

			if( pLine->bHasSlider ) {
				MEContextAddLabel( "ValueTitle", "Value", NULL );
				MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin1, pLine->fScaleMax1, 1, "Value1", NULL, NULL );
			}
		}
	} else if( pLine->iType == kCostumeEditLineType_TextureScale ) {
		if( pPart ) {
			if( !pPart->pMovableTexture ) {
				pPart->pMovableTexture = StructCreateNoConst( parse_PCMovableTextureInfo );
				assert( pPart->pMovableTexture );
			}

			if( stricmp( "PositionX", pLine->pcName ) == 0 ) {
				pLine->fTempValue1 = pPart->pMovableTexture->fMovableX;
				pLine->fTempValue2 = pPart->pMovableTexture->fMovableY;

				MEContextAddLabel( "PositionTitle", "Position", NULL );
				MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin1, pLine->fScaleMax1, 1, "Value1", NULL, NULL );
				MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin2, pLine->fScaleMax2, 1, "Value2", NULL, NULL );
			} else if (stricmp("ScaleX", pLine->pcName) == 0) {
				pLine->fTempValue1 = pPart->pMovableTexture->fMovableScaleX;
				pLine->fTempValue2 = pPart->pMovableTexture->fMovableScaleY;
				
				MEContextAddLabel( "ScaleTitle", "Scale", NULL );
				MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin1, pLine->fScaleMax1, 1, "Value1", NULL, NULL );
				MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin2, pLine->fScaleMax2, 1, "Value2", NULL, NULL );
			} else if (stricmp("Rotation", pLine->pcName) == 0) {
				pLine->fTempValue1 = pPart->pMovableTexture->fMovableRotation;

				MEContextAddLabel( "RotationTitle", "Rotation", NULL );
				MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin1, pLine->fScaleMax1, 1, "Value1", NULL, NULL );
			} else {
				Errorf( "Unexpected texture scale value '%s'", pLine->pcName );
			}
		}
	} else if( pLine->iType == kCostumeEditLineType_Scale ) {
		const PCScaleValue* pScale = costumeTailor_FindScaleValueByName( CONTAINER_RECONST( PlayerCostume, pCostume ), pLine->pcName );
		const PCScaleValue* pScale2 = costumeTailor_FindScaleValueByName( CONTAINER_RECONST( PlayerCostume, pCostume ), pLine->pcName2 );

		if( !nullStr( pLine->pcName )) {
			pLine->fTempValue1 = SAFE_MEMBER( pScale, fValue );

			MEContextAddLabel( "Title", labelBuf, NULL );
			MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin1, pLine->fScaleMax1, 0, "Value1", NULL, NULL );
		}
		
		if( !nullStr( pLine->pcName2 )) {
			pLine->fTempValue2 = SAFE_MEMBER( pScale2, fValue );
			
			MEContextAddLabel( "Title2", TranslateDisplayMessage( pLine->displayNameMsg2 ), NULL );
			MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin2, pLine->fScaleMax2, 0, "Value2", NULL, NULL );
		}
	} else if( pLine->iType == kCostumeEditLineType_BodyScale ) {
		int scaleIndex = costumeTailor_FindBodyScaleInfoIndexByName( CONTAINER_RECONST( PlayerCostume, pCostume ), pLine->pcName );
		PCSkeletonDef* pSkel = GET_REF( pCostume->hSkeleton );
		const PCBodyScaleInfo* scale = NULL;
		if( pSkel ) {
			scale = eaGet( &pSkel->eaBodyScaleInfo, scaleIndex );
		}

		if( !nullStr( pLine->pcName )) {
			if( eaSize( &scale->eaValues ) == 0 ) {
				pLine->fTempValue1 = eafGet( &pCostume->eafBodyScales, scaleIndex );
				MEContextAddLabel( "Title", labelBuf, NULL );
				MEContextAddMinMax( kMEFieldType_Slider, 0, 100, 1, "Value1", NULL, NULL );
			} else {
				// unsupported -- needs a drop down
			}
		}
	} else if( pPart ) {
		void*** peaModel = NULL;
		ParseTable *pModelParse = NULL;
		const char *pcLabelText = labelBuf;
		int iIndent = 0;

		switch( pLine->iType ) {
			xcase kCostumeEditLineType_Geometry:
				peaModel = &pLine->eaGeo;
				pModelParse = parse_PCGeometryDef;
				pLine->pcSysName = REF_STRING_FROM_HANDLE( pPart->hGeoDef );
				
			xcase kCostumeEditLineType_Material:
				peaModel = &pLine->eaMat;
				pModelParse = parse_PCMaterialDef;
				pLine->pcSysName = REF_STRING_FROM_HANDLE( pPart->hMatDef );
				if( bIsSpace ) {
					if( prevTypeIsGeometry ) {
						pcLabelText = "Option";
						iIndent = 15;
					} else {
						PCBoneDef *pBone = GET_REF( pPart->hBoneDef );
						pcLabelText = TranslateDisplayMessage( pBone->displayNameMsg );
					}
				}
				
			xcase kCostumeEditLineType_Texture0:
				peaModel = &pLine->eaTex;
				pModelParse = parse_PCTextureDef;
				pLine->pcSysName = REF_STRING_FROM_HANDLE( pPart->hPatternTexture );
				pLine->fTempValue1 = SAFE_MEMBER2( pPart, pTextureValues, fPatternValue );
				
			xcase kCostumeEditLineType_Texture1:
				peaModel = &pLine->eaTex;
				pModelParse = parse_PCTextureDef;
				pLine->pcSysName = REF_STRING_FROM_HANDLE( pPart->hDetailTexture );
				pLine->fTempValue1 = SAFE_MEMBER2( pPart, pTextureValues, fDetailValue );
				
			xcase kCostumeEditLineType_Texture2:
				peaModel = &pLine->eaTex;
				pModelParse = parse_PCTextureDef;
				pLine->pcSysName = REF_STRING_FROM_HANDLE( pPart->hSpecularTexture );
				pLine->fTempValue1 = SAFE_MEMBER2( pPart, pTextureValues, fSpecularValue );
				
			xcase kCostumeEditLineType_Texture3:
				peaModel = &pLine->eaTex;
				pModelParse = parse_PCTextureDef;
				pLine->pcSysName = REF_STRING_FROM_HANDLE( pPart->hDiffuseTexture );
				pLine->fTempValue1 = SAFE_MEMBER2( pPart, pTextureValues, fDiffuseValue );
				
			xdefault:
				Errorf( "Unexpected line type %d", pLine->iType );
		}

		MEContextAddLabel( "Title", pcLabelText, NULL );
		
		if( peaModel && pModelParse ) {
			MEFieldContextEntry* entry = MEContextAddDataProvided(
					kMEFieldType_Combo, pModelParse, peaModel, "Name",
					"SysName", NULL, NULL );
			MEFieldSetDictField( ENTRY_FIELD( entry ), "Name", "DisplayNameMsg", true );
			ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorCharCreator_LineHoverCB, pData );
		}

		if( pLine->bHasSlider ) {
			MEContextAddLabel( "ValueTitle", "Value", NULL );
			MEContextAddMinMax( kMEFieldType_Slider, pLine->fScaleMin1, pLine->fScaleMax1, 1, "Value1", NULL, NULL );
		}
	}

	if( pPart && iNumColors ) {
		int colorIt = 0;

		if( pLine->bColor0Allowed ) {
			ugcCostumeEditorCharCreator_RefreshLineColor( pDoc, pData, pPart, 0, colorIt, iNumColors, iSingleColorY );
			++colorIt;
		}
		if( pLine->bColor1Allowed ) {
			ugcCostumeEditorCharCreator_RefreshLineColor( pDoc, pData, pPart, 1, colorIt, iNumColors, iSingleColorY );
			++colorIt;
		}
		if( pLine->bColor2Allowed ) {
			ugcCostumeEditorCharCreator_RefreshLineColor( pDoc, pData, pPart, 2, colorIt, iNumColors, iSingleColorY );
			++colorIt;
		}
		if( pLine->bColor3Allowed ) {
			ugcCostumeEditorCharCreator_RefreshLineColor( pDoc, pData, pPart, 3, colorIt, iNumColors, iSingleColorY );
			++colorIt;
		}

		if( iNumColors > 1 ) {
			MEContextStepDown();
		}
	}

	MEContextPop( uiCtxName );
}


//// NEVERWINTER MODE UTILITIES
void ugcCostumeEditorNeverwinter_PartColorChangedCB( UIColorCombo *pCombo, UGCCostumePartColorUI* colorUI )
{
 	UGCCostumeEditorDoc *pDoc = colorUI->pDoc;
 	Vec4 vColor;
	U8 color[4];
 
	// Get the color into the right format
 	ui_ColorComboGetColor(pCombo, vColor);
	VEC4_TO_COSTUME_COLOR(vColor, color);
 
	// Apply color change
	if( colorUI->colorIsSkin ) {
		pDoc->pUGCCostume->data.skinColor = u8ColorToRGBA( color );
	} else {
		colorUI->part->colors[ colorUI->colorIndex ] = u8ColorToRGBA( color );
	}

	ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
}

void ugcCostumeEditorNeverwinter_PartColorHoverCB( UIColorCombo *pCombo, bool bHover, Vec4 vColor, UGCCostumePartColorUI* colorUI )
{
 	UGCCostumeEditorDoc *pDoc = colorUI->pDoc;
	UGCCostumePart* part = colorUI->part;
	U8 color[4];
 
	// Get the color into the right format
	VEC4_TO_COSTUME_COLOR( vColor, color );

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( bHover ) {
		if( colorUI->colorIsSkin ) {
			pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_SKIN_COLOR;
			pDoc->hoverData.iValue = u8ColorToRGBA( color );
		} else {
			pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_COLOR;
			pDoc->hoverData.astrName = part->astrBoneName;
			pDoc->hoverData.colorIndex = colorUI->colorIndex;
			pDoc->hoverData.iValue = u8ColorToRGBA( color );
		}
	}
}


void ugcCostumeEditorNeverwinter_GeometryHoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI )
{
	UGCCostumeEditorDoc* pDoc = partUI->pDoc;
	UGCCostumePart* part = partUI->part;

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		PCGeometryDef* geo = (*pCombo->model)[ iRow ];
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_GEOMETRY;
		pDoc->hoverData.astrName = part->astrBoneName;
		pDoc->hoverData.strValue = StructAllocString( geo->pcName );
	}
}


void ugcCostumeEditorNeverwinter_MaterialHoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI )
{
	UGCCostumeEditorDoc* pDoc = partUI->pDoc;
	UGCCostumePart* part = partUI->part;

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		PCMaterialDef* mat = (*pCombo->model)[ iRow ];
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_MATERIAL;
		pDoc->hoverData.astrName = part->astrBoneName;
		pDoc->hoverData.strValue = StructAllocString( mat->pcName );
	}
}


void ugcCostumeEditorNeverwinter_Texture0HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI )
{
	UGCCostumeEditorDoc* pDoc = partUI->pDoc;
	UGCCostumePart* part = partUI->part;

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		PCTextureDef* tex = (*pCombo->model)[ iRow ];
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_TEXTURE0;
		pDoc->hoverData.astrName = part->astrBoneName;
		pDoc->hoverData.strValue = StructAllocString( tex->pcName );
	}
}


void ugcCostumeEditorNeverwinter_Texture1HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI )
{
	UGCCostumeEditorDoc* pDoc = partUI->pDoc;
	UGCCostumePart* part = partUI->part;

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		PCTextureDef* tex = (*pCombo->model)[ iRow ];
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_TEXTURE1;
		pDoc->hoverData.astrName = part->astrBoneName;
		pDoc->hoverData.strValue = StructAllocString( tex->pcName );
	}
}


void ugcCostumeEditorNeverwinter_Texture2HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI )
{
	UGCCostumeEditorDoc* pDoc = partUI->pDoc;
	UGCCostumePart* part = partUI->part;

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		PCTextureDef* tex = (*pCombo->model)[ iRow ];
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_TEXTURE2;
		pDoc->hoverData.astrName = part->astrBoneName;
		pDoc->hoverData.strValue = StructAllocString( tex->pcName );
	}
}


void ugcCostumeEditorNeverwinter_Texture3HoverCB( UIComboBox* pCombo, S32 iRow, UGCCostumePartUI* partUI )
{
	UGCCostumeEditorDoc* pDoc = partUI->pDoc;
	UGCCostumePart* part = partUI->part;

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		PCTextureDef* tex = (*pCombo->model)[ iRow ];
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_PART_TEXTURE3;
		pDoc->hoverData.astrName = part->astrBoneName;
		pDoc->hoverData.strValue = StructAllocString( tex->pcName );
	}
}


UGCCostumeScale* ugcCostumeEditorNeverwinter_GetScale( UGCCostumeEditorDoc* pDoc, const char* name )
{
	int it;

	name = allocAddString( name );
	for( it = 0; it != eaSize( &pDoc->pUGCCostume->data.eaScales ); ++it ) {
		UGCCostumeScale* scale = pDoc->pUGCCostume->data.eaScales[ it ];
		if( scale->astrName == name ) {
			return scale;
		}
	}

	return NULL;
}

void ugcCostumeEditorNeverwinter_SetScale( UGCCostumeEditorDoc* pDoc, const char* name, float value )
{
	UGCCostumeScale* scale = ugcCostumeEditorNeverwinter_GetScale( pDoc, name );

	if( !scale ) {
		scale = StructCreate( parse_UGCCostumeScale );
		eaPush( &pDoc->pUGCCostume->data.eaScales, scale );
	}

	scale->value = value;
}

void ugcCostumeEditorNeverwinter_SetBodyScale( UGCCostumeEditorDoc* pDoc, const char* name, float value )
{
	int it;

	name = allocAddString( name );
	
	for( it = 0; it != eaSize( &pDoc->pUGCCostume->data.eaBodyScales ); ++it ) {
		UGCCostumeScale* scale = pDoc->pUGCCostume->data.eaBodyScales[ it ];
		if( scale->astrName == name ) {
			scale->value = value;
			break;
		}
	}

	{
		UGCCostumeScale* scale = StructCreate( parse_UGCCostumeScale );
		scale->astrName = name;
		scale->value = value;
		eaPush( &pDoc->pUGCCostume->data.eaBodyScales, scale );
	}
}

static void ugcCostumeEditorNeverwinter_RevertToPresetDoneCB( UGCAssetLibraryPane* pane, UserData rawDoc, UGCAssetLibraryRow* row )
{
	UGCCostumeEditorDoc* pDoc = rawDoc;	

	ugcCostumeRevertToPreset( pDoc->pUGCCostume, row->pcName );
	pDoc->bPreviewResetCamera = true;
	ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
}

void ugcCostumeEditorNeverwinter_RevertToPresetCB( UIWidget* ignored, UGCCostumeEditorDoc* pDoc )
{
	ugcAssetLibraryShowPicker( pDoc, false, "Choose Preset", NULL, "Costume", pDoc->pUGCCostume->data.astrPresetCostumeName, ugcCostumeEditorNeverwinter_RevertToPresetDoneCB );
}


static int eaSortCompare( const char** pstr1, const char** pstr2 )
{
	return stricmp( *pstr1, *pstr2 );
}

void ugcCostumeEditorNeverwinter_UpdateSlotCostumes( const char*** out_peaCostumes, const char* astrSlotName )
{
	ResourceSearchRequest request = { 0 };
	ResourceSearchResult* result;
	char buffer[ 1024 ];

	request.eSearchMode = SEARCH_MODE_TAG_SEARCH;
	sprintf( buffer, "UGC, CostumeItem, %s", astrSlotName );
	request.pcSearchDetails = buffer;
	request.pcName = NULL;
	request.pcType = "PlayerCostume";
	request.iRequest = 1;

	result = ugcResourceSearchRequest( &request );
	eaClear( out_peaCostumes );
	{
		int it;
		for( it = 0; it != eaSize( &result->eaRows ); ++it ) {
			const char* name = result->eaRows[ it ]->pcName;
			UGCCostumeMetadata* costumeMetadata = ugcResourceGetCostumeMetadata( name );
			if( costumeMetadata ) {
				eaPush( out_peaCostumes, allocAddString( name ));
			}
		}
	}
	eaQSort( *out_peaCostumes, eaSortCompare );
	StructDestroy( parse_ResourceSearchResult, result );
}


void ugcCostumeEditorNeverwinter_SlotChangedCB(MEField *pField, bool bFinished, UGCCostumeSlotUI* slotUI)
{
	UGCCostumeEditorDoc* pDoc = slotUI->pDoc;
	UGCCostumeSlotDef* slotDef = slotUI->slotDef;
	UGCCostumeSlot* slot = ugcCostumeFindSlot( pDoc->pUGCCostume, slotDef->astrName );

	if( !nullStr( slot->astrCostume )) {
		UGCCostumeMetadata* slotMetadata = ugcResourceGetCostumeMetadata( slot->astrCostume );

		eaiSetSize( &slot->eaColors, 4 );
		if( eaSize( &slotDef->eaBones )) {
			PCPart* bonePart = ugcCostumeMetadataGetPartByBone( slotMetadata, slotDef->eaBones[ 0 ]);
			if( bonePart ) {
				slot->eaColors[ 0 ] = u8ColorToRGBA( bonePart->color0 );
				slot->eaColors[ 1 ] = u8ColorToRGBA( bonePart->color1 );
				slot->eaColors[ 2 ] = u8ColorToRGBA( bonePart->color2 );
				slot->eaColors[ 3 ] = u8ColorToRGBA( bonePart->color3 );
			} else {
				slot->eaColors[ 0 ] = 0;
				slot->eaColors[ 1 ] = 0;
				slot->eaColors[ 2 ] = 0;
				slot->eaColors[ 3 ] = 0;
			}
		}
	} else {
		eaiClear( &slot->eaColors );
	}

	ugcCostumeEditor_FieldChangeCB( pField, bFinished, pDoc );
}

void ugcCostumeEditorNeverwinter_SlotHoverCB( UIComboBox *pCombo, S32 iRow, UGCCostumeSlotUI* slotUI )
{
	UGCCostumeEditorDoc* pDoc = slotUI->pDoc;
	UGCCostumeSlotDef* slotDef = slotUI->slotDef;
	UGCCostumeSlot* slot = ugcCostumeFindSlot( pDoc->pUGCCostume, slotDef->astrName );

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( iRow >= 0 ) {
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_SLOT;
		pDoc->hoverData.astrName = slotDef->astrName;
		pDoc->hoverData.strValue = StructAllocString( (*pCombo->model)[ iRow ]);
	}
}


void ugcCostumeEditorNeverwinter_SlotRevertCB( UIWidget* ignored, UGCCostumeSlotUI* slotUI )
{
	UGCCostumeEditorDoc* pDoc = slotUI->pDoc;
	UGCCostumeSlotDef* slotDef = slotUI->slotDef;
	UGCCostumeSlot* slot = ugcCostumeFindSlot( pDoc->pUGCCostume, slotDef->astrName );

	slot->astrCostume = NULL;
	eaiSetSize( &slot->eaColors, 4 );
	slot->eaColors[ 0 ] = 0;
	slot->eaColors[ 1 ] = 0;
	slot->eaColors[ 2 ] = 0;
	slot->eaColors[ 3 ] = 0;

	ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
}


void ugcCostumeEditorNeverwinter_SlotColorChangedCB( UIColorCombo *pCombo, UGCCostumeSlotColorUI* slotUI )
{
 	UGCCostumeEditorDoc *pDoc = slotUI->pDoc;
	UGCCostumeSlotDef* slotDef = slotUI->slotDef;
	UGCCostumeSlot* slot = ugcCostumeFindSlot( pDoc->pUGCCostume, slotDef->astrName );
 	Vec4 vColor;
	U8 color[4];
	
	// Get the color into the right format
 	ui_ColorComboGetColor( pCombo, vColor );
	VEC4_TO_COSTUME_COLOR( vColor, color );

	eaiSet( &slot->eaColors, u8ColorToRGBA( color ), slotUI->colorIndex );
	
	ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
}

void ugcCostumeEditorNeverwinter_SlotColorHoverCB( UIColorCombo *pCombo, bool bHover, Vec4 vColor, UGCCostumeSlotColorUI* slotUI )
{
 	UGCCostumeEditorDoc *pDoc = slotUI->pDoc;
	UGCCostumeSlotDef* slotDef = slotUI->slotDef;
	UGCCostumeSlot* slot = ugcCostumeFindSlot( pDoc->pUGCCostume, slotDef->astrName );
	U8 color[4];
 
	// Get the color into the right format
	VEC4_TO_COSTUME_COLOR( vColor, color );

	// Clear previous hover costume
	StructReset( parse_UGCCostumeOverride, &pDoc->hoverData );

	if( bHover ) {
		pDoc->hoverData.type = UGC_COSTUME_OVERRIDE_SLOT_COLOR;
		pDoc->hoverData.astrName = slotDef->astrName;
		pDoc->hoverData.colorIndex = slotUI->colorIndex;
		pDoc->hoverData.iValue = u8ColorToRGBA( color );
	}
}


static void ugcCostumeEditorNeverwinter_SlotTextCB( UIComboBox *pCombo, S32 iRow, bool bInBox, UserData userData, char **ppchOutput )
{
	const char* costumeName = eaGet( pCombo->model, iRow );
	if( !costumeName ) {
		ugcFormatMessageKey( ppchOutput, "UGC_CostumeEditor.FromPreset", STRFMT_END );
	} else {
		const WorldUGCProperties* ugcProps = ugcResourceGetUGCProperties( "PlayerCostume", costumeName );

		if( !ugcProps || !TranslateDisplayMessage( ugcProps->dVisibleName )) {
			estrPrintf( ppchOutput, "<UNTRANSLATED: %s>", costumeName );
		} else {
			estrPrintf( ppchOutput, "%s", TranslateDisplayMessage( ugcProps->dVisibleName ));
		}
	}
}


void ugcCostumeEditorNeverwinter_RefreshSlotTab( UGCCostumeEditorDoc *pDoc )
{
	PlayerCostume* basicCostume = ugcResourceGetCostumeMetadata( pDoc->pUGCCostume->data.astrPresetCostumeName )->pFullCostume;
	MEFieldContext* uiCtx = MEContextPush( "UGCCostumeEditor_SlotTab", NULL, NULL, NULL );
	UGCCostumeSkeletonSlotDef* skeletonDef = NULL;
	MEFieldContextEntry* entry;
	UIWidget* widget = NULL;

	if( basicCostume ) {
		skeletonDef = ugcDefaultsCostumeSkeletonDef( REF_STRING_FROM_HANDLE( basicCostume->hSkeleton ));
	}
	
	uiCtx->iXLabelStart = 0;
	uiCtx->iYLabelStart = 0;
	uiCtx->bLabelPaddingFromData = false;
	uiCtx->iXDataStart = 20;
	uiCtx->iYDataStart = UGC_ROW_HEIGHT - 10;
	uiCtx->iYStep = UGC_ROW_HEIGHT * 2 - 10;
	MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
	setVec2( uiCtx->iErrorIconOffset, 0, UGC_ROW_HEIGHT - 10 + 3 );
	uiCtx->bErrorIconOffsetFromRight = false;
	uiCtx->iErrorIconSpaceWidth = 0;

	{
		UIScrollArea* area = MEContextPushScrollAreaParent( "ScrollParent" );
		ui_WidgetSetDimensionsEx( UI_WIDGET( area ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		area->autosize = true;
	}

	if( skeletonDef ) {
		int slotIt;
		int colorIt;
			
		assert( eaSize( &skeletonDef->eaSlotDef ) < ARRAY_SIZE( pDoc->eaSlots ));
		for( slotIt = 0; slotIt != eaSize( &skeletonDef->eaSlotDef ); ++slotIt ) {
			UGCCostumeSlotDef* slotDef = skeletonDef->eaSlotDef[ slotIt ];
			UGCCostumeSlot* slot = ugcCostumeFindSlot( pDoc->pUGCCostume, slotDef->astrName );

			if( slot ) {
				MEFieldContext* curUICtx = MEContextPush( slotDef->astrName, slot, slot, parse_UGCCostumeSlot );
				UGCCostumeSlotUI* slotUI =  MEContextAllocMem( "Costume", sizeof( *slotUI ), NULL, false );
				UGCCostumeSlotDef* oldUISlotDef = slotUI->slotDef;
				
				slotUI->pDoc = pDoc;
				slotUI->slotDef = slotDef;
					
				curUICtx->cbChanged = ugcCostumeEditorNeverwinter_SlotChangedCB;
				curUICtx->pChangedData = slotUI;

				if( oldUISlotDef != slotDef ) {
					ugcCostumeEditorNeverwinter_UpdateSlotCostumes( &pDoc->eaSlots[ slotIt ], slotDef->astrName );
				}

				entry = MEContextAddDataProvided( kMEFieldType_Combo, NULL, &pDoc->eaSlots[ slotIt ], NULL, "Costume", TranslateMessageRef( slotDef->hDisplayName ), NULL );
				ui_ComboBoxSetTextCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_SlotTextCB, NULL );
				ui_ComboBoxSetHoverCallback( ENTRY_FIELD( entry )->pUICombo, ugcCostumeEditorNeverwinter_SlotHoverCB, slotUI );
				if( slot->astrCostume ) {
					MEContextEntryAddActionButton( entry, NULL, "UGC_Icons_Labels_Delete", ugcCostumeEditorNeverwinter_SlotRevertCB, slotUI, curUICtx->iWidgetHeight, "UGC_CostumeEditor.SlotReset.Tooltip" );
				}
				
				eaiSetSize( &slot->eaColors, 4 );
				for( colorIt = 0; colorIt < eaiSize( &slot->eaColors ); ++colorIt ) {
					char buffer[ 256 ];
					UGCCostumeSlotColorUI* colorUI;
					UIColorSet* pColorSet = RefSystem_ReferentFromString( "CostumeColors", "Character_Default_ColorSet" );
					U8 colors[ 4 ];
					sprintf( buffer, "Color%d", colorIt );

					colorUI = MEContextAllocMem( allocAddString( buffer ), sizeof( *colorUI ), NULL, false );
					colorUI->pDoc = pDoc;
					colorUI->slotDef = slotDef;
					colorUI->colorIndex = colorIt;
					RGBAToU8Color( colors, slot->eaColors[ colorIt ]);
					colors[3] = 0xFF;

					entry = MEContextAddCustom( allocAddString( buffer ));
					ugcRefreshColorCombo( (UIColorCombo**)&ENTRY_WIDGET( entry ), curUICtx->iXPos + curUICtx->iXDataStart + 25 * colorIt, curUICtx->iYPos, pColorSet, colors, colorUI, MEContextGetCurrent()->pUIContainer, ugcCostumeEditorNeverwinter_SlotColorChangedCB, ugcCostumeEditorNeverwinter_SlotColorHoverCB );
					widget = ENTRY_WIDGET( entry );
					ui_SetActive( widget, slot->astrCostume != NULL );
				}
				if( widget ) {
					MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget ) + 4;
				}

				MEContextPop( slotDef->astrName );
			}
		}
	}

	MEContextPop( "ScrollParent" );
	MEContextPop( "UGCCostumeEditor_SlotTab" );
}


void ugcCostumeEditorNeverwinter_RefreshPartColor( UGCCostumeEditorDoc* pDoc, CostumeEditLine* pData, int lineIt, int colorIdx, int colorIt, bool onRight, bool isSkin )
{
	MEFieldContext* uiCtx = MEContextGetCurrent();
	UGCCostumePart* pPart = uiCtx->id.ppNewData[0];
	UIColorSet* pColorSet = RefSystem_ReferentFromString( "CostumeColors", "Character_Default_ColorSet" );
	char entryName[ 256 ];
	const char* entryNamePooled;

	sprintf( entryName, "Line%d_Color%d", lineIt, colorIt );
	entryNamePooled = allocAddString( entryName );

	if( pColorSet ) {
		UGCCostumePartColorUI* colorUI = MEContextAllocMem( entryNamePooled, sizeof( *colorUI ), NULL, false );
		MEFieldContextEntry* entry;
		U8 u8color[4];

		if( isSkin ) {
			RGBAToU8Color( u8color, pDoc->pUGCCostume->data.skinColor );
		} else {
			RGBAToU8Color( u8color, pPart->colors[ colorIdx ]);
		}

		colorUI->pDoc = pDoc;
		colorUI->part = pPart;
		colorUI->colorIndex = colorIdx;
		colorUI->colorIsSkin = isSkin;

		entry = MEContextAddCustom( entryNamePooled );
		ugcRefreshColorCombo( (UIColorCombo**)&ENTRY_WIDGET( entry ), uiCtx->iXPos + uiCtx->iXDataStart + 25 * colorIt, uiCtx->iYPos, pColorSet, u8color, colorUI, uiCtx->pUIContainer, ugcCostumeEditorNeverwinter_PartColorChangedCB, ugcCostumeEditorNeverwinter_PartColorHoverCB );
		if( onRight ) {
			ui_WidgetSetPositionEx( ENTRY_WIDGET( entry ), 4 + 25 * colorIt, uiCtx->iYPos + uiCtx->iYDataStart, 0, 0, UITopRight );
		}
	}
}


void ugcCostumeEditorNeverwinter_ToggleAdvancedMode( UIButton* ignored, UGCCostumeEditorDoc* pDoc )
{
	if( pDoc->pUGCCostume->data.isAdvanced ) {
		pDoc->pUGCCostume->data.isAdvanced = false;
	} else {
		if( UIYes == ugcModalDialogMsg( "UGC_CostumeEditor.ToggleAdvanced_ToAdvanced_Title", "UGC_CostumeEditor.ToggleAdvanced_ToAdvanced_Details", UIYes | UINo )) {
			pDoc->pUGCCostume->data.isAdvanced = true;
		}
	}
	ugcCostumeEditor_FieldChangeCB( NULL, true, pDoc );
}

UGCNoCostumesEditorDoc *ugcNoCostumesEditor_Open( void )
{
	UGCNoCostumesEditorDoc* pDoc = calloc( 1, sizeof( *pDoc ));
	pDoc->pRootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	return pDoc;
}

void ugcNoCostumesEditor_UpdateUI(UGCNoCostumesEditorDoc *pDoc)
{
	UIPane* pane;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	
	MEContextPush( "UGCNoCostumesEditor", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( pDoc->pRootPane ));

	pane = MEContextPushPaneParent( "FTUE" );
	{
		entry = MEContextAddLabelMsg( "Text", "UGC_CostumeEditor.FTUEAddCostume", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ENTRY_LABEL( entry )->textFrom = UITop;
		ui_WidgetSetFont( widget, "UGC_Important_Alternate" );
		ui_WidgetSetPositionEx( widget, 0, -UGC_ROW_HEIGHT, 0, 0.5, UITop );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

		entry = MEContextAddButtonMsg( "UGC_CostumeEditor.AddCostume", "UGC_Icons_Labels_New_02", ugcEditorCreateNewCostume, NULL, "Button", NULL, "UGC_CostumeEditor.AddCostume.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0.5, UITop );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ENTRY_BUTTON( entry )->bCenterImageAndText = true;
		widget->width = MAX( widget->width, 200 );
		widget->height = MAX( widget->height, 50 );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
	}
	MEContextPop( "FTUE" );
	ui_PaneSetStyle( pane, "UGC_Story_BackgroundArea", true, false );
	ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );

	MEContextPop( "UGCNoCostumesEditor" );
}

void ugcNoCostumesEditor_SetVisible(UGCNoCostumesEditorDoc *pDoc)
{
	ugcEditorSetDocPane( pDoc->pRootPane );
}

void ugcNoCostumesEditor_OncePerFrame(UGCNoCostumesEditorDoc *pDoc)
{
	// nothing to do
}

void ugcNoCostumesEditor_Close(UGCNoCostumesEditorDoc **ppDoc)
{
	if( *ppDoc ) {
		MEContextDestroyByName( "UGCNoCostumesEditor" );
		ui_WidgetQueueFreeAndNull( &(*ppDoc)->pRootPane );
		SAFE_FREE( *ppDoc );
	}
}

void ugcNoCostumesEditor_HandleAction(UGCNoCostumesEditorDoc *pDoc, UGCActionID action)
{
	// nothing to do
}

bool ugcNoCostumesEditor_QueryAction(UGCNoCostumesEditorDoc *pDoc, UGCActionID action, char** out_estr)
{
	// nothing to do
	return false;
}

// Auto-Struct import
#include "NNOUGCCostumeEditor_h_ast.c"

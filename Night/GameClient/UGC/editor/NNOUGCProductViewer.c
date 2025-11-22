#include "UGCProductViewer.h"

#include "CharacterSelection.h"
#include "EditLibUIUtil.h"
#include "GameAccountDataCommon.h"
#include "GfxHeadshot.h"
#include "GfxSprite.h"
#include "GlobalComm.h"
#include "Login2Common.h"
#include "LoginCommon.h"
#include "NNOUGCCommon.h"
#include "StringFormat.h"
#include "UGCEditorMain.h"
#include "UGCProjectChooser.h"
#include "UIButton.h"
#include "UICore.h"
#include "UIGen.h"
#include "UILabel.h"
#include "UIList.h"
#include "UIPane.h"
#include "UITextEntry.h"
#include "UIWindow.h"
#include "dynBitField.h"
#include "gclBaseStates.h"
#include "gclLogin.h"
#include "globalstatemachine.h"
#include "itemCommon.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_UISystem););

extern Login2CharacterSelectionData *g_characterSelectionData;

#define STANDARD_ROW_HEIGHT  40

AUTO_STRUCT;
typedef struct UGCProductData
{
	char* pchName;
	char* pchDisplayName;
	char* pchDescription;
	REF_TO(ItemDef) hNumeric; AST(REFDICT(ItemDef))
	S32 iCost;
	bool bCanPurchase;
} UGCProductData;
extern ParseTable parse_UGCProductData[];
#define TYPE_parse_UGCProductData UGCProductData

AUTO_STRUCT;
typedef struct UGCProductViewerCharacterData
{
	char* pchName;
	U32 uEntID;
	REF_TO(ItemDef) hNumeric; AST(REFDICT(ItemDef))
	S32 iNumericValue;
} UGCProductViewerCharacterData;
extern ParseTable parse_UGCProductViewerCharacterData[];
#define TYPE_parse_UGCProductViewerCharacterData UGCProductViewerCharacterData

typedef struct UGCProductViewer
{
	UIPane* pBGPane;
	UIPane* pListPane;
	UIPane* pButtonsPane;
	UIWindow* pModalWindow;

	UILabel* pTitleLabel;
	UILabel* pPaymentLabel;
	UILabel* pUGCSlotLabel;
	UILabel* pPurchaseResultLabel;
	UIButton* pBuyButton;
	UIButton* pLeaveButton;
	UIList* pProductList;
	UIList* pCharacterList;

	UGCProductData** eaProducts;
	UGCProductViewerCharacterData** eaCharacters;
	U32 uResultTimeStart;
	S32 iUGCSlots;
	bool bQuit;
	bool bPurchaseSuccessful;
} UGCProductViewer;

UGCProductViewer* g_UGCProductViewer = NULL;

static void UGCProductViewer_CloseModalWindow(void);

void UGCProductViewer_Destroy(void)
{
	UGCProductViewer_CloseModalWindow();

	eaDestroyStruct(&g_UGCProductViewer->eaProducts, parse_UGCProductData);
	eaDestroyStruct(&g_UGCProductViewer->eaCharacters, parse_UGCProductViewerCharacterData);
	ui_ListSetModel(g_UGCProductViewer->pProductList, NULL, NULL);
	ui_ListSetModel(g_UGCProductViewer->pCharacterList, NULL, NULL);
	ui_WidgetQueueFreeAndNull(&g_UGCProductViewer->pBGPane);
	SAFE_FREE(g_UGCProductViewer);
}

static void UGCProductViewer_CloseModalWindow(void)
{
	if (g_UGCProductViewer && g_UGCProductViewer->pModalWindow)
	{
		ui_WindowClose(g_UGCProductViewer->pModalWindow);
		ui_WidgetQueueFreeAndNull(&g_UGCProductViewer->pModalWindow);
	}
}

// LOGIN2UGC - figure out how UGC slot purchases work
static void UGCProductViewer_GameAccountMakeNumericPurchase(SA_PARAM_OP_VALID PossibleCharacterChoice* pChoice, const char* pchDefName)
{
	GameAccountDataNumericPurchaseDef* pDef = GAD_NumericPurchaseDefFromName(pchDefName);

	if (GSM_IsStateActiveOrPending(GCL_LOGIN_USER_BROWSING_UGC_PRODUCTS))
	{
		if (pChoice && !pChoice->iVirtualShardID && pDef)
		{
			GameAccountData* pData = entity_GetGameAccount(NULL);

			if (pData && GAD_PossibleCharacterCanMakeNumericPurchase(pChoice->eaNumerics, pChoice->iVirtualShardID, pData, pDef, true))
			{
				Packet *pPak = pktCreate(gpLoginLink, TOLOGIN_GAME_ACCOUNT_MAKE_NUMERIC_PURCHASE);
				pktSendU32(pPak, pChoice->iID);
				pktSendU32(pPak, pData->iAccountID);
				pktSendString(pPak, pchDefName);
				pktSend(&pPak);	
			}
		}
	}
}

static void UGCProductViewer_ModalBuyWindow_OKClicked(UIButton* pButton, void* pUnused)
{
	if (g_UGCProductViewer && g_UGCProductViewer->pProductList && g_UGCProductViewer->pCharacterList)
	{
		S32 iProductRow = ui_ListGetSelectedRow(g_UGCProductViewer->pProductList);
		S32 iCharacterRow = ui_ListGetSelectedRow(g_UGCProductViewer->pCharacterList);
		UGCProductData* pProductData = eaGet(&g_UGCProductViewer->eaProducts, iProductRow);
		UGCProductViewerCharacterData* pCharacterData = eaGet(&g_UGCProductViewer->eaCharacters, iCharacterRow);
		PossibleCharacterChoice* pChoice = NULL;

        if (pProductData && pCharacterData)
        {
            //S32 i;
            // LOGIN2UGC - need to figure out how to do ugc slot purchases
            //for (i = 0; i < eaSize(&g_pCharacterChoices->ppChoices); i++)
            //{
            //    if (g_pCharacterChoices->ppChoices[i]->iID == pCharacterData->uEntID)
            //    {
            //        pChoice = g_pCharacterChoices->ppChoices[i];
            //        break;
            //    }
            //}
        }

		if (pChoice)
		{
			UGCProductViewer_GameAccountMakeNumericPurchase(pChoice, pProductData->pchName);
		}
	}

	UGCProductViewer_CloseModalWindow();
}

static void UGCProductViewer_ModalBuyWindow_CancelClicked(UIButton* pButton, void* pUnused)
{
	UGCProductViewer_CloseModalWindow();
}

static void UGCProductViewer_CreateModalBuyWindow(UIButton* pButton, void* pUnused)
{
	char pchPurchaseConfirmText[256];
	UILabel* pLabel;
	UIButton* pOKButton;
	UIButton* pCancelButton;
	S32 iProductRow = ui_ListGetSelectedRow(g_UGCProductViewer->pProductList);
	S32 iCharacterRow = ui_ListGetSelectedRow(g_UGCProductViewer->pCharacterList);
	UGCProductData* pProductData = eaGet(&g_UGCProductViewer->eaProducts, iProductRow);
	UGCProductViewerCharacterData* pCharacterData = eaGet(&g_UGCProductViewer->eaCharacters, iCharacterRow);
	const char* pchCharName = EMPTY_TO_NULL(SAFE_MEMBER(pCharacterData, pchName));
	const char* pchProductName = EMPTY_TO_NULL(SAFE_MEMBER(pProductData, pchDisplayName));
	const char* pchNumeric = NULL;
	S32 iCost = SAFE_MEMBER(pProductData, iCost);

	if (pProductData)
	{
		ItemDef* pNumericItemDef = GET_REF(pProductData->hNumeric);
		if (pNumericItemDef) {
			pchNumeric = TranslateDisplayMessage(pNumericItemDef->displayNameMsg);
		}
		if (!pchNumeric) {
			pchNumeric = REF_STRING_FROM_HANDLE(pProductData->hNumeric);
		}
	}

	g_UGCProductViewer->pModalWindow = ui_WindowCreate("Confirm Purchase", 0, 0, 375, 150);

	sprintf(pchPurchaseConfirmText, "%s is about to purchase %s for %d %s. You will be able to access this purchase from any character on this account. Are you sure you want to make this purchase?", pchCharName, pchProductName, iCost, pchNumeric);

	pLabel = ui_LabelCreate(pchPurchaseConfirmText, 0, 0);
	ui_LabelSetWordWrap(pLabel, 1);
	ui_WidgetSetDimensionsEx(UI_WIDGET(pLabel), 1.0f, 1.0f, UIUnitPercentage, UIUnitPercentage);
	ui_WidgetSetPositionEx(UI_WIDGET(pLabel), 0, 0, 0, 0, UITop);
	ui_WindowAddChild(g_UGCProductViewer->pModalWindow, pLabel);

	pCancelButton = ui_ButtonCreate("Cancel", 0, 0, UGCProductViewer_ModalBuyWindow_CancelClicked, NULL);
	ui_WidgetSetDimensions(UI_WIDGET(pCancelButton), 80, 30);
	ui_WidgetSetPositionEx(UI_WIDGET(pCancelButton), 0, 0, 0, 0, UIBottomRight);
	ui_WindowAddChild(g_UGCProductViewer->pModalWindow, pCancelButton);

	pOKButton = ui_ButtonCreate("OK", 0, 0, UGCProductViewer_ModalBuyWindow_OKClicked, NULL);
	ui_WidgetSetDimensions(UI_WIDGET(pOKButton), 80, 30);
	ui_WidgetSetPositionEx(UI_WIDGET(pOKButton), 90, 0, 0, 0, UIBottomRight);
	ui_WindowAddChild(g_UGCProductViewer->pModalWindow, pOKButton);

	ui_WindowSetModal(g_UGCProductViewer->pModalWindow, true);
	ui_WindowSetResizable(g_UGCProductViewer->pModalWindow, false);
	elUICenterWindow(g_UGCProductViewer->pModalWindow);
	ui_WindowShowEx(g_UGCProductViewer->pModalWindow, true);
}

static void UGCProductViewer_LeaveCB(UIButton* pButton, void* pUnused)
{
	if (g_UGCProductViewer)
	{
		g_UGCProductViewer->bQuit = true;
	}
}

static void UGCProductViewer_ProductListRowSelected(UIAnyWidget* pWidget, void* pUnused)
{
	UGCProductViewer_Refresh();
}

static void UGCProductViewer_CharacterListRowSelected(UIAnyWidget* pWidget, void* pUnused)
{
	UGCProductViewer_Refresh();
}

static void UGCProductViewer_SetListColProductText(UIList* pList, UIListColumn* pColumn, S32 iRow, void* pUnused, char** estrOutput)
{
	UGCProductData* pData = (UGCProductData*)eaGet(pList->peaModel, iRow);
	if (pData)
	{
		estrPrintf(estrOutput, "%s", NULL_TO_EMPTY(pData->pchDisplayName));
	}
}

static void UGCProductViewer_SetListColDescriptionText(UIList* pList, UIListColumn* pColumn, S32 iRow, void* pUnused, char** estrOutput)
{
	UGCProductData* pData = (UGCProductData*)eaGet(pList->peaModel, iRow);
	if (pData)
	{
		estrPrintf(estrOutput, "%s", NULL_TO_EMPTY(pData->pchDescription));
	}
}

static void UGCProductViewer_SetListColCharacterText(UIList* pList, UIListColumn* pColumn, S32 iRow, void* pUnused, char** estrOutput)
{
	UGCProductViewerCharacterData* pData = (UGCProductViewerCharacterData*)eaGet(pList->peaModel, iRow);
	if (pData)
	{
		estrPrintf(estrOutput, "%s", NULL_TO_EMPTY(pData->pchName));
	}
}

static void UGCProductViewer_SetListColProductCost(UIList* pList, UIListColumn* pColumn, S32 iRow, void* pUnused, char** estrOutput)
{
	UGCProductData* pData = (UGCProductData*)eaGet(pList->peaModel, iRow);
	if (pData)
	{
		ItemDef* pNumericItemDef = GET_REF(pData->hNumeric);
		const char* pchNumericDisplayName = NULL;
		if (pNumericItemDef)
		{
			pchNumericDisplayName = TranslateDisplayMessage(pNumericItemDef->displayNameMsg);
		}
		if (!pchNumericDisplayName)
		{
			pchNumericDisplayName = REF_STRING_FROM_HANDLE(pData->hNumeric);
		}
		estrPrintf(estrOutput, "%d %s", pData->iCost, NULL_TO_EMPTY(pchNumericDisplayName));
	}
}

static void UGCProductViewer_SetListColCharacterNumericValue(UIList* pList, UIListColumn* pColumn, S32 iRow, void* pUnused, char** estrOutput)
{
	UGCProductViewerCharacterData* pData = (UGCProductViewerCharacterData*)eaGet(pList->peaModel, iRow);
	if (pData)
	{
		ItemDef* pNumericItemDef = GET_REF(pData->hNumeric);
		const char* pchNumericDisplayName = NULL;
		if (pNumericItemDef)
		{
			pchNumericDisplayName = TranslateDisplayMessage(pNumericItemDef->displayNameMsg);
		}
		if (!pchNumericDisplayName)
		{
			pchNumericDisplayName = REF_STRING_FROM_HANDLE(pData->hNumeric);
		}
		estrPrintf(estrOutput, "%d %s", pData->iNumericValue, NULL_TO_EMPTY(pchNumericDisplayName));
	}
}

static void UGCProductViewer_GenerateProductList(void)
{
	const GameAccountData *pAccountData = entity_GetGameAccount(NULL);
	int i, iCount = 0;

	if (pAccountData)
	{
		for (i = 0; i < eaSize(&g_GameAccountDataNumericPurchaseDefs.eaDefs); i++)
		{
			GameAccountDataNumericPurchaseDef* pDef = g_GameAccountDataNumericPurchaseDefs.eaDefs[i];
		
			if (pDef->eCategory != kGameAccountDataNumericPurchaseCategory_UGC)
			{
				continue;
			}
			if (GAD_CanMakeNumericPurchaseCheckKeyValues(pAccountData, pDef))
			{
				UGCProductData* pData = eaGetStruct(&g_UGCProductViewer->eaProducts, parse_UGCProductData, iCount++);
				StructCopyString(&pData->pchName, pDef->pchName);
				StructCopyString(&pData->pchDisplayName, TranslateDisplayMessage(pDef->msgDisplayName));
				StructCopyString(&pData->pchDescription, TranslateDisplayMessage(pDef->msgDescription));
				SET_HANDLE_FROM_STRING("ItemDef", pDef->pchNumericItemDef, pData->hNumeric);
				pData->iCost = pDef->iNumericCost;
				pData->bCanPurchase = gclLogin_GameAccountCanMakeNumericPurchaseWithAnyCharacter(pDef);
			}
		}
	}
	eaSetSizeStruct(&g_UGCProductViewer->eaProducts, parse_UGCProductData, iCount);
}

static void UGCProductViewer_GenerateCharacterList(void)
{
	UGCProductData* pProductData = (UGCProductData*)eaGet(&g_UGCProductViewer->eaProducts, ui_ListGetSelectedRow(g_UGCProductViewer->pProductList));
	GameAccountDataNumericPurchaseDef* pDef = NULL;
    int iCount = 0;
	int iTextureSize = 512;

	if (pProductData)
	{
		pDef = GAD_NumericPurchaseDefFromName(pProductData->pchName);
	}
    // LOGIN2UGC - figure out slot purchase
    //if (pDef && g_pCharacterChoices)
	//{
    //	int i;
	//	for (i = 0; i < eaSize(&g_pCharacterChoices->ppChoices); i++)
	//	{
	//		PossibleCharacterChoice* pChoice = g_pCharacterChoices->ppChoices[i];
	//		if (!pChoice->iVirtualShardID)
	//		{
	//			if (GAD_PossibleCharacterCanMakeNumericPurchase(pChoice, NULL, pDef, false))
	//			{
	//				PossibleCharacterNumeric* pNumeric = eaIndexedGetUsingString(&pChoice->eaNumerics, REF_STRING_FROM_HANDLE(pProductData->hNumeric));
	//				UGCProductViewerCharacterData* pData = eaGetStruct(&g_UGCProductViewer->eaCharacters, parse_UGCProductViewerCharacterData, iCount++);
	//				pData->uEntID = pChoice->iID;
	//				StructCopyString(&pData->pchName, pChoice->name);
	//				COPY_HANDLE(pData->hNumeric, pProductData->hNumeric);
	//				pData->iNumericValue = SAFE_MEMBER(pNumeric, iNumericValue);
	//			}
	//		}
	//	}
	//}
	eaSetSizeStruct(&g_UGCProductViewer->eaCharacters, parse_UGCProductViewerCharacterData, iCount);
}

#define UGCPRODUCTVIEWER_COLUMN_WIDTH 262

static void UGCProductViewer_UpdateLists(void)
{
	F32 y = 0;
	
	if (!g_UGCProductViewer->pProductList)
	{
		UIListColumn* pColumn;
		g_UGCProductViewer->pProductList = ui_ListCreate(NULL, NULL, 40);
		ui_ListSetMultiselect(g_UGCProductViewer->pProductList, false);
		ui_ListSetSelectedCallback(g_UGCProductViewer->pProductList, UGCProductViewer_ProductListRowSelected, NULL);
		g_UGCProductViewer->pProductList->fHeaderHeight = 0;
	
		pColumn = ui_ListColumnCreateText("", UGCProductViewer_SetListColProductText, NULL);
		pColumn->fWidth = UGCPRODUCTVIEWER_COLUMN_WIDTH;
		ui_ListAppendColumn(g_UGCProductViewer->pProductList, pColumn);
		pColumn = ui_ListColumnCreateText("", UGCProductViewer_SetListColDescriptionText, NULL);
		pColumn->fWidth = UGCPRODUCTVIEWER_COLUMN_WIDTH;
		ui_ListAppendColumn(g_UGCProductViewer->pProductList, pColumn);
		pColumn = ui_ListColumnCreateText("", UGCProductViewer_SetListColProductCost, NULL);
		pColumn->fWidth = UGCPRODUCTVIEWER_COLUMN_WIDTH;
		ui_ListAppendColumn(g_UGCProductViewer->pProductList, pColumn);

		ui_ListSetModel(g_UGCProductViewer->pProductList, parse_UGCProductData, &g_UGCProductViewer->eaProducts);
		ui_PaneAddChild(g_UGCProductViewer->pListPane, g_UGCProductViewer->pProductList);
	}

	if (ui_ListGetSelectedRow(g_UGCProductViewer->pProductList) < 0)
	{
		ui_ListSetSelectedRow(g_UGCProductViewer->pProductList, 0);
	}
	ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pProductList), 0, y, 0.0f, 0.0f, UITop);
	ui_WidgetSetDimensionsEx(UI_WIDGET(g_UGCProductViewer->pProductList), 1.0, 0.45, UIUnitPercentage, UIUnitPercentage);
	UGCProductViewer_GenerateProductList();

	if (!g_UGCProductViewer->pCharacterList)
	{
		UIListColumn* pColumn;
		g_UGCProductViewer->pCharacterList = ui_ListCreate(NULL, NULL, 40);
		ui_ListSetMultiselect(g_UGCProductViewer->pCharacterList, false);
		ui_ListSetSelectedCallback(g_UGCProductViewer->pCharacterList, UGCProductViewer_CharacterListRowSelected, NULL);
		g_UGCProductViewer->pCharacterList->fHeaderHeight = 0;

		pColumn = ui_ListColumnCreateText("", UGCProductViewer_SetListColCharacterText, NULL);
		pColumn->fWidth = UGCPRODUCTVIEWER_COLUMN_WIDTH;
		ui_ListAppendColumn(g_UGCProductViewer->pCharacterList, pColumn);
		pColumn = ui_ListColumnCreateText("", UGCProductViewer_SetListColCharacterNumericValue, NULL);
		ui_ListAppendColumn(g_UGCProductViewer->pCharacterList, pColumn);

		ui_ListSetModel(g_UGCProductViewer->pCharacterList, parse_UGCProductViewerCharacterData, &g_UGCProductViewer->eaCharacters);
		ui_PaneAddChild(g_UGCProductViewer->pListPane, g_UGCProductViewer->pCharacterList);
	}
	if (ui_ListGetSelectedRow(g_UGCProductViewer->pCharacterList) < 0)
	{
		ui_ListSetSelectedRow(g_UGCProductViewer->pCharacterList, 0);
	}

	ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pCharacterList), 0, y, 0.0f, 0.0f, UIBottom);
	ui_WidgetSetDimensionsEx(UI_WIDGET(g_UGCProductViewer->pCharacterList), 1.0, 0.45, UIUnitPercentage, UIUnitPercentage);
	UGCProductViewer_GenerateCharacterList();
}

static void UGCProductViewer_UpdateButtons(void)
{
	if (!g_UGCProductViewer->pLeaveButton)
		{
		g_UGCProductViewer->pLeaveButton = ui_ButtonCreate("", 0, 0, UGCProductViewer_LeaveCB, NULL);
		ui_WidgetSetTextMessage( UI_WIDGET( g_UGCProductViewer->pLeaveButton ), "UGC_ProductViewer.Leave" );
		ui_WidgetSetDimensions(UI_WIDGET(g_UGCProductViewer->pLeaveButton), 150, 40);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pLeaveButton), 20, 20, 0, 0, UIBottomRight);
		ui_PaneAddChild(g_UGCProductViewer->pButtonsPane, g_UGCProductViewer->pLeaveButton);
	}
	if (!g_UGCProductViewer->pBuyButton)
	{
		g_UGCProductViewer->pBuyButton = ui_ButtonCreate("", 0, 0, UGCProductViewer_CreateModalBuyWindow, NULL);
		ui_WidgetSetTextMessage( UI_WIDGET( g_UGCProductViewer->pBuyButton ), "UGC_ProductViewer.Buy" );
		ui_WidgetSetDimensions(UI_WIDGET(g_UGCProductViewer->pBuyButton), 150, 40);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pBuyButton), 190, 20, 0, 0, UIBottomRight);
		ui_PaneAddChild(g_UGCProductViewer->pButtonsPane, g_UGCProductViewer->pBuyButton);
	}
	// Update states
	{
		S32 iProductRow = ui_ListGetSelectedRow(g_UGCProductViewer->pProductList);
		S32 iCharacterRow = ui_ListGetSelectedRow(g_UGCProductViewer->pCharacterList);
		UGCProductData* pProductData = eaGet(&g_UGCProductViewer->eaProducts, iProductRow);
		UGCProductViewerCharacterData* pCharacterData = eaGet(&g_UGCProductViewer->eaCharacters, iCharacterRow);
		PossibleCharacterChoice* pChoice = NULL;

        // LOGIN2UGC
		//if (pProductData && pCharacterData)
		//{
		//	S32 i;
		//	for (i = 0; i < eaSize(&g_pCharacterChoices->ppChoices); i++)
		//	{
		//		if (g_pCharacterChoices->ppChoices[i]->iID == pCharacterData->uEntID)
		//		{
		//			pChoice = g_pCharacterChoices->ppChoices[i];
		//			break;
		//		}
		//	}
		//}

		if (pChoice)
		{
			GameAccountData* pData = entity_GetGameAccount(NULL);
			GameAccountDataNumericPurchaseDef* pDef = GAD_NumericPurchaseDefFromName(pProductData->pchName);
			ui_SetActive(UI_WIDGET(g_UGCProductViewer->pBuyButton), GAD_PossibleCharacterCanMakeNumericPurchase(pChoice->eaNumerics, pChoice->iVirtualShardID, pData, pDef, true));
		}
		else
		{
			ui_SetActive(UI_WIDGET(g_UGCProductViewer->pBuyButton), false);
		}
	}
}

static void UGCProductViewer_UpdateLabels(void)
{
	char* estr = NULL;
	GameAccountData* pAccountData = entity_GetGameAccount(NULL);
	F32 y = 0;

	g_UGCProductViewer->iUGCSlots = Login2_UGCGetProjectMaxSlots(pAccountData);

	if (!g_UGCProductViewer->pTitleLabel)
	{
		g_UGCProductViewer->pTitleLabel = ui_LabelCreate("", 0, 0);
		ui_WidgetSetTextMessage( UI_WIDGET( g_UGCProductViewer->pTitleLabel ), "UGC_ProductViewer.Header" );
		ui_WidgetSetDimensionsEx(UI_WIDGET(g_UGCProductViewer->pTitleLabel), 1.0f, 40, UIUnitPercentage, UIUnitFixed);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pTitleLabel), 0, y, 0, 0, UITopLeft);
		ui_PaneAddChild(g_UGCProductViewer->pBGPane, g_UGCProductViewer->pTitleLabel);
	}

	y += 200;

	if (!g_UGCProductViewer->pPaymentLabel)
	{
		g_UGCProductViewer->pPaymentLabel = ui_LabelCreate("", 0, 0);
		ui_WidgetSetTextMessage( UI_WIDGET( g_UGCProductViewer->pPaymentLabel ), "UGC_ProductViewer.PaymentSource" );
		ui_WidgetSetDimensionsEx(UI_WIDGET(g_UGCProductViewer->pPaymentLabel), 1.0f, 40, UIUnitPercentage, UIUnitFixed);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pPaymentLabel), 0, y, 0, 0, UITopLeft);
		ui_PaneAddChild(g_UGCProductViewer->pBGPane, g_UGCProductViewer->pPaymentLabel);
	}

	y = 100;

	if (!g_UGCProductViewer->pUGCSlotLabel)
	{
		g_UGCProductViewer->pUGCSlotLabel = ui_LabelCreate("", 0, 0);
		ui_WidgetSetDimensionsEx(UI_WIDGET(g_UGCProductViewer->pUGCSlotLabel), 1.0f, 20, UIUnitPercentage, UIUnitFixed);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pUGCSlotLabel), 0, y, 0, 0, UIBottom);
		ui_PaneAddChild(g_UGCProductViewer->pBGPane, g_UGCProductViewer->pUGCSlotLabel);
	}

	y -= 20;

	if (!g_UGCProductViewer->pPurchaseResultLabel)
	{
		g_UGCProductViewer->pPurchaseResultLabel = ui_LabelCreate("", 0, 0);
		ui_WidgetSetDimensionsEx(UI_WIDGET(g_UGCProductViewer->pPurchaseResultLabel), 1.0f, 20, UIUnitPercentage, UIUnitFixed);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pPurchaseResultLabel), 0, y, 0, 0, UIBottom);
		ui_PaneAddChild(g_UGCProductViewer->pBGPane, g_UGCProductViewer->pPurchaseResultLabel);
	}

	if (g_UGCProductViewer->uResultTimeStart > 0)
	{
		if (g_UGCProductViewer->bPurchaseSuccessful) {
			ui_LabelSetMessage(g_UGCProductViewer->pPurchaseResultLabel, "UGC_ProductViewer.PurchaseSuccessful");
		} else {
			ui_LabelSetMessage(g_UGCProductViewer->pPurchaseResultLabel, "UGC_ProductViewer.PurchaseFailed");
		}
	}
	else
	{
		ui_LabelSetText(g_UGCProductViewer->pPurchaseResultLabel, "");
	}

	if (g_UGCProductViewer->iUGCSlots > 0)
	{
		ugcFormatMessageKey( &estr, "UGC_ProductViewer.NumSlots",
							 STRFMT_INT( "NumProjectSlots", g_UGCProductViewer->iUGCSlots ),
							 STRFMT_INT( "NumSeriesSlot", -1 ),
							 STRFMT_END );
		ui_LabelSetText(g_UGCProductViewer->pUGCSlotLabel, estr);
	}
	else
	{
		ui_LabelSetMessage( g_UGCProductViewer->pUGCSlotLabel, "UGC_ProducViewer.NoSlots" );
	}

	estrDestroy( &estr );
}

void UGCProductViewer_Refresh(void)
{
	if(!g_UGCProductViewer) {
		g_UGCProductViewer = calloc(1, sizeof(UGCProductViewer));
	}
	
	if (!g_UGCProductViewer->pBGPane) {
		g_UGCProductViewer->pBGPane = ui_PaneCreate(0, 0, 800, 540, UIUnitFixed, UIUnitFixed, 0);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pBGPane), 0, 0, 0, 0, UINoDirection);
		ui_WidgetAddToDevice(UI_WIDGET(g_UGCProductViewer->pBGPane), NULL);
		ui_PaneSetStyle(g_UGCProductViewer->pBGPane, "CarbonFibre_SharpBackgroundDark", true, false);
	}

	if (!g_UGCProductViewer->pListPane) {
		g_UGCProductViewer->pListPane = ui_PaneCreate(0, 0, 1, 360, UIUnitPercentage, UIUnitFixed, 0);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pListPane), 0, 30, 0, 0, UITop);
		g_UGCProductViewer->pListPane->invisible = true;
		ui_PaneAddChild(g_UGCProductViewer->pBGPane, g_UGCProductViewer->pListPane);
	}

	UGCProductViewer_UpdateLists();

	if (!g_UGCProductViewer->pButtonsPane) {
		g_UGCProductViewer->pButtonsPane = ui_PaneCreate(0, 0, 1, 60, UIUnitPercentage, UIUnitFixed, 0);
		ui_WidgetSetPositionEx(UI_WIDGET(g_UGCProductViewer->pButtonsPane), 0, 0, 0, 0, UIBottom);
		g_UGCProductViewer->pButtonsPane->invisible = true;
		ui_PaneAddChild(g_UGCProductViewer->pBGPane, g_UGCProductViewer->pButtonsPane);
	}

	UGCProductViewer_UpdateButtons();

	UGCProductViewer_UpdateLabels();
}

void UGCProductViewer_OncePerFrame(void)
{
	if (g_UGCProductViewer)
	{
		if (g_UGCProductViewer->bQuit)
		{
			gclLogin_BrowseUGCProducts(false);
		}
		else
		{
			GameAccountData* pAccountData = entity_GetGameAccount(NULL);
			S32 iUGCSlots = Login2_UGCGetProjectMaxSlots(pAccountData);
			bool bRefresh = false;

			if (g_UGCProductViewer->uResultTimeStart > 0)
			{
				const U32 uResultDisplayTime = 5000;

				if (g_UGCProductViewer->uResultTimeStart + uResultDisplayTime < g_ui_State.totalTimeInMs)
				{
					g_UGCProductViewer->uResultTimeStart = 0;
					bRefresh = true;
				}
			}

			if (g_UGCProductViewer->iUGCSlots != iUGCSlots)
			{
				bRefresh = true;
			}

			if (bRefresh)
			{
				UGCProductViewer_Refresh();
			}
		}
	}
}

void UGCProductViewer_SetPurchaseResult(U32 uEntID, const char* pchProductDef, bool bSuccess)
{
	GameAccountDataNumericPurchaseDef* pDef = GAD_NumericPurchaseDefFromName(pchProductDef);
	if (pDef && g_UGCProductViewer /*&& g_pCharacterChoices*/)
	{
		if (bSuccess)
		{
			PossibleCharacterChoice* pChoice = NULL;
            // LOGIN2UGC
			//int i;
			//for (i = 0; i < eaSize(&g_pCharacterChoices->ppChoices); i++)
			//{
			//	if (g_pCharacterChoices->ppChoices[i]->iID == uEntID)
			//	{
			//		pChoice = g_pCharacterChoices->ppChoices[i];
			//		break;
			//	}
			//}
			if (pChoice)
			{
				PossibleCharacterNumeric* pNumeric = eaIndexedGetUsingString(&pChoice->eaNumerics, pDef->pchNumericItemDef);
				if (pNumeric)
				{
					pNumeric->iNumericValue -= pDef->iNumericCost;
				}
			}
		}
		g_UGCProductViewer->uResultTimeStart = g_ui_State.totalTimeInMs;
		g_UGCProductViewer->bPurchaseSuccessful = bSuccess;
		UGCProductViewer_Refresh();
	}
}

#include "AutoGen/NNOUGCProductViewer_c_ast.c"

#include "UGCProjectChooser.h"

#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "Login2Common.h"
#include "LoginCommon.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCProjectChooser.h"
#include "NNOUGCSeriesEditor.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UGCProjectCommon.h"
#include "UGCSeriesEditor.h"
#include "UIPane.h"
#include "UISprite.h"
#include "UITextureAssembly.h"
#include "UIWindow.h"
#include "crypt.h"
#include "gclBaseStates.h"
#include "gclLogin.h"
#include "gclUGC.h"
#include "Organization.h"

#include "AutoGen/UGCProjectCommon_h_ast.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

/// Structure for the list model.
///
/// The data pointed to isn't owned, it points into the
/// PossibleUGCProjects in UGCProjectChooser.
typedef struct UGCProjectChooserEntry
{
	PossibleUGCProject* pProject;
	UGCProjectSeries* pSeries;
	int indentLevel;
} UGCProjectChooserEntry;

typedef enum UGCProjectChooserTab
{
	UGCProjectChooserTab_Main,
	UGCProjectChooserTab_Map,
	UGCProjectChooserTab_Reviews,
} UGCProjectChooserTab;

typedef struct UGCProjectChooser
{
	// Global UI
	UGCProjectChooserMode eMode;
	UIPane* pRootPane;
	UIList* pProjectList;

	// Model data
	PossibleUGCProjects* pPossibleUGCProjects;
	UGCProjectChooserEntry** eaModel;
	int numProjectSlots;
	int numSeriesSlots;
	int numProjects;
	int numSeries;
	UGCProjectChooserTab activeTab;
} UGCProjectChooser;

static UGCProjectChooser *g_UGCProjectChooser = NULL;

static void ugcProjectChooser_Refresh( void );
static void ugcProjectChooser_RefreshModel( void );
static void ugcProjectChooser_BackClickedCB( UIButton* ignored, UserData ignored2 );
static void ugcProjectChooser_CreateProjectCB( UIWidget* ignored, UserData ignored2 );
static void ugcProjectChooser_CreateSeriesCB( UIWidget* ignored, UserData ignored2 );
static void ugcProjectChooser_EditEntryCB( UIWidget* ignored, UserData ignored2 );
static void ugcProjectChooser_DeleteEntryCB( UIWidget* ignored, UserData ignored2 );
static void UGCProjectChooser_SelectEntryCB( UIWidget* ignored, UserData ignored2 );
static void ugcProjectChooser_DuplicateProjectCB( UIWidget* ignored, UserData ignored2 );
static void ugcProjectChooser_BuyUGCProductsCB( UIButton* pButton, void* unused );
static void ugcProjectChooser_GetMoreReviewsCB( UIButton *button, void* ignored );
static void ugcProjectChooser_EntryListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData pDrawData );
static void ugcProjectChooser_SetActiveTabCB( UIButton* ignored, UserData rawTab );
static void ugcProjectChooser_GotoURLCB( UIButton* ignored, UserData rawURL );
static void ugcProjectChooser_EnableIfIsDefault( UITextEntry* textEntry, UserData rawOKButton );
static void ugcProjectChooser_EntryDraw( UGCProjectChooserEntry* entry, UI_MY_ARGS, float z, bool isSelected, bool isHovering );
static const char* ugcProjectChooser_EntryName( UGCProjectChooserEntry* entry );
static AtlasTex* ugcProjectChooser_EntryTypeTexture( UGCProjectChooserEntry* entry );
static AtlasTex* ugcProjectChooser_EntryLocationTexture( UGCProjectChooserEntry* entry );
static const UGCProjectReviews* ugcProjectChooser_EntryReviews( UGCProjectChooserEntry* entry );
static U32 ugcProjectChooser_CreateProjectEULAGetCrc( void );
static bool ugcProjectChooser_CreateProjectEULAIsAccepted( void );
	
void ugcProjectChooserInit( void )
{
	ui_SetGlobalValuesFromActiveSkin();
	ui_GameUIHide(UI_GAME_HIDER_UGC);

	if(!g_UGCProjectChooser)
	{
		g_UGCProjectChooser = calloc(1, sizeof(UGCProjectChooser));
	}

	if(!g_UGCProjectChooser->pRootPane) {
		g_UGCProjectChooser->pRootPane = ui_PaneCreate(0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0);
	}

	ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_CHOOSE_PROJECT );
}

void ugcProjectChooserFree( void )
{
	if(!g_UGCProjectChooser)
		return;

	MEContextDestroyByName( "UGCProjectChooser" );
	ui_WidgetQueueFreeAndNull(&g_UGCProjectChooser->pRootPane);

	eaDestroyEx( &g_UGCProjectChooser->eaModel, NULL );
	StructDestroySafe( parse_PossibleUGCProjects, &g_UGCProjectChooser->pPossibleUGCProjects );
	SAFE_FREE( g_UGCProjectChooser );

	// Clear skin
	if (!g_ui_State.bInUGCEditor) {
		REMOVE_HANDLE( g_ui_State.hActiveSkin );
		g_ui_State.minScreenWidth = 0;
		g_ui_State.minScreenHeight = 0;
		ui_SetGlobalValuesFromActiveSkin();
		ui_GameUIShow(UI_GAME_HIDER_UGC);
		ui_WindowCloseAll();
	}
}

bool ugcProjectChooser_IsOpen(void)
{
	return g_UGCProjectChooser != NULL;
}

void ugcProjectChooserShow(void)
{
	// Set the skin here
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser", g_ui_State.hActiveSkin );
	g_ui_State.minScreenWidth = 1024;
	g_ui_State.minScreenHeight = 720;
	
	if (g_UGCProjectChooser)
	{
		ugcProjectChooser_Refresh();
		ui_WidgetAddToDevice( UI_WIDGET( g_UGCProjectChooser->pRootPane ), NULL );
	}
}

void ugcProjectChooserHide(void)
{
	REMOVE_HANDLE( g_ui_State.hActiveSkin );
	
	if (g_UGCProjectChooser)
	{
		ui_WidgetRemoveFromGroup( UI_WIDGET( g_UGCProjectChooser->pRootPane ));
	}
}

void ugcProjectChooserSetPossibleProjects(PossibleUGCProjects* pProjects)
{
	if( !g_UGCProjectChooser ) {
		return;
	}

	if( !g_UGCProjectChooser->pPossibleUGCProjects ) {
		g_UGCProjectChooser->pPossibleUGCProjects = StructClone( parse_PossibleUGCProjects, pProjects );
	} else {
		StructCopy( parse_PossibleUGCProjects, pProjects, g_UGCProjectChooser->pPossibleUGCProjects, 0, 0, 0 );
	}

	// also notify the series editor
	ugcSeriesEditor_SetPossibleProjects( pProjects );

	ugcProjectChooser_RefreshModel();
	ugcProjectChooser_Refresh();
}

void ugcProjectChooserSetImportProjects(PossibleUGCProject **eaProjects)
{
	// MJF Nov/15/2012 -- This code path is unused.  Until we provide
	// a way to import from other people's projects, this does
	// nothing.
}

void ugcProjectChooserReceiveMoreReviews(U32 iProjectID, U32 iSeriesID, int iPageNumber, UGCProjectReviews *pReviews)
{
	if( g_UGCProjectChooser ) {
		PossibleUGCProject* pProject = ugcProjectChooser_GetPossibleProjectByID( iProjectID );
		if( pProject && pProject->pProjectReviews ) {
			ugcEditorUpdateReviews(CONTAINER_NOCONST(UGCProjectReviews, pProject->pProjectReviews), &pProject->iProjectReviewsPageNumber, pReviews, iPageNumber);
			ugcProjectChooser_Refresh();
		}
	}
}

void ugcProjectChooser_FinishedLoading( void )
{
	if( g_UGCProjectChooser ) {
		ugcProjectChooserFree();
	}
}

void ugcProjectChooser_SetMode( UGCProjectChooserMode eMode )
{
	g_UGCProjectChooser->eMode = eMode;

	switch( g_UGCProjectChooser->eMode ) {
		xcase UGC_PROJECT_CHOOSER_MODE_CHOOSE_PROJECT:
			ugcSeriesEditor_Hide();
			ugcProjectChooserShow();
			
		xcase UGC_PROJECT_CHOOSER_MODE_EDIT_SERIES:
			ugcProjectChooserHide();
			ugcSeriesEditor_Show();
	}
}

void ugcProjectChooser_Refresh( void )
{
	MEFieldContext* uiCtx = MEContextPush( "UGCProjectChooser", NULL, NULL, NULL );
	MEFieldContextEntry* entry;
	UIPane* pane;
	UIList* list;
	UIWidget* widget;
	UIScrollArea* scrollArea;
	char* estr = NULL;

	MEContextSetParent( UI_WIDGET( g_UGCProjectChooser->pRootPane ));
	g_UGCProjectChooser->pRootPane->invisible = true;

	entry = MEContextAddSprite( "UGC_Dashboard_Background", "BG", NULL, NULL );
	widget = UI_WIDGET( ENTRY_SPRITE( entry ));
	ENTRY_SPRITE( entry )->bPreserveAspectRatioFill = true;
	ui_WidgetSetPosition( widget, 0, 0 );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

	entry = MEContextAddLabelMsg( "Header", "UGC_ProjectChooser.Header", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_Header", widget->hOverrideSkin );
	ui_WidgetSetPositionEx( widget, 0, 640, 0, 0, UINoDirection );
	ui_LabelResize( ENTRY_LABEL( entry ));
	MEContextStepBackUp();
	uiCtx->iYPos = ui_WidgetGetNextY( widget );

	pane = MEContextPushPaneParent( "ContentPane" );
	ui_PaneSetStyle( pane, "Window_Without_TitleBar_NoFooter", true, false );
	ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 100, 0, 0, UINoDirection );
	pane->widget.priority = 200;
	if( eaSize( &g_UGCProjectChooser->eaModel ) == 0 ) {
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 300, 200, UIUnitFixed, UIUnitFixed );
		ui_PaneSetStyle( pane, "CharCreation_FooterDetails", true, false );
		
		// Also, move the header widget down
		ui_WidgetSetPositionEx( widget, 0, 260, 0, 0, UINoDirection );
		
		entry = MEContextAddLabelMsg( "FirstTimeLabel", "UGC_ProjectChooser.FirstTimeLabel", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
		ui_LabelForceAutosize( ENTRY_LABEL( entry ));
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ui_WidgetSetFont( widget, "Game_HUD" );
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITop );

		entry = MEContextAddButtonMsg( "UGC_ProjectChooser.NewProject", NULL, ugcProjectChooser_CreateProjectCB, NULL, "NewProjectButton", NULL, "UGC_ProjectChooser.NewProject.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, UGC_ROW_HEIGHT * 2, 0, 0, UIBottom );
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_HeavyweightButton", widget->hOverrideSkin );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetWidthEx( widget, 200, UIUnitFixed );
		ui_SetActive( widget, g_UGCProjectChooser->numProjects < g_UGCProjectChooser->numProjectSlots );
	} else {
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 850, 500, UIUnitFixed, UIUnitFixed );
		
		pane = MEContextPushPaneParent( "ListPane" );
		ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 0, 0, 0, UITopLeft );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 0.49, 1, UIUnitPercentage, UIUnitPercentage );
		ui_PaneSetStyle( pane, "Window_Content_Area", true, false );
		{
			pane = MEContextPushPaneParent( "ListAndSlotsPane" );
			ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 0, 0, 0, UITopLeft );
			ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, 0, (UGC_ROW_HEIGHT + 3) * 2 );
			ui_PaneSetStyle( pane, "CStore_Banner_TextBox", true, false );
			{
				entry = ugcMEContextAddList( &g_UGCProjectChooser->eaModel, 80, ugcProjectChooser_EntryListDrawCB, NULL, "ProjectList" );
				list = (UIList*)ENTRY_WIDGET( entry );
				widget = ENTRY_WIDGET( entry );
				ui_WidgetSetPosition( widget, 0, 0 );
				ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
				ui_ListSetSelectedCallback( list, UGCProjectChooser_SelectEntryCB, NULL );
				ui_ListSetActivatedCallback( list, ugcProjectChooser_EditEntryCB, NULL );
				if( !ui_ListGetSelectedObject( list )) {
					ui_ListSetSelectedRow( list, 0 );
				}
				widget->bottomPad = UGC_ROW_HEIGHT;
				g_UGCProjectChooser->pProjectList = list;

				ugcFormatMessageKey( &estr, "UGC_ProjectChooser.SlotsUsed",
									 STRFMT_INT( "NumProjects", g_UGCProjectChooser->numProjects ),
									 STRFMT_INT( "NumProjectSlots", g_UGCProjectChooser->numProjectSlots ),
									 STRFMT_INT( "NumSeries", g_UGCProjectChooser->numSeries ),
									 STRFMT_INT( "NumSeriesSlots", g_UGCProjectChooser->numSeriesSlots ),
									 STRFMT_END );
				entry = MEContextAddLabel( "SlotsLabel", estr, NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottom );
				ui_LabelResize( ENTRY_LABEL( entry ));
			}
			MEContextPop( "ListAndSlotsPane" );

			entry = MEContextAddButtonMsg( "UGC_ProjectChooser.NewProject", NULL, ugcProjectChooser_CreateProjectCB, NULL, "NewProjectButton", NULL, "UGC_ProjectChooser.NewProject.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, UGC_ROW_HEIGHT + 3, 0, 0, UIBottomLeft );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetWidthEx( widget, 0.5, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 0, 2, 0, 0 );
			ui_SetActive( widget, g_UGCProjectChooser->numProjects < g_UGCProjectChooser->numProjectSlots );
			
			entry = MEContextAddButtonMsg( "UGC_ProjectChooser.NewSeries", NULL, ugcProjectChooser_CreateSeriesCB, NULL, "NewSeriesButton", NULL, "UGC_ProjectChooser.NewSeries.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, UGC_ROW_HEIGHT + 3, 0, 0, UIBottomRight );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetWidthEx( widget, 0.5, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 2, 0, 0, 0 );
			ui_SetActive( widget, g_UGCProjectChooser->numSeries < g_UGCProjectChooser->numSeriesSlots );

			entry = MEContextAddButtonMsg( "UGC_ProjectChooser.Delete", NULL, ugcProjectChooser_DeleteEntryCB, NULL, "DeleteButton", NULL, "UGC_ProjectChooser.Delete.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottom );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetWidthEx( widget, 0.5, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		}
		MEContextPop( "ListPane" );

		pane = MEContextPushPaneParent( "DetailsPane" );
		ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 0, 0, 0, UITopRight );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 0.49, 1, UIUnitPercentage, UIUnitPercentage );
		ui_PaneSetStyle( pane, "Window_Content_Area", true, false );
		{
			UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );

			{
				AtlasTex* locationImage = ugcProjectChooser_EntryLocationTexture( selectedEntry );
				entry = MEContextAddSprite( locationImage->name, "LocationImage", NULL, NULL );
				widget = UI_WIDGET( ENTRY_SPRITE( entry ));
				ENTRY_SPRITE( entry )->bPreserveAspectRatioFill = true;
				ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
				ui_WidgetSetDimensionsEx( widget, 1, 180, UIUnitPercentage, UIUnitFixed );
			}

			MEContextPush( "InSprite", NULL, NULL, NULL );
			MEContextSetParent( widget );
		 	{
				MEContextGetCurrent()->astrOverrideSkinName = "UGCProjectChooser_TabButton";

				entry = MEContextAddLabel( "Name", ugcProjectChooser_EntryName( selectedEntry ), NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_DetailsHeader", widget->hOverrideSkin );
				ui_LabelResize( ENTRY_LABEL( entry ));
				ENTRY_LABEL( entry )->textFrom = UITop;
				ui_WidgetSetPositionEx( widget, 0, 25, 0, 0, UITop );
				ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), .8, UIUnitPercentage );

				{
					AtlasTex* typeTex = ugcProjectChooser_EntryTypeTexture( selectedEntry );
					entry = MEContextAddSprite( typeTex->name, "TypeTexture", NULL, NULL );
					widget = UI_WIDGET( ENTRY_SPRITE( entry ));
					ui_WidgetSetPositionEx( widget, -typeTex->width / 2, 25 - typeTex->height / 2, 0, 0, UITop );
				}
				
				entry = MEContextAddButtonMsg( "UGC_ProjectChooser.MainTab", NULL, ugcProjectChooser_SetActiveTabCB, (UserData)UGCProjectChooserTab_Main, "MainTab", NULL, NULL );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				if( g_UGCProjectChooser->activeTab == UGCProjectChooserTab_Main ) {
					SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_ActiveTabButton", widget->hOverrideSkin );
				}
				ui_ButtonResize( ENTRY_BUTTON( entry ));
				ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomLeft );
				ui_WidgetSetWidthEx( widget, 1.0/3, UIUnitPercentage );
				ui_WidgetSetPaddingEx( widget, 6, 6, 0, 0 );

				entry = MEContextAddButtonMsg( "UGC_ProjectChooser.MapTab", NULL, ugcProjectChooser_SetActiveTabCB, (UserData)UGCProjectChooserTab_Map, "MapTab", NULL, NULL );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				if( g_UGCProjectChooser->activeTab == UGCProjectChooserTab_Map ) {
					SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_ActiveTabButton", widget->hOverrideSkin );
				}
				ui_ButtonResize( ENTRY_BUTTON( entry ));
				ui_WidgetSetPositionEx( widget, 0, 0, 1.0/3, 0, UIBottomLeft );
				ui_WidgetSetWidthEx( widget, 1.0/3, UIUnitPercentage );
				ui_WidgetSetPaddingEx( widget, 6, 6, 0, 0 );

				entry = MEContextAddButtonMsg( "UGC_ProjectChooser.ReviewsTab", NULL, ugcProjectChooser_SetActiveTabCB, (UserData)UGCProjectChooserTab_Reviews, "ReviewsTab", NULL, NULL );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				if( g_UGCProjectChooser->activeTab == UGCProjectChooserTab_Reviews ) {
					SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_ActiveTabButton", widget->hOverrideSkin );
				}
				ui_ButtonResize( ENTRY_BUTTON( entry ));
				ui_WidgetSetPositionEx( widget, 0, 0, 2.0/3, 0, UIBottomLeft );
				ui_WidgetSetWidthEx( widget, 1.0/3, UIUnitPercentage );
				ui_WidgetSetPaddingEx( widget, 6, 6, 0, 0 );
				ui_SetActive( widget, SAFE_MEMBER( selectedEntry, pProject ) != NULL );
			}
			MEContextPop( "InSprite" );
			
			entry = MEContextAddButtonMsg( "UGC_ProjectChooser.Edit", NULL, ugcProjectChooser_EditEntryCB, NULL, "EditButton", NULL, "UGC_ProjectChooser.Edit.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_HeavyweightButton", widget->hOverrideSkin );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomLeft );
			ui_WidgetSetWidthEx( widget, 0.5, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 0, 2, 0, 0 );
			ui_SetActive( widget, selectedEntry != NULL );

			entry = MEContextAddButtonMsg( "UGC_ProjectChooser.Duplicate", NULL, ugcProjectChooser_DuplicateProjectCB, NULL, "DuplicateButton", NULL, "UGC_ProjectChooser.Duplicate.Tooltip" );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_HeavyweightButton", widget->hOverrideSkin );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomRight );
			ui_WidgetSetWidthEx( widget, 0.5, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 2, 0, 0, 0 );
			ui_SetActive( widget,
						  SAFE_MEMBER( selectedEntry, pProject ) != NULL
						  && g_UGCProjectChooser->numProjects < g_UGCProjectChooser->numProjectSlots );
			
			pane = MEContextPushPaneParent( "TabContent" );
			ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 0, 0, 0, UITopLeft );
			ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, 180, ui_WidgetGetNextY( widget ) + 4 );
			ui_PaneSetStyle( pane, "CStore_Banner_TextBox", true, false );
			if( selectedEntry ) { 
				switch( g_UGCProjectChooser->activeTab ) {
					xcase UGCProjectChooserTab_Main: {
						int y = 0;

						if( selectedEntry->pProject ) {
							UGCProjectStatusQueryInfo* pStatus = selectedEntry->pProject->pStatus;
							if( !pStatus ) {
								estrPrintf( &estr, "" );
							} else if( !pStatus->bCurrentlyPublishing ) {
								if( !pStatus->iLastPublishTime ) {
									ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_NotPublished", STRFMT_END );
								} else if( !pStatus->bLastPublishSucceeded ) {
									ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_PublishFailed", STRFMT_END );
								} else {
									if( pStatus->pFeatured ) {
										U32 curTime = timeSecondsSince2000();
										if( curTime > pStatus->pFeatured->iStartTimestamp ) {
											ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_Featured",
																 STRFMT_STRING( "FeatureTime", timeGetLocalDateNoTimeStringFromSecondsSince2000( pStatus->pFeatured->iStartTimestamp )),
																 STRFMT_END );
										} else {
											ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_FutureFeatured",
																 STRFMT_STRING( "FeatureTime", timeGetLocalDateNoTimeStringFromSecondsSince2000( pStatus->pFeatured->iStartTimestamp )),
																 STRFMT_END );
										}
									} else {
										ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_Published",
															 STRFMT_STRING( "PublishTime", timeGetLocalDateNoTimeStringFromSecondsSince2000( pStatus->iLastPublishTime )),
															 STRFMT_END );
									}
								}
							} else {
								ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_PublishInProgress", STRFMT_END );
							}
						} else if( selectedEntry->pSeries ) {
							ugcFormatMessageKey( &estr, "UGC_ProjectChooser.State_SeriesUpdated",
												 STRFMT_STRING( "UpdateTime", timeGetLocalDateNoTimeStringFromSecondsSince2000( selectedEntry->pSeries->iLastUpdatedTime )),
												 STRFMT_END );
						}
						entry = MEContextAddLabel( "PublishState", estr, NULL );

						widget = UI_WIDGET( ENTRY_LABEL( entry ));
						ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
						ui_LabelResize( ENTRY_LABEL( entry ));
						y = ui_WidgetGetNextY( widget );

						{
							char shortCodeBuffer[ 256 ] = "";

							if( selectedEntry->pProject ) {
								UGCIDString_IntToString( selectedEntry->pProject->iID, false, shortCodeBuffer );
							} else if( selectedEntry->pSeries ) {
								UGCIDString_IntToString( selectedEntry->pSeries->id, true, shortCodeBuffer );
							}

							ugcFormatMessageKey( &estr, "UGC_ProjectChooser.ShortCodeID",
												 STRFMT_STRING( "ShortCode", shortCodeBuffer ),
												 STRFMT_END );
							entry = MEContextAddLabel( "ID", estr, NULL );
						}
						widget = UI_WIDGET( ENTRY_LABEL( entry ));
						ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
						ui_LabelResize( ENTRY_LABEL( entry ));
						y = ui_WidgetGetNextY( widget );

						pane = MEContextPushPaneParent( "MainScrollAreaPane" );
						widget = UI_WIDGET( pane );
						ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
						ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
						ui_WidgetSetPaddingEx( widget, 0, 0, y, 0 );
						ui_PaneSetStyle( pane, "Window_Content_Row_Cell", true, false );
					
						scrollArea = MEContextPushScrollAreaParent( "MainScrollArea" );
						widget = UI_WIDGET( scrollArea );
						ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
						ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
						{
							float scrollY = 0;

							if( selectedEntry && selectedEntry->pProject ) {
								entry = MEContextAddLabelMsg( "DescriptionHeader", "UGC_ProjectChooser.Description", NULL );
								widget = UI_WIDGET( ENTRY_LABEL( entry ));
								UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "Game_Header_Small" );
								ui_WidgetSetPositionEx( widget, 0, scrollY, 0, 0, UITopLeft );
								ui_LabelResize( ENTRY_LABEL( entry ));
								scrollY = ui_WidgetGetNextY( widget );

								entry = MEContextAddLabel( "Description", NULL_TO_EMPTY( selectedEntry->pProject->pProjectInfo->strDescription ), NULL );
								widget = UI_WIDGET( ENTRY_LABEL( entry ));
								ui_WidgetSetPositionEx( widget, 0, scrollY, 0, 0, UITopLeft );
								ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
								ui_LabelUpdateDimensionsForWidth( ENTRY_LABEL( entry ), 320 );
								scrollY = MAX( scrollY + 10, ui_WidgetGetNextY( widget ));

								entry = MEContextAddLabelMsg( "NotesHeader", "UGC_ProjectChooser.PrivateNotes", NULL );
								widget = UI_WIDGET( ENTRY_LABEL( entry ));
								UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "Game_Header_Small" );
								ui_WidgetSetPositionEx( widget, 0, scrollY, 0, 0, UITopLeft );
								ui_LabelResize( ENTRY_LABEL( entry ));
								scrollY = ui_WidgetGetNextY( widget );

								entry = MEContextAddLabel( "Notes", NULL_TO_EMPTY( selectedEntry->pProject->pProjectInfo->strNotes ), NULL );
								widget = UI_WIDGET( ENTRY_LABEL( entry ));
								ui_WidgetSetPositionEx( widget, 0, scrollY, 0, 0, UITopLeft );
								ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
								ui_LabelUpdateDimensionsForWidth( ENTRY_LABEL( entry ), 320 );
								scrollY = ui_WidgetGetNextY( widget );
							} else if( selectedEntry && selectedEntry->pSeries ) {
								entry = MEContextAddLabel( "Description", NULL_TO_EMPTY( selectedEntry->pSeries->eaVersions[ 0 ]->strDescription ), NULL );
								widget = UI_WIDGET( ENTRY_LABEL( entry ));
								ui_WidgetSetPositionEx( widget, 0, scrollY, 0, 0, UITopLeft );
								ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
								ui_LabelUpdateDimensionsForWidth( ENTRY_LABEL( entry ), 320 );
								scrollY = ui_WidgetGetNextY( widget );
							}
						}
						MEContextPop( "MainScrollArea" );
						MEContextPop( "MainScrollAreaPane" );
					}

					xcase UGCProjectChooserTab_Map: {
						entry = MEContextAddSprite( "World_Map", "OverworldMap", NULL, NULL );
						widget = UI_WIDGET( ENTRY_SPRITE( entry ));
						ENTRY_SPRITE( entry )->bPreserveAspectRatio = true;
						ENTRY_SPRITE( entry )->bChildrenUseDrawBox = true;
						ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
						ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );

						MEContextPush( "InMap", NULL, NULL, NULL );
						MEContextSetParent( widget );
						{
							if( selectedEntry->pProject ) {
								UGCMapLocation* pMapLocation = selectedEntry->pProject->pProjectInfo->pMapLocation;
								if( !nullStr( SAFE_MEMBER( pMapLocation, astrIcon ))) {
									entry = MEContextAddSprite( pMapLocation->astrIcon, "MapLocation", NULL, NULL );
									widget = UI_WIDGET( ENTRY_SPRITE( entry ));
									ui_WidgetSetPositionEx( widget, -widget->width/2, -widget->height/2, pMapLocation->positionX, pMapLocation->positionY, UITopLeft );
								}
							} else if( selectedEntry->pSeries ) {
								int it;
								for( it = 0; it != eaSize( &selectedEntry->pSeries->eaVersions[ 0 ]->eaChildNodes ); ++it ) {
									UGCProjectSeriesNode* node = selectedEntry->pSeries->eaVersions[ 0 ]->eaChildNodes[ it ];
									PossibleUGCProject* pProject = ugcProjectChooser_GetPossibleProjectByID( node->iProjectID );

									if( pProject ) {
										UGCMapLocation* pMapLocation = pProject->pProjectInfo->pMapLocation;
										if( !nullStr( SAFE_MEMBER( pMapLocation, astrIcon ))) {
											entry = MEContextAddSpriteIndex( pMapLocation->astrIcon, "MapLocation", it, NULL, NULL );
											widget = UI_WIDGET( ENTRY_SPRITE( entry ));
											ui_WidgetSetPositionEx( widget, -widget->width/2, -widget->height/2, pMapLocation->positionX, pMapLocation->positionY, UITopLeft );
										}
									}
								}
							}
						}
						MEContextPop( "InMap" );
					}

					xcase UGCProjectChooserTab_Reviews: {
						int y = 0;

						if( selectedEntry->pProject ) {
							ugcFormatMessageKey( &estr, "UGC_ProjectChooser.TipsReceived",
												 STRFMT_INT( "NumTips", selectedEntry->pProject->pProjectInfo->uLifetimeTipsReceived ),
												 STRFMT_END );
						} else {
							estrPrintf( &estr, "" );
						}
						entry = MEContextAddLabel( "Tips", estr, NULL );
						widget = UI_WIDGET( ENTRY_LABEL( entry ));
						ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
						ui_LabelResize( ENTRY_LABEL( entry ));
						y = ui_WidgetGetNextY( widget ) + 4;

						if( selectedEntry->pProject ) {
							ugcFormatMessageKey( &estr, "UGC_ProjectChooser.ViewingPagesXOfY",
												 STRFMT_INT( "CurPage", selectedEntry->pProject->iProjectReviewsPageNumber + 1 ),
												 STRFMT_INT( "NumPages", selectedEntry->pProject->pProjectReviews->iNumReviewPagesCached ),
												 STRFMT_END );
						} else {
							estrPrintf( &estr, "" );
						}
						entry = MEContextAddLabel( "ReviewPages", estr, NULL );
						widget = UI_WIDGET( ENTRY_LABEL( entry ));
						ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
						ui_LabelResize( ENTRY_LABEL( entry ));

						entry = MEContextAddButtonMsg( "UGC_ProjectChooser.MoreReviews", NULL, ugcProjectChooser_GetMoreReviewsCB, NULL, "ReviewNextPageButton", NULL, "UGC_ProjectChooser.MoreReviews.Tooltip" );
						widget = UI_WIDGET( ENTRY_BUTTON( entry ));
						ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopRight );
						SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_LightweightButton", widget->hOverrideSkin );
						ui_ButtonResize( ENTRY_BUTTON( entry ));
						ui_SetActive( widget, selectedEntry->pProject->iProjectReviewsPageNumber + 1 < selectedEntry->pProject->pProjectReviews->iNumReviewPagesCached );
					
						y = ui_WidgetGetNextY( widget ) + 4;

						pane = MEContextPushPaneParent( "MainScrollAreaPane" );
						widget = UI_WIDGET( pane );
						ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
						ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
						ui_WidgetSetPaddingEx( widget, 0, 0, y, 0 );
						ui_PaneSetStyle( pane, "Window_Content_Row_Cell", true, false );
					
						scrollArea = MEContextPushScrollAreaParent( "MainScrollArea" );
						widget = UI_WIDGET( scrollArea );
						ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
						ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
						{
							const UGCProjectReviews* pReviews = ugcProjectChooser_EntryReviews( selectedEntry );
							if( eaSize( &pReviews->ppReviews ) == 0 ) { 
								entry = MEContextAddLabelMsg( "NoText", "UGC_ProjectChooser.NoReviews", NULL );
								widget = UI_WIDGET( ENTRY_LABEL( entry ));
								ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UINoDirection );
								ui_LabelResize( ENTRY_LABEL( entry ));
							} else {
								UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", "Window_Content_Row_Cell" );
								int it;

								if( texas ) {
									float reviewY = 0;
									for( it = 0; it != eaSize( &pReviews->ppReviews ); ++it ) {
										UGCSingleReview* pReview = pReviews->ppReviews[ it ];

										entry = ugcMEContextAddStarRatingIndex( pReview->fRating, "SingleReviewHeader", it );
										widget = ENTRY_WIDGET( entry );
										ui_WidgetSetPositionEx( widget, 0, reviewY, 0, 0, UITopLeft );

										ugcFormatMessageKey( &estr, "UGC_ProjectChooser.ReviewFormat",
															 STRFMT_STRING( "ReviewerAccount", pReview->pReviewerAccountName ),
															 STRFMT_STRING( "ReviewTime", timeGetLocalDateNoTimeStringFromSecondsSince2000( pReview->iTimestamp )),
															 STRFMT_END );
										entry = MEContextAddLabelIndex( "SimpleReviewLabel", it, estr, NULL );
										widget = UI_WIDGET( ENTRY_LABEL( entry ));
										ui_WidgetSetPositionEx( widget, 0, reviewY, 0, 0, UITopRight );
										reviewY = ui_WidgetGetNextY( widget );
								
										entry = MEContextAddLabelIndex( "SingleReviewText", it, pReview->pComment, NULL );
										widget = UI_WIDGET( ENTRY_LABEL( entry ));
										ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
										ui_WidgetSetPositionEx( widget, 20, reviewY, 0, 0, UITopLeft );
										ui_LabelUpdateDimensionsForWidth( ENTRY_LABEL( entry ), 270 );
										reviewY = ui_WidgetGetNextY( widget );
									}
								}
							}
						}
						MEContextPop( "MainScrollArea" );
						MEContextPop( "MainScrollAreaPane" );
					}
				}
			}
			MEContextPop( "TabContent" );
		}
		MEContextPop( "DetailsPane" );
	}
	MEContextPop( "ContentPane" );

	entry = MEContextAddSprite( "Login_Footer", "Footer", NULL, NULL );
	widget = UI_WIDGET( ENTRY_SPRITE( entry ));
	ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomLeft );
	ui_WidgetSetDimensionsEx( widget, 1, 100, UIUnitPercentage, UIUnitFixed );

	entry = MEContextAddButtonMsg( "UGC_ProjectChooser.Back", NULL, ugcProjectChooser_BackClickedCB, NULL, "BackButton", NULL, "UGC_ProjectChooser.MoreReviews.Tooltip" );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_BackButton", widget->hOverrideSkin );
	ui_WidgetSetPositionEx( widget, 20, 20, 0, 0, UIBottomLeft );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	widget->width = MAX( widget->width, 100 );
	widget->height = MAX( widget->height, 40 );

	{
		float x;
		float y;

		x = 0.3;
		y = 10;
		entry = MEContextAddButtonMsg( "UGC_ProjectChooser.WebLink_Wiki", "UGC_Dashboard_Weblink", ugcProjectChooser_GotoURLCB, "http://" ORGANIZATION_DOMAIN "/Foundry/Wiki", "WikiButton", NULL, "UGC_ProjectChooser.WebLink_Wiki.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ENTRY_BUTTON( entry )->textOffsetFrom = UITop;
		widget->width = MAX( widget->width, 180 );
		ui_WidgetSetPositionEx( widget, -widget->width / 2, y, x, 0, UIBottomLeft );
		y = ui_WidgetGetNextY( widget ) + 5;

		entry = MEContextAddSprite( "UGC_Dashboard_Wiki", "WikiSprite", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		ui_SpriteResize( ENTRY_SPRITE( entry ));
		ui_WidgetSetPositionEx( widget, -widget->width / 2, y, x, 0, UIBottomLeft );

		x = 0.5;
		y = 10;
		entry = MEContextAddButtonMsg( "UGC_ProjectChooser.WebLink_Community", "UGC_Dashboard_Weblink", ugcProjectChooser_GotoURLCB, "http://" ORGANIZATION_DOMAIN "/Foundry/Forums", "CommunityButton", NULL, "UGC_ProjectChooser.WebLink_Community.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ENTRY_BUTTON( entry )->textOffsetFrom = UITop;
		widget->width = MAX( widget->width, 180 );
		ui_WidgetSetPositionEx( widget, -widget->width / 2, y, x, 0, UIBottomLeft );
		y = ui_WidgetGetNextY( widget ) + 5;

		entry = MEContextAddSprite( "UGC_Dashboard_Community", "CommunitySprite", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		ui_SpriteResize( ENTRY_SPRITE( entry ));
		ui_WidgetSetPositionEx( widget, -widget->width / 2, y, x, 0, UIBottomLeft );

		x = 0.7;
		y = 10;
		entry = MEContextAddButtonMsg( "UGC_ProjectChooser.WebLink_Training", "UGC_Dashboard_Weblink", ugcProjectChooser_GotoURLCB, "http://" ORGANIZATION_DOMAIN "/Foundry/Tutorial", "TrainingButton", NULL, "UGC_ProjectChooser.WebLink_Training.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ENTRY_BUTTON( entry )->textOffsetFrom = UITop;
		widget->width = MAX( widget->width, 180 );
		ui_WidgetSetPositionEx( widget, -widget->width / 2, y, x, 0, UIBottomLeft );
		y = ui_WidgetGetNextY( widget ) + 5;

		entry = MEContextAddSprite( "UGC_Dashboard_Training", "TrainingSprite", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		ui_SpriteResize( ENTRY_SPRITE( entry ));
		ui_WidgetSetPositionEx( widget, -widget->width / 2, y, x, 0, UIBottomLeft );
	}

	MEContextPop( "UGCProjectChooser" );

	estrDestroy( &estr );
}

void ugcProjectChooser_RefreshModel( void )
{
	eaClearEx( &g_UGCProjectChooser->eaModel, NULL );
	g_UGCProjectChooser->numProjectSlots = 0;
	g_UGCProjectChooser->numSeriesSlots = 0;
	g_UGCProjectChooser->numProjects = 0;
	g_UGCProjectChooser->numSeries = 0;

	if( g_UGCProjectChooser->pPossibleUGCProjects ) {
		int it;
		int nodeIt;

		g_UGCProjectChooser->numProjectSlots = g_UGCProjectChooser->pPossibleUGCProjects->iProjectSlotsMax;
		g_UGCProjectChooser->numSeriesSlots = g_UGCProjectChooser->pPossibleUGCProjects->iSeriesSlotsMax;
		for( it = 0; it != eaSize( &g_UGCProjectChooser->pPossibleUGCProjects->eaProjectSeries ); ++it ) {
			UGCProjectSeries* series = g_UGCProjectChooser->pPossibleUGCProjects->eaProjectSeries[ it ];
			UGCProjectSeriesVersion* seriesVersion = series->eaVersions[ 0 ];
			UGCProjectChooserEntry* accum = calloc( 1, sizeof( *accum ));
			eaPush( &g_UGCProjectChooser->eaModel, accum );
			
			accum->pSeries = series;

			for( nodeIt = 0; nodeIt != eaSize( &seriesVersion->eaChildNodes ); ++nodeIt ) {
				UGCProjectSeriesNode* node = seriesVersion->eaChildNodes[ nodeIt ];
				UGCProjectChooserEntry* projAccum = calloc( 1, sizeof( *projAccum ));
				eaPush( &g_UGCProjectChooser->eaModel, projAccum );
				
				assert( node->iProjectID ); //< Neverwinter only supports a single depth
				projAccum->pProject = ugcProjectChooser_GetPossibleProjectByID( node->iProjectID );
				projAccum->indentLevel = 1;
			}
			++g_UGCProjectChooser->numSeries;
		}

		g_UGCProjectChooser->numProjects = g_UGCProjectChooser->pPossibleUGCProjects->iProjectSlotsUsed;
		for( it = 0; it != eaSize( &g_UGCProjectChooser->pPossibleUGCProjects->ppProjects ); ++it ) {
			PossibleUGCProject* project = g_UGCProjectChooser->pPossibleUGCProjects->ppProjects[ it ];

			if( project->iID && !project->pStatus->pFeatured ) {
				if( !project->iSeriesID ) {
					UGCProjectChooserEntry* accum = calloc( 1, sizeof( *accum ));
					eaPush( &g_UGCProjectChooser->eaModel, accum );
				
					accum->pProject = project;
				}
			}
		}
	}
}

void ugcProjectChooser_BackClickedCB(UIButton* pButton, void* unused)
{
	Login_Back();
	ugcProjectChooserFree();
}

static void ugcProjectChooser_CreateProject_EULAYesClickedCB( UIButton* ignored, void* ignored2 )
{
	ugcModalDialogClose( NULL, NULL );
		
	if( gpLoginLink ) {
		Packet* pak = pktCreate( gpLoginLink, TOLOGIN_ACCEPTED_UGC_PROJECT_CREATE_EULA );
		pktSendU32( pak, ugcProjectChooser_CreateProjectEULAGetCrc() );
		pktSend( &pak );
	}

	gclChooseNewUGCProject( "", NULL, -1 );
}

void ugcProjectChooser_CreateProjectCB( UIWidget* ignored, UserData ignored2 )
{
	if( g_UGCProjectChooser->pPossibleUGCProjects ) {
		if( !ugcProjectChooser_CreateProjectEULAIsAccepted() ) {
			ugcEditorShowEULA( ugcProjectChooser_CreateProject_EULAYesClickedCB );
		} else {
			gclChooseNewUGCProject( "", NULL, -1 );
			ugcLoadingUpdateState( UGC_LOAD_INIT, 0 );
		}
	}
}

static void ugcProjectChooser_CreateSeries_EULAYesClickedCB( UIButton* ignored, void* ignored2 )
{
	ugcModalDialogClose( NULL, NULL );
		
	if( gpLoginLink ) {
		Packet* pak = pktCreate( gpLoginLink, TOLOGIN_ACCEPTED_UGC_PROJECT_CREATE_EULA );
		pktSendU32( pak, ugcProjectChooser_CreateProjectEULAGetCrc() );
		pktSend( &pak );
	}

	{
		NOCONST(UGCProjectSeries)* seriesAccum = StructCreateNoConst( parse_UGCProjectSeries );
		eaPush( &seriesAccum->eaVersions, StructCreateNoConst( parse_UGCProjectSeriesVersion ));
		ugcSeriesEditor_EditNewSeries( CONTAINER_RECONST( UGCProjectSeries, seriesAccum ));
		StructDestroyNoConst( parse_UGCProjectSeries, seriesAccum );
		ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_EDIT_SERIES );
	}
}

void ugcProjectChooser_CreateSeriesCB( UIWidget* ignored, UserData ignored2 )
{
	if( g_UGCProjectChooser->pPossibleUGCProjects ) {
		if( !ugcProjectChooser_CreateProjectEULAIsAccepted() ) {
			ugcEditorShowEULA( ugcProjectChooser_CreateSeries_EULAYesClickedCB );
		} else {
			NOCONST(UGCProjectSeries)* seriesAccum = StructCreateNoConst( parse_UGCProjectSeries );
			eaPush( &seriesAccum->eaVersions, StructCreateNoConst( parse_UGCProjectSeriesVersion ));
			ugcSeriesEditor_EditNewSeries( CONTAINER_RECONST( UGCProjectSeries, seriesAccum ));
			StructDestroyNoConst( parse_UGCProjectSeries, seriesAccum );
			ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_EDIT_SERIES );
		}
	}
}

static void ugcProjectChooser_EditEntry_EULAYesClickedCB( UIButton* ignored, void* ignored2 )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );

	ugcModalDialogClose( NULL, NULL );
		
	if( gpLoginLink ) {
		Packet* pak = pktCreate( gpLoginLink, TOLOGIN_ACCEPTED_UGC_PROJECT_CREATE_EULA );
		pktSendU32( pak, ugcProjectChooser_CreateProjectEULAGetCrc() );
		pktSend( &pak );
	}
	
	if( selectedEntry->pProject ) {
		gclChooseUGCProject( selectedEntry->pProject );
		ugcLoadingUpdateState( UGC_LOAD_INIT, 0 );
	} else if( selectedEntry->pSeries ) {
		ugcSeriesEditor_EditSeries( selectedEntry->pSeries );
		ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_EDIT_SERIES );
	}
}

static void ugcProjectChooser_UndoAllowFeaturedDialog_OKClickedCB( UIWidget* ignored, UserData ignored2 )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
	if( selectedEntry && selectedEntry->pProject ) {
		gclChooseUGCProject( selectedEntry->pProject );
		ugcLoadingUpdateState( UGC_LOAD_INIT, 0 );
	}
}

static void ugcProjectChooser_UndoAllowFeaturedDialog_EnterCB( UITextEntry* textEntry, PossibleUGCProject* pProject )
{
	if( stricmp_safe( ui_EditableGetText( UI_EDITABLE( textEntry )), ui_EditableGetDefault( UI_EDITABLE( textEntry )) ) == 0 ) {
		ugcProjectChooser_UndoAllowFeaturedDialog_OKClickedCB( NULL, NULL );
	}
}

static void ugcProjectChooser_ShowUndoAllowFeaturedDialog(PossibleUGCProject *pProject)
{
	UILabel* pLabel = NULL;
	UITextEntry* pTextEntry = NULL;
	F32 y = 0;

	if( !g_UGCProjectChooser ) {
		return;
	}

	ugcModalDialogClose( NULL, NULL );
	
	ugcModalDialogPrepare( TranslateMessageKey( "UGC.WarningDialogTitle" ),
						   TranslateMessageKey( "UGC.OK" ), ugcProjectChooser_UndoAllowFeaturedDialog_OKClickedCB,
						   TranslateMessageKey( "UGC.Cancel" ), NULL,
						   NULL );

	pLabel = ui_LabelCreate( TranslateMessageKey( "UGC.ClearProjectAuthorAllowsFeaturedDescription" ), 5, 0 );
	ui_LabelSetWordWrap( pLabel, true );
	ui_LabelUpdateDimensionsForWidth( pLabel, 315 );
	ugcModalDialogAddWidget( UI_WIDGET( pLabel ));
	y += ui_WidgetGetHeight( UI_WIDGET( pLabel ));

	pTextEntry = ui_TextEntryCreate( "", 0, y );
	ui_EditableSetDefaultString( UI_EDITABLE( pTextEntry ), pProject->pProjectInfo->pcPublicName );
	ui_WidgetSetWidthEx( UI_WIDGET( pTextEntry ), 1, UIUnitPercentage );
	ugcModalDialogAddWidget( UI_WIDGET( pTextEntry ));
	y += ui_WidgetGetHeight( UI_WIDGET( pTextEntry ));

	ui_TextEntrySetChangedCallback( pTextEntry, ugcProjectChooser_EnableIfIsDefault, ugcModalDialogButton1() );
	ugcProjectChooser_EnableIfIsDefault( pTextEntry, ugcModalDialogButton1() );
	ui_TextEntrySetEnterCallback( pTextEntry, ugcProjectChooser_UndoAllowFeaturedDialog_EnterCB, pProject );

	ugcModalDialogShow( 325, 200 );
}

static bool ugcProjectChooser_MaybeShowUndoAllowFeaturedDialog( PossibleUGCProject* pProject )
{
	if( pProject->pStatus->pFeatured ) {
		ugcModalDialogMsg( "UGC.ErrorDialogTitle", "UGC.ProjectFeaturedImpliesEditorDisabled", UIOk );
		return true;
	} else if( ugcDefaultsAuthorAllowsFeaturedBlocksEditing() && pProject->pStatus->bAuthorAllowsFeatured ) {
		ugcProjectChooser_ShowUndoAllowFeaturedDialog( pProject );
		return true;
	}

	return false;
}

void ugcProjectChooser_EditEntryCB( UIWidget* ignored, UserData ignored2 )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
	if( selectedEntry ) {
		if( !ugcProjectChooser_CreateProjectEULAIsAccepted() ) {
			ugcEditorShowEULA( ugcProjectChooser_EditEntry_EULAYesClickedCB );
		} else {
			if( selectedEntry->pProject ) {
				if( ugcProjectChooser_MaybeShowUndoAllowFeaturedDialog( selectedEntry->pProject )) {
					// Do nothing, we're showing a modal dialog
				} else {
					gclChooseUGCProject( selectedEntry->pProject );
					ugcLoadingUpdateState( UGC_LOAD_INIT, 0 );
				}
			} else if( selectedEntry->pSeries ) {
				ugcSeriesEditor_EditSeries( selectedEntry->pSeries );
				ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_EDIT_SERIES );
			}
		}
	}
}

static void ugcProjectChooser_DeleteDialog_OKClickedCB(UIButton *pButton, UserData ignored)
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );

	if( selectedEntry && selectedEntry->pProject )
	{
		gclDeleteUGCProject(selectedEntry->pProject);
		ugcModalDialogClose( NULL, NULL );
	}
}

static void ugcProjectChooser_DeleteDialog_OKClickedCBIfIsDefault(UITextEntry* textEntry, UserData rawProject)
{
	if( stricmp_safe( ui_EditableGetText( UI_EDITABLE( textEntry )), ui_EditableGetDefault( UI_EDITABLE( textEntry ))) == 0 ) {
		ugcProjectChooser_DeleteDialog_OKClickedCB(NULL, NULL);
	}	
}

static void ugcProjectChooser_DeleteSeriesDialog_OKClickedCB(UIButton *pButton, UserData ignored2 )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
	if( selectedEntry && selectedEntry->pSeries )
	{
		Packet* pak = pktCreate( gpLoginLink, TOLOGIN_UGCPROJECTSERIES_DESTROY );
		pktSendU32( pak, selectedEntry->pSeries->id );
		pktSend( &pak );

		ugcModalDialogClose( NULL, NULL );
	}
}

static void ugcProjectChooser_DeleteSeriesDialog_OKClickedCBIfIsDefault(UITextEntry* textEntry, UserData rawSeries)
{
	if( stricmp_safe( ui_EditableGetText( UI_EDITABLE( textEntry )), ui_EditableGetDefault( UI_EDITABLE( textEntry ))) == 0 ) {
		ugcProjectChooser_DeleteSeriesDialog_OKClickedCB(NULL, NULL);
	}	
}

void ugcProjectChooser_DeleteEntryCB( UIWidget* ignored, UserData ignored2 )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
	if( selectedEntry ) {
		if( selectedEntry->pProject ) {
			PossibleUGCProject* pProject = selectedEntry->pProject;
			if( pProject->pStatus->pFeatured ) {
				ugcModalDialogMsg( "UGC.ErrorDialogTitle", "UGC.ProjectFeaturedImpliesEditorDisabled", UIOk );
			} else if( ugcDefaultsAuthorAllowsFeaturedBlocksEditing() && pProject->pStatus->bAuthorAllowsFeatured ) {
				ugcModalDialogMsg( "UGC.ErrorDialogTitle", "UGC.ProjectAuthorAllowsFeaturedImpliesEditorDisabled", UIOk );
			} else {
				UILabel* pLabel;
				UITextEntry* pTextEntry;

				ugcModalDialogClose( NULL, NULL );

				{
					char* estr = NULL;
					ugcFormatMessageKey( &estr, "UGC.DeleteProjectNameQuestion",
										 STRFMT_STRING( "ProjectName", ugcProjectChooser_EntryName(selectedEntry) ),
										 STRFMT_END );
					ugcModalDialogPrepare( estr,
										   TranslateMessageKey( "UGC.Ok" ), ugcProjectChooser_DeleteDialog_OKClickedCB,
										   TranslateMessageKey( "UGC.Cancel" ), NULL,
										   NULL );
					estrDestroy( &estr );
				}

				pLabel = ui_LabelCreate(TranslateMessageKey("UGC.DeleteProjectQuestion"), 0, 0);
				ui_LabelSetWordWrap(pLabel, 1);
				ui_WidgetSetDimensionsEx(UI_WIDGET(pLabel), 1.0f, 1.0f, UIUnitPercentage, UIUnitPercentage);
				ui_WidgetSetPositionEx(UI_WIDGET(pLabel), 0, 0, 0, 0, UITop);
				ugcModalDialogAddWidget( UI_WIDGET( pLabel ));

				pTextEntry = ui_TextEntryCreate("", 0, UGC_ROW_HEIGHT * 2.5 );
				ui_EditableSetDefaultString( UI_EDITABLE( pTextEntry ), ugcProjectChooser_EntryName(selectedEntry) );
				ui_WidgetSetWidthEx( UI_WIDGET( pTextEntry ), 1, UIUnitPercentage );
				ugcModalDialogAddWidget( UI_WIDGET( pTextEntry ));

				ui_TextEntrySetChangedCallback(pTextEntry, ugcProjectChooser_EnableIfIsDefault, ugcModalDialogButton1() );
				ugcProjectChooser_EnableIfIsDefault( pTextEntry, ugcModalDialogButton1() );
				ui_TextEntrySetEnterCallback(pTextEntry, ugcProjectChooser_DeleteDialog_OKClickedCBIfIsDefault, pProject );

				ugcModalDialogShow( 375, 150 );
			}
		} else if( selectedEntry->pSeries ) {
			UGCProjectSeries* pProjectSeries = selectedEntry->pSeries;
			UILabel* pLabel;
			UITextEntry* pTextEntry;
			char *text = NULL;

			ugcModalDialogClose( NULL, NULL );

			ugcFormatMessageKey(&text, "UGC.DeleteSeriesNameQuestion",
								STRFMT_STRING("SeriesName", ugcProjectChooser_EntryName( selectedEntry )),
								STRFMT_END );
			ugcModalDialogPrepare( text,
								   TranslateMessageKey( "UGC.Ok" ), ugcProjectChooser_DeleteSeriesDialog_OKClickedCB,
								   TranslateMessageKey( "UGC.Cancel" ), NULL,
								   NULL );
			estrDestroy(&text);
		
			pLabel = ui_LabelCreate(TranslateMessageKey("UGC.DeleteSeriesQuestion"), 0, 0);
			ui_LabelSetWordWrap(pLabel, 1);
			ui_WidgetSetDimensionsEx(UI_WIDGET(pLabel), 1.0f, 1.0f, UIUnitPercentage, UIUnitPercentage);
			ui_WidgetSetPositionEx(UI_WIDGET(pLabel), 0, 0, 0, 0, UITop);
			ugcModalDialogAddWidget( UI_WIDGET( pLabel ));

			pTextEntry = ui_TextEntryCreate("", 0, UGC_ROW_HEIGHT * 2.5 );
			ui_EditableSetDefaultString( UI_EDITABLE( pTextEntry ), ugcProjectChooser_EntryName( selectedEntry ));
			ui_WidgetSetWidthEx( UI_WIDGET( pTextEntry ), 1, UIUnitPercentage );
			ugcModalDialogAddWidget( UI_WIDGET( pTextEntry ));

			ui_TextEntrySetChangedCallback(pTextEntry, ugcProjectChooser_EnableIfIsDefault, ugcModalDialogButton1() );
			ugcProjectChooser_EnableIfIsDefault( pTextEntry, ugcModalDialogButton1() );
			ui_TextEntrySetEnterCallback(pTextEntry, ugcProjectChooser_DeleteSeriesDialog_OKClickedCBIfIsDefault, pProjectSeries );

			ugcModalDialogShow( 375, 150 );
		}
	}
}

void UGCProjectChooser_SelectEntryCB( UIWidget* ignored, UserData ignored2 )
{
	g_UGCProjectChooser->activeTab = UGCProjectChooserTab_Main;
	ugcProjectChooser_Refresh();
}

static void ugcProjectChooser_DuplicateProject_EULAYesClickedCB( UIButton* ignored, void* ignored2 )
{
	ugcModalDialogClose( NULL, NULL );
		
	if( gpLoginLink ) {
		Packet* pak = pktCreate( gpLoginLink, TOLOGIN_ACCEPTED_UGC_PROJECT_CREATE_EULA );
		pktSendU32( pak, ugcProjectChooser_CreateProjectEULAGetCrc() );
		pktSend( &pak );
	}

	{
		UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
		if( selectedEntry && selectedEntry->pProject && selectedEntry->pProject->pProjectInfo ) {
			char buffer[ 512 ];
			PossibleUGCProject* pCopy = StructClone( parse_PossibleUGCProject, selectedEntry->pProject );
			pCopy->iCopyID = pCopy->iID;
			pCopy->iID = 0;
			sprintf( buffer, "Copy of %s", ugcProjectChooser_EntryName( selectedEntry ));
			StructCopyString( &pCopy->pProjectInfo->pcPublicName, buffer );
			
			gclChooseUGCProject( pCopy );
			ugcLoadingUpdateState( UGC_LOAD_INIT, 0 );
			StructDestroy( parse_PossibleUGCProject, pCopy );
		}
	}
}

void ugcProjectChooser_DuplicateProjectCB( UIWidget* ignored, UserData ignored2 )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
	if( selectedEntry && selectedEntry->pProject && selectedEntry->pProject->pProjectInfo ) {
		if( !ugcProjectChooser_CreateProjectEULAIsAccepted() ) {
			ugcEditorShowEULA( ugcProjectChooser_DuplicateProject_EULAYesClickedCB );
		} else {
			char buffer[ 512 ];
			PossibleUGCProject* pCopy = StructClone( parse_PossibleUGCProject, selectedEntry->pProject );
			pCopy->iCopyID = pCopy->iID;
			pCopy->iID = 0;
			sprintf( buffer, "Copy of %s", ugcProjectChooser_EntryName( selectedEntry ));
			StructCopyString( &pCopy->pProjectInfo->pcPublicName, buffer );
			
			gclChooseUGCProject( pCopy );
			ugcLoadingUpdateState( UGC_LOAD_INIT, 0 );
			StructDestroy( parse_PossibleUGCProject, pCopy );
		}
	}
}

void ugcProjectChooser_BuyUGCProductsCB( UIButton* pButton, void* unused )
{
	gclLogin_BrowseUGCProducts( true );
}

void ugcProjectChooser_GetMoreReviewsCB(UIButton *button, void* ignored )
{
	UGCProjectChooserEntry* selectedEntry = ui_ListGetSelectedObject( g_UGCProjectChooser->pProjectList );
	if( selectedEntry && selectedEntry->pProject ) {
		ContainerID uProjectID = selectedEntry->pProject->iID;
		int iPageNumber = selectedEntry->pProject->iProjectReviewsPageNumber;

		gclUGC_RequestReviewsForPage(uProjectID, /*uSeriesID=*/0, iPageNumber + 1);
		ugcProjectChooser_Refresh();
	}
}

void ugcProjectChooser_EntryListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData pDrawData )
{
	UGCProjectChooserEntry* entry = eaGet( pList->peaModel, iRow );
	if( entry ) {
		CBox box;
		BuildCBox( &box, x, y, w, h );
		ugcProjectChooser_EntryDraw( entry, UI_MY_VALUES, z, ui_ListIsSelected( pList, pColumn, iRow ), ui_ListIsHovering( pList, pColumn, iRow ));
	}
}

void ugcProjectChooser_SetActiveTabCB( UIButton* ignored, UserData rawTab )
{
	UGCProjectChooserTab tab = (UGCProjectChooserTab)rawTab;
	g_UGCProjectChooser->activeTab = tab;

	ugcProjectChooser_Refresh();
}

void ugcProjectChooser_GotoURLCB( UIButton* ignored, UserData rawURL )
{
	if( ugcModalDialogMsg( "UGC_ProjectChooser.GoToURL_Title", "UGC_ProjectChooser.GoToURL_Body", UIYes | UINo ) == UIYes ) {
		openURL( rawURL );
	}
}

void ugcProjectChooser_EnableIfIsDefault( UITextEntry* textEntry, UserData rawOKButton )
{
	UIWidget* okButton = rawOKButton;

	if( stricmp_safe( ui_EditableGetText( UI_EDITABLE( textEntry )), ui_EditableGetDefault( UI_EDITABLE( textEntry ))) == 0 ) {
		ui_SetActive( okButton, true );
	} else {
		ui_SetActive( okButton, false );
	}	
}
	
void ugcProjectChooser_EntryDraw( UGCProjectChooserEntry* entry, UI_MY_ARGS, float z, bool isSelected, bool isHovering )
{
	UITextureAssembly* normal = RefSystem_ReferentFromString( "UITextureAssembly", "UGCProjectChooser_Item_Idle" );
	UITextureAssembly* hover = RefSystem_ReferentFromString( "UITextureAssembly", "UGCProjectChooser_Item_Over" );
	UITextureAssembly* selected = RefSystem_ReferentFromString( "UITextureAssembly", "UGCProjectChooser_Item_Selected" );
	AtlasTex* starTex = atlasFindTexture( "UGC_Widgets_Star_Rating_Fill" );
	AtlasTex* emptyTex = atlasFindTexture( "UGC_Widgets_Star_Rating_Empty" );
	UIStyleFont* bigFont = ui_StyleFontGet( "Game_Header" );
	UIStyleFont* bigFontSelected = ui_StyleFontGet( "Game_Header_Yellow" );
	UIStyleFont* normalFont = ui_StyleFontGet( "Game_HUD" );
	UIStyleFont* normalFontSelected = ui_StyleFontGet( "Game_HUD_Yellow" );
	
	CBox box;
	CBox drawBox;
	BuildCBox( &box, x, y, w, h );
	BuildCBox( &drawBox, x, y + 2, w - 2, h - 2 );
	drawBox.lx += SAFE_MEMBER( entry, indentLevel ) * 20;

	if(   !normal || !hover || !selected || !bigFont || !bigFontSelected || !normalFont || !normalFontSelected
		  || !starTex || !emptyTex ) {
		return;
	}

	if( isSelected ) {
		ui_TextureAssemblyDraw( selected, &drawBox, &drawBox, scale, z, z + 0.1, 255, NULL );
	} else if( isHovering ) {
		ui_TextureAssemblyDraw( hover, &drawBox, &drawBox, scale, z, z + 0.1, 255, NULL );
	} else {
		ui_TextureAssemblyDraw( normal, &drawBox, &drawBox, scale, z, z + 0.1, 255, NULL );
	}
	clipperPushRestrict( &drawBox );
	z += 0.2;
	
	{
		AtlasTex* tex = ugcProjectChooser_EntryLocationTexture( entry );
		AtlasTex* layerTex = atlasFindTexture( "Gradient_Quest" );
		CBox spriteBox = drawBox;
		ui_SpriteCalcDrawBox( &spriteBox, tex, NULL, true );
		display_sprite_box( tex, &spriteBox, z, (isSelected || isHovering ? -1 : 0xFFFFFFCC) );
		display_sprite_box( layerTex, &spriteBox, z, 0xFFFFFF81 );
	}
	z += 0.1;

	ui_StyleFontUse( isSelected ? bigFontSelected : bigFont, false, 0 );
	gfxfont_PrintMaxWidth( drawBox.lx + 4, drawBox.hy - 4, z, w - 4 - 80 - 4, scale, scale, 0,
						   ugcProjectChooser_EntryName( entry ));
	display_sprite( ugcProjectChooser_EntryTypeTexture( entry ), drawBox.lx + 4, drawBox.ly + 4, z, scale, scale, -1 );

	ui_StyleFontUse( normalFont, false, 0 );
	{
		const UGCProjectReviews* reviews = ugcProjectChooser_EntryReviews( entry );
		if( reviews ) {
			char buffer[ 256 ];
			float xIt = drawBox.hx - 4;
			sprintf( buffer, "(%d)", reviews->iNumRatingsCached );
			xIt -= ui_StyleFontWidth( normalFont, scale, buffer );
			gfxfont_Print( xIt, drawBox.hy - 4, z, scale, scale, 0, buffer );

			xIt -= starTex->width + 4;
			{
				float quantizedValue = round( reviews->fAverageRating * 10 ) / 10.0f;
				CBox areaBox;
				CBox starBox;
				areaBox.lx = xIt;
				areaBox.hx = areaBox.lx + 80;
				areaBox.hy = drawBox.hy - 4;
				areaBox.ly = areaBox.hy - starTex->height;

				display_sprite_box( emptyTex, &areaBox, z, -1 );
				starBox = areaBox;
				starBox.hx = interpF32( quantizedValue, areaBox.lx, areaBox.hx );
				clipperPushRestrict( &starBox );
				display_sprite_box( starTex, &areaBox, z + 0.1, -1 );
				clipperPop();
			}
		}
	}

	clipperPop();
}

const char* ugcProjectChooser_EntryName( UGCProjectChooserEntry* entry )
{
	if( entry ) {
		if( entry->pProject ) {
			if( !nullStr( entry->pProject->pProjectInfo->pcPublicName )) {
				return entry->pProject->pProjectInfo->pcPublicName;
			} else {
				return TranslateMessageKey( "UGC.Unnamed_Project" );
			}
		} else if( entry->pSeries ) {
			if( !nullStr( entry->pSeries->eaVersions[ 0 ]->strName )) {
				return entry->pSeries->eaVersions[ 0 ]->strName;
			} else {
				return TranslateMessageKey( "UGC.Unnamed_Series" );
			}
		}
	}
	
	return "???";
}

AtlasTex* ugcProjectChooser_EntryTypeTexture( UGCProjectChooserEntry* entry )
{
	if( entry ) {
		if( entry->pProject ) {
			return atlasFindTexture( "Icon_Quest_Quest" );
		} else if( entry->pSeries ) {
			return atlasFindTexture( "Icon_Quest_Campaign" );
		}
	}
	
	return atlasFindTexture( "Icon_Quest_Quest" );
}

AtlasTex* ugcProjectChooser_EntryLocationTexture( UGCProjectChooserEntry* entry )
{
	if( entry ) {
		if( entry->pProject ) {
			if( !nullStr( SAFE_MEMBER( entry->pProject->pProjectInfo->pMapLocation, astrIcon ))) {
				char buffer[ 256 ];
				sprintf( buffer, "Header_%s", entry->pProject->pProjectInfo->pMapLocation->astrIcon );
				return atlasFindTexture( buffer );
			}
		} else if( entry->pSeries ) {
			if( !nullStr( entry->pSeries->eaVersions[ 0 ]->strImage )) {
				char buffer[ 256 ];
				sprintf( buffer, "Header_%s", entry->pSeries->eaVersions[ 0 ]->strImage );
				return atlasFindTexture( buffer );
			}
		}
	}

	return atlasFindTexture( "Header_Mapicon_Neverwintercity_01" );
}

const UGCProjectReviews* ugcProjectChooser_EntryReviews( UGCProjectChooserEntry* entry )
{
	if( entry->pProject ) {
		return entry->pProject->pProjectReviews;
	} else if( entry->pSeries ) {
		return &entry->pSeries->ugcReviews;
	}

	return NULL;
}

PossibleUGCProject* ugcProjectChooser_GetPossibleProjectByID( ContainerID uProjectID )
{
	if( !g_UGCProjectChooser || !g_UGCProjectChooser->pPossibleUGCProjects || !uProjectID ) {
		return NULL;
	}

	FOR_EACH_IN_EARRAY( g_UGCProjectChooser->pPossibleUGCProjects->ppProjects, PossibleUGCProject, project ) {
		if( project->iID == uProjectID ) {
			return project;
		}
	} FOR_EACH_END;

	return NULL;
}

U32 ugcProjectChooser_CreateProjectEULAGetCrc( void )
{
	const char* eulaTxt = TranslateMessageKey( "UGC.CreateProjectEULA" );
	return cryptAdler32( eulaTxt, strlen( eulaTxt ));
}

bool ugcProjectChooser_CreateProjectEULAIsAccepted( void )
{
    return g_UGCProjectChooser->pPossibleUGCProjects->uAccountAcceptedProjectEULACrc == ugcProjectChooser_CreateProjectEULAGetCrc();
}

void ugcProjectChooser_ProjectDrawByID( ContainerID projectID, UI_MY_ARGS, float z, bool isSelected, bool isHovering )
{
	PossibleUGCProject* project = ugcProjectChooser_GetPossibleProjectByID( projectID );
	if( project ) {
		UGCProjectChooserEntry entry = { 0 };
		entry.pProject = project;
		ugcProjectChooser_EntryDraw( &entry, UI_MY_VALUES, z, isSelected, isHovering );
	}
}

#include "NNOUGCProjectEditor.h"

#include "GfxClipper.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "UGCError.h"
#include "UIEditable.h"
#include "UIPane.h"
#include "UITextureAssembly.h"
#include "WorldGrid.h"
#include "gclUGC.h"
#include "BlockEarray.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

typedef struct UGCProjectEditorDoc {
	UIPane* pRootPane;
	StaticDefineInt* beaLanguagesModel;
} UGCProjectEditorDoc;

static int ugcProjectEditor_RefreshEditableData( UGCProjectEditorDoc* doc );
static void ugcProjectEditor_RefreshReadOnlyData( int yMiddle );
static const char* ugcProjectEditor_StatusMapLocationImage( UGCProjectStatusQueryInfo* pStatus );
static void ugcProjectEditor_StatusPublishedAndFeaturedString( char** estr, UGCProjectStatusQueryInfo* pStatus );
static void ugcProjectEditor_StatusPublishName( char** estr, UGCProjectStatusQueryInfo* pStatus );
static void ugcProjectEditor_StatusPublishDateString( char** estr, UGCProjectStatusQueryInfo* pStatus );
static void ugcProjectEditor_ShortCode( char** estr );

UGCProjectEditorDoc* ugcProjectEditor_Open( void )
{
	UGCProjectEditorDoc* accum = calloc( 1, sizeof( *accum ));
	accum->pRootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	return accum;
}

static void ugcProjectEditor_SetAuthorAllowsFeatured( UIButton* button, UserData ignored )
{
	ugcEditorShowSetAuthorAllowsFeaturedDialog();
}

void ugcProjectEditor_Refresh( UGCProjectEditorDoc* pDoc )
{
	MEFieldContext* uiCtx = MEContextPush( "UGCProjectEditor", NULL, NULL, NULL );
	UGCRuntimeErrorContext* errorCtx = ugcMakeTempErrorContextDefault();
	int yMiddle;

	// Refresh the list of supported languages
	{
		StaticDefineInt* accum;
		int it;
		beaSetSize( &pDoc->beaLanguagesModel, 0 );

		accum = beaPushEmpty( &pDoc->beaLanguagesModel );
		accum->key = U32_TO_PTR( DM_INT );

		for( it = 0; it != LANGUAGE_MAX; ++it ) {
			if( langIsSupportedThisShard( it )) {
				accum = beaPushEmpty( &pDoc->beaLanguagesModel );
				accum->key = locGetDisplayName( locGetIDByLanguage( it ));
				accum->value = it;
			}
		}

		accum = beaPushEmpty( &pDoc->beaLanguagesModel );
		accum->key = U32_TO_PTR( DM_END );
	}
	
	uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
	MEContextSetErrorFunction( ugcEditorMEFieldErrorCB );
	MEContextSetErrorIcon( "ugc_icons_labels_alert", -1, -1 );
	setVec2( uiCtx->iErrorIconOffset, 5, 3 );
	uiCtx->iErrorIconSpaceWidth = atlasFindTexture( "ugc_icons_labels_alert" )->width + 10;
	MEContextSetErrorContext( errorCtx );

	uiCtx->pUIContainer = UI_WIDGET( pDoc->pRootPane );
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;

	// Header pane
	{
		UIPane* pane;
		MEFieldContextEntry* entry;
		UIWidget* widget;

		pane = MEContextPushPaneParent( "HeaderPane" );
		ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, UGC_PANE_TOP_BORDER, UIUnitPercentage, UIUnitFixed );
		
		entry = MEContextAddLabelMsg( "Header", "UGC_ProjectEditor.Header", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
		ui_LabelResize( ENTRY_LABEL( entry ));
		ui_WidgetSetPosition( widget, 4, 10 );
		
		MEContextPop( "HeaderPane" );
	}
	yMiddle = ugcProjectEditor_RefreshEditableData( pDoc );
	ugcProjectEditor_RefreshReadOnlyData( yMiddle );

	MEContextPop( "UGCProjectEditor" );
}

void ugcProjectEditor_SetVisible( UGCProjectEditorDoc* pDoc )
{
	ugcEditorSetDocPane( pDoc->pRootPane );
}

void ugcProjectEditor_Close( UGCProjectEditorDoc** ppDoc )
{
	if( !*ppDoc ) {
		return;
	}

	MEContextPush( "UGCProjectEditor", NULL, NULL, NULL );
	MEContextPop( "UGCProjectEditor" );

	beaDestroy( &(*ppDoc)->beaLanguagesModel );
	ui_WidgetQueueFreeAndNull( &(*ppDoc)->pRootPane );
	SAFE_FREE( *ppDoc );
}

static void ugcProjectEditor_ComboDrawLanguage( UIComboBox *pCombo, UGCProjectInfo* ugcProjInfo, S32 iRow, F32 fX, F32 fY, F32 fZ, F32 fW, F32 fH, F32 fScale, bool bInBox )
{
	CBox box = {fX, fY, fX + fW, fY + fH};
	const char* languageName = locGetDisplayName( locGetIDByLanguage( ugcProjInfo->eLanguage ));

	clipperPushRestrict(&box);

	{
		UIStyleFont *pFont = ui_ComboBoxGetFont( pCombo );
		F32 fCenterY = fY + fH / 2.f;
		fZ += 0.2f;
		ui_StyleFontUse(pFont, false, UI_WIDGET(pCombo)->state);
		if( !nullStr( languageName )) {
			gfxfont_Printf(fX + UI_HSTEP * fScale, fCenterY, fZ, fScale, fScale, CENTER_Y, "%s", languageName);
		} else {
			const char* defaultText = ui_WidgetGetText(UI_WIDGET(pCombo));
			if( defaultText ) {
				gfxfont_Printf(fX + UI_HSTEP * fScale, fCenterY, fZ, fScale, fScale, CENTER_Y, "%s", defaultText );
			}
		}
	}
	clipperPop();
}

/// The first column is all editable data
int ugcProjectEditor_RefreshEditableData( UGCProjectEditorDoc* doc )
{
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCProjectInfo* info = ugcProj->project;
	MEFieldContext* uiCtx;
	UIPane* pane;
	int yMiddle;
	
	pane = MEContextPushPaneParent( "EditableDataPane" );
	ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, UGC_PANE_TOP_BORDER, 0, 0, UITopLeft );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 0.5, 1, UIUnitPercentage, UIUnitPercentage );

	uiCtx = MEContextPush( "EditableData", NULL, info, parse_UGCProjectInfo );
	uiCtx->fXPosPercentage = 0;
	uiCtx->iXDataStart = 100;
	uiCtx->iYPos = 4;

	MEContextAddTextMsg( false, NULL, "PublicName", "UGC_ProjectEditor.PublicName", "UGC_ProjectEditor.PublicName.Tooltip" );
	entry = ugcMEContextAddProjectEditorMultilineTextMsg( "Description", "UGC_ProjectEditor.Description", "UGC_ProjectEditor.Description.Tooltip" );
	ui_EditableSetDefaultMessage( ENTRY_FIELD( entry )->pUIEditable, "UGC_ProjectEditor.Description.Default" );

	{
		MEFieldContext* ctx = MEContextPush( "LanguageCombo", NULL, info, parse_UGCProjectInfo );
		MEFieldContextEntry* pEntry;

		// 3 means exactly one entry, plus DM_INT and DM_END
		if( beaSize( &doc->beaLanguagesModel ) == 3 ) {
			pEntry = MEContextAddTwoLabels( "Language", TranslateMessageKey( "UGC_ProjectEditor.Language" ), doc->beaLanguagesModel[ 1 ].key, TranslateMessageKey( "UGC_ProjectEditor.Language.Tooltip" ));
		} else {
			ctx->bDontSortComboEnums = true;
			pEntry = MEContextAddEnum( kMEFieldType_Combo, doc->beaLanguagesModel, "Language", TranslateMessageKey( "UGC_ProjectEditor.Language" ), TranslateMessageKey( "UGC_ProjectEditor.Language.Tooltip" ));
			ENTRY_FIELD( pEntry )->bDontSortEnums = true;
			ui_ComboBoxSetDrawFunc( ENTRY_FIELD( pEntry )->pUICombo, ugcProjectEditor_ComboDrawLanguage, info );
		}

		MEContextPop( "LanguageCombo" );
	}

	entry = ugcMEContextAddProjectEditorMultilineTextMsg( "Notes", "UGC_ProjectEditor.Notes", "UGC_ProjectEditor.Notes.Tooltip" );
	ui_EditableSetDefaultMessage( ENTRY_FIELD( entry )->pUIEditable, "UGC_ProjectEditor.Notes.Default" );
	yMiddle = uiCtx->iYPos;
	
	MEContextAddCustomSpacer( 10 );
	entry = MEContextAddSeparator( "Separator" );
	widget = UI_WIDGET( entry->pSeparator );
	ui_WidgetSetPaddingEx( widget, 4, 4, 0, 0 );
	MEContextAddCustomSpacer( 10 );

	entry = MEContextAddLabelMsg( "ErrorsHeader", "UGC.Errors_Long", NULL );
	MEContextStepBackUp();
	uiCtx->iYPos = ui_WidgetGetNextY( UI_WIDGET( ENTRY_LABEL( entry ))) + 5;

	entry = MEContextAddCustom( "ErrorsList" );
	ugcEditorErrorsWidgetRefresh( (UITree**)&ENTRY_WIDGET( entry ), true );
	widget = ENTRY_WIDGET( entry );
	ui_WidgetRemoveFromGroup( widget );
	ui_WidgetAddChild( uiCtx->pUIContainer, widget );

	ui_WidgetSetPosition( widget, 0, 0 );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, uiCtx->iXPos, 30, uiCtx->iYPos, 4 );

	MEContextPop( "EditableData" );
	MEContextPop( "EditableDataPane" );

	return yMiddle;
}

static int eaiMax( const int*const* peai )
{
	int max = eaiGet( peai, 0 );
	int it;
	for( it = 0; it != eaiSize( peai ); ++it ) {
		max = MAX( max, (*peai)[ it ]);
	}

	return max;
}

static MEFieldContextEntry* ugcProjectEditorMEContextAddBarGraphMsg( const char* skin, int barValue, int maxBarValue, const char* uid, const char* messageKey, int y )
{
	MEFieldContextEntry* entry = ugcMEContextAddBarGraphMsg( barValue, maxBarValue, uid, messageKey, NULL );
	UIWidget* labelWidget = UI_WIDGET( ENTRY_LABEL( entry ));
	UIWidget* barWidget = ENTRY_WIDGET( entry );
	UIWidget* countWidget = UI_WIDGET( ENTRY_LABEL2( entry ));
	
	ui_WidgetSetPosition( labelWidget, 0, y );
	ui_WidgetSetPosition( barWidget, 0, y + 4 );
	ui_WidgetSetPositionEx( countWidget, 0, y, 0, 0, UITopRight );
	SET_HANDLE_FROM_STRING( g_hUISkinDict, skin, barWidget->hOverrideSkin );

	return entry;
}

/// The other columns are all read only data
void ugcProjectEditor_RefreshReadOnlyData( int yMiddle )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCProjectInfo* info = ugcProj->project;
	MEFieldContext* uiCtx;
	UGCProjectStatusQueryInfo* pStatus = NULL;
	UGCProjectReviews* pReviews = NULL;
	int iReviewsPageNumber = 0;
	UIPane* pane;
	ugcEditorGetCachedProjectData( &pReviews, &iReviewsPageNumber, &pStatus );

	pane = MEContextPushPaneParent( "ReadOnlyData" );
	ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, UGC_PANE_TOP_BORDER, 0, 0, UITopRight );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 0.5, 1, UIUnitPercentage, UIUnitPercentage );

	uiCtx = MEContextGetCurrent();
	uiCtx->iXDataStart = 100;
	uiCtx->iXPos = 4;
	uiCtx->iYPos = 4;
	uiCtx->fXPosPercentage = 0;
	uiCtx->fDataWidgetWidth = 1;
	{
		MEFieldContextEntry* entry;
		UIWidget* widget;
		char* estr = NULL;
			
		// Put the image as the background
		estrPrintf( &estr, "Header_%s", ugcProjectEditor_StatusMapLocationImage( pStatus ));
		entry = MEContextAddSprite( estr, "LocationImage", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		ui_WidgetSetPositionEx( widget, 0, 0, uiCtx->fXPosPercentage, 0, UITopLeft );
		ui_WidgetSetDimensionsEx( widget, uiCtx->fDataWidgetWidth, yMiddle, UIUnitPercentage, UIUnitFixed );
		ui_WidgetSetPaddingEx( widget, uiCtx->iXPos, 4, 4, 0 );
		ENTRY_SPRITE( entry )->bPreserveAspectRatioFill = true;

		// Place different pieces of info on top of it
		MEContextPush( "ImageParent", NULL, NULL, NULL );
		MEContextSetParent( widget );
		{
			int y = 0;
			bool bIsPublished;

			if( pStatus && !pStatus->bCurrentlyPublishing && pStatus->iLastPublishTime && pStatus->bLastPublishSucceeded ) {
				bIsPublished = true;
			} else {
				bIsPublished = false;
			}

			if( pStatus && pStatus->pFeatured && UGCProject_IsFeaturedWindow( pStatus->pFeatured->iStartTimestamp, pStatus->pFeatured->iEndTimestamp, true, false )) {
				entry = MEContextAddSprite( "Banner_Featured", "FeaturedIcon", NULL, NULL );
				widget = UI_WIDGET( ENTRY_SPRITE( entry ));
				ui_WidgetSetPositionEx( widget, 20, 0, 0, 0, UITopRight );
			} else {
				ugcProjectEditor_StatusPublishedAndFeaturedString( &estr, pStatus );
				entry = MEContextAddLabel( "PublishStatus", estr, NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "Game_Header" );
				ui_LabelResize( ENTRY_LABEL( entry ));
				ui_WidgetSetPositionEx( widget, 4, y, 0, 0, UITopRight );
				y = ui_WidgetGetNextY( widget );

				if( bIsPublished ) {
					ugcProjectEditor_StatusPublishDateString( &estr, pStatus );
					entry = MEContextAddLabel( "PublishDate", estr, NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "Game_Header" );
					ui_LabelResize( ENTRY_LABEL( entry ));
					ui_WidgetSetPositionEx( widget, 4, y, 0, 0, UITopRight );
					y = ui_WidgetGetNextY( widget );
				}
			}

			if( bIsPublished ) {
				if( !pStatus->pFeatured && !pStatus->bAuthorAllowsFeatured ) {
					entry = MEContextAddButtonMsg( "UGC_ProjectEditor.SubmitForFeaturing", NULL, ugcProjectEditor_SetAuthorAllowsFeatured, NULL,
												   "SubmitButton", NULL, "UGC_ProjectEditor.SubmitForFeaturing.Tooltip" );
					widget = UI_WIDGET( ENTRY_BUTTON( entry ));
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopRight );
					ui_ButtonResize( ENTRY_BUTTON( entry ));
					ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
					y += UGC_ROW_HEIGHT;
				}

				y = 0;

				ugcProjectEditor_ShortCode( &estr );
				entry = MEContextAddLabel( "ShortCode", estr, NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "Game_Header" );
				ui_LabelResize( ENTRY_LABEL( entry ));
				ui_WidgetSetPositionEx( widget, 4, y, 0, 0, UIBottomLeft );
				y = ui_WidgetGetNextY( widget );

				ugcProjectEditor_StatusPublishName( &estr, pStatus );
				entry = MEContextAddLabel( "PublishName", estr, NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "Game_Header" );
				ui_LabelResize( ENTRY_LABEL( entry ));
				ui_WidgetSetPositionEx( widget, 4, y, 0, 0, UIBottomLeft );
				ui_LabelForceAutosize( ENTRY_LABEL( entry ));
				
				entry = ugcMEContextAddEditorButton( UGC_ACTION_WITHDRAW, true, false );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomRight );
			}
		}
		MEContextPop( "ImageParent" );

		uiCtx->iYPos = yMiddle;
		MEContextAddCustomSpacer( 10 );
		entry = MEContextAddSeparator( "Separator" );
		widget = UI_WIDGET( entry->pSeparator );
		ui_WidgetSetPaddingEx( widget, 4, 4, 0, 0 );
		MEContextAddCustomSpacer( 10 );

		entry = MEContextAddLabelMsg( "ReviewsHeader", "UGC_ProjectEditor.Reviews", NULL );
		MEContextStepBackUp();
		uiCtx->iYPos = ui_WidgetGetNextY( UI_WIDGET( ENTRY_LABEL( entry ))) + 5;
		if( pReviews ) {
			{
				float yTop = uiCtx->iYPos;
				float yMax = 0;
				float y = 0;
				UIPane* ratingPane;
				UIPane* barChartPane;
				UIPane* tipsPane;
				UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_ContentArea" );

				ratingPane = MEContextPushPaneParent( "AverageRatingPane" );
				ui_PaneSetStyle( ratingPane, texas->pchName, true, false );
				ui_WidgetSetPositionEx( UI_WIDGET( ratingPane ), uiCtx->iXPos, yTop, uiCtx->fXPosPercentage, 0, UITopLeft );
				ui_WidgetSetWidth( UI_WIDGET( ratingPane ), 100 );
				{
					y = 0;

					entry = MEContextAddLabelMsg( "Header", "UGC_ProjectEditor.AverageRating", NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
					ENTRY_LABEL( entry )->textFrom = UITop;
					y = ui_WidgetGetNextY( widget );

					entry = ugcMEContextAddStarRating( info->fAverageRating, "Stars" );
					widget = ENTRY_WIDGET( entry );
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
					y = ui_WidgetGetNextY( widget ) + 4;

					estrPrintf( &estr, "%1.1f", info->fAverageRating * 5 );
					entry = MEContextAddLabel( "Rating", estr, NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Important_Huge" );
					ui_LabelResize( ENTRY_LABEL( entry ));
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
					y = ui_WidgetGetNextY( widget ) + 4;

					ugcFormatMessageKey( &estr, "UGC_ProjectEditor.NumRatings",
										 STRFMT_INT( "NumRatings", pReviews->iNumRatingsCached ),
										 STRFMT_END );
					entry = MEContextAddLabel( "TotalCount", estr, NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 90, UIUnitFixed );
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
					ENTRY_LABEL( entry )->textFrom = UITop;
					y = ui_WidgetGetNextY( widget );
				}
				yMax = MAX( yMax, y );
				MEContextPop( "AverageRatingPane" );

				barChartPane = MEContextPushPaneParent( "BarChartPane" );
				ui_PaneSetStyle( barChartPane, texas->pchName, true, false );
				ui_WidgetSetPositionEx( UI_WIDGET( barChartPane), 0, yTop, uiCtx->fXPosPercentage, 0, UITopLeft );
				ui_WidgetSetWidthEx( UI_WIDGET( barChartPane ), 1 - uiCtx->fXPosPercentage, UIUnitPercentage );
				ui_WidgetSetPaddingEx( UI_WIDGET( barChartPane ), 110, 110, 0, 0 );
				MEContextGetCurrent()->iXPos = 0;
				MEContextGetCurrent()->fXPosPercentage = 0;
				{
					int maxRating = eaiMax( &pReviews->piNumRatings );

					y = 0;
					entry = ugcProjectEditorMEContextAddBarGraphMsg( "UGCBarGraph_5Star", eaiGet( &pReviews->piNumRatings, 4 ), maxRating,
																	 "Reviews5Star", "UGC_ProjectEditor.5Star", y );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					y = ui_WidgetGetNextY( widget );

					entry = ugcProjectEditorMEContextAddBarGraphMsg( "UGCBarGraph_4Star", eaiGet( &pReviews->piNumRatings, 3 ), maxRating,
																	 "Reviews4Star", "UGC_ProjectEditor.4Star", y );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					y = ui_WidgetGetNextY( widget );

					entry = ugcProjectEditorMEContextAddBarGraphMsg( "UGCBarGraph_3Star", eaiGet( &pReviews->piNumRatings, 2 ), maxRating,
																	 "Reviews3Star", "UGC_ProjectEditor.3Star", y );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					y = ui_WidgetGetNextY( widget );

					entry = ugcProjectEditorMEContextAddBarGraphMsg( "UGCBarGraph_2Star", eaiGet( &pReviews->piNumRatings, 1 ), maxRating,
																	 "Reviews2Star", "UGC_ProjectEditor.2Star", y );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					y = ui_WidgetGetNextY( widget );

					entry = ugcProjectEditorMEContextAddBarGraphMsg( "UGCBarGraph_1Star", eaiGet( &pReviews->piNumRatings, 0 ), maxRating,
																	 "Reviews1Star", "UGC_ProjectEditor.1Star", y );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					y = ui_WidgetGetNextY( widget );

					entry = MEContextAddSeparator( "VSeparator" );
					widget = UI_WIDGET( entry->pSeparator );
					entry->pSeparator->orientation = UIVertical;
					ui_SeparatorResize( entry->pSeparator );
					ui_WidgetSetPosition( widget, 50, 0 );
				}
				yMax = MAX( yMax, y );
				MEContextPop( "BarChartPane" );

				tipsPane = MEContextPushPaneParent( "TipsPane" );
				ui_PaneSetStyle( tipsPane, texas->pchName, true, false );
				ui_WidgetSetPositionEx( UI_WIDGET( tipsPane ), uiCtx->iXPos, yTop, 0, 0, UITopRight );
				ui_WidgetSetWidth( UI_WIDGET( tipsPane ), 100 );
				MEContextGetCurrent()->iXPos = 0;
				MEContextGetCurrent()->fXPosPercentage = 0;
				{
					y = 0;

					entry = MEContextAddLabelMsg( "Header", "UGC_ProjectEditor.Tips", NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
					ENTRY_LABEL( entry )->textFrom = UITop;
					y = ui_WidgetGetNextY( widget ) + 12 + 4;

					estrPrintf( &estr, "%d", info->uLifetimeTipsReceived );
					entry = MEContextAddLabel( "Count", estr, NULL );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));
					UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Important_Huge" );
					ui_LabelResize( ENTRY_LABEL( entry ));
					ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITop );
					y = ui_WidgetGetNextY( widget );
				}
				yMax = MAX( yMax, y );
				MEContextPop( "TipsPane" );

				ui_WidgetSetHeight( UI_WIDGET( ratingPane ), yMax + ui_TextureAssemblyHeight( texas ));
				ui_WidgetSetHeight( UI_WIDGET( barChartPane ), yMax + ui_TextureAssemblyHeight( texas ));
				ui_WidgetSetHeight( UI_WIDGET( tipsPane ), yMax + ui_TextureAssemblyHeight( texas ));
				yMax = MAX( yMax, y );

				uiCtx->iYPos = yTop + yMax + ui_TextureAssemblyHeight( texas );
			}

			MEContextAddCustomSpacer( 4 );

			pane = MEContextPushPaneParent( "ReviewsPane" );
			ui_PaneSetStyle( pane, "UGC_Pane_ContentArea", true, false );
			ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
			ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			ui_WidgetSetPaddingEx( UI_WIDGET( pane ), uiCtx->iXPos, uiCtx->iXPos, uiCtx->iYPos, 4 );
			{
				MEFieldContext* scrollCtx;
				UIScrollArea* scrollArea;
		
				// Add the scroll area
				entry = MEContextAddCustom( "ReviewScroll" );
				if( !ENTRY_WIDGET( entry )) {
					scrollArea = ui_ScrollAreaCreate( 0, 0, 1, 1, 1, 1, false, true );
					ENTRY_WIDGET( entry ) = UI_WIDGET( scrollArea );
				} else {
					scrollArea = (UIScrollArea*)ENTRY_WIDGET( entry );
				}
				widget = UI_WIDGET( scrollArea );
				ui_WidgetRemoveFromGroup( widget );
				ui_WidgetAddChild( UI_WIDGET( pane ), widget );
				//ui_WidgetSetPositionEx( widget, 0, 0, uiCtx->fXPosPercentage, 0, UITopLeft );
				//ui_WidgetSetPaddingEx( widget, uiCtx->iXPos, 4, uiCtx->iYPos, 4 );
				//ui_WidgetSetDimensionsEx( widget, uiCtx->fDataWidgetWidth, 1, UIUnitPercentage, UIUnitPercentage );
				ui_WidgetSetPosition( widget, 0, 0 );
				ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
				scrollArea->autosize = true;

				scrollCtx = MEContextPush( "ScrollArea", NULL, NULL, NULL );
				scrollCtx->iXPos = 0;
				scrollCtx->fXPosPercentage = 0;
				MEContextSetParent( UI_WIDGET( scrollArea ));

				if( eaSize( &pReviews->ppReviews ) == 0 ) {
					ui_WidgetSetTextMessage( UI_WIDGET( scrollArea ), "UGC_ProjectEditor.NoReviews" );
				} else {
					int it;
					ui_WidgetSetTextString( UI_WIDGET( scrollArea ), "" );
					for( it = 0; it != eaSize( &pReviews->ppReviews ); ++it ) {
						UGCSingleReview* pReview = pReviews->ppReviews[ it ];

						ugcFormatMessageKey( &estr, "UGC_ProjectEditor.ReviewFormat",
											 STRFMT_STRING( "ReviewerAccount", pReview->pReviewerAccountName ),
											 STRFMT_STRING( "ReviewTime", timeGetLocalDateNoTimeStringFromSecondsSince2000( pReview->iTimestamp )),
											 STRFMT_END );
						MEContextAddLabelIndex( "SimpleReviewHeader", it, estr, NULL );
						MEContextStepBackUp();
						entry = ugcMEContextAddStarRatingIndex( pReview->fRating, "SingleReviewHeader", it );
						ui_WidgetSetPositionEx( ENTRY_WIDGET( entry ), 0, ENTRY_WIDGET( entry )->y, 0, 0, UITopRight );

						MEContextIndentRight();
					
						entry = MEContextAddLabelIndex( "SingleReviewText", it, pReview->pComment, NULL );
						widget = UI_WIDGET( ENTRY_LABEL( entry ));
						ui_LabelSetWordWrap( ENTRY_LABEL( entry ), true );
						ui_LabelUpdateDimensionsForWidth( ENTRY_LABEL( entry ), 400 );
						MEContextStepBackUp();
						MEContextAddCustomSpacer( ui_WidgetGetHeight( widget ));
						MEContextIndentLeft();

						MEContextAddCustomSpacer( UGC_ROW_HEIGHT / 2 );
						MEContextAddSeparatorIndex( "ReviewSeparator", it );
						MEContextAddCustomSpacer( UGC_ROW_HEIGHT / 2 );
					}

					if( iReviewsPageNumber+1 < pReviews->iNumReviewPagesCached  ) {
						entry = MEContextAddButtonMsg( "UGC_ProjectEditor.MoreReviews", NULL, ugcEditorGetMoreReviews, NULL, "MoreReviews", NULL, NULL );
						widget = UI_WIDGET( ENTRY_BUTTON( entry ));
						ui_SetActive( widget, !gclUGC_RequestReviewsInProgress() );
						widget->x = 0;
						ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
						ui_WidgetSetPaddingEx( widget, 4, 4, 0, 0 );
					}
				}

				MEContextPop( "ScrollArea" );
			}
			MEContextPop( "ReviewsPane" );
		}
	}
	MEContextPop( "ReadOnlyData" );
}

const char* ugcProjectEditor_StatusMapLocationImage( UGCProjectStatusQueryInfo* pStatus )
{
	if( !pStatus || !pStatus->pPublishedMapLocation || !pStatus->pPublishedMapLocation->astrIcon ) {
		return "Mapicon_Neverwintercity_01";
	} else {
		return pStatus->pPublishedMapLocation->astrIcon;
	}
}

void ugcProjectEditor_StatusPublishedAndFeaturedString( char** estr, UGCProjectStatusQueryInfo* pStatus )
{
	if( !pStatus ) {
		// Haven't yet found out the status -- have it be empty
		estrPrintf( estr, "" );
	} else if( !pStatus->bCurrentlyPublishing ) {
		if( !pStatus->iLastPublishTime ) {
			ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_NotPublished", STRFMT_END );
		} else if( !pStatus->bLastPublishSucceeded ) {
			ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_PublishFailed", STRFMT_END );
		} else {
			if( pStatus->pFeatured ) {
				U32 curTime = timeSecondsSince2000();
				if( curTime > pStatus->pFeatured->iStartTimestamp ) {
					ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_Featured", STRFMT_END );
				} else {
					ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_FutureFeatured", STRFMT_END );
				}
			} else if( pStatus->bAuthorAllowsFeatured ) {
				ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_SubmitedForFeatured", STRFMT_END );
			} else {
				ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_Published", STRFMT_END );
			}
		}
	} else {
		ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_PublishInProgress", STRFMT_END );
		/*
		if( pStatus->iCurPlaceInQueue > 0 ) {
			estrPrintf( estr, "Waiting to Publish (#%d in queue)", pStatus->iCurPlaceInQueue );
		} else if( pStatus->iCurPlaceInQueue < 0 ) {
			estrPrintf( estr, "Waiting to Publish" );
		} else {
			estrPrintf( estr, "Currently Publishing, %.02f%% done", pStatus->fPublishPercentage );
		}
		*/
	}
}

void ugcProjectEditor_StatusPublishName( char** estr, UGCProjectStatusQueryInfo* pStatus )
{
	estrPrintf( estr, "%s", pStatus->strPublishedName );
}

void ugcProjectEditor_StatusPublishDateString( char** estr, UGCProjectStatusQueryInfo* pStatus )
{
	if( pStatus && pStatus->pFeatured ) {
		ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_Timestamp",
							 STRFMT_STRING( "Time", timeGetLocalDateNoTimeStringFromSecondsSince2000( pStatus->pFeatured->iStartTimestamp )),
							 STRFMT_END );
	} else if( pStatus && !pStatus->bCurrentlyPublishing && pStatus->iLastPublishTime && pStatus->bLastPublishSucceeded ) {
		ugcFormatMessageKey( estr, "UGC_ProjectEditor.State_Timestamp",
							 STRFMT_STRING( "Time", timeGetLocalDateNoTimeStringFromSecondsSince2000( pStatus->iLastPublishTime )),
							 STRFMT_END );
	} else {
		estrPrintf( estr, "" );
	}
}

void ugcProjectEditor_ShortCode( char** estr )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCProjectInfo* info = ugcProj->project;
	ContainerID projID = UGCProject_GetContainerIDFromUGCNamespace( info->pcName );
	char buffer[ UGC_IDSTRING_LENGTH_BUFFER_LENGTH ] = "";

	if( projID ) {
		UGCIDString_IntToString( projID, false, buffer );
	}
	estrPrintf( estr, "%s", buffer );
}

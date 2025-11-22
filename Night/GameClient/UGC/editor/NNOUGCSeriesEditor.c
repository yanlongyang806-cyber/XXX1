#include "NNOUGCSeriesEditor.h"

#include "GameStringFormat.h"
#include "GfxClipper.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "GlobalComm.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCProjectChooser.h"
#include "ResourceInfo.h"
#include "ResourceSearch.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "TimedCallback.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UGCError.h"
#include "UGCProjectChooser.h"
#include "UGCProjectCommon.h"
#include "UGCSeriesEditor.h"
#include "UITextureAssembly.h"
#include "file.h"
#include "gclLogin.h"
#include "net.h"
#include "structNet.h"

#include "UGCProjectCommon_h_ast.h"

typedef struct UIButton UIButton;

typedef struct UGCProjectSeries UGCProjectSeries;
typedef struct UGCProjectSeriesGroup UGCProjectSeriesGroup;
typedef struct UGCRuntimeStatus UGCRuntimeStatus;
typedef struct UIPane UIPane;
typedef struct UIList UIList;
typedef struct UIWindow UIWindow;
typedef void* UserData;

typedef void (*UGCProjectSeriesGroupChangedFn)( UGCProjectSeriesGroup* pGroup, UserData data );

typedef enum UGCSeriesEditorCloseOption
{
	UGC_SERIES_EDITOR_CLOSE_NONE = 0,

	UGC_SERIES_EDITOR_CLOSE_CANCEL = 1,	// canceling the operation
	UGC_SERIES_EDITOR_CLOSE,			// closing the project and returning to the project chooser

	// Save bit indicates that a save should happen, then the close option should be queued
	UGC_SERIES_EDITOR_SAVE = BIT(31)
} UGCSeriesEditorCloseOption;

typedef struct UGCSeriesEditor {
	UGCProjectSeries* pLastSaveSeries;
	UGCProjectSeries* pSeries;
	UGCRuntimeStatus runtimeStatus;
	UGCRuntimeError** eaRuntimeErrors;
	
	UIPane* pMainPane;

	UGCProjectSeriesGroup* pDetails;
	UIWindow* pModalDialog;
	TimedCallback* pModalTimeout;

	unsigned isSaving : 1;
	unsigned isCloseQueued : 1;
	unsigned isNewSeriesQueued : 1;

	ContainerID newSeriesID;
} UGCSeriesEditor;

static UGCSeriesEditor g_UGCSeriesDoc;

typedef struct UGCProjectSeriesGroup
{
	bool bIgnoreChanges;
	
	const char* astrName;
	UIPane* pParent;
	UGCProjectSeries* pSeries;
	UGCProjectSeriesGroupChangedFn changedFn;
	UserData changedData;

	UIList* projectsList;
	UIList* pAddProjectsList;
	UIWindow* pModalDialog;

	// Icon search result
	ResourceSearchResultRow** eaIconRows;
} UGCProjectSeriesGroup;

UGCProjectSeriesGroup* ugcSeriesGroup_Create( const char* groupName, UIPane* pParent, UGCProjectSeriesGroupChangedFn changedFn, UserData changedData );
void ugcSeriesGroup_Destroy( UGCProjectSeriesGroup** ppGroup );
void ugcSeriesGroup_Refresh( UGCProjectSeriesGroup* pGroup, UGCProjectSeries* pSeries, bool bAllowSave, UGCRuntimeStatus* runtimeStatus );
void ugcSeriesGroup_CloseModalDialog( UGCProjectSeriesGroup* pGroup );

static void ugcSeriesEditor_MEFieldChangedCB( MEField* pField, bool bFinished, void* rawGroup );
static void ugcSeriesEditor_WidgetCB(UIWidget* ignored, void* rawGroup );
static void ugcSeriesEditor_IconListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData pDrawData );
static void ugcSeriesEditor_IconListDraw( ResourceSearchResultRow* row, UI_MY_ARGS, float z, bool isSelected, bool isHovering );
static void ugcSeriesEditor_IconListSelectCB( UIList* pList, UGCProjectSeriesGroup* pGroup );
static void ugcSeriesEditor_ProjectListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData pDrawData );

static int ugcSeriesEditor_GetSelectedSeriesNodeIndex( UGCProjectSeriesGroup* pGroup );
static void ugcSeriesEditor_AddProjects( UIButton* ignored, UserData rawGroup );
static void ugcSeriesEditor_DeleteNode( UIButton* ignored, UserData rawGroup );
static void ugcSeriesEditor_MoveNodeUpCB( UIButton* ignored, UserData rawGroup );
static void ugcSeriesEditor_MoveNodeDownCB( UIButton* ignored, UserData rawGroup );
static void ugcSeriesEditor_CloseModalDialog( void );
static void ugcSeriesEditor_Save( UIButton* ignored, UserData ignored2 );
static void ugcSeriesEditor_ReturnCB( UIButton* ignored, UserData ignored2 );
static void ugcSeriesEditor_RecieveSeriesList( UGCProjectSeries** eaProjectSeries );
	
PossibleUGCProject** g_eaOwnedProjectNoSeriesRows;

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

/// MJF: This is a crappy way to do this -- a "context" global
/// variable. :(
static UGCRuntimeStatus* s_UGCSeriesEditorRuntimeStatus = NULL;
static bool ugcSeriesEditorMEFieldErrorCB(UGCRuntimeErrorContext*pErrorContext, const char *pchFieldName, int iFieldIndex, char **estrToolTip_out)
{
	return ugcEditorErrorPrint(s_UGCSeriesEditorRuntimeStatus, pErrorContext, pchFieldName, iFieldIndex, estrToolTip_out);
}

UGCProjectSeriesGroup* ugcSeriesGroup_Create( const char* seriesName, UIPane* pParent, UGCProjectSeriesGroupChangedFn changedFn, UserData changedData )
{
	UGCProjectSeriesGroup* accum = calloc( 1, sizeof( *accum ));

	accum->astrName = allocAddString( seriesName );
	accum->pParent = pParent;
	accum->changedFn = changedFn;
	accum->changedData = changedData;

	return accum;
}

void ugcSeriesGroup_Destroy( UGCProjectSeriesGroup** ppGroup )
{
	if( !*ppGroup ) {
		return;
	}

	MEContextDestroyByName( (*ppGroup)->astrName );
	(*ppGroup)->projectsList = NULL;
	ui_WidgetQueueFreeAndNull( &(*ppGroup)->pAddProjectsList );
	ugcSeriesGroup_CloseModalDialog( *ppGroup );
	
	SAFE_FREE( *ppGroup );
}

void ugcSeriesGroup_Refresh( UGCProjectSeriesGroup* pGroup, UGCProjectSeries* pSeries, bool bAllowSave, UGCRuntimeStatus* runtimeStatus )
{
	char* estr = NULL;
	MEFieldContext* uiCtx;
	bool bOldIgnoreChanges;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIPane* pane;
	UIList* list;

	if( !pGroup ) {
		return;
	}
	
	MEFieldSetDisableContextMenu( true );

	s_UGCSeriesEditorRuntimeStatus = runtimeStatus;
	pGroup->pSeries = pSeries;
	pGroup->pParent->invisible = true;
	bOldIgnoreChanges = pGroup->bIgnoreChanges;
	pGroup->bIgnoreChanges = true;

	uiCtx = MEContextPush( pGroup->astrName, NULL, NULL, NULL );
	uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
	uiCtx->cbChanged = ugcSeriesEditor_MEFieldChangedCB;
	uiCtx->pChangedData = pGroup;
	uiCtx->iXDataStart = PROJECT_INFO_DATA_COLUMN;
	uiCtx->iYStep = UGC_ROW_HEIGHT;
	uiCtx->iTextAreaHeight = 3;
	MEContextSetParent( UI_WIDGET( pGroup->pParent ));
	
	entry = MEContextAddSprite( "UGC_Dashboard_Background", "BG", NULL, NULL );
	widget = UI_WIDGET( ENTRY_SPRITE( entry ));
	ENTRY_SPRITE( entry )->bPreserveAspectRatioFill = true;
	ui_WidgetSetPosition( widget, 0, 0 );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

	entry = MEContextAddLabel( "Header", "Campaign Editor", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_Header", widget->hOverrideSkin );
	ui_WidgetSetPositionEx( widget, 0, 640, 0, 0, UINoDirection );
	ui_LabelResize( ENTRY_LABEL( entry ));

	pane = MEContextPushPaneParent( "ContentPane" );
	ui_PaneSetStyle( pane, "Window_Without_TitleBar_NoFooter", true, false );
	ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 30, 0, 0, UINoDirection );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 850, 580, UIUnitFixed, UIUnitFixed );
	pane->widget.priority = 200;
	if( pSeries ) {
		UGCProjectSeriesVersion* pVersion = pSeries->eaVersions[ 0 ];

		pane = MEContextPushPaneParent( "Col1" );
		ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 0, 0, 0, UITopLeft );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 0.49, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, 0, 20 );
		{
			UGCRuntimeErrorContext* errorCtx = ugcMakeTempErrorContextDefault();
			const char* errorIconName = "ugc_icons_labels_alert";
			MEFieldContext* seriesUICtx;
			
			seriesUICtx = MEContextPush( "UGCSeriesGroup_SeriesData", pVersion, pVersion, parse_UGCProjectSeriesVersion );
			MEContextSetErrorContext( errorCtx );
			MEContextSetErrorFunction( ugcSeriesEditorMEFieldErrorCB );
			MEContextSetErrorIcon( errorIconName, -1, -1 );
			setVec2( seriesUICtx->iErrorIconOffset, 5, 3 );
			{
				MEContextAddTextMsg( false, "<EMPTY>", "strName", "UGC_SeriesEditor.Name", "UGC_SeriesEditor.Name.Tooltip" );
				
				entry = MEContextAddTextMsg( true, NULL, "strDescription", "UGC_SeriesEditor.Description", "UGC_SeriesEditor.Description.Tooltip" );
				widget = ENTRY_FIELD( entry )->pUIWidget;
				ui_EditableSetMaxLength( UI_EDITABLE( ENTRY_FIELD( entry )->pUITextArea ), UGC_TEXT_MULTI_LINE_MAX_LENGTH );
				MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget ) + 4;

				if( ugcDefaultsPreviewImagesAndOverworldMapEnabled() ) {
					// Search for all valid icons (as if we were the asset library)
					{
						ResourceSearchRequest request = { 0 };
						ResourceSearchResult* result = NULL;
						
						request.eSearchMode = SEARCH_MODE_TAG_SEARCH;
						request.pcSearchDetails = "UGC, OverworldMapIcon";
						request.pcName = NULL;
						request.pcType = "Texture";
						result = ugcResourceSearchRequest( &request );
						eaClearStruct( &pGroup->eaIconRows, parse_ResourceSearchResultRow );
						eaCopyStructs( &result->eaRows, &pGroup->eaIconRows, parse_ResourceSearchResultRow );
						StructDestroySafe( parse_ResourceSearchResult, &result );
					}

					entry = MEContextAddLabelMsg( "MapIconHeader", "UGC_SeriesEditor.MapIcon", "UGC_SeriesEditor.MapIcon.Tooltip" );
					widget = UI_WIDGET( ENTRY_LABEL( entry ));

					{
						int iYPos = ui_WidgetGetNextY( widget );
						entry = ugcMEContextAddList( &pGroup->eaIconRows, 70, ugcSeriesEditor_IconListDrawCB, pGroup, "MapIcon" );
						list = (UIList*)ENTRY_WIDGET( entry );
						widget = ENTRY_WIDGET( entry );
						ui_ListSetSelectedCallback( list, ugcSeriesEditor_IconListSelectCB, pGroup );
						ui_WidgetSetPosition( widget, 0, 0 );
						ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
						ui_WidgetSetPaddingEx( widget, 0, seriesUICtx->iErrorIconSpaceWidth, iYPos, 0 );
						FOR_EACH_IN_EARRAY_FORWARDS( pGroup->eaIconRows, ResourceSearchResultRow, row ) {
							if( stricmp( row->pcName, pVersion->strImage ) == 0 ) {
								ui_ListSetSelectedRow( list, FOR_EACH_IDX( result->eaRows, row ));
								break;
							}
						} FOR_EACH_END;
					}
				}
				
			}
			MEContextPop( "UGCSeriesGroup_SeriesData" );
		}
		MEContextPop( "Col1" );

		pane = MEContextPushPaneParent( "Col2" );
		ui_WidgetSetPositionEx( UI_WIDGET( pane ), 0, 0, 0, 0, UITopRight );
		ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 0.49, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, 0, 20 );
		{
			int toolbarY;
			
			MEContextAddLabelMsg( "ProjectsHeader", "UGC_SeriesEditor.ProjectsInSeries", NULL );
			toolbarY = MEContextGetCurrent()->iYPos;

			entry = ugcMEContextAddList( &CONTAINER_NOCONST( UGCProjectSeriesVersion, pVersion )->eaChildNodes, 80, ugcSeriesEditor_ProjectListDrawCB, NULL, "Projects" );
			list = (UIList*)ENTRY_WIDGET( entry );
			widget = ENTRY_WIDGET( entry );
			ui_ListSetSelectedCallback( list, ugcSeriesEditor_WidgetCB, pGroup );
			// Position after the toolbar is positioned
			pGroup->projectsList = list;

			{
				int selectedIndex = ui_ListGetSelectedRow( pGroup->projectsList );
				UIWidget** eaToolbarWidgets = NULL;
				
				entry = MEContextAddButtonMsg( "UGC_SeriesEditor.AddProject", NULL, ugcSeriesEditor_AddProjects, pGroup, "ToolbarAddProject", NULL, "UGC_SeriesEditor.AddProject.Tooltip" );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ui_SetActive( widget, eaSize( &g_eaOwnedProjectNoSeriesRows ) > 0 );
				eaPush( &eaToolbarWidgets, widget );
				
				entry = MEContextAddButtonMsg( "UGC_SeriesEditor.DeleteProject", NULL, ugcSeriesEditor_DeleteNode, pGroup, "ToolbarDelete", NULL, "UGC_SeriesEditor.DeleteProject.Tooltip" );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ui_SetActive( widget, selectedIndex >= 0 );
				eaPush( &eaToolbarWidgets, widget );

				entry = MEContextAddButtonMsg( "UGC_SeriesEditor.MoveProjectUp", NULL, ugcSeriesEditor_MoveNodeUpCB, pGroup, "ToolbarMoveUp", NULL, "UGC_SeriesEditor.MoveProjectUp.Tooltip" );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ui_SetActive( widget, selectedIndex > 0 );
				eaPush( &eaToolbarWidgets, widget );

				entry = MEContextAddButtonMsg( "UGC_SeriesEditor.MoveProjectDown", NULL, ugcSeriesEditor_MoveNodeDownCB, pGroup, "ToolbarMoveDown", NULL, "UGC_SeriesEditor.MoveProjectDown.Tooltip" );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ui_SetActive( widget, selectedIndex >= 0 && selectedIndex < eaSize( &pVersion->eaChildNodes ) - 1 );
				eaPush( &eaToolbarWidgets, widget );

				{
					int toolbarIt;
					float buttonWidth = 1.0f / eaSize( &eaToolbarWidgets );
					for( toolbarIt = 0; toolbarIt != eaSize( &eaToolbarWidgets ); ++toolbarIt ) {
						widget = eaToolbarWidgets[ toolbarIt ];
						ui_WidgetSetPositionEx( widget, 0, toolbarY, buttonWidth * toolbarIt, 0, UITopLeft );
						ui_WidgetSetWidthEx( widget, buttonWidth, UIUnitPercentage );
					}
					MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );
				}

				eaDestroy( &eaToolbarWidgets );
			}

			// Position the list
			ui_WidgetSetPosition( UI_WIDGET( pGroup->projectsList ), 0, 0 );
			ui_WidgetSetDimensionsEx( UI_WIDGET( pGroup->projectsList ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			ui_WidgetSetPaddingEx( UI_WIDGET( pGroup->projectsList ), 0, 0, MEContextGetCurrent()->iYPos, 0 );
		}
		MEContextPop( "Col2" );
	}
	MEContextPop( "ContentPane" );

	// Update the save button
	if( pSeries ) {
		entry = MEContextAddButtonMsg( "UGC_SeriesEditor.Save", NULL, ugcSeriesEditor_Save, NULL, "HeaderSave", NULL, "UGC_SeriesEditor.Save.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_HeavyweightButton", widget->hOverrideSkin );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, -550, 0, 0, UINoDirection );
		ui_WidgetSetWidth( widget, MAX( 350, widget->width ));
		ui_WidgetSetPaddingEx( UI_WIDGET( ENTRY_BUTTON( entry )), 0, 0, 0, 0 );
		widget->priority = 200;
		ui_WidgetGroupSteal( widget->group, widget );

		if( isDevelopmentMode() ) {
			estrPrintf( &estr, "<DEV MODE> Container ID: %d", pSeries->id );
			entry = MEContextAddLabel( "DevModeID", estr, NULL );
			widget = UI_WIDGET( ENTRY_LABEL( entry ));
			ui_LabelResize( ENTRY_LABEL( entry ));
			ui_WidgetSetPositionEx( widget, 0, -600, 0, 0, UINoDirection );
			widget->priority = 200;
		}
	}

	entry = MEContextAddSprite( "Login_Footer", "Footer", NULL, NULL );
	widget = UI_WIDGET( ENTRY_SPRITE( entry ));
	ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UIBottomLeft );
	ui_WidgetSetDimensionsEx( widget, 1, 100, UIUnitPercentage, UIUnitFixed );

	entry = MEContextAddButtonMsg( "UGC_SeriesEditor.Return", NULL, ugcSeriesEditor_ReturnCB, NULL, "ReturnButton", NULL, "UGC_SeriesEditor.Return.Tooltip" );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser_BackButton", widget->hOverrideSkin );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPositionEx( widget, 20, 20, 0, 0, UIBottomLeft );
	ui_WidgetSetDimensions( widget, MAX( widget->width, 100 ), MAX( widget->height, 40 ));

	ui_WidgetGroupSort( &uiCtx->pUIContainer->children );
	MEContextPop( pGroup->astrName );
	s_UGCSeriesEditorRuntimeStatus = NULL;
	pGroup->bIgnoreChanges = bOldIgnoreChanges;

	estrDestroy( &estr );

	MEFieldSetDisableContextMenu( false );
}

int ugcSeriesEditor_GetSelectedSeriesNodeIndex( UGCProjectSeriesGroup* pGroup )
{
	if( pGroup->projectsList ) {
		return ui_ListGetSelectedRow( pGroup->projectsList );
	} else {
		return -1;
	}
}

void ugcSeriesEditor_MEFieldChangedCB( MEField* pField, bool bFinished, void* rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	if( bFinished && !pGroup->bIgnoreChanges ) {
		pGroup->changedFn( pGroup, pGroup->changedData );
	}
}

void ugcSeriesEditor_WidgetCB(UIWidget* ignored, void* rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	if( !pGroup->bIgnoreChanges ) {
		pGroup->changedFn( pGroup, pGroup->changedData );
	}
}

void ugcSeriesEditor_IconListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData pDrawData )
{
	ResourceSearchResultRow* row = eaGet( pList->peaModel, iRow );
	if( row ) {
		CBox box;
		BuildCBox( &box, x, y, w, h );
		ugcSeriesEditor_IconListDraw( row, UI_MY_VALUES, z, ui_ListIsSelected( pList, pColumn, iRow ), ui_ListIsHovering( pList, pColumn, iRow ));
	}
}

void ugcSeriesEditor_IconListDraw( ResourceSearchResultRow* row, UI_MY_ARGS, float z, bool isSelected, bool isHovering )
{
	UITextureAssembly* normal = RefSystem_ReferentFromString( "UITextureAssembly", "UGCProjectChooser_Item_Idle" );
	UITextureAssembly* hover = RefSystem_ReferentFromString( "UITextureAssembly", "UGCProjectChooser_Item_Over" );
	UITextureAssembly* selected = RefSystem_ReferentFromString( "UITextureAssembly", "UGCProjectChooser_Item_Selected" );
	UIStyleFont* bigFont = ui_StyleFontGet( "Game_Header" );
	UIStyleFont* bigFontSelected = ui_StyleFontGet( "Game_Header_Yellow" );
	
	CBox box;
	CBox drawBox;
	BuildCBox( &box, x, y, w, h );
	BuildCBox( &drawBox, x, y + 2, w - 2, h - 2 );

	if( !normal || !hover || !selected || !bigFont || !bigFontSelected ) {
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
		CBox spriteBox = drawBox;
		char buffer[ RESOURCE_NAME_MAX_SIZE ];
		AtlasTex* tex;

		sprintf( buffer, "Header_%s", row->pcName );
		tex = atlasLoadTexture( buffer );
		ui_SpriteCalcDrawBox( &spriteBox, tex, NULL, true );
		display_sprite_box( tex, &spriteBox, z, (isSelected || isHovering ? -1 : 0xFFFFFFCC) );
	}
	z += 0.1;

	ui_StyleFontUse( isSelected ? bigFontSelected : bigFont, false, 0 );
	{
		const WorldUGCProperties* ugcProps = ugcResourceGetUGCProperties( row->pcType, row->pcName );
		char buffer[ 256 ];
		const char* toPrint;

		toPrint = TranslateDisplayMessage( ugcProps->dVisibleName );
		if( !toPrint ) {
			sprintf( buffer, "%s (UNTRANSLATED)", row->pcName );
			toPrint = buffer;
		}
		
		gfxfont_PrintMaxWidth( drawBox.lx + 4, drawBox.hy - 4, z, w - 4 - 80 - 4, scale, scale, 0,
							   toPrint );
	}
	clipperPop();
}

void ugcSeriesEditor_IconListSelectCB( UIList* pList, UGCProjectSeriesGroup* pGroup )
{
	ResourceSearchResultRow* row = ui_ListGetSelectedObject( pList );
	NOCONST(UGCProjectSeries)* pSeries = CONTAINER_NOCONST( UGCProjectSeries, pGroup->pSeries ); 
	if( row && pSeries ) {
		NOCONST(UGCProjectSeriesVersion)* pVersion = pSeries->eaVersions[ 0 ];

		StructCopyString( &pVersion->strImage, row->pcName );
		ugcSeriesEditor_WidgetCB( UI_WIDGET( pList ), pGroup );
	}
}

void ugcSeriesEditor_ProjectListDrawCB( UIList* pList, UIListColumn* pColumn, UI_MY_ARGS, F32 z, CBox* pLogicalBox, S32 iRow, UserData pDrawData )
{
	UGCProjectSeriesNode* node = eaGet( pList->peaModel, iRow );
	if( node ) {
		CBox box;
		BuildCBox( &box, x, y, w, h );
		ugcProjectChooser_ProjectDrawByID( node->iProjectID, UI_MY_VALUES, z, ui_ListIsSelected( pList, pColumn, iRow ), ui_ListIsHovering( pList, pColumn, iRow ));
	}
}

static void ugcSeriesEditor_AddProjects_OKClickedCB(UIWidget* ignored, UserData rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	const S32*const* peaSelectedProjects = ui_ListGetSelectedRows( pGroup->pAddProjectsList );
	NOCONST(UGCProjectSeries)* series = CONTAINER_NOCONST(UGCProjectSeries, pGroup->pSeries );
	
	{
		int it;
		for( it = 0; it != ea32Size( peaSelectedProjects ); ++it ) {
			PossibleUGCProject* proj = g_eaOwnedProjectNoSeriesRows[ (*peaSelectedProjects)[ it ]];
			if( proj && proj->iID ) {
				NOCONST(UGCProjectSeriesNode)* node = StructCreateNoConst( parse_UGCProjectSeriesNode );
				node->iProjectID = proj->iID;
				eaPush( &series->eaVersions[0]->eaChildNodes, node );
			}
		}
	}
	
	ugcSeriesGroup_CloseModalDialog( pGroup );
	pGroup->changedFn( pGroup, pGroup->changedData );
}

static void ugcSeriesEditor_ModalDialog_CancelClickedCB(UIButton *pButton, UserData rawGroup)
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	ugcSeriesGroup_CloseModalDialog( pGroup );
}

static void ugcSeriesEditor_SetRowText(UIList *pList, UIListColumn *pColumn, S32 iRow, UserData pDrawData, char **estrOutput)
{
	PossibleUGCProject **rows = (PossibleUGCProject**)(*pList->peaModel);
	PossibleUGCProject *row = rows && iRow < eaSize(&rows) ? rows[iRow] : NULL;

	if(row && row->pProjectInfo)
	{
		if( !nullStr( row->pProjectInfo->pcPublicName )) {
			estrPrintf( estrOutput, "%s", row->pProjectInfo->pcPublicName );
		} else {
			estrPrintf( estrOutput, "%s", TranslateMessageKey( "UGC.Unnamed_Project" ));
		}
	}
}

void ugcSeriesEditor_AddProjects( UIButton* ignored, UserData rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	UILabel* pLabel = NULL;
	UIButton* pButton = NULL;

	ugcSeriesGroup_CloseModalDialog( pGroup );

	pGroup->pModalDialog = ui_WindowCreate("", 0, 0, 325, 100);
	ui_WidgetSetTextMessage( UI_WIDGET( pGroup->pModalDialog ), "UGC_SeriesEditor.AddProjectTitle" );

	if( eaSize( &g_eaOwnedProjectNoSeriesRows ) > 0 )
	{
		F32 y = 0;
		
		pLabel = ui_LabelCreate("", 0, y);
		ui_WidgetSetTextMessage( UI_WIDGET( pLabel ), "UGC_SeriesEditor.AddProjectDetails" );
		ui_LabelResize( pLabel );
		ui_WindowAddChild(pGroup->pModalDialog, pLabel);
		y += UGC_ROW_HEIGHT;
		{
			UIList* list = ui_ListCreate( parse_PossibleUGCProject, &g_eaOwnedProjectNoSeriesRows, 40);
			ui_ListAppendColumn( list, ui_ListColumnCreateText( "Project", ugcSeriesEditor_SetRowText, NULL ));
			ui_ListSetMultiselect( list, true );
			list->bToggleSelect = true;
			list->eaColumns[0]->bShowCheckBox = true;
			list->fHeaderHeight = 0;
			
			ui_WidgetSetPosition( UI_WIDGET( list ), 0, y );
			ui_WidgetSetDimensionsEx( UI_WIDGET( list ), 1, 200, UIUnitPercentage, UIUnitFixed );
			ui_WindowAddChild( pGroup->pModalDialog, list );
			y += 200;

			
			pGroup->pAddProjectsList = list;
		}

		pButton = ui_ButtonCreate("", 0, 0, ugcSeriesEditor_AddProjects_OKClickedCB, pGroup );
		ui_WidgetSetTextMessage(UI_WIDGET( pButton ), "UGC.Ok" );
		ui_WidgetSetWidth(UI_WIDGET(pButton), 80);
		ui_WidgetSetPositionEx(UI_WIDGET(pButton), 0, 0, 0, 0, UIBottomRight);
		ui_WindowAddChild(pGroup->pModalDialog, pButton);
		
		pButton = ui_ButtonCreate("", 0, 0, ugcSeriesEditor_ModalDialog_CancelClickedCB, pGroup);
		ui_WidgetSetTextMessage(UI_WIDGET( pButton ), "UGC.Cancel" );
		ui_WidgetSetWidth(UI_WIDGET(pButton), 80);
		ui_WidgetSetPositionEx(UI_WIDGET(pButton), 80, 0, 0, 0, UIBottomRight);
		ui_WindowAddChild(pGroup->pModalDialog, pButton);

		ui_WidgetSetHeight( UI_WIDGET( pGroup->pModalDialog ), y + 45 );
	}
	else
	{
		pLabel = ui_LabelCreate("", 0, 0);
		ui_WidgetSetTextMessage( UI_WIDGET( pLabel ), "UGC_SeriesEditor.AddProjectDetails_NoProjects" );
		ui_LabelSetWordWrap(pLabel, 1);
		ui_WidgetSetWidth(UI_WIDGET(pLabel), 315);
		ui_WindowAddChild(pGroup->pModalDialog, pLabel);

		pButton = ui_ButtonCreate("OK", 0, 0, ugcSeriesEditor_ModalDialog_CancelClickedCB, pGroup);
		ui_WidgetSetDimensions(UI_WIDGET(pButton), 80, 30);
		ui_WidgetSetPositionEx(UI_WIDGET(pButton), 0, 0, 0, 0, UIBottomRight);
		ui_WindowAddChild(pGroup->pModalDialog, pButton);

		ui_WidgetSetHeight( UI_WIDGET( pGroup->pModalDialog ), 120 );
	}

	ui_WindowSetModal(pGroup->pModalDialog, true);
	ui_WindowSetResizable(pGroup->pModalDialog, false);
	elUICenterWindow(pGroup->pModalDialog);
	ui_WindowShowEx(pGroup->pModalDialog, true);
}

void ugcSeriesEditor_DeleteNode( UIButton* ignored, UserData rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	int index = ugcSeriesEditor_GetSelectedSeriesNodeIndex( pGroup );
	NOCONST(UGCProjectSeriesVersion)* pVersion = CONTAINER_NOCONST( UGCProjectSeriesVersion, pGroup->pSeries->eaVersions[ 0 ]);
	NOCONST(UGCProjectSeriesNode)* selectedNode = eaGet( &pVersion->eaChildNodes, index );

	if( selectedNode ) {
		eaRemove( &pVersion->eaChildNodes, index );
		StructDestroyNoConstSafe( parse_UGCProjectSeriesNode, &selectedNode );
		pGroup->changedFn( pGroup, pGroup->changedData );
	}
}

void ugcSeriesEditor_MoveNodeUpCB( UIButton* ignored, UserData rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	int index = ugcSeriesEditor_GetSelectedSeriesNodeIndex( pGroup );
	NOCONST(UGCProjectSeriesVersion)* pVersion = CONTAINER_NOCONST( UGCProjectSeriesVersion, pGroup->pSeries->eaVersions[ 0 ]);
	NOCONST(UGCProjectSeriesNode)* selectedNode = eaGet( &pVersion->eaChildNodes, index );

	eaSwap( &pVersion->eaChildNodes, index, index - 1 );
	if( pGroup->projectsList ) {
		ui_ListSetSelectedRow( pGroup->projectsList, index - 1 );
	}
	pGroup->changedFn( pGroup, pGroup->changedData );
}

void ugcSeriesEditor_MoveNodeDownCB( UIButton* ignored, UserData rawGroup )
{
	UGCProjectSeriesGroup* pGroup = rawGroup;
	int index = ugcSeriesEditor_GetSelectedSeriesNodeIndex( pGroup );
	NOCONST(UGCProjectSeriesVersion)* pVersion = CONTAINER_NOCONST( UGCProjectSeriesVersion, pGroup->pSeries->eaVersions[ 0 ]);
	NOCONST(UGCProjectSeriesNode)* selectedNode = eaGet( &pVersion->eaChildNodes, index );

	eaSwap( &pVersion->eaChildNodes, index, index + 1 );
	if( pGroup->projectsList ) {
		ui_ListSetSelectedRow( pGroup->projectsList, index + 1 );
	}
	pGroup->changedFn( pGroup, pGroup->changedData );
}

void ugcSeriesGroup_CloseModalDialog( UGCProjectSeriesGroup* pGroup )
{
	if(pGroup && pGroup->pModalDialog)
	{
		ui_WindowClose(pGroup->pModalDialog);
		ui_WidgetQueueFreeAndNull(&pGroup->pModalDialog);
	}
}

static void ugcSeriesEditor_NewSeries_Timeout( TimedCallback* callback, F32 timeSinceLastCallback, UserData userData )
{
	ugcSeriesEditor_CloseModalDialog();
	ugcModalDialog( "Error", "Error creating series:\nCreate timed out", UIOk );
	g_UGCSeriesDoc.isNewSeriesQueued = false;
	ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_CHOOSE_PROJECT );
}

void ugcSeriesEditor_EditNewSeries( const UGCProjectSeries* newSeries )
{
	UILabel* label;
	Packet* pak = pktCreate( gpLoginLink, TOLOGIN_UGCPROJECTSERIES_CREATE );
	ParserSendStruct( parse_UGCProjectSeries, pak, newSeries );
	pktSend( &pak );

	StructDestroySafe( parse_UGCProjectSeries, &g_UGCSeriesDoc.pSeries );
	StructDestroySafe( parse_UGCProjectSeries, &g_UGCSeriesDoc.pLastSaveSeries );

	g_UGCSeriesDoc.isCloseQueued = false;
	g_UGCSeriesDoc.isNewSeriesQueued = true;
	ugcSeriesEditor_CloseModalDialog();	

	g_UGCSeriesDoc.pModalDialog = ui_WindowCreate( "", 0, 0, 200, 50);
	ui_WidgetSetTextMessage( UI_WIDGET( g_UGCSeriesDoc.pModalDialog ), "UGC_SeriesEditor.LoadingTitle" );
	label = ui_LabelCreate( "", 0, 0 );
	ui_WidgetSetTextMessage( UI_WIDGET( label ), "UGC_SeriesEditor.LoadingDetails" );
	ui_LabelResize( label );
	ui_WindowAddChild( g_UGCSeriesDoc.pModalDialog, label );
	ui_WindowSetClosable( g_UGCSeriesDoc.pModalDialog, false );
	ui_WindowSetModal( g_UGCSeriesDoc.pModalDialog, true );
	elUICenterWindow( g_UGCSeriesDoc.pModalDialog );
	ui_WindowSetModal( g_UGCSeriesDoc.pModalDialog, true );
	ui_WindowShowEx( g_UGCSeriesDoc.pModalDialog, true );
	
	g_UGCSeriesDoc.pModalTimeout = TimedCallback_Add( ugcSeriesEditor_NewSeries_Timeout, NULL, UGC_SAVE_STATUS_TIMEOUT );
}

void ugcSeriesEditor_EditSeries( const UGCProjectSeries* series )
{
	StructDestroySafe( parse_UGCProjectSeries, &g_UGCSeriesDoc.pSeries );
	StructDestroySafe( parse_UGCProjectSeries, &g_UGCSeriesDoc.pLastSaveSeries );
	g_UGCSeriesDoc.pSeries = StructClone( parse_UGCProjectSeries, series );
	g_UGCSeriesDoc.pLastSaveSeries = StructClone( parse_UGCProjectSeries, series );

	// The minimal amount of dict loading to get the item picker to work.
	ugcResourceInfoPopulateDictionary();
	ugcLoadDictionaries();

	g_UGCSeriesDoc.isCloseQueued = false;
}

static bool ugcSeriesEditor_ReturnCBCB( UIDialog* ignored, UIDialogButton eButton, UserData ignored2 )
{
	if(UGC_SERIES_EDITOR_CLOSE_CANCEL != eButton)
	{
		if(UGC_SERIES_EDITOR_SAVE & eButton)
		{
			eButton &= ~UGC_SERIES_EDITOR_SAVE;

			g_UGCSeriesDoc.isCloseQueued = !!eButton;

			ugcSeriesEditor_Save(NULL, NULL);
		}
		else if(eButton)
			ugcProjectChooser_SetMode(UGC_PROJECT_CHOOSER_MODE_CHOOSE_PROJECT);
	}

	return true;
}

void ugcSeriesEditor_ReturnCB( UIButton* ignored, UserData ignored2 )
{
	if( StructCompare( parse_UGCProjectSeries, g_UGCSeriesDoc.pSeries, g_UGCSeriesDoc.pLastSaveSeries, 0, 0, 0 ) != 0 ) {
		UIDialog *pDialog;
		pDialog = ui_DialogCreateEx( TranslateMessageKey( "UGC_Editor.UnsavedChangesTitle" ),
									 TranslateMessageKey( "UGC_Editor.UnsavedChangesDetails" ),
									 ugcSeriesEditor_ReturnCBCB, NULL, NULL,
									 TranslateMessageKey( "UGC.Cancel" ), UGC_SERIES_EDITOR_CLOSE_CANCEL,
									 TranslateMessageKey( "UGC_Editor.UnsavedChanges_DontSaveAndQuit" ), UGC_SERIES_EDITOR_CLOSE, 
									 TranslateMessageKey( "UGC_Editor.UnsavedChanges_SaveAndQuit" ), UGC_SERIES_EDITOR_SAVE | UGC_SERIES_EDITOR_CLOSE,
									 NULL );
		ui_WindowSetModal( UI_WINDOW( pDialog ), true );
		ui_WindowShowEx( UI_WINDOW( pDialog ), true );
		UI_WIDGET( pDialog )->height += 20;
	} else {
		ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_CHOOSE_PROJECT );
	}
}

void ugcSeriesEditor_Changed( UGCProjectSeriesGroup* pGroup, UserData ignored )
{
	StructReset( parse_UGCRuntimeStatus, &g_UGCSeriesDoc.runtimeStatus );
	ugcSetStageAndAdd( &g_UGCSeriesDoc.runtimeStatus, "UGC Series Validate" );
	if( g_UGCSeriesDoc.pSeries ) {
		ugcValidateSeries( g_UGCSeriesDoc.pSeries );
	}
	ugcClearStage();

	// All ERRORS SHOULD BE GENERATED NOW
	{
		int it;

		eaSetSize( &g_UGCSeriesDoc.eaRuntimeErrors, 0 );
		for( it = 0; it != eaSize( &g_UGCSeriesDoc.runtimeStatus.stages ); ++it ) {
			eaPushEArray( &g_UGCSeriesDoc.eaRuntimeErrors, &g_UGCSeriesDoc.runtimeStatus.stages[ it ]->errors );
		}
	}

	ugcSeriesGroup_Refresh(
			g_UGCSeriesDoc.pDetails, g_UGCSeriesDoc.pSeries,
			StructCompare( parse_UGCProjectSeries, g_UGCSeriesDoc.pSeries, g_UGCSeriesDoc.pLastSaveSeries, 0, 0, 0 ) != 0,
			&g_UGCSeriesDoc.runtimeStatus );
}

static void ugcSeriesEditor_Save_Timeout( TimedCallback* callback, F32 timeSinceLastCallback, UserData userData )
{
	ugcSeriesEditor_ProjectSeriesUpdate_Result( false, TranslateMessageKey( "UGC_Editor.SaveError_SaveTimedOut" ));
}

void ugcSeriesEditor_Save( UIButton* ignored, UserData ignored2 )
{
	UGCProjectSeries* series = g_UGCSeriesDoc.pSeries;
	UILabel* label;

	if( series ) {
		Packet* pak = pktCreate( gpLoginLink, TOLOGIN_UGCPROJECTSERIES_UPDATE );
		ParserSendStruct( parse_UGCProjectSeries, pak, series );
		pktSend( &pak );
	}
	g_UGCSeriesDoc.isSaving = true;

	// create the modal dialog
	ugcSeriesEditor_CloseModalDialog();	
	g_UGCSeriesDoc.pModalDialog = ui_WindowCreate( "", 0, 0, 200, 50);
	ui_WidgetSetTextMessage( UI_WIDGET( g_UGCSeriesDoc.pModalDialog ), "UGC_SeriesEditor.SavingTitle" );
	label = ui_LabelCreate("Saving series...", 0, 0 );
	ui_WidgetSetTextMessage( UI_WIDGET( label ), "UGC_SeriesEditor.SavingDetails" );
	ui_WindowAddChild( g_UGCSeriesDoc.pModalDialog, label );
	ui_WindowSetClosable( g_UGCSeriesDoc.pModalDialog, false );
	ui_WindowSetModal( g_UGCSeriesDoc.pModalDialog, true );
	elUICenterWindow( g_UGCSeriesDoc.pModalDialog );
	ui_WindowSetModal( g_UGCSeriesDoc.pModalDialog, true );

	g_UGCSeriesDoc.pModalTimeout = TimedCallback_Add( ugcSeriesEditor_Save_Timeout, NULL, UGC_SAVE_STATUS_TIMEOUT );
}

void ugcSeriesEditor_Show( void )
{
	if( !g_UGCSeriesDoc.pMainPane ) {
		g_UGCSeriesDoc.pMainPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	}
	ui_WidgetAddToDevice( UI_WIDGET( g_UGCSeriesDoc.pMainPane ), NULL );
	
	if( !g_UGCSeriesDoc.pDetails ) {
		g_UGCSeriesDoc.pDetails = ugcSeriesGroup_Create( "UGCSeriesEditor", g_UGCSeriesDoc.pMainPane, ugcSeriesEditor_Changed, NULL );
	}
	
	ugcSeriesEditor_Changed( g_UGCSeriesDoc.pDetails, NULL );
	SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCProjectChooser", g_ui_State.hActiveSkin );
	g_ui_State.minScreenWidth = 1024;
	g_ui_State.minScreenHeight = 720;
	ui_SetGlobalValuesFromActiveSkin();
}

void ugcSeriesEditor_Hide( void )
{
	if( g_UGCSeriesDoc.pMainPane ) {
		ui_WidgetRemoveFromGroup( UI_WIDGET( g_UGCSeriesDoc.pMainPane ));
	}
	REMOVE_HANDLE( g_ui_State.hActiveSkin );
	g_ui_State.minScreenWidth = 0;
	g_ui_State.minScreenHeight = 0;
	ui_SetGlobalValuesFromActiveSkin();
}

void ugcSeriesEditor_ProjectSeriesCreate_Result( ContainerID newSeriesID )
{
	if( !g_UGCSeriesDoc.isNewSeriesQueued ) {
		return;
	}
	g_UGCSeriesDoc.newSeriesID = newSeriesID;
}

void ugcSeriesEditor_RecieveSeriesList( UGCProjectSeries** eaProjectSeries )
{
	int it;
	
	if( !g_UGCSeriesDoc.isNewSeriesQueued || !g_UGCSeriesDoc.newSeriesID ) {
		return;
	}

	for( it = 0; it != eaSize( &eaProjectSeries ); ++it ) {
		if( eaProjectSeries[ it ]->id == g_UGCSeriesDoc.newSeriesID ) {
			ugcSeriesEditor_CloseModalDialog();
			g_UGCSeriesDoc.isNewSeriesQueued = false;
			
			ugcSeriesEditor_EditSeries( eaProjectSeries[ it ]);
			ugcSeriesEditor_Changed( g_UGCSeriesDoc.pDetails, NULL );
		}
	}
}

void ugcSeriesEditor_ProjectSeriesUpdate_Result( bool success, const char* errorMsg )
{
	char* estr = NULL;
	if( !g_UGCSeriesDoc.isSaving ) {
		return;
	}
	
	ugcSeriesEditor_CloseModalDialog();

	if( !success ) {
		ugcFormatMessageKey( &estr, "UGC_Editor.SaveError",
							 STRFMT_STRING( "Error", errorMsg ),
							 STRFMT_END );
		ugcModalDialog( TranslateMessageKey( "UGC.Error" ), estr, UIOk );
	} else {
		StructCopyAll( parse_UGCProjectSeries, g_UGCSeriesDoc.pSeries, g_UGCSeriesDoc.pLastSaveSeries );
		if( g_UGCSeriesDoc.isCloseQueued ) {
			g_UGCSeriesDoc.isCloseQueued = false;
			ugcProjectChooser_SetMode( UGC_PROJECT_CHOOSER_MODE_CHOOSE_PROJECT );
		} else {
			ugcSeriesEditor_Changed( g_UGCSeriesDoc.pDetails, NULL );
		}
	}

	g_UGCSeriesDoc.isSaving = false;

	estrDestroy( &estr );
}

void ugcSeriesEditor_CloseModalDialog( void )
{
	if(g_UGCSeriesDoc.pModalDialog)
	{
		ui_WindowClose(g_UGCSeriesDoc.pModalDialog);
		ui_WidgetQueueFreeAndNull(&g_UGCSeriesDoc.pModalDialog);
	}

	if( g_UGCSeriesDoc.pModalTimeout ) {
		TimedCallback_Remove( g_UGCSeriesDoc.pModalTimeout );
	}
	g_UGCSeriesDoc.pModalTimeout = NULL;

	ugcSeriesGroup_CloseModalDialog( g_UGCSeriesDoc.pDetails );
}

#define UGC_SORT_POSS_PROJECTS_HANDLE_NULL(p1, p2)	\
								if(!p1 && !p2)		\
									return 0;		\
								else if(!p2)		\
									return -1;		\
								else if(!p1)		\
									return 1;

static int UGCProject_SortPossibleProjectsByName(const PossibleUGCProject **project1, const PossibleUGCProject **project2)
{
	const UGCProjectInfo* projectInfo1 = SAFE_MEMBER( *project1, pProjectInfo );
	const UGCProjectInfo* projectInfo2 = SAFE_MEMBER( *project2, pProjectInfo );
	UGC_SORT_POSS_PROJECTS_HANDLE_NULL(projectInfo1, projectInfo2);
	return stricmp_safe(projectInfo1->pcPublicName, projectInfo2->pcPublicName);
}

void ugcSeriesEditor_SetPossibleProjects( PossibleUGCProjects* pProjects )
{
	eaClear(&g_eaOwnedProjectNoSeriesRows);
	eaPushEArray(&g_eaOwnedProjectNoSeriesRows, &pProjects->ppProjects);
	{
		int it;
		for( it = eaSize( &g_eaOwnedProjectNoSeriesRows ) - 1; it >= 0; --it ) {
			PossibleUGCProject* proj = g_eaOwnedProjectNoSeriesRows[ it ];
			if( proj->iID == 0 || proj->iSeriesID ) {
				eaRemove( &g_eaOwnedProjectNoSeriesRows, it );
			}
		}
	}

	eaQSort(g_eaOwnedProjectNoSeriesRows, UGCProject_SortPossibleProjectsByName);

	ugcSeriesEditor_RecieveSeriesList( pProjects->eaProjectSeries );
}

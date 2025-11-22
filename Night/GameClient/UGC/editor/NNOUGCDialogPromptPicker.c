#include "NNOUGCDialogPromptPicker.h"

#include "Color.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCommon.h"
#include "NNOUGCDialogTreeEditor.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "UIButton.h"
#include "UICore.h"
#include "UIList.h"
#include "UIPane.h"
#include "UITreechart.h"
#include "UIWindow.h"

typedef struct UGCDialogTreeDoc UGCDialogTreeDoc;
typedef struct UGCComponent UGCComponent;

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

/// The DialogPrompt picker.  Sits in a UIPane, so that it can get
/// embedded into arbitrary UI.
typedef struct UGCDialogPromptPicker
{
	UIPane* rootPane;
	UIList* list;

	// Use the DialogTree doc to get access to the treechart
	UGCDialogTreeDoc* dialogDoc;

	UGCComponent** eaAvailableDialogs;
	U32 selectedDialogID;
	int selectedPromptID;
} UGCDialogPromptPicker;

/// A window containing just a UGCDialogPromptPicker, and some basic
/// buttons.
typedef struct UGCDialogPromptPickerWindow
{
	UGCDialogPromptPicker* picker;
	UIWindow* window;
	
	UGCDialogPromptFn fn;
	UserData userData;
} UGCDialogPromptPickerWindow;

static void ugcDialogPromptPickerWindowOK( UIButton* ignored, UserData rawPickerWindow );
static void ugcDialogPromptPickerWindowCancel( UIButton* ignored, UserData rawPickerWindow );
static bool ugcDialogPromptPickerWindowClosed( UIWindow* ignored, UserData rawPickerWindow );
static UGCDialogPromptPicker* ugcDialogPromptPickerCreate( UGCComponent** availableDialogs );
static void ugcDialogPromptPickerDestroy( UGCDialogPromptPicker* picker );
static void ugcDialogPromptPickerSelectionChanged( UGCDialogPromptPicker* picker );

/// This variable exists ONLY to make debugging easier -- it stores
/// the last created picker window.
static UGCDialogPromptPickerWindow* debugLastPickerWindow = NULL;

static int ugcDialogPromptPickerWindowsOpen = 0;

void ugcShowDialogPromptPicker( UGCDialogPromptFn fn, UserData userData, const char* mapName )
{
	UGCDialogPromptPickerWindow* pickerWindow;
	UIButton* okButton;
	UIButton* cancelButton;
	
	pickerWindow = calloc( 1, sizeof( *pickerWindow ));
	pickerWindow->window = ui_WindowCreate( "Select Dialog", 0, 0, 900, 700 );
	ui_WindowSetCloseCallback( pickerWindow->window, ugcDialogPromptPickerWindowClosed, pickerWindow );
	
	{
		UGCComponent*const*const components = ugcEditorGetComponentList()->eaComponents;
		UGCComponent** availableDialogs = NULL;
		int it;
		for( it = 0; it != eaSize( &components ); ++it ) {
			UGCComponent* component = components[ it ];
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				if( ugcComponentIsOnMap( component, mapName, false )) {
					eaPush( &availableDialogs, component );
				} else {
					UGCComponent* contact = ugcEditorFindComponentByID( component->uActorID );
					if( contact && ugcComponentIsOnMap( contact, mapName, false )) {
						eaPush( &availableDialogs, component );
					}
				}
			}
		}

		if( eaSize( &availableDialogs )) {
			pickerWindow->picker = ugcDialogPromptPickerCreate( availableDialogs );
			ui_WindowAddChild( pickerWindow->window, pickerWindow->picker->rootPane );
		} else {
			ugcModalDialogMsg( "UGC_Editor.NoDialogs", "UGC_Editor.NoDialogsDetails", UIOk );
			ui_WidgetQueueFreeAndNull( &pickerWindow->window );
			free( pickerWindow );
			return;
		}
	}
	
	pickerWindow->fn = fn;
	pickerWindow->userData = userData;

	okButton = ui_ButtonCreate( "OK", 0, 0, ugcDialogPromptPickerWindowOK, pickerWindow );
	ui_WidgetSetPositionEx( UI_WIDGET( okButton ), 0, 0, 0, 0, UIBottomRight );
	ui_WidgetSetWidth( UI_WIDGET( okButton ), 80 );
	ui_WindowAddChild( pickerWindow->window, okButton );

	cancelButton = ui_ButtonCreate( "Cancel", 0, 0, ugcDialogPromptPickerWindowCancel, pickerWindow );
	ui_WidgetSetPositionEx( UI_WIDGET( cancelButton ), 80, 0, 0, 0, UIBottomRight );
	ui_WidgetSetWidth( UI_WIDGET( cancelButton ), 80 );
	ui_WindowAddChild( pickerWindow->window, cancelButton );
	pickerWindow->picker->rootPane->widget.bottomPad = UI_WIDGET( cancelButton )->height + 5;

	ui_WindowSetDimensions( pickerWindow->window, 900, 700, 450, 350 );
	elUICenterWindow( pickerWindow->window );
	ui_WindowSetModal( pickerWindow->window, true );
	ui_WindowShowEx( pickerWindow->window, true );

	debugLastPickerWindow = pickerWindow;

	++ugcDialogPromptPickerWindowsOpen;
}

void ugcDialogPromptPickerWindowOK( UIButton* ignored, UserData rawPickerWindow )
{
	UGCDialogPromptPickerWindow* pickerWindow = rawPickerWindow;

	if( pickerWindow->fn ) {
		pickerWindow->fn( pickerWindow->picker->selectedDialogID, pickerWindow->picker->selectedPromptID, pickerWindow->userData );
	}
	ui_WindowClose( pickerWindow->window );
	--ugcDialogPromptPickerWindowsOpen;
	assert( ugcDialogPromptPickerWindowsOpen >= 0 );
}

void ugcDialogPromptPickerWindowCancel( UIButton* ignored, UserData rawPickerWindow )
{
	UGCDialogPromptPickerWindow* pickerWindow = rawPickerWindow;
	ui_WindowClose( pickerWindow->window );
	--ugcDialogPromptPickerWindowsOpen;
	assert( ugcDialogPromptPickerWindowsOpen >= 0 );
}

bool ugcDialogPromptPickerWindowClosed( UIWindow* ignored, UserData rawPickerWindow )
{
	UGCDialogPromptPickerWindow* pickerWindow = rawPickerWindow;

	ugcDialogPromptPickerDestroy( pickerWindow->picker );
	ui_WidgetQueueFreeAndNull( &pickerWindow->window );
	free( pickerWindow );

	if( pickerWindow == debugLastPickerWindow ) {
		debugLastPickerWindow = NULL;
	}

	return true;
}

static void ugcDialogPromptPickerListSelectionCB( UIList* list, UserData rawPicker )
{
	UGCDialogPromptPicker* picker = rawPicker;
	UGCComponent* component = ui_ListGetSelectedObject( list );

	if( component ) {
		ugcDialogTreeDocSetDialogTreeForPicker( picker->dialogDoc, component->uID );
		picker->selectedDialogID = component->uID;
	}
}

UGCDialogPromptPicker* ugcDialogPromptPickerCreate( UGCComponent** availableDialogs )
{
	UGCDialogPromptPicker* picker = calloc( 1, sizeof( *picker ));
	assert( eaSize( &availableDialogs ) > 0 );

	picker->rootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	picker->rootPane->invisible = true;

	eaCopy( &picker->eaAvailableDialogs, &availableDialogs );
	picker->list = ui_ListCreate( parse_UGCComponent, &picker->eaAvailableDialogs, 40 );
	ui_ListSetSelectedCallback( picker->list, ugcDialogPromptPickerListSelectionCB, picker );
	ui_ListAppendColumn( picker->list, ui_ListColumnCreateParseName( "Dialog", "PromptBody", NULL ));
	picker->list->fHeaderHeight = 0;
	ui_WidgetSetDimensionsEx( UI_WIDGET( picker->list ), 300, 1, UIUnitFixed, UIUnitPercentage );
	ui_PaneAddChild( picker->rootPane, picker->list );

	picker->dialogDoc = ugcDialogTreeDocCreateForPicker( picker->eaAvailableDialogs[ 0 ]->uID, ugcDialogPromptPickerSelectionChanged, picker );
	if( picker->dialogDoc ) {
		UITreechart* docTreechart = ugcDialogTreeDocTreechartForPicker( picker->dialogDoc );
		ui_WidgetRemoveFromGroup( UI_WIDGET( docTreechart ));
		ui_WidgetSetPosition( UI_WIDGET( docTreechart ), 300, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( docTreechart ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( docTreechart ), 0, 0, 0, 0 );
		ui_PaneAddChild( picker->rootPane, docTreechart );
	}

	ui_ListSetSelectedRowAndCallback( picker->list, 0 );
	ugcDialogPromptPickerSelectionChanged( picker );

	if( !picker->dialogDoc ) {
		ugcDialogPromptPickerDestroy( picker );
		return NULL;
	}
	
	return picker;
}

void ugcDialogPromptPickerDestroy( UGCDialogPromptPicker* picker )
{
	static void** emptyModel = NULL;
	ui_ListSetModel( picker->list, NULL, &emptyModel );
	ui_WidgetQueueFreeAndNull( &picker->rootPane );
	eaDestroy( &picker->eaAvailableDialogs );
	ugcDialogTreeDocDestroy( picker->dialogDoc );
	free( picker );
}

void ugcDialogPromptPickerSelectionChanged( UGCDialogPromptPicker* picker )
{
	picker->selectedPromptID = ugcDialogTreeDocGetSelectedPromptForPicker( picker->dialogDoc );

	// if there's other UI to do, it goes here
}

bool ugcDialogPromptPickerWindowOpen( void )
{
	return ugcDialogPromptPickerWindowsOpen > 0;
}

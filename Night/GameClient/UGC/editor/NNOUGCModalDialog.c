#include "NNOUGCModalDialog.h"

#include "Color.h"
#include "EditLibUIUtil.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIModalDialog.h"
#include "UIWindow.h"

static UIWindow* ugcModalDialogCreateUI( bool isMessage, const char* title, const char* body, UIDialogButtons buttons );
static bool ugcModalDialogCancelCB( UIWindow* ignored, UserData ignored2 );
static void ugcModalDialogButtonClickCB( UIWindow* ignored, UserData rawDialogButton );

static UIDialogButtons g_returnValue;

UIDialogButtons ugcModalDialog( const char* title, const char* body, UIDialogButtons buttons )
{
	UIGlobalState oldState = { 0 };
	UIWindow* dialog;
	
	ui_ModalDialogBeforeWidgetAdd( &oldState );

	dialog = ugcModalDialogCreateUI( false, title, body, buttons );
	elUICenterWindow( dialog );
	ui_WindowShow( dialog );

	ui_ModalDialogLoop();

	ui_WindowFreeInternal( dialog );

	ui_ModalDialogAfterWidgetDestroy( &oldState );

	return g_returnValue;
}

UIDialogButtons ugcModalDialogMsg( const char* title, const char* body, UIDialogButtons buttons )
{
	UIGlobalState oldState = { 0 };
	UIWindow* dialog;
	
	ui_ModalDialogBeforeWidgetAdd( &oldState );

	dialog = ugcModalDialogCreateUI( true, title, body, buttons );
	elUICenterWindow( dialog );
	ui_WindowShow( dialog );

	ui_ModalDialogLoop();

	ui_WindowFreeInternal( dialog );

	ui_ModalDialogAfterWidgetDestroy( &oldState );

	return g_returnValue;
}

UIWindow* ugcModalDialogCreateUI( bool isMessage, const char* title, const char* body, UIDialogButtons buttons )
{
	UIWindow* window;
	UILabel* label;
	UIButton* button;

	window = ui_WindowCreate( "", 0, 0, 400, 100 );
	if( isMessage ) {
		ui_WidgetSetTextMessage( UI_WIDGET( window ), title );
	} else {
		ui_WidgetSetTextString( UI_WIDGET( window ), title );
	}
	ui_WindowSetResizable( window, false );
	ui_WindowSetCloseCallback( window, ugcModalDialogCancelCB, NULL );

	label = ui_LabelCreate( "", 0, 0 );
	if( isMessage ) {
		ui_WidgetSetTextMessage( UI_WIDGET( label ), body );
	} else {
		ui_WidgetSetTextString( UI_WIDGET( label ), body );
	}
	ui_WidgetSetPosition( UI_WIDGET( label ), 0, 0 );
	ui_LabelSetWordWrap( label, true );
	ui_LabelUpdateDimensionsForWidth( label, 380 );
	ui_WidgetAddChild( UI_WIDGET( window ), UI_WIDGET( label ));

	MAX1( UI_WIDGET( window )->height, UI_WIDGET( label )->height + 50 );

	// Make buttons
	{
		float x = 0;
		
		struct {
			UIDialogButtons flag;
			const char* messageKey;
		} list[] = {
			{ UIOk, "UGC.Ok" },
			{ UIYes, "UGC.Yes" },
			{ UINo, "UGC.No" },
			{ UICancel, "UGC.Cancel" },
		};
		int it;

		for( it = 0; it != ARRAY_SIZE( list ); ++it ) {
			if( buttons & list[ it ].flag ) {
				button = ui_ButtonCreate( "", 0, 0, ugcModalDialogButtonClickCB, (UserData)list[ it ].flag );
				ui_ButtonSetMessage( button, list[ it ].messageKey );
				ui_WidgetSetDimensions( UI_WIDGET( button ), 80, 22 );
				ui_WidgetSetPositionEx( UI_WIDGET( button ), x, 0, 0, 0, UIBottomRight );
				ui_WidgetAddChild( UI_WIDGET( window ), UI_WIDGET( button ));
				x = ui_WidgetGetNextX( UI_WIDGET( button ));
			}
		}
	}

	return window;
}

bool ugcModalDialogCancelCB( UIWindow* ignored, UserData ignored2 )
{
	ui_ModalDialogLoopExit();
	g_returnValue = UICancel;

	return true;
}

void ugcModalDialogButtonClickCB( UIWindow* ignored, UserData rawDialogButton )
{
	ui_ModalDialogLoopExit();
	g_returnValue = (UIDialogButtons)rawDialogButton;
}

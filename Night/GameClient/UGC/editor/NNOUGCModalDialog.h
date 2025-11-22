//// UGC-editor specific version of ui_ModalDialog.
////
//// Lays out the buttons like you would expect.

typedef enum UIDialogButtons UIDialogButtons;

UIDialogButtons ugcModalDialog( const char* title, const char* body, UIDialogButtons buttons );
UIDialogButtons ugcModalDialogMsg( const char* title, const char* body, UIDialogButtons buttons );

//// A picker to choose a specific prompt on a specific dialog.
#pragma once

typedef void (*UGCDialogPromptFn)( U32 dialogID, int promptID, UserData userData );

void ugcShowDialogPromptPicker( UGCDialogPromptFn fn, UserData userData, const char* mapName );

bool ugcDialogPromptPickerWindowOpen( void );

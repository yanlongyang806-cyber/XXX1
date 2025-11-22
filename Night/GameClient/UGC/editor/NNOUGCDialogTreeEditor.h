//// The Dialog Tree editor
#pragma once
GCC_SYSTEM

typedef enum UGCActionID UGCActionID;
typedef struct UGCComponent UGCComponent;
typedef struct UGCDialogTreeBlock UGCDialogTreeBlock;
typedef struct UGCDialogTreeDoc UGCDialogTreeDoc;
typedef struct UGCNoDialogTreesDoc UGCNoDialogTreesDoc;
typedef struct UGCDialogTreePrompt UGCDialogTreePrompt;
typedef struct UITreechart UITreechart;

typedef void (*UGCDialogTreeDocSelectionFn)( UserData data );

SA_RET_OP_VALID UGCDialogTreeDoc* ugcDialogTreeDocCreate( U32 componentID );
void ugcDialogTreeDocDestroy( SA_PRE_NN_VALID SA_POST_NN_FREE UGCDialogTreeDoc* doc );

SA_ORET_OP_VALID UGCDialogTreeBlock* ugcDialogTreeDocGetBlock( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, SA_PRE_VALID SA_POST_NN_OP_VALID UGCComponent** out_component );
SA_ORET_OP_VALID UGCDialogTreePrompt* ugcDialogTreeDocGetPrompt( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, int promptID );
void ugcDialogTreeDocRefresh( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );
void ugcDialogTreeDocSetVisible( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );
void ugcDialogTreeDocSetSelectedPromptAndAction( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, int promptID, int actionIndex );

void* ugcDialogTreeIDAsPtrFromIDIndex( U32 componentID );
void* ugcDialogTreeDocGetIDAsPtr( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );
void ugcDialogTreeDocOncePerFrame( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, bool isActive );
void ugcDialogTreeDocHandleAction( SA_PARAM_NN_VALID UGCDialogTreeDoc *doc, UGCActionID action );
bool ugcDialogTreeDocQueryAction( SA_PARAM_NN_VALID UGCDialogTreeDoc *doc, UGCActionID action, SA_PARAM_NN_VALID char** out_estr );

// Crazy internal functions for using the logic of the DialogTreeDoc for pickers.
SA_RET_OP_VALID UGCDialogTreeDoc* ugcDialogTreeDocCreateForPicker( U32 componentID, UGCDialogTreeDocSelectionFn fn, UserData data );
SA_ORET_NN_VALID UITreechart* ugcDialogTreeDocTreechartForPicker( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );
void ugcDialogTreeDocSetDialogTreeForPicker( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc, U32 dialogTreeID );
int ugcDialogTreeDocGetSelectedPromptForPicker( SA_PARAM_NN_VALID UGCDialogTreeDoc* doc );

/// The NoDialogTrees doc -- displayed when you are on the tab, but there is no dialog tree to see
SA_RET_OP_VALID UGCNoDialogTreesDoc* ugcNoDialogTreesDocCreate( void );
void ugcNoDialogTreesDocDestroy( UGCNoDialogTreesDoc** ppDoc );
void ugcNoDialogTreesDocRefresh( SA_PARAM_NN_VALID UGCNoDialogTreesDoc* pDoc );
void ugcNoDialogTreesDocSetVisible( SA_PARAM_NN_VALID UGCNoDialogTreesDoc* pDoc );
void ugcNoDialogTreesDocOncePerFrame( SA_PARAM_NN_VALID UGCNoDialogTreesDoc* pDoc );
void ugcNoDialogTreesDocHandleAction( SA_PARAM_NN_VALID UGCNoDialogTreesDoc* pDoc, UGCActionID action );
bool ugcNoDialogTreesDocQueryAction( SA_PARAM_NN_VALID UGCNoDialogTreesDoc* pDoc, UGCActionID action, SA_PARAM_NN_VALID char** out_estr );

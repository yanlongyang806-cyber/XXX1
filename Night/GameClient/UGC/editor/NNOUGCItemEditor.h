#pragma once
GCC_SYSTEM

typedef struct UGCItem UGCItem;
typedef struct UIPane UIPane;
typedef enum UGCActionID UGCActionID;

typedef struct UGCItemEditorDoc
{
	UIPane *pRootPane;
	int numItemsLastRefresh;
} UGCItemEditorDoc;

UGCItemEditorDoc *ugcItemEditorCreate(void);
void ugcItemEditorDestroy(UGCItemEditorDoc **ppDoc);
void ugcItemEditorRefresh(UGCItemEditorDoc *pDoc);
void ugcItemEditorSetVisible(UGCItemEditorDoc *pDoc);
void ugcItemEditorOncePerFrame(UGCItemEditorDoc *pDoc);
bool ugcItemEditorQueryAction(UGCItemEditorDoc *pDoc, UGCActionID action, char** out_estr);
void ugcItemEditorHandleAction(UGCItemEditorDoc *pDoc, UGCActionID action);

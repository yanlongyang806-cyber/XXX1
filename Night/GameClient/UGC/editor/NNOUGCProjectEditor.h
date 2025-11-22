//// The Project Editor in UGC.
////
//// This has an overview of the UGCProject.  It's the first editor
//// seen by someone who just logged in.  It edits a UGCProjectInfo,
//// which is the editor-side view of the UGCProject container.
#pragma once

typedef struct UGCProjectEditorDoc UGCProjectEditorDoc;

/// Standard UGC lifetime function.
UGCProjectEditorDoc* ugcProjectEditor_Open( void );

/// Standard UGC lifetime function.
void ugcProjectEditor_Refresh( UGCProjectEditorDoc* pDoc );

/// Standard UGC lifetime function.
void ugcProjectEditor_SetVisible( UGCProjectEditorDoc* pDoc );

/// Standard UGC lifetime function.
void ugcProjectEditor_Close( UGCProjectEditorDoc** ppDoc );

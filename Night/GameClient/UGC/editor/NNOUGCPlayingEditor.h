//// The While-Playing editor.
//// (a.k.a. the 3D Editor, a.k.a. the 3D Nudger)
////
//// The While-Playing editor allows an author who is previewing a UGC
//// map to edit the position of Objects in the map 3D, and see
//// real-time updates to those objects.
////
//// It does this by taking advantage of the WorldInteractionEntry's
//// to hide/show objects and draw ghosts in their place.  This avoids
//// the overhead of switching a layer to Edit Mode and allows for a
//// much simpler editor than the Ctrl-E World Editor.
#pragma once

typedef enum UGCActionID UGCActionID;
typedef struct UGCComponent UGCComponent;
typedef struct UGCPlayComponentData UGCPlayComponentData;

// Called after each change, to refresh the editor UI
void ugcPlayingEditorRefresh( void );

// Called to figure out if an editor action is available
bool ugcPlayingEditorQueryAction( UGCActionID action, char** out_estr );

// Called to handle an editor action
void ugcPlayingEditorHandleAction( UGCActionID action );

// Called once per frame, handles input and logic.
void ugcPlayingEditorOncePerFrame( void );

// Called once per frame, handled additional drawing in the world
void ugcPlayingEditorDrawGhosts( void );

// Call this when first entering / exiting edit mode
void ugcPlayingEditorEditModeChanged( bool value );


// Call this on map change to tell the PlayingEditor how to map
// ComponentID's to Mat4's.
//
// Pass NULL to clear the mapping.
void ugcPlayingEditorMapChanged( UGCPlayComponentData** eaComponentData );

// For hooking up properties -- this will get a component's position as a mat4
void ugcPlayingEditorComponentMat4( const UGCComponent* component, Mat4 mat4 );
void ugcPlayingEditorComponentApplyMat4( UGCComponent* component, const Mat4 mat4 );

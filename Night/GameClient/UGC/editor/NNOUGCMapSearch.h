//// The component search window.
////
//// This provides a way to find components by name instead of by 2D
//// location.
////
//// NOTE: Maybe this should be integrated in with the ZeniPicker?
//// Currently it can't be done because the ZeniPicker shows a flat list
//// and this shows a tree.
#pragma once

typedef struct UGCProjectData UGCProjectData;
typedef struct UIWindow UIWindow;
typedef void* UserData;

typedef void (*UGCMapSearchCallback)( UserData data, const char* zmName, const char* logicalName ); //< Intentionaly use a similar interface to the ZeniPicker

UIWindow* ugcMapSearchWindowCreate( UGCProjectData* ugcProj, const char* mapName, UGCMapSearchCallback cb, UserData data );
void ugcMapSearchWindowRefresh( UIWindow* window );

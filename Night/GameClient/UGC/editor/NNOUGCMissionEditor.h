//// The mission (story) editor in UGC.
#pragma once

#include "UICore.h"

typedef enum UGCActionID UGCActionID;
typedef enum UGCComponentType UGCComponentType;
typedef enum UGCInteractPropertiesFlags UGCInteractPropertiesFlags;
typedef enum UGCMapType UGCMapType;
typedef struct UGCCheckedAttrib UGCCheckedAttrib;
typedef struct UGCInteractProperties UGCInteractProperties;
typedef struct UGCMap UGCMap;
typedef struct UGCMission UGCMission;
typedef struct UGCMissionDoc UGCMissionDoc;
typedef struct UGCMissionLibraryModel UGCMissionLibraryModel;
typedef struct UGCMissionMapLink UGCMissionMapLink;
typedef struct UGCMissionNodeGroup UGCMissionNodeGroup;
typedef struct UIComboBox UIComboBox;
typedef struct UISprite UISprite;
typedef struct UITreeNode UITreeNode;

typedef void* UserData;
typedef void (*UIActivationFunc)(void*, UserData);

UGCMissionDoc *ugcMissionDocLoad(void);
void ugcMissionDocClose(UGCMissionDoc** ppDoc);
void ugcMissionDocRefresh( UGCMissionDoc* doc );
void ugcMissionSetVisible(UGCMissionDoc *doc);
UGCMission *ugcMissionGetMission(UGCMissionDoc *doc);
void ugcMissionDocOncePerFrame(UGCMissionDoc* doc, bool isActive);
void ugcMissionDocHandleAction(UGCMissionDoc *doc, UGCActionID action);
bool ugcMissionDocQueryAction(UGCMissionDoc *doc, UGCActionID action, char** out_estr);
void ugcMissionDocHandleNewMap( UGCMissionDoc* doc, UGCMap* map );

void ugcMissionSetSelectedObjectiveByName( UGCMissionDoc* doc, const char* name );
void ugcMissionSetSelectedObjectiveByComponentName( UGCMissionDoc* doc, const char* name );
void ugcMissionSetSelectedObjectiveByDialogName( UGCMissionDoc* doc, const char* name );
void ugcMissionSetSelectedDialogTreeBlock( UGCMissionDoc* doc, U32 dialog_id );
void ugcMissionSetSelectedMapTransition( UGCMissionDoc* doc, U32 map_transition_objective_id );
void ugcMissionSetSelectedMapTransitionByMapLink( UGCMissionDoc* doc, UGCMissionMapLink* mapLink );
void ugcMissionSetSelectedMap( UGCMissionDoc* doc, U32 map_objective_id );
void ugcMissionSetSelectedMapByMapLink( UGCMissionDoc* doc, UGCMissionMapLink* mapLink );
void ugcMissionSetSelectedGroup(UGCMissionNodeGroup *group);
void ugcMissionSetSelectedGroupAndMakeVisible(UGCMissionNodeGroup *group);

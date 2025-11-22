#pragma once

typedef struct UGCComponent UGCComponent;
typedef struct UGCMap UGCMap;
typedef struct UGCMapEditorDoc UGCMapEditorDoc;
typedef struct UISprite UISprite;

void ugcMapEditorPropertiesRefresh( UGCMapEditorDoc* doc );
void ugcMapEditorPropertiesRefreshComponent( UGCComponent* component, bool isPlayingEditor );
void ugcMapEditorBackdropPropertiesRefresh( UGCMap* map, bool isPlayingEditor );
void ugcMapEditorPropertiesOncePerFrame( UGCMapEditorDoc* doc );

void ugcMapEditorGlobalPropertiesWindowShow( UGCMapEditorDoc* doc );
void ugcMapEditorGlobalPropertiesWindowRefresh( UGCMapEditorDoc* doc );
void ugcMapEditorGlobalPropertiesErrorButtonRefresh( UGCMapEditorDoc* doc, UISprite** ppErrorSprite );

void ugcMapEditorBackdropWindowShow( UGCMapEditorDoc* doc );
void ugcMapEditorBackdropPropertiesWindowRefresh( UGCMapEditorDoc* doc );
void ugcMapEditorBackdropPropertiesErrorButtonRefresh( UGCMapEditorDoc* doc, UISprite** ppErrorSprite );

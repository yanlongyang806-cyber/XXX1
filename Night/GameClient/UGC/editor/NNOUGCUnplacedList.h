#pragma once

typedef struct UGCComponent UGCComponent;
typedef struct UGCUnplacedList UGCUnplacedList;
typedef struct UIWidget UIWidget;
typedef void* UserData;

typedef void (*UGCUnplacedListSelectFn)( UGCUnplacedList* list, UserData userdata, U32 componentID );

#define UGC_UNPLACED_LIST_ROW_HEIGHT 90

typedef enum UGCUnplacedListMode
{
	UGCUnplacedList_MapEditor,
	UGCUnplacedList_PlayingEditor,
} UGCUnplacedListMode;

// List management
UGCUnplacedList* ugcUnplacedListCreate( UGCUnplacedListMode mode, UGCUnplacedListSelectFn pDragFn, UserData userdata );
void ugcUnplacedListDestroy( UGCUnplacedList** ppList );

void ugcUnplacedListSetMap( UGCUnplacedList* list, const char* mapName );
void ugcUnplacedListSetSelectedComponent( UGCUnplacedList* list, UGCComponent* component );

UIWidget* ugcUnplacedListGetUIWidget( UGCUnplacedList* list );
int ugcUnplacedListGetContentHeight( UGCUnplacedList* list );

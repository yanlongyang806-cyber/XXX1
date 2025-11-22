/// Widgets and helper functions for searching UGC assets.
#pragma once

typedef enum UGCComponentType UGCComponentType;
typedef enum UGCMapType UGCMapType;
typedef struct UGCAssetLibraryPane UGCAssetLibraryPane;
typedef struct UGCAssetLibraryRow UGCAssetLibraryRow;
typedef struct UGCAssetTagType UGCAssetTagType;
typedef struct UGCProjectData UGCProjectData;
typedef struct UIPane UIPane;
typedef struct UIWidget UIWidget;
typedef struct WorldUGCProperties WorldUGCProperties;
typedef void* UserData;

typedef void (*UGCAssetLibrarySelectFn)(UGCAssetLibraryPane *lib_pane, UserData userdata, UGCAssetLibraryRow *row);
typedef void (*UGCAssetLibraryMapSelectFn)(const char* mapName, const char* logicalName, UserData userdata);
typedef bool (*UGCAssetLibraryCustomFilterFn)(UserData data, UGCAssetLibraryRow *row);

typedef enum UGCAssetLibraryMode
{
	UGCAssetLibrary_Legacy,
	UGCAssetLibrary_MapEditorEmbedded,
	UGCAssetLibrary_PlayingEditorEmbedded,
	UGCAssetLibrary_GenericWindow,
	UGCAssetLibrary_NewMapWindow,
} UGCAssetLibraryMode;

/// A single item in the asset library.
AUTO_STRUCT;
typedef struct UGCAssetLibraryRow
{
	const char *pcName;					AST(NAME("Name"))
	const char *astrType;				AST(NAME("Type") POOL_STRING)
	const char *astrTags;				AST(NAME("Tags") POOL_STRING)
	WorldUGCProperties *pProperties;	AST(NAME("Properties"))
} UGCAssetLibraryRow;
extern ParseTable parse_UGCAssetLibraryRow[];
#define TYPE_parse_UGCAssetLibraryRow UGCAssetLibraryRow

// Picker Windows, for all your picking needs
UGCAssetLibraryPane *ugcAssetLibraryShowPicker(UserData userdata, bool bUseProjectData, const char *title, const char* note, const char *tag_type_name, const char* default_value, UGCAssetLibrarySelectFn callback);
void ugcAssetLibraryShowCostumePicker(UserData userdata, bool bUseProjectData, const char *title,
									  const char* default_costume, const char* default_pet,
									  UGCAssetLibrarySelectFn callback, UGCAssetLibraryMapSelectFn mapCallback,
									  UGCAssetLibrarySelectFn petCallback);
void ugcAssetLibraryShowCheckedAttribPicker(UserData userdata, bool bUseProjectData, bool allowSkills, bool allowItems, const char *title, UGCAssetLibrarySelectFn callback);

// Low-level pane management
UGCAssetLibraryPane* ugcAssetLibraryPaneCreate( UGCAssetLibraryMode mode, bool bUseProjectData, UGCAssetLibrarySelectFn pLibraryDragCB, UGCAssetLibrarySelectFn pLibraryDoubleClickCB, UserData userdata );
// Legacy interface
UGCAssetLibraryPane* ugcAssetLibraryPaneCreateLegacy(UserData userdata, bool bUseProjectData, const char *tag_type_name, const char* default_value, const char** default_tags, const char** force_tags, UGCAssetLibrarySelectFn pLibraryDragCB, UGCAssetLibrarySelectFn pLibraryDoubleClickCB, bool bEmbeddedMode_IGNORED);
void ugcAssetLibraryPaneDestroy(UGCAssetLibraryPane* ppPane);

// Basic state querying
UGCAssetLibraryRow *ugcAssetLibraryPaneGetSelected(UGCAssetLibraryPane *lib_pane);
void ugcAssetLibraryPaneSetSelected(UGCAssetLibraryPane *lib_pane, const char *object_name);
void ugcAssetLibraryPaneClearSelected(UGCAssetLibraryPane *lib_pane);
void ugcAssetLibraryPaneSetTagTypeName(UGCAssetLibraryPane* lib_pane, const char* tag_type_name);
const char* ugcAssetLibraryPaneGetTagTypeName(UGCAssetLibraryPane *lib_pane);
void ugcAssetLibraryPaneSetHeaderWidget(UGCAssetLibraryPane* lib_pane, UIWidget* headerWidget );
bool ugcAssetLibraryPickerWindowOpen( void );

// Get the root pane
UIPane *ugcAssetLibraryPaneGetUIPane(UGCAssetLibraryPane *lib_pane);

// Refresh the library list's model
void ugcAssetLibraryPaneRefreshLibraryModel(UGCAssetLibraryPane *lib_pane);

// Filter out any resource not available on this map type.
//
// (Used primarily for ground/space costumes)
void ugcAssetLibraryPaneRestrictMapType(UGCAssetLibraryPane *lib_pane, UGCMapType map_type);

// Filter out any resource not passing this functions.
void ugcAssetLibraryPaneSetExtraFilter(UGCAssetLibraryPane *lib_pane, UGCAssetLibraryCustomFilterFn fn, UserData data);

// Helper function to convert from an asset library row to a component
bool ugcAssetLibraryRowFillComponentTypeAndName( UGCAssetLibraryPane* pane, UGCAssetLibraryRow* row, UGCComponentType* out_type, char* out_rowName, int out_rowName_size );

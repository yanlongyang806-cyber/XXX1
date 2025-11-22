//// Pickers for objects on existing maps.
////
//// This looks at all the Zeni files, and lets authors pick among
//// them.
#pragma once
GCC_SYSTEM

typedef struct UGCZeniPickerOverworldMap UGCZeniPickerOverworldMap;
typedef struct UIWidget UIWidget;
typedef struct UIWidgetWidget UIWidgetWidget;
typedef struct ZoneMapEncounterInfo ZoneMapEncounterInfo;
typedef struct ZoneMapEncounterObjectInfo ZoneMapEncounterObjectInfo;

AUTO_ENUM;
typedef enum UGCZeniPickerFilterType {
	UGCZeniPickerType_None,		EIGNORE
	UGCZeniPickerType_Spawn,
	UGCZeniPickerType_Clickie,
	UGCZeniPickerType_Destructible,
	UGCZeniPickerType_Door,
	UGCZeniPickerType_Encounter,
	UGCZeniPickerType_Volume,
	UGCZeniPickerType_Contact,
	UGCZeniPickerType_Other,
	UGCZeniPickerType_WholeMap,
	UGCZeniPickerType_Reward_Box,
	
	UGCZeniPickerType_Usable_As_Warp,
	UGCZeniPickerType_Usable_For_ComponentComplete,
	UGCZeniPickerType_Usable_For_ComponentReached,
	UGCZeniPickerType_Any,
} UGCZeniPickerFilterType;
extern StaticDefineInt UGCZeniPickerFilterTypeEnum[];

typedef void (*UGCZeniPickerCallback)( const char* zmName, const char* logicalName, const float* mapPos, const char* mapIcon, UserData userData );
typedef bool (*UGCZeniPickerFilterFn)( const char* zmName, ZoneMapEncounterObjectInfo* object, UserData userData );
typedef void (*UGCZeniPickerChangedFn)( UserData userData );

bool ugcZeniPickerShow(ZoneMapEncounterInfo **ugcInfoOverrides,
					   UGCZeniPickerFilterType forceFilterType,
					   const char* defaultZmap, const char* defaultObj,
					   UGCZeniPickerFilterFn filterFn, UserData filterData,
					   UGCZeniPickerCallback cb, UserData userData );
void ugcZeniPickerOverworldMapShow(const char** eaIconNames, float* defaultPos, const char* defaultIcon,
								   UGCZeniPickerCallback cb, UserData userData );

UIWidget* ugcZeniPickerWidgetCreate( bool* out_selectedDefault, ZoneMapEncounterInfo **ugcInfoOverrides,
									 UGCZeniPickerFilterType forceFilterType,
									 SA_PARAM_OP_STR const char* defaultZmap, SA_PARAM_OP_STR const char* defaultObj,
									 UGCZeniPickerFilterFn filterfn, UserData filterData );
void ugcZeniPickerWidgetGetSelection( UIWidget* widget, const char** out_mapName, const char** out_logicalName );

UIWidget* ugcZeniPickerOverworldMapWidgetCreate( const char** iconNames, const float* defaultPos, const char* defaultIcon,
												 UGCZeniPickerChangedFn changedFn, UserData changedData );
void ugcZeniPickerOverworldMapWidgetGetSelection( UIWidget* widget, float* out_mapPos, const char** out_icon );
bool ugcZeniPickerWindowOpen( void );

UIWidgetWidget* ui_ZeniPickerIconCreate( UGCZeniPickerOverworldMap* picker );
void ui_ZeniPickerIconSetIcon( UIWidgetWidget* widget, const char* iconName );

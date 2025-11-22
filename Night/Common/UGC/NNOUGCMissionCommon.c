//// UGC mission common routines
////
//// Code used to validate & generate UGC missions on both client & server.

#include "NNOUGCMissionCommon.h"

#include "AnimList_Common.h"
#include "Error.h"
#include "Expression.h"
#include "NNOUGCCommon.h"
#include "NNOUGCResource.h"
#include "RegionRules.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "TextFilter.h"
#include "TokenStore.h"
#include "NNOUGCInteriorCommon.h"
#include "UGCProjectUtils.h"
#include "WorldGrid.h"
#include "contact_common.h"
#include "mission_common.h"
#include "timing.h"
#include "wlUGC.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

static bool ugcObjectiveHasComponent( UGCMissionObjective* objective, UserData rawComponentID );

static void ugcMissionObjectiveListInOrder( UGCMissionObjective*** out_objectives, UGCMissionObjective** objectives )
{
	int it;
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		
		if( objective->type == UGCOBJ_IN_ORDER || objective->type == UGCOBJ_ALL_OF ) {
			ugcMissionObjectiveListInOrder( out_objectives, objective->eaChildren );
		} else {
			eaPush( out_objectives, objective );
		}
	}
}

static int ugcMapIdxByName( UGCProjectData* projData, const char* mapName )
{
	int mapIt;
	for( mapIt = 0; mapIt != eaSize( &projData->maps ); ++mapIt ) {
		UGCMap* map = projData->maps[ mapIt ];

		if( resNamespaceBaseNameEq( mapName, map->pcName )) {
			return mapIt;
		}
	}
			
	return -1;
}

static void ugcMissionObjectivesCount1( UGCProjectData *ugcProj, UGCMissionObjective** objectives, int* out_maxDepth, int* out_numObjectives )
{
	int it;
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		int maxDepth = 0;

		ugcMissionObjectivesCount1( ugcProj, objective->eaChildren, &maxDepth, out_numObjectives );

		// We must look at the component of componentID to determine what type of objective we are
		if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR )
		{
			UGCComponent* component = ugcComponentFindByID( ugcProj->components, objective->componentID );
			if( component && component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP )
			{
				++*out_numObjectives;
			}
		}
		if( objective->type == UGCOBJ_ALL_OF ) {
			*out_maxDepth = MAX( *out_maxDepth, maxDepth + 1 );
		} else {
			*out_maxDepth = MAX( *out_maxDepth, maxDepth );
		}
	}
}

void ugcMissionObjectivesCount( UGCProjectData *ugcProj, int* out_maxDepth, int* out_numObjectives )
{
	*out_maxDepth = 0;
	*out_numObjectives = 0;

	ugcMissionObjectivesCount1( ugcProj, ugcProj->mission->objectives, out_maxDepth, out_numObjectives );
}

const char* ugcObjectiveInternalMapName( UGCProjectData* ugcProj, UGCMissionObjective* objective )
{
	bool isInternal;
	const char* mapName = ugcObjectiveMapName( ugcProj, objective, &isInternal );

	if( !isInternal ) {
		return NULL;
	} else {
		return mapName;
	}
}

bool ugcObjectiveIsCrypticMap( UGCProjectData* ugcProj, UGCMissionObjective* objective )
{
	if( objective->type == UGCOBJ_ALL_OF || objective->type == UGCOBJ_IN_ORDER ) {
		int it;
		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			if( !ugcObjectiveIsCrypticMap( ugcProj, objective->eaChildren[ it ])) {
				return false;
			}
		}

		return true;
	} else {
		bool isInternal;
		const char* mapName = ugcObjectiveMapName( ugcProj, objective, &isInternal );

		return !isInternal && !nullStr( mapName );
	}
}

const char* ugcObjectiveMapName( const UGCProjectData* ugcProj, const UGCMissionObjective* objective, bool* out_isInternal )
{
	const char* toReturn = NULL;
	bool dummy;
	if( !out_isInternal ) {
		out_isInternal = &dummy;
	}

	if( objective->type == UGCOBJ_TMOG_MAP_MISSION || objective->type == UGCOBJ_TMOG_REACH_INTERNAL_MAP ) {
		if( objective->astrMapName ) {
			*out_isInternal = true;
			return objective->astrMapName;
		} else {
			*out_isInternal = false;
			return NULL;
		}
	} else if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR ) {
		UGCComponent* component = ugcComponentFindByID( ugcProj->components, objective->componentID );

		if( !component ) {
			*out_isInternal = false;
			return NULL;
		}

		if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			// the actual important component for placement is the contact
			component = ugcComponentFindByID( ugcProj->components, component->uActorID );
		}

		if( !component ) {
			*out_isInternal = false;
			return NULL;
		}

		if( component->sPlacement.bIsExternalPlacement ) {
			if( component->sPlacement.pcExternalMapName ) {
				*out_isInternal = false;
				return component->sPlacement.pcExternalMapName;
			} else {
				*out_isInternal = false;
				return NULL;
			}
		} else {
			if( component->sPlacement.pcMapName ) {
				*out_isInternal = true;
				return component->sPlacement.pcMapName;
			} else {
				*out_isInternal = false;
				return NULL;
			}
		}
	} else {
		UGCMissionObjective* child = eaGet( &objective->eaChildren, 0 );
		int it;
		const char* childMap;
		
		// must be a compound objective type
		assert( objective->type == UGCOBJ_ALL_OF || objective->type == UGCOBJ_IN_ORDER );
		
		if( !child ) {
			*out_isInternal = false;
			return NULL;
		}

		childMap = ugcObjectiveMapName( ugcProj, child, out_isInternal );
		for( it = 1; it < eaSize( &objective->eaChildren ); ++it ) {
			bool childIsInternal;
			if(   stricmp( childMap, ugcObjectiveMapName( ugcProj, objective->eaChildren[ it ], &childIsInternal )) != 0
				  || childIsInternal != *out_isInternal ) {
				*out_isInternal = false;
				return NULL;
			}
		}

		return childMap;
	}

	*out_isInternal = false;
	return NULL;
}

bool ugcObjectiveIsReachMap( UGCProjectData* ugcProj, UGCMissionObjective* objective )
{
	if( objective->type == UGCOBJ_ALL_OF || objective->type == UGCOBJ_IN_ORDER ) {
		int it;
		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			if( !ugcObjectiveIsReachMap( ugcProj, objective->eaChildren[ it ])) {
				return false;
			}
		}

		return true;
	} else if( objective->type == UGCOBJ_COMPLETE_COMPONENT ) {
		UGCComponent* component = ugcComponentFindByID( ugcProj->components, objective->componentID );

		if( component && component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP ) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}


UGCMapInfo** ugcObjectiveFinishMapNames( UGCProjectData* ugcProj, UGCMissionObjective* objective )
{
	UGCMapInfo** accum = NULL;

	bool isInternal = false;
	const char* mapName = ugcObjectiveMapName( ugcProj, objective, &isInternal );

	if( mapName ) {
		UGCMapInfo* info = StructCreate( parse_UGCMapInfo );
		info->mapName = StructAllocString( mapName );
		info->isInternal = isInternal;
		eaPush( &accum, info );
	} else if( objective->type == UGCOBJ_ALL_OF ) {
		UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );
		UGCMissionObjective* origObjective = ugcObjectiveFind( ugcProj->mission->objectives, objective->id );

		if( origObjective ) {
			int colIt;
			for( colIt = 0; colIt != eaSize( &origObjective->eaChildren ); ++colIt ) {
				UGCMissionObjective* colObjective = origObjective->eaChildren[ colIt ];
				UGCMapTransitionInfo* transition = ugcMissionFindTransitionForObjective( transitions, colObjective->eaChildren[ 0 ]->id ); 

				if( transition ) {
					if( transition->prevIsInternal ) {
						UGCMapInfo* info = StructCreate( parse_UGCMapInfo );
						info->mapName = StructAllocString( transition->prevMapName );
						info->isInternal = transition->prevIsInternal;
						eaPush( &accum, info );
					} else {
						UGCMissionMapLink* link = ugcMissionFindLinkByObjectiveID( ugcProj, colObjective->eaChildren[ 0 ]->id, false );
						UGCComponent* linkComponent = NULL;

						if( link ) {
							linkComponent = ugcComponentFindByID( ugcProj->components, link->uDoorComponentID );
						}

						if( linkComponent ) {
							UGCMapInfo* info = StructCreate( parse_UGCMapInfo );
							assert( linkComponent->sPlacement.bIsExternalPlacement && linkComponent->sPlacement.pcExternalMapName );
							info->mapName = StructAllocString( linkComponent->sPlacement.pcExternalMapName );
							info->isInternal = false;
							eaPush( &accum, info );
						} else {
							UGCMapInfo* info = StructCreate( parse_UGCMapInfo );
							UGCPerAllegianceDefaults* defaults = ugcGetAllegianceDefaults(ugcProj);
							if( defaults ) {
								info->mapName = StructAllocString( defaults->pcDefaultCrypticMap );
							} else {
								info->mapName = StructAllocString( "KFR_Sol_Starbase_Ground" ); // TOMY TODO: fix this hack
							}
							info->isInternal = false;
							eaPush( &accum, info );
						}
					}
				}
			}

			// remove duplicates
			{
				int it1;
				int it2;
				for( it1 = 0; it1 != eaSize( &accum ); ++it1 ) {
					for( it2 = it1 + 1; it2 != eaSize( &accum ); ++it2 ) {
						UGCMapInfo* info1 = accum[ it1 ];
						UGCMapInfo* info2 = accum[ it2 ];

						if( stricmp( info1->mapName, info2->mapName ) == 0 && info1->isInternal == info2->isInternal ) {
							eaRemove( &accum, it2 );
							StructDestroy( parse_UGCMapInfo, info2 );
							--it2;
						}
					}
				}
			}
		}

		eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
	}

	return accum;
}

static void ugcMissionTransmogrifyObjectives1( UGCProjectData* ugcProj, UGCMissionObjective** objectives, const char* targetMapName, bool createTmogCrypticMap, U32* generatedID, UGCMissionObjective*** out_peaTmogObjectives );

void ugcMissionTransmogrifyObjectives( UGCProjectData* ugcProj, UGCMissionObjective** objectives, const char* targetMapName, bool createTmogCrypticMap, UGCMissionObjective*** out_peaTmogObjectives )
{
	U32 generatedID = 9001;
	ugcMissionTransmogrifyObjectives1( ugcProj, objectives, targetMapName, createTmogCrypticMap, &generatedID, out_peaTmogObjectives );
}

static void ugcMissionObjectiveAddChildNoInOrder( UGCMissionObjective*** peaObjectives, UGCMissionObjective* objective )
{
	// Just push the children list on
	if( objective->type == UGCOBJ_IN_ORDER ) {
		int it;
		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			eaPush( peaObjectives, StructClone( parse_UGCMissionObjective, objective->eaChildren[ it ]));
		}
	} else {
		eaPush( peaObjectives, StructClone( parse_UGCMissionObjective, objective ));
	}
}

static void ugcMissionTransmogrifyObjectives1( UGCProjectData* ugcProj, UGCMissionObjective** objectives, const char* targetMapName, bool createTmogCrypticMap, U32* generatedID, UGCMissionObjective*** out_peaTmogObjectives )
{
	int it;

	if( eaSize( &objectives ) == 0 ) {
		return;
	}
	
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		const char* mapName = ugcObjectiveInternalMapName( ugcProj, objective );

		if( mapName ) {
			UGCMap* map = ugcMapFindByName( ugcProj, mapName );
			bool isReachMapAccum = true;
			UGCMissionObjective* mapObjective = NULL;
			
			if( !targetMapName ) {
				mapObjective = StructCreate( parse_UGCMissionObjective );

				mapObjective->type = UGCOBJ_TMOG_MAP_MISSION;
				mapObjective->id = (*generatedID)++;
				mapObjective->astrMapName = allocAddString( mapName );
				if( ugcDefaultsSingleMissionEnabled() ) {
					UGCMissionMapLink* exitLink = ugcMissionFindLinkByObjectiveID( ugcProj, objective->id, false );
					mapObjective->uiString = ugcAllocSMFString( ugcMapMissionLinkReturnText( exitLink ), false );
				} else {
					mapObjective->uiString = ugcAllocSMFString( ugcMapMissionName( ugcProj, objective ), false );
				}
				eaPush( out_peaTmogObjectives, mapObjective );

				if( isReachMapAccum ) {
					isReachMapAccum = ugcObjectiveIsReachMap( ugcProj, objective );
				}
			} else if( resNamespaceBaseNameEq( targetMapName, mapName )) {
				ugcMissionObjectiveAddChildNoInOrder( out_peaTmogObjectives, objective );
			}

			{
				UGCMissionObjective* nextObjective = eaGet( &objectives, it + 1 );
				while( nextObjective && resNamespaceBaseNameEq( ugcObjectiveInternalMapName( ugcProj, nextObjective ), mapName )) {
					if( isReachMapAccum ) {
						isReachMapAccum = ugcObjectiveIsReachMap( ugcProj, nextObjective );
					}
					
					if( resNamespaceBaseNameEq( targetMapName, mapName )) {
						ugcMissionObjectiveAddChildNoInOrder( out_peaTmogObjectives, nextObjective );
					}
					
					++it;
					nextObjective = eaGet( &objectives, it + 1 );
				}
			}

			if( isReachMapAccum && mapObjective ) {
				mapObjective->type = UGCOBJ_TMOG_REACH_INTERNAL_MAP;
			}
		} else {
			if( !targetMapName ) {
				if( createTmogCrypticMap && ugcObjectiveIsCrypticMap( ugcProj, objective )) {
					UGCMissionObjective* crypticObjective = StructCreate( parse_UGCMissionObjective );
					crypticObjective->type = UGCOBJ_TMOG_REACH_CRYPTIC_MAP;
					crypticObjective->id = (*generatedID)++;
					eaPush( out_peaTmogObjectives, crypticObjective );

					// Don't push "IN_ORDER" nodes directly into the list of children -- that's not useful
					ugcMissionObjectiveAddChildNoInOrder( &crypticObjective->eaChildren, objective );

					{
						UGCMissionObjective* nextObjective = eaGet( &objectives, it + 1 );
						while( nextObjective && ugcObjectiveIsCrypticMap( ugcProj, nextObjective )) {
							ugcMissionObjectiveAddChildNoInOrder( &crypticObjective->eaChildren, nextObjective );
							++it;
							nextObjective = eaGet( &objectives, it + 1 );
						}
					}					
				} else {
					UGCMissionObjective* newObjective = StructClone( parse_UGCMissionObjective, objective );
					devassert( newObjective );
					eaDestroyStruct( &newObjective->eaChildren, parse_UGCMissionObjective );
					eaPush( out_peaTmogObjectives, newObjective );
					
					ugcMissionTransmogrifyObjectives1( ugcProj, objective->eaChildren, NULL, createTmogCrypticMap, generatedID, &newObjective->eaChildren );
				}
			} else {
				ugcMissionTransmogrifyObjectives1( ugcProj, objective->eaChildren, targetMapName, createTmogCrypticMap, generatedID, out_peaTmogObjectives );
			}
		}
	}
}

//////////////////////////////////////////////////////////
// Component pre-transmogrify
//////////////////////////////////////////////////////////


typedef bool (*ObjectiveFindFn)( UGCMissionObjective* obj, UserData data );
UGCMissionObjective* ugcObjectiveFindEx( UGCMissionObjective** objectives, ObjectiveFindFn fn, UserData data )
{
	int it;	
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		if( fn( objective, data )) {
			return objective;
		}

		{
			UGCMissionObjective* namedObjective = ugcObjectiveFindEx( objective->eaChildren, fn, data );
			if( namedObjective ) {
				return namedObjective;
			}
		}
	}

	return NULL;
}

UGCMissionObjective* ugcObjectiveFind( UGCMissionObjective** objectives, U32 id )
{
	return ugcObjectiveFindEx( objectives, ugcObjectiveHasIDRaw, (UserData)(uintptr_t)id );
}

static U32 ugcObjectiveNameGetID(const char* name)
{
	char* beforeId = strrchr(name, '_');
	if( beforeId ) {
		return atoi( beforeId + 1 );
	} else {
		return 0;
	}
}

UGCMissionObjective* ugcObjectiveFindByLogicalName( UGCMissionObjective** objectives, const char* logicalName )
{
	return ugcObjectiveFind( objectives, ugcObjectiveNameGetID( logicalName ));
}

UGCMissionObjective* ugcObjectiveFindPrevious( UGCMissionObjective** objectives, U32 id )
{
	UGCMissionObjective* prev = NULL;
	int it;	
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		if( objective->id == id ) {
			return prev;
		}

		{
			UGCMissionObjective* namedObjective = ugcObjectiveFindPrevious( objective->eaChildren, id );
			if( namedObjective ) {
				return namedObjective;
			}
		}

		prev = objective;
	}

	return NULL;
}

/// Returns an EArray of objectives, that contain the path from the
/// root (passed in as OBJECTIVES) to an objective with id ID.  It
/// ends up containing the objective at the end.
UGCMissionObjective** ugcObjectiveFindPath( UGCMissionObjective** objectives, U32 id )
{	
	int it;	
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		if( objective->id == id ) {
			UGCMissionObjective** path = NULL;
			eaPush( &path, objective );
			return path;
		}

		{
			UGCMissionObjective** path = ugcObjectiveFindPath( objective->eaChildren, id );
			if( path ) {
				eaInsert( &path, objective, 0 );
				return path;
			}
		}
	}

	return NULL;
}

UGCMissionObjective*** ugcObjectiveFindParentEA( UGCMissionObjective*** peaObjectives, U32 id, int* out_index )
{
	// Certainly not memory efficient, but it's quite easy to code
	UGCMissionObjective*** parentEA = NULL;
	UGCMissionObjective** path = ugcObjectiveFindPath( *peaObjectives, id );
	
	if( !path ) {
		parentEA = NULL;
		*out_index = -1;
	} else if( eaSize( &path ) == 1 ) {
		parentEA = peaObjectives;
		*out_index = eaFind( peaObjectives, path[ 0 ]);
	} else {
		parentEA = &path[ eaSize( &path ) - 2 ]->eaChildren;
		*out_index = eaFind( parentEA, path[ eaSize( &path ) - 1 ]);
	}

	return parentEA;
}

UGCMissionObjective* ugcObjectiveFindMapObjective( UGCMissionObjective** objectives, const char* name )
{
	return ugcObjectiveFindEx( objectives, ugcObjectiveIsCompleteMapOnMap, (char*)name );
}


UGCMissionObjective* ugcObjectiveFindComponent( UGCMissionObjective** objectives, U32 componentID )
{
	return ugcObjectiveFindEx( objectives, ugcObjectiveHasComponent, (UserData)(uintptr_t)componentID );
}

/// If you update this function, make sure to also update
/// ugcObjectiveFindComponentRelatedUsingTable()
UGCMissionObjective* ugcObjectiveFindComponentRelated( UGCMissionObjective** objectives, UGCComponentList *componentList, U32 componentID)
{
	UGCComponent *component = ugcComponentFindByID(componentList, componentID);
	if (!component)
		return NULL;

	if (component->eType == UGC_COMPONENT_TYPE_CONTACT || component->eType == UGC_COMPONENT_TYPE_OBJECT)
	{
		UGC_FOR_EACH_COMPONENT_OF_TYPE(componentList, UGC_COMPONENT_TYPE_DIALOG_TREE, other_component)
		{
			if (other_component->uActorID == component->uID)
			{
				UGCMissionObjective *objective = ugcObjectiveFindComponent( objectives, other_component->uID);
				if (objective)
					return objective;
			}
		}
		UGC_FOR_EACH_COMPONENT_END;
	}
	return ugcObjectiveFindComponent( objectives, componentID );
}

/// If you update this function, make sure to also update
/// ugcObjectiveFindComponentRelated()
const UGCMissionObjective* ugcObjectiveFindComponentRelatedUsingTableConst( const UGCProjectData* ugcProj, const UGCBacklinkTable* pBacklinkTable, U32 componentID)
{
	UGCComponent *component = ugcComponentFindByID(ugcProj->components, componentID);
	if (!component)
		return NULL;

	if (component->eType == UGC_COMPONENT_TYPE_CONTACT || component->eType == UGC_COMPONENT_TYPE_OBJECT)
	{
		UGCComponent** eaDialogTrees = NULL;
		ugcBacklinkTableGetDialogTreesForComponent( ugcProj, pBacklinkTable, componentID, &eaDialogTrees );

		FOR_EACH_IN_EARRAY_FORWARDS( eaDialogTrees, UGCComponent, other_component)
		{
			if (other_component->uActorID == component->uID)
			{
				UGCMissionObjective *objective = ugcObjectiveFindComponent( ugcProj->mission->objectives, other_component->uID);
				if (objective) {
					eaDestroy( &eaDialogTrees );
					return objective;
				}
			}
		}
		FOR_EACH_END;

		eaDestroy( &eaDialogTrees );
	}
	return ugcObjectiveFindComponent( ugcProj->mission->objectives, componentID );
}

typedef struct MapData
{
	UGCComponentList* componentList;
	const char* map;
} MapData;

static bool ugcObjectiveIsOnMap( UGCMissionObjective* objective, UserData rawMapData )
{
	MapData* mapData = (MapData*)rawMapData;

	if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR ) {
		UGCComponent* component = ugcComponentFindByID( mapData->componentList, objective->componentID );

		if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			component = ugcComponentFindByID( mapData->componentList, component->uActorID );
		}

		if( component && mapData->map && ugcComponentIsOnMap( component, mapData->map, true )) {
			return true;
		}
	}

	return false;
}

UGCMissionObjective* ugcObjectiveFindOnMap( UGCMissionObjective** objectives, UGCComponentList* componentList, const char* map )
{
	MapData mapData = { componentList, map };
	return ugcObjectiveFindEx( objectives, ugcObjectiveIsOnMap, (UserData)&mapData );
}

static bool ugcObjectiveIsOnCrypticMap( UGCMissionObjective* objective, UserData rawMapData )
{
	MapData* mapData = (MapData*)rawMapData;

	if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR ) {
		UGCComponent* component = ugcComponentFindByID( mapData->componentList, objective->componentID );

		if(   component && ugcComponentIsOnMap( component, NULL, false )
			  && stricmp( component->sPlacement.pcExternalMapName, mapData->map ) == 0 ) {
			return true;
		}
	}

	return false;
}

UGCMissionObjective* ugcObjectiveFindOnCrypticMap( UGCMissionObjective** objectives, UGCComponentList* componentList, const char* map )
{
	MapData mapData = { componentList, map };
	return ugcObjectiveFindEx( objectives, ugcObjectiveIsOnCrypticMap, (UserData)&mapData );
}


void ugcMissionGetComponentPortalProperties( UGCProjectData *ugcProj, UGCComponent *pComponent, const char* mission_name, bool is_start,
										   const char **out_map_name, UGCPortalClickableInfo **out_clickable,
										   const char **out_spawn, char **out_warp_text)
{
	int i;
	char ns[RESOURCE_NAME_MAX_SIZE], buf[RESOURCE_NAME_MAX_SIZE];

	if (out_clickable && !*out_clickable)
	{
		*out_clickable = StructCreate(parse_UGCPortalClickableInfo);
		(*out_clickable)->mission_name = StructAllocString(mission_name);
		(*out_clickable)->layout_name = StructAllocString(GENESIS_UGC_LAYOUT_NAME);
	}

	if (!pComponent)
	{
		if (out_clickable)
		{
			(*out_clickable)->is_whole_map = true;
		}
		if (out_spawn)
		{
			*out_spawn = StructAllocString( "MISSION_RETURN" );
		}
		if (out_map_name)
		{
			UGCPerAllegianceDefaults *defaults = ugcGetAllegianceDefaults(ugcProj);
			if( defaults && defaults->pcDefaultCrypticMap)
			{
				*out_map_name = StructAllocString(defaults->pcDefaultCrypticMap);
			}
			else
			{
				*out_map_name = StructAllocString("Kfr_Sol_Starbase_Ground"); // TomY TODO remove this hack
			}
		}
		return;
	}
	if (pComponent->sPlacement.bIsExternalPlacement)
	{
		if (out_clickable)
		{
			if (pComponent->eType == UGC_COMPONENT_TYPE_WHOLE_MAP)
				(*out_clickable)->is_whole_map = true;
			else
				(*out_clickable)->challenge_name = StructAllocString(pComponent->sPlacement.pcExternalObjectName);
		}
		if (out_spawn)
		{
			assert( pComponent->eType == UGC_COMPONENT_TYPE_SPAWN );
			if (!nullStr(pComponent->sPlacement.pcExternalObjectName)) {
				*out_spawn = StructAllocString( pComponent->sPlacement.pcExternalObjectName );
			} else {
				*out_spawn = StructAllocString( "Mission_Return" );
			}
		}
		if (out_map_name)
			*out_map_name = StructAllocString(pComponent->sPlacement.pcExternalMapName);
	}
	else
	{
		if (out_map_name)
		{
			resExtractNameSpace_s(ugcProj->components->pcName, SAFESTR(ns), NULL, 0);
			sprintf(buf, "%s:%s", ns, pComponent->sPlacement.pcMapName);
			*out_map_name = StructAllocString(buf);
		}
		if (pComponent->eType == UGC_COMPONENT_TYPE_ROOM_MARKER)
		{
			if (out_clickable && !pComponent->bIsRoomVolume)
			{
				sprintf(buf, "%s_%s", mission_name, ugcComponentGetLogicalNameTemp(pComponent));
				(*out_clickable)->challenge_name = StructAllocString(buf);
				(*out_clickable)->is_volume = true;
			}
			if (out_spawn)
			{
				sprintf(buf, "%s_%s", mission_name, ugcComponentGetLogicalNameTemp(pComponent));
				*out_spawn = StructAllocString(buf);
			}
		}
		else if (pComponent->eType == UGC_COMPONENT_TYPE_SPAWN)
		{
			if (out_spawn)
			{
				sprintf(buf, "%s_%s", mission_name, ugcComponentGetLogicalNameTemp(pComponent));
				*out_spawn = StructAllocString(buf);
			}
		}
		else if (pComponent->eType == UGC_COMPONENT_TYPE_WHOLE_MAP)
		{
			if (out_clickable)
			{
				(*out_clickable)->is_whole_map = true;
			}
		}
		else
		{
			if (out_clickable)
			{
				sprintf(buf, "%s_%s", mission_name, ugcComponentGetLogicalNameTemp(pComponent));
				(*out_clickable)->challenge_name = StructAllocString(buf);
				if (pComponent->eType == UGC_COMPONENT_TYPE_PLANET)
					(*out_clickable)->is_volume = true;
			}
			if (out_spawn)
			{
				bool found_spawn = false;
				for (i = 0; i < eaiSize(&pComponent->uChildIDs); i++)
				{
					UGCComponent *pChild = ugcComponentFindByID(ugcProj->components, pComponent->uChildIDs[i]);
					if (pChild && pChild->eType == UGC_COMPONENT_TYPE_SPAWN)
					{
						sprintf(buf, "%s_%s", mission_name, ugcComponentGetLogicalNameTemp(pChild));
						*out_spawn = StructAllocString(buf);
						found_spawn = true;
						break;
					}
				}
				if (!found_spawn)
				{
					sprintf(buf, "%s_%s_SPAWN", mission_name, ugcComponentGetLogicalNameTemp(pComponent));
					*out_spawn = StructAllocString(buf);
				}
			}
		}

		if (out_clickable)
			(*out_clickable)->room_name = StructAllocString(ugcIntLayoutGetRoomName(pComponent->sPlacement.uRoomID));
	}

	if (out_warp_text)
		*out_warp_text = StructAllocString("Go to next map");
}

const char *ugcMissionGetPortalClickableLogicalNameTemp(UGCPortalClickableInfo *clickable)
{
	static char buf[128];
	if (clickable->is_whole_map)
	{
		sprintf(buf, "%s_%s_Optacts", clickable->layout_name, clickable->mission_name);
	}
	else if (clickable->challenge_name)
	{
		if (clickable->is_volume)
		{
			// Component Volume
			sprintf(buf, "%s_VOLUME", clickable->challenge_name);
		}
		else
		{
			// Component
			sprintf(buf, "%s", clickable->challenge_name);
		}
	}
	else
	{
		sprintf(buf, "%s_%s_%s", clickable->layout_name, clickable->mission_name, clickable->room_name);
	}
	return buf;
}

bool ugcMissionGetPortalShouldAutoExecute(UGCPortalClickableInfo* clickable)
{
	if( ugcDefaultsMapTransitionsSpecifyDoor() ) {
		return clickable->is_volume;
	} else {
		return clickable->is_whole_map || clickable->is_volume;
	}
}

static UGCMapTransitionInfo** ugcMissionGetMapTransitions1( const UGCProjectData* ugcProj, const UGCMissionObjective** missionObjectives, const char* prevMapName, bool prevIsInternal, UGCMapTransitionInfo *prevTransition MEM_DBG_PARMS )
{
	UGCMapTransitionInfo** accum = NULL;
	
	int it;
	for( it = 0; it != eaSize( &missionObjectives ); ++it ) {
		const UGCMissionObjective* objective = missionObjectives[ it ];
		bool isInternal;
		const char* mapName = ugcObjectiveMapName( ugcProj, objective, &isInternal );

		if (mapName)
		{
			if( isInternal != prevIsInternal || stricmp( mapName, prevMapName ) != 0 ) {
				UGCMapTransitionInfo* transition = StructCreate_dbg( parse_UGCMapTransitionInfo, NULL MEM_DBG_PARMS_CALL );
				transition->objectiveID = objective->id;
				transition->prevMapName = StructAllocString_dbg( prevMapName, caller_fname, line );
				transition->prevIsInternal = prevIsInternal;

				eaPush_dbg( &accum, transition MEM_DBG_PARMS_CALL );

				prevTransition = transition;
			}
			else if (prevTransition)
			{
				eaiPush_dbg(&prevTransition->mapObjectiveIDs, objective->id, caller_fname, line );
			}

			prevMapName = mapName;
			prevIsInternal = isInternal;
		}

		if( objective->type == UGCOBJ_ALL_OF ) {
			int childIt;
			UGCMapTransitionInfo *backup_prevTransition = prevTransition;

			for( childIt = 0; childIt != eaSize( &objective->eaChildren ); ++childIt ) {
				const UGCMissionObjective* colObjective = objective->eaChildren[ childIt ];
				prevTransition = backup_prevTransition;

				if( colObjective->type == UGCOBJ_IN_ORDER ) {
					UGCMapTransitionInfo** colTransitions = ugcMissionGetMapTransitions1( ugcProj, colObjective->eaChildren, prevMapName, prevIsInternal, prevTransition MEM_DBG_PARMS_CALL );
					eaPushEArray_dbg( &accum, &colTransitions MEM_DBG_PARMS_CALL );
					eaDestroy( &colTransitions );
				} else {
					bool colIsInternal;
					const char* colMapName = ugcObjectiveMapName( ugcProj, colObjective, &colIsInternal );

					if (colMapName)
					{
						if( colIsInternal != prevIsInternal || stricmp( colMapName, prevMapName ) != 0 ) {
							UGCMapTransitionInfo* transition = StructCreate_dbg( parse_UGCMapTransitionInfo, NULL MEM_DBG_PARMS_CALL );
							transition->objectiveID = colObjective->id;
							transition->prevMapName = StructAllocString_dbg( prevMapName, caller_fname, line );
							transition->prevIsInternal = prevIsInternal;

							eaPush_dbg( &accum, transition MEM_DBG_PARMS_CALL );
						}
						else if (prevTransition)
						{
							eaiPush_dbg( &prevTransition->mapObjectiveIDs, colObjective->id, caller_fname, line );
						}
					}
				}
			}

			prevTransition = backup_prevTransition;
		}
	}

	return accum;
}

UGCMapTransitionInfo** ugcMissionGetMapTransitions_dbg( const UGCProjectData* ugcProj, const UGCMissionObjective** missionObjectives MEM_DBG_PARMS )
{
	return ugcMissionGetMapTransitions1( ugcProj, missionObjectives, NULL, false, NULL MEM_DBG_PARMS_CALL );
}

UGCMapTransitionInfo* ugcMissionFindTransitionForObjective( UGCMapTransitionInfo** transitions, U32 objectiveID )
{
	int it;
	for( it = 0; it != eaSize( &transitions ); ++it ) {
		if( transitions[ it ]->objectiveID == objectiveID ) {
			return transitions[ it ];
		}
	}

	return NULL;
}

bool ugcObjectiveIsCompleteMapOnMap( UGCMissionObjective* objective, char* mapName )
{
	char mapNameNoNS[ RESOURCE_NAME_MAX_SIZE ];
	resExtractNameSpace_s( mapName, NULL, 0, SAFESTR( mapNameNoNS ));
	
	return ((objective->type == UGCOBJ_TMOG_MAP_MISSION || objective->type == UGCOBJ_TMOG_REACH_INTERNAL_MAP) && (stricmp( objective->astrMapName, mapNameNoNS ) == 0));
}

bool ugcObjectiveHasID( UGCMissionObjective* objective, U32 id )
{
	return objective->id == id;
}

bool ugcObjectiveHasIDRaw( UGCMissionObjective* objective, UserData rawID )
{
	return objective->id == (U32)(uintptr_t)rawID;
}

bool ugcObjectiveHasComponent( UGCMissionObjective* objective, UserData rawComponentID )
{
	U32 componentID = (U32)(uintptr_t)rawComponentID;
	
	return ((objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR)
			&& (objective->componentID == componentID || ea32Find( &objective->extraComponentIDs, componentID ) >= 0));
}

const char* ugcMissionObjectiveUIString( const UGCMissionObjective* objective )
{
	if( !objective ) {
		return TranslateMessageKey( "UGC.Default_Objective_UIString" );
	} else if( nullStr( objective->uiString )) {
		if( objective->type == UGCOBJ_ALL_OF || objective->type == UGCOBJ_IN_ORDER ) {
			return NULL;
		} else {
			return TranslateMessageKey( "UGC.Default_Objective_UIString" );
		}
	} else {
		return objective->uiString;
	}
}

static char tempLogicalName[ 128 ];
const char* ugcMissionObjectiveLogicalNameTemp( const UGCMissionObjective* objective )
{
	sprintf( tempLogicalName, "Objective_%d", objective->id );
	return tempLogicalName;
}

const char* ugcMissionObjectiveIDLogicalNameTemp( U32 id )
{
	sprintf( tempLogicalName, "Objective_%d", id );
	return tempLogicalName;
}

const char* ugcDialogTreePromptButtonText( const UGCDialogTreePrompt* prompt, int actionIndex )
{
	if( !prompt || !eaGet( &prompt->eaActions, actionIndex ) || nullStr( prompt->eaActions[ actionIndex ]->pcText )) {
		return TranslateMessageKey( "UGC.Default_Block_Continue_Text" );
	} else {
		return prompt->eaActions[ actionIndex ]->pcText;
	}
}

const char* ugcDialogTreePromptStyle( const UGCDialogTreePrompt* prompt )
{
	if( !nullStr( prompt->pcPromptStyle )) {
		return prompt->pcPromptStyle;
	} else {
		if( ugcDefaultsDialogStyle() == UGC_DIALOG_STYLE_IN_WORLD ) {
			return NULL;
		} else {
			return "HeadshotStyle_Default";
		}
	}
}

const char* ugcMapDisplayName( const UGCMap* map )
{
	if( !map || nullStr( map->pcDisplayName )) {
		return "<Unnamed Map>";
	} else {
		return map->pcDisplayName;
	}
}

const char* ugcInteractPropsInteractText( const UGCInteractProperties* props )
{	
	if( !props || nullStr( props->pcInteractText )) {
		return TranslateMessageKey( "UGC.Default_Interact_Text" );
	} else {
		return props->pcInteractText;
	}
}

const char* ugcInteractPropsInteractFailureText( const UGCInteractProperties* props )
{	
	if( !props || nullStr( props->pcInteractFailureText )) {
		return TranslateMessageKey( "UGC.Default_Interact_Failure_Text" );
	} else {
		return props->pcInteractFailureText;
	}
}

UGCMissionMapLink *ugcMissionFindLink( UGCMission* mission, UGCComponentList *com_list, const char* nextMap, const char* prevMap )
{
	FOR_EACH_IN_EARRAY(mission->map_links, UGCMissionMapLink, link)
	{
		UGCComponent *component = ugcComponentFindByID(com_list, link->uDoorComponentID);
		bool is_external = component && component->sPlacement.bIsExternalPlacement;
		if (component && (
			(prevMap && (!is_external) && resNamespaceBaseNameEq(component->sPlacement.pcMapName, prevMap)) ||
			((!prevMap) && is_external)))
		{
			if ((nextMap && resNamespaceBaseNameEq(link->strSpawnInternalMapName, nextMap)) ||
				(!nextMap && !link->strSpawnInternalMapName))
			{
				return link;
			}
		}
	}
	FOR_EACH_END;
	return NULL;
}

UGCMissionMapLink *ugcMissionFindLinkByObjectiveID( UGCProjectData* ugcProj, U32 objectiveID, bool allMapObjectives )
{
	UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );
	int it;
	for( it = 0; it != eaSize( &transitions ); ++it ) {
		UGCMapTransitionInfo* transition = transitions[ it ];
		bool found = false;
		if (allMapObjectives)
		{
			if (eaiFind(&transition->mapObjectiveIDs, objectiveID) != -1)
				found = true;
		}
		if( found || transition->objectiveID == objectiveID ) {
			UGCMissionObjective* objective = ugcObjectiveFind( ugcProj->mission->objectives, objectiveID );
			bool nextIsInternal;
			const char* nextMapName = ugcObjectiveMapName( ugcProj, objective, &nextIsInternal );

			UGCMissionMapLink *link = ugcMissionFindLink( ugcProj->mission, ugcProj->components,
														  (nextIsInternal ? nextMapName : NULL),
														  (transition->prevIsInternal ? transition->prevMapName : NULL) );
			
			eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
			return link;
		}
	}

	eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
	return NULL;
}

UGCMissionMapLink *ugcMissionFindLinkByExitComponent( UGCProjectData* ugcProj, U32 componentID )
{
	int it;
	for( it = 0; it != eaSize( &ugcProj->mission->map_links ); ++it ) {
		UGCMissionMapLink* mapLink = ugcProj->mission->map_links[ it ];

		if( mapLink->uDoorComponentID == componentID ) {
			return mapLink;
		}
	}
	if( ugcProj->mission->return_map_link && ugcProj->mission->return_map_link->uDoorComponentID == componentID ) {
		return ugcProj->mission->return_map_link;
	}
	
	return NULL;
}

UGCMissionMapLink *ugcMissionFindLinkByMap( UGCProjectData* ugcProj, const char* map )
{
	UGCMissionObjective** mapObjectives = NULL;
	ugcMissionTransmogrifyObjectives( ugcProj, ugcProj->mission->objectives, map, true, &mapObjectives );
	if( !mapObjectives ) {
		return NULL;
	}

	{
		UGCMissionMapLink* link = ugcMissionFindLinkByObjectiveID( ugcProj, mapObjectives[ 0 ]->id, true );
		eaDestroyStruct( &mapObjectives, parse_UGCMissionObjective );
		return link;
	}
}

UGCMissionMapLink *ugcMissionFindLinkByExitMap( UGCProjectData* ugcProj, const char* map )
{
	int it;
	for( it = 0; it != eaSize( &ugcProj->mission->map_links ); ++it ) {
		UGCMissionMapLink* mapLink = ugcProj->mission->map_links[ it ];
		UGCComponent* component = ugcComponentFindByID( ugcProj->components, mapLink->uDoorComponentID );

		if(   component && !component->sPlacement.bIsExternalPlacement
			  && resNamespaceBaseNameEq( component->sPlacement.pcMapName, map )) {
			return mapLink;
		}
	}
	if( ugcProj->mission->return_map_link ) {
		UGCMissionMapLink* mapLink = ugcProj->mission->return_map_link;
		UGCComponent* component = ugcComponentFindByID( ugcProj->components, mapLink->uDoorComponentID );

		if(   component && !component->sPlacement.bIsExternalPlacement
			  && resNamespaceBaseNameEq( component->sPlacement.pcMapName, map )) {
			return mapLink;
		}
	}
	return NULL;
}

UGCMissionMapLink *ugcMissionFindCrypticSourceLink( UGCProjectData* ugcProj, UGCMissionMapLink* link )
{
	while( link ) {
		UGCComponent* door = ugcComponentFindByID( ugcProj->components, link->uDoorComponentID );
		if( !door ) {
			return NULL;
		} else if( door->sPlacement.bIsExternalPlacement ) {
			return link;
		} else {
			const char* mapName = door->sPlacement.pcMapName;
			link = ugcMissionFindLinkByMap( ugcProj, mapName );
		}
	}
	
	return NULL;
}

const char* ugcLinkButtonText( UGCMissionMapLink* link )
{
	UGCPerProjectDefaults* config = ugcGetDefaults();
	UGCDialogTreePrompt* prompt = SAFE_MEMBER( link, pDialogPrompt ) ? link->pDialogPrompt : config->pDefaultTransitionPrompt;

	if( prompt ) {
		if( eaSize( &prompt->eaActions ) > 0 && !nullStr( prompt->eaActions[ 0 ]->pcText )) {
			return prompt->eaActions[ 0 ]->pcText;
		}
	} else {
		if( !nullStr( SAFE_MEMBER( link, strInteractText ))) {
			return link->strInteractText;
		}
	}

	return "Go to Next Map";
}

UGCComponent *ugcMissionGetDefaultComponentForMap(UGCProjectData *ugcProj, UGCComponentType type, const char *map_name)
{
	if( map_name && resNamespaceIsUGC( map_name )) {
		UGC_FOR_EACH_COMPONENT_OF_TYPE(ugcProj->components, type, component)
		{
			if (resNamespaceBaseNameEq(component->sPlacement.pcMapName, map_name))
				return component;
		}
		UGC_FOR_EACH_COMPONENT_END;
	} else {
		// Cryptic map
		if( !map_name ) {
			map_name = ugcGetDefaultMapName( ugcProj );
		}
		
		if( type == UGC_COMPONENT_TYPE_WHOLE_MAP ) {
			return ugcComponentOpExternalObjectFind( ugcProj, map_name, "WHOLE_MAP" );
		} else if( type == UGC_COMPONENT_TYPE_SPAWN ) {
			return ugcComponentOpExternalObjectFind( ugcProj, map_name, "MISSION_RETURN" );
		}
	}
	return NULL;
}

void ugcObjectiveGenerateIDs( UGCProjectData* ugcProj, U32* out_ids, int numIds )
{
	int it = 0;
	U32 id = 0;

	for( it = 0; it != numIds; ++it ) {
		do {
			++id;
		} while( ugcObjectiveFind( ugcProj->mission->objectives, id ));
		out_ids[ it ] = id;
	}
}

#include "AutoGen/NNOUGCMissionCommon_h_ast.c"

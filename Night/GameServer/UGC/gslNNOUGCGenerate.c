/***************************************************************************
 
 
 
 *
 *	Requirements:
 *		1) Generation should handle ALL input UGC project data gracefully,
 *			with no 'Errorf's or crashes.
 *		2) When given input that had validation errors ('Tasks' in the
 *			client UI), generate resources that allow the player to preview
 *			the project as best as possible.
 *
 *			a) For example, when an Objective
 *				with an unplaced Object is encountered, the generate creates a
 *				placeholder Objective that is completable without requiring the
 *				Object to be placed.
 *			b) If it is not possible to do so, generate should raise a
 *				UGC Error (via ugcRaiseError), which blocks Preview.
 *				Up until now, this has not been necessary.
 *			c) UGCGenesis itself does raise errors, however, UGC Generate should
 *				not give data to UGCGenesis that does so.
 *		3) The input project data should never be modified during generate.
 *
 *	The entry point to the generate algorithm is ugcProjectGenerateOnServer.
 *  Per-map 'pieces' go through ugcMissionGenerateForMap and cross-map 'pieces'
 *	go through ugcMissionGenerate.
 *
 *	Jared says these design requirements have not changed since before 2010, if ever.
***************************************************************************/

#include "CostumeCommon.h"
#include "CostumeCommonLoad.h"
#include "error.h"
#include "gslNNOUGCGenesisMissions.h"
#include "gslSpawnPoint.h"
#include "gslUGC.h"
#include "gslNNOUGC.h"
#include "ItemCommon.h"
#include "itemCommon_h_ast.h"
#include "ItemEnums.h"
#include "itemEnums_h_ast.h"
#include "mission_common.h"
#include "rand.h"
#include "quat.h"
#include "RewardCommon.h"
#include "StateMachine.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "NNOUGCInteriorCommon.h"
#include "UGCInteriorCommon.h"
#include "NNOUGCMissionCommon.h"
#include "wlExclusionGrid.h"
#include "gslNNOUGCGenesisStructs.h"
#include "../../libs/WorldLib/wlState.h"
#include "wlUGC.h"
#include "WorldColl.h"
#include "../../libs/WorldLib/StaticWorld/WorldGridPrivate.h"
#include "UGCError.h"
#include "NNOUGCResource.h"
#include "wlSky.h"
#include "NNOUGCCommon.h"
#include "Powers.h"
#include "GameServerLib.h"

#include "AutoGen/CostumeCommon_h_ast.h"

static bool g_UGCWriteOutMapDesc = false;

// In this file
void ugcGenerateLayer(ZoneMapInfo *zmap_info, UGCGenesisMissionRequirements **mission_reqs, ZoneMapLayer *layer, UGCGenesisZoneShared *space, bool *is_space);
void ugcGeneratePathNodesLayer( ZoneMapLayer* layer, UGCGenesisZoneShared* gen_data );
void ugcGenerateSpaceMiniLayer(ZoneMapInfo *zmap_info, UGCGenesisMissionRequirements **mission_reqs, ZoneMapLayer *layer, UGCGenesisZoneShared *data);
void ugcPlaceObjects(ZoneMapInfo *zmap_info, UGCGenesisToPlaceState *to_place, GroupDef *root_def);

// In gslUGCGenerateMission
void ugcMissionGenerate( UGCProjectData* ugcProj );
void ugcMissionGenerateForMap(UGCProjectData* ugcProj, const char *raw_map_name, UGCGenesisMapDescription* out_mapDesc, Vec3 override_spawn_pos, Vec3 override_spawn_rot);

static bool isZeroVec3( Vec3 vec )
{
	return vec[0] == 0 && vec[1] == 0 && vec[2] == 0;
}

////////////////////////////////////////////////////////////
// Transmogrify
////////////////////////////////////////////////////////////

static void ugcTransmogrifyChallenge(UGCGenesisZoneShared *concrete, UGCGenesisMapDescription *map_desc, int mission_num, UGCGenesisMissionChallenge *challenge)
{
	GroupDef *object_def = NULL;
	const char *mission_name;
	char fullChallengeName[ 256 ];
	UGCGenesisObject ***object_list = NULL;

	// Skip this challenge -- it's in a different layout
	if (stricmp(challenge->pcLayoutName, concrete->layout_name) != 0) {
		return;
	}

	if (mission_num >= 0)
		mission_name = SAFE_MEMBER(map_desc->missions[mission_num], zoneDesc.pcName);
	else
		mission_name = NULL;

	if( !nullStr( challenge->pcChallengeName )) {
		object_def = objectLibraryGetGroupDefByName(challenge->pcChallengeName, false);
		if (!object_def)
		{
			ugcRaiseErrorContext(UGC_ERROR, ugcMakeTempErrorContextChallenge( challenge->pcName, mission_name, challenge->pcLayoutName ),
								 "Could not find object for challenge." );
			return;
		}
	}

	if (mission_name)
	{
		sprintf( fullChallengeName, "%s_%s", mission_name, challenge->pcName );
		FOR_EACH_IN_EARRAY(concrete->missions, GenesisZoneSharedUGCMission, mission)
		{
			if (stricmp(mission->mission_name, mission_name) == 0)
			{
				object_list = &mission->objects;
				break;
			}
		}
		FOR_EACH_END;
		if (!object_list)
		{
			GenesisZoneSharedUGCMission *new_mission = StructCreate(parse_GenesisZoneSharedUGCMission);
			new_mission->mission_name = StructAllocString(mission_name);
			object_list = &new_mission->objects;
			eaPush(&concrete->missions, new_mission);
		}
	}
	else
	{
		sprintf( fullChallengeName, "Shared_%s", challenge->pcName );
		object_list = &concrete->objects;
	}

	{
		UGCGenesisObject *object = StructCreate(parse_UGCGenesisObject);
		if( object_def ) {
			object->obj.name_str = StructAllocString(object_def->name_str);
			object->obj.name_uid = objectLibraryUIDFromObjName(object->obj.name_str);
		}
		object->challenge_type = challenge->eType;
		object->challenge_name = StructAllocString(fullChallengeName);
		object->platform_group = challenge->iPlatformGroup;
		object->platform_parent_group = challenge->iPlatformParentGroup;
		object->platform_parent_level = challenge->iPlatformParentLevel;
		eaCopyStructs(&challenge->eaChildParams, &object->eaChildParams, parse_UGCGenesisGroupDefChildParam);
		eaCopyStructs(&challenge->eaRoomDoors, &object->eaRoomDoors, parse_UGCGenesisRoomDoorSwitch);
		object->force_named_object = challenge->bForceNamedObject;
		object->spawn_point_name = StructAllocString(challenge->pcStartSpawnName);
		object->is_trap = (eaSize(&challenge->eaTraps) > 0);
		copyVec3(challenge->vPosition, object->params.position);
		copyVec3(challenge->vRotation, object->params.rotation);
		object->params.bAbsolutePos = challenge->bAbsolutePos;
		object->params.bSnapToGeo = challenge->bSnapToGeo;
		object->params.bSnapRayCast = challenge->bSnapRayCast;
		object->params.bSnapNormal = challenge->bSnapNormal;
		object->params.bLegacyHeightCheck = challenge->bLegacyHeightCheck;
		object->ePathNodesFrom = challenge->ePathNodesFrom;
		FOR_EACH_IN_EARRAY_FORWARDS( challenge->eastrPathNodesAutoconnectChallenge, char, str ) {
			eaPush( &object->eastrPathNodesAutoconnectChallenge, StructAllocString( str ));
		} FOR_EACH_END;
		object->bPathNodesAutoconnectNearest = challenge->bPathNodesAutoconnectNearest;

		if (challenge->pPatrol)
			object->patrol_specified = StructClone(parse_WorldPatrolProperties, challenge->pPatrol);
		if (challenge->pVolume)
			object->volume = StructClone(parse_UGCGenesisObjectVolume, challenge->pVolume);
		if(challenge->eaChildren) {
			int it;
			for( it = 0; it != eaSize( &challenge->eaChildren ); ++it ) {
				UGCGenesisPlacementChildParams* newChildParam = StructClone( parse_UGCGenesisPlacementChildParams, challenge->eaChildren[ it ]);
				eaPush( &object->params.children, newChildParam );
				
				if( !nullStr( newChildParam->pcLogicalName )) {
					char fullLogicalName[ 256 ];
					if( mission_name ) {
						sprintf( fullLogicalName, "%s_%s", mission_name, newChildParam->pcLogicalName );
					} else {
						sprintf( fullLogicalName, "Shared_%s", newChildParam->pcLogicalName );
					}
					StructCopyString( &newChildParam->pcLogicalName, fullLogicalName );
				}
			}
		}
		object->source_context = ugcMakeErrorContextChallenge(challenge->pcName, mission_name, concrete->layout_name);

		if(!nullStr(challenge->astrRoomToneSoundEvent))
		{
			UGCGenesisGroupDefChildParam *child_param = StructCreate(parse_UGCGenesisGroupDefChildParam);
			child_param->astrParameter = allocAddString("Room_Tone");
			child_param->astrValue = challenge->astrRoomToneSoundEvent;
			eaPush(&object->eaChildParams, child_param);
		}

		if(!nullStr(challenge->astrRoomSoundDSP))
		{
			UGCGenesisGroupDefChildParam *child_param = StructCreate(parse_UGCGenesisGroupDefChildParam);
			child_param->astrParameter = allocAddString("Room_DSP");
			child_param->astrValue = challenge->astrRoomSoundDSP;
			eaPush(&object->eaChildParams, child_param);
		}

		if(!nullStr(challenge->astrObjectSoundEvent))
			object->astrObjectSoundEvent = challenge->astrObjectSoundEvent;

		eaPush(object_list, object);
	}
}

static void ugcTransmogrifyBackdrop(UGCGenesisZoneShared *concrete, UGCGenesisBackdrop *ugc_backdrop)
{
	if( !concrete->sky_group ) {
		concrete->sky_group = StructCreate( parse_SkyInfoGroup );
	}
	StructReset( parse_SkyInfoGroup, concrete->sky_group );

	if( IS_HANDLE_ACTIVE( ugc_backdrop->hSkyBase )) {
		SkyInfoOverride* skyInfoOverride = StructCreate( parse_SkyInfoOverride );
		COPY_HANDLE( skyInfoOverride->sky, ugc_backdrop->hSkyBase );
		eaPush( &concrete->sky_group->override_list, skyInfoOverride );
	}
	FOR_EACH_IN_EARRAY_FORWARDS( ugc_backdrop->eaSkyOverrides, UGCGenesisBackdropSkyOverride, skyOverride ) {
		if( IS_HANDLE_ACTIVE( skyOverride->hSkyOverride )) {
			SkyInfoOverride* skyInfoOverride = StructCreate( parse_SkyInfoOverride );
			COPY_HANDLE( skyInfoOverride->sky, skyOverride->hSkyOverride );
			eaPush( &concrete->sky_group->override_list, skyInfoOverride );
		}
	} FOR_EACH_END;

	if (!nullStr(ugc_backdrop->strAmbientSoundOverride))
	{
		UGCSound* sound = RefSystem_ReferentFromString( "UGCSound", ugc_backdrop->strAmbientSoundOverride );
		StructCopyString( &concrete->amb_sound, SAFE_MEMBER( sound, strSoundName ));
	}
}

void ugcTransmogrifySpaceMap(U32 seed, U32 detail_seed, UGCGenesisMapDescription *map_desc, UGCGenesisZoneShared *concrete)
{
	int mission_idx, challenge_idx;
	UGCGenesisSpace *vague = map_desc->space_ugc;

	concrete->layout_name = StructAllocString(GENESIS_UGC_LAYOUT_NAME);

	for (mission_idx = 0; mission_idx < eaSize(&map_desc->missions); mission_idx++)
	{
		UGCGenesisMissionDescription *mission = map_desc->missions[mission_idx];
		for (challenge_idx = 0; challenge_idx < eaSize(&mission->eaChallenges); challenge_idx++)
		{
			ugcTransmogrifyChallenge(concrete, map_desc, mission_idx, mission->eaChallenges[challenge_idx]);
		}
	}
	for (challenge_idx = 0; challenge_idx < eaSize(&map_desc->shared_challenges); challenge_idx++)
	{
		ugcTransmogrifyChallenge(concrete, map_desc, -1, map_desc->shared_challenges[challenge_idx]);
	}

	ugcTransmogrifyBackdrop(concrete, &vague->backdrop);

	concrete->external_map_name = "Ugc_Empty_Space_Map";
}


void ugcTransmogrifyPrefabMap(U32 seed, U32 detail_seed, UGCGenesisMapDescription *map_desc, UGCGenesisZoneShared *concrete)
{
	int mission_idx, challenge_idx;
	UGCGenesisPrefab *vague = map_desc->prefab_ugc;
	ZoneMapEncounterRegionInfo *zeni_region;

	concrete->layout_name = StructAllocString(GENESIS_UGC_LAYOUT_NAME);

	for (mission_idx = 0; mission_idx < eaSize(&map_desc->missions); mission_idx++)
	{
		UGCGenesisMissionDescription *mission = map_desc->missions[mission_idx];
		for (challenge_idx = 0; challenge_idx < eaSize(&mission->eaChallenges); challenge_idx++)
		{
			ugcTransmogrifyChallenge(concrete, map_desc, mission_idx, mission->eaChallenges[challenge_idx]);
		}
	}
	for (challenge_idx = 0; challenge_idx < eaSize(&map_desc->shared_challenges); challenge_idx++)
	{
		ugcTransmogrifyChallenge(concrete, map_desc, -1, map_desc->shared_challenges[challenge_idx]);
	}

	ugcTransmogrifyBackdrop(concrete, &vague->backdrop);

	concrete->external_map_name = vague->map_name;
	zeni_region = ugcGetZoneMapDefaultRegion(vague->map_name);
	concrete->external_region_name = zeni_region ? zeni_region->regionName : NULL;
}

bool ugcTransmogrifyMapDesc(ZoneMapInfo *zminfo, UGCGenesisZoneMapData* data, UGCRuntimeStatus *gen_status)
{
	UGCGenesisMapDescription *map_desc = SAFE_MEMBER(data, map_desc);
	int i, j;

	if(	!data || !map_desc)
		return false;

	ugcSetStageAndAdd(gen_status, "Transmogrifier");
	
	{
		UGCGenesisMissionZoneChallenge **zone_challenges = NULL;
		UGCGenesisZoneMission **zone_missions = NULL;
		UGCGenesisProceduralEncounterProperties **encounter_properties = NULL;
		for(i = 0; i != eaSize(&map_desc->missions); ++i)
		{
			UGCGenesisZoneMission* zone_mission;
			
			zone_mission = ugcGenesisTransmogrifyMission(zminfo, map_desc, i );
			if( zone_mission ) {
				eaPush( &zone_missions, zone_mission );
			}
			for(j = 0; j != eaSize(&map_desc->missions[i]->eaChallenges); ++j)
			{
				ugcGenesisTransmogrifyChallengePEPHack( map_desc, i, map_desc->missions[i]->eaChallenges[j], &encounter_properties);
			}
		}
		
		zone_challenges = ugcGenesisTransmogrifySharedChallenges(zminfo, map_desc);
		for( i = 0; i != eaSize( &map_desc->shared_challenges ); ++i ) {
			ugcGenesisTransmogrifyChallengePEPHack( map_desc, -1, map_desc->shared_challenges[ i ], &encounter_properties );
		}

		data->genesis_mission = zone_missions;
		data->genesis_shared_challenges = zone_challenges;
		data->encounter_overrides = encounter_properties;
	}

	data->is_map_tracking_enabled = map_desc->is_tracking_enabled;
	
	eaDestroyStruct(&data->genesis_shared, parse_UGCGenesisZoneShared);
	if (map_desc->space_ugc)
	{
		UGCGenesisZoneShared *shared = StructCreate(parse_UGCGenesisZoneShared);
		eaPush(&data->genesis_shared, shared);
		ugcTransmogrifySpaceMap(data->seed, data->detail_seed, map_desc, shared);
	}
	if (map_desc->prefab_ugc)
	{
		UGCGenesisZoneShared *shared = StructCreate(parse_UGCGenesisZoneShared);
		eaPush(&data->genesis_shared, shared);
		ugcTransmogrifyPrefabMap(data->seed, data->detail_seed, map_desc, shared);
	}

	ugcClearStage();

	// Detect failure
	if (ugcStatusFailed(gen_status))
		return false;
	return true;
}

#if 0
// WOLF[23Jul12] Fog was poorly implemented. See [COR-16358] Commenting out for now

////////////////////////////////////////////////////////////
// Backdrop generation
////////////////////////////////////////////////////////////

AUTO_STRUCT AST_STARTTOK("") AST_ENDTOK(EndSkyFog) AST_STRIP_UNDERSCORES;
typedef struct FakeSkyTimeFog
{
	Vec3	lowFogColorHSV;			AST( NAME(FogColorHSV) FORMAT_HSV )
	Vec3	highFogColorHSV;		AST( NAME(HighFogColorHSV) FORMAT_HSV )
	Vec3	clipFogColorHSV;		AST( NAME(ClipFogColorHSV) FORMAT_HSV )
	Vec3	clipBackgroundColorHSV;	AST( NAME(ClipBackgroundColorHSV) FORMAT_HSV )
	F32		clipFogDistanceAdjust;	AST( NAME(ClipFogDistanceAdjust) )
	Vec2	lowFogDist;				AST( NAME(FogDist) )
	Vec2	highFogDist;			AST( NAME(HighFogDist) )
	F32		lowFogMax;				AST( NAME(FogMax) )
	F32		highFogMax;				AST( NAME(HighFogMax) )
	Vec2	fogHeight;				AST( NAME(FogHeight) )
	
	Vec2	fogDensity;				AST( NAME(FogDensity) )
	Vec3	volumeFogPos;			AST( NAME(VolumeFogPos) )
	Vec3	volumeFogScale;			AST( NAME(VolumeFogScale) )
} FakeSkyTimeFog;
extern ParseTable parse_FakeSkyTimeFog[];
#define TYPE_parse_FakeSkyTimeFog FakeSkyTimeFog

// This struct shadows SkyInfo in GfxSky.h. It's a method to carry the fog values into a fullfledged skyInfo.
//  We need to make sure bFilenameNoPathShouldNotBeOverwritten gets set so that filename_no_path stays intact.
AUTO_STRUCT AST_STARTTOK("") AST_ENDTOK(EndSky) AST_STRIP_UNDERSCORES;
typedef struct FakeSkyInfo
{
	const char	*filename;							AST( NAME(FN) CURRENTFILE) // Must be first for ParserReloadFile
	const char	*filename_no_path;					AST( NAME(FNNoPath) KEY POOL_STRING )
	FakeSkyTimeFog **fogValues;						AST( NAME(SkyFog) )
	bool bFilenameNoPathShouldNotBeOverwritten;		AST( NAME(FNNoPathShouldNotBeOverwritten) )		// filename_no_path contains the true reference name and
																									// should not be overwritten by filename in gfxSkyFixupFunc
																									// in GfxSky.c. Makes UGC FakeSky stuff not error on preview
																									// (see COR-16241)
} FakeSkyInfo;
extern ParseTable parse_FakeSkyInfo[];
#define TYPE_parse_FakeSkyInfo FakeSkyInfo

AUTO_STRUCT;
typedef struct FakeSkyInfoList
{
	FakeSkyInfo **skies;					AST(NAME(SkyInfo))
} FakeSkyInfoList;
extern ParseTable parse_FakeSkyInfoList[];
#define TYPE_parse_FakeSkyInfoList FakeSkyInfoList

void ugcGenerateBackdrop(ZoneMapInfo *zminfo, const char *project_prefix, UGCGenesisBackdrop *ugc_backdrop)
{
	if (ugc_backdrop)
	{
		char *estr = NULL;
		char ns[RESOURCE_NAME_MAX_SIZE], map_name[RESOURCE_NAME_MAX_SIZE];
		char path[MAX_PATH], name[RESOURCE_NAME_MAX_SIZE];
		FakeSkyInfo *new_info = StructCreate(parse_FakeSkyInfo);
		FakeSkyInfoList *list = StructCreate(parse_FakeSkyInfoList);
		FakeSkyTimeFog *fog;

		// Generate a fog sky layer
		if (resExtractNameSpace(zmapInfoGetPublicName(zminfo), ns, map_name))
		{
			sprintf(path, "ns/%s/Maps/%s/%s/Sky/%s_FogLayer.sky", ns, project_prefix, map_name, map_name);
			sprintf(name, "%s:%s_%s_FogLayer", ns, project_prefix, map_name);
		}
		else
		{
			sprintf(path, "Maps/%s/%s/Sky/%s_FogLayer.sky", project_prefix, zmapInfoGetPublicName(zminfo), zmapInfoGetPublicName(zminfo));
			sprintf(name, "%s_%s_FogLayer", project_prefix, zmapInfoGetPublicName(zminfo));
		}
		new_info->filename_no_path = allocAddString(name);
		new_info->bFilenameNoPathShouldNotBeOverwritten=true; // Prevents gfxSkyFixupFunc from overwriting our no_path name and causing an error in UGC Preview

		fog = StructCreate(parse_FakeSkyTimeFog);
		copyVec3(ugc_backdrop->sCustomFog.vFogColor, fog->highFogColorHSV);
		copyVec3(ugc_backdrop->sCustomFog.vFogColor, fog->clipFogColorHSV);
		copyVec3(ugc_backdrop->sCustomFog.vFogColor, fog->clipBackgroundColorHSV);
		setVec2(fog->highFogDist, ugc_backdrop->sCustomFog.fNearDist, ugc_backdrop->sCustomFog.fFarDist);
		fog->highFogMax = 1.f;

		setVec3(fog->lowFogColorHSV, 0, 0, 0);
		setVec2(fog->lowFogDist, 1000, 1000);
		fog->lowFogMax = 0.f;
		setVec2(fog->fogHeight, -10000, -10000);
		setVec3(fog->volumeFogScale, 1e7, 1e7, 1e7);
		eaPush(&new_info->fogValues, fog);

		eaPush(&list->skies, new_info);

		ParserWriteTextFile(path, parse_FakeSkyInfoList, list, 0, 0);

		estrCreate(&estr);
		ParserWriteText(&estr, parse_FakeSkyInfo, new_info, 0, 0, 0);
		ugcLayerCacheWriteSky(estr);
		estrDestroy(&estr);
	}
}
#endif
////////////////////////////////////////////////////////////
// Geometry generation
////////////////////////////////////////////////////////////

ZoneMapLayer *ugcGenerateMakeSingleLayer(ZoneMapInfo *zminfo, const char *layer_name, const char* region_name)
{
	char temp_str[MAX_PATH], *dir_start;
	char layer_filename[MAX_PATH];
	ZoneMapLayer *output_layer;
	ZoneMapLayerInfo *layer_info;

	strcpy(temp_str, zmapInfoGetFilename(zminfo));
	dir_start = (temp_str[0] == '/') ? &temp_str[1] : &temp_str[0];
	sprintf(layer_filename, "%s/%s.layer", getDirectoryName(dir_start), layer_name);

	output_layer = StructCreate(parse_ZoneMapLayer);
	output_layer->filename = allocAddFilename(layer_filename);
	output_layer->zmap_parent = NULL;

	sprintf(layer_filename, "%s.layer", layer_name);
	output_layer->name = strdup(layer_filename);

	createLayerData(output_layer, true);
	layerResetRootGroupDef(output_layer, false);
	output_layer->terrain.exclusion_version = EXCLUSION_SIMPLE;
	output_layer->terrain.color_shift = 0.0f;
	output_layer->grouptree.unsaved_changes = true;

	output_layer->layer_mode = LAYER_MODE_EDITABLE;
	output_layer->locked = 3;

	layer_info = StructCreate(parse_ZoneMapLayerInfo);
	layer_info->filename = allocAddFilename(layer_filename);
	layer_info->region_name = allocAddString(region_name);
	eaPush(&zminfo->layers, layer_info);

	return output_layer;
}

void ugcGenerateGeometry(ZoneMapInfo *zminfo, const char *project_prefix, UGCGenesisZoneMapData *data, UGCGenesisBackdrop *ugc_backdrop, UGCGenesisMissionRequirements** mission_reqs)
{
	UGCGenesisZoneShared *ugc_data = data->genesis_shared[0];
	ZoneMapLayer *layer;
	WorldRegion *region;
	bool is_space = false;
	char layer_name[256];
	SecondaryZoneMap *zmap_ref;
	ZoneMapInfo *external_zminfo;

	assert(eaSize(&data->genesis_shared) == 1);
	assert(ugc_data->external_map_name);

	{
		ZoneMapEncounterRegionInfo *default_region = ugcGetZoneMapDefaultRegion(ugc_data->external_map_name);
		assert(default_region);

		if (default_region->regionName != NULL)
		{
			// Create default region, to prevent errors
			eaPush(&zminfo->regions, StructCreate(parse_WorldRegion));
		}

		region = StructCreate(parse_WorldRegion);
		region->name = allocAddString(default_region->regionName);
		region->sky_group = StructClone(parse_SkyInfoGroup, ugc_data->sky_group);
		eaPush(&zminfo->regions, region);
	}

	sprintf(layer_name, "Default");
	layer = ugcGenerateMakeSingleLayer(zminfo, layer_name, region->name);

#if 0
// WOLF[23Jul12] Fog was poorly implemented. See [COR-16358] Commenting out for now
	if (ugc_backdrop)
	{
		// Add fog sky
		char sky_name[RESOURCE_NAME_MAX_SIZE];
		char ns[RESOURCE_NAME_MAX_SIZE], map_name[RESOURCE_NAME_MAX_SIZE];
		SkyInfoOverride *override = StructCreate(parse_SkyInfoOverride);

		if (resExtractNameSpace(zmapInfoGetPublicName(zminfo), ns, map_name))
		{
			sprintf(sky_name, "%s:%s_%s_FogLayer", ns, project_prefix, map_name);
		}
		else
			sprintf(sky_name, "%s_%s_FogLayer", project_prefix, zmapInfoGetPublicName(zminfo));
		SET_HANDLE_FROM_STRING("SkyInfo", sky_name, override->sky);
		eaPush(&region->sky_group->override_list, override);
	}
#endif	

	external_zminfo = worldGetZoneMapByPublicName(ugc_data->external_map_name);
	assert(external_zminfo);
	region->type = external_zminfo->regions[0]->type; // TomY TODO - should come from the default region above, but it's not currently cached

	zmap_ref = StructCreate(parse_SecondaryZoneMap);
	zmap_ref->map_name = StructAllocString(ugc_data->external_map_name);
	eaPush(&zminfo->secondary_maps, zmap_ref);

	// Copy over secondary maps
	{
		SecondaryZoneMap **secondary_maps = ugcGetZoneMapSecondaryMaps(ugc_data->external_map_name);
		int i;
		for(i = 0; i < eaSize(&secondary_maps); i++)
			eaPush(&zminfo->secondary_maps, StructClone(parse_SecondaryZoneMap, secondary_maps[i]));
	}

	ugcGenerateLayer(zminfo, mission_reqs, layer, ugc_data, &is_space);

	layerSaveAs(layer, layer->filename, true, false, false, true);
	ugcLayerCacheWriteLayer(layer);
	layerUnload(layer);
	StructDestroy(parse_ZoneMapLayer, layer);

	if (is_space)
	{
		DependentWorldRegion *dependant = StructCreate(parse_DependentWorldRegion);
		WorldRegion *minimap_region;

		//Overview Layer
		sprintf(layer_name, "UGC_SpaceMinimap");
		layer = ugcGenerateMakeSingleLayer(zminfo, layer_name, layer_name);

		minimap_region = StructCreate(parse_WorldRegion);
		minimap_region->name = allocAddString(layer_name);
		minimap_region->type = WRT_SystemMap;

		dependant->name = allocAddString(layer_name);
		dependant->hidden_object_id = 1;
		eaPush(&region->dependents, dependant);

		eaPush(&zminfo->regions, minimap_region);

		ugcGenerateSpaceMiniLayer(zminfo, mission_reqs, layer, ugc_data);

		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": About to save layer, file=%s", layer->filename ));
		layerSaveAs(layer, layer->filename, true, false, false, true);
		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": Layer save done, file=%s", layer->filename ));
		ugcLayerCacheWriteLayer(layer);
		layerUnload(layer);
		StructDestroy(parse_ZoneMapLayer, layer);
	}

	if( ugcDefaultsIsPathNodesEnabled() ) {
		sprintf(layer_name, "Golden_Path");
		layer = ugcGenerateMakeSingleLayer(zminfo, layer_name, region->name);

		ugcGeneratePathNodesLayer( layer, ugc_data );

		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": About to save layer, file=%s", layer->filename ));
		layerSaveAs( layer, layer->filename, true, false, false, true );
		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": Layer save done, file=%s", layer->filename ));
		ugcLayerCacheWriteLayer( layer );
		layerUnload( layer );
		StructDestroy( parse_ZoneMapLayer, layer );
	}
}

static HeightMapExcludeGrid *g_PrefabPlatformGrid = NULL;

F32 ugcPopulateGetTerrainHeight(int iPartitionIdx, Vec3 pos, bool snap_to_geo, bool bLegacyHeightCheck, F32 default_height, Vec3 normal)
{
	const WorldUGCProperties* props = ugcResourceGetUGCProperties( "ZoneMap", zmapInfoGetPublicName( NULL ));
	WorldCollCollideResults results = { 0 };
	bool allow_terrain = !SAFE_MEMBER( props, bMapOnlyPlatformsAreLegal );
	
	if(normal) // default the normal
	{
		normal[0] = 0;
		normal[1] = 1;
		normal[2] = 0;
	}

	if (g_PrefabPlatformGrid)
	{
		// Do a collision check against the grid
		static int test_num = 0;
		F32* heights = NULL;
		F32* normals = NULL;
		Mat4 item_matrix;
		identityMat4(item_matrix);
		item_matrix[3][0] = pos[0];
		item_matrix[3][2] = pos[2];
		exclusionCalculateObjectHeight(g_PrefabPlatformGrid, item_matrix, 100, test_num++, true, iPartitionIdx, &heights, &normals);
		if (eafSize(&heights) != 0)
		{
			results.hitSomething = true;
			results.posWorldImpact[1] = heights[0];
			if( eafSize( &normals ) >= 3 ) {
				copyVec3( normals, results.normalWorld );
			} else {
				setVec3( results.normalWorld, 0, 1, 0 );
			}
		}
		eafDestroy(&heights);
		eafDestroy(&normals);
	}
	if (!results.hitSomething && allow_terrain)
	{
		Vec3 source, target;
		copyVec3(pos, source);
		source[1] = 5000;
		copyVec3(source, target);

		// All projects prior to STO Season 6 (Release 5) used
		//   the 0.0f as the target height. There was a bug
		//   fix to extend the height below ground, but
		//   we need to keep the old height check around
		//   so that placement in older projects is not affected.
		// bLegacyHeightCheck should be exposed through a different
		//   Y Placement Object on the object itself.
		//   Look for eSnap and UGCComponentHeightSnap
		// Note we EITHER look for WORLD_NORMAL or TERRAIN. Never both
		
		if (bLegacyHeightCheck)
		{
			target[1] = 0;
		}
		else
		{
			target[1] = -5000;
		}
		wcRayCollide(worldGetActiveColl(iPartitionIdx), source, target, snap_to_geo ? WC_FILTER_BIT_WORLD_NORMAL : WC_FILTER_BIT_TERRAIN, &results);
	}
	if (results.hitSomething)
	{
		if(normal)
			copyVec3(results.normalWorld, normal);
		return results.posWorldImpact[1];
	}
	return default_height;
}

static UGCGenesisInstancedObjectParams* ugcGenesisFindMissionChallengeInstanceParams(UGCGenesisMissionRequirements* missionReq, const char* challenge_name)
{
	UGCGenesisMissionChallengeRequirements* challengeReq = NULL;

	if( !missionReq ) {
		return NULL;
	}

	{
		int it;
		for( it = 0; it != eaSize( &missionReq->challengeRequirements ); ++it ) {
			if( stricmp( missionReq->challengeRequirements[ it ]->challengeName, challenge_name ) == 0 ) {
				challengeReq = missionReq->challengeRequirements[ it ];
				break;
			}
		}
	}
	if( !challengeReq ) {
		return NULL;
	}

	return challengeReq->params;
}

static UGCGenesisInteractObjectParams* ugcGenesisFindMissionChallengeInteractParams(UGCGenesisMissionRequirements* missionReq, const char* challenge_name)
{
	UGCGenesisMissionChallengeRequirements* challengeReq = NULL;

	if( !missionReq ) {
		return NULL;
	}

	{
		int it;
		for( it = 0; it != eaSize( &missionReq->challengeRequirements ); ++it ) {
			if( stricmp( missionReq->challengeRequirements[ it ]->challengeName, challenge_name ) == 0 ) {
				challengeReq = missionReq->challengeRequirements[ it ];
				break;
			}
		}
	}
	if( !challengeReq ) {
		return NULL;
	}

	return challengeReq->interactParams;
}

static UGCGenesisProceduralObjectParams* ugcGenesisFindMissionChallengeVolumeParams(UGCGenesisMissionRequirements* missionReq, const char* challenge_name)
{
	UGCGenesisMissionChallengeRequirements* challengeReq = NULL;

	if( !missionReq ) {
		return NULL;
	}

	{
		int it;
		for( it = 0; it != eaSize( &missionReq->challengeRequirements ); ++it ) {
			if( stricmp( missionReq->challengeRequirements[ it ]->challengeName, challenge_name ) == 0 ) {
				challengeReq = missionReq->challengeRequirements[ it ];
				break;
			}
		}
	}
	if( !challengeReq ) {
		return NULL;
	}

	return challengeReq->volumeParams;
}

static UGCGenesisInstancedChildParams *ugcGenesisChildParamsToInstancedChildParams(UGCGenesisInstancedObjectParams *params, UGCGenesisPlacementChildParams *child, int idx, const char* challengeName)
{
	UGCGenesisInstancedChildParams *instanced = NULL;

	instanced = eaGet(&params->eaChildParams, idx);

	if(!instanced)
		eaSet(&params->eaChildParams, instanced = StructCreate(parse_UGCGenesisInstancedChildParams), idx);

	if( !nullStr( child->pcLogicalName )) {
		StructCopyString( &instanced->pcLogicalName, child->pcLogicalName );
	}
	copyVec3(child->vOffset, instanced->vOffset);
	copyVec3(child->vPyr, instanced->vPyr);
	if(child->actor_params.costume) {
		if(!instanced->pCostumeProperties) {
			instanced->pCostumeProperties = StructCreate(parse_WorldActorCostumeProperties);
		}

		StructCopyAll(parse_WorldActorCostumeProperties, child->actor_params.costume, instanced->pCostumeProperties);
	}
	return instanced;
}

static UGCGenesisToPlaceObject *ugcGenesisCreateChallengeVolume(UGCGenesisToPlaceObject *parent_object, UGCGenesisProceduralObjectParams* volume_params,
								  GroupDef *orig_def, UGCGenesisToPlaceObject *orig_object, UGCRuntimeErrorContext *parent_context, UGCGenesisObjectVolume *volume)
{
	char volume_str[256];
	F32 size = volume->size;
	UGCGenesisToPlaceObject *to_place_object = StructCreate( parse_UGCGenesisToPlaceObject );
	to_place_object->uid = 0;

	sprintf(volume_str, "%s_VOLUME", orig_object->challenge_name);
	to_place_object->challenge_name = strdup(volume_str);
	to_place_object->object_name = strdup(volume_str);
	to_place_object->challenge_is_unique = true;
	to_place_object->force_named_object = true;
	copyMat4(orig_object->mat, to_place_object->mat);
	to_place_object->parent = parent_object;
	to_place_object->seed = 0;
	to_place_object->source_context = StructClone( parse_UGCRuntimeErrorContext, parent_context );

	to_place_object->params = StructCreate(parse_UGCGenesisProceduralObjectParams);

	StructCopyAll( parse_UGCGenesisProceduralObjectParams, volume_params, to_place_object->params );

	if (!to_place_object->params->volume_properties)
		to_place_object->params->volume_properties = StructCreate(parse_GroupVolumeProperties);

	if (volume->pVolumeProperties)
	{
		to_place_object->params->volume_properties->eShape = volume->pVolumeProperties->eShape;
		copyVec3(volume->pVolumeProperties->vBoxMin, to_place_object->params->volume_properties->vBoxMin);
		copyVec3(volume->pVolumeProperties->vBoxMax, to_place_object->params->volume_properties->vBoxMax);
		to_place_object->params->volume_properties->fSphereRadius = volume->pVolumeProperties->fSphereRadius;
	}
	else if (volume->is_relative)
	{
		if (volume->is_square)
		{
			to_place_object->params->volume_properties->eShape = GVS_Box;
			scaleVec3(orig_def->bounds.min, size, to_place_object->params->volume_properties->vBoxMin);
			scaleVec3(orig_def->bounds.max, size, to_place_object->params->volume_properties->vBoxMax);
		}
		else
		{
			to_place_object->params->volume_properties->eShape = GVS_Sphere;
			to_place_object->params->volume_properties->fSphereRadius = orig_def->bounds.radius*size;
		}
	}
	else
	{
		if (volume->is_square)
		{
			to_place_object->params->volume_properties->eShape = GVS_Box;
			setVec3same(to_place_object->params->volume_properties->vBoxMin, -size);
			setVec3same(to_place_object->params->volume_properties->vBoxMax,  size);
		}
		else
		{
			to_place_object->params->volume_properties->eShape = GVS_Sphere;
			to_place_object->params->volume_properties->fSphereRadius = size;
		}
	}
	//eaPushUnique(&to_place_object->params->volume_properties->ppcVolumeTypes, allocAddString("Interaction"));

	return to_place_object;
}

void ugcPopulateObject(int iPartitionIdx, UGCGenesisToPlaceState *to_place, MersenneTable *table, UGCGenesisObject *object, UGCGenesisToPlaceObject *parent_object, UGCGenesisMissionRequirements *mission_req, F32 default_height, Vec3 clamp_min, Vec3 clamp_max)
{
	UGCGenesisToPlaceObject *to_place_object;
	GroupDef *def;
	UGCGenesisToPlaceObject *parent;
	Vec3 snap_pos;

	if( !object->obj.name_uid && !object->astrObjectSoundEvent ) {
		return;
	}
	
	to_place_object = StructCreate( parse_UGCGenesisToPlaceObject );
	to_place_object->uid = object->obj.name_uid;
	to_place_object->challenge_type = object->challenge_type;
	to_place_object->challenge_name = strdup(object->challenge_name);
	to_place_object->force_named_object = object->force_named_object;
	to_place_object->challenge_index = object->challenge_id;
	to_place_object->challenge_is_unique = true;
	to_place_object->parent = parent_object;
	eaCopyStructs(&object->eaChildParams, &to_place_object->eaChildParams, parse_UGCGenesisGroupDefChildParam);
	eaCopyStructs(&object->eaRoomDoors, &to_place_object->eaRoomDoors, parse_UGCGenesisRoomDoorSwitch);

	if(!nullStr(object->astrObjectSoundEvent))
	{
		to_place_object->params = StructCreate(parse_UGCGenesisProceduralObjectParams);
		to_place_object->params->sound_sphere_properties = StructCreate(parse_WorldSoundSphereProperties);	
		to_place_object->params->sound_sphere_properties->pcEventName = object->astrObjectSoundEvent;
	}

	if (object->is_trap)
	{
		char trap_name[256];
		sprintf(trap_name, "%s_TRAP", object->challenge_name);
		to_place_object->trap_name = allocAddString(trap_name);
	}

	createMat3DegYPR(to_place_object->mat, object->params.rotation);
	to_place_object->mat[3][0] = object->params.position[0];
	to_place_object->mat[3][2] = object->params.position[2];

	copyVec3(to_place_object->mat[3], snap_pos);
	for (parent = to_place_object->parent; parent; parent = parent->parent)
	{
		addVec3(snap_pos, parent->mat[3], snap_pos);
	}

	// absolute position takes precedence
	if (object->params.bAbsolutePos)
	{
		to_place_object->mat[3][1] = default_height;
	}
	else
	{
		bool height_found = false;
		
		// Look for a platform_parent
		if (object->platform_parent_group != 0)
		{
			FOR_EACH_IN_EARRAY(to_place->platform_groups, UGCGenesisToPlacePlatformGroup, group)
			{
				if (group->group_id == object->platform_parent_group &&
					group->platform_level == object->platform_parent_level)
				{
					// Do a collision check against the grid
					static int test_num = 0;
					F32* heights = NULL;
					F32* normals = NULL;
					Mat4 item_matrix;
					identityMat4(item_matrix);
					item_matrix[3][0] = snap_pos[0];
					item_matrix[3][2] = snap_pos[2];
					exclusionCalculateObjectHeight(group->platform_grid, item_matrix, 100, test_num++, true, iPartitionIdx, &heights, &normals);
					if (eafSize(&heights) != 0)
					{
						to_place_object->mat[3][1] = heights[0];
						if(object->params.bSnapNormal && eafSize(&normals) >= 3)
						{
							Mat3 up_mat, result;
							mat3FromUpVector(normals, up_mat);
							mulMat3(up_mat, to_place_object->mat, result);
							copyMat3(result, to_place_object->mat);
						}
						height_found = true;
					}
					eafDestroy(&heights);
					eafDestroy(&normals);
					if (height_found)
						break;
				}
			}
			FOR_EACH_END;
		}
		if (!height_found)
		{
			// Couldn't find a platform, try for a raycast
			if (object->params.bSnapRayCast)
			{
				Vec3 normal;
				to_place_object->mat[3][1] = ugcPopulateGetTerrainHeight(iPartitionIdx, snap_pos, object->params.bSnapToGeo, object->params.bLegacyHeightCheck,
																		 default_height, normal);
				if(object->params.bSnapNormal)
				{
					Mat3 up_mat, result;
					mat3FromUpVector(normal, up_mat);
					mulMat3(up_mat, to_place_object->mat, result);
					copyMat3(result, to_place_object->mat);
				}
			}
			else
			{
				// Default to InternalSpawnPoint Y
				to_place_object->mat[3][1] = default_height;
			}
		}
	}
	
	to_place_object->mat[3][1] += object->params.position[1];

	to_place_object->mat[3][0] = CLAMP(to_place_object->mat[3][0], clamp_min[0], clamp_max[0]);
	to_place_object->mat[3][1] = CLAMP(to_place_object->mat[3][1], clamp_min[1], clamp_max[1]);
	to_place_object->mat[3][2] = CLAMP(to_place_object->mat[3][2], clamp_min[2], clamp_max[2]);

	to_place_object->seed = randomMersenneInt(table);
	to_place_object->source_context = StructClone( parse_UGCRuntimeErrorContext, object->source_context );
	to_place_object->spawn_name = allocAddString(object->spawn_point_name);

	if (mission_req)
	{
		UGCGenesisInstancedObjectParams* object_params = NULL;
		UGCGenesisInteractObjectParams* interact_params = NULL;

		object_params = ugcGenesisFindMissionChallengeInstanceParams(mission_req, to_place_object->challenge_name);
		if( object_params ) {
			if( !to_place_object->instanced ) {
				to_place_object->instanced = StructCreate( parse_UGCGenesisInstancedObjectParams );
			}
			StructCopyAll( parse_UGCGenesisInstancedObjectParams, object_params, to_place_object->instanced );
		}

		interact_params = ugcGenesisFindMissionChallengeInteractParams(mission_req, to_place_object->challenge_name);
		if( interact_params ) {
			if( !to_place_object->interact ) {
				to_place_object->interact = StructCreate( parse_UGCGenesisInteractObjectParams );
			}
			StructCopyAll( parse_UGCGenesisInteractObjectParams, interact_params, to_place_object->interact );
		}
	}

	if(object->params.children)
	{
		if(!to_place_object->instanced)
			to_place_object->instanced = StructCreate( parse_UGCGenesisInstancedObjectParams );

		FOR_EACH_IN_EARRAY_FORWARDS(object->params.children, UGCGenesisPlacementChildParams, child)
		{
			Vec3 actor_pos;
			F32 world_height;
			// Fill in actor data
			UGCGenesisInstancedChildParams *instanced = ugcGenesisChildParamsToInstancedChildParams(to_place_object->instanced, child, FOR_EACH_IDX(0, child), to_place_object->challenge_name);

			if (child->bSnapRayCast)
			{
				// Get relative offset
				mulVecMat3(instanced->vOffset, to_place_object->mat, actor_pos);
				// Convert to world position
				actor_pos[0] += to_place_object->mat[3][0];
				actor_pos[2] = to_place_object->mat[3][2] - actor_pos[2];
				// Do height check
				world_height = child->bAbsolutePos ? default_height : ugcPopulateGetTerrainHeight(iPartitionIdx, actor_pos, child->bSnapToGeo,
																								  child->bLegacyHeightCheck,
																								  default_height, NULL);
				// Set relative height
				instanced->vOffset[1] += (world_height - to_place_object->mat[3][1]) + object->params.position[1];
			}
		}
		FOR_EACH_END
	}

	if (object->patrol_specified)
	{
		char patrol_name[256];
		UGCGenesisToPlacePatrol *to_place_patrol = StructCreate( parse_UGCGenesisToPlacePatrol );
		sprintf(patrol_name, "%s_Patrol", object->challenge_name);
		to_place_patrol->patrol_name = strdup(patrol_name);
		StructCopy(parse_WorldPatrolProperties, object->patrol_specified, &to_place_patrol->patrol_properties, 0, 0, 0);
		to_place_patrol->source_context = StructClone( parse_UGCRuntimeErrorContext, object->source_context );
		eaPush(&to_place->patrols, to_place_patrol);

		if( !to_place_object->instanced ) {
			to_place_object->instanced = StructCreate( parse_UGCGenesisInstancedObjectParams );
		}

		// [COR-16104] Because 0 actually means default_height, we
		// need to modify all the patrol point positions to use that
		// default height
		{
			int it;
			for( it = 0; it != eaSize( &to_place_patrol->patrol_properties.patrol_points ); ++it ) {
				WorldPatrolPointProperties* patrolPoint = to_place_patrol->patrol_properties.patrol_points[ it ];
				patrolPoint->pos[1] += default_height;
			}
		}

		to_place_object->instanced->has_patrol = true;
	}

	eaPush(&to_place->objects, to_place_object);

	def = objectLibraryGetGroupDef(object->obj.name_uid, false);

	if (def && object->volume)
	{
		UGCGenesisProceduralObjectParams *volume_params = ugcGenesisFindMissionChallengeVolumeParams(mission_req, to_place_object->challenge_name);
		
		// We don't have bounds yet because we are not in editor. Get the bounds explicitly so we can generate properly
		ugcComponentCalcBoundsForObjLib(object->obj.name_uid, def->bounds.min, def->bounds.max, &(def->bounds.radius));
		if (volume_params)
		{
			to_place_object = ugcGenesisCreateChallengeVolume(parent_object, volume_params, def, to_place_object, object->source_context, object->volume);
			eaPush(&to_place->objects, to_place_object);
		}
	}

	if (def && object->platform_group != 0)
	{
		UGCRoomInfo* room_info = ugcRoomGetRoomInfo( def->name_uid );
		ExclusionVolumeGroup **platforms = NULL;
		int level;

		for (level = 0; level < (room_info ? room_info->iNumLevels : 1); level++)
		{
			UGCGenesisToPlacePlatformGroup *group = StructCreate( parse_UGCGenesisToPlacePlatformGroup );
			group->group_id = object->platform_group;
			group->platform_level = level;
			group->platform_grid = exclusionGridCreate(0, 0, 1, 1);
			ugcZoneMapRoomGetPlatforms(def->name_uid, level, &platforms);

			FOR_EACH_IN_EARRAY(platforms, ExclusionVolumeGroup, volume_group)
			{
				ExclusionObject *ex_object = calloc(1, sizeof(ExclusionObject));
				copyMat4(to_place_object->mat, ex_object->mat);
				ex_object->volume_group = volume_group;
				ex_object->max_radius = 1e8;
				ex_object->volume_group_owned = true;
				exclusionGridAddObject(group->platform_grid, ex_object, 1e8, false);
			}
			FOR_EACH_END;
			eaDestroy(&platforms);

			eaPush(&to_place->platform_groups, group);
		}
	}
}

static void ugcGenerateObjectSortHelper(UGCGenesisObject *in_object, UGCGenesisObject **in_array, UGCGenesisObject ***out_array)
{
	if (eaFind(out_array, in_object) != -1)
		return;

	if (in_object->platform_parent_group != 0)
	{
		FOR_EACH_IN_EARRAY(in_array, UGCGenesisObject, object)
		{
			if (object->platform_group == in_object->platform_parent_group &&
				object != in_object)
				ugcGenerateObjectSortHelper(object, in_array, out_array);
		}
		FOR_EACH_END;
	}

	eaPush(out_array, in_object);
}

static void ugcGenerateObjectSort(UGCGenesisObject **in_array, UGCGenesisObject ***out_array)
{
	eaDestroy(out_array);
	FOR_EACH_IN_EARRAY(in_array, UGCGenesisObject, in_object)
	{
		ugcGenerateObjectSortHelper(in_object, in_array, out_array);
	}
	FOR_EACH_END;
}

static UGCGenesisToPlaceObject *ugcSolarSystemPlaceGroupDef(GroupDef *def, const char *name, const F32 *pos, F32 fRot, UGCGenesisToPlaceObject *parent_object, UGCGenesisToPlaceState *to_place)
{
	UGCGenesisToPlaceObject *child_object = StructCreate( parse_UGCGenesisToPlaceObject );
	child_object->group_def = (def && def->filename) ? objectLibraryGetEditingCopy(def, true, false) : def; // Object Library pieces need an editing copy
	child_object->mat_relative = true;
	if (name)
		child_object->object_name = allocAddString(name);
	identityMat3(child_object->mat);
	yawMat3(fRot,child_object->mat);
	if (pos)
		copyVec3(pos, child_object->mat[3]);
	child_object->parent = parent_object;
	eaPush(&to_place->objects, child_object);
	return child_object;
}

static void ugcGenesisProceduralObjectEnsureType(UGCGenesisProceduralObjectParams *params)
{
	if (!params->hull_properties)
		params->hull_properties = StructCreate(parse_GroupHullProperties);

	if (params->power_volume)
		eaPushUnique(&params->hull_properties->ppcTypes, allocAddString("Power"));
	if (params->fx_volume)
		eaPushUnique(&params->hull_properties->ppcTypes, allocAddString("FX"));
	if (params->sky_volume_properties)
		eaPushUnique(&params->hull_properties->ppcTypes, allocAddString("SkyFade"));
	if (params->event_volume_properties)
		eaPushUnique(&params->hull_properties->ppcTypes, allocAddString("Event"));
	if (params->action_volume_properties)
		eaPushUnique(&params->hull_properties->ppcTypes, allocAddString("Action"));
	if (params->optionalaction_volume_properties)
		eaPushUnique(&params->hull_properties->ppcTypes, allocAddString("OptionalAction"));
}

static UGCGenesisToPlaceObject* ugcGenesisMakeExteriorVolume(UGCGenesisToPlaceObject *parent_object, const char *name, Vec3 min, Vec3 max)
{
	UGCGenesisToPlaceObject *to_place_object = StructCreate( parse_UGCGenesisToPlaceObject );
	to_place_object->object_name = allocAddString(name);
	copyMat4(unitmat, to_place_object->mat);
	to_place_object->params = StructCreate(parse_UGCGenesisProceduralObjectParams);
	to_place_object->params->volume_properties = StructCreate(parse_GroupVolumeProperties);
	to_place_object->params->volume_properties->eShape = GVS_Box;
	copyVec3(min, to_place_object->params->volume_properties->vBoxMin);
	copyVec3(max, to_place_object->params->volume_properties->vBoxMax);
	to_place_object->uid = 0;
	to_place_object->parent = parent_object;
	return to_place_object;
}

static void ugcGenesisMakeExteriorBoundingSkyVolume(UGCGenesisToPlaceObject ***object_list, UGCGenesisToPlaceObject *parent_object, const char *name, Vec2 min, Vec2 max, WorldSkyVolumeProperties *sky_props)
{
	UGCGenesisToPlaceObject *to_place_object;
	to_place_object = ugcGenesisMakeExteriorVolume(parent_object, name, min, max); 
	to_place_object->params->sky_volume_properties = StructClone(parse_WorldSkyVolumeProperties, sky_props);
	ugcGenesisProceduralObjectEnsureType(to_place_object->params);
	eaPush(object_list, to_place_object);
}

static void ugcGenesisMakeExteriorBoundingSkyVolumes(UGCGenesisToPlaceObject ***object_list, UGCGenesisToPlaceObject *parent_object, Vec3 min, Vec3 max)
{
	#define GEN_BOUND_VOL_SIZE 100
	Vec3 vol_min, vol_max;
	UGCPerProjectDefaults* config = ugcGetDefaults();
	WorldSkyVolumeProperties *sky_props;

	if(!config)
		return;
	sky_props = config->boundary;
	if(!sky_props)
		return;

	// Upper
	{
		vol_min[0] =  min[0];
		vol_min[1] =  max[1] - GEN_BOUND_VOL_SIZE;
		vol_min[2] =  min[2];
		vol_max[0] =  max[0];
		vol_max[1] =  max[1];
		vol_max[2] =  max[2];
		ugcGenesisMakeExteriorBoundingSkyVolume(object_list, parent_object, "Exterior_Autogen_SkyFade_Upper", vol_min, vol_max, sky_props);
	}

	// Lower
	{
		vol_min[0] =  min[0];
		vol_min[1] =  min[1];
		vol_min[2] =  min[2];
		vol_max[0] =  max[0];
		vol_max[1] =  min[1] + GEN_BOUND_VOL_SIZE;
		vol_max[2] =  max[2];
		ugcGenesisMakeExteriorBoundingSkyVolume(object_list, parent_object, "Exterior_Autogen_SkyFade_Lower", vol_min, vol_max, sky_props);
	}

	// North
	{
		vol_min[0] =  min[0];
		vol_min[1] =  min[1];
		vol_min[2] =  max[2] - GEN_BOUND_VOL_SIZE;
		vol_max[0] =  max[0];
		vol_max[1] =  max[1];
		vol_max[2] =  max[2];
		ugcGenesisMakeExteriorBoundingSkyVolume(object_list, parent_object, "Exterior_Autogen_SkyFade_North", vol_min, vol_max, sky_props);
	}

	// South
	{
		vol_min[0] =  min[0];
		vol_min[1] =  min[1];
		vol_min[2] =  min[2];
		vol_max[0] =  max[0];
		vol_max[1] =  max[1];
		vol_max[2] =  min[2] + GEN_BOUND_VOL_SIZE;
		ugcGenesisMakeExteriorBoundingSkyVolume(object_list, parent_object, "Exterior_Autogen_SkyFade_South", vol_min, vol_max, sky_props);
	}

	// East
	{
		vol_min[0] =  max[0] - GEN_BOUND_VOL_SIZE;
		vol_min[1] =  min[1];
		vol_min[2] =  min[2];
		vol_max[0] =  max[0];
		vol_max[1] =  max[1];
		vol_max[2] =  max[2];
		ugcGenesisMakeExteriorBoundingSkyVolume(object_list, parent_object, "Exterior_Autogen_SkyFade_East", vol_min, vol_max, sky_props);
	}

	// West
	{
		vol_min[0] =  min[0];
		vol_min[1] =  min[1];
		vol_min[2] =  min[2];
		vol_max[0] =  min[0] + GEN_BOUND_VOL_SIZE;
		vol_max[1] =  max[1];
		vol_max[2] =  max[2];
		ugcGenesisMakeExteriorBoundingSkyVolume(object_list, parent_object, "Exterior_Autogen_SkyFade_West", vol_min, vol_max, sky_props);
	}
}

static void ugcGenesisProceduralObjectAddVolumeType(UGCGenesisProceduralObjectParams *params, const char *type)
{
	if (!params->hull_properties)
		params->hull_properties = StructCreate(parse_GroupHullProperties);

	type = allocAddString(type);
	eaPushUnique(&params->hull_properties->ppcTypes, type);
}

void ugcGenesisProceduralObjectSetEventVolume(UGCGenesisProceduralObjectParams *params)
{
	if (!params->event_volume_properties)
		params->event_volume_properties = StructCreate(parse_WorldEventVolumeProperties);
	
	ugcGenesisProceduralObjectEnsureType(params);
}

void ugcGenesisProceduralObjectSetActionVolume(UGCGenesisProceduralObjectParams *params)
{
	if (!params->action_volume_properties)
		params->action_volume_properties = StructCreate(parse_WorldActionVolumeProperties);

	ugcGenesisProceduralObjectEnsureType(params);
}

void ugcGenesisProceduralObjectSetOptionalActionVolume(UGCGenesisProceduralObjectParams *params)
{
	if (!params->optionalaction_volume_properties)
		params->optionalaction_volume_properties = StructCreate(parse_WorldOptionalActionVolumeProperties);

	ugcGenesisProceduralObjectEnsureType(params);
}

#define EXTERIOR_MIN_PLAYFIELD_BUFFER 150
static void ugcGenesisMakeDetailObjects(UGCGenesisToPlaceState *to_place, UGCGenesisEcosystem *ecosystem, UGCGenesisZoneNodeLayout *layout, bool force_no_water, bool make_sky_volumes)
{
	int i;
	Vec3 min, max;
	F32 cx[] = { 0.f, 1.f, 0.5f, 0.5f };
	F32 cz[] = { 0.5f, 0.5f, 1.f, 0.f };
	UGCGenesisGeotype *geotype = NULL;
	F32 play_buffer;
	bool has_playable_volume = false;

	UGCGenesisToPlaceObject *detail_objects = StructCreate( parse_UGCGenesisToPlaceObject );
	detail_objects->object_name = allocAddString("GenesisDetailObjects");
	detail_objects->uid = 0;
	detail_objects->parent = NULL;
	identityMat4(detail_objects->mat);
	eaPush(&to_place->objects, detail_objects);

	// Add Water Plane
	assert( force_no_water );

	for ( i=0; i < eaSize(&ecosystem->placed_objects); i++ )
	{
		GroupDefRef *def_ref = ecosystem->placed_objects[i];
		int uid = def_ref->name_uid;
		if(!uid)
			uid = objectLibraryUIDFromObjName(def_ref->name_str);
		if(!uid)
		{
			ugcRaiseErrorInternal(UGC_ERROR, "Ecosystem", ecosystem->name,
								  "References a just placed object %s that does not exist.",
								  def_ref->name_uid);
		}
		else 
		{
			UGCGenesisToPlaceObject *to_place_object = StructCreate( parse_UGCGenesisToPlaceObject );
			copyMat4(unitmat, to_place_object->mat);
			to_place_object->uid = uid;
			eaPush(&to_place->objects, to_place_object);	
		}
	}

	if (layout)
	{
		min[0] = layout->play_min[0];
		min[2] = layout->play_min[1];
		max[0] = layout->play_max[0];
		max[2] = layout->play_max[1];
		play_buffer = layout->play_buffer;
	}
	else
	{
		min[0] = 0;
		min[2] = 0;
		max[0] = 2048.f;
		max[2] = 2048.f;
		play_buffer = EXTERIOR_MIN_PLAYFIELD_BUFFER;
	}

	if (max[0] > min[0] && max[2] > min[2])
	{
		min[1] = max[1] = 0;
		addVec3same(min, play_buffer, min);
		subVec3same(max, play_buffer, max);

		if (layout->play_heights[1] > layout->play_heights[0])
		{
			min[1] = layout->play_heights[0];
			max[1] = layout->play_heights[1];
		}
		else
		{
			min[1] = -1500;
			max[1] = 5000;
		}

		has_playable_volume = true;
	}

	if (make_sky_volumes)
	{
		// Add Bounding Sky Fade Volumes
		ugcGenesisMakeExteriorBoundingSkyVolumes(&to_place->objects, detail_objects, min, max);
	}

	// Add Room Volume[s]
	if (eaSize(&layout->room_partitions) > 0)
	{
		FOR_EACH_IN_EARRAY(layout->room_partitions, ZoneMapEncounterRoomInfo, partition)
		{
			UGCGenesisToPlaceObject *to_place_object = ugcGenesisMakeExteriorVolume(detail_objects, "Exterior_Autogen_Volume", partition->min, partition->max);
			to_place_object->params->room_properties = StructCreate(parse_WorldRoomProperties);
			to_place_object->params->room_properties->eRoomType = WorldRoomType_Room;
			eaPush(&to_place->objects, to_place_object);
		}
		FOR_EACH_END;
	}
	else if (has_playable_volume)
	{
		UGCGenesisToPlaceObject *to_place_object = ugcGenesisMakeExteriorVolume(detail_objects, "Exterior_Autogen_Volume", min, max);
		to_place_object->params->room_properties = StructCreate(parse_WorldRoomProperties);
		to_place_object->params->room_properties->eRoomType = WorldRoomType_Room;

		if (!nullStr(layout->amb_sound)) {
			to_place_object->params->sound_sphere_properties = StructCreate(parse_WorldSoundSphereProperties);	
			to_place_object->params->sound_sphere_properties->pcEventName = allocAddString(layout->amb_sound);
		}

		eaPush(&to_place->objects, to_place_object);
	}
	
	if (has_playable_volume)
	{
		UGCGenesisToPlaceObject *to_place_object = ugcGenesisMakeExteriorVolume(detail_objects, "Exterior_Autogen_Playable_Volume", min, max);
		ugcGenesisProceduralObjectAddVolumeType(to_place_object->params, "Playable");
		eaPush(&to_place->objects, to_place_object);
	}
}

static UGCGenesisProceduralObjectParams* ugcGenesisCreateMultiMissionWrapperParams(void)
{
	UGCGenesisProceduralObjectParams* params = StructCreate(parse_UGCGenesisProceduralObjectParams);
	WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );
	
	params->interaction_properties = StructCreate(parse_WorldInteractionProperties);
	params->interaction_properties->pChildProperties = StructCreate(parse_WorldChildInteractionProperties);
	params->interaction_properties->pChildProperties->pChildSelectExpr = exprCreateFromString("GetMapVariableInt(\"Mission_Num\")", NULL);
	eaPush(&params->interaction_properties->eaEntries, entry);
	entry->pcInteractionClass = allocAddString( "NAMEDOBJECT" );

	return params;
}

static void ugcGenesisCreateMissionVolumes(UGCGenesisToPlaceState *to_place, Vec3 min, Vec3 max, bool allowMounts, const char *layout_name, UGCGenesisMissionRequirements **mission_reqs)
{
	int j;
	UGCGenesisToPlaceObject *mission_parent;

	mission_parent = StructCreate( parse_UGCGenesisToPlaceObject );
	mission_parent->object_name = allocAddString("Missions");
	mission_parent->uid = 0;
	identityMat4(mission_parent->mat);
	mission_parent->parent = NULL;
	if (eaSize(&mission_reqs) > 1)
	{
		mission_parent->params = ugcGenesisCreateMultiMissionWrapperParams();
	}
	eaPush(&to_place->objects, mission_parent);

	for (j = 0; j < eaSize(&mission_reqs); j++)
	{
		UGCGenesisToPlaceObject *mission_object = StructCreate( parse_UGCGenesisToPlaceObject );
		mission_object->object_name = ugcGenesisMissionVolumeName(layout_name, mission_reqs[j]->missionName);
		mission_object->challenge_name = strdup(mission_object->object_name);
		mission_object->challenge_is_unique = true;
		identityMat3(mission_object->mat);
		mission_object->uid = 0;
		mission_object->parent = mission_parent;
		mission_object->params = StructClone(parse_UGCGenesisProceduralObjectParams, mission_reqs[j]->params);
		mission_object->params->volume_properties = StructCreate(parse_GroupVolumeProperties);
		mission_object->params->volume_properties->eShape = GVS_Box;
		copyVec3(min, mission_object->params->volume_properties->vBoxMin);
		copyVec3(max, mission_object->params->volume_properties->vBoxMax);

		if( allowMounts ) {
			mission_object->params->power_volume = StructCreate( parse_WorldPowerVolumeProperties );
			SET_HANDLE_FROM_STRING( g_hPowerDefDict, "Volume_MountAllowed", mission_object->params->power_volume->power );
			mission_object->params->power_volume->strength = WorldPowerVolumeStrength_Default;
			mission_object->params->power_volume->repeat_time = 1;
		}
		
		eaPush(&to_place->objects, mission_object);
	}
}

static GroupDef *ugcSolarSystemMakeGroupDef(GroupDefLib *def_lib, const char *name)
{
	GroupDef *new_def = NULL;
	char node_name[64];

	groupLibMakeGroupName(def_lib, name, SAFESTR(node_name), 0);
	new_def = groupLibNewGroupDef(def_lib, NULL, 0, node_name, 0, false, true);
	return new_def;
}

static void ugcSolarSystemInitVolume(GroupDef *volume_def, Vec3 min, Vec3 max)
{
	if(!volume_def->property_structs.volume)
		volume_def->property_structs.volume = StructCreate(parse_GroupVolumeProperties);
	volume_def->property_structs.volume->eShape = GVS_Box;
	copyVec3(min, volume_def->property_structs.volume->vBoxMin);
	copyVec3(max, volume_def->property_structs.volume->vBoxMax);
}

static UGCGenesisToPlaceObject* ugcSolarSystemMakeToPlaceVolume(UGCGenesisToPlaceObject *parent_object, const char *name, Vec3 min, Vec3 max)
{
	UGCGenesisToPlaceObject *to_place_object = StructCreate( parse_UGCGenesisToPlaceObject );
	to_place_object->object_name = allocAddString(name);
	copyMat4(unitmat, to_place_object->mat);
	if(!to_place_object->params)
		to_place_object->params = StructCreate(parse_UGCGenesisProceduralObjectParams);
	to_place_object->params->volume_properties = StructCreate(parse_GroupVolumeProperties);
	to_place_object->params->volume_properties->eShape = GVS_Box;
	copyVec3(min, to_place_object->params->volume_properties->vBoxMin);
	copyVec3(max, to_place_object->params->volume_properties->vBoxMax);
	to_place_object->uid = 0;
	to_place_object->parent = parent_object;
	return to_place_object;
}

static void ugcGenesisApplyObjectParams(GroupDef *def, UGCGenesisProceduralObjectParams *params)
{
	if (params->model_name)
		def->model = modelFind(params->model_name, false, WL_FOR_WORLD);
	if (params->action_volume_properties)
		def->property_structs.server_volume.action_volume_properties = StructClone(parse_WorldActionVolumeProperties, params->action_volume_properties);
	if (params->event_volume_properties)
		def->property_structs.server_volume.event_volume_properties = StructClone(parse_WorldEventVolumeProperties, params->event_volume_properties);
	if (params->optionalaction_volume_properties)
		def->property_structs.server_volume.obsolete_optionalaction_properties = StructClone(parse_WorldOptionalActionVolumeProperties, params->optionalaction_volume_properties);
	if (params->fx_volume)
		def->property_structs.client_volume.fx_volume_properties = StructClone(parse_WorldFXVolumeProperties, params->fx_volume);
	if (params->sky_volume_properties)
		def->property_structs.client_volume.sky_volume_properties = StructClone(parse_WorldSkyVolumeProperties, params->sky_volume_properties);
	if (params->sound_volume_properties)
		def->property_structs.client_volume.sound_volume_properties = StructClone(parse_WorldSoundVolumeProperties, params->sound_volume_properties);
	if (params->power_volume)
		def->property_structs.server_volume.power_volume_properties = StructClone(parse_WorldPowerVolumeProperties, params->power_volume);
	if (params->curve)
		def->property_structs.curve = StructClone(parse_WorldCurve, params->curve);
	if (params->patrol_properties)
		def->property_structs.patrol_properties = StructClone(parse_WorldPatrolProperties, params->patrol_properties);
	if (params->interaction_properties)
		def->property_structs.interaction_properties = StructClone(parse_WorldInteractionProperties, params->interaction_properties);
	if (params->spawn_properties)
		def->property_structs.spawn_properties = StructClone(parse_WorldSpawnProperties, params->spawn_properties);
	StructCopyAll(parse_WorldPhysicalProperties, &params->physical_properties, &def->property_structs.physical_properties);
	StructCopyAll(parse_WorldTerrainProperties, &params->terrain_properties, &def->property_structs.terrain_properties);
	if (params->volume_properties)
		def->property_structs.volume = StructClone(parse_GroupVolumeProperties, params->volume_properties);
	if (params->hull_properties)
		def->property_structs.hull = StructClone(parse_GroupHullProperties, params->hull_properties);
	if (params->light_properties)
		def->property_structs.light_properties = StructClone(parse_WorldLightProperties, params->light_properties);
	if (params->genesis_properties)
		def->property_structs.genesis_properties = StructClone(parse_WorldGenesisProperties, params->genesis_properties);
	if (params->room_properties)
		def->property_structs.room_properties = StructClone(parse_WorldRoomProperties, params->room_properties);
	if (params->sound_sphere_properties)
		def->property_structs.sound_sphere_properties = StructClone(parse_WorldSoundSphereProperties, params->sound_sphere_properties);
}

static UGCGenesisToPlaceObject *ugcGenesisMakeToPlaceObject(const char *name, UGCGenesisToPlaceObject *parent_object, const F32 *pos, UGCGenesisToPlaceState *to_place)
{
	UGCGenesisToPlaceObject *child_object = StructCreate( parse_UGCGenesisToPlaceObject );
	if (name)
		child_object->object_name = allocAddString(name);
	identityMat3(child_object->mat);
	if (pos)
		copyVec3(pos, child_object->mat[3]);
	child_object->parent = parent_object;
	eaPush(&to_place->objects, child_object);
	return child_object;
}

void ugcGenesisObjectGetAbsolutePos(UGCGenesisToPlaceObject* obj, Vec3 out)
{
	setVec3same( out, 0 );
 	while( obj ) {
		addVec3( out, obj->mat[3], out );

		if( !obj->mat_relative ) {
			obj = NULL;
		} else {
			obj = obj->parent;
		}
	}
}

void ugcGenesisCalcMissionVolumePointsInto(UGCGenesisMissionVolumePoints ***peaVolumePointsList, UGCGenesisMissionRequirements* pReq, UGCGenesisToPlaceState *pToPlace)
{
	int extraVolumeIt;
	int objIt;
	int volObjIt;
	for( extraVolumeIt = 0; extraVolumeIt != eaSize( &pReq->extraVolumes ); ++extraVolumeIt ) {
		UGCGenesisMissionExtraVolume* extraVolume = pReq->extraVolumes[ extraVolumeIt ];
		
		UGCGenesisMissionVolumePoints* pVolPoints = StructCreate( parse_UGCGenesisMissionVolumePoints );
		pVolPoints->volume_name = StructAllocString( extraVolume->volumeName );
		
		for( objIt = 0; objIt != eaSize( &pToPlace->objects ); ++objIt ) {
			UGCGenesisToPlaceObject* object = pToPlace->objects[ objIt ];
			if( object->challenge_name ) {
				for( volObjIt = 0; volObjIt != eaSize( &extraVolume->objects ); ++volObjIt ) {
					if( eaFindString( &extraVolume->objects, object->challenge_name ) != -1 ) {
						Vec3 pos;
						float posRadius = 0;
						ugcGenesisObjectGetAbsolutePos( object, pos );

						if( object->params && object->params->volume_properties && object->params->volume_properties->eShape == GVS_Box) {
							Vec3 bounds[2];
							copyVec3(object->params->volume_properties->vBoxMin, bounds[0]);
							copyVec3(object->params->volume_properties->vBoxMax, bounds[1]);
							posRadius = MAX( posRadius, sqrtf( SQR( bounds[0][0]) + SQR( bounds[0][2] )));
							posRadius = MAX( posRadius, sqrtf( SQR( bounds[0][0]) + SQR( bounds[1][2] )));
							posRadius = MAX( posRadius, sqrtf( SQR( bounds[1][0]) + SQR( bounds[0][2] )));
							posRadius = MAX( posRadius, sqrtf( SQR( bounds[1][0]) + SQR( bounds[1][2] )));
						} else if( object->params && object->params->volume_properties && object->params->volume_properties->eShape == GVS_Sphere) {
							posRadius = object->params->volume_properties->fSphereRadius;
						}

						if( posRadius > 0 ) {
							Vec3 temp;
							setVec3( temp, pos[0] - posRadius, pos[1], pos[2] - posRadius );
							eafPush3( &pVolPoints->positions, temp );
							setVec3( temp, pos[0] - posRadius, pos[1], pos[2] + posRadius );
							eafPush3( &pVolPoints->positions, temp );
							setVec3( temp, pos[0] + posRadius, pos[1], pos[2] - posRadius );
							eafPush3( &pVolPoints->positions, temp );
							setVec3( temp, pos[0] + posRadius, pos[1], pos[2] + posRadius );
							eafPush3( &pVolPoints->positions, temp );
						} else {
							eafPush3( &pVolPoints->positions, pos );
						}
					}
				}
			}
		}

		eaPush( peaVolumePointsList, pVolPoints );
	}
}

static void ugcGenesisPopulateWaypointVolumes(UGCGenesisToPlaceState *to_place, UGCGenesisMissionRequirements **mission_reqs)
{
	int i, j;
	UGCGenesisToPlaceObject *waypoint_group;

	waypoint_group = ugcGenesisMakeToPlaceObject("Waypoints", NULL, zerovec3, to_place);

	for ( i=0; i < eaSize(&mission_reqs); i++ )
	{
		UGCGenesisMissionVolumePoints **volume_points_list = NULL;
		UGCGenesisMissionRequirements *req = mission_reqs[i];

		ugcGenesisCalcMissionVolumePointsInto(&volume_points_list, req, to_place);

		for ( j=0; j < eaSize(&volume_points_list); j++ )
		{
			UGCGenesisMissionVolumePoints *volume_info = volume_points_list[j];
			UGCGenesisToPlaceObject *volume_obj;
			UGCBoundingVolume volume = { 0 };

			volume_obj = StructCreate( parse_UGCGenesisToPlaceObject );
			volume_obj->object_name = allocAddString(volume_info->volume_name);
			volume_obj->challenge_name = strdup(volume_info->volume_name);
			volume_obj->challenge_is_unique = true;
			volume_obj->uid = 0;
			volume_obj->parent = waypoint_group;

			if(ugcGetBoundingVolumeFromPoints(&volume, volume_info->positions)) {
				copyMat4(unitmat, volume_obj->mat);
				copyVec3(volume.center, volume_obj->mat[3]);
				yawMat3(volume.rot, volume_obj->mat);
				
				volume_obj->params = StructCreate(parse_UGCGenesisProceduralObjectParams);
				ugcGenesisProceduralObjectSetEventVolume(volume_obj->params);
				volume_obj->params->volume_properties = StructCreate(parse_GroupVolumeProperties);
				volume_obj->params->volume_properties->eShape = GVS_Box;
				copyVec3(volume.extents[0], volume_obj->params->volume_properties->vBoxMin);
				copyVec3(volume.extents[1], volume_obj->params->volume_properties->vBoxMax);
				eaPush(&to_place->objects, volume_obj);
			} else {
				free(volume_obj->challenge_name);
				free(volume_obj);

				ugcRaiseErrorInternalCode(UGC_ERROR, "Waypoint: %s -- Could not place waypoint.",
										  volume_info->volume_name);
			}
		}

		eaDestroyStruct(&volume_points_list, parse_UGCGenesisMissionVolumePoints);
	}
}

void ugcGenerateLayer(ZoneMapInfo *zmap_info, UGCGenesisMissionRequirements **mission_reqs, ZoneMapLayer *layer, UGCGenesisZoneShared *gen_data, bool *is_space)
{
	UGCGenesisToPlaceState to_place = { 0 };
	U32 seed = (gen_data->layout_seed ? gen_data->layout_seed : 0);
	char name_str[MAX_PATH] = "";
	MersenneTable *table = mersenneTableCreate( seed );
	Vec3 center, bounds_min, bounds_max;
	float fZeroRot=0.0f;
	bool game_callback_disable;
	UGCMapType map_type;
	Vec3 spawn_pos;
	Quat qOrientation;
	float fspawn_rot=0.0f;
	ZoneMapEncounterRegionInfo *default_region;
	ZoneMapInfo *prefab_map_info;
	int iPartitionIdx = worldGetAnyCollPartitionIdx();
	
	assert(gen_data->external_map_name);
	prefab_map_info = worldGetZoneMapByPublicName(gen_data->external_map_name);

	loadstart_printf("Populating UGC Prefab layer...");

	worldLoadZoneMapByName(prefab_map_info->map_name);
	game_callback_disable = wl_state.disable_game_callbacks;
	wl_state.disable_game_callbacks = true;
	worldCheckForNeedToOpenCells(); // Open world cells to get interactable data
	wl_state.disable_game_callbacks = game_callback_disable;
	worldCreateAllCollScenes();

	map_type = ugcMapGetPrefabType(prefab_map_info->map_name, false);
	default_region = ugcGetZoneMapDefaultRegion(prefab_map_info->map_name);

	if (prefab_map_info)
	{
		UGCMapPlatformData *platform_data;
		
		ugcPlatformDictionaryLoad();

		platform_data = RefSystem_ReferentFromString(UGC_PLATFORM_INFO_DICT, prefab_map_info->map_name);
		if (platform_data)
			g_PrefabPlatformGrid = ugcMapEditorGenerateExclusionGrid(platform_data);
	}

	/////////////////////////////////////////////
	// Set up the center and bounds
	
	setVec3(center, 0, 0, 0);
	setVec3(bounds_min, 0, 0, 0);
	setVec3(bounds_max, 2048, 2000, 2048);

	if (default_region)
	{
		copyVec3(default_region->min, bounds_min);
		copyVec3(default_region->max, bounds_max);
		addVec3(default_region->min, default_region->max, center);
		scaleVec3(center, 0.5f, center);
	}


	/////////////////////////////////////////////
	//  Find a 'default' spawn position. 
	// This pulls from the prefab version of the map and is looking in a layer file for a particular artist-set position
	// We use this to generate an object that is called "UGC_SPAWN_INTERNAL" and
	//   then we use the Y placement (in absolute coordinates) for geometry or terrain relative
	//   placement of other objects (Including the real spawn point) when the relevant
	//   ray cast does not hit anything.
	// "UGC_SPAWN_INTERNAL" appears to only used (via ugcProjectGetMapStartSpawnTemp) as a fallback
	//   spawn point to be references in the .zone file if we don't find a component-based spawn point.
	//   Really we should consider removing this dependency and creating the default component before
	//   we start true generation.
	
	if (ugcGetZoneMapSpawnPoint(prefab_map_info->map_name, spawn_pos, &qOrientation))
	{
		Vec3 vPYR;
		quatToPYR(qOrientation, vPYR);
		fspawn_rot=vPYR[1];
	}
	else
	{
		// Couldn't find said spawn point, use the center of the map
		copyVec3(center, spawn_pos);
	}


	{
		UGCGenesisToPlaceObject *system_object, *mission_select_object, *mission_object, *spawn_object;
		GroupDefLib *def_lib = layer->grouptree.def_lib;
		GroupDef *system_root_def;
		GroupDef *fallback_spawn_point;
		Vec3 play_min, play_max;
		UGCGenesisEcosystem ecosystem = { 0 };
		UGCGenesisZoneNodeLayout layout = { 0 };
		UGCGenesisObject **sorted_objects = NULL;

		// Fallback spawn point
		fallback_spawn_point = objectLibraryGetGroupDefByName("Goto Spawn Point", false);

		spawn_object = ugcSolarSystemPlaceGroupDef(fallback_spawn_point, NULL, spawn_pos, fspawn_rot, NULL, &to_place);
		spawn_object->challenge_name = strdup("UGC_SPAWN_INTERNAL");
		spawn_object->challenge_is_unique = true;

		ugcGetZoneMapPlaceableBounds( play_min, play_max, prefab_map_info->map_name, false );
		setVec2(layout.play_min, play_min[0], play_min[2]);
		setVec2(layout.play_max, play_max[0], play_max[2]);
		setVec2(layout.play_heights, play_min[1], play_max[1]);
		
		if (map_type == UGC_MAP_TYPE_PREFAB_GROUND || map_type == UGC_MAP_TYPE_PREFAB_INTERIOR ||
			map_type == UGC_MAP_TYPE_INTERIOR)
		{
			if (map_type == UGC_MAP_TYPE_PREFAB_INTERIOR)
			{
				FOR_EACH_IN_EARRAY(default_region->rooms, ZoneMapEncounterRoomInfo, room)
				{
					eaPush(&layout.room_partitions, room);	
				}
				FOR_EACH_END;
				assert(eaSize(&layout.room_partitions) > 0);
			}
			
			ugcGenesisMakeDetailObjects(&to_place, &ecosystem, &layout, true, (map_type == UGC_MAP_TYPE_PREFAB_GROUND));
			ugcGenesisCreateMissionVolumes(&to_place, bounds_min, bounds_max, (map_type == UGC_MAP_TYPE_PREFAB_GROUND), gen_data->layout_name, mission_reqs);
			eaDestroy(&layout.room_partitions);
		}
		/*else if (map_type == UGC_MAP_TYPE_PREFAB_SPACE)
		{
			UGCGenesisShoebox shoebox = { 0 };
			copyVec3(center, shoebox.layer_center);
			copyVec3(play_min, shoebox.layer_min);
			copyVec3(play_max, shoebox.layer_max);
			setVec3(shoebox.overview_pos, 100.f, -0.5f, 100.f);

			if (is_space)
				*is_space = true;

			ugcSolarSystemCreateCommonObjects(&shoebox, backdrop, gen_data->layout_name, layer->grouptree.def_lib, &to_place, mission_reqs);
		}*/
		else
		{
			assertmsg(0, "Invalid map type on NNO UGC map!");
		}

		// Create damage volumes -- known to be the only power volumes we care about
		{
			ZoneMapEncounterInfo* zeniInfo = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", prefab_map_info->map_name );
			if( zeniInfo ) {
				UGCGenesisToPlaceObject* damageRoot = ugcSolarSystemPlaceGroupDef(
						ugcSolarSystemMakeGroupDef( def_lib, "PowerVolumes" ),
						"PowerVolumes", zerovec3, fZeroRot, NULL, &to_place );
				
				FOR_EACH_IN_EARRAY_FORWARDS( zeniInfo->objects, ZoneMapEncounterObjectInfo, zeniObj ) {
					if( zeniObj->volume && ugcPowerPropertiesIsUsedInUGC( zeniObj->volume->power_properties )) {
						GroupDef* powerVolumeDef = ugcSolarSystemMakeGroupDef( def_lib, "Volume" );
						UGCGenesisToPlaceObject* powerVolume;

						powerVolumeDef->property_structs.server_volume.power_volume_properties
							= StructClone( parse_WorldPowerVolumeProperties, zeniObj->volume->power_properties );

						powerVolumeDef->property_structs.volume = StructCreate( parse_GroupVolumeProperties );
						powerVolumeDef->property_structs.volume->eShape = zeniObj->volume->shape;
						copyVec3( zeniObj->volume->boxMin, powerVolumeDef->property_structs.volume->vBoxMin );
						copyVec3( zeniObj->volume->boxMax, powerVolumeDef->property_structs.volume->vBoxMax );
						powerVolumeDef->property_structs.volume->fSphereRadius = zeniObj->volume->sphereRadius;

						powerVolumeDef->property_structs.hull = StructCreate( parse_GroupHullProperties );
						eaPush( &powerVolumeDef->property_structs.hull->ppcTypes, allocAddString( "Power" ));
						powerVolumeDef->property_structs.physical_properties.bOnlyAVolume = true;

						powerVolume = ugcSolarSystemPlaceGroupDef( powerVolumeDef, "Volume", zerovec3, fZeroRot, damageRoot, &to_place );
						quatToMat( zeniObj->qOrientation, powerVolume->mat );
						copyVec3( zeniObj->pos, powerVolume->mat[ 3 ]);
					}
				} FOR_EACH_END;
			}
		}

		//Create parent def for all objects
		{
			Vec3 vOffset;
			setVec3(vOffset, 0, 0, 0);
			system_root_def = ugcSolarSystemMakeGroupDef(def_lib, "ObjectsRoot");
			system_object = ugcSolarSystemPlaceGroupDef(system_root_def, "Objects", vOffset, fZeroRot, NULL, &to_place);
		}

		// Sort by platform group & populate
		ugcGenerateObjectSort(gen_data->objects, &sorted_objects);
		FOR_EACH_IN_EARRAY_FORWARDS(sorted_objects, UGCGenesisObject, object)
		{
			ugcPopulateObject(iPartitionIdx, &to_place, table, object, system_object, NULL, spawn_pos[1], play_min, play_max);
		}
		FOR_EACH_END;
		
		mission_select_object = ugcSolarSystemPlaceGroupDef(NULL, "Missions", NULL, fZeroRot, system_object, &to_place);
		mission_select_object->params = ugcGenesisCreateMultiMissionWrapperParams(); 
		
		FOR_EACH_IN_EARRAY_FORWARDS(gen_data->missions, GenesisZoneSharedUGCMission, mission)
		{
			UGCGenesisMissionRequirements *mission_req = NULL;
			mission_object = ugcSolarSystemPlaceGroupDef(NULL, mission->mission_name, NULL, fZeroRot, mission_select_object, &to_place);

			FOR_EACH_IN_EARRAY(mission_reqs, UGCGenesisMissionRequirements, req)
			{
				if (stricmp(req->missionName, mission->mission_name) == 0)
				{
					mission_req = req;
					break;
				}
			}
			FOR_EACH_END;

			// Sort by platform group & populate
			ugcGenerateObjectSort(mission->objects, &sorted_objects);
			FOR_EACH_IN_EARRAY_FORWARDS(sorted_objects, UGCGenesisObject, object)
			{
				ugcPopulateObject(iPartitionIdx, &to_place, table, object, mission_object, mission_req, spawn_pos[1], play_min, play_max);
			}
			FOR_EACH_END;
		}
		FOR_EACH_END;

		eaDestroy(&sorted_objects);
	}
	
	ugcGenesisPopulateWaypointVolumes(&to_place, mission_reqs);

	//Actually place all the objects
	ugcPlaceObjects(zmap_info, &to_place, layer->grouptree.root_def);

	exclusionGridFree(g_PrefabPlatformGrid);
	g_PrefabPlatformGrid = NULL;

	loadend_printf("done.");

	mersenneTableFree(table);
	StructReset( parse_UGCGenesisToPlaceState, &to_place );
}

static GroupDef* ugcGeneratePathNodeDef( GroupDef* parentDef, const char* defName, Vec3 position )
{
	GroupDef* pathNodeTemplate = objectLibraryGetGroupDefByName( "PathNode", false );
	GroupChild* newChild = StructCreate( parse_GroupChild );
	GroupDef* newNode;
	eaPush( &parentDef->children, newChild );


	assert( pathNodeTemplate );
	newNode = groupLibCopyGroupDef( parentDef->def_lib, parentDef->filename, pathNodeTemplate, defName, false, true, false, 0, false);

	newChild->name_uid = newNode->name_uid;
	identityMat4( newChild->mat );
	copyVec3( position, newChild->mat[ 3 ]);
	newChild->uid_in_parent = eaSize( &parentDef->children );

	return newNode;
}

static int ugcGeneratePathNodeDefID( const StashTable defIDByPathNodeID, int connectionID )
{
	int result;
	if( stashIntFindInt( defIDByPathNodeID, connectionID, &result )) {
		return result;
	}

	return 0;
}

static void ugcGeneratePathNodeDefs( GroupDef*** out_peaNodes, GroupDef* parentDef, const char* name, const ZoneMapMetadataPathNode** eaPathNodes, const Mat4 mat )
{
	GroupDef** newNodes = NULL;
			
	// Create all the nodes
	FOR_EACH_IN_EARRAY_FORWARDS( eaPathNodes, const ZoneMapMetadataPathNode, pathNode ) {
		char buffer[ 256 ];
		Vec3 pos;
		GroupDef* newNode;

		sprintf( buffer, "%s__%d", name, FOR_EACH_IDX( eaPathNodes, pathNode ));
		mulVecMat4( pathNode->pos, mat, pos );							
		newNode = ugcGeneratePathNodeDef( parentDef, buffer, pos );
		eaPush( &newNodes, newNode );
	} FOR_EACH_END;

	// Create all the connections
	{
		StashTable defIDByPathNodeID = stashTableCreateInt( eaSize( &eaPathNodes ));
		int it;

		assert( eaSize( &eaPathNodes ) == eaSize( &newNodes ));
		for( it = 0; it != eaSize( &eaPathNodes ); ++it ) {
			stashIntAddInt( defIDByPathNodeID, eaPathNodes[ it ]->defUID, newNodes[ it ]->name_uid, true );
		}
		
		FOR_EACH_IN_EARRAY( newNodes, GroupDef, defNode ) {
			const ZoneMapMetadataPathNode* pathNodeMetadata = eaPathNodes[ FOR_EACH_IDX( newNodes, defNode )];

			defNode->property_structs.path_node_properties = StructCreate( parse_WorldPathNodeProperties );

			FOR_EACH_IN_EARRAY_FORWARDS( pathNodeMetadata->eaConnections, const ZoneMapMetadataPathEdge, connection ) {
				WorldPathEdge* newConnection = StructCreate( parse_WorldPathEdge );
				eaPush( &defNode->property_structs.path_node_properties->eaConnections, newConnection );
				newConnection->uOther = ugcGeneratePathNodeDefID( defIDByPathNodeID, connection->uOther );
				newConnection->bUGCGenerated = true;
			} FOR_EACH_END;
		} FOR_EACH_END;

		stashTableDestroy( defIDByPathNodeID );
	}

	if( out_peaNodes ) {
		eaPushEArray( out_peaNodes, &newNodes );
	}
	eaDestroy( &newNodes );
}

/// NOTE: This function assumes that all the group defs are direct
/// children of the layer root def.
const F32* ugcGeneratePathNodesGetDefPos( ZoneMapLayer* layer, GroupDef* def )
{
	FOR_EACH_IN_EARRAY( layer->grouptree.root_def->children, GroupChild, child ) {
		if( child->name_uid == def->name_uid ) {
			// Don't use pos, because at this during generation only
			// the mat is filled out.
			return child->mat[ 3 ];
		}
	} FOR_EACH_END;

	return zerovec3;
}

static bool ugcGeneratePathNodesFindNearestNode( GroupDef** out_ppThisNearestNode, GroupDef** out_ppLayerNearestNode, ZoneMapLayer* layer, GroupDef** eaNodes, const char* restrictToChallenge )
{
	float bestCost = FLT_MAX;
	GroupDef* bestThis = NULL;
	GroupDef* bestLayer = NULL;
	char restrictToChallengeWithSeparator[ 256 ];

	if( restrictToChallenge ) {
		sprintf( restrictToChallengeWithSeparator, "%s__", restrictToChallenge );
	} else {
		restrictToChallengeWithSeparator[ 0 ] = '\0';
	}
	
	FOR_EACH_IN_EARRAY( eaNodes, GroupDef, thisNode ) {
		assert( thisNode->property_structs.path_node_properties );
		FOR_EACH_IN_STASHTABLE( layer->grouptree.def_lib->defs, GroupDef, layerNode ) {
			float cost;
			if(   layerNode == thisNode || !layerNode->property_structs.path_node_properties
				  || eaFind( &eaNodes, layerNode ) >= 0 ) {
				continue;
			}

			// Because the challenge could be in any mission, all of
			// these names need to be detected if restrictToChallenge
			// is "Challenge_1":
			//
			// * Shared_Challenge_1
			// * Shared_Challenge_1__100
			// * Mission_Challenge_1
			// * Mission_Challenge_1__100
			//
			// But not this one:
			// 
			// * Shared_Challenge_100
			if( !nullStr( restrictToChallenge )) {
				char layerNodeNameWithoutSeparator[ 256 ];
				char* separatorPos;
				strcpy( layerNodeNameWithoutSeparator, layerNode->name_str );
				separatorPos = strstr( layerNodeNameWithoutSeparator, "__" );
				if( separatorPos ) {
					*separatorPos = '\0';
				}

				if( strEndsWith( layerNodeNameWithoutSeparator, restrictToChallenge )) {
					continue;
				}
			}
			
			cost = distance3Squared( ugcGeneratePathNodesGetDefPos( layer, thisNode ), ugcGeneratePathNodesGetDefPos( layer, layerNode ));
			if( cost < bestCost ) {
				bestCost = cost;
				bestThis = thisNode;
				bestLayer = layerNode;
			}
		} FOR_EACH_END;
	} FOR_EACH_END;

	*out_ppThisNearestNode = bestThis;
	*out_ppLayerNearestNode = bestLayer;
	return bestThis != NULL && bestLayer != NULL;
}

/// Link the nearest nodes from LAYER and EA-NODES, optionally only
/// looking at nodes that came from RESTRICT-TO-CHALLENGE.
static void ugcGeneratePathNodesLinkNearestNode( ZoneMapLayer* layer, GroupDef** eaNodes, const char* restrictToChallenge )
{
	GroupDef* thisNearestNode = NULL;
	GroupDef* layerNearestNode = NULL;
				
	if( ugcGeneratePathNodesFindNearestNode( &thisNearestNode, &layerNearestNode, layer, eaNodes, restrictToChallenge )) {
		{
			WorldPathEdge* thisToLayer = StructCreate( parse_WorldPathEdge );
			eaPush( &thisNearestNode->property_structs.path_node_properties->eaConnections, thisToLayer );
			thisToLayer->uOther = layerNearestNode->name_uid;
		}
		{			
			WorldPathEdge* layerToThis = StructCreate( parse_WorldPathEdge );
			eaPush( &layerNearestNode->property_structs.path_node_properties->eaConnections, layerToThis );
			layerToThis->uOther = thisNearestNode->name_uid;
		}
	}
}

static int ugcGeneratePathNodesSort( const UGCGenesisObject** ppObj1, const UGCGenesisObject** ppObj2 )
{
	// Ensure the order for objects is:
	// 1. Obj w/ no links
	// 2. Obj w/ link to specific challenge
	// 3. Obj w/ link to any challenge
	//
	// This won't work if an object links to a challenge that has
	// another link in it, but that's not legal now.
	int index1;
	int index2;
	
	if( eaSize( &(*ppObj1)->eastrPathNodesAutoconnectChallenge )) {
		index1 = 1;
	} else if( (*ppObj1)->bPathNodesAutoconnectNearest ) {
		index1 = 2;
	} else {
		index1 = 0;
	}
	
	if( eaSize( &(*ppObj2)->eastrPathNodesAutoconnectChallenge )) {
		index2 = 1;
	} else if( (*ppObj2)->bPathNodesAutoconnectNearest ) {
		index2 = 2;
	} else {
		index2 = 0;
	}

	return index1 - index2;
}

void ugcGeneratePathNodesLayer(ZoneMapLayer *layer, UGCGenesisZoneShared *gen_data)
{
	GroupDefLib* defLib = layer->grouptree.def_lib;
	GroupDef* parentDef = layer->grouptree.root_def;
	
	UGCGenesisObject** eaSortedObjects = NULL;

	eaPushEArray( &eaSortedObjects, &gen_data->objects );

	// The completed mission (index 0) has the assumed final state of
	// all objects.  Path nodes can be generated based on that.
	if( eaSize( &gen_data->missions ) > 0 ) { 
		eaPushEArray( &eaSortedObjects, &gen_data->missions[ 0 ]->objects );
	}

	eaQSort( eaSortedObjects, ugcGeneratePathNodesSort );

	if( !nullStr( gen_data->external_map_name )) {
		ZoneMapExternalMapSnap* zeniSnap = RefSystem_ReferentFromString( "ZoneMapExternalMapSnap", gen_data->external_map_name );
		if( zeniSnap ) {
			Mat4 zoneMapMat;

			// ZoneMap metadata has positions of the actual path
			// nodes, not the GroupDefs that generated them.
			//
			// De-offset them because they will get offset by
			// PATH_NODE_Y_OFFSET during binning.
			copyMat4( unitmat, zoneMapMat );
			zoneMapMat[ 3 ][ 1 ] = -PATH_NODE_Y_OFFSET;
			ugcGeneratePathNodeDefs( NULL, parentDef, "ZoneMap", zeniSnap->eaPathNodes, zoneMapMat );
		}
	}

	FOR_EACH_IN_EARRAY_FORWARDS( eaSortedObjects, UGCGenesisObject, object ) {
		GroupDef** objectNodes = NULL;
			
		switch( object->ePathNodesFrom ) {
			xcase UGCGenesisPathNodesFrom_GroupDef: {
				UGCGroupDefMetadata* defMetadata = ugcResourceGetGroupDefMetadataInt( object->obj.name_uid );

				if( defMetadata ) {
					Mat4 objectMat;
					createMat3DegYPR( objectMat, object->params.rotation );
					copyVec3( object->params.position, objectMat[ 3 ]);

					ugcGeneratePathNodeDefs( &objectNodes, parentDef, object->challenge_name, defMetadata->eaPathNodes, objectMat );
				}
			}
				
			xcase UGCGenesisPathNodesFrom_ChallengePos: {
				GroupDef* newNode = ugcGeneratePathNodeDef( parentDef, object->challenge_name, object->params.position );
				eaPush( &objectNodes, newNode );
			}
		}

		if( eaSize( &object->eastrPathNodesAutoconnectChallenge )) {
			if( object->bPathNodesAutoconnectNearest ) {
				ugcRaiseErrorInternalCode( UGC_ERROR, "Object %s -- Has both PathNodesAutoconnectChallenge and PathNodesAutoconnectNearest set.  Only one will be used.", object->challenge_name );
			}

			FOR_EACH_IN_EARRAY( object->eastrPathNodesAutoconnectChallenge, char, otherChallenge ) {
				ugcGeneratePathNodesLinkNearestNode( layer, objectNodes, otherChallenge );
			} FOR_EACH_END;
		} else if( object->bPathNodesAutoconnectNearest ) {
			ugcGeneratePathNodesLinkNearestNode( layer, objectNodes, NULL );
		}
		
		eaDestroy( &objectNodes );
	} FOR_EACH_END;

	eaDestroy( &eaSortedObjects );
}

#define WORLD_BOUNDS 14900.0f
#define MINI_SS_SIZE 10000.0f
#define SYSTEM_OVERVIEW_SCALE 0.1f

void ugcGenerateSpaceMiniLayer(ZoneMapInfo *zmap_info, UGCGenesisMissionRequirements **mission_reqs, ZoneMapLayer *layer, UGCGenesisZoneShared *data)
{
	UGCGenesisToPlaceState to_place = { 0 };
	GroupDefLib *solarsystem_lib = layer->grouptree.def_lib;
	float fZeroRot=0.0f;

	loadstart_printf("Populating solar system layer %s...", layerGetFilename(layer));

	{
		Vec3 solar_system_center;

		solar_system_center[0] = 0;
		solar_system_center[1] = -WORLD_BOUNDS - MINI_SS_SIZE/2.0f - MINI_SS_SIZE;
		solar_system_center[2] = 0;

		//Add the mini solar system representations of all the shoeboxes
		{
			UGCGenesisToPlaceObject *tagged_object;
			char id_str[16] = "";

			tagged_object = ugcSolarSystemPlaceGroupDef(NULL, "Planet", solar_system_center, fZeroRot, NULL, &to_place);
			tagged_object->params = StructCreate( parse_UGCGenesisProceduralObjectParams );
			tagged_object->params->physical_properties.iTagID = 1;
		}

		// Actually place all the objects
		ugcPlaceObjects(zmap_info, &to_place, layer->grouptree.root_def);
	}

	loadend_printf("Done.");

	StructReset( parse_UGCGenesisToPlaceState, &to_place );
}

static GroupDef *ugcGenesisInstancePath(GroupDefLib *def_lib, GroupDef *def, char *path)
{
	int *pathIndexes = groupDefScopeGetIndexesFromPath(def, path);
	char new_name[128];
		
	GroupDef *defIt = def;
	int it;
	for( it = 0; it != eaiSize( &pathIndexes ); ++it ) {
		GroupChild *child = defIt->children[ pathIndexes[ it ]];
		GroupDef *child_def = groupChildGetDef(defIt, child, false);
		GroupDef *instanced;

		assert(child_def);
		groupLibMakeGroupName(def_lib, child_def->name_str, SAFESTR(new_name), 0);
		instanced = groupLibCopyGroupDef(def_lib, NULL, child_def, new_name, false, true, false, 0, false);
			
		defIt = child_def;
	}

	eaiDestroy(&pathIndexes);
	return defIt;
}


//If you change the order of things in this function, you must also change exclusionGetDefVolumes
void ugcGenesisApplyActorData(UGCGenesisInstancedChildParams ***ea_actor_data, GroupDef *challenge_def, const Mat4 parent_mat)
{
	int i, j;

	if(challenge_def->property_structs.encounter_properties)
	{
		WorldEncounterProperties *enc_props = challenge_def->property_structs.encounter_properties;
		for ( j=0; j < eaSize(&enc_props->eaActors); j++ )
		{
			UGCGenesisInstancedChildParams *actor_data;
			WorldActorProperties *actor = enc_props->eaActors[j];

			actor_data = eaGet(ea_actor_data, j);

			if(!actor_data)
				continue;

			copyVec3(actor_data->vOffset, actor->vPos);
			copyVec3(actor_data->vPyr, actor->vRot);

			if(!actor->critterGroupDisplayNameMsg.pEditorCopy || nullStr(actor->critterGroupDisplayNameMsg.pEditorCopy->pcDefaultString))
				StructCopyAll(parse_DisplayMessage, &actor_data->critterGroupDisplayNameMsg, &actor->critterGroupDisplayNameMsg);

			if(!actor->displayNameMsg.pEditorCopy || nullStr(actor->displayNameMsg.pEditorCopy->pcDefaultString))
				StructCopyAll(parse_DisplayMessage, &actor_data->displayNameMsg, &actor->displayNameMsg);

			if(actor_data->pCostumeProperties)
			{
				if(!actor->pCostumeProperties)
					actor->pCostumeProperties = StructCreate(parse_WorldActorCostumeProperties);

				StructCopyAll(parse_WorldActorCostumeProperties, actor_data->pCostumeProperties, actor->pCostumeProperties);
			}
		}
		eaDestroyStruct(ea_actor_data, parse_UGCGenesisInstancedChildParams);
	}

	for (i = 0; i < eaSize(&challenge_def->children); i++)
	{
		GroupChild *child = challenge_def->children[i];
		GroupDef *child_def = groupChildGetDef(challenge_def, child, false);
		if (child_def)
		{
			Mat4 child_mat;
			mulMat4(parent_mat, child->mat, child_mat);
			ugcGenesisApplyActorData(ea_actor_data, child_def, child_mat);
		}
	}
}

static void ugcGenesisBuildObjectPath(UGCGenesisToPlaceObject *object, char *path, int path_size)
{
	char tmp[10];
	if (object->parent)
		ugcGenesisBuildObjectPath(object->parent, path, path_size);
	sprintf(tmp, "%d,", object->uid_in_parent);
	strcat_s(path, path_size, tmp);
}

static void ugcGenesisSetInternalObjectLogicalName(UGCGenesisToPlaceObject *object, const char *external_name, const char *internal_name, const char *internal_path, GroupDef *root_def, LogicalGroup *group)
{
	// Set external name
	char path[256] = { 0 };
	ugcGenesisBuildObjectPath(object, path, 256);
	assert(internal_path || internal_name);
	if (internal_path)
	{
		strcat(path, internal_path);
	}
	else if (internal_name && stricmp(internal_name, "") != 0)
	{
		strcatf(path, "%s,", internal_name);
	}
	groupDefScopeSetPathName(root_def, path, external_name, false);
	if (!groupDefScopeIsNameUsed(root_def, external_name))
		ugcRaiseErrorInternal(UGC_ERROR, OBJECT_LIBRARY_DICT, object->group_def->name_str,
							  "Object library piece specifies it has a child group with logical path \"%s\", but no child group was found.", path );

	if (group)
	{
		// Add to logical group
		eaPush(&group->child_names, StructAllocString(external_name));
	}
}

void ugcPlaceObjects(ZoneMapInfo *zmap_info, UGCGenesisToPlaceState *to_place, GroupDef *root_def)
{
	int i, j;
	MersenneTable *random_table = NULL;
	
	if(zmap_info && zmap_info->genesis_data)
		random_table = mersenneTableCreate(0);

	////////////////////////////////////////////////////////
	/// 1. Put patrols into the objects list
	{
		UGCGenesisToPlaceObject* patrol_parent = StructCreate( parse_UGCGenesisToPlaceObject );
		patrol_parent->object_name = allocAddString("Patrol Routes");
		identityMat4(patrol_parent->mat);
		eaPush(&to_place->objects, patrol_parent);

		// convert patrols into objects
		for (i = 0; i < eaSize(&to_place->patrols); i++)
		{
			UGCGenesisToPlacePatrol *patrol = to_place->patrols[i];
			UGCGenesisToPlaceObject *object = StructCreate( parse_UGCGenesisToPlaceObject );
			object->challenge_name = StructAllocString(patrol->patrol_name);
			object->challenge_is_unique = true;
			object->parent = patrol_parent;
			identityMat4(object->mat);
			object->params = StructCreate(parse_UGCGenesisProceduralObjectParams);
			object->params->patrol_properties = StructClone( parse_WorldPatrolProperties, &patrol->patrol_properties );
			eaPush(&to_place->objects, object);
		}
	}

	////////////////////////////////////////////////////////
	/// 2. Put all the objects into layers.
	for (i = 0; i < eaSize(&to_place->objects); i++)
	{
		int idx;
		UGCGenesisToPlaceObject *object = to_place->objects[i];
		GroupDef *parent = object->parent ? object->parent->group_def : root_def;
		char *internal_path = NULL;
		char *spawn_internal_path = NULL;
		char *external_name = NULL;
		GroupDef *group_to_name = NULL;
		bool wrapper_def_uid = 0;
		bool object_needs_interact = false;
		bool instanced_def = false;
		const WorldUGCProperties *ugc_properties;

		assert (parent && parent->def_lib->zmap_layer);
		idx = eaSize(&parent->children);
		object->uid_in_parent = 1;
		for (j = 0; j < eaSize(&parent->children); j++)
			if (parent->children[j]->uid_in_parent >= object->uid_in_parent)
				object->uid_in_parent = parent->children[j]->uid_in_parent+1;
		eaPush(&parent->children, StructCreate(parse_GroupChild));
		parent->children[idx]->uid_in_parent = object->uid_in_parent;
		if (!object->group_def)
			object->group_def = objectLibraryGetGroupDef(object->uid, false);
		if (!object->group_def)
		{
			object->group_def = groupLibFindGroupDef(root_def->def_lib, object->uid, false);
		}
		if (!object->group_def)
		{
			// if uid is 0, then we create a new def from scratch
			char groupName[256];
			GroupDefLib *def_lib = root_def->def_lib;
			groupLibMakeGroupName(def_lib, object->object_name, SAFESTR(groupName), 0);
			object->group_def = groupLibNewGroupDef(def_lib, root_def->filename, 0, groupName, 0, false, true);
			groupDefModify(object->group_def, UPDATE_GROUP_PROPERTIES, true);
		}
		ugc_properties = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", object->uid );

		// Generate the external name for this object
		if (object->challenge_name) {
			size_t external_name_size = strlen(object->challenge_name)+4;
			external_name = calloc(1, external_name_size);

			if (!object->challenge_is_unique)
			{
				sprintf_s(SAFESTR2(external_name), "%s_%02d", object->challenge_name, object->challenge_index);
			}
			else
			{
				object->challenge_index = 0;
				strcpy_s(external_name, external_name_size, object->challenge_name);
			}
		}
		
		/*
		if (SAFE_MEMBER(genesis_challenge_properties, spawn_name))
		{
			char *child_path;
			stashFindPointer(object->group_def->name_to_path, genesis_challenge_properties->spawn_name, &child_path);
			strdup_alloca(spawn_internal_path, child_path);
		}
		*/
		
		if (!groupIsObjLib(object->group_def))
		{
			if (object->params)
			{
				ugcGenesisApplyObjectParams(object->group_def, object->params);
			}
			if (object->instanced)
			{
				ugcGenesisApplyInstancedObjectParams(zmapInfoGetPublicName(zmap_info), object->group_def, object->instanced, object->interact, external_name, object->source_context);
			}
		}
		else if (object->instanced || (object->interact && ugc_properties && ugc_properties->groupDefProps.strClickableName))
		{
			// We have to instance this GroupDef.
			char new_name[128];
			GroupDef *new_def, *challenge_def;
			const char *complete_name = NULL;

			groupLibMakeGroupName(root_def->def_lib, object->group_def->name_str, SAFESTR(new_name), 0);
			new_def = groupLibCopyGroupDef(root_def->def_lib, root_def->filename, object->group_def, new_name, false, true, false, 0, false);
			challenge_def = new_def;
			
			if (ugc_properties && ugc_properties->groupDefProps.strClickableName)
				complete_name = ugc_properties->groupDefProps.strClickableName;
			if (complete_name)
			{
				GroupDef *instanced_groupdef;
				char *child_path;
				if (stashFindPointer(object->group_def->name_to_path, complete_name, &child_path) &&
					(instanced_groupdef = ugcGenesisInstancePath(root_def->def_lib, new_def, child_path))) {
					challenge_def = instanced_groupdef;
					//strdup_alloca(internal_path, child_path);
				} else {
					ugcRaiseErrorInternal(UGC_ERROR, "ObjectLibrary", object->group_def->name_str,
											  "Attempt to instance child \"%s\", but no such child group found.",
											  complete_name );
				}
			}
			
			if (object->instanced)
			{
				if (!nearSameVec3(object->instanced->model_scale, zerovec3))
				{
					copyVec3(object->instanced->model_scale, challenge_def->model_scale);
				}

				//Apply Child Positions
				if(eaSize(&object->instanced->eaChildParams))
				{
					if( !object->instanced->bChildParamsAreGroupDefs ) {
						ugcGenesisApplyActorData(&object->instanced->eaChildParams, challenge_def, object->mat);
						assert(!object->instanced->eaChildParams);
					} else {
						int it;
						if( eaSize( &object->instanced->eaChildParams ) != eaSize( &challenge_def->children )) {
							ugcRaiseErrorInternalCode( UGC_ERROR, "ObjectLibrary", object->group_def->name_str,
													   "Attempt to override %d positions, but def does not have that amount of children",
														   eaSize( &challenge_def->children ));
						} else {
							for( it = 0; it < eaSize( &object->instanced->eaChildParams ); ++it ) {
								const UGCGenesisInstancedChildParams* childParam = object->instanced->eaChildParams[ it ];
								GroupChild* child = challenge_def->children[ it ];

								copyVec3( childParam->vOffset, child->pos );
								copyVec3( childParam->vPyr, child->rot );
								createMat3YPR( child->mat, child->rot );
								copyVec3( child->pos, child->mat[ 3 ]);
							}
						}
					}
				}

				if( object->instanced->pcFSMName ) {
					WorldEncounterProperties *enc_props = challenge_def->property_structs.encounter_properties;
					int it;
					for( it = 0; it != eaSize( &enc_props->eaActors ); ++it ) {
						WorldActorProperties *actor = enc_props->eaActors[it];
						SET_HANDLE_FROM_STRING(gFSMDict, object->instanced->pcFSMName, actor->hFSMOverride);
					}
				}

				ugcGenesisApplyInstancedObjectParams(zmapInfoGetPublicName(zmap_info), challenge_def, object->instanced, object->interact, external_name, object->source_context);
			}
			else if (object->interact)
			{
				ugcGenesisApplyInteractObjectParams(zmapInfoGetPublicName(zmap_info), challenge_def, object->interact, external_name, object->source_context);
			}

			if (object->params)
			{
				ugcGenesisApplyObjectParams(new_def, object->params);
			}

			object->group_def = new_def;

			instanced_def = true;
		}

		//parent->children[idx]->def = object->group_def;
		if(!isNonZeroMat3(object->mat))
			ugcRaiseErrorInternalCode( UGC_FATAL_ERROR, "Object: %s -- Rotation matrix that is all zeros, please find a programmer.", object->object_name );
		copyMat4(object->mat, parent->children[idx]->mat);
		if (!object->mat_relative && object->parent)
		{
			// TomY TODO multiply by parent's inverse rotation
			// If you do this, then make it a separate flag, as code is relying on only the position being relative

			// NOTE: This setup would never have worked, but there may
			// be code depending on the current bug state.
			UGCGenesisToPlaceObject* parentIt = object->parent;
			do {
				if( parentIt->mat_relative && !isZeroVec3( parentIt->mat[3] )) {
					ugcRaiseErrorInternalCode( UGC_ERROR, "Object: %s -- This uses absolute positioning, but all of its parents do not.", object->object_name );
					break;
				}
				parentIt = parentIt->parent;
			} while( parentIt );
			
			subVec3(object->mat[3], object->parent->mat[3], parent->children[idx]->mat[3]);
		}

		parent->children[idx]->name_uid = object->group_def->name_uid;
		if(!object->seed && random_table)
			parent->children[idx]->seed = randomMersenneU32(random_table);
		else
			parent->children[idx]->seed = object->seed;
		parent->children[idx]->scale = object->scale;

		FOR_EACH_IN_EARRAY(object->eaChildParams, UGCGenesisGroupDefChildParam, child_param)
		{
			GroupChildParameter *parameter = StructCreate(parse_GroupChildParameter);
			parameter->parameter_name = child_param->astrParameter;
			parameter->int_value = child_param->iValue;
			parameter->string_value = child_param->astrValue;
			eaPush(&parent->children[idx]->simpleData.params, parameter);
		}
		FOR_EACH_END;

		groupSetBounds(parent, false);

		if (wl_state.genesis_error_on_encounter1)
		{
			if( object->group_def->property_structs.encounter_hack_properties ) {
				ugcRaiseErrorContext(UGC_FATAL_ERROR, object->source_context,
									 "Encounter 1 objects are not allowed.");
			}
		}

		object_needs_interact =
			(object->challenge_name > 0 && !groupDefNeedsUniqueName(object->group_def)) // Object needs a logical name
			|| (object->interact && (!instanced_def || object->interact->clickieVisibleWhenCond)) // Object needs interaction properties added to it
			|| object->force_named_object;

		if ((object_needs_interact || object->params) && groupIsObjLib(object->group_def))
		{
			// Create a wrapper GroupDef with an interaction so that we can assign a name to this object
			char groupName[256];
			GroupDef *named_def;
			GroupDefLib *def_lib = root_def->def_lib;
			groupLibMakeGroupName(def_lib, object->challenge_name, SAFESTR(groupName), 0);
			named_def = groupLibNewGroupDef(def_lib, root_def->filename, 0, groupName, 0, false, true);
			groupDefModify(named_def, UPDATE_GROUP_PROPERTIES, true);

			if (object->interact)
			{
				if (!instanced_def)
					ugcGenesisApplyInteractObjectParams(zmapInfoGetPublicName(zmap_info), named_def, object->interact, external_name, object->source_context);
				ugcGenesisApplyObjectVisibilityParams(named_def, object->interact, external_name, object->source_context);
			}

			if(object->params)
				ugcGenesisApplyObjectParams(named_def, object->params);

			// Insert the def between this one and its parent
			eaPush(&named_def->children, StructCreate(parse_GroupChild));
			named_def->children[0]->name_uid = object->group_def->name_uid;
			named_def->children[0]->seed = object->seed;
			named_def->children[0]->scale = object->scale;
			named_def->children[0]->uid_in_parent = 1;
			identityMat4(named_def->children[0]->mat);

			parent->children[idx]->name_uid = named_def->name_uid;
			parent->children[idx]->seed = 0;
			parent->children[idx]->scale = 0;
			groupSetBounds(parent, false);

			group_to_name = named_def;

			wrapper_def_uid = 1;
		}
		else
		{
			group_to_name = object->group_def;

			if (object->interact)
			{
				if (!instanced_def)
					ugcGenesisApplyInteractObjectParams(zmapInfoGetPublicName(zmap_info), object->group_def, object->interact, external_name, object->source_context);
				ugcGenesisApplyObjectVisibilityParams(object->group_def, object->interact, external_name, object->source_context);
			}
		}

		if( object_needs_interact )
		{
			// If there are still no interact properties, force the issue by adding a NamedObject entry
			if (!group_to_name->property_structs.interaction_properties ||
				eaSize(&group_to_name->property_structs.interaction_properties->eaEntries) == 0)
			{
				WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );
				entry->pcInteractionClass = allocAddString( "NamedObject" );
				if (!group_to_name->property_structs.interaction_properties)
					group_to_name->property_structs.interaction_properties = StructCreate( parse_WorldInteractionProperties );
				eaPush(&group_to_name->property_structs.interaction_properties->eaEntries, entry);
			}
		}

		if (object->challenge_name && (groupDefNeedsUniqueName(group_to_name)
									   || (instanced_def && object->challenge_type != GenesisChallenge_None)))
		{
			LogicalGroup *group;
			const char *internal_name = NULL;

			if( !internal_name && ugc_properties && ugc_properties->groupDefProps.strClickableName ) {
				internal_name = ugc_properties->groupDefProps.strClickableName;
			}
			if( !internal_name ) {
				internal_name = "";
			}

			group = NULL;

			// Create a logical group
			{
				char group_name[ 256 ];
				sprintf( group_name, "LogGrp_%s", object->challenge_name );
				
				for (j = 0; j < eaSize(&root_def->logical_groups); j++)
				{
					if (!strcmp(root_def->logical_groups[j]->group_name, group_name))
					{
						group = root_def->logical_groups[j];
						break;
					}
				}

				if (!group)
				{
					group = StructCreate(parse_LogicalGroup);
					group->group_name = StructAllocString(group_name);
					eaPush(&root_def->logical_groups, group);
				}
			}
			//printf("Placing %s\n", external_name);

			if( wrapper_def_uid && !nullStr( internal_name )) {
				char buffer[ 256 ];
				sprintf( buffer, "%d,%s,", wrapper_def_uid, internal_name );
				strdup_alloca( internal_path, buffer );
			}
			ugcGenesisSetInternalObjectLogicalName(object, external_name, internal_name, internal_path, root_def, group);

			FOR_EACH_IN_EARRAY(object->eaRoomDoors, UGCGenesisRoomDoorSwitch, detail)
			{
				char detail_name[256];
				char path[256];
				char *existing_name = NULL;
				sprintf(detail_name, "%s_DET_%d", external_name, detail->iIndex);
				if (object->group_def->path_to_name && stashFindPointer(object->group_def->path_to_name, detail->astrScopePath, &existing_name))
				{
					if (wrapper_def_uid)
						sprintf(path, "%d,%s,", wrapper_def_uid, existing_name);
					else
						strcpy(path, existing_name);
				}
				else
				{
					if (wrapper_def_uid)
						sprintf(path, "%d,%s,", wrapper_def_uid, detail->astrScopePath);
					else
						strcpy(path, detail->astrScopePath);
				}
				ugcGenesisSetInternalObjectLogicalName(object, detail_name, NULL, path, root_def, group);

				// Also create a parameter
				{
					GroupChildParameter *parameter = StructCreate(parse_GroupChildParameter);
					parameter->parameter_name = allocAddString( existing_name );
					parameter->int_value = detail->iSelected;
					eaPush(&parent->children[idx]->simpleData.params, parameter);
				}
			}
			FOR_EACH_END;
		}

		// Give children names, if necessary
		if(   object->instanced && eaSize( &object->instanced->eaChildParams )
			  && object->instanced->bChildParamsAreGroupDefs
			  && eaSize( &object->instanced->eaChildParams ) == eaSize( &object->group_def->children )) {
			int it;
			for( it = 0; it < eaSize( &object->instanced->eaChildParams ); ++it ) {
				const UGCGenesisInstancedChildParams* childParam = object->instanced->eaChildParams[ it ];
				GroupChild* child = object->group_def->children[ it ];

				if( !nullStr( childParam->pcLogicalName )) {
					char* childScopeName = NULL;

					groupDefFindScopeNameByFullPath( object->group_def, &child->uid_in_parent, 1, &childScopeName );
					if( childScopeName ) {
						assert( !wrapper_def_uid );
						ugcGenesisSetInternalObjectLogicalName( object, childParam->pcLogicalName, childScopeName, NULL, root_def, NULL );
					} else {
						ugcRaiseErrorInternal( UGC_ERROR, OBJECT_LIBRARY_DICT, object->group_def->name_str,
											   "Object library piece is being used as a teleporter, but the children's scope name could not be found." );
					}
				}
			}
		}

		if (object->trap_name)
		{
			char path[256];
			if (wrapper_def_uid)
				sprintf(path, "%d,Trap_Core,", wrapper_def_uid);
			else
				strcpy(path, "Trap_Core,");
			ugcGenesisSetInternalObjectLogicalName(object, object->trap_name, NULL, path, root_def, NULL);
		}

/*
		if (object->spawn_name)
		{
			if (spawn_internal_path)
			{
				ugcGenesisSetInternalObjectLogicalName(object, object->spawn_name, object->group_def->property_structs.genesis_challenge_properties->spawn_name, spawn_internal_path, root_def, NULL);
			}
			else
			{
				GroupDef *def = object->group_def ? object->group_def : objectLibraryGetGroupDef(object->uid, false);
				if (groupDefScopeIsNameUsed(def, "SpawnPoint"))
					ugcGenesisSetInternalObjectLogicalName(object, object->spawn_name, "SpawnPoint", NULL, root_def, NULL);
				else
				{
					// Create a spawn point here
					UGCGenesisToPlaceObject *spawn_object = calloc(1, sizeof(UGCGenesisToPlaceObject));
					GroupDef *spawn_def = objectLibraryGetGroupDefByName("Goto Spawn Point", false);
					spawn_object->uid = spawn_def->name_uid;
					copyMat4(object->mat, spawn_object->mat);
					spawn_object->mat_relative = object->mat_relative;
					spawn_object->challenge_name = strdup(object->spawn_name);
					spawn_object->parent = object->parent;
					eaPush(&to_place->objects, spawn_object);
				}
			}
		}
		*/
		
		SAFE_FREE(external_name);
	}

	if(random_table)
		mersenneTableFree(random_table);
}

static void ugcReportGenerationError(UGCRuntimeStatus *generated_status, const char *stage, const UGCProjectData *data)
{
	UGCRuntimeError* error = NULL;
	if(error = ugcStatusMostImportantError(generated_status))
		AssertOrAlertWarningWithStruct("UGC_GENERATION_INTERNAL_ERROR", parse_UGCRuntimeError, error, "Stage: %s, Namespace: %s", stage, data->ns_name);
}

////////////////////////////////////////////////////////////
// Calculate a map's parent map
////////////////////////////////////////////////////////////

static const char *gslUGC_CalculateLastStaticMap(UGCProjectData* ugc_proj, const char *map_name)
{
	UGCMapTransitionInfo **transition_infos = ugcMissionGetMapTransitions(ugc_proj, ugc_proj->mission->objectives);
	const char *last_static_map = NULL;
	FOR_EACH_IN_EARRAY_FORWARDS(transition_infos, UGCMapTransitionInfo, info) {
		UGCMissionMapLink *link = ugcMissionFindLinkByObjectiveID( ugc_proj, info->objectiveID, false );

		if( !info->prevIsInternal ) {
			if( info->prevMapName ) {
				last_static_map = allocAddString( info->prevMapName );
			}
			if( link ) {
				UGCComponent* door_component = ugcComponentFindByID( ugc_proj->components, link->uDoorComponentID );
				if( door_component && door_component->sPlacement.pcExternalMapName ) {
					last_static_map = allocAddString( door_component->sPlacement.pcExternalMapName );
				}
			}
		}

		if( link && resNamespaceBaseNameEq( link->strSpawnInternalMapName, map_name )) {
			break;
		}
	} FOR_EACH_END;

	eaDestroyStruct( &transition_infos, parse_UGCMapTransitionInfo );
	return last_static_map;
}


////////////////////////////////////////////////////////////
// Main entry point to generate a UGC project
////////////////////////////////////////////////////////////

static WorldVariableDef* ugcGenesisInternVariableDef( ZoneMapInfo *zminfo, WorldVariableDef *varDef)
{
	int i;
	for( i = eaSize(&zminfo->variable_defs) - 1; i >= 0; --i ) {
		if( zminfo->variable_defs[i]->pcName == varDef->pcName ) {
			return zminfo->variable_defs[i];
		}
	}

	{
		WorldVariableDef* newVarDef = StructClone( parse_WorldVariableDef, varDef );
		eaPush(&zminfo->variable_defs, newVarDef);
		
		return newVarDef;
	}
}

/// Ensure that ZMINFO has all variables a genesis map should have.
///
/// If REMOVE-OTHER-VARS is true, then also remove any non-genesis map
/// vars.
///
/// This should be kept in sync with genesisVariableDefNames()
void ugcGenesisInternVariableDefs(ZoneMapInfo* zminfo)
{
	bool isStarClusterMap = false;
	bool isUGCGeneratedMap = true;
	bool removeOtherVars = true;

	static WorldVariableDef* missionNumDef = NULL;
	static WorldVariableDef* mapEntryKeyDef = NULL;
	if( !missionNumDef ) {
		missionNumDef = StructCreate( parse_WorldVariableDef );
		
		missionNumDef->pcName = allocAddString( "Mission_Num" );
		missionNumDef->eType = WVAR_INT;
		missionNumDef->eDefaultType = WVARDEF_SPECIFY_DEFAULT;
		missionNumDef->pSpecificValue = StructAlloc( parse_WorldVariable );
		missionNumDef->pSpecificValue->pcName = missionNumDef->pcName;
		missionNumDef->pSpecificValue->eType = WVAR_INT;
		missionNumDef->pSpecificValue->iIntVal = 0;
	}
	if( !mapEntryKeyDef ) {
		mapEntryKeyDef = StructCreate( parse_WorldVariableDef );

		mapEntryKeyDef->pcName = allocAddString( "MAP_ENTRY_KEY" );
		mapEntryKeyDef->eType = WVAR_STRING;
		mapEntryKeyDef->eDefaultType = WVARDEF_SPECIFY_DEFAULT;
		mapEntryKeyDef->pSpecificValue = StructAlloc( parse_WorldVariable );
		mapEntryKeyDef->pSpecificValue->pcName = mapEntryKeyDef->pcName;
		mapEntryKeyDef->pSpecificValue->eType = WVAR_STRING;
		mapEntryKeyDef->pSpecificValue->pcStringVal = StructAllocString( "" );
	}

	ugcGenesisInternVariableDef( zminfo, missionNumDef );

	if( isStarClusterMap ) {
		ugcGenesisInternVariableDef( zminfo, mapEntryKeyDef );
	}

	{
		WorldVariableDef** defs = NULL;
		int it;
			
		if( isUGCGeneratedMap ) {
			defs = ugcGetDefaultVariableDefs();
		}

			
		for( it = 0; it != eaSize( &defs ); ++it ) {
			ugcGenesisInternVariableDef( zminfo, defs[ it ]);
		}

		// remove other vars
		if( removeOtherVars ) {
			int defaultIt;
			for( it = eaSize( &zminfo->variable_defs ) - 1; it >= 0; --it ) {
				WorldVariableDef* varDef = zminfo->variable_defs[ it ];
				bool found = false;

				if( defs ) {
					for( defaultIt = 0; defaultIt != eaSize( &defs ); ++defaultIt ) {
						WorldVariableDef* defaultVarDef = defs[ defaultIt ];

						if( stricmp( varDef->pcName, defaultVarDef->pcName ) == 0 ) {
							found = true;
							break;
						}
					}
				}
				if( stricmp( varDef->pcName, missionNumDef->pcName ) == 0 ) {
					found = true;
				}
				if( isStarClusterMap && stricmp( varDef->pcName, mapEntryKeyDef->pcName ) == 0 ) {
					found = true;
				}

				if( !found ) {
					eaRemove( &zminfo->variable_defs, it );
				}
			}
		}
	}
}

bool ugcProjectGenerateOnServerEx(UGCProjectData *data, const char *override_spawn_map, Vec3 override_spawn_pos, Vec3 override_spawn_rot)
{
	ZoneMapInfo *zminfo;
	UGCGenesisZoneMapData *gen_data;
	UGCRuntimeStatus *generated_status;
	ResourceActionList actionList = { 0 };
	UGCPerProjectDefaults *defaults = ugcGetDefaults();
	bool success = true;
	char mission_name[RESOURCE_NAME_MAX_SIZE];

	// The project was likely just sent over the network.  Make sure
	// it has the stashtable.
	FixupStructLeafFirst( parse_UGCProjectData, data, FIXUPTYPE_POST_TEXT_READ, NULL );

	// Also, make sure all the strings so far are normalized
	UTF8NormalizeAllStringsInStruct( parse_UGCProjectData, data );
	
	if (!data->project_prefix)
	{
		char project_prefix[256];
		if (!resExtractNameSpace_s(data->mission->name, NULL, 0, SAFESTR(project_prefix)))
		{
			strcpy(project_prefix, data->mission->name);
		}
		data->project_prefix = StructAllocString(project_prefix);
	}

	if (!nullStr(data->ns_name))
		sprintf(mission_name, "%s:%s", data->ns_name, data->project_prefix);
	else
		strcpy(mission_name, data->project_prefix);

	// Clear out data from previous generation
	ugcLayerCacheClear();

	// Set trivia for any error tracker reporting
	gslUGC_AddTriviaData( data );

	// Generate each costume (and report errors)
	FOR_EACH_IN_EARRAY(data->costumes, UGCCostume, pUGCCostume)
	{
		PlayerCostume* pPCCostume = ugcCostumeGeneratePlayerCostume( pUGCCostume, NULL, data->project_prefix );
		
		if(pPCCostume) {
			// Write the file
			resSetDictionaryEditMode(g_hPlayerCostumeDict, true);
			resSetDictionaryEditMode( gMessageDict, true );
			resAddRequestLockResource( &actionList, g_hPlayerCostumeDict, pPCCostume->pcName, pPCCostume);
			resAddRequestSaveResource( &actionList, g_hPlayerCostumeDict, pPCCostume->pcName, pPCCostume);
		}
	}
	FOR_EACH_END;

	// Generate items & drop tables
	FOR_EACH_IN_EARRAY(data->items, UGCItem, pItem)
	{
		char item_name[RESOURCE_NAME_MAX_SIZE];
		char path[MAX_PATH];
		ItemDef *pOutItem = StructCreate(parse_ItemDef);
		RewardTable *pOutTable = StructCreate(parse_RewardTable);
		RewardEntry *pOutEntry = StructCreate(parse_RewardEntry);

		sprintf(item_name, "%s:%s", data->ns_name, pItem->astrName);

		// Create the item
		
		pOutItem->pchName = allocAddString(item_name);
		pOutItem->flags = kItemDefFlag_BindOnPickup;
		pOutItem->pchIconName = ugcItemGetIconName( pItem );
		pOutItem->displayNameMsg.pEditorCopy = langCreateMessage("", "", "", pItem->strDisplayName);
		pOutItem->descriptionMsg.pEditorCopy = langCreateMessage("", "", "", pItem->strDescription);
		pOutItem->eType = kItemType_Mission;
		SET_HANDLE_FROM_STRING("Mission", mission_name, pOutItem->hMission);
		item_FixMessages(pOutItem);

		// Create the drop table

		pOutTable->pchName = allocAddString(item_name);
		pOutTable->Algorithm = kRewardAlgorithm_GiveAll;
		pOutTable->PickupType = kRewardPickupType_FromOrigin;

		pOutEntry->ChoiceType = kRewardChoiceType_Choice;
		pOutEntry->Type = kRewardType_Item;
		SET_HANDLE_FROM_STRING("RewardTable", item_name, pOutEntry->hItemDef);
		eaPush(&pOutTable->ppRewardEntry, pOutEntry);

		// Write out the files

		sprintf(path, "Maps/%s/Items", data->project_prefix);
		pOutItem->pchScope = allocAddString(path);
		pOutTable->pchScope = allocAddString(path);

		resSetDictionaryEditMode(g_hItemDict, true);
		resSetDictionaryEditMode(g_hRewardTableDict, true);
		resSetDictionaryEditMode( gMessageDict, true );
		resAddRequestLockResource( &actionList, g_hItemDict, pOutItem->pchName, pOutItem);
		resAddRequestSaveResource( &actionList, g_hItemDict, pOutItem->pchName, pOutItem);
		resAddRequestLockResource( &actionList, g_hRewardTableDict, pOutTable->pchName, pOutTable);
		resAddRequestSaveResource( &actionList, g_hRewardTableDict, pOutTable->pchName, pOutTable);
	}
	FOR_EACH_END;

	// Apply costume actions then clean up memory
	TellControllerToLog( __FUNCTION__ ": About to request resource actions." );
	resRequestResourceActions( &actionList );
	StructDeInit( parse_ResourceActionList, &actionList );
	TellControllerToLog( __FUNCTION__ ": Resource actions done." );

	// Generate root mission (and report errors)
	generated_status = StructCreate(parse_UGCRuntimeStatus);
	ugcSetStageAndAdd(generated_status, "Project Mission Generation");		
	ugcMissionGenerate(data);
	ugcClearStage();
	ugcReportGenerationError(generated_status, "Mission Generation", data);
	StructDestroySafe( parse_UGCRuntimeStatus, &generated_status );

	// Generate each map (and report errors)
	FOR_EACH_IN_EARRAY(data->maps, UGCMap, map)
	{
		UGCGenesisBackdrop *ugc_backdrop = NULL;
		char base_map_name[RESOURCE_NAME_MAX_SIZE];
		const char *start_spawn;
		char map_filename[MAX_PATH];
		ZoneMapLightOverrideType light_type = MAP_LIGHT_OVERRIDE_NONE;
		UGCGenesisMissionRequirements **mission_reqs = NULL;
		bool override_spawn = override_spawn_pos && stricmp_safe(map->pcName, override_spawn_map) == 0;
		int mission_idx;

		if (map->pUnitializedMap)
			continue;

		// Set light type
		
		if (map->pSpace)
		{
			light_type = MAP_LIGHT_OVERRIDE_USE_PRIMARY;
		}
		else if (map->pPrefab)
		{
			UGCMapType map_type = ugcMapGetPrefabType(map->pPrefab->map_name, false);
			switch (map_type)
			{
			xcase UGC_MAP_TYPE_PREFAB_SPACE:
				light_type = MAP_LIGHT_OVERRIDE_USE_PRIMARY;
			xcase UGC_MAP_TYPE_PREFAB_INTERIOR:
				light_type = MAP_LIGHT_OVERRIDE_USE_SECONDARY;
			}
		}

		loadstart_printf("Generating map %s...", map->pcName);

		resExtractNameSpace_s(map->pcName, NULL, 0, SAFESTR(base_map_name));

		// Initialize dummy ZoneMapInfo

		if(override_spawn)
			start_spawn = "SPAWN_OVERRIDE";
		else
			start_spawn = ugcProjectGetMapStartSpawnTemp(data, map);
		sprintf(map_filename, "ns/%s/UGC/%s.ugcmap", data->ns_name, base_map_name);
		zminfo = zmapInfoCreateUGCDummy(map->pcName, data->project_prefix, map_filename, ugcMapDisplayName(map), namespaceIsUGC(data->ns_name) ? ugcProjectContainerID(data) : 0, start_spawn, light_type);
		zmapInfoSetMapForceTeamSize( zminfo, 1 );

		{
			const char* lastStaticMap = gslUGC_CalculateLastStaticMap(data, map->pcName);
			if( lastStaticMap ) {
				zminfo->pParentMap = StructCreate(parse_ParentZoneMap);
				zminfo->pParentMap->pchMapName = lastStaticMap;
				zminfo->pParentMap->pchSpawnPoint = StructAllocString(START_SPAWN);
			}
		}

		gen_data = StructCreate( parse_UGCGenesisZoneMapData );
		gen_data->map_desc = StructCreate( parse_UGCGenesisMapDescription );

		if (map->pSpace)
		{
			UGCSound *sound = NULL;

			gen_data->map_desc->space_ugc = StructClone(parse_UGCGenesisSpace, map->pSpace);
			assert(gen_data->map_desc->space_ugc);
		}
		else if (map->pPrefab)
		{
			UGCMapType map_type = ugcMapGetPrefabType(map->pPrefab->map_name, false);
			gen_data->map_desc->prefab_ugc = StructClone(parse_UGCGenesisPrefab, map->pPrefab);
			assert(gen_data->map_desc->prefab_ugc);
		}
		else
		{
			assert(0); // This is a completely faily failure case we should never get to. --TomY
		}

		generated_status = StructCreate(parse_UGCRuntimeStatus);

		// UGC mission -> Genesis mission conversion
		ugcSetStageAndAdd(generated_status, "MapMission Pre-Generation");
		if( override_spawn ) {
			ugcMissionGenerateForMap(data, base_map_name, gen_data->map_desc, override_spawn_pos, override_spawn_rot);
		} else {
			ugcMissionGenerateForMap(data, base_map_name, gen_data->map_desc, NULL, NULL);
		}

		if( g_UGCWriteOutMapDesc ) {
			char buffer[ CRYPTIC_MAX_PATH ];

			changeFileExt( zminfo->filename, ".mapdesc", buffer );
			ParserWriteTextFile( buffer, parse_UGCGenesisMapDescription, gen_data->map_desc, 0, 0 );
		}
		
		// Transmogrify map
		ugcTransmogrifyMapDesc(zminfo, gen_data, generated_status);

		if (!ugcStatusFailed(generated_status))
		{
			// Genesis mission generation
			ugcSetStageAndAdd(generated_status, "MapMission Generation");
			for (mission_idx = 0; gen_data && mission_idx != eaSize(&gen_data->genesis_mission); ++mission_idx)
			{
				UGCGenesisMissionRequirements *req;
				req = ugcGenesisGenerateMission(zminfo, gen_data, mission_idx, gen_data->genesis_mission[mission_idx], NULL, true, data->project_prefix, true);
				if( !req ) {
					eaDestroyStruct( &mission_reqs, parse_UGCGenesisMissionRequirements );
					break;
				}
				else
				{
					eaPush( &mission_reqs, req );
				}
			}
		}

		if (!ugcStatusFailed(generated_status))
		{
			ugcSetStageAndAdd(generated_status, "Map Generation");
		
			// Generate ZoneMaps & layer geometry
			// genesisFixupZoneMapInfo(zminfo);
			ugcGenesisInternVariableDefs(zminfo);
#if 0			
	// WOLF[23Jul12] Fog was poorly implemented. See [COR-16358] Commenting out for now
			ugcGenerateBackdrop(zminfo, data->project_prefix, ugc_backdrop);
#endif			
			ugcGenerateGeometry(zminfo, data->project_prefix, gen_data, ugc_backdrop, mission_reqs);
			zmapInfoRemoveMapDesc(zminfo);
			if (!namespaceIsUGC(data->ns_name))
				zmapInfoClearUGCFile(zminfo);
			zmapInfoSetModified(zminfo);

			TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": About to save ZoneMap, file=%s", zminfo->filename ));
			zmapInfoSave(zminfo);
			TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": ZoneMap save done, file=%s", zminfo->filename ));
		}
		else
		{
			success = false;
		}
		ugcClearStage();

		eaDestroyStruct( &mission_reqs, parse_UGCGenesisMissionRequirements );

		ugcReportGenerationError(generated_status, "Map Generation", data);

		// Free dummy ZoneMapInfo
		zmapInfoDestroyUGCDummy(zminfo);
		StructDestroy(parse_UGCRuntimeStatus, generated_status);
		StructDestroy(parse_UGCGenesisZoneMapData, gen_data);

		loadend_printf("Done.");
	}
	FOR_EACH_END;

	// Clear error tracker trivia
	gslUGC_RemoveTriviaData();

	return success;
}

TextParserResult fixupUGCGenesisToPlacePlatformGroup( UGCGenesisToPlacePlatformGroup *pStruct, enumTextParserFixupType eType, void *pExtraData )
{
	switch( eType ) {
		xcase FIXUPTYPE_DESTRUCTOR:
			exclusionGridFree( pStruct->platform_grid );
	}

	return PARSERESULT_SUCCESS;
}

// WOLF[23Jul12] Fog was poorly implemented. See [COR-16358] Commenting out for now
/// #include "AutoGen/gslUGCGenerate_c_ast.c"
//

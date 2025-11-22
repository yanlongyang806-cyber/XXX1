//// The Genesis system -- Mission generation
#pragma once
GCC_SYSTEM

typedef struct ContactCostume ContactCostume;
typedef struct Expression Expression;
typedef struct GroupDef GroupDef;
typedef struct ImageMenuItemOverride ImageMenuItemOverride;
typedef struct InteractableOverride InteractableOverride;
typedef struct MissionDef MissionDef;
typedef struct MissionOfferOverride MissionOfferOverride;
typedef struct UGCGenesisEpisode UGCGenesisEpisode;
typedef struct UGCGenesisInstancedObjectParams UGCGenesisInstancedObjectParams;
typedef struct UGCGenesisInteractObjectParams UGCGenesisInteractObjectParams;
typedef struct UGCGenesisMapDescription UGCGenesisMapDescription;
typedef struct UGCGenesisMissionChallenge UGCGenesisMissionChallenge;
typedef struct UGCGenesisMissionContext UGCGenesisMissionContext;
typedef struct UGCGenesisMissionCostume UGCGenesisMissionCostume;
typedef struct UGCGenesisMissionDescription UGCGenesisMissionDescription;
typedef struct UGCGenesisMissionPrompt UGCGenesisMissionPrompt;
typedef struct UGCGenesisMissionRequirements UGCGenesisMissionRequirements;
typedef struct UGCGenesisMissionZoneChallenge UGCGenesisMissionZoneChallenge;
typedef struct UGCGenesisProceduralEncounterProperties UGCGenesisProceduralEncounterProperties;
typedef struct UGCGenesisZoneMapData UGCGenesisZoneMapData;
typedef struct UGCGenesisZoneMission UGCGenesisZoneMission;
typedef struct UGCRuntimeErrorContext UGCRuntimeErrorContext;
typedef struct WorldGameActionProperties WorldGameActionProperties;
typedef struct ZoneMap ZoneMap;
typedef struct ZoneMapInfo ZoneMapInfo;

AUTO_STRUCT;
typedef struct UGCGenesisMissionAdditionalParams {
	InteractableOverride** eaInteractableOverrides;
	MissionOfferOverride** eaMissionOfferOverrides;
	ImageMenuItemOverride** eaImageMenuItemOverrides;
	WorldGameActionProperties** eaSuccessActions;
} UGCGenesisMissionAdditionalParams;
extern ParseTable parse_UGCGenesisMissionAdditionalParams[];
#define TYPE_parse_UGCGenesisMissionAdditionalParams UGCGenesisMissionAdditionalParams

UGCGenesisZoneMission* ugcGenesisTransmogrifyMission(ZoneMapInfo* zmap_info, UGCGenesisMapDescription* map_desc, int mission_num);
UGCGenesisMissionZoneChallenge** ugcGenesisTransmogrifySharedChallenges(ZoneMapInfo* zmap_info, UGCGenesisMapDescription* map_desc);
void ugcGenesisTransmogrifyChallengePEPHack( UGCGenesisMapDescription* map_desc, int mission_num, UGCGenesisMissionChallenge* challenge, UGCGenesisProceduralEncounterProperties*** outPepList );

// genesisApplyInstancedObjectParams will automatically call genesisApplyInteractObjectParams.
// Call genesisApplyInteractObjectParams only when the object is not instanced. (Used in UGC)
void ugcGenesisApplyInstancedObjectParams(const char *zmap_name, GroupDef *def, UGCGenesisInstancedObjectParams *params, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext); 
void ugcGenesisApplyInteractObjectParams(const char *zmap_name, GroupDef *def, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext);
void ugcGenesisApplyObjectVisibilityParams(GroupDef *def, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext);

void ugcGenesisDeleteMissions(const char* zmap_filename);
UGCGenesisMissionRequirements* ugcGenesisGenerateMission(ZoneMapInfo* zmap_info, UGCGenesisZoneMapData* genesis_data, int mission_num, UGCGenesisZoneMission* mission, UGCGenesisMissionAdditionalParams* additionalParams, bool is_ugc, const char *project_prefix, bool write_mission);

// TomY ENCOUNTER_HACK support -- needed even with NO_EDITORS
UGCGenesisMissionPrompt* ugcGenesisFindPromptPEPHack( UGCGenesisMissionDescription* missionDesc, char* prompt_name );
Expression* ugcGenesisCreateEncounterSpawnCond(UGCGenesisMissionContext* context, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties);
Expression* ugcGenesisCreateEncounterDespawnCond(UGCGenesisMissionContext* context, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties);
void ugcGenesisCreateEncounterSpawnCondText(UGCGenesisMissionContext* context, char** estr, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties);
void ugcGenesisCreateEncounterDespawnCondText(UGCGenesisMissionContext* context, char** estr, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties);
Expression* ugcGenesisCreateChallengeSpawnCond(UGCGenesisMissionContext* context, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties);
void ugcGenesisCreateChallengeSpawnCondText(UGCGenesisMissionContext* context, char** estr, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties, bool isEncounter);

void ugcGenesisMissionMessageFillKeys( MissionDef * accum, const char* root_mission_name );

UGCGenesisMissionZoneChallenge* ugcGenesisFindZoneChallenge( UGCGenesisZoneMapData* zmap_data, UGCGenesisZoneMission* zone_mission, const char* challenge_name );
UGCGenesisMissionZoneChallenge* ugcGenesisFindZoneChallengeRaw( UGCGenesisZoneMapData* zmap_data, UGCGenesisZoneMission* zone_mission, UGCGenesisMissionZoneChallenge** override_challenges, const char* challenge_name );

// Structure conversion functions
void ugcGenesisMissionCostumeToContactCostume( UGCGenesisMissionCostume* genesisCostume, ContactCostume* contactCostume );
void ugcGenesisMissionCostumeFromContactCostume( UGCGenesisMissionCostume* genesisCostume, ContactCostume* contactCostume );

// Exposed for UGC:
const char* ugcGenesisMissionNameRaw( const char* zmapName, const char* genesisMissionName, bool isOpenMission );
const char* ugcGenesisContactNameRaw( const char* zmapName, const char* missionName, const char* challengeName );
const char *ugcGenesisMissionVolumeName(const char* layout_name, const char *mission_name);

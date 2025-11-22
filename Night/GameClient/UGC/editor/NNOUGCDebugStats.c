#include "file.h"
#include "utils.h"
#include "fileutil.h"
#include "fileutil2.h"
#include "referencesystem.h"
#include "error.h"
#include "StaticWorld/group.h"
#include "ResourceInfo.h"
#include "ResourceSearch.h"
#include "ObjectLibrary.h"
#include <sys/stat.h>

#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "NNOUGCResource.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "wlUGC.h"
#include "hoglib.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

typedef struct UGCDebugStat
{
	char *pLabel;
	U64 iCount;							// # of times hit in the current sample
	U64 iTotalCount;					// # of times hit since start
	U64 *eaiBucketCounts;				// One for each bucket in the group
} UGCDebugStat;

typedef struct UGCDebugStatGroup
{
	const char *pLabel;
	UGCDebugStat **eaStats;
	U64 iCount;							// # of samples taken
	U64 *eaiBuckets;					// Ranges for buckets: A, B, C means A..(B-1), B..(C-1), etc.
} UGCDebugStatGroup;

void UGCDebugStatFree(UGCDebugStat *stat)
{
	SAFE_FREE(stat->pLabel);
	eai64Destroy(&stat->eaiBucketCounts);
	SAFE_FREE(stat);
}

void UGCDebugStatGroupFree(UGCDebugStatGroup *group)
{
	eaDestroyEx(&group->eaStats, UGCDebugStatFree);
	eai64Destroy(&group->eaiBuckets);
	SAFE_FREE(group);
}

typedef struct UGCDebugStats
{
	UGCDebugStatGroup **eaStatGroups;

	UGCDebugStatGroup *pMapType;
	UGCDebugStatGroup *pMap;

	UGCDebugStatGroup *pComponentType;
	UGCDebugStatGroup *pComponentTypeMap;
	UGCDebugStatGroup *pComponentObject;
	UGCDebugStatGroup *pComponentShowTrigger;
	UGCDebugStatGroup *pComponentHideTrigger;
	UGCDebugStatGroup *pComponentPromptCount;
	UGCDebugStatGroup *pTotalPromptCount;

	UGCDebugStatGroup *pTransitionType;
	UGCDebugStatGroup *pTransitionMap;
	UGCDebugStatGroup *pTransitionObject;

	UGCDebugStatGroup *pObjectiveType;
	UGCDebugStatGroup *pObjectiveTypeInt;
	UGCDebugStatGroup *pObjectiveTypeExt;
	UGCDebugStatGroup *pObjectiveNest;
	UGCDebugStatGroup *pObjectiveWaypoints;
	UGCDebugStatGroup *pObjectivesPerMap;
	UGCDebugStatGroup *pObjectiveObject;
	UGCDebugStatGroup *pObjectiveBranchCt;

	UGCDebugStatGroup *pCostumeType;

	UGCDebugStatGroup *pFileSize;
} UGCDebugStats;

static UGCDebugStatGroup *UGCDebugStatGroupCreate(UGCDebugStats *stats, const char *label, const char **stat_labels, const int *buckets)
{
	int i;
	UGCDebugStatGroup *ret = calloc(1, sizeof(UGCDebugStatGroup));
	ret->pLabel = label;
	if (stat_labels)
	{
		i = 0;
		while (stat_labels[i])
		{
			UGCDebugStat *stat = calloc(1, sizeof(UGCDebugStat));
			stat->pLabel = strdup(stat_labels[i]);
			eaPush(&ret->eaStats, stat);
			i++;
		}
	}
	if (buckets)
	{
		i = 0;
		while (buckets[i] >= 0)
		{
			eai64Push(&ret->eaiBuckets, buckets[i]);
			i++;
		}
	}
	eaPush(&stats->eaStatGroups, ret);
	return ret;
}

static UGCDebugStat *UGCDebugStatGroupGetStatByLabel(UGCDebugStatGroup *group, const char *label)
{
	UGCDebugStat *ret;

	FOR_EACH_IN_EARRAY(group->eaStats, UGCDebugStat, stat)
	{
		if (stricmp(stat->pLabel, label) == 0)
			return stat;
	}
	FOR_EACH_END;

	ret = calloc(1, sizeof(UGCDebugStat));
	ret->pLabel = strdup(label);
	eaPush(&group->eaStats, ret);
	return ret;
}

static void UGCDebugStatGroupFinishSample(UGCDebugStatGroup *group)
{
	int i;
	FOR_EACH_IN_EARRAY(group->eaStats, UGCDebugStat, stat)
	{
		stat->iTotalCount += stat->iCount;
		if (eai64Size(&stat->eaiBucketCounts) < eai64Size(&group->eaiBuckets))
			eai64SetSize(&stat->eaiBucketCounts, eai64Size(&group->eaiBuckets));

		for (i = 0; i < eai64Size(&group->eaiBuckets); i++)
		{
			if (stat->iCount >= group->eaiBuckets[i] &&
				(i == (eai64Size(&group->eaiBuckets)-1) || stat->iCount < group->eaiBuckets[i+1]))
			{
				stat->eaiBucketCounts[i]++;
			}
		}
		stat->iCount = 0;
	}
	FOR_EACH_END;
	group->iCount++;
}

void UGCDebugGatherObjectiveStatistics(UGCProjectData *data, UGCDebugStats *stats, UGCMissionObjective** objectives, int recursion_depth)
{
	if (eaSize(&objectives) == 0)
		return;

	stats->pObjectiveNest->eaStats[0]->iCount = MAX(stats->pObjectiveNest->eaStats[0]->iCount, recursion_depth);

	FOR_EACH_IN_EARRAY(objectives, UGCMissionObjective, objective)
	{
		int type_idx = -1;
		bool is_internal = false;
		ugcObjectiveMapName(data, objective, &is_internal);

		switch (objective->type)
		{
		case UGCOBJ_ALL_OF:
			type_idx = 1; // Complete all

			stats->pObjectiveBranchCt->eaStats[0]->iCount = eaSize(&objective->eaChildren);
			UGCDebugStatGroupFinishSample(stats->pObjectiveBranchCt);
			break;
		case UGCOBJ_IN_ORDER:
			type_idx = 2; // One per column
			break;
		case UGCOBJ_COMPLETE_COMPONENT: case UGCOBJ_UNLOCK_DOOR:
			{
				UGCComponent *component = ugcComponentFindByID(data->components, objective->componentID);
				if (component)
				{
					switch (component->eType)
					{
						case UGC_COMPONENT_TYPE_KILL:
							type_idx = 3; // Kill
							break;
						case UGC_COMPONENT_TYPE_WHOLE_MAP:
						case UGC_COMPONENT_TYPE_ROOM_MARKER:
						case UGC_COMPONENT_TYPE_PLANET:
							type_idx = 4; // Reach Marker
							break;
						case UGC_COMPONENT_TYPE_CONTACT:
						case UGC_COMPONENT_TYPE_DIALOG_TREE:
							type_idx = 5; // Talk to Contact
							break;
						default:
							type_idx = 6; // Click
							break;
					}
				}
				if (component->sPlacement.bIsExternalPlacement)
				{
					char objname[256];
					sprintf(objname, "%s on %s", component->sPlacement.pcExternalObjectName, component->sPlacement.pcExternalMapName);
					UGCDebugStatGroupGetStatByLabel(stats->pObjectiveObject, objname)->iCount++;
				}
				stats->pObjectiveWaypoints->eaStats[objective->waypointMode]->iCount++; 
			}
			break;
		}

		stats->pObjectiveType->eaStats[0]->iCount++; // Total
		if (type_idx != -1)
			stats->pObjectiveType->eaStats[type_idx]->iCount++;

		if (is_internal)
		{
			stats->pObjectiveTypeInt->eaStats[0]->iCount++; // Total
			if (type_idx != -1)
				stats->pObjectiveTypeInt->eaStats[type_idx]->iCount++;
		}
		else
		{
			stats->pObjectiveTypeExt->eaStats[0]->iCount++; // Total
			if (type_idx != -1)
				stats->pObjectiveTypeExt->eaStats[type_idx]->iCount++;
		}

		UGCDebugGatherObjectiveStatistics(data, stats, objective->eaChildren, recursion_depth+1);
	}
	FOR_EACH_END;
}

void UGCDebugGatherProjectStatistics(UGCProjectData *data, UGCDebugStats *stats)
{
	// Maps and Map type
	FOR_EACH_IN_EARRAY(data->maps, UGCMap, map)
	{
		UGCMapType type = ugcMapGetType(map);
		if (type != UGC_MAP_TYPE_ANY)
		{
			stats->pMapType->eaStats[type]->iCount++;
			stats->pMapType->eaStats[0]->iCount++; // Total
		}

		if(map->pUnitializedMap)
			UGCDebugStatGroupGetStatByLabel(stats->pMap, "<UNINITIALIZED>")->iCount++;
		else if(map->pPrefab)
			UGCDebugStatGroupGetStatByLabel(stats->pMap, map->pPrefab->map_name)->iCount++;
		else if(map->pSpace)
			UGCDebugStatGroupGetStatByLabel(stats->pMap, "<SPACE>")->iCount++;

		UGC_FOR_EACH_COMPONENT_ON_MAP(data->components, map->pcName, component)
		{
			stats->pComponentTypeMap->eaStats[component->eType+1]->iCount++;
			stats->pComponentTypeMap->eaStats[0]->iCount++; // Total
		}
		UGC_FOR_EACH_COMPONENT_END;
		UGCDebugStatGroupFinishSample(stats->pComponentTypeMap);
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(data->components->eaComponents, UGCComponent, component)
	{
		GroupDef* pDef;
		int iComponentID = component->iObjectLibraryId;
		if (iComponentID==0)
		{
			iComponentID=ugcFixupGetIDFromName(component->pcOldObjectLibraryName);
		}
		if (iComponentID==0)
		{
			if( component->eType == UGC_COMPONENT_TYPE_REWARD_BOX ) {
				GroupDef* def = objectLibraryGetGroupDefByName( ugcDefaultsGetRewardBoxObjlib(), false );
				iComponentID = SAFE_MEMBER( def, name_uid );
			} else if( component->eType == UGC_COMPONENT_TYPE_RESPAWN ) {
				GroupDef* def = objectLibraryGetGroupDefByName( "UGC_Respawn_Point_With_Campfire", false );
				iComponentID = SAFE_MEMBER( def, name_uid );
			}
		}

		pDef = objectLibraryGetGroupDef(iComponentID, false);
		if (pDef!=NULL)
		{
			UGCDebugStatGroupGetStatByLabel(stats->pComponentObject, pDef->name_str)->iCount++;
		}

		// Don't count externalPlacement components. They don't use StartWhen or HideWhen.
		if (!component->sPlacement.bIsExternalPlacement)
		{
			if (component->pStartWhen)
			{
				if (component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE && eaSize(&component->blocksV1) > 0)
					stats->pComponentShowTrigger->eaStats[component->pStartWhen->eType]->iCount += eaSize(&component->blocksV1); 
				else
					stats->pComponentShowTrigger->eaStats[component->pStartWhen->eType]->iCount++; 
			}
			if (component->pHideWhen)
			{
				stats->pComponentHideTrigger->eaStats[component->pHideWhen->eType]->iCount++;
			}
		}
		stats->pComponentType->eaStats[component->eType+1]->iCount++;
		stats->pComponentType->eaStats[0]->iCount++; // Total

		if (component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE)
		{
			if (eaSize(&component->blocksV1) > 0)
			{
				FOR_EACH_IN_EARRAY(component->blocksV1, UGCDialogTreeBlock, block)
				{
					stats->pComponentPromptCount->eaStats[0]->iCount += eaSize(&block->prompts) + 1;
					stats->pTotalPromptCount->eaStats[0]->iCount += eaSize(&block->prompts) + 1;
				}
				FOR_EACH_END;
			}
			else
			{
				stats->pComponentPromptCount->eaStats[0]->iCount = eaSize(&component->dialogBlock.prompts) + 1;
				stats->pTotalPromptCount->eaStats[0]->iCount += eaSize(&component->dialogBlock.prompts) + 1; 
			}
			UGCDebugStatGroupFinishSample(stats->pComponentPromptCount);
		}
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(data->costumes, UGCCostume, costume)
	{
		stats->pCostumeType->eaStats[costume->eRegion+1]->iCount++;
		stats->pCostumeType->eaStats[0]->iCount++; // Total
	}
	FOR_EACH_END;

	UGCDebugGatherObjectiveStatistics(data, stats, data->mission->objectives, 0);

	{
		UGCMapTransitionInfo **transitions = ugcMissionGetMapTransitions(data, data->mission->objectives);
		FOR_EACH_IN_EARRAY(transitions, UGCMapTransitionInfo, transition)
		{
			UGCMissionObjective *obj = ugcObjectiveFind(data->mission->objectives, transition->objectiveID);
			if (obj)
			{
				int type = transition->prevIsInternal ? 2 : 0;
				bool is_internal = false;
				UGCMissionMapLink *link;
				const char *nextMap = ugcObjectiveMapName(data, obj, &is_internal);
				if (is_internal)
					type++;

				if (!transition->prevIsInternal)
				{
					link = ugcMissionFindLink(data->mission, data->components, nextMap, transition->prevMapName);
					if (link)
					{
						UGCComponent *door_component = ugcComponentFindByID(data->components, link->uDoorComponentID);
						if (door_component)
						{
							char objname[256];
							sprintf(objname, "%s on %s", door_component->sPlacement.pcExternalObjectName, door_component->sPlacement.pcExternalMapName);
							UGCDebugStatGroupGetStatByLabel(stats->pTransitionObject, objname)->iCount++;
						}
					}

					UGCDebugStatGroupGetStatByLabel(stats->pTransitionMap, transition->prevMapName)->iCount++;
				}

				stats->pTransitionType->eaStats[type+1]->iCount++;
				stats->pTransitionType->eaStats[0]->iCount++; // Total

				stats->pObjectivesPerMap->eaStats[0]->iCount = eaiSize(&transition->mapObjectiveIDs)+1;
				UGCDebugStatGroupFinishSample(stats->pObjectivesPerMap);
			}
		}
		FOR_EACH_END;
		eaDestroyStruct(&transitions, parse_UGCMapTransitionInfo);
	}

	UGCDebugStatGroupFinishSample(stats->pMap);
	UGCDebugStatGroupFinishSample(stats->pMapType);
	UGCDebugStatGroupFinishSample(stats->pComponentType);
	UGCDebugStatGroupFinishSample(stats->pComponentObject);
	UGCDebugStatGroupFinishSample(stats->pComponentShowTrigger);
	UGCDebugStatGroupFinishSample(stats->pComponentHideTrigger);
	UGCDebugStatGroupFinishSample(stats->pTransitionType);
	UGCDebugStatGroupFinishSample(stats->pTransitionMap);
	UGCDebugStatGroupFinishSample(stats->pTransitionObject);
	UGCDebugStatGroupFinishSample(stats->pObjectiveType);
	UGCDebugStatGroupFinishSample(stats->pObjectiveTypeInt);
	UGCDebugStatGroupFinishSample(stats->pObjectiveTypeExt);
	UGCDebugStatGroupFinishSample(stats->pObjectiveNest);
	UGCDebugStatGroupFinishSample(stats->pObjectiveObject);
	UGCDebugStatGroupFinishSample(stats->pObjectiveWaypoints);
	UGCDebugStatGroupFinishSample(stats->pCostumeType);
	UGCDebugStatGroupFinishSample(stats->pTotalPromptCount);
}

static int UGCDebugStatSort(const UGCDebugStat **stat_a, const UGCDebugStat **stat_b)
{
	if (stricmp((*stat_a)->pLabel, "Total") == 0)
		return -1;
	if (stricmp((*stat_b)->pLabel, "Total") == 0)
		return 1;

	if ((*stat_b)->iTotalCount >= (*stat_a)->iTotalCount)
		return (*stat_b)->iTotalCount - (*stat_a)->iTotalCount;
	return -1 * (int)((*stat_a)->iTotalCount - (*stat_b)->iTotalCount);
}

void UGCDebugGatherFileStatistics(UGCDebugStats *stats, const char *namespace, const char *localdir)
{
	char **files = NULL;
	char dirname[MAX_PATH];
	sprintf(dirname, "%s/data/ns/%s", localdir, namespace);

	files = fileScanDir(dirname);
	FOR_EACH_IN_EARRAY(files, char, dir_filename)
	{
		struct _stat64 file_stat = { 0 };
		if (_stat64(dir_filename, &file_stat) == 0)
		{
			int size = file_stat.st_size;
			char *ext = strrchr(dir_filename, '.');
			if (ext)
			{
				UGCDebugStatGroupGetStatByLabel(stats->pFileSize, ext + 1)->iCount += size;
			}
			UGCDebugStatGroupGetStatByLabel(stats->pFileSize, "Total")->iCount += size;
		}
	}
	FOR_EACH_END;
	fileScanDirFreeNames(files);
	UGCDebugStatGroupFinishSample(stats->pFileSize);
}

void UGCDebugInitStatGroups(UGCDebugStats *stats)
{
	{
		static const char *labels[] = { "Total", "Prefab Interior", "Custom Interior", "Prefab Space", "Custom Space", "Prefab Ground", "Custom Ground", NULL };
		static const int buckets[] = { 0, 1, 3, 5, -1 };
		stats->pMapType = UGCDebugStatGroupCreate(stats, "Map Types", labels, buckets);
	}
	stats->pMap = UGCDebugStatGroupCreate(stats, "Maps", NULL, NULL);
	{
		static const int buckets[] = { 0, 1, 5, 10, 15, 20, 30, 40, 50, -1 };
		const char **labels = NULL;
		int enum_idx = 1;
		eaPush(&labels, "Total");
		do
		{
			eaPush(&labels, UGCComponentTypeEnum[enum_idx].key);
			enum_idx++;
		} while (UGCComponentTypeEnum[enum_idx].key != DM_END);
		eaPush(&labels, NULL);
		stats->pComponentType = UGCDebugStatGroupCreate(stats, "Component Types", labels, buckets);
		stats->pComponentTypeMap = UGCDebugStatGroupCreate(stats, "Component Types (per map)", labels, buckets);
		eaDestroy(&labels);
	}
	{
		static const char *labels[] = { "Objective in Progress", "Objective Complete", "Objective Start", "Mission Start", "Map Start", "Component Complete", "Component Reached", "Current Component Complete", "Dialog Prompt Reached", "Player Has Item", "Manual", NULL };
		stats->pComponentShowTrigger = UGCDebugStatGroupCreate(stats, "Component Show Trigger", labels, NULL);
		stats->pComponentHideTrigger = UGCDebugStatGroupCreate(stats, "Component Hide Trigger", labels, NULL);
	}
	{
		static const char *labels[] = { "Count", NULL };
		static const int buckets[] = { 0, 1, 2, 3, 4, 5, 10, 15, 50, -1 };
		stats->pComponentPromptCount = UGCDebugStatGroupCreate(stats, "Prompt Count (per dialog)", labels, buckets);
	}
	{
		static const char *labels[] = { "Count", NULL };
		static const int buckets[] = { 0, 10, 20, 30, 40, 50, 100, 250, 500, -1 };
		stats->pTotalPromptCount = UGCDebugStatGroupCreate(stats, "Prompt Count (total)", labels, buckets);
	}
	stats->pComponentObject = UGCDebugStatGroupCreate(stats, "Component Objects", NULL, NULL);
	{
		static const char *labels[] = { "Total", "Cryptic -> Cryptic", "Cryptic -> UGC", "UGC -> Cryptic", "UGC -> UGC", NULL };
		stats->pTransitionType = UGCDebugStatGroupCreate(stats, "Transition Types", labels, NULL);
	}
	stats->pTransitionMap = UGCDebugStatGroupCreate(stats, "Transition Map", NULL, NULL);
	stats->pTransitionObject = UGCDebugStatGroupCreate(stats, "Transition Door", NULL, NULL);
	{
		static const char *labels[] = { "Count", NULL };
		static const int buckets[] = { 0, 1, 5, 10, 15, 20, 25, 30, 40, 50, -1 };
		stats->pObjectivesPerMap = UGCDebugStatGroupCreate(stats, "Objectives (per map)", labels, buckets);
	}
	{
		static const char *labels[] = { "Total", "Complete All", "Branches", "Kill", "Reach Marker", "Talk to Contact", "Click", NULL };
		static const int buckets[] = { 0, 1, 5, 15, 50, -1 };
		stats->pObjectiveType = UGCDebugStatGroupCreate(stats, "Objective Types", labels, buckets);
		stats->pObjectiveTypeInt = UGCDebugStatGroupCreate(stats, "Objective Types (Internal)", labels, buckets);
		stats->pObjectiveTypeExt = UGCDebugStatGroupCreate(stats, "Objective Types (External)", labels, buckets);
	}
	{
		static const char *labels[] = { "Depth", NULL };
		static const int buckets[] = { 0, 1, 3, 5, -1 };
		stats->pObjectiveNest = UGCDebugStatGroupCreate(stats, "Objective Recursion Depth", labels, buckets);
	}
	{
		static const char *labels[] = { "Branch Count", NULL };
		static const int buckets[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1 };
		stats->pObjectiveBranchCt = UGCDebugStatGroupCreate(stats, "Branches (per Complete All)", labels, buckets);
	}
	{
		static const char *labels[] = { "None", "Area", "Points", NULL };
		stats->pObjectiveWaypoints = UGCDebugStatGroupCreate(stats, "Objective Waypoint Mode", labels, NULL);
	}
	stats->pObjectiveObject = UGCDebugStatGroupCreate(stats, "Cryptic Object Objectives", NULL, NULL);

	{
		static const char *labels[] = { "Total", "None", "Space", "Ground", NULL };
		static const int buckets[] = { 0, 1, 5, 15, 20, 25, 30, -1 };
		stats->pCostumeType = UGCDebugStatGroupCreate(stats, "Costume Types", labels, buckets);
	}
	{
		static const int buckets[] = { 0, 16*1024, 32*1024, 64*1024, 128*1024, 256*1024, 512*1024, 1024*1024, 2*1024*1024, 4*1024*1024, 8*1024*1024, 16*1024*1024, 32*1024*1024, 64*1024*1024, 128*1024*1024, 256*1024*1024, -1 };
		stats->pFileSize = UGCDebugStatGroupCreate(stats, "File Size", NULL, buckets);
	}
}

UGCDebugStats *UGCDebugGatherStatisticsForProject(UGCProjectData *data)
{
	UGCDebugStats *stats = calloc(1, sizeof(UGCDebugStats));
	UGCDebugInitStatGroups(stats);
	UGCDebugGatherProjectStatistics(data, stats);
	return stats;
}

void UGCDebugGatherFreeStatistics(UGCDebugStats *stats)
{
	eaDestroyEx(&stats->eaStatGroups, UGCDebugStatGroupFree);
	SAFE_FREE(stats);
}

bool UGCDebugGatherStatisticsForProjectData(UGCProjectData *data, UGCDebugStats *stats, FILE *fErrorOut)
{
	UGCDebugStats *stats_pre, *stats_post;
	int numDialogsDeleted = 0, numCostumesReset = 0, numObjectivesReset = 0;
	int fixupFlags = 0;
	bool bFoundError = false;\
	UGCRuntimeStatus *validateRet = NULL;
	char localdir[MAX_PATH];

	sprintf(localdir, "%s/Data/", fileLocalDataDir());

	// Make sure we have the default components for the WHOLE_MAP and MISSION_RETURN. This is typically now done in UGCFixupCommon.c to adjust for the STOSeason4 to STOSeason5
	//  changes in the way Maps are handled. These components should always exist for all projects, so we don't want the count delta to show up as an error. WOLF[11May2012]
	{
		UGCComponent *pTheComponent;
		pTheComponent = ugcComponentOpExternalObjectFindOrCreate( data, ugcGetDefaultMapName( data ), "WHOLE_MAP" );
		pTheComponent = ugcComponentOpExternalObjectFindOrCreate( data, ugcGetDefaultMapName( data ), "MISSION_RETURN" );
	}

	// There were some fixups for Deprecated Maps that will trigger component count errors. Let's do the fix up now so we don't get those errors
	{
		bool bFixup=true;
		ugcProjectFixupDeprecated(data, bFixup);
	}

	stats_pre = UGCDebugGatherStatisticsForProject(data);
	ParserWriteTextFile("UGC/Pre.ugcproject", parse_UGCProjectData, data, 0, 0);

	ugcEditorFixupProjectData(data, &numDialogsDeleted, &numCostumesReset, &numObjectivesReset, &fixupFlags);
	ugcProjectFixupDeprecated(data, true);

	stats_post = UGCDebugGatherStatisticsForProject(data);
	ParserWriteTextFile("UGC/Post.ugcproject", parse_UGCProjectData, data, 0, 0);

	{
		int stats_idx, stats_sub_idx;
		for (stats_idx = 0; stats_idx < eaSize(&stats_pre->eaStatGroups); stats_idx++)
		{
			UGCDebugStatGroup *pre_group = stats_pre->eaStatGroups[stats_idx];
			UGCDebugStatGroup *post_group = stats_post->eaStatGroups[stats_idx];

			if ((fixupFlags & UGC_FIXUP_MOVED_DIALOG_BLOCKS) == 0 || pre_group != stats_pre->pComponentPromptCount)
			{
				if (pre_group->iCount != post_group->iCount)
				{
					if (!bFoundError)
						fprintf(fErrorOut, "*** PROJECT %s HAS ERRORS:\n", data->ns_name);
					fprintf(fErrorOut, "## Stat group %s has different counts: %I64u / %I64u\n", pre_group->pLabel, pre_group->iCount, post_group->iCount);
					bFoundError = true;
				}
			}
			for (stats_sub_idx = 0; stats_sub_idx < eaSize(&pre_group->eaStats); stats_sub_idx++)
			{
				int stats_sub_idx_2;
				UGCDebugStat *pre_stat = pre_group->eaStats[stats_sub_idx];
				UGCDebugStat *post_stat = NULL;
				for (stats_sub_idx_2 = 0; stats_sub_idx_2 < eaSize(&post_group->eaStats); stats_sub_idx_2++)
					if (stricmp(post_group->eaStats[stats_sub_idx_2]->pLabel, pre_stat->pLabel) == 0)
					{
						post_stat = post_group->eaStats[stats_sub_idx_2];
						break;
					}
					if (!post_stat)
					{
						if (!bFoundError)
							fprintf(fErrorOut, "*** PROJECT %s HAS ERRORS:\n", data->ns_name);
						fprintf(fErrorOut, "## Stat %s in group %s missing after fixup: %I64u\n", pre_stat->pLabel, pre_group->pLabel, pre_stat->iTotalCount);
						continue;
					}
					if ((fixupFlags & UGC_FIXUP_MOVED_DIALOG_BLOCKS)
						&& (pre_group == stats_pre->pComponentType || pre_group == stats_pre->pComponentTypeMap)
						&& (stats_sub_idx == (UGC_COMPONENT_TYPE_DIALOG_TREE+1) || stats_sub_idx == 0))
						continue;
					if ((fixupFlags & UGC_FIXUP_CLEARED_WHENS) &&
						(pre_group == stats_pre->pComponentShowTrigger || pre_group == stats_pre->pComponentHideTrigger))
						continue;
					if (pre_stat->iTotalCount != post_stat->iTotalCount)
					{
						if (!bFoundError)
							fprintf(fErrorOut, "*** PROJECT %s HAS ERRORS:\n", data->ns_name);
						fprintf(fErrorOut, "## Stat %s in group %s has different counts: %I64u / %I64u\n", pre_stat->pLabel, pre_group->pLabel, pre_stat->iTotalCount, post_stat->iTotalCount);
						bFoundError = true;
					}
			}
		}
	}

	// Validate the project
	ugcSetIsRepublishing(true);
	validateRet = StructCreate(parse_UGCRuntimeStatus);
	ugcSetStageAndAdd(validateRet, "UGC Validate");
	ugcValidateProject(data);
	ugcClearStage();
	ugcSetIsRepublishing(false);

	if (ugcStatusHasErrors(validateRet, UGC_ERROR))
	{
		int error_count = 0;
		if (!bFoundError)
			fprintf(fErrorOut, "*** PROJECT %s HAS ERRORS:\n", data->ns_name);
		bFoundError = true;
		FOR_EACH_IN_EARRAY(validateRet->stages, UGCRuntimeStage, stage)
		{
			if (eaSize(&stage->errors) > 0)
			{
				fprintf(fErrorOut, "## In stage %s:\n", stage->name);
				FOR_EACH_IN_EARRAY(stage->errors, UGCRuntimeError, error)
				{
					char *buf = NULL;
					estrCreate(&buf);
					ParserWriteText(&buf, parse_UGCRuntimeError, error, 0, 0, 0);
					fprintf(fErrorOut, "%s\n", buf);
					estrDestroy(&buf);
					error_count++;
				}
				FOR_EACH_END;
			}
		}
		FOR_EACH_END;
		fprintf(fErrorOut, "\n\n");
		Errorf("Project %s has %d validation errors", data->ns_name, error_count);
	}
	StructDestroy(parse_UGCRuntimeStatus, validateRet);

	UGCDebugGatherProjectStatistics(data, stats);
	UGCDebugGatherFileStatistics(stats, data->ns_name, localdir);

	return bFoundError;
}

void UGCDebugGatherStatisticsOutput(UGCDebugStats *stats, FILE *fOut)
{
	// Output data
	FOR_EACH_IN_EARRAY_FORWARDS(stats->eaStatGroups, UGCDebugStatGroup, group)
	{
		int i;
		fprintf(fOut, "*** %s ***,%I64u\n,Count,Avg", group->pLabel, group->iCount);
		for (i = 0; i < eai64Size(&group->eaiBuckets); i++)
			if (i < eai64Size(&group->eaiBuckets)-1)
				fprintf(fOut, ",%I64u..%I64u", group->eaiBuckets[i], group->eaiBuckets[i+1]-1);
			else
				fprintf(fOut, ",%I64u+", group->eaiBuckets[i]);
		fprintf(fOut, "\n");

		eaQSort(group->eaStats, UGCDebugStatSort);

		FOR_EACH_IN_EARRAY_FORWARDS(group->eaStats, UGCDebugStat, stat)
		{
			F32 avg = (group->iCount > 0) ? ((F32)stat->iTotalCount) / ((F32)group->iCount) : 0;
			fprintf(fOut, "%s,%I64u,%0.01f", stat->pLabel, stat->iTotalCount, avg);
			for (i = 0; i < eai64Size(&stat->eaiBucketCounts); i++)
				fprintf(fOut, ",%I64u", stat->eaiBucketCounts[i]);
			fprintf(fOut, "\n");
		}
		FOR_EACH_END;
		fprintf(fOut, "\n");
	}
	FOR_EACH_END;
}

AUTO_COMMAND;
void UGCDebugGatherStatistics(const char *filename)
{
	int length, i, project_error_count = 0;
	char *file_data = fileAlloc(filename, &length);
	char **proj_list = NULL;
	FILE *fOut, *fErrorOut;
	UGCDebugStats stats = { 0 };

	if (!file_data)
	{
		Alertf("Failed to read file.");
		return;
	}
	{
		char buf[MAX_PATH], outfilename[MAX_PATH];
		sprintf(buf, "UGC/UGC_Project_Statistics.csv");
		fileLocateWrite(buf, outfilename);	
		makeDirectoriesForFile(outfilename);
		fOut = fopen(outfilename, "w");
	}
	if (!fOut)
	{
		Alertf("Failed to open file for writing.");
		SAFE_FREE(file_data);
		return;
	}
	{
		char buf[MAX_PATH], outfilename[MAX_PATH];
		sprintf(buf, "UGC/UGC_Project_Errors.txt");
		fileLocateWrite(buf, outfilename);	
		makeDirectoriesForFile(outfilename);
		fErrorOut = fopen(outfilename, "w");
	}
	if (!fErrorOut)
	{
		Alertf("Failed to open error file for writing.");
		fclose(fOut);
		SAFE_FREE(file_data);
		return;
	}

	UGCDebugInitStatGroups(&stats);

	DivideString(file_data, "\t\n\r", &proj_list, DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);
	for (i = 0; i < eaSize(&proj_list); i++)
	{
		char localdir[MAX_PATH];
		UGCProjectData *data = NULL;

		sprintf(localdir, "%s/Data/", fileLocalDataDir());
		data = UGC_LoadProjectData(proj_list[i], localdir);

		if (!data)
		{
			fprintf(fErrorOut, "##### PROJECT %s Could not be loaded.\n", proj_list[i]);
			continue;
		}

		if(UGCDebugGatherStatisticsForProjectData(data, &stats, fErrorOut))
			project_error_count++;

		StructDestroy(parse_UGCProjectData, data);

		printf("*** Finished processing namespace %s (%d/%d)\n", proj_list[i], i, eaSize(&proj_list));
	}

	UGCDebugGatherStatisticsOutput(&stats, fOut);

	fprintf(fErrorOut, "*** %d PROJECTS WITH ERRORS.\n", project_error_count);
	
	fclose(fErrorOut);
	fclose(fOut);
	SAFE_FREE(file_data);
	eaDestroyEx(&proj_list, NULL);
	eaDestroyEx(&stats.eaStatGroups, UGCDebugStatGroupFree);

	printf("Done.");
}

AUTO_COMMAND;
void UGCDebugGatherStatisicsForSingleProject(const char* filename)
{
	int project_error_count = 0;
	char **proj_list = NULL;
	FILE *fOut, *fErrorOut;
	UGCDebugStats stats = { 0 };

	{
		char buf[MAX_PATH], outfilename[MAX_PATH];
		sprintf(buf, "UGC/UGC_Project_Statistics.csv");
		fileLocateWrite(buf, outfilename);	
		makeDirectoriesForFile(outfilename);
		fOut = fopen(outfilename, "w");
	}
	if (!fOut)
	{
		Alertf("Failed to open file for writing.");
		return;
	}
	{
		char buf[MAX_PATH], outfilename[MAX_PATH];
		sprintf(buf, "UGC/UGC_Project_Errors.txt");
		fileLocateWrite(buf, outfilename);	
		makeDirectoriesForFile(outfilename);
		fErrorOut = fopen(outfilename, "w");
	}
	if (!fErrorOut)
	{
		Alertf("Failed to open error file for writing.");
		fclose(fOut);
		return;
	}

	UGCDebugInitStatGroups(&stats);

	do
	{
		UGCProjectData *data = StructCreate( parse_UGCProjectData );
		if (!ParserReadTextFile(filename, parse_UGCProjectData, data, 0))
		{
			fprintf(fErrorOut, "##### PROJECT could not be loaded.\n");
			StructDestroy(parse_UGCProjectData, data);
			break;
		}

		if(UGCDebugGatherStatisticsForProjectData(data, &stats, fErrorOut))
			project_error_count++;

		StructDestroy(parse_UGCProjectData, data);

		printf("*** Finished processing single project\n");
	} while( 0 );

	UGCDebugGatherStatisticsOutput(&stats, fOut);

	fprintf(fErrorOut, "*** %d PROJECTS WITH ERRORS.\n", project_error_count);
	
	fclose(fErrorOut);
	fclose(fOut);
	eaDestroyEx(&stats.eaStatGroups, UGCDebugStatGroupFree);

	printf("Done.");
}

AUTO_COMMAND;
char *UGCDebugGatherStatisticsInExportHogg(const char *hogg_filename, bool onlyImportant)
{
	char *result = NULL;
	int err_return = 0;
	HogFile *hogg = hogFileRead(hogg_filename, NULL, PIGERR_PRINTF, &err_return, HOG_NOCREATE|HOG_READONLY);
	if(!hogg)
	{
		char *estrError = NULL;
		estrPrintf(&estrError, "ERROR: Couldn't open \"%s\" for reading: %s!\n", hogg_filename, hogFileGetArchiveFileName(hogg));
		result = StructAllocString(estrError);
		estrDestroy(&estrError);
	}
	else
	{
		UGCSearchResult *pUGCSearchResult = StructCreate(parse_UGCSearchResult);
		if(PARSERESULT_ERROR == ParserReadTextFileFromHogg("UGCSearchResult", parse_UGCSearchResult, pUGCSearchResult, hogg))
		{
			char *estrError = NULL;
			estrPrintf(&estrError, "ERROR: Could not find UGCSearchResult in hogg %s.\n", hogFileGetArchiveFileName(hogg));
			result = StructAllocString(estrError);
			estrDestroy(&estrError);
		}
		else
		{
			int project_error_count = 0;
			FILE *fOut, *fErrorOut;
			char buf[MAX_PATH], outfilename[MAX_PATH];

			sprintf(buf, "UGC/UGC_Project_Statistics.csv");
			fileLocateWrite(buf, outfilename);	
			makeDirectoriesForFile(outfilename);
			fOut = fopen(outfilename, "w");
			if(!fOut)
			{
				char *estrError = NULL;
				estrPrintf(&estrError, "ERROR: Failed to open file for writing: %s.\n", outfilename);
				result = StructAllocString(estrError);
				estrDestroy(&estrError);
			}
			else
			{
				sprintf(buf, "UGC/UGC_Project_Errors.txt");
				fileLocateWrite(buf, outfilename);	
				makeDirectoriesForFile(outfilename);
				fErrorOut = fopen(outfilename, "w");
				if(!fErrorOut)
				{
					char *estrError = NULL;
					estrPrintf(&estrError, "ERROR: Failed to open error file for writing: %s.\n", outfilename);
					result = StructAllocString(estrError);
					estrDestroy(&estrError);
					fclose(fOut);
				}
				else
				{
					int index;
					UGCDebugStats stats = { 0 };

					UGCDebugInitStatGroups(&stats);

					for(index = 0; index < eaSize(&pUGCSearchResult->eaResults); index++)
					{
						UGCProject *pUGCProject = StructCreate(parse_UGCProject);
						char filename[1024];

						sprintf(filename, "UGCProject/%d/UGCProject.con", pUGCSearchResult->eaResults[index]->iUGCProjectID);

						if(PARSERESULT_ERROR != ParserReadTextFileFromHogg(filename, parse_UGCProject, pUGCProject, hogg))
						{
							UGCProjectVersion *pUGCProjectVersionPublished = NULL;
							UGCProjectVersion *pUGCProjectVersionSaved = NULL;
							int i;

							if( onlyImportant && !UGCProject_IsImportant( CONTAINER_NOCONST( UGCProject, pUGCProject ))) {
								continue;
							}

							for(i = eaSize(&pUGCProject->ppProjectVersions) - 1; i >= 0; i--)
							{
								UGCProjectVersionState eState = ugcProjectGetVersionStateConst(pUGCProject->ppProjectVersions[i]);
								if(UGC_PUBLISHED == eState || UGC_REPUBLISHING == eState || UGC_NEEDS_REPUBLISHING == eState)
									pUGCProjectVersionPublished = pUGCProject->ppProjectVersions[i];
								else if(UGC_SAVED == eState)
									pUGCProjectVersionSaved = pUGCProject->ppProjectVersions[i];

								if(pUGCProjectVersionPublished && pUGCProjectVersionSaved)
									break;
							}

							if(pUGCProjectVersionPublished)
							{
								UGCProjectData *pUGCProjectData = StructCreate(parse_UGCProjectData);
								sprintf(filename, "data/ns/%s/project/%s", pUGCProjectVersionPublished->pNameSpace, pUGCProject->pIDString);

								if(PARSERESULT_ERROR != ParserReadTextFileFromHogg(filename, parse_UGCProjectData, pUGCProjectData, hogg))
								{
									if(UGCDebugGatherStatisticsForProjectData(pUGCProjectData, &stats, fErrorOut))
										project_error_count++;

									printf("*** Finished processing namespace %s\n", pUGCProjectData->ns_name);
								}

								StructDestroy(parse_UGCProjectData, pUGCProjectData);
							}

							// MJF Aug/8/2013 -- The saved projects are distorting information.
							// Usually we just want the published data.  Commenting this out in case
							// we want this back.
							// 
							// if(pUGCProjectVersionSaved)
							// {
							// 	UGCProjectData *pUGCProjectData = StructCreate(parse_UGCProjectData);
							// 	sprintf(filename, "data/ns/%s/project/%s", pUGCProjectVersionSaved->pNameSpace, pUGCProject->pIDString);

							// 	if(PARSERESULT_ERROR != ParserReadTextFileFromHogg(filename, parse_UGCProjectData, pUGCProjectData, hogg))
							// 	{
							// 		if(UGCDebugGatherStatisticsForProjectData(pUGCProjectData, &stats, fErrorOut))
							// 			project_error_count++;

							// 		printf("*** Finished processing namespace %s\n", pUGCProjectData->ns_name);
							// 	}

							// 	StructDestroy(parse_UGCProjectData, pUGCProjectData);
							// }
						}

						StructDestroy(parse_UGCProject, pUGCProject);

						printf("*** Finished processing project %d/%d\n", index, eaSize(&pUGCSearchResult->eaResults));
					}

					UGCDebugGatherStatisticsOutput(&stats, fOut);

					fprintf(fErrorOut, "*** %d PROJECTS WITH ERRORS.\n", project_error_count);

					fclose(fErrorOut);
					fclose(fOut);
					eaDestroyEx(&stats.eaStatGroups, UGCDebugStatGroupFree);

					printf("Done.");
				}
			}
		}

		StructDestroy(parse_UGCSearchResult, pUGCSearchResult);
	}

	hogFileDestroy(hogg, true);

	return result;
}

AUTO_COMMAND;
char *UGCDebugGatherProjectStatisticsInExportHogg(const char *hogg_filename)
{
	char* result = NULL;
	HogFile* hogg = NULL;
	UGCSearchResult* pUGCSearchResult = NULL;
	FILE* fOut = NULL;

	hogg = hogFileRead(hogg_filename, NULL, PIGERR_PRINTF, NULL, HOG_NOCREATE|HOG_READONLY);
	if(!hogg) {
		char *estrError = NULL;
		estrPrintf(&estrError, "ERROR: Couldn't open \"%s\" for reading: %s!\n", hogg_filename, hogFileGetArchiveFileName(hogg));
		result = StructAllocString(estrError);
		estrDestroy(&estrError);
		goto cleanup;
	}

	pUGCSearchResult = StructCreate(parse_UGCSearchResult);
	if(PARSERESULT_ERROR == ParserReadTextFileFromHogg("UGCSearchResult", parse_UGCSearchResult, pUGCSearchResult, hogg)) {
		char *estrError = NULL;
		estrPrintf(&estrError, "ERROR: Could not find UGCSearchResult in hogg %s.\n", hogFileGetArchiveFileName(hogg));
		result = StructAllocString(estrError);
		estrDestroy(&estrError);
		goto cleanup;
	}

	fOut = fopen( "c:/ugc_project_stats.csv", "w" );
	if( !fOut ) {
		char *estrError = NULL;
		estrPrintf( &estrError, "ERROR: Failed to open file for writing: c:/ugc_project_stats.csv.\n" );
		result = StructAllocString(estrError);
		estrDestroy(&estrError);
		goto cleanup;
	}

	/// AT THIS POINT, EVERYTHING IS LOADED

	fprintf( fOut, "ProjectID,Rating,AdjustedRating,PlayCount,Important\n" );

	FOR_EACH_IN_EARRAY_FORWARDS( pUGCSearchResult->eaResults, UGCContentInfo, info ) {
		UGCProject* pUGCProject = StructCreate( parse_UGCProject );
		char filename[ MAX_PATH ];
		sprintf( filename, "UGCProject/%d/UGCProject.con", info->iUGCProjectID );

		if( PARSERESULT_ERROR == ParserReadTextFileFromHogg( filename, parse_UGCProject, pUGCProject, hogg )) {
			printf( "*** ERROR READING PROJECT %d\n", info->iUGCProjectID );
		} else {
			fprintf( fOut, "%d,%f,%f,%d,%d\n",
					 pUGCProject->id,
					 pUGCProject->ugcReviews.fAverageRating,
					 pUGCProject->ugcReviews.fAdjustedRatingUsingConfidence,
					 UGCProject_GetTotalPlayedCount( pUGCProject ),
					 UGCProject_IsImportant( CONTAINER_NOCONST( UGCProject, pUGCProject )));
		}

		printf("*** Finished processing project %d/%d\n", FOR_EACH_IDX( _, info ) + 1, eaSize( &pUGCSearchResult->eaResults ));
	} FOR_EACH_END;

cleanup:
	if( fOut ) {
		fclose( fOut );
	}
	if( pUGCSearchResult ) {
		StructDestroy( parse_UGCSearchResult, pUGCSearchResult );
	}
	if( hogg ) {
		hogFileDestroy(hogg, true);
	}

	return result;
}

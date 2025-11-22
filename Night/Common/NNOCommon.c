/***************************************************************************



***************************************************************************/

#include "NNOCommon.h"
#include "file.h"
#include "WorldLibEnums.h"
#include "appRegCache.h"
#include "NNOCharacterBackground.h"
#include "GenericMesh.h"
#include "StringCache.h"
#include "cmdParse.h"
#include "GlobalTypes.h"
#include "WorldLibEnums.h"
#include "storeCommon.h"

#if defined(GAMECLIENT) || defined(GAMESERVER)
#include "rewardcommon.h"
#endif

extern bool g_isContinuousBuilder;

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_EngineMisc););

AUTO_RUN_FIRST; 
void RegisterProjectVars(void)
{
	SetProductName("Night", "NW");
	regSetAppName("Never Rage in Winter");
}

void OVERRIDE_LATELINK_ProdSpecificGlobalConfigSetup(void)
{
	gConf.bHideCombatMessages = 0;	
	gConf.bAllowOldEncounterData = 0;
	gConf.iDefaultEditorAggroRadius = 45;
	gConf.bEnableMissionReturnCmd = 1;
	gConf.bUserContent = 1;
	gConf.bItemArt = 1;
	gConf.pcLevelingNumericItem = "XP";
	gConf.bAllowPlayerCostumeFX = 1;
	gConf.bAlwaysQuickplay = 1;
	gConf.bIgnoreUnboundItemCostumes = 1;
	gConf.bUseNNOPowerDescs = 1;
	gConf.bRequirePowerTrainer = 0;
	gConf.bAllowMultipleItemPowersWithCharges = 1;
	gConf.bUseNNOAlgoNames = 1;
	//gConf.bAutoRewardLogging = 1;
	//gConf.bAutoCombatLogging = 1;
	//gConf.bVerboseCombatLogging = 1;
	gConf.bAllowNoResultRecipes = 1;
	//gConf.bLogEncounterSummary = 1;
	gConf.bPlayerCanTrainPowersAnywhere = 1;
	gConf.bEncountersScaleWithActivePets = 1;
	gConf.bDisableEncounterTeamSizeRangeCheckOnMissionMaps = 1;
	gConf.bClickiesGlowOnMouseover = 0;//0 means clickies always glow
	gConf.bTimeControlAllowed = 1;
	gConf.IgnoreAwayTeamRulesForTeamPets = 1;
	gConf.bDeactivateHenchmenWhenDroppedFromTeam = 1;
	gConf.bShowMapIconsOnFakeZones = 1;
	gConf.bDamageFloatsDrawOverEntityGen = 1;
	gConf.bTabTargetingLoopsWithNoBreaks = 1;
	gConf.bTabTargetingAlwaysIncludesActiveCombatants = 1;
	gConf.bUseGlobalExpressionForInteractableGlowDistance = 1;
	gConf.bRewardTablesUseEncounterLevel = 0;//All our encounters are level 0.... :(
	gConf.bTargetDual = 0;
	gConf.bEnableBulletins = 1;
	gConf.bCharacterPathMustBeFollowed = 1;
	gConf.bLootBagsRemainOnInteractablesUntilEmpty = 1;
	gConf.bEnableNNOTeamWarp = 1;
	gConf.bLootModesDontCountAsInteraction = 1;
	gConf.bEnableLootModesForInteractables = 0;
	gConf.bCheckOverlayCostumeSpecies = 1;
	gConf.bDontOverrideLODOnScaledUnlessUsingLODTemplate = 1;
	gConf.bRolloverLootIsForPlayersOnly = 1;
	gConf.bAutomaticallyCleanUpDeadPets = 1;
	gConf.bDisableRemoteContactSelling = 1;

	gConf.bDisableGuildEPWithdrawLimit = 1;
	gConf.bEnableGuildItemWithdrawLimit = 1;
	gConf.bShowContactOffersOnCooldown = 1;
	gConf.bGuildWithdrawalUsesShardInterval = 1;	// Causes the guild withdrawal limits to be applied based in shard interval timing rather then based on the first withdrwal.	

	gConf.bAllowGuildAllegianceChanges=1;			 // If this is set to true, guilds can have their allegiance changed after creation
	gConf.bEnforceGuildGuildMemberAllegianceMatch=0; // If this is set to true, allegiance is checked when joining the guild to see that it matches the guild itself (dubious compatibility with previous option)

	gConf.bQueueSmartGroupNNO=1;					// Use NNO smart grouping. Sad that game-specific is done like this. But whatever. The alternatives are much worse
													// This is only the type of grouping. QueueConfig can enable/disable smart grouping overall
	
	// Changes to Team management
	gConf.bManageTeamDisconnecteds = 1;
	gConf.bKeepOnTeamOnLogout=1;
	gConf.bAlwaysTryToRejoinTeamAfterLogout=1;

	gConf.bNewAnimationSystem = 1;
	gConf.bUseNWOMovementAndAnimationOptions = 1;

	gConf.rollover_pickup_time = 1.5;
	gConf.rollover_postpickup_linger_time = 0.25;
	gConf.lootent_postloot_linger_time = 0.25;

	gConf.fCostumeMirrorChanceRequired = 1.0;
	gConf.fCostumeMirrorChanceOptional = 1.0;
	gConf.bNNOInteractionTooltips = true;
	gConf.bManualSubRank = true;
	gConf.bDefaultToOpenTeaming = 1;
	gConf.bClientDangerData = 1;
	gConf.eCCGetBaseAttribValues = CCGETBASEATTRIBVALUES_RETURN_CLASS_VALUE;
	gConf.eCCGetPointsLeft = CCGETPOINTSLEFT_RETURN_USE_DD_POINT_SYSTEM;
	gConf.eCCValidateAttribChanges = CCVALIDATEATTRIBCHANGES_USE_DD_RULES;
	gConf.fCharSelectionPortraitTextureFov = 50.0f;
	gConf.iCharSelectionHeadshotWidth = 512;
	gConf.iCharSelectionHeadshotHeight = 512;
	gConf.bKeepLootsOnCorpses = false;
	gConf.bDontAutoEquipUpgrades = true;
	//gConf.bDoNotShowHUDOptions = true;	
	gConf.bAutoBuyPowersGoInTray = true;
	gConf.pcNeedBeforeGreedThreshold = "Silver";
	gConf.fSpawnPointLoadingScreenDistSq = 160000; // 400 squared
	gConf.bShowAutoLootOption = 1;
	gConf.bUIGenDisallowExprObjPathing = 0;
#if defined(GAMECLIENT) || defined(GAMESERVER)
	item_RegisterItemTypeAsAlwaysSafeForGranting(kItemType_SavedPet);
	item_RegisterItemTypeAsAlwaysSafeForGranting(kItemType_TradeGood);
#endif

	// Hide the in progress missions for the contact dialog
	gConf.bDoNotShowInProgressMissionsForContact = true;

	// Do not automatically show mission turn in dialog
	gConf.bDoNotSkipContactOptionsForMissionTurnIn = true;

	// We don't want the dialog to be ended if the contact offers the same mission for the mission offer game action
	gConf.bDoNotEndDialogForMissionOfferActionIfContactOffers = true;

	// Set the animlist for default contact animation
	gConf.pchClientSideContactDialogAnimList = "Contact_Idle";

	// Show open missions under personal missions if they are related
	gConf.bAddRelatedOpenMissionsToPersonalMissionHelper = true;

	// Remember visited dialogs
	gConf.bRememberVisitedDialogs = true;

	// Set the default guild theme
	gConf.pchDefaultGuildThemeName = "GuildTheme_Adventuring_Company";

    // Set the character creation video
    gConf.pchCharCreateVideoPath = "fmv/LaunchVideo.bik";

	// The name of the interactable category for doors
	gConf.pchCategoryNameForDoors = "Door";

	// Enable persisted stores
	gConf.bEnablePersistedStores = true;

	// Pets won't collide with players
	gConf.bPetsDontCollideWithPlayer = true;

	//gConf.bNoEntityCollision = true;
	//gConf.bEnemiesDontCollideWithPlayer = true;
	gConf.bNPCsDontCollideWithNPCs = true;

	// Use D&D base stats for stat points UI in character creation
	gConf.bUseDDBaseStatPointsFunction = true;

	// When we autogrant critter pets, just discard them silently if they're a dupe.
	gConf.bDiscardDuplicateCritterPets = true;


	//gConf.maxServerFPS = 120.f;
	//gConf.combatUpdateTimer = 0.025f;

	gConf.bOverheadEntityGens = true;
	gConf.pchOverheadGenBone = "FX_RootScale";
	gConf.bManageOffscreenGens = true;
	gConf.iMaxOffscreenIconsPlayers = 4;
	gConf.iMaxOffscreenIconsCritters = 4;

	gConf.bDisableSuperJump = true;

	gConf.fDefaultTooltipDelay = 0.5f;

	// Enable game server auto map selection algorithm
	gConf.bAutoChooseMapOptionInGameServer = true;

	gConf.bVoiceChat = false;

	// Set the default cut-scene for contacts
	gConf.pchDefaultCutsceneForContacts = "Contact_New_Close_Neutral";

	// Allow buy backs in stores
	gConf.bAllowBuyback = true;

	// don't do anything special for Xbox controllers
	gConf.bXboxControllersAreJoysticks = true;

	gConf.fMapSnapIndoorRes = 1.5f;
	gConf.fMapSnapOutdoorRes = 0.8f;
	gConf.fMapSnapOutdoorOrthoSkewX = 0.3f;
	gConf.fMapSnapOutdoorOrthoSkewZ= 0.45f;
	gConf.fMapSnapIndoorOrthoSkewX = 0.1f;
	gConf.fMapSnapIndoorOrthoSkewZ= 0.15f;

	//This should be true if you want the map and minimap to scale to default when moving to a new area
	gConf.bSetMapScaleDefaultOnLoad = true;

	//Set the max name length to match the UI restriction
	gConf.iMaxNameLength = 20;

	gConf.bTargetSelectUnderMouseUsesPowersLOSCheck = 1;

	// Enable power stat bonuses so we can display useful information to the player
	gConf.bUsePowerStatBonuses = 1;

	// Send the innate attrib mod data to the client
	gConf.bSendInnateAttribModData = 1;

	//Scale the map snap render resolution between the min and the max
	gConf.bUseLinearMapSnapResolutionScaling = 1;

	//Golden Path should be enabled for Neverwinter
	gConf.bEnableGoldenPath = 1;

	//
	gConf.bHTMLViewerEnabled = false;

	// Enable FMV by default
	gConf.bEnableFMV = true;

	gConf.iMinimumTeamLevel = 4;

	gConf.bEnableShaderQualitySlider = SHADER_QUALITY_SLIDER_LABEL;

	gConf.bEnableClusters = 1;

	// Simplygon mesh reduction for Neverwinter. Needs to be set for anything that could possibly do geometry binning.
	simplygonSetEnabled(false);

	gConf.iMaxHideShowCollisionTris = 500;

	gConf.bCombatDeathPrediction = true;

	gConf.bRoundItemStatsOnApplyToChar = true;

	// I wrote this to try to make the game less jittery.  Putting it in on a trial basis.  [RMARR - 11/1/12]
	gConf.bSmartInputSmoothing = true;

	gConf.bUGCAchievementsEnable = true;

	gConf.iUGCMissionRewardScaleOverTime = 10;
	gConf.fUGCMissionRewardMaxScale = 1;
	gConf.pchUGCMissionDefaultRewardTable = "Ugc_Defaultmissionnumerics";
	gConf.pchUGCMissionFeaturedRewardTable = "Ugc_Defaultmissionnumerics";

	gConf.bEnableAssists = true;

#if defined(GAMECLIENT)
	globCmdParse("simpleCpuGraphTargetCycles 80000");
#endif

	gConf.bDontAllowGADModification = true;

	// make quest turn-ins beat info dialogs
	gConf.bLowImportanceInfoDialogs = true;

	gConf.bAllowBuybackUntilMapMove = true;

    // Auto conversion of character types.  Needed when GamePermissions are turned on.
    gConf.bAutoConvertCharacterToPremium = true;
    gConf.bAutoConvertCharacterToStandard = true;

    gConf.bDisableOldCharacterSlotCheck = true;

    // If bUseSimplifiedMultiShardCharacterSlotRules is true, then bVirtualShardsOnlyUseRestrictedCharacterSlots must also be true.
    gConf.bVirtualShardsOnlyUseRestrictedCharacterSlots = true;
    gConf.bUseSimplifiedMultiShardCharacterSlotRules = true;

    gConf.bHideZoneInstanceNumberInChat = true;

	gConf.bShowOnlyOneCopyOfEachResolution = true;

	// NNO's minimum resolution is 1024x768. Any smaller and the UI starts overlapping itself.
	gConf.iMinimumFullscreenResolutionWidth = 1024;
	gConf.iMinimumFullscreenResolutionHeight = 768;

    // Which objectdb entity header extra field contains the player class name.
    gConf.iCharacterChoiceExtraHeaderForClass = 1;

    // Which objectdb entity header extra field contains the player character path.
    gConf.iCharacterChoiceExtraHeaderForPath = 2;

    // Which objectdb entity header extra field contains the player character species.
    gConf.iCharacterChoiceExtraHeaderForSpecies = 3;

	// make imperceptible entities immediately disappear
	gConf.bClientImperceptibleFadesImmediately = true;

	gConf.bCutsceneDefault_bPlayersAreUntargetable = true;

	gConf.bInteractMapLeaveConfirm = true;

	//We want an extra step so that users are not put into alt mode whenever another player requests to trade
	gConf.bEnableTwoStepTradeRequest = true;

	//We want trades between players to have an extra escrow step
	gConf.bEnableEscrowTrades = true;

	gConf.bTailorPaymentChoice = true;

	gConf.bUgcRewardOverrideEnable = true;
	gConf.fUgcRewardOverrideTimeMinutes = 15;

	gConf.bDoNotInitJoystick = true;

    gConf.uProjSpecificLogStringUpdateInterval = 10;

	gConf.uSecondsBetweenChangeInstance = 120;

	gConf.bIgnoreEntityGenOffscreenExpression = true;

	// NW we are allowing respecing not require being interacting with a contact
	gConf.bAllowRespecAwayFromContact = true;

	gConf.bUseRenderScaleSlider = true;

	gConf.bUGCAveragePlayingTimeUsesCustomMapPlayingTime = true;
	gConf.uUGCHoursBetweenRecordingCompletionForEntity = 24;

	gConf.bKeybindsFromControlSchemes = 0;

	gConf.bUGCSearchTreatsDefaultLanguageAsAll = true;

    gConf.fSecondaryMissionCooldownHours = 0.25f;

	gConf.bDisableRespawnOnPvPMatchStart = true;
	gConf.bNeedRollRequiresUsageMatch = true;

	gConf.bHUDRearrangeModeEatsKeys = true;

    // Neverwinter does not give a free costume slot when a player joins a guild.
    gConf.bDisableGuildFreeCostumeViaNumeric = true;

	gConf.uBeaconizerJumpHeight = 2;

	gConf.bUseChatTradeChannel = true;
	gConf.bUseChatLFGChannel = true;
	gConf.bPreventItemLinksInZoneLikeChatChannels = true;
	gConf.bOnlyOneContactPerMission = true;
	gConf.bRewardsIgnoreCritterSubRank = true;
	gConf.bNoCostDonationTasksAllowed = true;
}

bool OVERRIDE_LATELINK_isValidRegionTypeForGame(U32 world_region_type)
{
	return (world_region_type == WRT_Ground) || 
		   (world_region_type == WRT_CharacterCreator) || 
		   (world_region_type == WRT_Indoor) || 
		   (world_region_type == WRT_None) || 
		   (world_region_type == WRT_PvP);
}

bool OVERRIDE_LATELINK_store_VendorValidations(StoreDef* pStore)
{
	bool bValid = true;

	if(pStore->bSellEnabled)
	{
		if(REF_STRING_FROM_HANDLE(pStore->hCurrency) &&
			stricmp(REF_STRING_FROM_HANDLE(pStore->hCurrency), "Resources") != 0)
		{
			ErrorFilenamef(pStore->filename, "Store %s has a sell currency that is %s instead of Resources", 
				pStore->name, REF_STRING_FROM_HANDLE(pStore->hCurrency));
			bValid = false;
		}

		if(pStore->fSellMultiplier != 1.0)
		{
			ErrorFilenamef(pStore->filename, "Store %s has a sell multiplier that isn't 1.0", 
				pStore->name);
			bValid = false;
		}
	}

	return bValid;
}

extern void NNOLandingPageZenPanel_Load(); // NNOLandingPageZenPanel.c

AUTO_STARTUP(GAMESPECIFIC) ASTRT_DEPS(Items, PowerTrees, GraphicsLibEarly, AS_Messages);
void NNO_AutoStartupLoadFiles(void)
{
	//only load these on the server for binning purposes during a build.
	if ((IsGameServerSpecificallly_NotRelatedTypes() && g_isContinuousBuilder) || IsClient() || IsLoginServer())
		CharacterBackground_Load();

#if defined(GAMECLIENT)
	NNOLandingPageZenPanel_Load();
#endif	
	
}

int OVERRIDE_LATELINK_gameSpecificFixup_Version(void)
{
    return NNO_ENTITYFIXUPVERSION;
}

/***************************************************************************
 
 
 
 *
 ***************************************************************************/
#include "stdtypes.h"

#include "Guild.h"
#include "Gateway/gslGatewaySession.h"

#include "ActivityLogCommon.h"
#include "ActivityLogEnums.h"
#include "ChatCommon.h"
#include "GameStringFormat.h"
#include "GroupProjectCommon.h"
#include "Message.h"
#include "OfficerCommon.h"
#include "time.h"
#include "timing.h"
#include "WorldGridPrivate.h"

#include "AutoGen/ActivityLogEnums_h_ast.h"
#include "ChatCommon_h_ast.h"
#include "Guild_h_ast.h"
#include "NNOGatewayMappedGuild_c_ast.h"

/////////////////////////////////////////////////////////////////////////////

#define ONE_DAY_IN_SECONDS (60 * 60 * 24)
#define NUM_CALENDAR_WEEKS 25
#define NUM_CALENDAR_WEEKS_IN_SECONDS (NUM_CALENDAR_WEEKS * (7 * ONE_DAY_IN_SECONDS))

typedef struct ZoneMapInfo ZoneMapInfo;


/////////////////////////////////////////////////////////////////////////////
//
// Mapped structs
//

AUTO_ENUM;
typedef enum DateType
{
	DateType_None,
	DateType_Today,
	DateType_Past,
	DateType_ThisMonth,
} DateType;

AUTO_STRUCT;
typedef struct MappedGuildEventReply
{
	ContainerID iMemberID;						AST(NAME(ID) DEFAULT(-1))
	char *estrName;								AST(ESTRING NAME(Name))
	char *estrPublicAccountName;				AST(ESTRING NAME(PublicAccountName))
	GuildEventReplyType eType;					AST(NAME(ReplyType))
	char *estrMessage;							AST(ESTRING NAME(ReplyMessage))

	//If event recurs this is the start time the reply applies too.
	U32 iStartTime;								AST(NAME(StartTimeSeconds))
} MappedGuildEventReply;

AUTO_STRUCT;
typedef struct MappedGuildEvent
{
	U32 uiID;									AST(NAME(ID) DEFAULT(-1))
	char *estrTitle;							AST(ESTRING NAME(Title))
	char *estrDescription;						AST(ESTRING NAME(Description))

	char *estrStartTime;						AST(ESTRING NAME(StartTime))
	S32 iDuration;								AST(ESTRING NAME(Duration))
	S32 iRecurrenceFrequency;					AST(NAME(RecurrenceFrequency))
	S32 iRecurrenceCount;						AST(NAME(RecurrenceCount))
	bool bCanceled;								AST(NAME(Canceled))

	char *estrMinGuildRank;						AST(ESTRING NAME(MinGuildRank))
	char *estrMinGuildEditRank;					AST(ESTRING NAME(MinGuildEditRank))
	int iMinLevel;								AST(NAME(MinLevel))
	int iMaxLevel;								AST(NAME(MaxLevel))

	U32 iMinAccepts;							AST(NAME(MinAccepts) DEFAULT(-1))
	U32 iMaxAccepts;							AST(NAME(MaxAccepts) DEFAULT(-1))

	MappedGuildEventReply **eaReplies;			AST(NAME(Replies))

	// This might be removed in the future, if I can figure out a fancy js way to get the data from eaReplies
	int iAccepts;								AST(NAME(Accepts) DEFAULT(-1))
	int iMaybes;								AST(NAME(Maybes) DEFAULT(-1))
	int iRefusals;								AST(NAME(Refusals) DEFAULT(-1))

} MappedGuildEvent;

AUTO_STRUCT;
typedef struct MappedGuildEventOccurrence
{
	U32 uiID;									AST(NAME(ID) DEFAULT(-1))
	char *estrTitle;							AST(ESTRING NAME(Title))
	char *estrStartTime;						AST(ESTRING NAME(StartTime))
	U32 iStartTime;								AST(NAME(StartTimeSeconds))
} MappedGuildEventOccurrence;

AUTO_STRUCT;
typedef struct MappedGuildDay
{
	int iYear;									AST(NAME(Year) DEFAULT(-1))
	int iMonth;									AST(NAME(Month) DEFAULT(-1))
	int iDate;									AST(NAME(Date) DEFAULT(-1))
	U32 iTime;									AST(NAME(Time))
	DateType eDateType;							AST(NAME(Type))
	MappedGuildEventOccurrence **eaEvents;		AST(NAME(Events))
} MappedGuildDay;

AUTO_STRUCT;
typedef struct MappedGuildWeek
{
	MappedGuildDay **eaDays;					AST(NAME(Days))
} MappedGuildWeek;

AUTO_STRUCT;
typedef struct MappedGuildCalendar
{
	MappedGuildWeek **eaWeeks;					AST(NAME(Weeks))
} MappedGuildCalendar;

AUTO_STRUCT;
typedef struct MappedGuildActivityLogEntry
{
	char* estrTime;								AST(ESTRING NAME(Time))
	char* estrType;								AST(ESTRING NAME(Type))
	char* estrString;							AST(ESTRING NAME(String))
} MappedGuildActivityLogEntry;

AUTO_STRUCT;
typedef struct MappedGuildCustomRank
{
	char *estrName;								AST(ESTRING NAME(Name))
	int iRank;									AST(NAME(Rank))
	GuildRankPermissions ePerms;				AST(FLAGS NAME(Permissions))
} MappedGuildCustomRank;

AUTO_STRUCT;
typedef struct MappedGuildMember
{
	ContainerID id;
	char *estrName;								AST(ESTRING NAME(Name))
	char *estrJoined;							AST(ESTRING NAME(Joined))
	char *estrPublicAccountName;				AST(ESTRING NAME(PublicAccountName))
	char *estrStatus;							AST(ESTRING NAME(Status))
	int iLevel;									AST(NAME(Level))
	char *estrPublicComment;					AST(ESTRING NAME(PublicComment))
	char *estrOfficerComment;					AST(ESTRING NAME(OfficerComment))
	char *estrClassType;						AST(ESTRING NAME(ClassType))
	int iRank;									AST(NAME(Rank))
	char *estrOfficerRank;						AST(ESTRING NAME(OfficerRank))
	int iGuildContribution1;					AST(NAME(GuildContribution1))
	int iGuildContribution2;					AST(NAME(GuildContribution2))
	char *estrLocation;							AST(ESTRING NAME(Location))
	char *estrLogoutTime;						AST(ESTRING NAME(LogoutTime))
	TeamMode eLFG;								AST(NAME(LFG))
	bool bOnline;								AST(NAME(Online))
} MappedGuildMember;

AUTO_STRUCT;
typedef struct MappedGuild
{
	ContainerID id;
	bool bViewerIsMember;						AST(NAME(ViewerIsMember))

	char *estrName;								AST(ESTRING NAME(Name)) // REQUIRED
	char *estrCreatedOn;						AST(ESTRING NAME(CreatedOn))
	char *estrMotD;								AST(ESTRING NAME(MotD))
	char *estrDescription;						AST(ESTRING NAME(Description))
	char *estrWebsite;							AST(ESTRING NAME(Website))
	char *estrRecruitMessage;					AST(ESTRING NAME(RecruitMessage))
	int iMinLevelRecruit;						AST(NAME(MinLevelRecruit))

	MappedGuildCustomRank **eaRanks;			AST(NAME(Ranks))
	MappedGuildMember **eaMembers;				AST(NAME(Members))
	int iTotalMembers;							AST(NAME(TotalMembers))

	MappedGuildActivityLogEntry **eaEntries;	AST(NAME(ActivityEntries))

	MappedGuildCalendar *pCalendar;				AST(NAME(Calendar))

	MappedGuildEvent **eaEvents;				AST(NAME(Events))

} MappedGuild;

//////////////////////////////////////////////////////////////////////////
//
// Convenience formatters
//

static void FormatGuildCustomRankName(Language langID, char **ppchDest, GuildCustomRank *pRank)
{
	// Sadly, the pRank in this function and in the previous one are completely unrelated.
	if (pRank)
	{
		estrCopy2(ppchDest, pRank->pcDisplayName
			? pRank->pcDisplayName
			: langTranslateMessageKey(langID, pRank->pcDefaultNameMsg));
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Mapping functions
//

static void FillMappedGuildEventOccurrence(GatewaySession *pSess, MappedGuildEventOccurrence *pMappedOccurrence, GuildEvent *pEvent, U32 uiEventTime)
{
	pMappedOccurrence->uiID = pEvent->uiID;
	pMappedOccurrence->iStartTime = uiEventTime;
	estrCopy2(&pMappedOccurrence->estrStartTime, timeGetRFC822StringFromSecondsSince2000(uiEventTime));
	estrCopy2(&pMappedOccurrence->estrTitle, pEvent->pcTitle);
}

static int MappedGuildEventOccurrenceComparitor(const MappedGuildEventOccurrence **ppA, const MappedGuildEventOccurrence **ppB)
{
	return (*ppA)->iStartTime - (*ppB)->iStartTime;
}

static void FillMappedGuildEventOccurrences(GatewaySession *pSess, MappedGuildEventOccurrence ***peaMappedOccurrences, CONST_EARRAY_OF(GuildEvent) *peaEvents, U32 uiStart, U32 uiEnd)
{
	int i;
	PERFINFO_AUTO_START_FUNC();
	for (i = 0; i < eaSize(peaEvents); i++)
	{
		GuildEvent *pEvent = (*peaEvents)[i];
		U32 uiEventTime = pEvent->iStartTimeTime;
		S32 iRecurrances = pEvent->iRecurrenceCount;
		while (uiEventTime < uiEnd)
		{
			if (uiEventTime >= uiStart)
			{
				MappedGuildEventOccurrence *pMappedOccurrence= eaGetStruct(peaMappedOccurrences, parse_MappedGuildEventOccurrence, eaSize(peaMappedOccurrences));
				FillMappedGuildEventOccurrence(pSess, pMappedOccurrence, pEvent, uiEventTime);
			}

			// This enum value is how often it should recur, in days. 
			// I'm not sure how I feel about that. 
			uiEventTime += pEvent->eRecurType * ONE_DAY_IN_SECONDS;
			if (pEvent->eRecurType == GuildEventRecurType_Once
				|| iRecurrances-- == 0)
			{
				break;
			}
		}
	}
	eaQSort(*peaMappedOccurrences, MappedGuildEventOccurrenceComparitor);
	PERFINFO_AUTO_STOP();
}

static U32 GetSS2000ForMonthStart()
{
	U32 uiTime = timeSecondsSince2000();
	struct tm tmTime;
	timeMakeTimeStructFromSecondsSince2000(uiTime, &tmTime);
	tmTime.tm_sec = 0;
	tmTime.tm_min = 0;
	tmTime.tm_hour = 0;
	tmTime.tm_mday = 1;
	return timeGetSecondsSince2000FromTimeStruct(&tmTime);
}

static U32 GetSS2000ForDayStart()
{
	U32 uiTime = timeSecondsSince2000();
	struct tm tmTime;
	timeMakeTimeStructFromSecondsSince2000(uiTime, &tmTime);
	tmTime.tm_sec = 0;
	tmTime.tm_min = 0;
	tmTime.tm_hour = 0;
	return timeGetSecondsSince2000FromTimeStruct(&tmTime);
}

static void CreateMappedGuildDaysWeeksAndDays(MappedGuildWeek ***peaWeeks, U32 uiStart, U32 uiEnd) 
{
	const int SATURDAY = 6;

	U32 uiTime = uiStart;
	U32 uiNow = timeSecondsSince2000();
	U32 uiToday = GetSS2000ForDayStart(uiNow);
	int iWeek = 0;
	struct tm tmNow;
	struct tm tmDate;
	struct tm tmTime;

	PERFINFO_AUTO_START_FUNC();
	
	timeMakeTimeStructFromSecondsSince2000(uiNow, &tmNow);
	timeMakeTimeStructFromSecondsSince2000(uiTime, &tmTime);
	eaSetSizeStruct(peaWeeks, parse_MappedGuildWeek, NUM_CALENDAR_WEEKS);

	// Move back the time to the most recent Sunday
	uiTime -= tmTime.tm_wday * ONE_DAY_IN_SECONDS;

	while (uiTime < uiEnd)
	{
		MappedGuildWeek *pWeek = eaGet(peaWeeks, iWeek++);
		if (!pWeek)
			break;
		do 
		{
			MappedGuildDay *pDay;
			timeMakeTimeStructFromSecondsSince2000(uiTime, &tmDate);
			pDay = eaGetStruct(&pWeek->eaDays, parse_MappedGuildDay, eaSize(&pWeek->eaDays));
			pDay->iTime = uiTime;
			pDay->iDate = tmDate.tm_mday;
			pDay->iMonth = tmDate.tm_mon;
			pDay->iYear = tmDate.tm_year + 1900;

			if (uiTime < uiToday)
				pDay->eDateType = DateType_Past;
			else if (uiToday >= uiTime && uiToday < (uiTime + ONE_DAY_IN_SECONDS))
				pDay->eDateType = DateType_Today;
			else if (tmNow.tm_mon == tmDate.tm_mon && tmNow.tm_year == tmDate.tm_year)
				pDay->eDateType = DateType_ThisMonth;

			uiTime += ONE_DAY_IN_SECONDS;
		} while (tmDate.tm_wday != SATURDAY);
	}

	PERFINFO_AUTO_STOP();
}

static MappedGuildDay* GetMappedGuildDayByTime(MappedGuildWeek ***peaSortedWeeks, U32 uiTime, int *piStartWeek, int *piStartDay)
{
	MappedGuildWeek *pWeek;
	MappedGuildDay *pDay;

	while (true)
	{ 
		pWeek = eaGet(peaSortedWeeks, *piStartWeek);
		pDay = pWeek ? eaGet(&pWeek->eaDays, *piStartDay) : NULL;

		if (!pDay)
		{
			break;
		}

		if (uiTime >= pDay->iTime
			&& uiTime < (pDay->iTime + ONE_DAY_IN_SECONDS))
		{
			break;
		}

		++(*piStartDay);
		if ((*piStartDay) >= eaSize(&pWeek->eaDays))
		{
			++(*piStartWeek);
			(*piStartDay) = 0;
		}
	}
	return pDay;
}

static void FillMappedGuildWeeks(GatewaySession *pSess, MappedGuildWeek ***peaWeeks, MappedGuildEventOccurrence ***peaMappedOccurrences)
{
	int iWeek = 0;
	int iDay = 0;
	int i;
	for (i = 0; i < eaSize(peaMappedOccurrences); i++)
	{
		MappedGuildEventOccurrence *pOccurrence = (*peaMappedOccurrences)[i];
		MappedGuildDay *pDay = GetMappedGuildDayByTime(peaWeeks, pOccurrence->iStartTime, &iWeek, &iDay);
		if (pDay)
		{
			eaPush(&pDay->eaEvents, pOccurrence);
		}
		else
		{
			break;
		}
	}	
}

static void FillMappedGuildEventReply(GatewaySession *pSess, MappedGuildEventReply *pMappedReply, GuildEventReply *pReply, CONST_EARRAY_OF(GuildMember) *peaMembers, int *piAccepts, int *piMaybes, int *piRefusals)
{
	GuildMember *pMember = eaIndexedGetUsingInt(peaMembers, pReply->iMemberID);
	if (pMember)
	{
		pMappedReply->iMemberID = pReply->iMemberID;
		estrCopy2(&pMappedReply->estrName, pMember->pcName);
		estrAppend2(&pMappedReply->estrPublicAccountName, pMember->pcAccount);

		pMappedReply->eType = pReply->eGuildEventReplyType;
		*piAccepts += (pMappedReply->eType == GuildEventReplyType_Accept);
		*piMaybes += (pMappedReply->eType == GuildEventReplyType_Maybe);
		*piRefusals += (pMappedReply->eType == GuildEventReplyType_Refuse);

		estrCopy2(&pMappedReply->estrMessage, pReply->pcReplyMessage);

		pMappedReply->iStartTime = pReply->iStartTime;
	}
}

static void FillMappedGuildEventReplies(GatewaySession *pSess, MappedGuildEventReply ***peaMappedReplies, CONST_EARRAY_OF(GuildEventReply) *peaReplies, CONST_EARRAY_OF(GuildMember) *peaMembers, int *piAccepts, int *piMaybes, int *piRefusals)
{
	int i;
	for (i = 0; i < eaSize(peaReplies); i++)
	{
		GuildEventReply *pReply = (*peaReplies)[i];
		MappedGuildEventReply *pMappedReply = StructCreate(parse_MappedGuildEventReply);
		FillMappedGuildEventReply(pSess, pMappedReply, pReply, peaMembers, piAccepts, piMaybes, piRefusals);
		eaPush(peaMappedReplies, pMappedReply);
	}
}

static void FillMappedGuildEvent(GatewaySession *pSess, MappedGuildEvent *pMappedEvent, GuildEvent *pEvent, CONST_EARRAY_OF(GuildCustomRank) *peaRanks, CONST_EARRAY_OF(GuildMember) *peaMembers)
{
	pMappedEvent->uiID = pEvent->uiID;

	estrCopy2(&pMappedEvent->estrTitle, pEvent->pcTitle);
	estrCopy2(&pMappedEvent->estrDescription, pEvent->pcDescription);

	estrCopy2(&pMappedEvent->estrStartTime, timeGetRFC822StringFromSecondsSince2000(pEvent->iStartTimeTime));
	pMappedEvent->iDuration = pEvent->iDuration / 60;
	pMappedEvent->iRecurrenceFrequency = pEvent->eRecurType;
	pMappedEvent->iRecurrenceCount = pEvent->iRecurrenceCount;
	pMappedEvent->bCanceled = pEvent->bCanceled;

	pMappedEvent->iMinLevel = pEvent->iMinLevel;
	pMappedEvent->iMaxLevel = pEvent->iMaxLevel;

	FormatGuildCustomRankName(pSess->lang, &pMappedEvent->estrMinGuildRank, eaGet(peaRanks, pEvent->iMinGuildRank));
	FormatGuildCustomRankName(pSess->lang, &pMappedEvent->estrMinGuildEditRank, eaGet(peaRanks, pEvent->iMinGuildEditRank));

	pMappedEvent->iMinAccepts = pEvent->iMinAccepts;
	pMappedEvent->iMaxAccepts = pEvent->iMaxAccepts;

	pMappedEvent->iAccepts = 0;
	pMappedEvent->iMaybes = 0;
	pMappedEvent->iRefusals = 0;

	FillMappedGuildEventReplies(pSess, &pMappedEvent->eaReplies, &pEvent->eaReplies, peaMembers, &pMappedEvent->iAccepts, &pMappedEvent->iMaybes, &pMappedEvent->iRefusals);
}

static void FillMappedGuildEvents(GatewaySession *pSess, MappedGuildEvent ***peaMappedEvents, CONST_EARRAY_OF(GuildEvent) *peaEvents, CONST_EARRAY_OF(GuildCustomRank) *peaRanks, CONST_EARRAY_OF(GuildMember) *peaMembers)
{
	int i;
	for (i = 0; i < eaSize(peaEvents); i++)
	{
		GuildEvent *pEvent = (*peaEvents)[i];
		MappedGuildEvent *pMappedEvent = StructCreate(parse_MappedGuildEvent);
		FillMappedGuildEvent(pSess, pMappedEvent, pEvent, peaRanks, peaMembers);
		eaPush(peaMappedEvents, pMappedEvent);
	}
}

static void FillMappedGuildCalendar(GatewaySession *pSess, MappedGuildCalendar *pMappedGuildCalendar, CONST_EARRAY_OF(GuildEvent) *peaGuildEvents, U32 uiStart, U32 uiEnd)
{
	MappedGuildEventOccurrence **eaMappedGuildOccurrences = NULL;

	PERFINFO_AUTO_START_FUNC();

	CreateMappedGuildDaysWeeksAndDays(&pMappedGuildCalendar->eaWeeks, uiStart, uiEnd);

	FillMappedGuildEventOccurrences(pSess, &eaMappedGuildOccurrences, peaGuildEvents, uiStart, uiEnd);

	FillMappedGuildWeeks(pSess, &pMappedGuildCalendar->eaWeeks, &eaMappedGuildOccurrences);

	PERFINFO_AUTO_STOP();
}

static void FillMappedGuildMember(GatewaySession *pSess, MappedGuildMember *pMappedMember, GuildMember *pMember, CONST_EARRAY_OF(GuildCustomRank) *peaRanks)
{
	ZoneMapInfo *pZMapInfo = RefSystem_ReferentFromString(g_ZoneMapDictionary, pMember->pcMapName);
	GuildCustomRank *pRank = eaGet(peaRanks, pMember->iRank);
	pMappedMember->id = pMember->iEntID;
	estrCopy2(&pMappedMember->estrName, pMember->pcName);
	estrCopy2(&pMappedMember->estrPublicAccountName, pMember->pcAccount);
	estrCopy2(&pMappedMember->estrJoined, timeGetRFC822StringFromSecondsSince2000(pMember->iJoinTime));
	estrCopy2(&pMappedMember->estrStatus, pMember->pcStatus);
	estrCopy2(&pMappedMember->estrPublicComment, pMember->pcPublicComment);
	estrCopy2(&pMappedMember->estrClassType, pMember->pchClassName);
	estrCopy2(&pMappedMember->estrLocation, langTranslateMessage(pSess->lang, zmapInfoGetDisplayNameMessagePtr(pZMapInfo)));
	estrCopy2(&pMappedMember->estrLogoutTime, timeGetRFC822StringFromSecondsSince2000(pMember->iLogoutTime));
	FormatGuildCustomRankName(pSess->lang, &pMappedMember->estrOfficerRank, pRank);
	pMappedMember->eLFG = pMember->eLFGMode;
	pMappedMember->bOnline = pMember->bOnline;
	pMappedMember->iRank = pMember->iRank;
	pMappedMember->iLevel = pMember->iLevel;
}


static void FillMappedGuildMembers(GatewaySession *pSess, MappedGuildMember ***peaMappedMembers, CONST_EARRAY_OF(GuildMember) *peaMembers, CONST_EARRAY_OF(GuildCustomRank) *peaRanks, bool bViewerIsMember)
{
	int i;
	PERFINFO_AUTO_START_FUNC();
	for (i = 0; i < eaSize(peaMembers); i++)
	{
		GuildMember *pMember = (*peaMembers)[i];
		GuildCustomRank *pRank = eaGet(peaRanks, pMember->iRank);
		if (bViewerIsMember || (pRank->ePerms & GuildPermission_Invite))
		{
			MappedGuildMember *pMappedMember = StructCreate(parse_MappedGuildMember); 
			FillMappedGuildMember(pSess, pMappedMember, pMember, peaRanks);
			eaPush(peaMappedMembers, pMappedMember);
		}
	}
	PERFINFO_AUTO_STOP();
}

static void FillMappedGuildCustomRank(GatewaySession *pSess, MappedGuildCustomRank *pdest, GuildCustomRank *psrc, int iRank)
{
	pdest->iRank = iRank;
	FormatGuildCustomRankName(pSess->lang, &pdest->estrName, psrc);
	pdest->ePerms = psrc->ePerms;
}

static void FillMappedGuildCustomRanks(GatewaySession *pSess, MappedGuildCustomRank ***peaMappedRanks, CONST_EARRAY_OF(GuildCustomRank) *peaRanks)
{
	int i;
	for (i = 0; i < eaSize(peaRanks); i++)
	{
		GuildCustomRank *pRank = (*peaRanks)[i];
		MappedGuildCustomRank *pMappedRank = StructCreate(parse_MappedGuildCustomRank);
		FillMappedGuildCustomRank(pSess, pMappedRank, pRank, i);
		eaPush(peaMappedRanks, pMappedRank);
	}
}


void FillMappedGuildActivityLogEntry(GatewaySession *pSess, MappedGuildActivityLogEntry *pMappedEntry, ActivityLogEntry *pEntry, Guild *pGuild)
{
	estrCopy2(&pMappedEntry->estrTime, timeGetRFC822StringFromSecondsSince2000(pEntry->time));
	langFormatGameString(pSess->lang, &pMappedEntry->estrType, langTranslateStaticDefineInt(pSess->lang, ActivityLogEntryTypeEnum, pEntry->type),
		STRFMT_STRING("Guild", pGuild->pcName),
		STRFMT_END);
	ActivityLog_FormatEntry(pSess->lang, &pMappedEntry->estrString, pEntry, pGuild);
}

void FillMappedGuildActivityLogEntries(GatewaySession *pSess, MappedGuildActivityLogEntry ***peaMappedEntries, CONST_EARRAY_OF(ActivityLogEntry) *peaEntries, Guild *pGuild)
{
	int i;
	PERFINFO_AUTO_START_FUNC();
	for (i = 0; i < eaSize(peaEntries); i++)
	{
		ActivityLogEntry *pEntry = (*peaEntries)[i];

		// skip log entries for ex-members
		if (pEntry->subjectID == 0
			|| eaIndexedGetUsingInt(&pGuild->eaMembers, pEntry->subjectID))
		{
			MappedGuildActivityLogEntry *pMappedEntry = StructCreate(parse_MappedGuildActivityLogEntry);
			FillMappedGuildActivityLogEntry(pSess, pMappedEntry, pEntry, pGuild);
			eaPush(peaMappedEntries, pMappedEntry);
		}
	}
	PERFINFO_AUTO_STOP();
}

static void FillMappedGuild(GatewaySession *pSess, MappedGuild *pMappedGuild, Guild *pGuild)
{
	PERFINFO_AUTO_START_FUNC();

	pMappedGuild->id = pGuild->iContainerID;
	pMappedGuild->bViewerIsMember = pGuild && guild_AccountIsMember(pGuild, pSess->idAccount);
	
	// Basic Info
	estrCopy2(&pMappedGuild->estrName, pGuild->pcName);
	estrCopy2(&pMappedGuild->estrCreatedOn, timeGetRFC822StringFromSecondsSince2000(pGuild->iCreatedOn));
	estrCopy2(&pMappedGuild->estrMotD, pGuild->pcMotD);
	estrCopy2(&pMappedGuild->estrDescription, pGuild->pcDescription);
	estrCopy2(&pMappedGuild->estrWebsite, pGuild->pcWebSite);
	if (!pGuild->bHideRecruitMessage)
	{
		estrCopy2(&pMappedGuild->estrRecruitMessage, pGuild->pcRecruitMessage);
	}
	pMappedGuild->iMinLevelRecruit = pGuild->iMinLevelRecruit;

	if (pMappedGuild->bViewerIsMember || !pGuild->bHideMembers)
	{
		FillMappedGuildCustomRanks(pSess, &pMappedGuild->eaRanks, &pGuild->eaRanks);
		FillMappedGuildMembers(pSess, &pMappedGuild->eaMembers, &pGuild->eaMembers, &pGuild->eaRanks, pMappedGuild->bViewerIsMember);
		pMappedGuild->iTotalMembers = eaSize(&pGuild->eaMembers);
	}

	if (pMappedGuild->bViewerIsMember)
	{
		U32 uiThisMonth = GetSS2000ForMonthStart();
		U32 uiEnd = uiThisMonth + NUM_CALENDAR_WEEKS_IN_SECONDS;

		FillMappedGuildEvents(pSess, &pMappedGuild->eaEvents, &pGuild->eaEvents, &pGuild->eaRanks, &pGuild->eaMembers);

		pMappedGuild->pCalendar = StructCreate(parse_MappedGuildCalendar);
		FillMappedGuildCalendar(pSess, pMappedGuild->pCalendar, &pGuild->eaEvents, uiThisMonth, uiEnd);

		FillMappedGuildActivityLogEntries(pSess, &pMappedGuild->eaEntries, &pGuild->eaActivityEntries, pGuild);
		
	}

	PERFINFO_AUTO_STOP();
}

//////////////////////////////////////////////////////////////////////////
//
// Public interface
//

MappedGuild *CreateMappedGuild(GatewaySession *pSess, ContainerTracker *pTracker, MappedGuild *pMappedGuild)
{
	Guild *pGuild;

	PERFINFO_AUTO_START_FUNC();

	pGuild = GET_REF(pTracker->hGuild);
	if(pGuild && !pMappedGuild && pTracker->gatewaytype == GLOBALTYPE_GUILD)
	{
		pMappedGuild = StructAlloc(parse_MappedGuild);
		FillMappedGuild(pSess, pMappedGuild, pGuild);
	}

	PERFINFO_AUTO_STOP();
	return pMappedGuild;
}

void DestroyMappedGuild(GatewaySession *pSess, ContainerTracker *ptracker, MappedGuild *pguild)
{
	PERFINFO_AUTO_START_FUNC();
	if(pguild)
	{
		StructDestroy(parse_MappedGuild, pguild);
	}
	PERFINFO_AUTO_STOP();
}

#include "NNOGatewayMappedGuild_c_ast.c"

// End of File

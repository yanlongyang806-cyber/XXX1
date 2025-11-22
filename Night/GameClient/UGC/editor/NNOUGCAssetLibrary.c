#include "NNOUGCAssetLibrary.h"

#include "../StaticWorld/WorldGridPrivate.h"
#include "../StaticWorld/group.h"
#include "Color.h"
#include "CostumeCommon.h"
#include "GfxClipper.h"
#include "GfxHeadshot.h"
#include "GfxPrimitive.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCResource.h"
#include "NNOUGCUIAnimatedResourcePreview.h"
#include "NNOUGCUIResourcePreview.h"
#include "NNOUGCZeniPicker.h"
#include "ObjectLibrary.h"
#include "ReferenceSystem.h"
#include "ResourceInfo.h"
#include "ResourceManagerUI.h"
#include "ResourceSearch.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "TokenStore.h"
#include "UGCCommon.h"
#include "UIMinimap.h"
#include "UITextureAssembly.h"
#include "contact_common.h"
#include "file.h"
#include "gclResourceSnap.h"
#include "wlUGC.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

static void ugcAssetLibraryDragCallback( UIWidget* ignored, UGCAssetLibraryPane *lib_pane);
static void ugcAssetLibraryDoubleClickCallback(UIButton *button, UGCAssetLibraryPane *lib_pane);
static void ugcAssetLibraryPaneRefreshUI(UGCAssetLibraryPane *lib_pane);

AUTO_STRUCT;
typedef struct UGCAssetLibraryPaneState
{
	const char* strSortCategory;			AST( NAME(SortCategory) )
	const char* strTextFilter;				AST( NAME(TextFilter) )
	bool bExpandedFilters;					AST( NAME(ExpandedFilters) )
} UGCAssetLibraryPaneState;
extern ParseTable parse_UGCAssetLibraryPaneState[];
#define TYPE_parse_UGCAssetLibraryPaneState UGCAssetLibraryPaneState

typedef struct UGCAssetLibraryPane
{
	/// Widgets:
	// Root pane
	UIPane* pRootPane;

	// The context holding all the other widgets
	MEFieldContext* pRootContext;

	// If set, we are updating UI and so we don't want to respond to changed callbacks.
	bool bIgnoreChanges;

	// Description pane can be either inside PANE (in windowed mode)
	// or as a floating window (in embedded mode)
	UIPane* pDetailsPane;

	// A sprite that's placed right near pDetailsPane
	UISprite* pDetailsSprite;

	// Lists of checkboxes for each tagtype.
	UIList** eaTagTypeFilterLists;

	// The list of all things available.
	UIList *pLibraryList;

	// An unowned widget that gets placed centered in the asset library
	UIWidget* pHeaderWidget;

	UGCAssetLibraryPaneState state;


	/// Other search data:
	REF_TO(UGCAssetTagType) hTagType;
	char** eaExtraFitlerTags;
	UGCAssetLibraryCustomFilterFn pExtraFilterFn;
	UserData pExtraFilterData;
	UGCMapType eExtraFilterMapType;

	// Search results, not including text filter (kept as an
	// optimization)
	UGCAssetLibraryRow** eaResultsNotIncludingTextFilter;

	// Final search results
	UGCAssetLibraryRow** eaResults;


	/// Misc flags:
	UGCAssetLibrarySelectFn pLibraryDragCB;
	UGCAssetLibrarySelectFn pLibraryDoubleClickCB;
	UserData userdata;
	bool bUseProjectData;
	UGCAssetLibraryMode eMode;
} UGCAssetLibraryPane;

AUTO_STRUCT;
typedef struct UGCAssetLibraryTagTypeListRow {
	REF_TO(Message) hDisplayName;		AST(NAME("DisplayName"))
	const char* strTag;					AST(NAME("Tag"))
} UGCAssetLibraryTagTypeListRow;
extern ParseTable parse_UGCAssetLibraryTagTypeListRow[];
#define TYPE_parse_UGCAssetLibraryTagTypeListRow UGCAssetLibraryTagTypeListRow

AUTO_STRUCT;
typedef struct UGCAssetLibraryTagTypeListData {
	UGCAssetLibraryTagTypeListRow** eaRows;
} UGCAssetLibraryTagTypeListData;
extern ParseTable parse_UGCAssetLibraryTagTypeListData[];
#define TYPE_parse_UGCAssetLibraryTagTypeListData UGCAssetLibraryTagTypeListData

#define ugcAssetLibraryRowDisplayName(row,buffer) ugcAssetLibraryRowDisplayNameSafe(row,SAFESTR(buffer))


static int ugcAssetLibraryPickerWindowsOpen = 0;

static char* ugcAssetLibraryRowDisplayNameSafe( const UGCAssetLibraryRow* row, char* buffer, int buffer_size )
{
	const char* translatedName = NULL;
	if( row->pProperties ) {
		translatedName = TranslateDisplayMessageOrEditCopy( row->pProperties->dVisibleName );
	}

	if( translatedName ) {
		strcpy_s( SAFESTR2( buffer ), translatedName );
	} else {
		sprintf_s( SAFESTR2( buffer ), "%s (UNTRANSLATED)", row->pcName );
	}

	return buffer;
}

static char *ugcAssetLibraryGetHumanReadableTags(UGCAssetLibraryRow *pRow, UGCAssetLibraryPane *lib_pane)
{
	char* estrAccum = NULL;
	if(pRow->astrTags)
	{
		char** eastrTags = NULL;

		DivideString( pRow->astrTags, ",", &eastrTags,
					  DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_REMOVEUNIQUE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );
		FOR_EACH_IN_EARRAY_FORWARDS( eastrTags, char, tag ) {
			
			if( stricmp( "UGC", tag ) == 0 ) {
				continue;
			}

			FOR_EACH_IN_EARRAY_FORWARDS( GET_REF(lib_pane->hTagType)->eaCategories, UGCAssetTagCategory, category)
			{
				FOR_EACH_IN_EARRAY_FORWARDS(category->eaTags, UGCAssetTag, categoryTag)
				{
					char buffer[ 512 ];
					sprintf( buffer, "%s_%s", category->pcName, categoryTag->pcName );
					if( 0 == stricmp( buffer, tag )) {
						if( estrLength( &estrAccum ) > 0 ) {
							estrConcatf( &estrAccum, ", " );
						}

						FormatMessageRef( &estrAccum, categoryTag->hDisplayName, STRFMT_END );
						goto searchDone;
					}
				} FOR_EACH_END;
			} FOR_EACH_END;

		searchDone:
			;
		} FOR_EACH_END;

		eaDestroyEx( &eastrTags, NULL );
	}

	return estrAccum;
}

static char *ugcAssetLibraryGetHumanReadableEncounterInfo(UGCAssetLibraryRow *pRow, UGCAssetLibraryPane *lib_pane)
{
	char *estrBuf = NULL;
	char *estrDescOut = NULL;
	if(pRow->pProperties && eaSize(&pRow->pProperties->groupDefProps.eaEncounterActors))
	{
		char **eaEstrActors = NULL;
		int index;
		for(index = 0; index < eaSize(&pRow->pProperties->groupDefProps.eaEncounterActors); index++)
		{
			WorldUGCActorProperties *actor = pRow->pProperties->groupDefProps.eaEncounterActors[index];
			const char *tr_name = TranslateDisplayMessageOrEditCopy(actor->displayNameMsg);
			const char *rank = actor->pcRankName;

			if(tr_name)
			{
				char message_key[256];
				const char *tr_group_name = TranslateDisplayMessageOrEditCopy(actor->groupDisplayNameMsg);
				const char * tr_class_name = NULL;
				sprintf(message_key, "UGC.CharacterClass.%s", actor->pcClass);
				tr_class_name = TranslateMessageKey(message_key);
				estrPrintf(&estrBuf, "%s - %s - %s", tr_name, tr_group_name, tr_class_name);
			}
			else if(rank)
			{
				char message_key[256];
				const char *tr_rank = NULL;
				sprintf(message_key, "UGC.Rank.%s", rank);
				tr_rank = TranslateMessageKey(message_key);
				estrPrintf(&estrBuf, "%s", tr_rank);
			}

			if(estrBuf)
			{
				eaPush(&eaEstrActors, estrBuf);

				estrBuf = NULL;
			}
		}

		for(index = 0; index < eaSize(&eaEstrActors); index++)
		{
			int count = 1;

			// remove duplicates while counting them
			int index2 = index + 1;
			while(index2 < eaSize(&eaEstrActors))
			{
				if(stricmp(eaEstrActors[index], eaEstrActors[index2]) == 0)
				{
					estrDestroy(&eaEstrActors[index2]);
					eaRemove(&eaEstrActors, index2);
					count++;
				}
				else
					index2++;
			}

			if(count > 1)
			{
				FormatMessageKey( &estrDescOut, "UGC_AssetLibrary.EncounterInfo_Multiple",
								  STRFMT_STRING( "EncounterInfo", eaEstrActors[index] ),
								  STRFMT_INT( "Count", count ),
								  STRFMT_END );
			}
			else
			{
				estrConcatf( &estrDescOut, "%s", eaEstrActors[ index ]);
			}
			estrConcatf( &estrDescOut, "<br>" );
		}

		eaDestroyEString(&eaEstrActors);
	}

	return estrDescOut;
}

static char *ugcAssetLibraryGetExtraDetailData(UGCAssetLibraryRow *pRow, UGCAssetLibraryPane *lib_pane)
{
	char* estr = NULL;
	
	if( pRow->astrType == allocAddString( "Objectlibrary" )) {
		if( pRow->pProperties && eaSize( &pRow->pProperties->groupDefProps.eaEncounterActors )) {
			;
		} else {
			int objlibId = atoi( pRow->pcName );
			Vec3 boundsMin;
			Vec3 boundsMax;
			float fRadius;
			if( objlibId && ugcComponentCalcBoundsForObjLib( objlibId, boundsMin, boundsMax, &fRadius )) {
				Vec3 dimensions;
				subVec3( boundsMax, boundsMin, dimensions );
				ugcFormatMessageKey( &estr, "UGC_AssetLibrary.ObjectDimensions",
									 STRFMT_INT( "X", (int)ceil( dimensions[ 0 ])),
									 STRFMT_INT( "Y", (int)ceil( dimensions[ 1 ])),
									 STRFMT_INT( "Z", (int)ceil( dimensions[ 2 ])),
									 STRFMT_END );
			}
		}
	} else if( pRow->astrType == allocAddString( "ZoneMap" )) {
		Vec3 boundsMin;
		Vec3 boundsMax;
		Vec3 dimensions;
		ugcGetZoneMapPlaceableBounds( boundsMin, boundsMax, pRow->pcName, true );

		if( !vec3IsZero( boundsMin ) && !vec3IsZero( boundsMax )) {
			subVec3( boundsMax, boundsMin, dimensions );
			ugcFormatMessageKey( &estr, "UGC_AssetLibrary.MapDimensions",
								 STRFMT_INT( "X", (int)ceil( dimensions[ 0 ])),
								 STRFMT_INT( "Y", (int)ceil( dimensions[ 1 ])),
								 STRFMT_INT( "Z", (int)ceil( dimensions[ 2 ])),
								 STRFMT_END );
		}
	}

	return estr;
}

static char *ugcAssetLibraryGetDescription(UGCAssetLibraryRow *pRow, UGCAssetLibraryPane *lib_pane)
{
	char *estrDescOut = NULL;

	{	
		char* estrTags = ugcAssetLibraryGetHumanReadableTags(pRow, lib_pane);
		if(estrTags)
		{
			estrConcatf(&estrDescOut, "%s<br>", estrTags);
			estrDestroy(&estrTags);
		}
	}
	if( GET_REF( lib_pane->hTagType )->pcName == allocAddString( "Detail" )) {
		FormatMessageKey( &estrDescOut, "UGC_AssetLibrary.Cost",
						  STRFMT_INT( "Cost", MAX( 1, pRow->pProperties->groupDefProps.iCost )),
						  STRFMT_END );
	}
	if( estrLength( &estrDescOut )) {
		estrConcatf( &estrDescOut, "<br>" );
	}

	if(pRow->pProperties) {
		if (!ugcIsFixedLevelEnabled())
		{
			if(pRow->pProperties->restrictionProps.iMinLevel && pRow->pProperties->restrictionProps.iMaxLevel) {
				FormatMessageKey( &estrDescOut, "UGC_AssetLibrary.LevelRestriction",
								  STRFMT_INT( "MinLevel", pRow->pProperties->restrictionProps.iMinLevel ),
								  STRFMT_INT( "MaxLevel", pRow->pProperties->restrictionProps.iMaxLevel ),
								  STRFMT_END );
				estrConcatf( &estrDescOut, "<br>" );
			} else if(pRow->pProperties->restrictionProps.iMinLevel) {
				FormatMessageKey( &estrDescOut, "UGC_AssetLibrary.LevelRestriction_JustMin",
								  STRFMT_INT( "MinLevel", pRow->pProperties->restrictionProps.iMinLevel ),
								  STRFMT_END );
				estrConcatf( &estrDescOut, "<br>" );
			} else if(pRow->pProperties->restrictionProps.iMaxLevel) {
				FormatMessageKey( &estrDescOut, "UGC_AssetLibrary.LevelRestriction_JustMax",
								  STRFMT_INT( "MaxLevel", pRow->pProperties->restrictionProps.iMaxLevel ),
								  STRFMT_END );
				estrConcatf( &estrDescOut, "<br>" );
			}
		}
	}
	
	{
		const char* desc = (pRow->pProperties ? TranslateDisplayMessageOrEditCopy( pRow->pProperties->dDescription ) : NULL);
		if( desc ) {
			estrConcatf( &estrDescOut, "%s<br>", desc );
		} else {
			estrConcatf( &estrDescOut, "%s<br>", TranslateMessageKey( "UGC_AssetLibrary.NoDescription" ));
		}
	}

	if (pRow->astrType == allocAddString("Objectlibrary"))
	{
		if(pRow->pProperties && eaSize(&pRow->pProperties->groupDefProps.eaEncounterActors))
		{
			char *text = ugcAssetLibraryGetHumanReadableEncounterInfo(pRow, lib_pane);
			if(text)
			{
				estrAppend2(&estrDescOut, text);
				estrDestroy(&text);
			}
		}
	}

	return estrDescOut;
}

static void ugcAssetLibraryAddResultRow(UGCAssetLibraryRow ***peaRows, ResourceSearchResultRow *pSearchRow, ResourceInfo *pInfo, UGCProjectData *ugcProj)
{
	UGCAssetLibraryRow *pRow = StructCreate(parse_UGCAssetLibraryRow);
	const char *display_name = NULL;
	const WorldUGCProperties *props;

	// Copy over basic information
	pRow->pcName = StructAllocString(pSearchRow->pcName);
	pRow->astrType = allocAddString(pSearchRow->pcType);

	if (pInfo)
		pRow->astrTags = pInfo->resourceTags;

	props = ugcResourceGetUGCProperties(pRow->astrType, pRow->pcName);

	if (pRow->astrType == allocAddString("Objectlibrary"))
	{
		GroupDef *def = objectLibraryGetGroupDefByName(pRow->pcName, false); 
		if (props)
			pRow->pProperties = StructClone(parse_WorldUGCProperties, props);
		else
			pRow->pProperties = StructCreate(parse_WorldUGCProperties);
	}
	else if (pRow->astrType == allocAddString("Trap"))
	{
		int objlibID;
		char objlibIDAsText[ 256 ];
		char powerName[ 256 ];
		const WorldUGCProperties* objlibProps;

		sscanf_s( pRow->pcName, "%d,%s", &objlibID, SAFESTR( powerName ));
		sprintf( objlibIDAsText, "%d", objlibID );
		objlibProps = ugcResourceGetUGCProperties( "ObjectLibrary", objlibIDAsText );
		
		pRow->pProperties = StructCreate( parse_WorldUGCProperties );
		pRow->pProperties->dVisibleName.pEditorCopy = StructCreate( parse_Message );
		pRow->pProperties->dVisibleName.pEditorCopy->pcDefaultString = StructAllocString( ugcTrapGetDisplayName( objlibID, powerName, "UNNAMED" ));
		if( objlibProps ) {
			StructCopyAll( parse_DisplayMessage, &objlibProps->dDescription, &pRow->pProperties->dDescription );
		}
	}
	else if (ugcProj && pRow->astrType == allocAddString("UGCCostume"))
	{
		UGCCostume *costume = ugcCostumeFindByName(ugcProj, pRow->pcName);
		if (costume)
		{
			char buffer[ RESOURCE_NAME_MAX_SIZE ];
			pRow->pProperties = StructCreate(parse_WorldUGCProperties);
			pRow->pProperties->dVisibleName.pEditorCopy = StructCreate(parse_Message);
			sprintf(buffer, "%s (My project)", costume->pcDisplayName);
			pRow->pProperties->dVisibleName.pEditorCopy->pcDefaultString = StructAllocString(buffer);
			pRow->pProperties->dDescription.pEditorCopy = StructCreate(parse_Message);
			pRow->pProperties->dDescription.pEditorCopy->pcDefaultString = StructAllocString(costume->pcDescription);
		}
	}
	else if (ugcProj && pRow->astrType == allocAddString("UGCItem"))
	{
		UGCItem *item = ugcItemFindByName(ugcProj, pRow->pcName);
		if (item)
		{
			pRow->pProperties = StructCreate(parse_WorldUGCProperties);
			pRow->pProperties->dVisibleName.pEditorCopy = StructCreate(parse_Message);
			pRow->pProperties->dVisibleName.pEditorCopy->pcDefaultString = StructAllocString(item->strDisplayName);
			pRow->pProperties->dDescription.pEditorCopy = StructCreate(parse_Message);
			pRow->pProperties->dDescription.pEditorCopy->pcDefaultString = StructAllocString(item->strDescription);
			pRow->pProperties->pchImageOverride = allocAddString(item->strIcon);
		}
	}
	else if (pRow->astrType == allocAddString("CheckedAttrib"))
	{
		char buffer[ RESOURCE_NAME_MAX_SIZE ];
		pRow->pProperties = StructCreate(parse_WorldUGCProperties);
		sprintf( buffer, "UGC.CheckedAttrib_%s", pRow->pcName);
		SET_HANDLE_FROM_STRING("Message", buffer, pRow->pProperties->dVisibleName.hMessage);
	}
	else if (pRow->astrType == allocAddString("Special"))
	{
		UGCSpecialComponentDef* def = ugcDefaultsSpecialComponentDef( pRow->pcName );

		if( def ) {
			pRow->pProperties = StructCreate( parse_WorldUGCProperties );
			SET_HANDLE_FROM_STRING( gMessageDict, def->astrMessageKey, pRow->pProperties->dVisibleName.hMessage );
			SET_HANDLE_FROM_STRING( gMessageDict, def->astrDescriptionMessageKey, pRow->pProperties->dDescription.hMessage );
			pRow->pProperties->pchImageOverride = def->astrTextureOverride;
		}
	}
	else if (props)
	{
		pRow->pProperties = StructClone(parse_WorldUGCProperties, props);
	}

	if (pRow->pProperties)
	{
		if (pRow->pProperties->dVisibleName.pEditorCopy)
		{
			display_name = pRow->pProperties->dVisibleName.pEditorCopy->pcDefaultString;
		}
		else
		{
			display_name = TranslateDisplayMessageOrEditCopy(pRow->pProperties->dVisibleName);
		}
	}

	// Add the row to the results
	eaPush(peaRows, pRow);
}

void ugcAssetLibraryPaneSetExtraFilter(UGCAssetLibraryPane *lib_pane, UGCAssetLibraryCustomFilterFn fn, UserData data)
{
	lib_pane->pExtraFilterFn = fn;
	lib_pane->pExtraFilterData = data;

	ugcAssetLibraryPaneRefreshLibraryModel( lib_pane );
}

static UGCSpecialComponentDef* ugcAssetLibraryGetSpecialComponentByName( const char* name )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	FOR_EACH_IN_EARRAY( defaults->eaSpecialComponents, UGCSpecialComponentDef, def ) {
		if( stricmp( def->pcLabel, name ) == 0 ) {
			return def;
		}
	} FOR_EACH_END;

	return NULL;
}

bool ugcAssetLibraryRowFillComponentTypeAndName( UGCAssetLibraryPane* pane, UGCAssetLibraryRow* row, UGCComponentType* out_type, char* out_rowName, int out_rowName_size )
{
	const char* tag_type_name = ugcAssetLibraryPaneGetTagTypeName( pane );
	
	if (stricmp(tag_type_name, "Detail") == 0 ||
		stricmp(tag_type_name, "SpaceDetail") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_OBJECT;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "UGCSound") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_SOUND;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "Destructible") == 0 ||
		stricmp(tag_type_name, "SpaceDestructible") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_DESTRUCTIBLE;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "Building") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_BUILDING_DEPRECATED;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "Costume") == 0 ||
		stricmp(tag_type_name, "SpaceCostume") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_CONTACT;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "Encounter") == 0 || 
		stricmp(tag_type_name, "SpaceEncounter") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_KILL;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "Planet") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_PLANET;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "Room") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_ROOM;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if (stricmp(tag_type_name, "RoomDoor") == 0)
	{
		assertmsg(0, "Not implemented");
	}
	else if (stricmp(tag_type_name, "Trap") == 0)
	{
		*out_type = UGC_COMPONENT_TYPE_TRAP;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if( stricmp( tag_type_name, "Teleporter" ) == 0 )
	{
		*out_type = UGC_COMPONENT_TYPE_TELEPORTER;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if( stricmp( tag_type_name, "Cluster" ) == 0 )
	{
		*out_type = UGC_COMPONENT_TYPE_CLUSTER;
		strcpy_s( SAFESTR2( out_rowName ), row->pcName );
		return true;
	}
	else if( stricmp( tag_type_name, "Special" ) == 0 )
	{
		UGCSpecialComponentDef* def = ugcAssetLibraryGetSpecialComponentByName( row->pcName );

		if( def ) {
			*out_type = def->eType;
			strcpy_s( SAFESTR2( out_rowName ), "" );
			if( def->astrObjectName ) {
				GroupDef* groupDef = objectLibraryGetGroupDefByName( def->astrObjectName, false );
				if( !groupDef ) {
					return false;
				}

				sprintf_s( SAFESTR2( out_rowName ), "%d", groupDef->name_uid );
			}
			return true;
		}
	}

	*out_type = 0;
	strcpy_s( SAFESTR2( out_rowName ), "" );
	return false;
}

//compares UGCAssetLibraryRows by project then name.  Used by ugcAssetLibraryDoSearch().
static int ugcAssetLibrarySortRows(UGCAssetLibraryPane *lib_pane, const UGCAssetLibraryRow** row_1, const UGCAssetLibraryRow** row_2)
{
	const UGCAssetLibraryRow* row1 = *row_1;
	const UGCAssetLibraryRow* row2 = *row_2;
	char sort_name1[ 1024 ];
	char sort_name2[ 1024 ];

	//check for UGC namespaces, return the one that is
	if (resNamespaceIsUGC(row1->pcName) != resNamespaceIsUGC(row2->pcName))
	{
		if (resNamespaceIsUGC(row1->pcName))
			return 1;
		else
			return -1;
	}
	if( SAFE_MEMBER( row1->pProperties, iSortPriority ) != SAFE_MEMBER( row2->pProperties, iSortPriority )) {
		return SAFE_MEMBER( row1->pProperties, iSortPriority ) - SAFE_MEMBER( row2->pProperties, iSortPriority );
	} 

	//Sort by name.
	ugcAssetLibraryRowDisplayName( row1, sort_name1 );
	ugcAssetLibraryRowDisplayName( row2, sort_name2 );
	
	return -stricmp(sort_name1, sort_name2);
}

/// Do the actual search, returning a fresh EArray of results.
static UGCAssetLibraryRow** ugcAssetLibraryDoSearch(UGCAssetLibraryPane *lib_pane)
{
	UGCAssetTagType *tag_type = GET_REF(lib_pane->hTagType);
	ResourceSearchRequestTags** eaNodesToAnd = { 0 };
	ResourceSearchResult *result;
	ResourceSearchRequest request = {0};
	ResourceSearchRequestTags sAndNode = { 0 };
	UGCAssetLibraryRow **ret = NULL;
	const char **filter_values = NULL;
	int i;

	if( !tag_type ) {
		return NULL;
	}

	{
		ResourceSearchRequestTags* ugcNode = StructCreate( parse_ResourceSearchRequestTags );
		ugcNode->strTag = StructAllocString( "UGC" );
		eaPush( &eaNodesToAnd, ugcNode );
	}
	if (tag_type->bFilterType)
	{
		ResourceSearchRequestTags* typeNode = StructCreate( parse_ResourceSearchRequestTags );
		typeNode->strTag = StructAllocString( tag_type->pcName );
		eaPush( &eaNodesToAnd, typeNode );
	}
	if (lib_pane)
	{
		FOR_EACH_IN_EARRAY(lib_pane->eaTagTypeFilterLists, UIList, filterList)
		{
			const S32* const* peaSelected = ui_ListGetSelectedRows( filterList );

			if( eaiSize( peaSelected ) && (*peaSelected)[0] != -1 )
			{
				ResourceSearchRequestTags** eaCategoryTags = NULL;
				int selectedIt;
				for( selectedIt = 0; selectedIt != eaiSize( peaSelected ); ++selectedIt ) {
					int sel = (*peaSelected)[ selectedIt ];
					UGCAssetLibraryTagTypeListRow* selRow = eaGet( filterList->peaModel, sel );

					if( SAFE_MEMBER( selRow, strTag )) {
						ResourceSearchRequestTags* tagNode = StructCreate( parse_ResourceSearchRequestTags );
						tagNode->strTag = StructAllocString( selRow->strTag );
						eaPush( &eaCategoryTags, tagNode );
					}
				}

				if( eaSize( &eaCategoryTags ) > 0 ) {
					ResourceSearchRequestTags* pCategoryNode = StructCreate( parse_ResourceSearchRequestTags );
					pCategoryNode->type = SEARCH_TAGS_NODE_OR;
					pCategoryNode->eaChildren = eaCategoryTags;
					eaPush( &eaNodesToAnd, pCategoryNode );
				} else {
					eaDestroy( &eaCategoryTags );
				}
			}
		}
		FOR_EACH_END;

		FOR_EACH_IN_EARRAY(lib_pane->eaExtraFitlerTags, const char, tag)
		{
			ResourceSearchRequestTags* pTagNode = StructCreate( parse_ResourceSearchRequestTags );
			pTagNode->strTag = StructAllocString( tag );
			eaPush( &eaNodesToAnd, pTagNode );
		}
		FOR_EACH_END;
	}

	// Set up search request
	sAndNode.type = SEARCH_TAGS_NODE_AND;
	sAndNode.eaChildren = eaNodesToAnd;
	request.eSearchMode = SEARCH_MODE_TAG_COMPLEX_SEARCH;
	request.pTagsDetails = &sAndNode;
	request.pcName = NULL;
	request.pcType = (char*)tag_type->pcDictName;
	request.iRequest = 1;

	// Do search within the project
	if (lib_pane->bUseProjectData)
	{
		UGCProjectData* project_data = ugcEditorGetProjectData();
		result = ugcProjectSearchRequest( project_data, &request, lib_pane->eExtraFilterMapType );
		for(i=0; i<eaSize(&result->eaRows); ++i) {
			ugcAssetLibraryAddResultRow(&ret, result->eaRows[i], NULL, project_data);
		}
		StructDestroy(parse_ResourceSearchResult, result);
	}

	// Do search of Cryptic resources
	result = ugcResourceSearchRequest( &request );
	for(i=0; i<eaSize(&result->eaRows); ++i) {
		ResourceInfo *info = ugcResourceGetInfo(tag_type->pcDictName, result->eaRows[i]->pcName);
		ugcAssetLibraryAddResultRow(&ret, result->eaRows[i], info, NULL);
	}
	StructDestroy(parse_ResourceSearchResult, result);

	// Remove any results that do not match the allegiance, or any other custom filter
	for(i=0; i<eaSize(&ret); ++i) {
		if( ret[i]->pProperties && !ugcEditorObjectRestrictionSetIsValid( &ret[i]->pProperties->restrictionProps )) {
			StructDestroySafe(parse_UGCAssetLibraryRow, &ret[i]);
			eaRemove(&ret, i);
			--i;
		}
		else
		{
			if (lib_pane->pExtraFilterFn && !lib_pane->pExtraFilterFn(lib_pane->pExtraFilterData, ret[i]))
			{
				StructDestroySafe(parse_UGCAssetLibraryRow, &ret[i]);
				eaRemove(&ret, i);
				--i;
			}
		}
	}

	//sort the results
	if(ret)
		eaQSort_s(ret, ugcAssetLibrarySortRows, lib_pane);

	eaDestroy(&filter_values);
	eaDestroyStruct( &eaNodesToAnd, parse_ResourceSearchRequestTags );
	return ret;
}

/// A quick version of ugcAssetLibraryPaneRefreshLibraryModel() that
/// just does text filtering.
static void ugcAssetLibraryPaneRefreshLibraryModelFiltered(UGCAssetLibraryPane *lib_pane)
{
	UGCAssetLibraryRow *prev_selected = ugcAssetLibraryPaneGetSelected(lib_pane);
	int selected = -1;
	char searchbuf[ MAX_PATH ];

	sprintf( searchbuf, "*%s*", NULL_TO_EMPTY( lib_pane->state.strTextFilter ));

	eaClear(&lib_pane->eaResults);
	if( strlen( searchbuf ) < 100 ) {
		FOR_EACH_IN_EARRAY(lib_pane->eaResultsNotIncludingTextFilter, UGCAssetLibraryRow, row)
		{
			char displayNameBuf[ 1024 ];
			if( match( searchbuf, ugcAssetLibraryRowDisplayName( row, displayNameBuf )))
			{
				if (row == prev_selected)
					selected = eaSize(&lib_pane->eaResults);
				eaPush(&lib_pane->eaResults, row);
			}
		}
		FOR_EACH_END;
	}

	ui_ListSetSelectedRow(lib_pane->pLibraryList, selected);
}

/// Update LIB-PANE's library list model.
void ugcAssetLibraryPaneRefreshLibraryModel(UGCAssetLibraryPane *lib_pane)
{
	ugcAssetLibraryPaneRefreshUI(lib_pane);
	eaDestroyStruct(&lib_pane->eaResultsNotIncludingTextFilter, parse_UGCAssetLibraryRow);
	lib_pane->eaResultsNotIncludingTextFilter = ugcAssetLibraryDoSearch(lib_pane);

	ugcAssetLibraryPaneRefreshLibraryModelFiltered(lib_pane);
}

static void ugcAssetLibraryTagFilterSelected(UIList *list, UGCAssetLibraryPane *lib_pane)
{
	ugcAssetLibraryPaneRefreshLibraryModel(lib_pane);
}

static void ugcAssetLibraryTagFilterCellClicked( UIList *pList, S32 iColumn, S32 iRow, F32 fMouseX, F32 fMouseY, CBox *pBox, UserData pCellData )
{
	// If the item selected was the "(any)" check, then clear everything and select just that
	if( iRow == 0 ) {
		ui_ListClearSelected( pList );
		ui_ListSetSelectedRowAndCallback( pList, 0 );
	} else {
		ui_ListDeselectRow( pList, 0 );
		ui_ListCellClickedDefault( pList, iColumn, iRow, fMouseX, fMouseY, pBox, pCellData );
	}
}

static void ugcAssetLibraryTagFilterCellActivated(UIList *pList, S32 iColumn, S32 iRow, F32 fMouseX, F32 fMouseY, CBox *pBox, UserData pCellData)
{
	// If the item selected was the "(any)" check, then clear everything and select just that
	if( iRow == 0 ) {
		ui_ListClearSelected( pList );
		ui_ListSetSelectedRowAndCallback( pList, 0 );
	} else {
		ui_ListDeselectRow( pList, 0 );
		ui_ListCellActivatedDefault( pList, iColumn, iRow, fMouseX, fMouseY, pBox, pCellData );
	}
}

static void ugcAssetLibraryPaneDetailsCloseCB( UIWidget* ignored, UGCAssetLibraryPane* lib_pane )
{
	ui_WidgetQueueFreeAndNull( &lib_pane->pDetailsPane );
	ui_WidgetQueueFreeAndNull( &lib_pane->pDetailsSprite );
	if( lib_pane->eMode == UGCAssetLibrary_MapEditorEmbedded || lib_pane->eMode == UGCAssetLibrary_PlayingEditorEmbedded ) {
		ui_ListClearSelected( lib_pane->pLibraryList );
	}
}

static void ugcAssetLibraryPaneRefreshDetailsUI(UGCAssetLibraryPane *lib_pane, bool explicitUI)
{
	UGCAssetLibraryRow* row_data = ui_ListGetSelectedObject( lib_pane->pLibraryList );
	UITextureAssembly* popupTexas;
	float popupWidth;
	float popupHeight;
	float popupTextLeftPadding;
	UILabel* label;
	UIPane* pane;
	UIPane* scrollAreaPane;
	UISMFView* smfView;
	UIButton* button;
	UIScrollArea* scrollArea;
	UIMinimap* minimap;
	UIWidget* widget;
	float closeButtonPadding;
	float y;
	CBox rowBox;
	float popupContentWidth;
	float popupContentHeight;

	ui_WidgetQueueFreeAndNull( &lib_pane->pDetailsPane );
	ui_WidgetQueueFreeAndNull( &lib_pane->pDetailsSprite );

	if( !row_data || lib_pane->eMode == UGCAssetLibrary_Legacy ) {
		return;
	}

	if( GET_REF( lib_pane->hTagType )->bEnableIconGridView ) {
		popupTexas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Popup_Window" );
		popupWidth = 500;
		popupHeight = 300;
		popupTextLeftPadding = 300 - ui_TextureAssemblyHeight( popupTexas );
	} else {
		popupTexas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Details_Popup_Window_NoPreview" );
		popupWidth = 200;
		popupHeight = 300;
		popupTextLeftPadding = 0;
	}

	lib_pane->bIgnoreChanges = true;

	ui_ListGetSelectedRowBox( lib_pane->pLibraryList, &rowBox );
	{
		AtlasTex* spriteTex = atlasFindTexture( "UGC_Kits_Details_Arrow" );

		float selectedYCenter = CLAMP( (rowBox.ly + rowBox.hy) / 2, lib_pane->pLibraryList->lastDrawBox.ly, lib_pane->pLibraryList->lastDrawBox.hy );
		float paneY = MIN( selectedYCenter - popupHeight / 2,
						   lib_pane->pLibraryList->lastDrawBox.hy + spriteTex->height / 2 - popupHeight );

		lib_pane->pDetailsSprite = ui_SpriteCreate( 0, 0, -1, -1, spriteTex->name );
		ui_WidgetSetPosition( UI_WIDGET( lib_pane->pDetailsSprite ),
							  lib_pane->pLibraryList->lastDrawBox.lx - spriteTex->width + 8,
							  selectedYCenter - spriteTex->height / 2 );
		UI_WIDGET( lib_pane->pDetailsSprite )->priority = UI_HIGHEST_PRIORITY;
		ui_TopWidgetAddToDevice( UI_WIDGET( lib_pane->pDetailsSprite ), NULL );

		lib_pane->pDetailsPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		ui_PaneSetStyle( lib_pane->pDetailsPane, popupTexas->pchName, true, false );
		ui_WidgetSetPositionEx( UI_WIDGET( lib_pane->pDetailsPane ), lib_pane->pDetailsSprite->widget.x - popupWidth, paneY, 0, 0, UITopLeft );
		ui_WidgetSetDimensions( UI_WIDGET( lib_pane->pDetailsPane ), popupWidth, popupHeight );
		UI_WIDGET( lib_pane->pDetailsPane )->priority = UI_HIGHEST_PRIORITY;
		ui_TopWidgetAddToDevice( UI_WIDGET( lib_pane->pDetailsPane ), NULL );
		popupContentWidth = popupWidth - ui_TextureAssemblyWidth( popupTexas );
		popupContentHeight = popupHeight - ui_TextureAssemblyHeight( popupTexas );
	}

	{
		bool bUseAnimatedPreview = (row_data != NULL
									&& nullStr( SAFE_MEMBER( row_data->pProperties, pchImageOverride )) 
									&& (row_data->astrType == allocAddString( "PlayerCostume" )
										|| row_data->astrType == allocAddString( "UGCCostume" )
										|| row_data->astrType == allocAddString( "ObjectLibrary" )
										|| row_data->astrType == allocAddString( "UGCSound" ))
									&& stricmp( GET_REF(lib_pane->hTagType)->pcName, "Planet") != 0
									&& stricmp( GET_REF(lib_pane->hTagType)->pcName, "SpaceDetail") != 0 );
		bool bUseMinimapPreview = (row_data != NULL
								   && row_data->astrType == allocAddString( "ZoneMap" ));
		UIWidget* buttonWidget = NULL;

		if( bUseMinimapPreview ) {
			const char* mapName = row_data->pcName;
			ZoneMapEncounterRegionInfo* mapZeniRegion = ugcGetZoneMapDefaultRegion( mapName );
			ZoneMapEncounterRoomInfo* mapPlayableVolume = ugcGetZoneMapPlayableVolume( mapName );

			scrollArea = ui_ScrollAreaCreate( 0, 0, 0, 0, 0, 0, true, true );
			widget = UI_WIDGET( scrollArea );
			scrollArea->autosize = true;
			scrollArea->draggable = true;
			scrollArea->enableDragOnLeftClick = true;
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCEditor_TinyScrollBars", scrollArea->widget.sb->hOverrideSkin );
			ui_ScrollAreaSetNoCtrlDraggable( scrollArea, true );
			ui_ScrollAreaSetZoomSlider( scrollArea, true );
			widget->sb->scrollBoundsX = UIScrollBounds_KeepContentsAtViewCenter;
			widget->sb->scrollBoundsY = UIScrollBounds_KeepContentsAtViewCenter;

			minimap = ui_MinimapCreate();
			widget = UI_WIDGET( minimap );
			minimap->autosize = true;
			if( mapZeniRegion ) {
				Vec3 restrictMin;
				Vec3 restrictMax;
				ugcGetZoneMapPlaceableBounds( restrictMin, restrictMax, mapName, false );
				ui_MinimapSetMapAndRestrictToRegion( minimap, mapName, mapZeniRegion->regionName, restrictMin, restrictMax );
			}
			if( mapPlayableVolume ) {
				ui_MinimapSetMapHighlightArea( minimap, mapPlayableVolume->min, mapPlayableVolume->max );
			}
			ui_WidgetSetPosition( widget, 0, 0 );
			
			ui_ScrollAreaAddChild( scrollArea, widget );

			// Center the minimap
			ui_ScrollAreaScrollToPosition( scrollArea, widget->width / 2, widget->height / 2 );
			scrollArea->autoScrollCenter = true;
			scrollArea->scrollToTargetRemaining = scrollArea->childScale = 1.0f / 9000; //< It's (one) over NINE THOUSAND!
			widget = UI_WIDGET( scrollArea );
		} else if( bUseAnimatedPreview ) {
			UGCUIAnimatedResourcePreview* preview = ugcui_AnimatedResourcePreviewCreate();
			if( row_data->astrType == allocAddString( "UGCCostume" )) {
				UGCProjectData* ugcProj = (lib_pane->bUseProjectData ? ugcEditorGetProjectData() : NULL);
				UGCCostume* ugcCostume = ugcCostumeFindByName( ugcProj, row_data->pcName );
				PlayerCostume* costume = ugcEditorGetObject( "PlayerCostume", row_data->pcName );
				bool bIsSpace = (ugcCostume->eRegion == (U32)StaticDefineIntGetIntDefault(CharClassTypesEnum, "Space", -1));
				
				ugcui_AnimatedResourcePreviewSetCostume( preview, costume, !bIsSpace );
			} else {
				ugcui_AnimatedResourcePreviewSetResource( preview, row_data->astrType, row_data->pcName, GET_REF(lib_pane->hTagType)->bIsYTranslate );
			}
			ugcui_AnimatedResourcePreviewResetCamera( preview );
			widget = UI_WIDGET( preview );
		} else if( SAFE_MEMBER( row_data->pProperties, pchImageOverride )) {
			UISprite* sprite = ui_SpriteCreate( 0, 0, 1, 1, row_data->pProperties->pchImageOverride );
			sprite->bPreserveAspectRatioFill = true;
			widget = UI_WIDGET( sprite );
		} else {
			UGCUIResourcePreview* preview = ugcui_ResourcePreviewCreate();
			ugcui_ResourcePreviewSetResource( preview , row_data->astrType, row_data->pcName );
			widget = UI_WIDGET( preview );
		}
		ui_WidgetSetPosition( widget, 0, 0 );
		ui_WidgetSetDimensions( widget, popupContentHeight, popupContentHeight );
		ui_PaneAddChild( lib_pane->pDetailsPane, widget );

		y = 0;

		pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		widget = UI_WIDGET( pane );
		ui_PaneSetStyle( pane, "UGC_Details_Popup_Pane", true, false );
		ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_WidgetSetPaddingEx( widget, popupTextLeftPadding, 0, 0, 0 );
		ui_PaneAddChild( lib_pane->pDetailsPane, widget );

		if( lib_pane->eMode == UGCAssetLibrary_MapEditorEmbedded || lib_pane->eMode == UGCAssetLibrary_PlayingEditorEmbedded ) {
			button = ui_ButtonCreateImageOnly( "ugc_icon_window_controls_close", 0, 0, ugcAssetLibraryPaneDetailsCloseCB, lib_pane );
			widget = UI_WIDGET( button );
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
			ui_ButtonResize( button );
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopRight );
			ui_PaneAddChild( pane, widget );
			closeButtonPadding = ui_WidgetGetWidth( widget );
			buttonWidget = widget;
		} else {
			closeButtonPadding = 0;
		}

		{
			char buffer[ 1024 ];
			label = ui_LabelCreate( ugcAssetLibraryRowDisplayName( row_data, buffer ), 0, 0 );
		}
		widget = UI_WIDGET( label );
		UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
		ui_LabelSetWordWrap( label, true );
		ui_LabelUpdateDimensionsForWidth( label, 160 );
		ui_PaneAddChild( pane, widget );
		y = ui_WidgetGetNextY( widget );

		// Make sure the close button is centered in the text space
		if( buttonWidget ) {
			ui_WidgetSetHeight(	buttonWidget, 21 );
		}

		{
			char* estr = ugcAssetLibraryGetExtraDetailData( row_data, lib_pane );
			label = ui_LabelCreate( estr, 0, 0 );
			widget = UI_WIDGET( label );
			ui_WidgetSetPosition( widget, 0, y );
			ui_LabelSetWidthNoAutosize( label, 1, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 0, closeButtonPadding, 0, 0 );
			ui_PaneAddChild( pane, widget );
			y = ui_WidgetGetNextY( widget );

			estrDestroy( &estr );
		}
		y += 2;

		scrollAreaPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		widget = UI_WIDGET( scrollAreaPane );
		ui_PaneSetStyle( scrollAreaPane, "UGC_Pane_ContentArea", true, false );
		ui_WidgetSetPosition( widget, 0, y );
		ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_PaneAddChild( pane, widget );

		scrollArea = ui_ScrollAreaCreate( 0, 0, 0, 0, 0, 0, false, true );
		widget = UI_WIDGET( scrollArea );
		ui_WidgetSetPosition( widget, 0, 0 );
		ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
		ui_PaneAddChild( scrollAreaPane, widget );

		smfView = ui_SMFViewCreate( 0, 0, 0, 0 );
		widget = UI_WIDGET( smfView );
		{
			char* estrDesc = ugcAssetLibraryGetDescription( row_data, lib_pane );
			ui_SMFViewSetText( smfView, estrDesc, NULL );
			estrDestroy( &estrDesc );
		}
		ui_WidgetSetPosition( widget, 0, 0 );
		ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitFixed );
		ui_ScrollAreaAddChild( scrollArea, widget );
	}

	lib_pane->bIgnoreChanges = false;
}

static void ugcAssetLibraryIconGridDrawCB( UIList* pList, UIListColumn* pCol, UI_MY_ARGS, F32 z, CBox* pBox, int index, UGCAssetLibraryPane* libPane )
{
	UITextureAssembly* normal = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Idle" );
	UITextureAssembly* hover = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Over" );
	UITextureAssembly* pressed = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Pressed" );
	UITextureAssembly* selected = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Selected" );
	UITextureAssembly* overlay = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Overlay" );
	UITextureAssembly* overlayIdle = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Picker_Grid_Overlay_Idle" );
	
	bool isSelected = ui_ListIsSelected( pList, NULL, index );
	bool isHovering = ui_ListIsHovering( pList, NULL, index );
	UGCAssetLibraryRow* row = eaGet( pList->peaModel, index );
	ResourceInfo* info;
	void* pData;
	CBox previewBox;
	CBox textBox;

	if( row->astrType == allocAddString( "Trap" )) {
		int objId;
		char buffer[ RESOURCE_NAME_MAX_SIZE ];
		sscanf( row->pcName, "%d,%s", &objId, buffer );
		info = ugcResourceGetInfoInt( "ObjectLibrary", objId );
	} else {
		info = ugcResourceGetInfo( row->astrType, row->pcName );
	}

	if( !info ) {
		pData = ugcEditorGetObject( GET_REF(libPane->hTagType)->pcDictName, row->pcName );
	} else {
		pData = NULL;
	}

	// Draw background
	if( isHovering && mouseIsDown( MS_LEFT ) && libPane->pLibraryDragCB ) {
		ui_TextureAssemblyDraw( pressed, pBox, &previewBox, scale, z, z + 0.1, 255, NULL );
		ui_SetCursorByName( "UGC_Cursors_Move_Pointer" );
	} else if( isSelected ) {
		ui_TextureAssemblyDraw( selected, pBox, &previewBox, scale, z, z + 0.1, 255, NULL );
	} else if( isHovering && !mouseIsDown( MS_LEFT )) {
		ui_TextureAssemblyDraw( hover, pBox, &previewBox, scale, z, z + 0.1, 255, NULL );
	} else {
		ui_TextureAssemblyDraw( normal, pBox, &previewBox, scale, z, z + 0.1, 255, NULL );
	}

	// preview
	{
		bool previewDrawn = false;
		unsigned char alpha = 255;

		if( !nullStr( SAFE_MEMBER( row->pProperties, pchImageOverride ))) {
			AtlasTex* tex = atlasLoadTexture( row->pProperties->pchImageOverride );
			float texScale = CBoxWidth( &previewBox ) / tex->width;
			display_sprite( tex, previewBox.lx, previewBox.ly, z + 0.2, texScale, texScale, 0xFFFFFF00 | alpha );
			previewDrawn = true;
		} else {
			if( info ) {
				if(   info->resourceDict == allocAddString( "ObjectLibrary" )
					  || info->resourceDict == allocAddString( "PlayerCostume" )) {
					ResourceSnapDesc desc = { 0 };
					desc.astrDictName = info->resourceDict;
					desc.astrResName = info->resourceName;
					desc.objectIsTopDownView = false;
					display_sprite_box( atlasLoadTexture( gclSnapGetResourceString( &desc, true )), &previewBox, z + 0.2, 0xFFFFFF00 | alpha );
					previewDrawn = true;
				} else {					
					previewDrawn = resDrawPreview( info, NULL, previewBox.lx, previewBox.ly, CBoxWidth( &previewBox ), CBoxWidth( &previewBox ), scale, z + 0.2, alpha );
				}
			} else if( pData ) {
				previewDrawn = resDrawResource( GET_REF(libPane->hTagType)->pcDictName, pData, NULL, previewBox.lx, previewBox.ly, CBoxWidth( &previewBox ), CBoxWidth( &previewBox ), scale, z + 0.2, alpha );
			}
		}

		if( !previewDrawn ) {
			AtlasTex* tex = atlasLoadTexture( "CF_Icon_NoPreview" );
			float texScale = CBoxWidth( &previewBox ) / tex->width;
			display_sprite( tex, previewBox.lx, previewBox.ly, z + 0.2, texScale, texScale, 0xFFFFFF00 | alpha );
		}
	}

	ui_TextureAssemblyDraw( overlay, pBox, &textBox, scale, z + 0.3, z + 0.4, 255, NULL );
	if( (isHovering ? mouseIsDown( MS_LEFT ) : true) && !isSelected ) {
		ui_TextureAssemblyDraw( overlayIdle, pBox, NULL, scale, z + 0.3, z + 0.4, 255, NULL );
	}

	ui_StyleFontUse( RefSystem_ReferentFromString( "UIStyleFont", "UGC_Default_Alternate" ), false, 0 );
	{
		// Ensure no more than 2 lines of the name are diaplayed.
		F32 fLineHeight;
		F32 fLastLineWidth;
		char displayName[ 1024 ];
		S32 iLineCount;

		ugcAssetLibraryRowDisplayName( row, displayName );
		iLineCount = gfxfont_PrintWrappedEx( textBox.lx, 0, z + 0.5, CBoxWidth( &textBox ), 0, 0, 2, &fLastLineWidth, &fLineHeight, scale, scale, 0, false, displayName ) ;
		if( iLineCount > 1 ) { // 2 lines needed, start name higher by 1 line
			gfxfont_PrintWrappedEx( textBox.lx, textBox.hy - fLineHeight, z + 0.5, CBoxWidth( &textBox ), 0, 0, 2, &fLastLineWidth, &fLineHeight, scale, scale, 0, true, displayName );
		} else { // only 1 line needed, place it at bottom
			gfxfont_PrintWrapped( textBox.lx, textBox.hy, z + 0.5, w - 4, scale, scale, 0, true, displayName );
		}
	}
}

static void ugcAssetLibraryTextDrawCB( UIList* pList, UIListColumn* pCol, UI_MY_ARGS, F32 z, CBox* pBox, int index, UGCAssetLibraryPane* libPane )
{
	UITextureAssembly* selected = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Kits_Dropdown_Over" );
	bool isSelected = ui_ListIsSelected( pList, NULL, index );
	bool isHovering = ui_ListIsHovering( pList, NULL, index );
	UGCAssetLibraryRow* row = eaGet( pList->peaModel, index );
	UIStyleFont *pFont = ui_ListItemGetFontFromSkinAndWidget( UI_GET_SKIN( pList ), UI_WIDGET( pList ), isSelected, false );
	char displayName[ 1024 ];
	CBox box;

	ugcAssetLibraryRowDisplayName( row, displayName );
	BuildCBox( &box, x, y, w, h );

	if( isSelected ) {
		ui_TextureAssemblyDraw( selected, &box, NULL, scale, z + 0.1, z + 0.2, 255, NULL );
	}
	
	// Move over a few pixels so the text doesn't overlap with the arrow pointing to it.
	ui_StyleFontUse( pFont, isSelected, UI_WIDGET( pList )->state );
	gfxfont_PrintMaxWidth( x + 10, y + h / 2, z + 0.5, w - 10, scale, scale, CENTER_Y, displayName );
}

static void ugcAssetLibraryDragCallback( UIWidget* ignored, UGCAssetLibraryPane *lib_pane)
{
	UGCAssetLibraryRow *row_data = ui_ListGetSelectedObject( lib_pane->pLibraryList );
	if (row_data && lib_pane->pLibraryDragCB)
	{
		lib_pane->pLibraryDragCB(lib_pane, lib_pane->userdata, row_data);
	}
}

static void ugcAssetLibraryClickCallback(UIButton *button, UGCAssetLibraryPane *lib_pane)
{
	ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, true );
}

static void ugcAssetLibraryDoubleClickCallback(UIButton *button, UGCAssetLibraryPane *lib_pane)
{
	UGCAssetLibraryRow* row_data = ui_ListGetSelectedObject(lib_pane->pLibraryList);
	if (row_data && lib_pane->pLibraryDoubleClickCB)
	{
		lib_pane->pLibraryDoubleClickCB(lib_pane, lib_pane->userdata, row_data);
	}
}

static void ugcAssetLibraryPaneToggleExpanedFiltersCB( UIButton* ignored, UGCAssetLibraryPane* lib_pane )
{
	lib_pane->state.bExpandedFilters = !lib_pane->state.bExpandedFilters;
	ugcAssetLibraryPaneRefreshUI( lib_pane );
}

static void ugcAssetLibraryListTick( UIList* list, UI_PARENT_ARGS )
{
	UGCAssetLibraryPane* lib_pane = (UGCAssetLibraryPane*)list->widget.userinfo;
	float startYPos = list->widget.sb->ypos;
	
	if( lib_pane->eMode == UGCAssetLibrary_MapEditorEmbedded || lib_pane->eMode == UGCAssetLibrary_PlayingEditorEmbedded ) {
		if( ui_ListGetSelectedRow( list ) != -1 ) {
			if( !ui_IsFocused( list ) && lib_pane->pDetailsPane && !ui_IsFocusedOrChildren( lib_pane->pDetailsPane )) {
				ui_ListClearSelected( list );
				ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
			}
		}
	} else if( lib_pane->eMode == UGCAssetLibrary_NewMapWindow || lib_pane->eMode == UGCAssetLibrary_GenericWindow ) {
		if( ui_ListGetSelectedRow( list ) == -1 ) {
			ui_ListSetSelectedRow( list, 0 );
			ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
		}
		if( !ui_IsFocused( list ) && lib_pane->pDetailsPane && !ui_IsFocusedOrChildren( lib_pane->pDetailsPane )) {
			ui_WidgetQueueFreeAndNull( &lib_pane->pDetailsPane );
			ui_WidgetQueueFreeAndNull( &lib_pane->pDetailsSprite );
		}
	}

	ui_ListTick( list, UI_PARENT_VALUES );

	if( startYPos != list->widget.sb->ypos ) {
		if( lib_pane->eMode == UGCAssetLibrary_MapEditorEmbedded || lib_pane->eMode == UGCAssetLibrary_PlayingEditorEmbedded ) {
			ui_ListClearSelected( list );
			ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
		}
	}
}

static void ugcAssetLibraryPaneRefreshUI(UGCAssetLibraryPane *lib_pane)
{
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIList* list;
	UIListColumn* column;
	UIPane* pane;
	UITextureAssembly* texas;
	int max_label_size = 0;

	assert( lib_pane->pRootPane );
	lib_pane->pRootPane->invisible = true;

	lib_pane->bIgnoreChanges = true;
	MEContextPushExternalContext( lib_pane->pRootContext, NULL, &lib_pane->state, parse_UGCAssetLibraryPaneState );
	MEContextSetParent( UI_WIDGET( lib_pane->pRootPane ));

	pane = MEContextPushPaneParent( "FiltersPane" );
	{
		if( lib_pane->pHeaderWidget ) {
			MEContextGetCurrent()->iYPos = lib_pane->pHeaderWidget->height / 2;
		}
		
		entry = MEContextAddTextMsg( false, "UGC_AssetLibrary.TextFilter", "TextFilter", NULL, NULL );
		widget = ENTRY_FIELD( entry )->pUIWidget;
		SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTextEntry_Search", widget->hOverrideSkin );
		ui_WidgetSetPosition( widget, 0, widget->y );
		ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
		MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );

		eaClear( &lib_pane->eaTagTypeFilterLists );
		if( GET_REF(lib_pane->hTagType) ) {
			FOR_EACH_IN_EARRAY_FORWARDS(GET_REF(lib_pane->hTagType)->eaCategories, UGCAssetTagCategory, category)
			{
				int it = FOR_EACH_IDX( GET_REF(lib_pane->hTagType)->eaCategories, category );
				bool skipCategory = false;
		
				FOR_EACH_IN_EARRAY_FORWARDS(lib_pane->eaExtraFitlerTags, const char, tag)
				{
					char tagScratch[ 256 ];
					char* underscorePos;
					char* categoryName;
					char* tagName;
			
					strcpy( tagScratch, tag );
					underscorePos = strchr( tagScratch, '_' );
					if( underscorePos ) {
						*underscorePos = '\0';
						categoryName = tagScratch;
						tagName = underscorePos + 1;
					} else {
						categoryName = NULL;
						tagName = NULL;
					}
			
					if( categoryName && tagName && stricmp( categoryName, category->pcName ) == 0 ) {
						skipCategory = true;
						break;
					}
				}
				FOR_EACH_END;
		
				if (!skipCategory && !category->bIsHidden && eaSize(&category->eaTags) > 0)
				{
					UGCAssetLibraryTagTypeListData* listData = MEContextAllocStructIndex( "CategoryList", it, parse_UGCAssetLibraryTagTypeListData, true );

					// Refresh the list data
					{
						// Add an "Any" tag
						UGCAssetLibraryTagTypeListRow* anyRow = StructCreate( parse_UGCAssetLibraryTagTypeListRow );
						SET_HANDLE_FROM_STRING( gMessageDict, "UGC_AssetLibrary.AnyTag", anyRow->hDisplayName );
						eaPush( &listData->eaRows, anyRow );

						// Add a row for each list entry
						FOR_EACH_IN_EARRAY_FORWARDS( category->eaTags, UGCAssetTag, tag ) {
							UGCAssetLibraryTagTypeListRow* tagRow = StructCreate( parse_UGCAssetLibraryTagTypeListRow );
							char buffer[ 256 ];
							COPY_HANDLE( tagRow->hDisplayName, tag->hDisplayName );
							sprintf( buffer, "%s_%s", category->pcName, tag->pcName );
							tagRow->strTag = StructAllocString( buffer );
							eaPush( &listData->eaRows, tagRow );
						} FOR_EACH_END;
					}

					entry = MEContextAddCustomIndex( "CategoryList", it );
					list = (UIList*)ENTRY_WIDGET( entry );
					if( !list ) {
						list = ui_ListCreate( NULL, NULL, 1 );
						ENTRY_WIDGET( entry ) = UI_WIDGET( list );
					}
					widget = UI_WIDGET( list );
					ui_ListDestroyColumns( list );
					column = ui_ListColumnCreateMsg( UIListPTMessage, REF_STRING_FROM_HANDLE( category->hDisplayName ), (intptr_t)"DisplayName", NULL );
					ui_ListColumnSetWidth( column, false, 1 );
					ui_ListColumnSetAlignment( column, UINoDirection, UILeft );
					column->bShowCheckBox = true;
					ui_ListAppendColumn( list, column );
					ui_ListSetModel( list, parse_UGCAssetLibraryTagTypeListRow, &listData->eaRows );
					list->fRowHeight = 22;
					list->bMultiSelect = true;
					list->bToggleSelect = true;
					list->fHeaderHeight = 20;
					ui_ListSetSelectedCallback( list, ugcAssetLibraryTagFilterSelected, lib_pane );
					ui_ListSetCellClickedCallback( list, ugcAssetLibraryTagFilterCellClicked, NULL );
					ui_ListSetCellActivatedCallback( list, ugcAssetLibraryTagFilterCellActivated, NULL );
					ui_WidgetSetPosition( UI_WIDGET( list ), 0, MEContextGetCurrent()->iYPos + 10 );
					ui_WidgetSetDimensionsEx( UI_WIDGET( list ), 1, 1, UIUnitFixed, UIUnitFixed );
					ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, widget );
					eaPush( &lib_pane->eaTagTypeFilterLists, list );

					// Select the "any" tab, if nothing else is selected
					if( !ui_ListIsAnyObjectSelected( list )) {
						ui_ListSetSelectedRow( list, 0 );
					}
				}
			} FOR_EACH_END;
		}

		// Fixup list placement, make it percentage
		if( eaSize( &lib_pane->eaTagTypeFilterLists ) > 0 ) {
			int numFilterLists = eaSize( &lib_pane->eaTagTypeFilterLists );
			float listWidth = 1.0f / numFilterLists;

			FOR_EACH_IN_EARRAY_FORWARDS( lib_pane->eaTagTypeFilterLists, UIList, filterList ) {
				int it = FOR_EACH_IDX( lib_pane->eaTagTypeFilterLists, filterList );
				widget = UI_WIDGET( filterList );
				ui_WidgetSetPositionEx( widget, 0, widget->y, listWidth * it, 0, UITopLeft );
				if( lib_pane->state.bExpandedFilters ) {
					ui_WidgetSetDimensionsEx( widget, listWidth, 1, UIUnitPercentage, UIUnitPercentage );
				} else {
					ui_WidgetSetDimensionsEx( widget, listWidth, 0, UIUnitPercentage, UIUnitFixed );
				}
				ui_WidgetSetPaddingEx( widget, (it > 0 ? 6 : 0), (it < eaSize( &lib_pane->eaTagTypeFilterLists ) - 1 ? 6 : 0), 0, 0 );
			} FOR_EACH_END;

			if( lib_pane->state.bExpandedFilters ) {
				MEContextAddCustomSpacer( 120 );
			}
		}
	}
	texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_Header_Box" );
	ui_PaneSetStyle( pane, texas->pchName, true, false );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, MEContextGetCurrent()->iYPos + ui_TextureAssemblyHeight( texas ), UIUnitPercentage, UIUnitFixed );
	MEContextPop( "FiltersPane" );

	if( lib_pane->pHeaderWidget ) {
		ui_WidgetSetPosition( UI_WIDGET( pane ), 0, lib_pane->pHeaderWidget->height / 2 );

		ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, lib_pane->pHeaderWidget );
		ui_WidgetSetPositionEx( lib_pane->pHeaderWidget, 0, 0, 0, 0, UITop );
	} else {
		ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
	}
	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( UI_WIDGET( pane ));

	if(   eaSize( &lib_pane->eaTagTypeFilterLists ) > 0
		  && (lib_pane->eMode == UGCAssetLibrary_MapEditorEmbedded || lib_pane->eMode == UGCAssetLibrary_PlayingEditorEmbedded) ) {
		if( lib_pane->state.bExpandedFilters ) {
			entry = MEContextAddButtonMsg( "UGC_AssetLibrary.Collapse", NULL, ugcAssetLibraryPaneToggleExpanedFiltersCB, lib_pane, "FiltersExpandCollapseButton", NULL, "UGC_AssetLibrary.Collapse.Tooltip" );
		} else {
			entry = MEContextAddButtonMsg( "UGC_AssetLibrary.Expand", NULL, ugcAssetLibraryPaneToggleExpanedFiltersCB, lib_pane, "FiltersExpandCollapseButton", NULL, "UGC_AssetLibrary.Expand.Tooltip" );
		}
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		SET_HANDLE_FROM_STRING( g_hUISkinDict, lib_pane->state.bExpandedFilters ? "UGCButton_Collapser" : "UGCButton_Expander", widget->hOverrideSkin );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, widget->y - 1, 0, 0, UITop );
		MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget ) + 5;
	} else {
		MEContextGetCurrent()->iYPos += 10;
	}

	entry = MEContextAddCustom( "Library" );
	list = (UIList*)ENTRY_WIDGET( entry );
	if( !list ) {
		list = ui_ListCreate( NULL, NULL, 1 );
		ENTRY_WIDGET( entry ) = UI_WIDGET( list );
	}
	list->bDrawSelection = false;
	ui_ListDestroyColumns( list );
	if(   GET_REF( lib_pane->hTagType ) && GET_REF( lib_pane->hTagType )->bEnableIconGridView ) {
		ui_ListAddGridIconColumn( list, ui_ListColumnCreateCallback( NULL, ugcAssetLibraryIconGridDrawCB, lib_pane ));
		list->eDisplayMode = UIListIconGrid;
	} else {
		ui_ListAppendColumn( list, ui_ListColumnCreateCallback( NULL, ugcAssetLibraryTextDrawCB, lib_pane ));
		list->eDisplayMode = UIListRows;
	}
	ui_ListSetModel( list, parse_UGCAssetLibraryRow, &lib_pane->eaResults );
	list->fGridCellWidth = 80;
	list->fGridCellHeight = 90;
	list->fRowHeight = 20;
	list->widget.tickF = ugcAssetLibraryListTick;
	list->widget.userinfo = lib_pane;

	ui_ListSetSelectedCallback( list, ugcAssetLibraryClickCallback, lib_pane );
	ui_WidgetSetDragCallback( UI_WIDGET( list ), ugcAssetLibraryDragCallback, lib_pane );
	ui_ListSetActivatedCallback( list, ugcAssetLibraryDoubleClickCallback, lib_pane );
	ui_WidgetSetTextMessage( UI_WIDGET( list ), "UGC_AssetLibrary.NoAssetsFound" );

	ui_WidgetSetPosition( UI_WIDGET( list ), 0, MEContextGetCurrent()->iYPos );
	ui_WidgetSetDimensionsEx( UI_WIDGET( list ), 1.0f, 1.0f, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetGroupMove( &lib_pane->pRootPane->widget.children, UI_WIDGET( list ));
	lib_pane->pLibraryList = list;
	
	MEContextPopExternalContext( lib_pane->pRootContext );
	lib_pane->bIgnoreChanges = false;

	ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
}

/// Common library panel

UIPane *ugcAssetLibraryPaneGetUIPane(UGCAssetLibraryPane *lib_pane)
{
	return lib_pane->pRootPane;
}

void ugcAssetLibraryPaneSetTagTypeName(UGCAssetLibraryPane* lib_pane, const char* tag_type_name)
{
	UGCAssetTagType* tag_type = RefSystem_ReferentFromString("TagType", tag_type_name);
	assertmsgf( tag_type, "TagType referenced in code is missing from project: %s!", tag_type_name );
	if( tag_type != GET_REF( lib_pane->hTagType )) {
		SET_HANDLE_FROM_STRING( "TagType", tag_type_name, lib_pane->hTagType );
		ugcAssetLibraryPaneRefreshUI(lib_pane);
		ugcAssetLibraryPaneRefreshLibraryModel(lib_pane);
	}
}

const char* ugcAssetLibraryPaneGetTagTypeName(UGCAssetLibraryPane *lib_pane)
{
	return REF_STRING_FROM_HANDLE(lib_pane->hTagType);
}

UGCAssetLibraryRow *ugcAssetLibraryPaneGetSelected(UGCAssetLibraryPane *lib_pane)
{
	return ui_ListGetSelectedObject(lib_pane->pLibraryList);
}

void ugcAssetLibraryPaneSetSelected(UGCAssetLibraryPane *lib_pane, const char *object_name)
{
	if( !nullStr( object_name )) {
		FOR_EACH_IN_EARRAY(lib_pane->eaResults, UGCAssetLibraryRow, row)
		{
			if (stricmp(row->pcName, object_name) == 0)
			{
				ui_ListSetSelectedRow(lib_pane->pLibraryList, FOR_EACH_IDX(lib_pane->eaResults, row));
				ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
				return;
			}
		}
		FOR_EACH_END;
	}

	ui_ListClearSelected(lib_pane->pLibraryList);
	ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
}

void ugcAssetLibraryPaneClearSelected(UGCAssetLibraryPane *lib_pane)
{
	ugcAssetLibraryPaneSetSelected(lib_pane, NULL);
}

void ugcAssetLibraryPaneSetHeaderWidget(UGCAssetLibraryPane* lib_pane, UIWidget* headerWidget )
{
	// Since this gets centered in the UI, this needs to be absolute sized
	assert( headerWidget->widthUnit == UIUnitFixed && headerWidget->heightUnit == UIUnitFixed );

	lib_pane->pHeaderWidget = headerWidget;
	ugcAssetLibraryPaneRefreshUI( lib_pane );
}

bool ugcAssetLibraryPickerWindowOpen( void )
{
	return ugcAssetLibraryPickerWindowsOpen > 0;
}

static int ugcAssetTagHasName(const void* rawAssetTag, const void* rawName)
{
	const UGCAssetTag* assetTag = rawAssetTag;
	const char* name = rawName;

	return stricmp( assetTag->pcName, name ) == 0;
}


static void ugcAssetLibraryPaneMEFieldChangedCB( MEField *pField, bool bFinished, UGCAssetLibraryPane* lib_pane )
{
	if( lib_pane->bIgnoreChanges ) {
		return;
	}
	
	if( bFinished ) {
		ugcAssetLibraryPaneRefreshLibraryModel( lib_pane );
	} else {
		ugcAssetLibraryPaneRefreshLibraryModelFiltered( lib_pane );
	}
}

UGCAssetLibraryPane* ugcAssetLibraryPaneCreate(UGCAssetLibraryMode eMode, bool bUseProjectData, UGCAssetLibrarySelectFn pLibraryDragCB, UGCAssetLibrarySelectFn pLibraryDoubleClickCB, UserData userdata )
{
	UGCAssetLibraryPane* lib_pane = calloc(1, sizeof(UGCAssetLibraryPane));

	lib_pane->eMode = eMode;
	lib_pane->bUseProjectData = bUseProjectData;
	lib_pane->pLibraryDragCB = pLibraryDragCB;
	lib_pane->pLibraryDoubleClickCB = pLibraryDoubleClickCB;
	lib_pane->userdata = userdata;

	lib_pane->eaResultsNotIncludingTextFilter = NULL;
	lib_pane->eaResults = NULL;

	lib_pane->eExtraFilterMapType = UGC_MAP_TYPE_ANY;
	lib_pane->state.bExpandedFilters = true;

	lib_pane->pRootPane = ui_PaneCreate(0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0);
	lib_pane->pRootContext = MEContextCreateExternalContext( "AssetLibrary" );
	lib_pane->pRootContext->cbChanged = ugcAssetLibraryPaneMEFieldChangedCB;
	lib_pane->pRootContext->pChangedData = lib_pane;

	ugcAssetLibraryPaneRefreshUI(lib_pane);
	ugcAssetLibraryPaneRefreshLibraryModel(lib_pane);
	
	return lib_pane;
}

UGCAssetLibraryPane *ugcAssetLibraryPaneCreateLegacy(UserData userdata, bool bUseProjectData, const char *tag_type_name, const char* default_value, const char** default_tags, const char** force_tags, UGCAssetLibrarySelectFn pLibraryDragCB, UGCAssetLibrarySelectFn pLibraryDoubleClickCB, bool bEmbeddedMode_IGNORED)
{
	UGCAssetLibraryPane* lib_pane = ugcAssetLibraryPaneCreate( UGCAssetLibrary_Legacy, bUseProjectData, pLibraryDragCB, pLibraryDoubleClickCB, userdata );
	ugcAssetLibraryPaneSetTagTypeName( lib_pane, tag_type_name );
	eaCopyEx( &(char**)force_tags, &lib_pane->eaExtraFitlerTags, strdupFunc, strFreeFunc );

	ugcAssetLibraryPaneRefreshUI(lib_pane);
	ugcAssetLibraryPaneRefreshLibraryModel(lib_pane);

	// Set the selected object
	lib_pane->pLibraryList->bScrollToCenter = true;
	if( default_value ) {
		int it;
		for( it = 0; it != eaSize( &lib_pane->eaResults ); ++it ) {
			if( stricmp( lib_pane->eaResults[ it ]->pcName, default_value ) == 0 ) {
				ui_ListSetSelectedRow( lib_pane->pLibraryList, it );
				ui_ListScrollToRow( lib_pane->pLibraryList, it );
				ugcAssetLibraryPaneRefreshDetailsUI( lib_pane, false );
				break;
			}
		}
	} else {
		ui_ListScrollToRow( lib_pane->pLibraryList, 0 );
	}

	return lib_pane;
}

void ugcAssetLibraryPaneDestroy( UGCAssetLibraryPane* pPane )
{
	if( pPane ) {
		// Clear models so that the widgets can safely continue this
		// tick (prevents a crash)
		static void** nullModel = NULL;
		if( pPane->pLibraryList ) {
			ui_ListSetModel( pPane->pLibraryList, NULL, &nullModel );
		}
		REMOVE_HANDLE( pPane->hTagType );

		eaDestroy( &pPane->eaTagTypeFilterLists );
		eaDestroyStruct( &pPane->eaResultsNotIncludingTextFilter, parse_UGCAssetLibraryRow );
		eaDestroy( &pPane->eaResults );
		eaDestroyEx( &pPane->eaExtraFitlerTags, NULL );
		ui_WidgetQueueFreeAndNull( &pPane->pDetailsPane );
		ui_WidgetQueueFreeAndNull( &pPane->pDetailsSprite );
		MEContextDestroyExternalContext( pPane->pRootContext );
		if( pPane->pHeaderWidget ) {
			ui_WidgetRemoveFromGroup( pPane->pHeaderWidget );
		}
		ui_WidgetQueueFreeAndNull( &pPane->pRootPane );
		StructReset( parse_UGCAssetLibraryPaneState, &pPane->state );
	
		SAFE_FREE( pPane );
	}
}

void ugcAssetLibraryPaneRestrictMapType(UGCAssetLibraryPane *lib_pane, UGCMapType map_type)
{
	if( lib_pane->eExtraFilterMapType != map_type ) {
		lib_pane->eExtraFilterMapType = map_type;
		ugcAssetLibraryPaneRefreshLibraryModel(lib_pane);
	}
}

/// Library as a window

typedef struct UGCAssetLibraryWindow {
	UIWindow* window;

	UITabGroup* tabGroup;
	UITab* mapTab;
	UIWidget* mapWidget;

	UITab* projMapTab;
	UIWidget* projMapWidget;
	
	UITab* listTab;
	UGCAssetLibraryPane* lib_pane;

	UITab* petTab;
	UGCAssetLibraryPane* pet_pane;

	UITab* attribTab;
	UGCAssetLibraryPane* attribPane;

	UGCAssetLibrarySelectFn callback;
	UGCAssetLibraryMapSelectFn mapCallback;
	UGCAssetLibrarySelectFn petCallback;
	UserData userdata;
} UGCAssetLibraryWindow;

static void ugcAssetLibraryWindowDestroy( UIWidget* ignored, UGCAssetLibraryWindow *data)
{
	ugcAssetLibraryPaneDestroy( data->lib_pane );
	ugcAssetLibraryPaneDestroy( data->pet_pane );
	ugcAssetLibraryPaneDestroy( data->attribPane );
	
	ui_WidgetQueueFree( UI_WIDGET( data->window ));
	free( data );
	--ugcAssetLibraryPickerWindowsOpen;
	assert( ugcAssetLibraryPickerWindowsOpen >= 0 );
}

static void ugcAssetLibraryApplyTagWindow(UGCAssetLibraryPane *lib_pane, UGCAssetLibraryWindow* data, UGCAssetLibraryRow *result)
{
	if(data->callback)
		data->callback(lib_pane, data->userdata, result);
	ugcAssetLibraryWindowDestroy(NULL, data);
}

static void ugcAssetLibraryApplyMapWindow(const char* mapName, const char* logicalName, UGCAssetLibraryWindow* data)
{
	if(data->mapCallback) {
		data->mapCallback(mapName, logicalName, data->userdata);
	}
	ugcAssetLibraryWindowDestroy(NULL, data);
}

static void ugcAssetLibraryApplyPetWindow(UGCAssetLibraryPane *pet_pane, UGCAssetLibraryWindow* data, UGCAssetLibraryRow *result)
{
	if(data->petCallback)
		data->petCallback(pet_pane, data->userdata, result);
	ugcAssetLibraryWindowDestroy(NULL, data);
}

static void ugcAssetLibraryApplyCheckedAttribWindow(UGCAssetLibraryPane *attrib_pane, UGCAssetLibraryWindow* data, UGCAssetLibraryRow *result)
{
	if(data->callback)
		data->callback(attrib_pane, data->userdata, result);
	ugcAssetLibraryWindowDestroy(NULL, data);
}

static void ugcAssetLibraryDoApplyTagWindow(UIButton *button, UGCAssetLibraryWindow* data)
{
	UITab* selectedTab = data->tabGroup ? ui_TabGroupGetActive( data->tabGroup ) : NULL;
	if( selectedTab == data->listTab ) {
		UGCAssetLibraryRow* row_data = ugcAssetLibraryPaneGetSelected( data->lib_pane );
		if (row_data) {
			ugcAssetLibraryApplyTagWindow(data->lib_pane, data, row_data);
		} else {
			ugcAssetLibraryWindowDestroy(NULL, data);
		}
	} else if( selectedTab == data->mapTab ) {
		const char* mapName = NULL;
		const char* logicalName = NULL;
		ugcZeniPickerWidgetGetSelection( data->mapWidget, &mapName, &logicalName );

		if( mapName && logicalName ) {
			ugcAssetLibraryApplyMapWindow(mapName, logicalName, data);
		} else {
			ugcAssetLibraryWindowDestroy(NULL, data);
		}
	} else if( selectedTab == data->projMapTab ) {
		const char* mapName = NULL;
		const char* logicalName = NULL;
		ugcZeniPickerWidgetGetSelection( data->projMapWidget, &mapName, &logicalName );

		if( mapName && logicalName ) {
			ugcAssetLibraryApplyMapWindow(mapName, logicalName, data);
		} else {
			ugcAssetLibraryWindowDestroy(NULL, data);
		}
	} else if( selectedTab == data->petTab ) {
		UGCAssetLibraryRow* row_data = ugcAssetLibraryPaneGetSelected( data->pet_pane );
		if(row_data) {
			ugcAssetLibraryApplyPetWindow(data->pet_pane, data, row_data);
		} else {
			ugcAssetLibraryWindowDestroy(NULL, data);
		}
	} else if( selectedTab == data->attribTab ) {
		UGCAssetLibraryRow* row_data = ugcAssetLibraryPaneGetSelected( data->attribPane );
		if(row_data) {
			ugcAssetLibraryApplyCheckedAttribWindow(data->attribPane, data, row_data);
		} else {
			ugcAssetLibraryWindowDestroy(NULL, data);
		}
	} else {
		ugcAssetLibraryWindowDestroy(NULL, data);
	}
}

static bool ugcAssetLibraryWindowClose(UIWidget* ignored, UGCAssetLibraryWindow *data)
{
	ugcAssetLibraryWindowDestroy(ignored, data);
	return true;
}

UGCAssetLibraryPane *ugcAssetLibraryShowPicker(UserData userdata, bool bUseProjectData, const char *title, const char* note, const char *tag_type_name, const char* default_value, UGCAssetLibrarySelectFn callback)
{
	UGCAssetLibraryWindow* data = calloc( 1, sizeof( *data ));
	UIPane* pane;
	UIButton *button;
	UILabel* label;
	int x;
	int y;

	// Assert to help track down [COR-10415] -- Please talk to Jared before removing.
	assert( userdata );

	data->window = ui_WindowCreate(title, 0, 0, 500, 500);
	ui_WindowSetCloseCallback(data->window, ugcAssetLibraryWindowClose, data);

	data->lib_pane = ugcAssetLibraryPaneCreate( UGCAssetLibrary_GenericWindow, bUseProjectData, NULL, ugcAssetLibraryApplyTagWindow, data );
	ugcAssetLibraryPaneSetTagTypeName( data->lib_pane, tag_type_name );
	pane = ugcAssetLibraryPaneGetUIPane( data->lib_pane );
	ui_WindowAddChild( data->window, pane);

	x = 0;
	y = 0;
	button = ui_ButtonCreate("", 0, 0, ugcAssetLibraryDoApplyTagWindow, data);
	ui_WidgetSetTextMessage( UI_WIDGET( button ), "UGC.Ok" );
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WidgetSetWidth(UI_WIDGET(button), 80);
	ui_WindowAddChild(data->window, button);

	x = ui_WidgetGetNextX( UI_WIDGET( button ));
	button = ui_ButtonCreate("", 0, 0, ugcAssetLibraryWindowDestroy, data);
	ui_WidgetSetTextMessage( UI_WIDGET( button ), "UGC.Cancel" );
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WindowAddChild(data->window, button);
	ui_WidgetSetWidth(UI_WIDGET(button), 80);
	y += button->widget.height;

	if( note ) {
		label = ui_LabelCreate( note, 0, y );
		ui_LabelSetWordWrap( label, true );
		ui_LabelUpdateDimensionsForWidth( label, UGC_LIBRARY_PANE_WIDTH );
		ui_WidgetSetPositionEx( UI_WIDGET( label ), 0, y, 0, 0, UIBottomLeft );
		ui_WindowAddChild( data->window, label );
		y += label->widget.height + 10;
	}

	UI_WIDGET(pane)->bottomPad = y + 5;

	{
		int width = g_ui_State.screenWidth;
		int height = g_ui_State.screenHeight;
		ui_WidgetSetDimensions( UI_WIDGET( data->window ), UGC_LIBRARY_PANE_WIDTH + 10, 610 );
		ui_WidgetSetPosition( UI_WIDGET( data->window ), width / 2 + 100, (height - UI_WIDGET( data->window )->height) / 2 );
	}
	ui_WindowSetModal(data->window, true);
	ui_WindowShowEx(data->window, true);
	ui_WindowSetMovable(data->window, false );

	data->callback = callback;
	data->userdata = userdata;

	++ugcAssetLibraryPickerWindowsOpen;
	return data->lib_pane;
}

void ugcAssetLibraryShowCostumePicker(UserData userdata, bool bUseProjectData, const char *title, const char* default_costume, const char* default_pet, UGCAssetLibrarySelectFn callback, UGCAssetLibraryMapSelectFn mapCallback, UGCAssetLibrarySelectFn petCallback)
{
	UGCAssetLibraryWindow* data = calloc( 1, sizeof( *data ));
	UIButton *button;
	int x, w;

	// Assert to help track down [COR-10415] -- Please talk to Jared before removing.
	assert( userdata );

	data->window = ui_WindowCreate(title, 0, 0, 500, 500);
	
	data->tabGroup = ui_TabGroupCreate(0, 0, 1, 1);
	ui_WidgetSetDimensionsEx(UI_WIDGET(data->tabGroup), 1, 1, UIUnitPercentage, UIUnitPercentage);
	ui_WindowAddChild(data->window, data->tabGroup);

	data->listTab = ui_TabCreate( "Asset View" );
	ui_TabGroupAddTab(data->tabGroup, data->listTab);
	data->lib_pane = ugcAssetLibraryPaneCreateLegacy(data, bUseProjectData, "Costume", default_costume,
											   NULL, NULL, NULL, ugcAssetLibraryApplyTagWindow, false);
	ui_TabAddChild(data->listTab, ugcAssetLibraryPaneGetUIPane(data->lib_pane));

	data->mapTab = ui_TabCreate( "Cryptic Maps" );
	ui_TabGroupAddTab(data->tabGroup, data->mapTab);
	data->mapWidget = ugcZeniPickerWidgetCreate( NULL, NULL, UGCZeniPickerType_Contact, NULL, NULL, ugcEditorEncObjFilter, NULL );
	assert(data->mapWidget);
	ui_TabAddChild(data->mapTab, data->mapWidget);

	data->petTab = ui_TabCreate( "Bridge Officers" );
	ui_TabGroupAddTab(data->tabGroup, data->petTab);
	data->pet_pane = ugcAssetLibraryPaneCreateLegacy(data, bUseProjectData, "PetContactList", default_pet, NULL, NULL, NULL, ugcAssetLibraryApplyPetWindow, false);
	ui_TabAddChild(data->petTab, ugcAssetLibraryPaneGetUIPane(data->pet_pane));

	if( default_costume ) {
		ui_TabGroupSetActiveIndex( data->tabGroup, 0 );
	} else if( default_pet ) {
		ui_TabGroupSetActiveIndex( data->tabGroup, 2 );
	} else {
		ui_TabGroupSetActiveIndex( data->tabGroup, 0 );
	}

	button = ui_ButtonCreate("Cancel", 0, 0, ugcAssetLibraryWindowDestroy, data);
	ui_WidgetSetPositionEx(UI_WIDGET(button), 5, 0, 0, 0, UIBottomRight);
	ui_WindowAddChild(data->window, button);
	w = button->widget.width*1.1f;
	x = w + 10;
	ui_WidgetSetWidth(UI_WIDGET(button), w);
	data->tabGroup->widget.bottomPad = button->widget.height + 5;

	button = ui_ButtonCreate("OK", 0, 0, ugcAssetLibraryDoApplyTagWindow, data);
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WidgetSetWidth(UI_WIDGET(button), w);
	ui_WindowAddChild(data->window, button);

	ui_WindowSetDimensions(data->window, 900, 700, 450, 350);
	elUICenterWindow(data->window);
	ui_WindowSetModal(data->window, true);
	ui_WindowShowEx(data->window, true);

	data->callback = callback;
	data->mapCallback = mapCallback;
	data->petCallback = petCallback;
	data->userdata = userdata;
	
	++ugcAssetLibraryPickerWindowsOpen;
}

void ugcAssetLibraryShowCheckedAttribPicker(UserData userdata, bool bUseProjectData, bool allowSkills, bool allowItems, const char *title, UGCAssetLibrarySelectFn callback)
{
	UGCAssetLibraryWindow* data = calloc( 1, sizeof( *data ));
	UIButton *button;
	int x;

	assert( allowSkills || allowItems );
	assert( userdata );

	data->window = ui_WindowCreate( title, 0, 0, 500, 500 );
	
	data->tabGroup = ui_TabGroupCreate( 0, 0, 1, 1 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( data->tabGroup ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WindowAddChild( data->window, data->tabGroup );

	if( allowItems ) {
		data->listTab = ui_TabCreate( "" );
		ui_TabSetTitleMessage( data->listTab, "UGC_Editor.CheckedAttrib_Items" );
		ui_TabGroupAddTab( data->tabGroup, data->listTab );
		data->lib_pane = ugcAssetLibraryPaneCreateLegacy( data, bUseProjectData, "MissionItem", NULL, NULL, NULL, NULL, ugcAssetLibraryApplyTagWindow, false );
		ui_TabAddChild( data->listTab, ugcAssetLibraryPaneGetUIPane( data->lib_pane ));
	}

	if( allowSkills ) {
		data->attribTab = ui_TabCreate( "" );
		ui_TabSetTitleMessage( data->attribTab, "UGC_Editor.CheckedAttrib_Skills" );
		ui_TabGroupAddTab( data->tabGroup, data->attribTab );
		data->attribPane = ugcAssetLibraryPaneCreateLegacy( data, bUseProjectData, "CheckedAttrib", NULL, NULL, NULL, NULL, ugcAssetLibraryApplyCheckedAttribWindow, false );
		ui_TabAddChild( data->attribTab, ugcAssetLibraryPaneGetUIPane( data->attribPane ));
	}

	ui_TabGroupSetActiveIndex( data->tabGroup, 0 );

	x = 0;
	button = ui_ButtonCreate("", 0, 0, ugcAssetLibraryDoApplyTagWindow, data);
	ui_WidgetSetTextMessage( UI_WIDGET( button ), "UGC.Ok" );
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WidgetSetWidth(UI_WIDGET(button), 80);
	ui_WindowAddChild(data->window, button);
	x = ui_WidgetGetNextX( UI_WIDGET( button ));
	
	button = ui_ButtonCreate("", 0, 0, ugcAssetLibraryWindowDestroy, data);
	ui_WidgetSetTextMessage( UI_WIDGET( button ), "UGC.Cancel" );
	ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
	ui_WindowAddChild(data->window, button);
	ui_WidgetSetWidth(UI_WIDGET(button), 80);
	data->tabGroup->widget.bottomPad = button->widget.height + 5;

	ui_WindowSetDimensions(data->window, 500, 500, 500, 500);
	elUICenterWindow(data->window);
	ui_WindowSetModal(data->window, true);
	ui_WindowShowEx(data->window, true);

	data->callback = callback;
	data->userdata = userdata;
	++ugcAssetLibraryPickerWindowsOpen;
}

#include "NNOUGCAssetLibrary_h_ast.c"
#include "NNOUGCAssetLibrary_c_ast.c"

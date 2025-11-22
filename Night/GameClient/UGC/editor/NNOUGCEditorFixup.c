#include "Color.h"
#include "ObjectLibrary.h"
#include "ResourceInfo.h"
#include "StringUtil.h"
#include "WorldGrid.h"
#include "../WorldLib/StaticWorld/WorldGridPrivate.h"

#include "NNOUGCCommon.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCInteriorCommon.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "wlUGC.h"
#include "UGCError.h"
#include "NNOUGCModalDialog.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

/// Fixup function called after every change is made in any UGC
/// editor.
///
/// Cleanup transient components and other data that is safe to delete
/// at any time here.
bool ugcEditorFixupPostEdit(bool query_delete)
{
	int numDialogsDeleted = 0;
	int numCostumesReset = 0;
	int numObjectivesReset = 0;
	UGCProjectData* ugcProj = ugcEditorGetProjectData();

	// Fixup project language
	{
		if( ugcProj->project->eLanguage == LANGUAGE_DEFAULT ) {
			ugcProj->project->eLanguage = locGetLanguage( getCurrentLocale() );
		}
	}

	ugcLoadStart_printf( "Fixup..." );
	ugcEditorFixupProjectData(ugcProj, &numDialogsDeleted, &numCostumesReset, &numObjectivesReset, NULL);
	ugcLoadEnd_printf( "done." );

	if (query_delete)
	{
		UGCBudgetValidateState lastEditBudgetState;
		UGCBudgetValidateState budgetState;
		UGCRuntimeStatus* status = StructCreate( parse_UGCRuntimeStatus );

		ugcSetStageAndAdd( status, "UGC Budget Validate" );
		budgetState = ugcValidateBudgets( ugcProj );
		lastEditBudgetState = ugcValidateBudgets( ugcEditorGetLastSaveData() );
		StructDestroySafe( parse_UGCRuntimeStatus, &status );
		ugcClearStage();

		if( budgetState == UGC_BUDGET_HARD_LIMIT ) {
			if( lastEditBudgetState == UGC_BUDGET_HARD_LIMIT ) {
				ugcModalDialogMsg( "UGC_Editor.BudgetWarning", "UGC_Editor.BudgetWarningDetails", UIYes );
			} else {
				ugcModalDialogMsg( "UGC_Editor.CriticalBudgetWarning", "UGC_Editor.CriticalBudgetWarningDetails", UIYes );

				return false;
			}
		}
		
		if( numDialogsDeleted > 0 ) {
			UIDialogButtons result;
			char buffer[ 256 ];

			sprintf( buffer, "This will require deleting %d dialogs.  Continue anyway?", numDialogsDeleted );
			result = ugcModalDialog( "Continue Anyway?", buffer, UIYes | UINo );
			if( result != UIYes ) {
				return false;
			}
		}
		if( numCostumesReset > 0 ) {
			UIDialogButtons result;
			char buffer[ 256 ];

			sprintf( buffer, "This costume is in use in %d places in the story and on the maps.  Continue anyway?", numCostumesReset );
			result = ugcModalDialog( "Continue Anyway?", buffer, UIYes | UINo );
			if( result != UIYes ) {
				return false;
			}
		}
		if( numObjectivesReset > 0 ) {
			UIDialogButtons result;
			char buffer[ 256 ];

			sprintf( buffer, "This will requires breaking the link between an objective and its component %d times.  Continue anyway?", numObjectivesReset );
			result = ugcModalDialog( "Continue Anyway?", buffer, UIYes | UINo );
			if( result != UIYes ) {
				return false;
			}
		}
	}

	// Fixup deprecation
	{
		int numExternalComponnetsDeprecated = ugcProjectFixupDeprecated( ugcProj, false );
		if( numExternalComponnetsDeprecated ) {
			UIDialogButtons result;
			char buffer[ 256 ];

			sprintf( buffer, "This project has %d deprecated resources.  They will be updated to a new resource.  Please verify you project behaves as expected.", numExternalComponnetsDeprecated );
			result = ugcModalDialog( "Warning", buffer, UIOk );
			ugcProjectFixupDeprecated( ugcProj, true );
		}
	}

	return true;
}

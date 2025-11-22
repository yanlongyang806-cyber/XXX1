//// Game-specific UI control for the UGC project chooser.
#pragma once

typedef struct PossibleUGCProject PossibleUGCProject;

void ugcProjectChooser_FinishedLoading( void );
PossibleUGCProject* ugcProjectChooser_GetPossibleProjectByID( ContainerID uProjectID );

// Exposed so the Series Editor can draw projects in the same way
void ugcProjectChooser_ProjectDrawByID( ContainerID projectID, UI_MY_ARGS, float z, bool isSelected, bool isHovering );

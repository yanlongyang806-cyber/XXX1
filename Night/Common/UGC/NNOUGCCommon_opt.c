#include "NNOUGCCommon.h"

#include "NNOUGCResource.h"
#include "earray.h"

int count = 0;

UGCComponent *ugcComponentFindByID(const UGCComponentList *list, U32 id)
{
	UGCComponent* component = NULL;
	stashIntFindPointer( list->stComponentsById, id, &component );
	return component;
}

bool ugcComponentHasParent(const UGCComponentList *list, const UGCComponent *component, U32 parent_id)
{
	UGCComponent *parent;

	// ID == 0 means two different things, both of which should return
	// false here:
	//
	// If uParentID == 0, then this component has no parent, so it
	// can not have parent_id as a parent.
	//
	// If parent_id == 0, then it is the temporary component, which
	// can not be a parent for anything.
	if( component->uParentID == 0 || parent_id == 0 ) {
		return false;
	}

	if (component->uParentID == parent_id)
		return true;

	parent = ugcComponentFindByID(list, component->uParentID);
	if (parent)
		return ugcComponentHasParent(list, parent, parent_id);

	return false;
}

#include "ice-checklist.h"
#include "ice-candidates.h"

struct ice_checklist_t
{
	ice_candidates_t locals;
	ice_candidates_t remotes;
};

struct ice_checklist_t* ice_checklist_create()
{
	struct ice_checklist_t* l;
	l = (struct ice_checklist_t*)calloc(1, sizeof(struct ice_checklist_t));
	if (l)
	{
		ice_candidates_init(&l->locals);
		ice_candidates_init(&l->remotes);
	}
	return l;
}

int ice_checklist_destroy(struct ice_checklist_t** pl)
{
	struct ice_checklist_t* l;
	if (!pl || !*pl)
		return -1;
	
	l = *pl;
	ice_candidates_free(&l->locals);
	ice_candidates_free(&l->remotes);
	free(l);
	*pl = NULL;
	return 0;
}

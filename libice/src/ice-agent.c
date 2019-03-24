#include "ice-agent.h"
#include "ice-streams.h"
#include "ice-checklist.h"

struct ice_agent_t
{
	ice_streams_t streams;
};

struct ice_agent_t* ice_agent_create()
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)calloc(1, sizeof(struct ice_agent_t));
	if (ice)
	{
		ice_streams_init(&ice->streams);
	}

	return ice;
}

int ice_agent_destroy(struct ice_agent_t* ice)
{
	if (ice)
	{
		ice_streams_free(&ice->streams);
	}
	return 0;
}

int ice_add_local_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* c)
{
	ice_streams_find(ice->streams, stream, NULL);
	return ice_candidates_insert(&ice->locals, c);
}

int ice_add_remote_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* c)
{
	return ice_candidates_insert(&ice->remotes, c);
}

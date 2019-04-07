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

struct ice_stream_t* ice_agent_fetch_stream(struct ice_agent_t* ice, int stream)
{
	int n, before;
	struct ice_stream_t s;

	n = ice_streams_find(&ice->streams, stream, &before);
	if (-1 != n)
		return (struct ice_stream_t*)darray_get(&ice->streams, n);
	
	memset(&s, 0, sizeof(s));
	s.checks = ice_checklist_create();
	s.stream = stream;
	s.default = darray_count(&ice->streams) == 0 ? 1 : 0; // add default stream

	if (0 != darray_insert(&ice->streams, before, &s, 1))
	{
		ice_checklist_destroy(&s.checks);
		return NULL;
	}

	return (struct ice_stream_t*)darray_get(&ice->streams, before);
}

int ice_add_local_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* c)
{
	struct ice_stream_t *s;
	s = ice_agent_fetch_stream(ice, stream);
	if (!s) return -1;

	return ice_checklist_add_local_candidate(s->checks, c);
}

int ice_add_remote_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* c)
{
	struct ice_stream_t *s;
	s = ice_agent_fetch_stream(ice, stream);
	if (!s) return -1;

	return ice_checklist_add_remote_candidate(s->checks, c);
}

int ice_gather_stun_candidate(struct ice_agent_t* ice, ice_agent_ongather ongather, void* param)
{
	int i;
	struct ice_stream_t *s;
	for (i = 0; i < darray_count(&ice->streams); i++)
	{
		s = (struct ice_stream_t *)darray_get(&ice->streams, i);
		if (s && s->l)
			ice_checklist_gather_stun_candidate(s->checks);
	}

	return ongather(param, -1);
}

int ice_get_default_candidate(struct ice_agent_t* ice, int stream, ice_component_t component, struct ice_candidate_t* c)
{
	int n;
	struct ice_stream_t* s;

	n = ice_streams_find(&ice->streams, stream, NULL);
	if (-1 == n)
		return -1;

	s = (struct ice_stream_t*)darray_get(&ice->streams, n);
	return ice_checklist_get_default_candidate(s->checks, component, c);
}

int ice_list_local_candidate(struct ice_agent_t* ice, int stream, ice_agent_oncandidate oncand, void* param)
{
	int n;
	struct ice_stream_t* s;

	n = ice_streams_find(&ice->streams, stream, NULL);
	if (-1 == n)
		return -1;

	s = (struct ice_stream_t*)darray_get(&ice->streams, n);
	return ice_checklist_list_local_candidate(s->checks, oncand, param);
}

int ice_list_remote_candidate(struct ice_agent_t* ice, int stream, ice_agent_oncandidate oncand, void* param)
{
	int n;
	struct ice_stream_t* s;

	n = ice_streams_find(&ice->streams, stream, NULL);
	if (-1 == n)
		return -1;

	s = (struct ice_stream_t*)darray_get(&ice->streams, n);
	return ice_checklist_list_remote_candidate(s->checks, oncand, param);
}

int ice_start(struct ice_agent_t* ice)
{
	int i;
	struct ice_stream_t* stream;
	
	if (ice_streams_count(&ice->streams) < 1)
		return -1;

	for (i = 0; i < ice_streams_count(&ice->streams); i++)
	{
		stream = ice_streams_get(&ice->streams, i);
		if (stream->default)
			break;
	}

	assert(i < ice_streams_count(&ice->streams));
	i = i % ice_streams_count(&ice->streams);
	stream = ice_streams_get(&ice->streams, i);

	return ice_checklist_start(&stream->checks);
}

int ice_agent_onbind(struct ice_agent_t* ice, const struct sockaddr_storage* addr)
{
	int i, r;
	struct ice_stream_t* stream;
	for (i = 0; i < ice_streams_count(&ice->streams); i++)
	{
		stream = ice_streams_get(&ice->streams, i);
		r = ice_checklist_onbind(&stream->checks, addr);
		if (-1 != r)
			return r;
	}

	return -1; // NOT found
}

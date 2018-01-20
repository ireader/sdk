// http://www.bittorrent.org/beps/bep_0028.html (Tracker exchange extension)

#include "peer-extended.h"
#include "bencode.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int peer_tex_read(const uint8_t* buffer, int bytes)
{
	int r;
	size_t i, j;
	char* tracker;
	struct bvalue_t root, *node;
	r = bencode_read(buffer, bytes, &root);
	if (r <= 0)
		return r;

	if (root.type == BT_DICT)
	{
		for (i = 0; i < root.v.dict.count && 0 == r; i++)
		{
			if (0 == strcmp("added", root.v.dict.names[i].name))
			{
				// { 'added': ['http://tracker.bittorrent.com/announce', 'http://tracker2.bittorrent.com'] }
				assert(BT_LIST == root.v.dict.values[i].type);
				if (BT_LIST == root.v.dict.values[i].type)
				{
					node = &root.v.dict.values[i];
					for (j = 0; j < node->v.list.count; j++)
					{
						assert(BT_STRING == node->v.list.values[j].type);
						if(0 == bencode_get_string(&node->v.list.values[j], &tracker))
							free(tracker);
					}
				}
			}
			else
			{
				// unknown keyword
				assert(0);
			}
		}
	}

	bencode_free(&root);
	return 0;
}

#include "meimm.h"

int main(int argc, char **argv)
{
	struct meimm __mm, *mm;
	mm = &__mm;

	meimm_init(mm, true);
	meimm_alloc_map_memory(mm, 1024);
	meimm_free_memory(mm);
	meimm_deinit(mm);

	return 0;
}


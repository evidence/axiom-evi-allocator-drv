#define DEBUG

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include <asm/page.h>

#include <linux/ioport.h>

#include "axiom_mem_manager.h"
#include "axiom_mem_dev_user.h"
#include "axiom_mem_dev_lib.h"

static inline int is_valid_res_mem(struct res_mem *mem)
{
	return !((mem->start == 0 && mem->end == 0)
		 || (mem->start > mem->end));
}

int setup_user_vaddr(struct mem_config *memory, struct axiom_mem_dev_info *r)
{
	struct res_mem tmp;
	int err;

	pr_info("%s] enter\n", __func__);
	if (is_valid_res_mem(&(memory->virt_mem))) {
		r->base = memory->virt_mem.start;
		r->size = memory->virt_mem.end - memory->virt_mem.start;
		pr_info("%s] already allocated b=0x%zx s=%zu\n", __func__,
			r->base, r->size);

		return 0;
	}

	r->size = min(memory->size, r->size);
	tmp.start = r->base;
	tmp.end = r->base + r->size;

	pr_info("%s] try using b=0x%llx s=%llu\n", __func__,
		tmp.start, tmp.end - tmp.start);
	err = is_valid_res_mem(&tmp);
	if (err) {
		memory->virt_mem.start = tmp.start;
		memory->virt_mem.end = tmp.end;
		memory->v2p_offset = memory->base - memory->virt_mem.start;
		pr_info("%s] set b=0x%llx s=%llu\n", __func__,
			memory->virt_mem.start,
			memory->virt_mem.end - memory->virt_mem.start);
	} else {
		r->size = 0;
		r->base = 0;
		pr_info("%s] error set b=0x%lx s=%zu\n", __func__,
			r->base, r->size);
	}

	return !err;
}

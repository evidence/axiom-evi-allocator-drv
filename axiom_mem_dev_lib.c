#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include <asm/page.h>

#include "axiom_mem_dev_user.h"
#include "axiom_mem_dev_lib.h"

int reserve_private(struct axiom_mem_dev_info *r, struct res_mem *priv_mem,
		    struct res_mem *shared_mem)
{
	u64 priv_start = priv_mem->start;
	u64 priv_end = priv_mem->end + r->size;
	u64 shared_start = shared_mem->start;
	/* u64 shared_end = shared_mem->end; */

	priv_end = ALIGN(priv_end, PAGE_SIZE);

	pr_info("shared_start 0x%llx\n", shared_start);
	pr_info("priv_start 0x%llx\n", priv_start);
	pr_info("priv_end 0x%llx\n", priv_end);

	if (priv_end > shared_start) {
		pr_err("private region too large\n");
		return -ENOMEM;
	}

	r->base = priv_mem->end;
	r->size = priv_end - priv_mem->end;

	priv_mem->end = priv_end;
	priv_mem->size = priv_end - priv_start;

	return 0;
}

static inline int is_valid_res_mem(struct res_mem *mem)
{
	return !((mem->start == 0 && mem->end == 0)
		 || (mem->start > mem->end)
		 || ((mem->end - mem->start) != mem->size));
}

int setup_user_vaddr(struct axiom_mem_dev_info *r, struct res_mem *mem,
		     size_t max_size)
{
	struct res_mem tmp;
	int err;

	if (is_valid_res_mem(mem)) {
		r->base = mem->start;
		r->size = mem->size;

		return 0;
	}

	r->size = min(max_size, r->size);
	tmp.start = r->base;
	tmp.end = r->base + r->size;
	tmp.size = r->size;

	err = is_valid_res_mem(&tmp);
	if (err) {
		mem->start = tmp.start;
		mem->end = tmp.end;
		mem->size = tmp.size;
	} else {
		r->size = 0;
		r->base = 0;
	}

	return !err;
}

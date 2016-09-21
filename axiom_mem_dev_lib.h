#ifndef AXIOM_MEM_DEV_LIB_H
#define AXIOM_MEM_DEV_LIB_H

struct res_mem {
	u64 start; /* unsigned long? */
	u64 end; /* unsigned long? */
	size_t size;
};

struct mem_config {
	u64 base; /* unsigned long? */
	size_t size;

	struct res_mem virt_mem;
	struct res_mem priv_mem;
	struct res_mem shared_mem;
};


int reserve_private(struct axiom_mem_dev_info *r, struct res_mem *priv_mem,
		    struct res_mem *shared_mem);

int setup_user_vaddr(struct axiom_mem_dev_info *r, struct res_mem *mem,
		     size_t max_size);
#endif /* AXIOM_MEM_DEV_LIB_H */

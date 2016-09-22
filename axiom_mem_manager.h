#ifndef AXIOM_MEM_MANAGER_H
#define AXIOM_MEM_MANAGER_H

struct list_elem_s {
	struct list_head list;
	int tag;
	long start;
	long end;
};

struct res_mem {
	u64 start; /* unsigned long? */
	u64 end; /* unsigned long? */
};

struct mem_config {
	u64 base; /* unsigned long? */
	size_t size;
	long v2p_offset;

	struct res_mem virt_mem;

	struct list_elem_s free_list;
	struct list_elem_s alloc_list;
};
#define TAG_APP_NONE (-1)

static inline unsigned long axiom_v2p(struct mem_config *mem,
				      unsigned long vaddr)
{
	return vaddr + mem->v2p_offset;
}

int allocate_space(struct mem_config *memory, int tag, long start, long end);
int free_space(struct mem_config *memory, struct list_elem_s *e);
void axiom_mem_dev_reset_mem(struct mem_config *memory);
void axiom_mem_dev_init_mem(struct mem_config *memory, struct resource *r);
void dump_list(struct list_elem_s *list);

struct mem_config *mem_manager_create(const char *s, struct resource *r);
struct mem_config *mem_manager_find_by_name(const char *s);
int mem_manager_destroy(struct mem_config *m);
void mem_manager_dump(void);

#endif /* AXIOM_MEM_MANAGER_H */

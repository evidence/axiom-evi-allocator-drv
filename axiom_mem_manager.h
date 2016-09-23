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
#define TAG_NONE (-1)

static inline unsigned long axiom_v2p(struct mem_config *mem,
				      unsigned long vaddr)
{
	return vaddr + mem->v2p_offset;
}

int mem_allocate_space(struct mem_config *memory, int tag,
		       long start, long end);
int mem_free_space(struct mem_config *memory, int tag, long start, long end);
void mem_dump_list(struct mem_config *mem);

struct mem_config *mem_manager_create(const char *s, struct resource *r);
struct mem_config *mem_manager_find_by_name(const char *s);
int mem_manager_destroy(struct mem_config *m);
void mem_manager_dump(void);

#endif /* AXIOM_MEM_MANAGER_H */

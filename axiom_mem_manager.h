#ifndef AXIOM_MEM_MANAGER_H
#define AXIOM_MEM_MANAGER_H

struct mem_config;
#define TAG_NONE (-1)

struct mem_config *mem_manager_create(const char *s, struct resource *r);
struct mem_config *mem_manager_find_by_name(const char *s);
int mem_manager_destroy(struct mem_config *memory);
void mem_manager_dump(void);

int mem_allocate_space(struct mem_config *memory, int tag,
		       long vaddr_start, long vaddr_end, unsigned long *offset);
int mem_free_space(struct mem_config *memory, int tag, long vaddr_start,
		   long vaddr_end);
int mem_setup_user_vaddr(struct mem_config *mem,
			 unsigned long *base, unsigned long *size);
int mem_get_phy_space(struct mem_config *mem, unsigned long *base, size_t *size);
void mem_dump_list(struct mem_config *mem);


#endif /* AXIOM_MEM_MANAGER_H */

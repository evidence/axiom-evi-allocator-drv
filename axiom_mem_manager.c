#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/mutex.h>

#include "axiom_mem_manager.h"

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
	unsigned long base;
	size_t size;
	long v2p_offset;

	struct res_mem virt_mem;

	struct list_elem_s free_list;
	struct list_elem_s alloc_list;

	struct mutex mem_mutex;
};

static inline unsigned long mem_v2p(struct mem_config *mem, unsigned long vaddr)
{
	return vaddr + mem->v2p_offset;
}

static void dump_list(struct list_elem_s *list)
{
	struct list_elem_s *tmp;
	int i = 0;

	list_for_each_entry(tmp, &(list->list), list) {
		pr_info("%d) [%d] <0x%lx, 0x%lx>\n", i, tmp->tag,
			 tmp->start, tmp->end);
		++i;
	}
}

static struct list_elem_s *new_element(long start, long end)
{
	struct list_elem_s *d;

	d = (struct list_elem_s *)kmalloc(sizeof(*d), GFP_KERNEL);
	if (d) {
		d->start = start;
		d->end = end;
		d->tag = TAG_NONE;
	}

	return d;
}

static struct list_elem_s *dup_element(struct list_elem_s *e)
{
	return new_element(e->start, e->end);
}

static void merge_elem(struct list_elem_s *c, struct list_head *n)
{
	struct list_elem_s *ne = list_entry(n, struct list_elem_s, list);

	if (ne->start == c->end) {
		c->end = ne->end;
		list_del(n);
	}
}

static int add_element_sorted(struct list_elem_s *l, struct list_elem_s *e)
{
	struct list_elem_s *tmp;
	struct list_head *pos;

	if (list_empty(&(l->list))) {
		list_add(&(e->list), &(l->list));
		return 0;
	}

	list_for_each(pos, &(l->list)) {
		tmp = list_entry(pos, struct list_elem_s, list);
		if (tmp->start >= e->end) {
			list_add_tail(&(e->list), &(tmp->list));
			return 0;
		}
	}
	list_add_tail(&(e->list), &(l->list));

	return 0;
}

static int init_space(struct list_elem_s *alist, struct list_elem_s *flist,
		      long start, long end)
{
	struct list_elem_s *tmp;

	pr_info("%s] start=%ld end=%ld\n", __func__, start, end);
	if (start > end)
		return -1;

	INIT_LIST_HEAD(&(flist->list));
	INIT_LIST_HEAD(&(alist->list));

	tmp = new_element(start, end);
	if (tmp == NULL) {
		pr_info("%s] Unable to allocate new_element\n", __func__);
		return -1;
	}
	list_add(&(tmp->list), &(flist->list));

	return 0;
}

static int add_element_merge(struct list_elem_s *l, struct list_elem_s *d)
{
	struct list_elem_s *tmp;
	struct list_elem_s *e;
	struct list_head *pos;
	struct list_head *n;

	if (d->start >= d->end) {
pr_info("%s] Nothing to do\n", __func__);
		return 0;
	}

	e = dup_element(d);
	if (e == NULL)
		return -1;

	if (list_empty(&(l->list))) {
		list_add(&(e->list), &(l->list));
pr_info("%s] Added first element\n", __func__);
		return 0;
	}

	list_for_each(pos, &(l->list)) {
		tmp = list_entry(pos, struct list_elem_s, list);
pr_info("%s] Scanning <%ld, %ld> (<%ld,%ld>)\n", __func__, tmp->start, tmp->end, e->start, e->end);
		if (e->end == tmp->start) {
pr_info("%s] Changed START\n", __func__);
			tmp->start = e->start;
			/*TODO: merge with previous element */
			n = pos->prev;
			if (n != &(l->list)) {
pr_info("%s] Merge prev\n", __func__);
				tmp = list_entry(n, struct list_elem_s, list);
				merge_elem(tmp, pos);
			}

			return 0;
		}

		if (e->start == tmp->end) {
pr_info("%s] Changed END\n", __func__);
			tmp->end = e->end;
			/*TODO: merge with next element */
			n = pos->next;
			if (n != &(l->list)) {
pr_info("%s] Merge next\n", __func__);
				merge_elem(tmp, n);
			}
			return 0;
		}

		if (tmp->start > e->end) {
pr_info("%s] Added (<%ld, %ld>) before <%ld, %ld>\n", __func__, e->start, e->end, tmp->start, tmp->end);
			list_add_tail(&(e->list), &(tmp->list));

			return 0;
		}
	}
pr_info("%s] Added before LAST <%ld, %ld>\n", __func__, tmp->start, tmp->end);
	list_add_tail(&(e->list), &(l->list));

	return 0;
}

struct mem_mon_t {
	struct list_head list;
	char *name;
	struct mem_config *mem;
};
static LIST_HEAD(mem_list);
static DEFINE_MUTEX(manager_mutex);

static struct mem_config *_find_mem_by_name(const char *s)
{
	struct mem_mon_t *p;

	list_for_each_entry(p, &mem_list, list) {
		if (strcmp(s, p->name) == 0) {
			return p->mem;
		}
	}

	return NULL;
}

struct mem_config *mem_manager_create(const char *s, struct resource *r)
{
	int err = 0;
	struct mem_config *memory;
	struct mem_mon_t *e;

	pr_info("%s]\n", __func__);

	mutex_lock(&manager_mutex);
	memory = _find_mem_by_name(s);
	if (memory != NULL) {
		mutex_unlock(&manager_mutex);
		pr_info("Memory already registered\n");
		return NULL;
	}

	memory = (struct mem_config *)kmalloc(sizeof(*memory), GFP_KERNEL);
	if (memory == NULL)
		goto out;

	memory->base = (u64)r->start;
	memory->size = resource_size(r);

	memory->virt_mem.start = memory->virt_mem.end = 0;

	err = init_space(&memory->alloc_list,
			 &memory->free_list,
			 memory->base,
			 memory->base + memory->size);
	pr_info("%s] init_space = %d\n", __func__, err);
	if (err)
		goto err1;

	e = (struct mem_mon_t *)kmalloc(sizeof(*e), GFP_KERNEL);
	if (e == NULL)
		goto err1;

	e->name = kstrdup(s, GFP_KERNEL);
	if (e->name == NULL)
		goto err2;

	e->mem = memory;
	list_add_tail(&e->list, &mem_list);
	mutex_init(&memory->mem_mutex);

	mutex_unlock(&manager_mutex);

	return memory;
err2:
	kfree(e);
err1:
	kfree(memory);
	memory = NULL;

out:
	mutex_unlock(&manager_mutex);

	return memory;
}


struct mem_config *mem_manager_find_by_name(const char *s)
{
	struct mem_config *mem = NULL;

	mutex_lock(&manager_mutex);
	mem = _find_mem_by_name(s);
	mutex_unlock(&manager_mutex);

	return mem;
}

static int _remove_mem(struct mem_config *m)
{
	struct mem_mon_t *p;
	struct mem_mon_t *r = NULL;
	int ret = -1;

	list_for_each_entry(p, &mem_list, list) {
		if (p->mem == m) {
			r = p;
			break;
		}
	}

	if (r) {
		list_del(&(r->list));
		kfree(r);
		ret = 0;
		BUG_ON(mutex_is_locked(&(m->mem_mutex)));
		kfree(m);
	}

	return ret;
}

int mem_manager_destroy(struct mem_config *memory)
{
	int err;

	mutex_lock(&manager_mutex);
	err = _remove_mem(memory);
	mutex_unlock(&manager_mutex);

	return err;
}

void mem_manager_dump()
{
	struct mem_mon_t *p;
	int i = 0;

	mutex_lock(&manager_mutex);
	list_for_each_entry(p, &mem_list, list)
		pr_info("%d) <%s> %p\n", i, p->name, p->mem);
	mutex_unlock(&manager_mutex);
}


static int _mem_allocate_space(struct mem_config *memory, int tag,
			       long vaddr_start, long vaddr_end)
{
	struct list_head *pos;
	struct list_elem_s *tmp;
	struct list_elem_s *z;

	struct list_elem_s *fl;
	struct list_elem_s *al;

	long start = mem_v2p(memory, vaddr_start);
	long end = mem_v2p(memory, vaddr_end);

	fl = &(memory->free_list);
	al = &(memory->alloc_list);

	pr_info("%s] Searching zone for <%ld,%ld>\n", __func__, start, end);
	pr_info("%s] memory: %p\n", __func__, memory);

	if (start >= end)
		return -EINVAL;

	list_for_each(pos, &(fl->list)) {
		tmp = list_entry(pos, struct list_elem_s, list);
		pr_info("%s] Got region: <%ld,%ld>\n", __func__,
			tmp->start, tmp->end);
		if (tmp->start <= start && tmp->end >= end) {
			pr_info("%s] Hit region: <%ld,%ld>\n", __func__,
				tmp->start, tmp->end);
			if (tmp->start < start) {
				if (tmp->end > end) {
					z = new_element(end, tmp->end);
					if (!z)
						return -ENOMEM;
					list_add(&(z->list), &(tmp->list));
				}
				//tmp->end = start - 1;
				tmp->end = start;
			} else {
				if (tmp->end == end) {
					list_del(&(tmp->list));
				} else {
					tmp->start = end;
				}
			}

			tmp = new_element(start, end);
			if (!tmp)
				return -ENOMEM;
			tmp->tag = tag;

			return add_element_sorted(al, tmp);
		}
	}

	return -ENOMEM;
}

int mem_allocate_space(struct mem_config *memory, int tag,
		       long vaddr_start, long vaddr_end)
{
	int ret;

	if (memory == NULL)
		return -EINVAL;

	mutex_lock(&memory->mem_mutex);
	ret = _mem_allocate_space(memory, tag, vaddr_start, vaddr_end);
	mutex_unlock(&memory->mem_mutex);

	return ret;
}

static int _mem_free_space(struct mem_config *memory, int tag,
			   long vaddr_start, long vaddr_end)
{
	struct list_head *pos, *q;
	struct list_elem_s *tmp;
	struct list_elem_s *rem;

	long start = mem_v2p(memory, vaddr_start);
	long end = mem_v2p(memory, vaddr_end);

	struct list_elem_s *fl = &(memory->free_list);
	struct list_elem_s *al = &(memory->alloc_list);

	int err = 0;

	list_for_each_safe(pos, q, &(al->list)) {
		tmp = list_entry(pos, struct list_elem_s, list);
		if (tmp->tag != tag)
			continue;

pr_info("Comparing <%ld,%ld> with <%ld,%ld>\n", tmp->start, tmp->end, start, end);

		if (start <= tmp->start && end >= tmp->end) {
pr_info("All inside <%ld,%ld> in <%ld,%ld>\n", tmp->start, tmp->end, start, end);
			add_element_merge(fl, tmp);
			list_del(pos);
			kfree(tmp);

			continue;
		}

		if (tmp->start <= start && tmp->end >= end) {
pr_info("Region <%ld,%ld> contains <%ld,%ld>\n", tmp->start, tmp->end, start, end);
			rem = new_element(start, end);
			if (rem == NULL)
				return -ENOMEM;

			err = add_element_merge(fl, rem);
			if (err)
				break;

			rem = new_element(end, tmp->end);
			if (rem == NULL)
				return -ENOMEM;

			rem->tag = tmp->tag;

			tmp->end = start;
pr_info("Reinsert <%ld,%ld> after <%ld,%ld>\n", rem->start, rem->end, tmp->start, tmp->end);
			list_add(&(rem->list), &(tmp->list));

			return 0;
		}

		if (tmp->end >= start && tmp->end <= end) {
			rem = new_element(start, tmp->end);
			if (rem == NULL)
				return -ENOMEM;

pr_info("change <%ld,%ld> with <%ld,%ld>\n", tmp->start, tmp->end, rem->start, rem->end);
			tmp->end = start;
			err = add_element_merge(fl, rem);
			if (err)
				break;
		}

		if (tmp->start >= start && tmp->start <= end) {
			rem = new_element(tmp->start, end);
			if (rem == NULL)
				return -ENOMEM;

pr_info("change <%ld,%ld> with <%ld,%ld>\n", tmp->start, tmp->end, rem->start, rem->end);
			tmp->start = end;
			err = add_element_merge(fl, rem);
			if (err)
				break;
		}
	}

	return err;
}

int mem_free_space(struct mem_config *memory, int tag,
		   long vaddr_start, long vaddr_end)
{
	int err = -EINVAL;

	if (memory != NULL) {
		mutex_lock(&memory->mem_mutex);
		err = _mem_free_space(memory, tag, vaddr_start, vaddr_end);
		mutex_unlock(&memory->mem_mutex);
	}

	return err;
}

static inline int is_valid_res_mem(struct res_mem *mem)
{
	return !((mem->start == 0 && mem->end == 0)
		 || (mem->start > mem->end));
}

static int _mem_setup_user_vaddr(struct mem_config *memory,
				 unsigned long *base, unsigned long *size)
{
	struct res_mem tmp;
	int err;

	pr_info("%s] enter\n", __func__);
	if (base == NULL || size == NULL) {
		pr_info("%s] invalid base/size arguments\n", __func__);
		return 0;
	}

	if (is_valid_res_mem(&(memory->virt_mem))) {
		*base = memory->virt_mem.start;
		*size = memory->virt_mem.end - memory->virt_mem.start;
		pr_info("%s] already allocated b=0x%zx s=%zu\n", __func__,
			*base, *size);

		return 0;
	}

	*size = min(memory->size, *size);
	tmp.start = *base;
	tmp.end = *base + *size;

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
		*size = 0;
		*base = 0;
		pr_info("%s] error set b=0x%lx s=%zu\n", __func__,
			*base, *size);
	}

	return !err;
}

int mem_setup_user_vaddr(struct mem_config *mem,
			 unsigned long *base, unsigned long *size)
{
	int ret;

	if (mem == NULL)
		return -1;

	mutex_lock(&(mem->mem_mutex));
	ret = _mem_setup_user_vaddr(mem, base, size);
	mutex_unlock(&(mem->mem_mutex));

	return ret;
}

int mem_get_phy_space(struct mem_config *mem, unsigned long *base, size_t *size)
{
	if (mem == NULL)
		return -1;

	mutex_lock(&(mem->mem_mutex));
	if (base != NULL)
		*base = mem->base;
	if (size != NULL)
		*size = mem->size;
	mutex_unlock(&(mem->mem_mutex));

	return 0;
}

void mem_dump_list(struct mem_config *mem)
{
	if (mem == NULL)
		return;

	mutex_lock(&mem->mem_mutex);
	pr_info("alloc list:\n");
	dump_list(&mem->alloc_list);
	pr_info("free list:\n");
	dump_list(&mem->free_list);
	mutex_unlock(&mem->mem_mutex);
}

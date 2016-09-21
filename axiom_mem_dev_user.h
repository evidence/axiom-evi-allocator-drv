#ifndef AXIOM_MEM_DEV_USER_H
#define AXIOM_MEM_DEV_USER_H

struct axiom_mem_dev_info {
	unsigned long base;
	unsigned long size;
};

#define AXIOM_MEM_DEV_MAGIC  'x'
#define AXIOM_MEM_DEV_GET_MEM_INFO _IOR(AXIOM_MEM_DEV_MAGIC, 100,\
					struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_PRIVATE_ALLOC _IOR(AXIOM_MEM_DEV_MAGIC, 101,\
					 struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_GET_PRIVATE_MEM_INFO _IOR(AXIOM_MEM_DEV_MAGIC, 102,\
						struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_SHARED_ALLOC _IOR(AXIOM_MEM_DEV_MAGIC, 103,\
					struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_CONFIG_VMEM _IOWR(AXIOM_MEM_DEV_MAGIC, 103,\
					struct axiom_mem_dev_info [1])

#endif /* AXIOM_MEM_DEV_USER_H */

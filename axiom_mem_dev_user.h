#ifndef AXIOM_MEM_DEV_USER_H
#define AXIOM_MEM_DEV_USER_H

struct axiom_mem_dev_info {
	unsigned long base;
	unsigned long size;
};

#define AXIOM_MEM_DEV_MAGIC  'x'
#define AXIOM_MEM_DEV_GET_MEM_INFO _IOR(AXIOM_MEM_DEV_MAGIC, 100,\
					struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_CONFIG_VMEM _IOWR(AXIOM_MEM_DEV_MAGIC, 103,\
					struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_RESERVE_MEM _IOWR(AXIOM_MEM_DEV_MAGIC, 104,\
					struct axiom_mem_dev_info [1])

#define AXIOM_MEM_DEV_SET_APP_ID  _IOWR(AXIOM_MEM_DEV_MAGIC, 105, int)

#define AXIOM_MEM_DEV_REVOKE_MEM  _IOWR(AXIOM_MEM_DEV_MAGIC, 106,\
					struct axiom_mem_dev_info [1])

#endif /* AXIOM_MEM_DEV_USER_H */

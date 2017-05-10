/*!
 * \file axiom_mem_dev.h
 *
 * \version     v0.12
 * \date        2016-09-23
 *
 * Copyright (C) 2016, Evidence Srl.
 * Terms of use are as specified in COPYING
 */
#ifndef AXIOM_MEM_DEV_H
#define AXIOM_MEM_DEV_H

int axiom_mem_dev_get_appspace(unsigned long *base, size_t *size);
int axiom_mem_dev_get_nicspace(unsigned long *base, size_t *size);
int axiom_mem_dev_virt2off(int appid, unsigned long virt, size_t size,
        unsigned long *offset);

#endif /* AXIOM_MEM_DEV_H */

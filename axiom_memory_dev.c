/*!
 * \file axiom_memory_dev.c
 *
 * \version     v0.13
 * \date        2016-09-23
 *
 * Copyright (C) 2016, Evidence Srl.
 * Terms of use are as specified in COPYING
 */
#include <linux/module.h>    /* included for all kernel modules */
#include <linux/version.h>
#include <linux/kernel.h>    /* included for KERN_INFO */
#include <linux/types.h>

#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/init.h>      /* included for __init and __exit macros */
#include <linux/slab.h>


#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

#include <asm/pgtable.h>

#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
/*#include <xen/page.h>*/
#include <asm/page.h>

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/highmem.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>

#include <linux/dma-contiguous.h>

#include <linux/mman.h>

#include <linux/ioctl.h>

#include "axiom_mem_manager.h"
#include "axiom_mem_dev_user.h"
#include "axiom_mem_dev.h"


#define DRIVER_NAME     "axiom"

static dev_t first_dev;   /* first device number  */

#define MAX_DEVICES_NUMBER 5
static struct class *axiom_mem_dev_cl;  /* device class */
static DEFINE_MUTEX(manager_mutex);
static int dev_num_instance;

#define MAX_APP_ID              255
#define DEFAULT_DEVICE          0

struct axiom_mem_app {
	int used_regions;
	struct axiom_mem_dev_info regions[AXIOM_MEM_APP_MAX_REG];
};

static struct axiom_mem_dev_struct {
	struct cdev c_dev; /* character device structure */
	struct device *dev; /* TODO: remove this field */
	dev_t devt;
	struct mem_config *memory;
	unsigned long app_phys_base;
	size_t app_phys_size;
	struct axiom_mem_app mem_app[MAX_APP_ID + 1];
} axiom_mem_dev[MAX_DEVICES_NUMBER];

struct fpriv_data_s {
	struct axiom_mem_dev_struct *dev;
	int axiom_app_id;
};

#define TAG_APP_NONE (TAG_NONE)

int axiom_mem_dev_get_appspace(unsigned long *base, size_t *size)
{
	struct axiom_mem_dev_struct *mem_dev = &axiom_mem_dev[DEFAULT_DEVICE];

	if (mem_dev->memory == NULL)
		return -EFAULT;

	*base = mem_dev->app_phys_base;
	*size = mem_dev->app_phys_size;

	return 0;
}
EXPORT_SYMBOL_GPL(axiom_mem_dev_get_appspace);

int axiom_mem_dev_get_nicspace(unsigned long *base, size_t *size)
{
	struct axiom_mem_dev_struct *mem_dev = &axiom_mem_dev[DEFAULT_DEVICE];

	return mem_get_nic_space(mem_dev->memory, base, size);
}
EXPORT_SYMBOL_GPL(axiom_mem_dev_get_nicspace);

int axiom_mem_dev_virt2off(int appid, unsigned long virt, size_t size,
			   unsigned long *offset)
{
	struct axiom_mem_dev_struct *mem_dev = &axiom_mem_dev[DEFAULT_DEVICE];
	struct axiom_mem_app *mem_app;
	int i;

	if (appid > MAX_APP_ID || mem_dev->mem_app[appid].used_regions == 0) {
		dev_err(mem_dev->dev, "application not initialized - appid: %d "
		        "used_regions: %d\n", appid,
		        mem_dev->mem_app[appid].used_regions);
		return -EFAULT;
	}

	mem_app = &mem_dev->mem_app[appid];
	for (i = 0; i < mem_app->used_regions; i++) {
		if ((virt >= mem_app->regions[i].base) &&
		    (virt + size <
		    mem_app->regions[i].base + mem_app->regions[i].size)) {
			unsigned long phys = mem_virt_to_phys(mem_dev->memory,
						(unsigned long)virt);

			*offset = phys - mem_dev->app_phys_base;

			return 0;
		}
	}

	dev_err(mem_dev->dev, "mappping not found - appid: %d virt: %lu "
	        "size: %lu \n", appid, virt, size);
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(axiom_mem_dev_virt2off);

static int axiom_mem_dev_open(struct inode *i, struct file *f)
{
	struct fpriv_data_s *pdata;
	int minor = iminor(i);
	int major = imajor(i);
	struct axiom_mem_dev_struct *dev =
		container_of(i->i_cdev, struct axiom_mem_dev_struct, c_dev);

	dev_dbg(dev->dev, "%s] dev %d:%d\n", __func__, major, minor);

	pdata = kmalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(dev->dev, "Not enough memory for file private data\n");
		return -ENOMEM;
	}
	pdata->dev = dev;
	pdata->axiom_app_id = TAG_APP_NONE;
	f->private_data = pdata;

	return 0;
}

static int axiom_mem_dev_close(struct inode *i, struct file *f)
{
	struct fpriv_data_s *pdata = f->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev;

        if (pdata->axiom_app_id != TAG_APP_NONE) {
		dev->mem_app[pdata->axiom_app_id].used_regions = 0;
        }
	mem_free_space(dev->memory, pdata->axiom_app_id, 0, LONG_MAX);
#ifdef DEBUG
	mem_dump_list(dev->memory);
#endif
	kfree(pdata);

	return 0;
}

static ssize_t axiom_mem_dev_read(struct file *f, char __user *buf, size_t len,
				  loff_t *off)
{
	return -ENOSYS;
}

static ssize_t axiom_mem_dev_write(struct file *f, const char __user *buf,
				   size_t len, loff_t *off)
{
	return -ENOSYS;
}

static int axiom_mem_dev_map_to_userspace(struct file *f,
					  struct axiom_mem_dev_info *region,
					  unsigned long offset)
{
	struct fpriv_data_s *pdata = f->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev;

	struct mm_struct *mm;
	struct vm_area_struct vma;
	long vmaddr;

	mm = get_task_mm(current);
	if (IS_ERR(mm)) {
		dev_err(dev->dev, "Unable to get mm\n");
		return -1;
	}
	vma.vm_start = region->base;
	vma.vm_end = region->base + region->size;
	vma.vm_pgoff = offset;

#if 0
	vma.vm_flags = VM_SHARED | VM_READ | VM_WRITE
		       | MAP_SHARED | MAP_LOCKED;
#else
	vma.vm_flags = MAP_SHARED | MAP_LOCKED | MAP_FIXED;
#endif

	vma.vm_mm = mm;

	pgprot_val(vma.vm_page_prot) = PROT_READ | PROT_WRITE;
	/* vma.vm_page_prot = PROT_NONE; */
	vma.vm_page_prot = pgprot_noncached(vma.vm_page_prot);

	/* force_mmap(f, &vma); */

	mmput(mm);

	dev_dbg(dev->dev, "Mapping vm_flags:%ld\n", vma.vm_flags);
	vmaddr = vm_mmap(f, vma.vm_start, vma.vm_end - vma.vm_start,
			 pgprot_val(vma.vm_page_prot), vma.vm_flags, vma.vm_pgoff);
	if (IS_ERR_VALUE(vmaddr)) {
		dev_err(dev->dev, "Unable to vm_mmap\n");
		return -1;
	} else {
		dev_dbg(dev->dev, "Mapped at %lx\n", vmaddr);
	}

	return 0;
}

static int axiom_mem_dev_reserve_mem(struct file *f,
				     struct axiom_mem_dev_info *info)
{
	struct fpriv_data_s *pdata = f->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev;
	unsigned long offset;
	int err;

	dev_dbg(dev->dev, "%s] allocate_space <%ld,%ld>(sz=%ld)\n",
		__func__, info->base, info->base + info->size, info->size);

	err = mem_allocate_space(dev->memory, pdata->axiom_app_id, info->base,
		info->base + info->size, &offset);
	if (err)
		return err;

	return axiom_mem_dev_map_to_userspace(f, info, offset);
}

static long axiom_mem_dev_ioctl(struct file *f, unsigned int cmd,
				unsigned long arg)
{
	struct fpriv_data_s *pdata = f->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev;
	int err;

	if (!dev)
		return -EINVAL;  /* -ENODEV? */

	switch (cmd) {
	case AXIOM_MEM_DEV_GET_MEM_INFO: {
		struct axiom_mem_dev_info tmp;

		err = mem_get_app_space(dev->memory, &tmp.base, &tmp.size);
		if (err)
			return -EINVAL;

		err = copy_to_user((void __user *)arg, &tmp, sizeof(tmp));
		if (err < 0)
			return -EFAULT;
		break;
	}
	case AXIOM_MEM_DEV_CONFIG_VMEM: {
		struct axiom_mem_dev_info tmp;

		err = copy_from_user(&tmp, (void __user *)arg, sizeof(tmp));
		if (err)
			return -EFAULT;

		err = mem_setup_user_vaddr(dev->memory, &tmp.base, &tmp.size);
		/*TODO: error from ioctl or from returned structure? */

		err = copy_to_user((void __user *)arg, &tmp, sizeof(tmp));
		if (err < 0)
			return -EFAULT;
		break;
	}
	case AXIOM_MEM_DEV_RESERVE_MEM: {
		struct axiom_mem_dev_info tmp;

		err = copy_from_user(&tmp, (void __user *)arg, sizeof(tmp));
		if (err)
			return -EFAULT;

		err = axiom_mem_dev_reserve_mem(f, &tmp);
		if (err)
			return -EFAULT;

#ifdef DEBUG
		mem_dump_list(dev->memory);
#endif
		break;
	}
	case AXIOM_MEM_DEV_REVOKE_MEM: {
		struct axiom_mem_dev_info tmp;
		long start, end;

		err = copy_from_user(&tmp, (void __user *)arg, sizeof(tmp));
		if (err)
			return -EFAULT;

		start = tmp.base;
		end = start + tmp.size;

		dev_dbg(dev->dev, "%s] free_space <%ld,%ld>(sz=%ld)\n",
			  __func__, start, end, tmp.size);

		err = mem_free_space(dev->memory, pdata->axiom_app_id,
				     start, end);
		if (err)
			return err;
#ifdef DEBUG
		mem_dump_list(dev->memory);
#endif
		break;
	}
	case AXIOM_MEM_DEV_SET_APP: {
		struct axiom_mem_dev_app tmp;
		struct axiom_mem_app *mem_app;
		int i;

		err = copy_from_user(&tmp, (void __user *)arg, sizeof(tmp));
		if (err)
			return -EFAULT;

		if (pdata->axiom_app_id != TAG_APP_NONE ||
			tmp.app_id > MAX_APP_ID)
			return -EINVAL;

		pdata->axiom_app_id = tmp.app_id;

		if (dev->mem_app[pdata->axiom_app_id].used_regions > 0 ||
			tmp.used_regions > AXIOM_MEM_APP_MAX_REG)
			return -EINVAL;

		mem_app = &dev->mem_app[pdata->axiom_app_id];

                for (i = 0; i < tmp.used_regions; i++) {
			mem_app->regions[i] = tmp.info[i];
			err = axiom_mem_dev_reserve_mem(f,
				&mem_app->regions[i]);
			if (err)
				return -EFAULT;
		}

		mem_app->used_regions = tmp.used_regions;

		dev_dbg(dev->dev, "Set app id: %d\n", pdata->axiom_app_id);

		break;
	}

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct vm_operations_struct mmap_mem_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

static int axiom_mem_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct fpriv_data_s *pdata = file->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev;
	size_t size = vma->vm_end - vma->vm_start;
	size_t mem_size;
	unsigned long mem_base;
	unsigned long phy_pfn;
	int err;

	dev_dbg(dev->dev, "1) vma = 0x%lx-0x%lx\n", vma->vm_start,
		  vma->vm_end);
	dev_dbg(dev->dev, "pg_off = %lx\n", vma->vm_pgoff);

	mem_get_app_space(dev->memory, &mem_base, &mem_size);
	if (size > mem_size) {
		dev_err(dev->dev, "%zu > max size (%zu)\n", size, mem_size);
		return -EAGAIN;
	}

#if 0
	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff, size,
						 vma->vm_page_prot);
	vma->vm_ops = &mmap_mem_ops;
#else
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

	if ((vma->vm_flags & VM_SHARED) != VM_SHARED)
		dev_dbg(dev->dev, "No shared MAP!!!!!!!!!!!!!!!!!\n");

	phy_pfn = (mem_base >> PAGE_SHIFT) + vma->vm_pgoff;

	/* remap-pfn-range will mark the range VM_IO */
	err = remap_pfn_range(vma, vma->vm_start, phy_pfn, size,
			      vma->vm_page_prot);
	if (err) {
		dev_err(dev->dev, "remap_pfn_range failed\n");
		return -EAGAIN;
	}

	return 0;
}

static const struct file_operations pugs_fops = {
	.owner = THIS_MODULE,
	.open = axiom_mem_dev_open,
	.release = axiom_mem_dev_close,
	.read = axiom_mem_dev_read,
	.write = axiom_mem_dev_write,
	.unlocked_ioctl = axiom_mem_dev_ioctl,
	.mmap = axiom_mem_dev_mmap,
};

static int axiom_mem_dev_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret;
	int ni;
	int major, minor;
	dev_t devt;
	struct mem_config *mem;

	/* no device tree device */
	if (!np) {
		pr_err("%s] No device tree found\n", __func__);
		return -ENODEV;
	}

	dev_dbg(dev, "%s M:%d m:%d   p=%p\n", __func__,
		 MAJOR(dev->devt), MINOR(dev->devt), dev);

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		dev_err(dev, "No memory-region specified\n");
		return -EINVAL;
	} else {
		dev_dbg(dev, "of_node_full_name = %s\n", of_node_full_name(np));
	}

	mem = mem_manager_find_by_name(of_node_full_name(np));
	if (mem == NULL) {
		dev_err(dev, "Unable to get memory <%s>\n",
			of_node_full_name(np));
		of_node_put(np);
			return -EINVAL;
	}
	of_node_put(np);

	mutex_lock(&manager_mutex);
	if (dev_num_instance > MAX_DEVICES_NUMBER) {
		dev_err(dev, "too many devices\n");
		ret = -ENODEV;
		goto err1;
	}

	ni = dev_num_instance;

	major = MAJOR(first_dev);
	minor = MINOR(first_dev) + ni;
	devt = MKDEV(major, minor);

	cdev_init(&axiom_mem_dev[ni].c_dev, &pugs_fops);
	axiom_mem_dev[ni].c_dev.owner = THIS_MODULE;
	axiom_mem_dev[ni].devt = devt;
	ret = cdev_add(&axiom_mem_dev[ni].c_dev, devt, 1);
	if (ret < 0) {
		dev_err(dev, "Error on cdev_add (minor:%d)\n", minor);
		goto err1;
	}

	axiom_mem_dev[ni].dev =
		device_create(axiom_mem_dev_cl, dev, devt,
			      NULL, "axiom_dev_mem%d", ni);

	if (IS_ERR(axiom_mem_dev[ni].dev)) {
		ret = PTR_ERR(axiom_mem_dev[ni].dev);
		dev_err(dev, "Unable to create device (minor:%d)\n", minor);
		goto err2;
	}

	/* dev_set_drvdata(dev, (void *)&(axiom_mem_dev[ni])); */
	dev_set_drvdata(axiom_mem_dev[ni].dev, (void *)&(axiom_mem_dev[ni]));
	platform_set_drvdata(pdev, (void *)&(axiom_mem_dev[ni]));

	axiom_mem_dev[ni].memory = mem;
	++dev_num_instance;

	mem_get_app_space(axiom_mem_dev[ni].memory,
			  &axiom_mem_dev[ni].app_phys_base,
			  &axiom_mem_dev[ni].app_phys_size);
	dev_info(dev, "dev%d base = 0x%lx size = %zu\n", ni,
		 axiom_mem_dev[ni].app_phys_base, axiom_mem_dev[ni].app_phys_size);

	mutex_unlock(&manager_mutex);

	return 0;
#if 0
err3:
	device_destroy(axiom_mem_dev_cl, axiom_mem_dev[ni].devt);
#endif
err2:
	cdev_del(&axiom_mem_dev[ni].c_dev);
err1:
	mutex_unlock(&manager_mutex);

	return ret;
}

static int axiom_mem_dev_remove(struct platform_device *pdev)
{
	struct axiom_mem_dev_struct *drvdata = platform_get_drvdata(pdev);

	cdev_del(&drvdata->c_dev);
	device_destroy(axiom_mem_dev_cl, drvdata->devt);

	mutex_lock(&manager_mutex);
	--dev_num_instance;
	mutex_unlock(&manager_mutex);

	return 0;
}


static const struct of_device_id axiom_mem_dev_of_match[] = {
	{ .compatible = "evi,axiom_mem_dev", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, axiom_mem_dev_of_match);

static struct platform_driver axiom_mem_dev_driver = {
	.driver = {
		.name = "evi,axiom_mem_dev",
		.of_match_table = axiom_mem_dev_of_match,
	},
	.probe          = axiom_mem_dev_probe,
	.remove         = axiom_mem_dev_remove,
};

static int __init axiom_mem_dev_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&first_dev, 0,
				  MAX_DEVICES_NUMBER, "MyModule");
	if (ret) {
		pr_err("Unable to allocate character region\n");
		goto err1;
	}

	axiom_mem_dev_cl = class_create(THIS_MODULE, "axiom_mem_dev_class");
	if (IS_ERR(axiom_mem_dev_cl)) {
		pr_err("Unable to create class\n");
		ret = PTR_ERR(axiom_mem_dev_cl);
		goto err2;
	}

	ret = platform_driver_register(&axiom_mem_dev_driver);
	if (ret) {
		pr_err("Unable to register axiom_mem_dev_driver\n");
		goto err3;
	}

	return ret;

err3:
	class_destroy(axiom_mem_dev_cl);
err2:
	unregister_chrdev_region(first_dev, MAX_DEVICES_NUMBER);
err1:
	return ret;
}

static void __exit axiom_mem_dev_cleanup(void)
{
	platform_driver_unregister(&axiom_mem_dev_driver);
	class_destroy(axiom_mem_dev_cl);
	unregister_chrdev_region(first_dev, MAX_DEVICES_NUMBER);
}

module_init(axiom_mem_dev_init);
module_exit(axiom_mem_dev_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bruno Morelli <b.morelli@evidence.eu.com");
MODULE_DESCRIPTION("Axiom Allocator Driver");
MODULE_VERSION("v0.13");

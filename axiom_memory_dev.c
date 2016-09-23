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
#include "axiom_mem_dev_lib.h"


#define DRIVER_NAME     "axiom"

static dev_t first_dev;   /* first device number  */


#define MAX_DEVICES_NUMBER 5
static struct class *axiom_mem_dev_cl;  /* device class */
static DEFINE_MUTEX(manager_mutex);
static int dev_num_instance;


static struct axiom_mem_dev_struct {
	struct cdev c_dev; /* character device structure */
	struct device *dev; /* TODO: remove this field */
	dev_t devt;
	struct mem_config *memory;
} axiom_mem_dev[MAX_DEVICES_NUMBER];

struct fpriv_data_s {
	struct axiom_mem_dev_struct *dev;
	int axiom_app_id;
	/*TODO busy list */
};

#define TAG_APP_NONE (TAG_NONE)
static unsigned long gaddr;
void *io_map;

static void dump_mem(struct axiom_mem_dev_struct *adev)
{
	struct device *dev = adev->dev;

	dev_info(dev, "Phy mem\n");
	dev_info(dev, "start   : 0x%llx\n", adev->memory->base);
	dev_info(dev, "size    : %zu\n", adev->memory->size);
}

/**/
#if 0
static int force_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct axiom_mem_dev_struct *dev = file->private_data;
	size_t size = vma->vm_end - vma->vm_start;
	unsigned long vm_pgoff_orig;
	unsigned long phy_pfn;
	int err;

	pr_info("Driver: mmap()\n");
	pr_info("1) vma = 0x%lx-0x%lx\n", vma->vm_start, vma->vm_end);

	vm_pgoff_orig = vma->vm_pgoff;
	pr_info("1) pg_off = %lx\n", vma->vm_pgoff);

	/* vma->vm_pgoff = (0x70000000 >> PAGE_SHIFT); */
	pr_info("2) pg_off = %lx\n", vma->vm_pgoff);

	if (size > dev->memory->size) {
		pr_err("%zu > max size (%zu)\n", size, dev->memory->size);
		return -EAGAIN;
	}

#if 0
	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff, size,
						 vma->vm_page_prot);
	vma->vm_ops = &mmap_mem_ops;
#else
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

	/* Remap-pfn-range will mark the range VM_IO */
#if 1

	pr_info("io_map = %p  pfn = %ld\n", io_map, vmalloc_to_pfn(io_map));
	pr_info("vm_page_prot = 0x%llx", pgprot_val(vma->vm_page_prot));

	if ((vma->vm_flags & VM_SHARED) != VM_SHARED)
		pr_info("No shared MAP!!!!!!!!!!!!!!!!!\n");
	else
		pr_info("OK for shared MAP\n");

	phy_pfn = dev->memory->base >> PAGE_SHIFT;

	err = remap_pfn_range(vma, vma->vm_start, phy_pfn, size,
			      vma->vm_page_prot);
	if (err) {
		pr_err("remap_pfn_range failed\n");
		return -EAGAIN;
	}
#endif
	gaddr = vma->vm_start;

	{
		pgd_t *pgd;
		pmd_t *pmd;
		pte_t *pte;
		struct page *page = NULL;
		pud_t *pud;
		struct mm_struct *mm = current->mm;
		unsigned long address = vma->vm_start;

		pr_info("c->mm = %p v->vm_mm = %p\n", current->mm,
		       vma->vm_mm);

		mm = vma->vm_mm;

		pgd = pgd_offset(mm, address);
		if (pgd_none(*pgd))
			pr_info("pgd none!\n");
		if (unlikely(pgd_bad(*pgd)))
			pr_info("pgd bad!\n");

		pr_info("PGD (from pgd_offset): 0x%p\n", pgd);
		pr_info("PGD (from mm )       : 0x%p\n", mm->pgd);

		pud = pud_offset(pgd, address);
		if (pud_none(*pud))
			pr_info("pud none!\n");
		if (unlikely(pud_bad(*pud)))
			pr_info("pud bad!\n");

		pmd = pmd_offset(pud, address);
		if (pmd_none(*pmd))
			pr_info("pmd none!\n");
		if (unlikely(pmd_bad(*pmd)))
			pr_info("pmd bad!\n");

		pte = pte_offset_map(pmd, address);
		if (pte_none(*pte))
			pr_info("pte none!\n");

		pr_info("pte       : 0x%p\n", pte);

		page = pte_page(*pte);
		/* pte_modify(*pte, pgprot_kernel); */

		pr_info("page      : 0x%p\n", page);
		pr_info("page2phys : 0x%llx\n", page_to_phys(page));

		pr_info("PTE: 0x%p\n", pte);
	}

/*	vma->vm_pgoff = vm_pgoff_orig; */
	pr_info("3) pg_off = %lx\n", vma->vm_pgoff);
	return 0;
}
#endif

/**/


static int axiom_mem_dev_open(struct inode *i, struct file *f)
{
	struct mm_struct *mm;
	struct mm_struct *mm2;
	struct fpriv_data_s *pdata;
	int minor = iminor(i);
	int major = imajor(i);

	struct axiom_mem_dev_struct *dev =
		container_of(i->i_cdev, struct axiom_mem_dev_struct, c_dev);

	struct vm_area_struct vma;
	long vmaddr;

	pr_info("Driver: open()\n");
	pr_info("dev %d:%d\n", major, minor);

	pr_info("sizeof(struct page): %ld\n", sizeof(struct page));
	pdata = kmalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

	/* f->private_data = dev; */

	pdata->dev = dev;
	pdata->axiom_app_id = TAG_APP_NONE;
	/* TODO init lists */
	f->private_data = pdata;

	mm = get_task_mm(current);
	if (IS_ERR(mm))
		pr_info("Unable to get mm\n");

	mm2 = current->mm;
	if (mm2)
		pr_info("valid mm2\n");
	else
		pr_info("INvalid mm2\n");

	pr_info("MM: %p   mm2: %p\n", mm, mm2);
	vma.vm_start = 0x4000000000;
	vma.vm_end = 0x4040000000;
	vma.vm_pgoff = 0;
#if 0
	vma.vm_flags = VM_SHARED | VM_READ | VM_WRITE
		       | MAP_SHARED | MAP_LOCKED;
#endif
	vma.vm_flags = MAP_SHARED | MAP_LOCKED | MAP_FIXED;
	vma.vm_mm = mm;

	vma.vm_page_prot = PROT_READ | PROT_WRITE;
	vma.vm_page_prot = PROT_NONE;
	vma.vm_page_prot = pgprot_noncached(vma.vm_page_prot);

	/* force_mmap(f, &vma); */

	mmput(mm);

	pr_info("Mapping vm_flags:%ld\n", vma.vm_flags);
	vmaddr = vm_mmap(f, vma.vm_start, vma.vm_end - vma.vm_start,
			 vma.vm_page_prot, vma.vm_flags, vma.vm_pgoff);
	if (vmaddr < 0)
		pr_err("Unable to vm_mmap\n");
	else
		pr_info("Mapped at %lx\n", vmaddr);

	return 0;
}

static int axiom_mem_dev_close(struct inode *i, struct file *f)
{
	struct fpriv_data_s *pdata = f->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev;
	struct list_elem_s remove;

	pr_info("Driver: close()\n");

	remove.tag = pdata->axiom_app_id;
	remove.start = 0;
	remove.end = LONG_MAX;
	mem_free_space(dev->memory, &remove);
	mem_dump_list(dev->memory);

	dump_mem(dev);

	kfree(pdata);

	return 0;
}

static void print_phys(struct mm_struct *mm)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	struct page *page = NULL;
	pud_t *pud;
	unsigned long address = gaddr;

	pr_info("g address: 0x%lx\n", address);
	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd))
		pr_info("pgd none!\n");
	if (unlikely(pgd_bad(*pgd)))
		pr_info("pgd bad!\n");

	pr_info("PGD (from pgd_offset): 0x%p\n", pgd);
	pr_info("PGD (from mm )       : 0x%p\n", mm->pgd);

	pud = pud_offset(pgd, address);
	if (pud_none(*pud))
		pr_info("pud none!\n");
	if (unlikely(pud_bad(*pud)))
		pr_info("pud bad!\n");

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd))
		pr_info("pmd none!\n");
	if (unlikely(pmd_bad(*pmd)))
		pr_info("pmd bad!\n");

	pte = pte_offset_map(pmd, address);
	if (pte_none(*pte))
		pr_info("pte none!\n");

	pr_info("pte       : 0x%p\n", pte);

	page = pte_page(*pte);
	pr_info("page      : 0x%p\n", page);
	pr_info("page2phys : 0x%llx\n", page_to_phys(page));
	pr_info("PTE: 0x%p\n", pte);
}

static ssize_t axiom_mem_dev_read(struct file *f, char __user *buf, size_t len,
				  loff_t *off)
{
	/* struct fpriv_data_s *pdata = f->private_data;
	struct axiom_mem_dev_struct *dev = pdata->dev; */

	pr_info("Driver: read()\n");

	print_phys(current->mm);

	return -ENOSYS;
}

static ssize_t axiom_mem_dev_write(struct file *f, const char __user *buf,
				   size_t len, loff_t *off)
{
	pr_info("Driver: write()\n");

	return len;
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

		tmp.base = dev->memory->base;
		tmp.size = dev->memory->size;
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

		err = setup_user_vaddr(dev->memory, &tmp);
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

		pr_info("%s] allocate_space for <%ld,%ld> (sz=%ld)\n", __func__,
			tmp.base, tmp.base + tmp.size, tmp.size);
		tmp.base = axiom_v2p(dev->memory, tmp.base);
		pr_info("%s] PHY allocate_space for <%ld,%ld> (sz=%ld)\n", __func__,
			tmp.base, tmp.base + tmp.size, tmp.size);
		err = mem_allocate_space(dev->memory,
				     pdata->axiom_app_id, tmp.base,
				     tmp.base + tmp.size);
		if (err)
			return err;

		mem_dump_list(dev->memory);

		break;
	}
	case AXIOM_MEM_DEV_REVOKE_MEM: {
		struct axiom_mem_dev_info tmp;
		struct list_elem_s rem;

		err = copy_from_user(&tmp, (void __user *)arg, sizeof(tmp));
		if (err)
			return -EFAULT;

		rem.tag = pdata->axiom_app_id;
		rem.start = axiom_v2p(dev->memory, tmp.base);
		rem.end = rem.start + tmp.size;

		pr_info("%s] free_space for <%ld,%ld> (sz=%ld)\n", __func__,
			rem.start, rem.end, tmp.size);

		err = mem_free_space(dev->memory, &rem);
		if (err)
			return err;

		mem_dump_list(dev->memory);

		break;
	}
	case AXIOM_MEM_DEV_SET_APP_ID: {
		int app_id;

		err = copy_from_user(&app_id, (void __user *)arg, sizeof(app_id));
		if (err)
			return -EFAULT;

		if (pdata->axiom_app_id != TAG_APP_NONE)
			return -EINVAL;

		pdata->axiom_app_id = app_id;
		dev_info(dev->dev, "Set app id: %d\n", pdata->axiom_app_id);

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
	unsigned long vm_pgoff_orig;
	unsigned long phy_pfn;
	int err;

	pr_info("Driver: mmap()\n");
	pr_info("1) vma = 0x%lx-0x%lx\n", vma->vm_start, vma->vm_end);

	vm_pgoff_orig = vma->vm_pgoff;
	pr_info("1) pg_off = %lx\n", vma->vm_pgoff);

	/* vma->vm_pgoff = (0x70000000 >> PAGE_SHIFT); */
	pr_info("2) pg_off = %lx\n", vma->vm_pgoff);
#if 0
	if (size != (10 * PAGE_SIZE)) {
		pr_err("%zu != PAGE_SIZE\n", size);
		return -EAGAIN;
	}
#endif
	if (size > dev->memory->size) {
		pr_err("%zu > max size (%zu)\n", size, dev->memory->size);
		return -EAGAIN;
	}

#if 0
	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff, size,
						 vma->vm_page_prot);
	vma->vm_ops = &mmap_mem_ops;
#else
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

	/* Remap-pfn-range will mark the range VM_IO */
#if 1

	pr_info("io_map = %p  pfn = %ld\n", io_map, vmalloc_to_pfn(io_map));
	pr_info("vm_page_prot = 0x%llx", pgprot_val(vma->vm_page_prot));

	pr_info("N:%p P:%p\n", vma->vm_next, vma->vm_prev);
	pr_info("vm_mm:%p\n", vma->vm_mm);

	if ((vma->vm_flags & VM_SHARED) != VM_SHARED)
		pr_info("No shared MAP!!!!!!!!!!!!!!!!!\n");
	else
		pr_info("OK for shared MAP\n");

	phy_pfn = dev->memory->base >> PAGE_SHIFT;
#if 0
	if (remap_pfn_range(vma, vma->vm_start,
			    /*vma->vm_pgoff*/ (0x70000000 >> PAGE_SHIFT)
			    /*vmalloc_to_pfn(io_map)*/, size,
			    vma->vm_page_prot)) {
		pr_err("remap_pfn_range failed\n");
		return -EAGAIN;
	}
#endif

	err = remap_pfn_range(vma, vma->vm_start, phy_pfn, size,
			      vma->vm_page_prot);
	if (err) {
		pr_err("remap_pfn_range failed\n");
		return -EAGAIN;
	}
#endif
	gaddr = vma->vm_start;

	{
		pgd_t *pgd;
		pmd_t *pmd;
		pte_t *pte;
		struct page *page = NULL;
		pud_t *pud;
		struct mm_struct *mm = current->mm;
		unsigned long address = vma->vm_start;

		pr_info("c->mm = %p v->vm_mm = %p\n", current->mm,
		       vma->vm_mm);

		mm = vma->vm_mm;

		pgd = pgd_offset(mm, address);
		if (pgd_none(*pgd))
			pr_info("pgd none!\n");
		if (unlikely(pgd_bad(*pgd)))
			pr_info("pgd bad!\n");

		pr_info("PGD (from pgd_offset): 0x%p\n", pgd);
		pr_info("PGD (from mm )       : 0x%p\n", mm->pgd);

		pud = pud_offset(pgd, address);
		if (pud_none(*pud))
			pr_info("pud none!\n");
		if (unlikely(pud_bad(*pud)))
			pr_info("pud bad!\n");

		pmd = pmd_offset(pud, address);
		if (pmd_none(*pmd))
			pr_info("pmd none!\n");
		if (unlikely(pmd_bad(*pmd)))
			pr_info("pmd bad!\n");

		pte = pte_offset_map(pmd, address);
		if (pte_none(*pte))
			pr_info("pte none!\n");

		pr_info("pte       : 0x%p\n", pte);

		page = pte_page(*pte);
		/* pte_modify(*pte, pgprot_kernel); */
#if 0
	if (remap_pfn_range(vma, vma->vm_start,
			    /*vma->vm_pgoff*/(0x70000000 >> PAGE_SHIFT), size,
			    vma->vm_page_prot)) {
		pr_err("remap_pfn_range failed\n");
		return -EAGAIN;
	}
#endif

		pr_info("page      : 0x%p\n", page);
		pr_info("page2phys : 0x%llx\n", page_to_phys(page));

#if 0
		/* mapping in kernel memory: */
		pr_info("b kmap\n");
		kernel_address = kmap(page);
		pr_info("a kmap\n");
		/* memset((char *)kernel_address + 16, 0xde, 16); */

		pr_info("kmap addr: 0x%p\n", kernel_address);
		pr_info("virt2ph:%llx   vmalloc_addr? %s\n",
		       (unsigned long long)__virt_to_phys(
						(unsigned long)kernel_address),
		       is_vmalloc_addr(kernel_address) ? "yes" : "no");


		pr_info("vmalloc phy address 0x%llx\n",
		       (unsigned long long)
			PFN_PHYS(vmalloc_to_pfn(kernel_address)));

		/* work with kernel_address.... */

		pr_info("b kunmap\n");
		kunmap(page);
		pr_info("a kunmap\n");
#endif

#if 0
		kernel_address = ioremap(page_to_phys(page), size);
		pr_info("ioremap addr: 0x%p\n", kernel_address);
		memset((char *)kernel_address + 16, 0xed, 16);
		/* ((unsigned int *)kernel_address)[16] = 0xdededede; */
		pr_info("vmalloc phy address 0x%llx\n",
		       (unsigned long long)
			PFN_PHYS(vmalloc_to_pfn(kernel_address)));
		iounmap(kernel_address);
#endif

		pr_info("PTE: 0x%p\n", pte);
	}

/*	vma->vm_pgoff = vm_pgoff_orig; */
	pr_info("3) pg_off = %lx\n", vma->vm_pgoff);
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


#define DMA_SIZE (39 * 1024 * 1024 + 256 /*(256 - 256) * 4096*/)
/* #define DMA_SIZE (8000 * 4096) */
dma_addr_t dma_handle;
void *buffer_addr;

static int axiom_mem_dev_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct resource r;
	int ret;
	int ni;
	int major, minor;
	dev_t devt;

	dev_info(dev, "%s M:%d m:%d   p=%p\n", __func__,
		 MAJOR(dev->devt), MINOR(dev->devt), dev);

	/* no device tree device */
	if (!np) {
		pr_err("%s] No device tree found\n", __func__);
		return -ENODEV;
	}

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		dev_err(dev, "No memory-region specified\n");
		return -EINVAL;
	} else {
		dev_info(dev, "of_node_full_name = %s\n", of_node_full_name(np));
	}

	ret = of_address_to_resource(np, 0, &r);
	if (ret) {
		of_node_put(np);
		return ret;
	}

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

#if 0
	axiom_mem_dev[ni].memory->base = (u64)r.start;
	axiom_mem_dev[ni].memory->size = resource_size(&r);
#else
pr_info("%s] memory: %p\n", __func__, &axiom_mem_dev[ni].memory);
	axiom_mem_dev[ni].memory = mem_manager_create(of_node_full_name(np), &r);
	if (axiom_mem_dev[ni].memory == NULL) {
		dev_err(dev, "Unable to create memory handler for %s\n",
			 of_node_full_name(np));
		goto err3;
	}
	dump_mem(&(axiom_mem_dev[ni]));
#endif
	++dev_num_instance;

	of_node_put(np);
	mutex_unlock(&manager_mutex);

	dev_info(dev, "dev%d base = 0x%llx size = %zu\n", ni,
		 axiom_mem_dev[ni].memory->base, axiom_mem_dev[ni].memory->size);


	return 0;

err3:
	device_destroy(axiom_mem_dev_cl, axiom_mem_dev[ni].devt);
err2:
	cdev_del(&axiom_mem_dev[ni].c_dev);

err1:
	of_node_put(np);
	mutex_unlock(&manager_mutex);

	return ret;
}

static int axiom_mem_dev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct axiom_mem_dev_struct *drvdata = platform_get_drvdata(pdev);

	cdev_del(&drvdata->c_dev);
	device_destroy(axiom_mem_dev_cl, drvdata->devt);

	mutex_lock(&manager_mutex);
	--dev_num_instance;
	mutex_unlock(&manager_mutex);

	dev_info(dev, "bye\n");
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

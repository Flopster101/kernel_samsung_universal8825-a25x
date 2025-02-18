#include <linux/kq/kq_mesh_user_nad.h>

static struct kq_mesh_user_nad_info *g_kmn_info;

void kq_mesh_user_nad_init_kmn_info(void *kmn_info)
{
	g_kmn_info = (struct kq_mesh_user_nad_info *)kmn_info;
}

static int kq_mesh_user_nad_init_base(unsigned long long base)
{
	int i;
	struct kq_mesh_user_nad_info *kmn_info = g_kmn_info;

	if (!kmn_info->area_len || kmn_info->v_addr)
		return -EFAULT;

	kmn_info->base = kmn_info->copy_area_address_arr[0];
	kmn_info->size = kmn_info->copy_area_size_arr[0];
	kmn_info->v_offset = 0;
	for (i = 0; i < kmn_info->area_len; i++) {
		if (kmn_info->copy_area_address_arr[i] == base) {
			kmn_info->base = base;
			kmn_info->size = kmn_info->copy_area_size_arr[i];
			break;
		}
	}
	pr_info("%s: %llu, %llu", __func__, kmn_info->base, kmn_info->size);

	return 0;
}

static int kq_mesh_user_nad_alloc_virtual_memory(void)
{
	int i, required_page_len;
	unsigned long long base;
	struct page **pages;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct kq_mesh_user_nad_info *kmn_info = g_kmn_info;

	kmn_info->buf = kvmalloc(sizeof(char) * (KQ_MESH_USER_NAD_BUF_SIZE + 2), GFP_KERNEL);
	if (!kmn_info->buf)
		return -EFAULT;

	base = kmn_info->base;
	required_page_len = (PAGE_ALIGN(kmn_info->size) / PAGE_SIZE);
	pages = kvmalloc(sizeof(struct page *) * required_page_len, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to alloc pages\n", __func__);
		kvfree(kmn_info->buf);
		return -EFAULT;
	}
	pr_info("%s: %d\n", __func__, required_page_len);

	for (i = 0; i < required_page_len; i++) {
		pages[i] = phys_to_page(base);
		base += PAGE_SIZE;
	}

	kmn_info->v_addr = vmap(pages, required_page_len, VM_MAP, prot);
	kvfree(pages);

	if (!kmn_info->v_addr) {
		pr_err("%s: Failed to vmap pages\n", __func__);
		kvfree(kmn_info->buf);
		return -EFAULT;
	}

	return 0;
}

static void kq_mesh_user_nad_free_virtual_memory(void)
{
	struct kq_mesh_user_nad_info *kmn_info = g_kmn_info;

	kmn_info->base = 0;
	kmn_info->size = 0;
	kmn_info->v_offset = 0;

	if (kmn_info->v_addr) {
		vunmap(kmn_info->v_addr);
		kmn_info->v_addr = NULL;
	}

	if (kmn_info->buf) {
		kvfree(kmn_info->buf);
		kmn_info->buf = NULL;
	}
}

static int kq_mesh_user_nad_open(struct inode *inode, struct file *filp)
{
	pr_info("%s ++\n", __func__);
	return 0;
}

static int kq_mesh_user_nad_close(struct inode *inode, struct file *filp)
{
	pr_info("%s --\n", __func__);
	kq_mesh_user_nad_free_virtual_memory();
	return 0;
}

static ssize_t kq_mesh_user_nad_read(struct file *filp, char *buf, size_t size, loff_t *offset)
{
	size_t ret = 0;
	struct kq_mesh_user_nad_info *kmn_info = g_kmn_info;

	if (!kmn_info->v_addr || kmn_info->v_offset + size > kmn_info->size)
		return -EFAULT;

	memcpy(kmn_info->buf, kmn_info->v_addr + kmn_info->v_offset, size);
	ret = copy_to_user(buf, kmn_info->buf, size);

	return !ret ? size : ret;
}

static ssize_t kq_mesh_user_nad_write(struct file *filp, const char *buf, size_t size, loff_t *offset)
{
	size_t ret = 0;
	struct kq_mesh_user_nad_info *kmn_info = g_kmn_info;

	if (!kmn_info->v_addr || kmn_info->v_offset + size > kmn_info->size)
		return -EFAULT;

	ret = copy_from_user(kmn_info->buf, buf, size);
	memcpy(kmn_info->v_addr + kmn_info->v_offset, kmn_info->buf, size);

	return !ret ? size : ret;
}

static long kq_mesh_user_nad_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long long data = 0;

	switch (cmd) {
	case KQ_MESH_USER_NAD_ALLOC_VMAP:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		pr_info("%s: alloc vmap base %llu\n", __func__, data);
		if (kq_mesh_user_nad_init_base(data))
			return -EFAULT;
		return kq_mesh_user_nad_alloc_virtual_memory();
	case KQ_MESH_USER_NAD_FREE_VMAP:
		pr_info("%s: free vmap\n", __func__);
		kq_mesh_user_nad_free_virtual_memory();
		break;
	case KQ_MESH_USER_NAD_SET_OFFSET:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		g_kmn_info->v_offset += data;
		pr_info("%s: offset %llu\n", __func__, g_kmn_info->v_offset);
		break;
	default:
		pr_info("%s: cmd doesn't defined\n", __func__);
		break;
	}

	return 0;
}

const struct file_operations kq_mesh_user_nad_fops = {
	.owner           = THIS_MODULE,
	.read            = kq_mesh_user_nad_read,
	.write           = kq_mesh_user_nad_write,
	.unlocked_ioctl  = kq_mesh_user_nad_ioctl,
	.open            = kq_mesh_user_nad_open,
	.release         = kq_mesh_user_nad_close,
};

int kq_mesh_user_nad_create_devfs(void)
{
	int ret;
	struct kq_mesh_user_nad_info *kmn_info = g_kmn_info;

	ret = alloc_chrdev_region(&kmn_info->id, 0, 1, KQ_MESH_USER_NAD_DEVICE_NAME);
	if (ret) {
		pr_err("%s alloc_chrdev_region error %d\n", __func__, ret);
		return ret;
	}

	cdev_init(&kmn_info->cdev, &kq_mesh_user_nad_fops);
	kmn_info->cdev.owner = THIS_MODULE;

	ret = cdev_add(&kmn_info->cdev, kmn_info->id, 1);
	if (ret) {
		pr_err("%s cdev_add error %d\n", __func__, ret);
		goto error_mesh_add_cdev;
	}

	kmn_info->class = class_create(THIS_MODULE, KQ_MESH_USER_NAD_DEVICE_NAME);
	if (IS_ERR(kmn_info->class)) {
		ret = PTR_ERR(kmn_info->class);
		pr_err("%s class_create error %d\n", __func__, ret);
		goto error_mesh_create_class;
	}

	kmn_info->dev = device_create(kmn_info->class, NULL,
	kmn_info->id, NULL, KQ_MESH_USER_NAD_DEVICE_NAME);
	if (IS_ERR(kmn_info->dev)) {
		ret = PTR_ERR(kmn_info->dev);
		pr_err("%s device_create error %d\n", __func__, ret);
		goto error_mesh_create_device;
	}

	return 0;

error_mesh_create_device:
	class_destroy(kmn_info->class);
error_mesh_create_class:
	cdev_del(&kmn_info->cdev);
error_mesh_add_cdev:
	unregister_chrdev_region(kmn_info->id, 1);

	return ret;
}

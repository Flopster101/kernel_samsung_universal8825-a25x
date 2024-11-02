/*
* @file sgpu_dmsg.h
* @copyright 2021 Samsung Electronics
*/
#include "amdgpu.h"
#include "sgpu_dmsg.h"

void sgpu_dmsg_log(struct amdgpu_device *adev, const char *caller, int func,
		   ktime_t time, int index, const char *fmt, ...)
{
	const char *func_name;

	if (!adev || !adev->sgpu_dmsg)
		return;

	switch (func) {
	case DMSG_DVFS:
		func_name = "DVFS";
		break;
	case DMSG_POWER:
		func_name = "POWER";
		break;
	case DMSG_MEMORY:
		func_name = "MEMORY";
		break;
	case DMSG_ETC:
		func_name = "ETC";
		break;
	default:
		func_name = "Unknown";
	}

	if (fmt) {
		struct va_format vaf;
		va_list args;

		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;

		scnprintf(adev->sgpu_dmsg->log[index], SGPU_DMSG_LOG_LENGTH,
			  "%5lld.%09lld  %4d %-8s  %-35s  %pV\n",
			  time / NSEC_PER_SEC, time % NSEC_PER_SEC, current->pid,
			  func_name, caller, &vaf);

		va_end(args);
	}
}

static int sgpu_debugfs_dmsg_log_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)m->private;
	int i, index;
	int64_t start, end;

	if (!adev || !adev->sgpu_dmsg)
		return -EINVAL;

	start = 1;
	end = atomic64_read(&adev->sgpu_dmsg_index);
	/* circular buffer */
	if (end >= SGPU_DMSG_ENTITY_LENGTH) {
		start = end+1;
		end += SGPU_DMSG_ENTITY_LENGTH;
	}

	for (i = start; i <= end; i++) {
		index = i % SGPU_DMSG_ENTITY_LENGTH;
		seq_printf(m, "%s", adev->sgpu_dmsg->log[index]);
	}

	return 0;
}

static int sgpu_debugfs_dmsg_log_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_debugfs_dmsg_log_show, inode->i_private);
}

static int sgpu_debugfs_dmsg_level_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)m->private;
	unsigned int level = adev->sgpu_dmsg_level;

	seq_printf(m, "---------------------------------\n");
	seq_printf(m, "sgpu_dmsg_level :              %2u\n", level);
	seq_printf(m, "---------------------------------\n");
	seq_printf(m, "DMSG_LEVEL_MIN                  0\n");
	seq_printf(m, "DMSG_DEBUG                      0\n");
	seq_printf(m, "DMSG_INFO                       1\n");
	seq_printf(m, "DMSG_WARNING                    2\n");
	seq_printf(m, "DMSG_ERROR                      3\n");
	seq_printf(m, "DMSG_LEVEL_MAX                  4\n");
	seq_printf(m, "---------------------------------\n");

	return 0;
}

static int sgpu_debugfs_dmsg_level_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_debugfs_dmsg_level_show, inode->i_private);
}

static ssize_t sgpu_debugfs_dmsg_level_write(struct file *f,
					     const char __user *data, size_t len,
					     loff_t *loff)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	char buf[128];
	size_t size;
	int level, r;

	size = min(sizeof(buf) - 1, len);
	if (copy_from_user(&buf, data, size))
		return -EFAULT;

	buf[size] = '\0';
	r = kstrtou32(buf, 0, &level);
	if (r)
		return -EFAULT;

	if (level <= DMSG_LEVEL_MAX)
		adev->sgpu_dmsg_level = level;
	else
		adev->sgpu_dmsg_level = DMSG_LEVEL_MAX;

	return len;
}

static int sgpu_debugfs_dmsg_funcmask_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)m->private;
	unsigned int mask = adev->sgpu_dmsg_funcmask;

	seq_printf(m, "---------------------------------\n");
	seq_printf(m, "sgpu_dmsg_funcmask :          0x%x\n", mask);
	seq_printf(m, "---------------------------------\n");
	seq_printf(m, "DMSG_DVFS                  (1<<0)\n");
	seq_printf(m, "DMSG_POWER                 (1<<1)\n");
	seq_printf(m, "DMSG_MEMORY                (1<<2)\n");
	seq_printf(m, "DMSG_ETC                   (1<<3)\n");
	seq_printf(m, "---------------------------------\n");

	return 0;
}

static int sgpu_debugfs_dmsg_funcmask_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_debugfs_dmsg_funcmask_show, inode->i_private);
}

static ssize_t sgpu_debugfs_dmsg_funcmask_write(struct file *f,
					    const char __user *data, size_t len,
					    loff_t *loff)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	char buf[128];
	size_t size;
	int funcmask, r;

	size = min(sizeof(buf) - 1, len);
	if (copy_from_user(&buf, data, size))
		return -EFAULT;

	buf[size] = '\0';
	r = kstrtou32(buf, 16, &funcmask);
	if (r)
		return -EFAULT;

	adev->sgpu_dmsg_funcmask = funcmask & DMSG_FUNCMASK;

	return len;
}

static const struct file_operations sgpu_debugfs_dmsg_log_fops = {
	.open    = sgpu_debugfs_dmsg_log_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static const struct file_operations sgpu_debugfs_dmsg_level_fops = {
	.open    = sgpu_debugfs_dmsg_level_open,
	.read    = seq_read,
	.write   = sgpu_debugfs_dmsg_level_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static const struct file_operations sgpu_debugfs_dmsg_funcmask_fops = {
	.open    = sgpu_debugfs_dmsg_funcmask_open,
	.read    = seq_read,
	.write   = sgpu_debugfs_dmsg_funcmask_write,
	.llseek  = seq_lseek,
	.release = single_release
};

int sgpu_debugfs_dmsg_init(struct amdgpu_device *adev)
{
	adev->debugfs_dmsg_log =
		debugfs_create_file("sgpu_dmsg_log", 0444,
				    adev_to_drm(adev)->primary->debugfs_root,
				    (void *)adev, &sgpu_debugfs_dmsg_log_fops);

	if (!adev->debugfs_dmsg_log) {
		DRM_ERROR("unable to create sgpu_dmsg_log debugfs file\n");
		return -EIO;
	}

	adev->debugfs_dmsg_level =
		debugfs_create_file("sgpu_dmsg_level", 0644,
				    adev_to_drm(adev)->primary->debugfs_root,
				    (void *)adev, &sgpu_debugfs_dmsg_level_fops);

	if (!adev->debugfs_dmsg_level) {
		DRM_ERROR("unable to create sgpu_dmsg_level debugfs file\n");
		return -EIO;
	}

	adev->debugfs_dmsg_funcmask =
		debugfs_create_file("sgpu_dmsg_funcmask", 0644,
				    adev_to_drm(adev)->primary->debugfs_root,
				    (void *)adev, &sgpu_debugfs_dmsg_funcmask_fops);

	if (!adev->debugfs_dmsg_funcmask) {
		DRM_ERROR("unable to create sgpu_dmsg_funcmask debugfs file\n");
		return -EIO;
	}

	return 0;
}

void sgpu_debugfs_dmsg_fini(struct amdgpu_device *adev)
{
	debugfs_remove(adev->debugfs_dmsg_log);
	debugfs_remove(adev->debugfs_dmsg_level);
	debugfs_remove(adev->debugfs_dmsg_funcmask);
}

int sgpu_dmsg_init(struct amdgpu_device *adev)
{
	if (!adev)
		return -EINVAL;

	adev->sgpu_dmsg = kvzalloc(sizeof(struct sgpu_dmsg), GFP_KERNEL);

	if (!adev->sgpu_dmsg) {
		DRM_ERROR("SGPU_DMSG Circular Buffer doesn't allocates.\n");
		return -ENOMEM;
	}

	atomic64_set(&adev->sgpu_dmsg_index, 0);
	adev->sgpu_dmsg_level = DMSG_LEVEL_MIN;
	adev->sgpu_dmsg_funcmask = DMSG_FUNCMASK;

	return 0;
}

void sgpu_dmsg_fini(struct amdgpu_device *adev)
{
	if (!adev || !adev->sgpu_dmsg)
		return;

	kfree(adev->sgpu_dmsg);
}

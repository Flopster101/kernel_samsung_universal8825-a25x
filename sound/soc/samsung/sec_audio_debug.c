/*
 *  sec_audio_debug.c
 *
 *  Copyright (c) 2018 Samsung Electronics
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <linux/sec_debug.h>

#include <linux/sched/clock.h>

#include <sound/soc.h>
#include <sound/samsung/sec_audio_debug.h>

#include <linux/sec_debug.h>

#define DBG_STR_BUFF_SZ 256
#define LOG_MSG_BUFF_SZ 512

#define SEC_AUDIO_DEBUG_STRING_WQ_NAME "sec_audio_dbg_str_wq"

#define MAX_DBG_PARAM 4
#define MAX_ERR_STR_LEN 20

char abox_err_s[MAX_ERR_STR_LEN][TYPE_ABOX_DEBUG_MAX + 1] = {
    [TYPE_ABOX_NOERROR] = "NOERR",
    [TYPE_ABOX_DATAABORT] = "DABRT",
    [TYPE_ABOX_PREFETCHABORT] = "PABRT",
    [TYPE_ABOX_OSERROR] = "OSERR",
    [TYPE_ABOX_VSSERROR] = "VSERR",
    [TYPE_ABOX_UNDEFEXCEPTION] = "UNDEF",
    [TYPE_ABOX_DEBUGASSERT] = "ASSRT",
    [TYPE_ABOX_DEBUG_MAX] = "UNKWN",
};

static size_t rmem_size_min[TYPE_SIZE_MAX] = {0xab0cab0c, 0xab0cab0c};

struct device *debug_dev;

struct modem_status {
	int event;
	unsigned long long time;
	unsigned long nanosec_t;
};

struct audio_mode_status {
	int audio_mode;
	unsigned long long time;
	unsigned long nanosec_t;
};

struct sec_audio_debug_data {
	char *dbg_str_buf;
	struct mutex dbg_lock;
	struct workqueue_struct *debug_string_wq;
	struct work_struct debug_string_work;
	enum abox_debug_err_type debug_err_type;
	int param[MAX_DBG_PARAM];
	struct audio_mode_status audio_mode_state;
	struct modem_status modem_state;
};

static struct sec_audio_debug_data *p_debug_data;

static struct proc_dir_entry *audio_procfs;
static struct dentry *audio_debugfs_link;
static struct sec_audio_log_data *p_debug_log_data;
static struct sec_audio_log_data *p_debug_bootlog_data;
static struct sec_audio_log_data *p_debug_pmlog_data;
static unsigned int debug_buff_switch;

int aboxlog_file_opened;
static DEFINE_MUTEX(aboxlog_file_lock);
int half2_buff_use;
#define ABOXLOG_BUFF_SIZE SZ_2M

ssize_t aboxlog_file_index;
static DEFINE_MUTEX(aboxlog_file_index_lock);
struct abox_log_kernel_buffer {
	char *buffer;
	unsigned int index;
	bool wrap;
	bool updated;
	struct mutex abox_log_lock;
};
static struct abox_log_kernel_buffer *p_debug_aboxlog_data;

static int read_half_buff_id;

static void send_half_buff_full_event(int buffer_id)
{
	char env[32] = {0,};
	char *envp[2] = {env, NULL};

	if (debug_dev == NULL) {
		pr_err("%s: no debug_dev\n", __func__);
		return;
	}

	read_half_buff_id = buffer_id;
	snprintf(env, sizeof(env), "ABOX_HALF_BUFF_ID=%d", buffer_id);
	kobject_uevent_env(&debug_dev->kobj, KOBJ_CHANGE, envp);
/*	pr_info("%s: env %s\n", __func__, env); */
}

static void abox_log_copy(const char *log_src, size_t copy_size)
{
	size_t left_size = 0;

	if (copy_size > ABOXLOG_BUFF_SIZE) {
		pr_err("%s copy_size(%u) exceeds buffer.\n",
			__func__, copy_size);
		return;
	}

	while ((left_size = (ABOXLOG_BUFF_SIZE -
			p_debug_aboxlog_data->index)) < copy_size) {
		memcpy(p_debug_aboxlog_data->buffer +
			p_debug_aboxlog_data->index, log_src, left_size);

		log_src += left_size;
		copy_size -= left_size;
		p_debug_aboxlog_data->index = 0;
		p_debug_aboxlog_data->wrap = true;
		//pr_info("%s: total buff full\n", __func__);
		half2_buff_use = 0;
		send_half_buff_full_event(1);
	}

	memcpy(p_debug_aboxlog_data->buffer + p_debug_aboxlog_data->index,
			log_src, copy_size);
	p_debug_aboxlog_data->index += (unsigned int)copy_size;

	if ((half2_buff_use == 0) &&
		(p_debug_aboxlog_data->index > (ABOXLOG_BUFF_SIZE / 2))) {
		//pr_info("%s: half buff full use 2nd half buff\n", __func__);
		half2_buff_use = 1;
		send_half_buff_full_event(0);
	}
}

void abox_log_extra_copy(char *src_base, unsigned int index_reader,
		unsigned int index_writer, unsigned int src_buff_size)
{
	mutex_lock(&p_debug_aboxlog_data->abox_log_lock);
	if (index_reader >= src_buff_size ||
		index_writer >= src_buff_size) {
		pr_err("%s(%pK,%u,%u,%u) index exceeds buffer size.\n",
			__func__, (void *) src_base, index_reader,
			index_writer, src_buff_size);
		mutex_unlock(&p_debug_aboxlog_data->abox_log_lock);
		return;
	}

	if (index_reader > index_writer) {
		abox_log_copy(src_base + index_reader,
				src_buff_size - index_reader);
		index_reader = 0;
	}
	abox_log_copy(src_base + index_reader,
			index_writer - index_reader);

	mutex_unlock(&p_debug_aboxlog_data->abox_log_lock);
}
EXPORT_SYMBOL_GPL(abox_log_extra_copy);

void set_modem_event(int event)
{
	if (!p_debug_data)
		return;

	p_debug_data->modem_state.event = event;
	p_debug_data->modem_state.time = local_clock();
	p_debug_data->modem_state.nanosec_t = do_div(p_debug_data->modem_state.time, 1000000000);
}
EXPORT_SYMBOL_GPL(set_modem_event);

static void abox_debug_string_update_workfunc(struct work_struct *wk)
{
	unsigned int len = 0;
	int i = 0;
	unsigned long long time = local_clock();
	unsigned long nanosec_t = do_div(time, 1000000000);
/*
	struct sec_audio_debug_data *dbg_data = container_of(wk,
					   struct sec_audio_debug_data, debug_string_work);
*/

	if (!p_debug_data)
		return;

	mutex_lock(&p_debug_data->dbg_lock);

	p_debug_data->dbg_str_buf = kzalloc(sizeof(char) * DBG_STR_BUFF_SZ, GFP_KERNEL);
	if (!p_debug_data->dbg_str_buf) {
		pr_err("%s: no memory\n", __func__);
		mutex_unlock(&p_debug_data->dbg_lock);
		return;
	}

	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
			"[%lu.%06lu] ", (unsigned long) time, nanosec_t / 1000);
	if (len >= (DBG_STR_BUFF_SZ - 1))
		goto buff_done;

	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
			"%s ", abox_err_s[p_debug_data->debug_err_type]);
	if (len >= (DBG_STR_BUFF_SZ - 1))
		goto buff_done;

	for (i = 0; i < MAX_DBG_PARAM; i++) {
		len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
				"%#x ", p_debug_data->param[i]);
		if (len >= (DBG_STR_BUFF_SZ - 1))
			goto buff_done;
	}

	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
					"mode %d:%lu.%06lu ", p_debug_data->audio_mode_state.audio_mode,
					(unsigned long) p_debug_data->audio_mode_state.time,
					p_debug_data->audio_mode_state.nanosec_t / 1000);
	if (len >= (DBG_STR_BUFF_SZ - 1))
		goto buff_done;

	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
					"modem ev %d:%lu.%06lu", p_debug_data->modem_state.event,
					(unsigned long) p_debug_data->modem_state.time,
					p_debug_data->modem_state.nanosec_t / 1000);
	if (len >= (DBG_STR_BUFF_SZ - 1))
		goto buff_done;

buff_done:
	if (len >= (DBG_STR_BUFF_SZ - 1))
		len = (DBG_STR_BUFF_SZ - 1);
	p_debug_data->dbg_str_buf[len] = 0;
	pr_info("%s: %s\n", __func__, p_debug_data->dbg_str_buf);
	secdbg_exin_set_aud(p_debug_data->dbg_str_buf);

	kfree(p_debug_data->dbg_str_buf);
	p_debug_data->dbg_str_buf = NULL;
	mutex_unlock(&p_debug_data->dbg_lock);
}

void abox_debug_string_update(enum abox_debug_err_type type, int p1, int p2, int p3,
				int audio_mode, unsigned long long audio_mode_time)
{
	if (!p_debug_data)
		return;

	if (type > TYPE_ABOX_DEBUG_MAX)
		type = TYPE_ABOX_DEBUG_MAX;

	p_debug_data->debug_err_type = type;
	p_debug_data->param[0] = type;
	p_debug_data->param[1] = p1;
	p_debug_data->param[2] = p2;
	p_debug_data->param[3] = p3;

	p_debug_data->audio_mode_state.audio_mode = audio_mode;
	p_debug_data->audio_mode_state.time = audio_mode_time;
	p_debug_data->audio_mode_state.nanosec_t = do_div(p_debug_data->audio_mode_state.time, 1000000000);

	queue_work(p_debug_data->debug_string_wq,
			&p_debug_data->debug_string_work);
}
EXPORT_SYMBOL_GPL(abox_debug_string_update);

void adev_err(struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];

	va_start(args, fmt);
	vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	dev_err(dev, "%s", temp_buf);
	sec_audio_log(3, dev, "%s", temp_buf);
}

void adev_warn(struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];

	va_start(args, fmt);
	vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	dev_warn(dev, "%s", temp_buf);
	sec_audio_log(4, dev, "%s", temp_buf);
}

void adev_info(struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];

	va_start(args, fmt);
	vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	dev_info(dev, "%s", temp_buf);
	sec_audio_log(6, dev, "%s", temp_buf);
}

void adev_dbg(struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];

	va_start(args, fmt);
	vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	dev_dbg(dev, "%s", temp_buf);
	sec_audio_log(7, dev, "%s", temp_buf);
}

static int get_debug_buffer_switch(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = debug_buff_switch;

	return 0;
}

static int set_debug_buffer_switch(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	int ret = 0;
	struct snd_soc_card *card = (struct snd_soc_card *)snd_kcontrol_chip(
					kcontrol);

	val = (unsigned int)ucontrol->value.integer.value[0];

	if (val) {
		ret = alloc_sec_audio_log(p_debug_log_data, SZ_1M);
		debug_buff_switch = SZ_1M;
	} else {
		ret = alloc_sec_audio_log(p_debug_log_data, 0);
		debug_buff_switch = 0;
	}
	if (ret < 0)
		dev_err(card->dev, "%s: allocation failed at sec_audio_log\n",
			__func__);

	return ret;
}

static const struct snd_kcontrol_new debug_controls[] = {
	SOC_SINGLE_BOOL_EXT("Debug Buffer Switch", 0,
			get_debug_buffer_switch,
			set_debug_buffer_switch),
};

int register_debug_mixer(struct snd_soc_card *card)
{
	return snd_soc_add_card_controls(card, debug_controls,
				ARRAY_SIZE(debug_controls));
}

static void free_sec_audio_log(struct sec_audio_log_data *p_dbg_log_data)
{
	p_dbg_log_data->sz_log_buff = 0;
	if (p_dbg_log_data->virtual)
		vfree(p_dbg_log_data->audio_log_buffer);
	else
		kfree(p_dbg_log_data->audio_log_buffer);
	p_dbg_log_data->audio_log_buffer = NULL;
}

int alloc_sec_audio_log(struct sec_audio_log_data *p_dbg_log_data,
		size_t buffer_len)
{
	if (p_dbg_log_data->sz_log_buff) {
		p_dbg_log_data->sz_log_buff = 0;
		free_sec_audio_log(p_dbg_log_data);
	}

	p_dbg_log_data->buff_idx = 0;
	p_dbg_log_data->full = 0;

	if (buffer_len <= 0) {
		pr_err("%s: Invalid buffer_len for %s %d\n", __func__,
				p_dbg_log_data->name, (int) buffer_len);
		p_dbg_log_data->sz_log_buff = 0;
		return 0;
	}

	if (p_dbg_log_data->virtual)
		p_dbg_log_data->audio_log_buffer = vzalloc(buffer_len);
	else
		p_dbg_log_data->audio_log_buffer = kzalloc(buffer_len,
								GFP_KERNEL);
	if (p_dbg_log_data->audio_log_buffer == NULL) {
		p_dbg_log_data->sz_log_buff = 0;
		return -ENOMEM;
	}

	p_dbg_log_data->sz_log_buff = buffer_len;

	return p_dbg_log_data->sz_log_buff;
}

static ssize_t make_prefix_msg(char *buff, int level, struct device *dev)
{
	unsigned long long time = local_clock();
	unsigned long nanosec_t = do_div(time, 1000000000);
	ssize_t msg_size = 0;

	msg_size = scnprintf(buff, LOG_MSG_BUFF_SZ, "<%d> [%5lu.%06lu] %s %s: ",
			level, (unsigned long) time, nanosec_t / 1000,
			(dev) ? dev_driver_string(dev) : "NULL",
			(dev) ? dev_name(dev) : "NULL");
	return msg_size;
}

static void copy_msgs(char *buff, struct sec_audio_log_data *p_dbg_log_data)
{
	if (p_dbg_log_data->buff_idx + strlen(buff) >
			p_dbg_log_data->sz_log_buff - 1) {
		p_dbg_log_data->full = 1;
		p_dbg_log_data->buff_idx = 0;
	}
	p_dbg_log_data->buff_idx +=
		scnprintf(p_dbg_log_data->audio_log_buffer +
				p_dbg_log_data->buff_idx,
				(strlen(buff) + 1), "%s", buff);
}

void sec_audio_log(int level, struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];
	ssize_t temp_buff_idx = 0;
	struct sec_audio_log_data *p_dbg_log_data = p_debug_log_data;

	if (!p_dbg_log_data->sz_log_buff)
		return;

	temp_buff_idx = make_prefix_msg(temp_buf, level, dev);

	va_start(args, fmt);
	temp_buff_idx +=
		vsnprintf(temp_buf + temp_buff_idx,
				LOG_MSG_BUFF_SZ - temp_buff_idx, fmt, args);
	va_end(args);

	copy_msgs(temp_buf, p_dbg_log_data);
}

void sec_audio_bootlog(int level, struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];
	ssize_t temp_buff_idx = 0;
	struct sec_audio_log_data *p_dbg_log_data = p_debug_bootlog_data;

	if (!p_dbg_log_data->sz_log_buff)
		return;

	temp_buff_idx = make_prefix_msg(temp_buf, level, dev);

	va_start(args, fmt);
	temp_buff_idx +=
		vsnprintf(temp_buf + temp_buff_idx,
				LOG_MSG_BUFF_SZ - (temp_buff_idx + 1),
				fmt, args);
	va_end(args);

	copy_msgs(temp_buf, p_dbg_log_data);
}

void sec_audio_pmlog(int level, struct device *dev, const char *fmt, ...)
{
	va_list args;
	char temp_buf[LOG_MSG_BUFF_SZ];
	ssize_t temp_buff_idx = 0;
	struct sec_audio_log_data *p_dbg_log_data = p_debug_pmlog_data;

	if (!p_dbg_log_data->sz_log_buff)
		return;

	temp_buff_idx = make_prefix_msg(temp_buf, level, dev);

	va_start(args, fmt);
	temp_buff_idx +=
		vsnprintf(temp_buf + temp_buff_idx,
				LOG_MSG_BUFF_SZ - (temp_buff_idx + 1),
				fmt, args);
	va_end(args);

	copy_msgs(temp_buf, p_dbg_log_data);
}

static int aboxhalflog_file_open(struct inode *inode, struct  file *file)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&aboxlog_file_lock);
	if (aboxlog_file_opened) {
		pr_err("%s: already opened\n", __func__);
		mutex_unlock(&aboxlog_file_lock);
		return -EBUSY;
	}

	aboxlog_file_opened = 1;
	mutex_unlock(&aboxlog_file_lock);

	mutex_lock(&aboxlog_file_index_lock);
	if (read_half_buff_id == 0)
		aboxlog_file_index = 0;
	else
		aboxlog_file_index = ABOXLOG_BUFF_SIZE / 2;
	mutex_unlock(&aboxlog_file_index_lock);

	return 0;
}

static int aboxhalflog_file_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&aboxlog_file_lock);
	aboxlog_file_opened = 0;
	mutex_unlock(&aboxlog_file_lock);

	return 0;
}

static ssize_t aboxhalflog_file_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	size_t end, copy_len = 0;
	ssize_t ret;

	pr_debug("%s(%zu, %lld)\n", __func__, count, *ppos);

	if (read_half_buff_id == 0)
		end = (ABOXLOG_BUFF_SIZE / 2) - 1;
	else
		end = ABOXLOG_BUFF_SIZE - 1;

	mutex_lock(&aboxlog_file_index_lock);
	if (aboxlog_file_index > end) {
		pr_err("%s: read done\n", __func__);
		mutex_unlock(&aboxlog_file_index_lock);
		return copy_len;
	}

	copy_len = min(count, end - aboxlog_file_index);

	ret = copy_to_user(user_buf,
			p_debug_aboxlog_data->buffer + aboxlog_file_index,
			copy_len);
	if (ret) {
		pr_err("%s: copy fail %d\n", __func__, (int) ret);
		mutex_unlock(&aboxlog_file_index_lock);
		return -EFAULT;
	}
	aboxlog_file_index += copy_len;
	mutex_unlock(&aboxlog_file_index_lock);

	return copy_len;
}

static ssize_t aboxhalflog_file_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[16];
	size_t size;
	int value;

	size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, size)) {
		pr_err("%s: copy_from_user err\n", __func__);
		return -EFAULT;
	}
	buf[size] = 0;

	if (kstrtoint(buf, 10, &value)) {
		pr_err("%s: Invalid value\n", __func__);
		return -EINVAL;
	}
	read_half_buff_id = value;

	return size;
}

static const struct proc_ops aboxhalflog_fops = {
	.proc_open = aboxhalflog_file_open,
	.proc_release = aboxhalflog_file_release,
	.proc_read = aboxhalflog_file_read,
	.proc_write = aboxhalflog_file_write,
	.proc_lseek = generic_file_llseek,
};

int check_upload_mode_disabled(void)
{
	int val = secdbg_mode_enter_upload();

	if (val == 0x5)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL_GPL(check_upload_mode_disabled);

int __read_mostly debug_level;
module_param(debug_level, int, 0440);

int check_debug_level_low(void)
{
	int debug_level_low = 0;
	pr_info("%s: 0x%x\n", __func__, debug_level);

	if (debug_level == 0x4f4c) {
		debug_level_low = 1;
	} else {
		debug_level_low = 0;
	}

	return debug_level_low;
}
EXPORT_SYMBOL_GPL(check_debug_level_low);

size_t get_rmem_size_min(enum rmem_size_type id)
{
	return rmem_size_min[id];
}
EXPORT_SYMBOL_GPL(get_rmem_size_min);

static int sec_audio_debug_probe(struct platform_device *pdev)
{
	struct sec_audio_debug_data *data;
	struct sec_audio_log_data *log_data;
	struct sec_audio_log_data *bootlog_data;
	struct sec_audio_log_data *pmlog_data;
	struct abox_log_kernel_buffer *aboxlog_data;
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	unsigned int val = 0;

	ret = of_property_read_u32(np, "abox_dbg_size_min", &val);
	if (ret < 0)
		dev_err(dev, "%s: failed to get abox_dbg_size_min %d\n", __func__, ret);
	else
		rmem_size_min[TYPE_ABOX_DBG_SIZE] = (size_t) val;

	ret = of_property_read_u32(np, "abox_slog_size_min", &val);
	if (ret < 0)
		dev_err(dev, "%s: failed to get abox_slog_size_min %d\n", __func__, ret);
	else
		rmem_size_min[TYPE_ABOX_SLOG_SIZE] = (size_t) val;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	p_debug_data = data;
	mutex_init(&p_debug_data->dbg_lock);

	audio_procfs = proc_mkdir("audio", NULL);
	if (!audio_procfs) {
		pr_err("Failed to create audio procfs\n");
		ret = -EPERM;
		goto err_data;
	}

	audio_debugfs_link = debugfs_create_symlink("audio", NULL,
			"/proc/audio");
	if (!audio_debugfs_link) {
		pr_err("Failed to create audio debugfs link\n");
		ret = -EPERM;
		goto err_data;
	}

	log_data = kzalloc(sizeof(*log_data), GFP_KERNEL);
	if (!log_data) {
		ret = -ENOMEM;
		goto err_procfs;
	}
	p_debug_log_data = log_data;
/*
 *	p_debug_log_data->audio_log_buffer = NULL;
 *	p_debug_log_data->buff_idx = 0;
 *	p_debug_log_data->full = 0;
 *	p_debug_log_data->read_idx = 0;
 *	p_debug_log_data->sz_log_buff = 0;
 */
	p_debug_log_data->virtual = 1;
	p_debug_log_data->name = kasprintf(GFP_KERNEL, "runtime");

	bootlog_data = kzalloc(sizeof(*bootlog_data), GFP_KERNEL);
	if (!bootlog_data) {
		ret = -ENOMEM;
		goto err_log_data;
	}
	p_debug_bootlog_data = bootlog_data;
	p_debug_bootlog_data->name = kasprintf(GFP_KERNEL, "boot");

	pmlog_data = kzalloc(sizeof(*pmlog_data), GFP_KERNEL);
	if (!pmlog_data) {
		ret = -ENOMEM;
		goto err_bootlog_data;
	}

	p_debug_pmlog_data = pmlog_data;
	p_debug_pmlog_data->name = kasprintf(GFP_KERNEL, "pm");

	alloc_sec_audio_log(p_debug_bootlog_data, SZ_1K);
	alloc_sec_audio_log(p_debug_pmlog_data, SZ_4K);

	aboxlog_data = kzalloc(sizeof(*aboxlog_data), GFP_KERNEL);
	if (!aboxlog_data) {
		ret = -ENOMEM;
		goto err_pmlog_data;
	}

	p_debug_aboxlog_data = aboxlog_data;
	p_debug_aboxlog_data->buffer = vzalloc(ABOXLOG_BUFF_SIZE);
	p_debug_aboxlog_data->index = 0;
	p_debug_aboxlog_data->wrap = false;
	mutex_init(&p_debug_aboxlog_data->abox_log_lock);

	p_debug_data->debug_string_wq = create_singlethread_workqueue(
						SEC_AUDIO_DEBUG_STRING_WQ_NAME);
	if (p_debug_data->debug_string_wq == NULL) {
		pr_err("%s: failed to creat debug_string_wq\n", __func__);
		ret = -ENOENT;
		goto err_aboxlog_data;
	}

	INIT_WORK(&p_debug_data->debug_string_work,
			abox_debug_string_update_workfunc);

	proc_create_data("aboxhalflog", 0660,
			audio_procfs, &aboxhalflog_fops, NULL);

	debug_dev = &pdev->dev;

	return 0;

err_aboxlog_data:
	mutex_destroy(&p_debug_aboxlog_data->abox_log_lock);
	kfree_const(p_debug_aboxlog_data->buffer);
	kfree(p_debug_aboxlog_data);
	p_debug_aboxlog_data = NULL;

err_pmlog_data:
	if (p_debug_pmlog_data->sz_log_buff)
		free_sec_audio_log(p_debug_pmlog_data);
	kfree_const(p_debug_pmlog_data->name);
	kfree(p_debug_pmlog_data);
	p_debug_pmlog_data = NULL;

err_bootlog_data:
	if (p_debug_bootlog_data->sz_log_buff)
		free_sec_audio_log(p_debug_bootlog_data);
	kfree_const(p_debug_bootlog_data->name);
	kfree(p_debug_bootlog_data);
	p_debug_bootlog_data = NULL;

err_log_data:
	kfree_const(p_debug_log_data->name);
	kfree(p_debug_log_data);
	p_debug_log_data = NULL;

err_procfs:
	proc_remove(audio_procfs);

err_data:
	mutex_destroy(&p_debug_data->dbg_lock);
	kfree(p_debug_data);
	p_debug_data = NULL;

	return ret;
}

static int sec_audio_debug_remove(struct platform_device *pdev)
{
	mutex_destroy(&p_debug_aboxlog_data->abox_log_lock);
	kfree_const(p_debug_aboxlog_data->buffer);
	kfree(p_debug_aboxlog_data);
	p_debug_aboxlog_data = NULL;

	if (p_debug_pmlog_data->sz_log_buff)
		free_sec_audio_log(p_debug_pmlog_data);
	kfree_const(p_debug_pmlog_data->name);
	kfree(p_debug_pmlog_data);
	p_debug_pmlog_data = NULL;

	if (p_debug_bootlog_data->sz_log_buff)
		free_sec_audio_log(p_debug_bootlog_data);
	kfree_const(p_debug_bootlog_data->name);
	kfree(p_debug_bootlog_data);
	p_debug_bootlog_data = NULL;

	if (p_debug_log_data->sz_log_buff)
		free_sec_audio_log(p_debug_log_data);
	kfree_const(p_debug_log_data->name);
	kfree(p_debug_log_data);
	p_debug_log_data = NULL;

	destroy_workqueue(p_debug_data->debug_string_wq);
	p_debug_data->debug_string_wq = NULL;

	proc_remove(audio_procfs);

	mutex_destroy(&p_debug_data->dbg_lock);
	kfree(p_debug_data);
	p_debug_data = NULL;

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_audio_debug_of_match[] = {
	{ .compatible = "samsung,audio-debug", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_audio_debug_of_match);
#endif /* CONFIG_OF */

static struct platform_driver sec_audio_debug_driver = {
	.driver		= {
		.name	= "sec-audio-debug",
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_audio_debug_of_match),
#endif /* CONFIG_OF */
	},

	.probe = sec_audio_debug_probe,
	.remove = sec_audio_debug_remove,
};

module_platform_driver(sec_audio_debug_driver);

MODULE_DESCRIPTION("Samsung Electronics Audio Debug driver");
MODULE_LICENSE("GPL");

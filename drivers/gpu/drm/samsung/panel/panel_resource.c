#include "usdm_panel.h"
#include "usdm_panel_debug.h"
#include "usdm_panel_obj.h"
#include "usdm_panel_resource.h"
#include "usdm_util.h"

char *usdm_get_resource_name(struct resinfo *res)
{
	return get_pnobj_name(&res->base);
}
EXPORT_SYMBOL(usdm_get_resource_name);

unsigned int usdm_get_resource_size(struct resinfo *res)
{
	return res->dlen;
}
EXPORT_SYMBOL(usdm_get_resource_size);

bool usdm_is_valid_resource(struct resinfo *res)
{
	if (!res)
		return false;

	if (get_pnobj_cmd_type(&res->base) != CMD_TYPE_RES)
		return false;

	if (!usdm_get_resource_name(res))
		return false;

	if (res->dlen == 0)
		return false;

	if (res->state < 0 || res->state >= MAX_RES_INIT_STATE)
		return false;

	return true;
}
EXPORT_SYMBOL(usdm_is_valid_resource);

bool usdm_is_resource_initialized(struct resinfo *res)
{
	if (!res)
		return false;

	return (res->state == RES_INITIALIZED);
}
EXPORT_SYMBOL(usdm_is_resource_initialized);

bool usdm_is_resource_mutable(struct resinfo *res)
{
	if (!res)
		return false;

	return (res->resui != NULL);
}
EXPORT_SYMBOL(usdm_is_resource_mutable);

void set_resource_state(struct resinfo *res, int state)
{
	if (!res)
		return;

	if (state < 0 || state >= MAX_RES_INIT_STATE) {
		panel_err("invalid state(%u)\n", state);
		return;
	}
	res->state = state;
}

int usdm_copy_resource_slice(u8 *dst, struct resinfo *res, u32 offset, u32 len)
{
	if (unlikely(!dst || !res || len == 0)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	if (unlikely(offset + len > usdm_get_resource_size(res))) {
		panel_err("slice array[%d:%d] out of range [:%d]\n",
				offset, offset + len, usdm_get_resource_size(res));
		return -EINVAL;
	}

	memcpy(dst, &res->data[offset], len);
	return 0;
}
EXPORT_SYMBOL(usdm_copy_resource_slice);

int usdm_copy_resource(u8 *dst, struct resinfo *res)
{
	if (unlikely(!dst || !res)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	if (!usdm_is_resource_initialized(res)) {
		panel_warn("%s not initialized\n", usdm_get_resource_name(res));
		return -EINVAL;
	}
	return usdm_copy_resource_slice(dst, res, 0, usdm_get_resource_size(res));
}
EXPORT_SYMBOL(usdm_copy_resource);

static int snprintf_resource_head(char *buf, size_t size, struct resinfo *res)
{
	int i, len = 0;

	if (!buf || !size || !res)
		return 0;

	len += snprintf(buf + len, size - len, "%s\n", usdm_get_resource_name(res));
	len += snprintf(buf + len, size - len, "state: %d (%s)\n", res->state,
			!usdm_is_resource_initialized(res) ? "UNINITIALIZED" : "INITIALIZED");

	len += snprintf(buf + len, size - len, "resui: %d (%s)\n",
			res->nr_resui, !usdm_is_resource_mutable(res) ? "IMMUTABLE" : "MUTABLE");
	for (i = 0; i < res->nr_resui; i++)
		len += snprintf(buf + len, size - len, "[%d]: offset: %d, rdi: %s\n",
				i, res->resui[i].offset, get_rdinfo_name(res->resui[i].rditbl));

	len += snprintf(buf + len, size - len, "size: %d", usdm_get_resource_size(res));

	return len;
}

int usdm_snprintf_resource_data(char *buf, size_t size, struct resinfo *res)
{
	int i, len = 0, resource_size;
	const unsigned int align = 16;

	if (!buf || !size || !res)
		return 0;

	if (!usdm_is_valid_resource(res))
		return 0;

	if (!usdm_is_resource_initialized(res))
		return 0;

	resource_size = usdm_get_resource_size(res);
	for (i = 0; i < resource_size; i++) {
		len += snprintf(buf + len, size - len, "%02X", res->data[i]);
		if (i + 1 == resource_size)
			break;
		len += snprintf(buf + len, size - len, "%s",
				!((i + 1) % align) ? "\n" : " ");
	}


	return len;
}
EXPORT_SYMBOL(usdm_snprintf_resource_data);

int usdm_snprintf_resource(char *buf, size_t size, struct resinfo *res)
{
	int len = 0;

	if (!buf || !size || !res)
		return 0;

	len = snprintf_resource_head(buf, size, res);
	if (usdm_is_resource_initialized(res)) {
		len += snprintf(buf + len, size - len, "\ndata:\n");
		len += usdm_snprintf_resource_data(buf + len, size - len, res);
	}

	return len;
}
EXPORT_SYMBOL(usdm_snprintf_resource);

void usdm_print_resource(struct resinfo *res)
{
	if (!usdm_is_valid_resource(res))
		return;

	if (!usdm_is_resource_initialized(res))
		return;

	panel_info("resource:%s\n", usdm_get_resource_name(res));
	usdm_info_bytes(res->data, usdm_get_resource_size(res));
}
EXPORT_SYMBOL(usdm_print_resource);

/**
 * usdm_create_resource - create a struct resinfo structure
 * @name: pointer to a string for the name of this packet.
 * @initdata: buffer to copy resource buffer.
 * @size: size of resource buffer.
 * @resui: resource update information array.
 * @nr_resui: size of resource update information array.
 *
 * This is used to create a struct resinfo pointer.
 *
 * Returns &struct resinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to usdm_destroy_resource().
 */
struct resinfo *usdm_create_resource(char *name, u8 *initdata,
		u32 size, struct res_update_info *resui, unsigned int nr_resui)
{
	struct resinfo *resource;

	if (!name || !size) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	resource = kzalloc(sizeof(*resource), GFP_KERNEL);
	if (!resource)
		return NULL;

	resource->data = kvmalloc(size, GFP_KERNEL);
	if (!resource->data)
		goto err;

	pnobj_init(&resource->base, CMD_TYPE_RES, name);
	resource->dlen = size;
	if (initdata) {
		memcpy(resource->data, initdata, size);
		resource->state = RES_INITIALIZED;
	} else {
		resource->state = RES_UNINITIALIZED;
	}

	if (resui) {
		resource->resui = kzalloc(sizeof(*resui) * nr_resui, GFP_KERNEL);
		if (!resource->resui)
			goto err;

		memcpy(resource->resui, resui, sizeof(*resui) * nr_resui);
		resource->nr_resui = nr_resui;
	}

	return resource;

err:
	kvfree(resource->data);
	kfree(resource);
	return NULL;
}
EXPORT_SYMBOL(usdm_create_resource);

/**
 * usdm_destroy_resource - destroys a struct resinfo structure
 * @rx_packet: pointer to the struct resinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to usdm_create_resource().
 */
void usdm_destroy_resource(struct resinfo *resource)
{
	if (!resource)
		return;

	pnobj_deinit(&resource->base);
	kvfree(resource->data);
	kfree(resource->resui);
	kfree(resource);
}
EXPORT_SYMBOL(usdm_destroy_resource);

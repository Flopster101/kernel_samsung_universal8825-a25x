#include <linux/kernel.h>
#include <linux/types.h>
#include "panel.h"
#include "panel_obj.h"
#include "panel_delay.h"

bool is_valid_delay(struct delayinfo *delay)
{
	unsigned int type;

	if (!delay)
		return false;

	type = get_pnobj_cmd_type(&delay->base);
	if (!IS_CMD_TYPE_DELAY(type) &&
			type != CMD_TYPE_TIMER_DELAY)
		return false;

	return true;
}

bool is_valid_timer_delay(struct delayinfo *delay)
{
	unsigned int type;

	if (!delay)
		return false;

	type = get_pnobj_cmd_type(&delay->base);
	if (type != CMD_TYPE_TIMER_DELAY)
		return false;

	return true;
}

bool is_valid_timer_delay_begin(struct timer_delay_begin_info *begin)
{
	unsigned int type;

	if (!begin)
		return false;

	if (!is_valid_timer_delay(begin->delay))
		return false;

	type = get_pnobj_cmd_type(&begin->base);
	if (type != CMD_TYPE_TIMER_DELAY_BEGIN)
		return false;

	return true;
}

unsigned int get_delay_type(struct delayinfo *delay)
{
	return get_pnobj_cmd_type(&delay->base);
}

char *get_delay_name(struct delayinfo *delay)
{
	return get_pnobj_name(&delay->base);
}

char *get_timer_delay_begin_name(struct timer_delay_begin_info *begin)
{
	return get_pnobj_name(&begin->base);
}

/**
 * create_delay - create a struct delayinfo structure
 * @name: pointer to a string for the name of this delay.
 * @type: type of delay.
 * @usec: microseconds to delay.
 * @nframe: number of frames to delay.
 * @nvsync: number of vsyncs to wait.
 * @no_sleep: flag to choose sleep or delay function.
 *
 * This is used to create a struct delayinfo pointer.
 *
 * Returns &struct delayinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_delay().
 */
struct delayinfo *create_delay(char *name, u32 type, u32 usec, u32 nframe, u32 nvsync, bool no_sleep)
{
	struct delayinfo *delay;

	if (!name)
		return NULL;

	if (!IS_CMD_TYPE_DELAY(type) &&
			type != CMD_TYPE_TIMER_DELAY)
		return NULL;

	delay = kzalloc(sizeof(*delay), GFP_KERNEL);
	if (!delay)
		return NULL;

	pnobj_init(&delay->base, type, name);
	delay->usec = usec;
	delay->nframe = nframe;
	delay->nvsync = nvsync;
	delay->no_sleep = no_sleep;

	return delay;
}
EXPORT_SYMBOL(create_delay);

/**
 * destroy_delay - destroys a struct delayinfo structure
 * @delay: pointer to the struct delayinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_delay().
 */
void destroy_delay(struct delayinfo *delay)
{
	if (!delay)
		return;

	pnobj_deinit(&delay->base);
	kfree(delay);
}
EXPORT_SYMBOL(destroy_delay);

/**
 * create_timer_delay_begin - create a struct timer_delay_begin_info structure
 * @name: pointer to a string for the name of this timer_delay_begin.
 * @name: pointer to a timer delay.
 *
 * This is used to create a struct timer_delay_begin pointer.
 *
 * Returns &struct timer_delay_begin pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_timer_delay_begin().
 */
struct timer_delay_begin_info *create_timer_delay_begin(char *name, struct delayinfo *delay)
{
	struct timer_delay_begin_info *begin;

	if (!name)
		return NULL;

	if (!is_valid_timer_delay(delay))
		return NULL;

	begin = kzalloc(sizeof(*begin), GFP_KERNEL);
	if (!begin)
		return NULL;

	pnobj_init(&begin->base, CMD_TYPE_TIMER_DELAY_BEGIN, name);
	begin->delay = delay;

	return begin;
}
EXPORT_SYMBOL(create_timer_delay_begin);

/**
 * destroy_timer_delay_begin - destroys a struct timer_delay_begin structure
 * @begin: pointer to the struct delayinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_delay().
 */
void destroy_timer_delay_begin(struct timer_delay_begin_info *begin)
{
	pnobj_deinit(&begin->base);
	kfree(begin);
}
EXPORT_SYMBOL(destroy_timer_delay_begin);

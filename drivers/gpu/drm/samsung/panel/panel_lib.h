#ifndef __PANEL_LIB_H__
#define __PANEL_LIB_H__

#include <linux/types.h>
#include "util.h"
#include "panel.h"

/*
 * PNOBJ LIST - Panel_drv pnobj primitives
 *
 * @ struct rdinfo
 * @ struct resinfo
 * @ struct keyinfo
 * @ struct pktinfo
 * @ struct delayinfo
 * @ struct timer_delay_begin_info
 * @ struct pininfo
 * @ struct dumpinfo
 * @ struct condinfo
 * @ struct pkt_update_info
 * @ struct res_update_info
 * @ struct condinfo_cont
 */

/*
 * PNOBJ CHAINING INFO - pnobj can contain or refer the other pnobj.
 *
 * @ struct keyinfo
 *	->struct pktinfo
 *		-> struct pkt_update_info
 *			-> struct maptbl
 *
 * @ struct dumpinfo (table)
 *	-> struct resinfo  (table)
 *		-> struct res_update_info
 *			-> struct rdinfo  (table)
 *
 * @ struct timer_delay_begin_info
 *	->struct delayinfo
 *
 * @ struct pininfo
 * @ struct condinfo
 */

/*
 * RULE - CREATE
 *
 * @ buf or data
 *	Optional (But *length of them is Essential)
 * @ member pnobj
 *	Optional
 *
 *
 * RULE - DESTROY
 *
 * @ buf or data
 *	Destroy together.
 * @ member pnobj
 *	Do nothing.
 *
 *
 * RULE - COPY
 *
 * @ values
 *	Just copy them.
 *
 * @ buf or data
 *	Should be copied on new allocated 'buf or data' of dst.
 *	If dst has 'data or buf' already, Let them release first and allocate new one.
 *
 * @ member pnobj
 *	Just copy addr of pnobj.
 *
 *
 * RULE - ALLOC / FREE / SET / CLEAR
 *
 * @ alloc
 *	If pnobj has 'buf or data' already, give up and return err.
 * @ free
 *	If pnobj has 'buf or data',  free it.
 */

/* rdinfo */
int panel_lib_rdinfo_alloc_buffer(struct rdinfo *m);
void panel_lib_rdinfo_free_buffer(struct rdinfo *m);
struct rdinfo *panel_lib_rdinfo_create(u32 type, char *name, u32 addr, u32 offset, u32 len, u8 *data);
void panel_lib_rdinfo_destroy(struct rdinfo *m);
int panel_lib_rdinfo_copy(struct rdinfo *dst, struct rdinfo *src);

/* res_update_info */
struct res_update_info *panel_lib_res_update_info_create(u32 offset, struct rdinfo *rditbl);
void panel_lib_res_update_info_destroy(struct res_update_info *resui);
int panel_lib_res_update_info_copy(struct res_update_info *dst, struct res_update_info *src);

/* resinfo*/
int panel_lib_resinfo_alloc_buffer(struct resinfo *m);
void panel_lib_resinfo_free_buffer(struct resinfo *m);
struct resinfo *panel_lib_resinfo_create(char *name, u8 *init_data, u32 dlen, struct res_update_info *resui, u32 nr_resui);
void panel_lib_resinfo_destroy(struct resinfo *m);
int panel_lib_resinfo_copy(struct resinfo *dst, struct resinfo *src);

#endif /* __PANEL_LIB_H__ */

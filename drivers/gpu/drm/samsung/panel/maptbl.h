#ifndef __MAPTBL_H__
#define __MAPTBL_H__

#include <linux/types.h>
#include "util.h"
#include "panel_obj.h"
#include "panel_function.h"
#include "panel_property.h"
#include "panel.h"

enum ndarray_dimen {
	NDARR_1D,
	NDARR_2D,
	NDARR_3D,
	NDARR_4D,
	NDARR_5D,
	NDARR_6D,
	NDARR_7D,
	NDARR_8D,
	MAX_NDARR_DIMEN,
};

struct maptbl;
typedef int (*maptbl_init_t)(struct maptbl *m);
typedef int (*maptbl_getidx_t)(struct maptbl *m);
typedef void (*maptbl_copy_t)(struct maptbl *m, u8 *dst);

/*
 * indices of n-dimension
 */
struct maptbl_pos {
	unsigned int index[MAX_NDARR_DIMEN];
};

/*
 * shape of maptbl
 */
struct maptbl_shape {
	unsigned int nr_dimen;	/* n-dimension */
	unsigned int sz_dimen[MAX_NDARR_DIMEN];
};

/* structure of mapping table */
struct maptbl_ops {
	struct pnobj_func *init;
	struct pnobj_func *getidx;
	struct pnobj_func *copy;
};

struct maptbl_props {
	char *name[MAX_NDARR_DIMEN];
};

struct maptbl {
	struct pnobj base;
	struct maptbl_shape shape;
	int sz_copy;
	u8 *arr;
	void *pdata;
	bool initialized;
	struct maptbl_ops ops;
	struct maptbl_props props;
};

#define MAPINFO(_name_) (_name_)

#define call_maptbl_init(_m) \
	(((_m) && (_m)->ops.init && (_m)->ops.init->symaddr) ? \
	  ((maptbl_init_t)(_m)->ops.init->symaddr)(_m) : -EINVAL)

#define call_maptbl_getidx(_m) \
	(((_m) && (_m)->ops.getidx && (_m)->ops.getidx->symaddr) ? \
	  ((maptbl_getidx_t)(_m)->ops.getidx->symaddr)(_m) : -EINVAL)

#define call_maptbl_copy(_m, _dst) \
	(((_m) && (_m)->ops.copy && (_m)->ops.copy->symaddr) ? \
	  ((maptbl_copy_t)(_m)->ops.copy->symaddr)(_m, _dst) : 0)

static inline char *maptbl_get_name(struct maptbl *m)
{
	return m ? get_pnobj_name(&m->base) : NULL;
}

static inline struct list_head *maptbl_get_list(struct maptbl *m)
{
	return m ? get_pnobj_list(&m->base) : NULL;
}

static inline int maptbl_get_countof_dimen(struct maptbl *m)
{
	return m ? m->shape.nr_dimen : 0;
}

static inline int maptbl_get_countof_n_dimen_element(struct maptbl *m, enum ndarray_dimen dimen)
{
	if (!m)
		return 0;

	if (dimen >= maptbl_get_countof_dimen(m))
		return 1;

	return m->shape.sz_dimen[dimen];
}

static inline int maptbl_get_countof_box(struct maptbl *m)
{
	return maptbl_get_countof_n_dimen_element(m, NDARR_4D);
}

static inline int maptbl_get_countof_layer(struct maptbl *m)
{
	return maptbl_get_countof_n_dimen_element(m, NDARR_3D);
}

static inline int maptbl_get_countof_row(struct maptbl *m)
{
	return maptbl_get_countof_n_dimen_element(m, NDARR_2D);
}

static inline int maptbl_get_countof_col(struct maptbl *m)
{
	return maptbl_get_countof_n_dimen_element(m, NDARR_1D);
}

static inline int maptbl_get_sizeof_copy(struct maptbl *m)
{
	return (m? m->sz_copy : 0);
}

static inline int maptbl_get_sizeof_n_dimen_element(struct maptbl *m, enum ndarray_dimen dimen)
{
	int i, res = 1;

	if (!m || !maptbl_get_countof_dimen(m))
		return 0;

	for (i = 0; i < dimen; i++)
		res *= maptbl_get_countof_n_dimen_element(m, i);

	return res;
}

static inline int maptbl_get_sizeof_box(struct maptbl *m)
{
	return maptbl_get_sizeof_n_dimen_element(m, NDARR_4D);
}

static inline int maptbl_get_sizeof_layer(struct maptbl *m)
{
	return maptbl_get_sizeof_n_dimen_element(m, NDARR_3D);
}

static inline int maptbl_get_sizeof_row(struct maptbl *m)
{
	return maptbl_get_sizeof_n_dimen_element(m, NDARR_2D);
}

static inline int maptbl_get_sizeof_col(struct maptbl *m)
{
	return maptbl_get_sizeof_n_dimen_element(m, NDARR_1D);
}

static inline int maptbl_get_sizeof_maptbl(struct maptbl *m)
{
	return maptbl_get_sizeof_n_dimen_element(m, maptbl_get_countof_dimen(m));
}

int maptbl_get_indexof_n_dimen_element(struct maptbl *m, enum ndarray_dimen dimen, unsigned int indexof_element);
int maptbl_get_indexof_box(struct maptbl *m, unsigned int indexof_box);
int maptbl_get_indexof_layer(struct maptbl *m, unsigned int indexof_layer);
int maptbl_get_indexof_row(struct maptbl *m, unsigned int indexof_row);
int maptbl_get_indexof_col(struct maptbl *m, unsigned int indexof_col);

/* maptbl shape macros */
#define DIMENSION(_n_) (_n_)
#define INDEX_TO_DIMEN(_index_) ((_index_) + (1))

#define __NDARR_INITIALIZER(...) \
{ \
	[NDARR_1D] = M_GET_BACK_1(__VA_ARGS__), \
	[NDARR_2D] = M_GET_BACK_2(__VA_ARGS__), \
	[NDARR_3D] = M_GET_BACK_3(__VA_ARGS__), \
	[NDARR_4D] = M_GET_BACK_4(__VA_ARGS__), \
	[NDARR_5D] = M_GET_BACK_5(__VA_ARGS__), \
	[NDARR_6D] = M_GET_BACK_6(__VA_ARGS__), \
	[NDARR_7D] = M_GET_BACK_7(__VA_ARGS__), \
	[NDARR_8D] = M_GET_BACK_8(__VA_ARGS__), \
}

#define __MAPTBL_SHAPE_INITIALIZER(...) \
{ \
	.nr_dimen = (M_NARGS(__VA_ARGS__)), \
	.sz_dimen = __NDARR_INITIALIZER(__VA_ARGS__) \
}

#define __MAPTBL_PROP_NAME_INITIALIZER(...) \
{ \
	.name = __NDARR_INITIALIZER(__VA_ARGS__) \
}

#define DEFINE_MAPTBL_SHAPE(shapename, ...) \
	struct maptbl_shape shapename = __MAPTBL_SHAPE_INITIALIZER(__VA_ARGS__)

#define __MAPTBL_SIZEOF_COPY_INITIALIZER(...) \
	M_GET_LAST(__VA_ARGS__)

#define ARRAY_SIZE_1(x) ARRAY_SIZE(x)
#define ARRAY_SIZE_2(x) ARRAY_SIZE(x), ARRAY_SIZE_1(x[0])
#define ARRAY_SIZE_3(x) ARRAY_SIZE(x), ARRAY_SIZE_2(x[0])
#define ARRAY_SIZE_4(x) ARRAY_SIZE(x), ARRAY_SIZE_3(x[0])
#define ARRAY_SIZE_5(x) ARRAY_SIZE(x), ARRAY_SIZE_4(x[0])
#define ARRAY_SIZE_6(x) ARRAY_SIZE(x), ARRAY_SIZE_5(x[0])
#define ARRAY_SIZE_7(x) ARRAY_SIZE(x), ARRAY_SIZE_6(x[0])
#define ARRAY_SIZE_8(x) ARRAY_SIZE(x), ARRAY_SIZE_7(x[0])

#define __MAPTBL_SHAPE_INITIALIZER_FROM_ARRAY(N, arrayname) \
	__MAPTBL_SHAPE_INITIALIZER(ARRAY_SIZE_##N(arrayname))

#define __MAPTBL_SIZEOF_COPY_INITIALIZER_FROM_ARRAY(N, arrayname) \
	__MAPTBL_SIZEOF_COPY_INITIALIZER(ARRAY_SIZE_##N(arrayname))

#define __MAPTBL_OPS_INITIALIZER(_init_func, _getidx_func, _copy_func) \
{ \
	.init = (_init_func), \
	.getidx = (_getidx_func), \
	.copy = (_copy_func), \
}

/* maptbl iteration macros */
#define maptbl_for_each(_m, _pos) \
	for ((_pos) = (0); \
		(_pos) < (maptbl_get_sizeof_maptbl(_m)); \
		(_pos)++)

#define maptbl_for_each_reverse(_m, _pos) \
	for ((_pos) = (maptbl_get_sizeof_maptbl(_m) - 1); \
		(int)(_pos) >= (0); \
		(_pos)--)

#define maptbl_for_each_dimen(_m, _dimen) \
	for ((_dimen) = (NDARR_1D); \
		(_dimen) < (maptbl_get_countof_dimen((_m))); \
		(_dimen)++)

#define maptbl_for_each_dimen_reverse(_m, _dimen) \
	for ((_dimen) = (maptbl_get_countof_dimen((_m)) - (1)); \
		(int)(_dimen) >= (NDARR_1D); \
		(_dimen)--)

#define maptbl_for_each_n_dimen_element(_m, _dimen, _pos) \
	for ((_pos) = (0); \
		(_pos) < (maptbl_get_countof_n_dimen_element((_m), (_dimen))); \
		(_pos)++)

#define maptbl_for_each_n_dimen_element_reverse(_m, _dimen, _pos) \
	for ((_pos) = (maptbl_get_countof_n_dimen_element(_m, (_dimen)) - 1); \
		(int)(_pos) >= (0); \
		(_pos)--)

#define maptbl_for_each_box(_m, _pos) \
	maptbl_for_each_n_dimen_element(_m, NDARR_4D, _pos)

#define maptbl_for_each_layer(_m, _pos) \
	maptbl_for_each_n_dimen_element(_m, NDARR_3D, _pos)

#define maptbl_for_each_row(_m, _pos) \
	maptbl_for_each_n_dimen_element(_m, NDARR_2D, _pos)

#define maptbl_for_each_col(_m, _pos) \
	maptbl_for_each_n_dimen_element(_m, NDARR_1D, _pos)

/* maptbl macros */
#define __MAPTBL_INITIALIZER(_name_, _ops_, _init_data_, _priv_, ...) \
	{ .base = __PNOBJ_INITIALIZER(_name_, CMD_TYPE_MAP) \
	, .arr = (u8 *)(_name_) \
	, .shape = __MAPTBL_SHAPE_INITIALIZER(__VA_ARGS__) \
	, .sz_copy = __MAPTBL_SIZEOF_COPY_INITIALIZER(__VA_ARGS__) \
	, .ops = *(_ops_) \
	, .pdata = (_priv_) }

#define DEFINE_MAPTBL(_name_, _arr_, _nlayer_, _nrow_, _ncol_, _sz_copy_, _init_func, _getidx_func, _copy_func)  \
	{ .base = { .name = (_name_), .cmd_type = (CMD_TYPE_MAP) } \
	, .arr = (u8 *)(_arr_) \
	, .shape = { \
		.nr_dimen = (((_nlayer_) > 0) ? 3 : \
				 (((_nrow_) > 0) ? 2 : 1)), \
		.sz_dimen = { \
			[NDARR_1D] = (_ncol_), \
			[NDARR_2D] = (_nrow_), \
			[NDARR_3D] = (_nlayer_), \
			[NDARR_4D] = (1), \
		}, \
	} \
	, .sz_copy = (_sz_copy_) \
	, .ops = __MAPTBL_OPS_INITIALIZER(_init_func, _getidx_func, _copy_func) \
	, .pdata = NULL }

#define DEFINE_0D_MAPTBL(_name_, _init_func, _getidx_func, _copy_func)  \
	{ .base = __PNOBJ_INITIALIZER(_name_, CMD_TYPE_MAP) \
	, .arr = NULL  \
	, .shape = __MAPTBL_SHAPE_INITIALIZER() \
	, .sz_copy = 0                       \
	, .ops = __MAPTBL_OPS_INITIALIZER(_init_func, _getidx_func, _copy_func) \
	, .pdata = NULL }

#define __MAPTBL_INITIALIZER_FROM_ARRAY(N, _name_, _init_func, _getidx_func, _copy_func) \
	{ .base = __PNOBJ_INITIALIZER(_name_, CMD_TYPE_MAP) \
	, .arr = (u8 *)(_name_) \
	, .shape = __MAPTBL_SHAPE_INITIALIZER_FROM_ARRAY(N, _name_) \
	, .sz_copy = __MAPTBL_SIZEOF_COPY_INITIALIZER_FROM_ARRAY(N, _name_) \
	, .ops = __MAPTBL_OPS_INITIALIZER(_init_func, _getidx_func, _copy_func) \
	, .pdata = NULL }

#define DEFINE_1D_MAPTBL(_name_, _init_func, _getidx_func, _copy_func) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(1, _name_, _init_func, _getidx_func, _copy_func)

#define DEFINE_2D_MAPTBL(_name_, _init_func, _getidx_func, _copy_func) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(2, _name_, _init_func, _getidx_func, _copy_func)

#define DEFINE_3D_MAPTBL(_name_, _init_func, _getidx_func, _copy_func) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(3, _name_, _init_func, _getidx_func, _copy_func)

#define DEFINE_4D_MAPTBL(_name_, _init_func, _getidx_func, _copy_func) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(4, _name_, _init_func, _getidx_func, _copy_func)

#define __SIMPLE_MAPTBL_INITIALIZER_FROM_ARRAY(N, _name, _init_func, _getidx_func, _copy_func, ...) \
	{ .base = __PNOBJ_INITIALIZER(_name, CMD_TYPE_MAP) \
	, .arr = (u8 *)(_name) \
	, .shape = __MAPTBL_SHAPE_INITIALIZER_FROM_ARRAY(N, _name) \
	, .props = __MAPTBL_PROP_NAME_INITIALIZER(__VA_ARGS__) \
	, .sz_copy = __MAPTBL_SIZEOF_COPY_INITIALIZER_FROM_ARRAY(N, _name) \
	, .ops = __MAPTBL_OPS_INITIALIZER(_init_func, _getidx_func, _copy_func) \
	, .pdata = NULL }

#define __SIMPLE_MAPTBL_INITIALIZER(_name, _init_func, _getidx_func, _copy_func, ...) \
	__SIMPLE_MAPTBL_INITIALIZER_FROM_ARRAY(M_NARGS(__VA_ARGS__), \
			_name, _init_func, _getidx_func, _copy_func, __VA_ARGS__)

int maptbl_alloc_buffer(struct maptbl *m, size_t size);
void maptbl_free_buffer(struct maptbl *m);
void maptbl_set_shape(struct maptbl *m, struct maptbl_shape *shape);
void maptbl_set_sizeof_copy(struct maptbl *m, unsigned int sizeof_copy);
void maptbl_set_ops(struct maptbl *m, struct maptbl_ops *ops);
void maptbl_set_initialized(struct maptbl *m, bool initialized);
void maptbl_set_private_data(struct maptbl *m, void *priv);
void *maptbl_get_private_data(struct maptbl *m);
struct maptbl *maptbl_create(char *name, struct maptbl_shape *shape,
		struct maptbl_ops *ops, struct maptbl_props *props, void *init_data, void *priv);
struct maptbl *maptbl_clone(struct maptbl *src);
struct maptbl *maptbl_deepcopy(struct maptbl *dst, struct maptbl *src);
void maptbl_destroy(struct maptbl *m);

int maptbl_index(struct maptbl *m, int layer, int row, int col);
int maptbl_4d_index(struct maptbl *m, int box, int layer, int row, int col);
bool maptbl_is_initialized(struct maptbl *m);
bool maptbl_is_index_in_bound(struct maptbl *m, unsigned int index);
int maptbl_init(struct maptbl *m);
int maptbl_getidx(struct maptbl *m);
int maptbl_copy(struct maptbl *m, u8 *dst);
int maptbl_cmp_shape(struct maptbl *m1, struct maptbl *m2);
struct maptbl *maptbl_memcpy(struct maptbl *dst, struct maptbl *src);
int maptbl_pos_to_index(struct maptbl *m, struct maptbl_pos *pos);
int maptbl_index_to_pos(struct maptbl *m, unsigned int index, struct maptbl_pos *pos);
int maptbl_fill(struct maptbl *m, struct maptbl_pos *pos, u8 *src, size_t n);
int maptbl_snprintf_head(struct maptbl *m, char *buf, size_t size);
int maptbl_snprintf_body(struct maptbl *m, char *buf, size_t size);
int maptbl_snprintf_tail(struct maptbl *m, char *buf, size_t size);
int maptbl_snprintf(struct maptbl *m, char *buf, size_t size);
void maptbl_print(struct maptbl *m);
int maptbl_getidx_from_props(struct maptbl *m);
#endif /* __MAPTBL_H__ */

#ifndef __MAPTBL_H__
#define __MAPTBL_H__

#include <linux/types.h>
#include "util.h"
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
	int (*init)(struct maptbl *tbl);
	int (*getidx)(struct maptbl *tbl);
	void (*copy)(struct maptbl *tbl, u8 *dst);
};

struct maptbl {
	u32 type;
	char *name;
	struct maptbl_shape shape;
	int sz_copy;
	u8 *arr;
	void *pdata;
	bool initialized;
	struct maptbl_ops ops;
};

#define MAPINFO(_name_) (_name_)

static inline int maptbl_get_countof_dimen(struct maptbl *tbl)
{
	return tbl ? tbl->shape.nr_dimen : 0;
}

static inline int maptbl_get_countof_n_dimen_element(struct maptbl *tbl, enum ndarray_dimen dimen)
{
	if (!tbl)
		return 0;

	if (dimen >= maptbl_get_countof_dimen(tbl))
		return 1;

	return tbl->shape.sz_dimen[dimen];
}

static inline int maptbl_get_countof_box(struct maptbl *tbl)
{
	return maptbl_get_countof_n_dimen_element(tbl, NDARR_4D);
}

static inline int maptbl_get_countof_layer(struct maptbl *tbl)
{
	return maptbl_get_countof_n_dimen_element(tbl, NDARR_3D);
}

static inline int maptbl_get_countof_row(struct maptbl *tbl)
{
	return maptbl_get_countof_n_dimen_element(tbl, NDARR_2D);
}

static inline int maptbl_get_countof_col(struct maptbl *tbl)
{
	return maptbl_get_countof_n_dimen_element(tbl, NDARR_1D);
}

static inline int maptbl_get_sizeof_copy(struct maptbl *tbl)
{
	return (tbl ? tbl->sz_copy : 0);
}

static inline int maptbl_get_sizeof_n_dimen_element(struct maptbl *tbl, enum ndarray_dimen dimen)
{
	int i, res = 1;

	if (!tbl || !maptbl_get_countof_dimen(tbl))
		return 0;

	for (i = 0; i < dimen; i++)
		res *= maptbl_get_countof_n_dimen_element(tbl, i);

	return res;
}

static inline int maptbl_get_sizeof_box(struct maptbl *tbl)
{
	return maptbl_get_sizeof_n_dimen_element(tbl, NDARR_4D);
}

static inline int maptbl_get_sizeof_layer(struct maptbl *tbl)
{
	return maptbl_get_sizeof_n_dimen_element(tbl, NDARR_3D);
}

static inline int maptbl_get_sizeof_row(struct maptbl *tbl)
{
	return maptbl_get_sizeof_n_dimen_element(tbl, NDARR_2D);
}

static inline int maptbl_get_sizeof_col(struct maptbl *tbl)
{
	return maptbl_get_sizeof_n_dimen_element(tbl, NDARR_1D);
}

static inline int maptbl_get_sizeof_maptbl(struct maptbl *tbl)
{
	return maptbl_get_sizeof_n_dimen_element(tbl, maptbl_get_countof_dimen(tbl));
}

int maptbl_get_indexof_n_dimen_element(struct maptbl *tbl, enum ndarray_dimen dimen, unsigned int indexof_element);
int maptbl_get_indexof_box(struct maptbl *tbl, unsigned int indexof_box);
int maptbl_get_indexof_layer(struct maptbl *tbl, unsigned int indexof_layer);
int maptbl_get_indexof_row(struct maptbl *tbl, unsigned int indexof_row);
int maptbl_get_indexof_col(struct maptbl *tbl, unsigned int indexof_col);

/* maptbl shape macros */
#define DIMENSION(_n_) (_n_)
#define INDEX_TO_DIMEN(_index_) ((_index_) + (1))

#define __MAPTBL_SHAPE_INITIALIZER(...) \
{ \
	.nr_dimen = (M_NARGS(__VA_ARGS__)), \
	.sz_dimen = { \
		[NDARR_1D] = M_GET_BACK_1(__VA_ARGS__), \
		[NDARR_2D] = M_GET_BACK_2(__VA_ARGS__), \
		[NDARR_3D] = M_GET_BACK_3(__VA_ARGS__), \
		[NDARR_4D] = M_GET_BACK_4(__VA_ARGS__), \
		[NDARR_5D] = M_GET_BACK_5(__VA_ARGS__), \
		[NDARR_6D] = M_GET_BACK_6(__VA_ARGS__), \
		[NDARR_7D] = M_GET_BACK_7(__VA_ARGS__), \
		[NDARR_8D] = M_GET_BACK_8(__VA_ARGS__), \
	} \
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

#define __MAPTBL_OPS_INITIALIZER(_init_func_, _getidx_func_, _copy_func_) \
{ \
	.init = (_init_func_), \
	.getidx = (_getidx_func_), \
	.copy = (_copy_func_), \
}

/* maptbl iteration macros */
#define maptbl_for_each(tbl, __pos__) \
	for ((__pos__) = (0); \
		(__pos__) < (maptbl_get_sizeof_maptbl(tbl)); \
		(__pos__)++)

#define maptbl_for_each_reverse(tbl, __pos__) \
	for ((__pos__) = (maptbl_get_sizeof_maptbl(tbl) - 1); \
		(int)(__pos__) >= (0); \
		(__pos__)--)

#define maptbl_for_each_dimen(tbl, __dimen__) \
	for ((__dimen__) = (NDARR_1D); \
		(__dimen__) < (maptbl_get_countof_dimen((tbl))); \
		(__dimen__)++)

#define maptbl_for_each_dimen_reverse(tbl, __dimen__) \
	for ((__dimen__) = (maptbl_get_countof_dimen((tbl)) - (1)); \
		(int)(__dimen__) >= (NDARR_1D); \
		(__dimen__)--)

#define maptbl_for_each_n_dimen_element(tbl, __dimen__, __pos__) \
	for ((__pos__) = (0); \
		(__pos__) < (maptbl_get_countof_n_dimen_element((tbl), (__dimen__))); \
		(__pos__)++)

#define maptbl_for_each_n_dimen_element_reverse(tbl, __dimen__, __pos__) \
	for ((__pos__) = (maptbl_get_countof_n_dimen_element(tbl, (__dimen__)) - 1); \
		(int)(__pos__) >= (0); \
		(__pos__)--)

#define maptbl_for_each_box(tbl, __pos__) \
	maptbl_for_each_n_dimen_element(tbl, NDARR_4D, __pos__)

#define maptbl_for_each_layer(tbl, __pos__) \
	maptbl_for_each_n_dimen_element(tbl, NDARR_3D, __pos__)

#define maptbl_for_each_row(tbl, __pos__) \
	maptbl_for_each_n_dimen_element(tbl, NDARR_2D, __pos__)

#define maptbl_for_each_col(tbl, __pos__) \
	maptbl_for_each_n_dimen_element(tbl, NDARR_1D, __pos__)

/* maptbl macros */
#define __MAPTBL_INITIALIZER(_name_, _ops_, _init_data_, _priv_, ...) \
{ \
	.type = (CMD_TYPE_MAP), \
	.name = STRINGFY(_name_), \
	.arr = (u8 *)(_name_), \
	.shape = __MAPTBL_SHAPE_INITIALIZER(__VA_ARGS__), \
	.sz_copy = __MAPTBL_SIZEOF_COPY_INITIALIZER(__VA_ARGS__), \
	.ops = *(_ops_), \
	.pdata = (_priv_), \
}

#define DEFINE_MAPTBL(_name_, _arr_, _nlayer_, _nrow_, _ncol_, _sz_copy_, _init_func_, _getidx_func_, _copy_func_)  \
{ \
	.type = (CMD_TYPE_MAP), \
	.name = _name_, \
	.arr = (u8 *)(_arr_), \
	.shape = {                          \
		.nr_dimen = (((_nlayer_) > 0) ? 3 : \
				 (((_nrow_) > 0) ? 2 : 1)), \
		.sz_dimen = {                   \
			[NDARR_1D] = (_ncol_),      \
			[NDARR_2D] = (_nrow_),      \
			[NDARR_3D] = (_nlayer_),    \
			[NDARR_4D] = (1),           \
		},                              \
	},                                  \
	.sz_copy = (_sz_copy_),             \
	.ops = __MAPTBL_OPS_INITIALIZER(_init_func_, _getidx_func_, _copy_func_), \
	.pdata = NULL, \
}

#define DEFINE_0D_MAPTBL(_name_, _init_func_, _getidx_func_, _copy_func_)  \
{ \
	.type = (CMD_TYPE_MAP), \
	.name = STRINGFY(_name_), \
	.arr = NULL, \
	.shape = __MAPTBL_SHAPE_INITIALIZER(), \
	.sz_copy = 0,                       \
	.ops = __MAPTBL_OPS_INITIALIZER(_init_func_, _getidx_func_, _copy_func_), \
	.pdata = NULL, \
}

#define __MAPTBL_INITIALIZER_FROM_ARRAY(N, _name_, _init_func_, _getidx_func_, _copy_func_) \
{ \
	.type = (CMD_TYPE_MAP), \
	.name = STRINGFY(_name_), \
	.arr = (u8 *)(_name_), \
	.shape = __MAPTBL_SHAPE_INITIALIZER_FROM_ARRAY(N, _name_), \
	.sz_copy = __MAPTBL_SIZEOF_COPY_INITIALIZER_FROM_ARRAY(N, _name_), \
	.ops = __MAPTBL_OPS_INITIALIZER(_init_func_, _getidx_func_, _copy_func_), \
	.pdata = NULL, \
}

#define DEFINE_1D_MAPTBL(_name_, _init_func_, _getidx_func_, _copy_func_) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(1, _name_, _init_func_, _getidx_func_, _copy_func_)

#define DEFINE_2D_MAPTBL(_name_, _init_func_, _getidx_func_, _copy_func_) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(2, _name_, _init_func_, _getidx_func_, _copy_func_)

#define DEFINE_3D_MAPTBL(_name_, _init_func_, _getidx_func_, _copy_func_) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(3, _name_, _init_func_, _getidx_func_, _copy_func_)

#define DEFINE_4D_MAPTBL(_name_, _init_func_, _getidx_func_, _copy_func_) \
	__MAPTBL_INITIALIZER_FROM_ARRAY(4, _name_, _init_func_, _getidx_func_, _copy_func_)

int maptbl_alloc_buffer(struct maptbl *m, size_t size);
void maptbl_free_buffer(struct maptbl *m);
void maptbl_set_shape(struct maptbl *m, struct maptbl_shape *shape);
void maptbl_set_ops(struct maptbl *m, struct maptbl_ops *ops);
void maptbl_set_initialized(struct maptbl *m, bool initialized);
void maptbl_set_private_data(struct maptbl *m, void *priv);
void *maptbl_get_private_data(struct maptbl *m);
struct maptbl *maptbl_create(char *name, struct maptbl_shape *shape,
		struct maptbl_ops *ops, void *init_data, void *priv);
struct maptbl *maptbl_clone(struct maptbl *src);
struct maptbl *maptbl_deepcopy(struct maptbl *dst, struct maptbl *src);
void maptbl_destroy(struct maptbl *m);

int maptbl_index(struct maptbl *tbl, int layer, int row, int col);
int maptbl_4d_index(struct maptbl *tbl, int box, int layer, int row, int col);
bool maptbl_is_initialized(struct maptbl *tbl);
bool maptbl_is_index_in_bound(struct maptbl *tbl, unsigned int index);
int maptbl_init(struct maptbl *tbl);
int maptbl_getidx(struct maptbl *tbl);
int maptbl_copy(struct maptbl *tbl, u8 *dst);
int maptbl_cmp_shape(struct maptbl *m1, struct maptbl *m2);
struct maptbl *maptbl_memcpy(struct maptbl *dst, struct maptbl *src);
int maptbl_pos_to_index(struct maptbl *tbl, struct maptbl_pos *pos);
int maptbl_index_to_pos(struct maptbl *tbl, unsigned int index, struct maptbl_pos *pos);
int maptbl_fill(struct maptbl *tbl, struct maptbl_pos *pos, u8 *src, size_t n);
int maptbl_snprintf_head(struct maptbl *tbl, char *buf, size_t size);
int maptbl_snprintf_body(struct maptbl *tbl, char *buf, size_t size);
int maptbl_snprintf_tail(struct maptbl *tbl, char *buf, size_t size);
int maptbl_snprintf(struct maptbl *tbl, char *buf, size_t size);
void maptbl_print(struct maptbl *tbl);
#endif /* __MAPTBL_H__ */

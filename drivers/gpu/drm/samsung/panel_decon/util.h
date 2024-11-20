#ifndef __UTIL_H__
#define __UTIL_H__

#include <linux/types.h>

#define STRINGFY(x)	(#x)

/* count arguments */
#define M_NARGS(...) M_NARGS_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define M_NARGS_(_8, _7, _6, _5, _4, _3, _2, _1, N, ...) N

/* utility (concatenation) */
#define M_CONC(A, B) M_CONC_(A, B)
#define M_CONC_(A, B) A##B

/* get element from starting from 0th */
#define __M_GET_ELEM(N, ...) M_CONC(__M_GET_ELEM_, N)(__VA_ARGS__)
#define __M_GET_ELEM_0(_0, ...) _0
#define __M_GET_ELEM_1(_0, _1, ...) _1
#define __M_GET_ELEM_2(_0, _1, _2, ...) _2
#define __M_GET_ELEM_3(_0, _1, _2, _3, ...) _3
#define __M_GET_ELEM_4(_0, _1, _2, _3, _4, ...) _4
#define __M_GET_ELEM_5(_0, _1, _2, _3, _4, _5, ...) _5
#define __M_GET_ELEM_6(_0, _1, _2, _3, _4, _5, _6, ...) _6
#define __M_GET_ELEM_7(_0, _1, _2, _3, _4, _5, _6, _7, ...) _7
#define __M_GET_ELEM_8(_0, _1, _2, _3, _4, _5, _6, _7, _8, ...) _8

/* get element from start */
#define M_GET_ELEM_1(...) __M_GET_ELEM(1, _, __VA_ARGS__)
#define M_GET_ELEM_2(...) __M_GET_ELEM(2, _, __VA_ARGS__)
#define M_GET_ELEM_3(...) __M_GET_ELEM(3, _, __VA_ARGS__)
#define M_GET_ELEM_4(...) __M_GET_ELEM(4, _, __VA_ARGS__)
#define M_GET_ELEM_5(...) __M_GET_ELEM(5, _, __VA_ARGS__)
#define M_GET_ELEM_6(...) __M_GET_ELEM(6, _, __VA_ARGS__)
#define M_GET_ELEM_7(...) __M_GET_ELEM(7, _, __VA_ARGS__)
#define M_GET_ELEM_8(...) __M_GET_ELEM(8, _, __VA_ARGS__)

/* get element from end */
#define M_GET_LAST(...) M_GET_BACK_1(__VA_ARGS__)
#define M_GET_BACK_8(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0,0,0,0,0,0,0, ##__VA_ARGS__ ,0)
#define M_GET_BACK_7(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0,0,0,0,0,0, ##__VA_ARGS__ ,0,0)
#define M_GET_BACK_6(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0,0,0,0,0, ##__VA_ARGS__ ,0,0,0)
#define M_GET_BACK_5(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0,0,0,0, ##__VA_ARGS__ ,0,0,0,0)
#define M_GET_BACK_4(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0,0,0, ##__VA_ARGS__ ,0,0,0,0,0)
#define M_GET_BACK_3(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0,0, ##__VA_ARGS__ ,0,0,0,0,0,0)
#define M_GET_BACK_2(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, 0, ##__VA_ARGS__ ,0,0,0,0,0,0,0)
#define M_GET_BACK_1(...) __M_GET_ELEM(M_NARGS(__VA_ARGS__), _, ##__VA_ARGS__  ,0,0,0,0,0,0,0,0)

int copy_from_sliced_byte_array(u8 *dest, const u8 *src,
		int start, int stop, int step);
int copy_to_sliced_byte_array(u8 *dest, const u8 *src,
		int start, int stop, int step);
s32 hextos32(u32 hex, u32 bits);
u32 s32tohex(s32 dec, u32 bits);
#endif /* __UTIL_H__ */

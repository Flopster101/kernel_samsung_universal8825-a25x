// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit mock for struct KUNIT_T.
 *
 * Copyright (C) 2018, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#include "test-mock.h"

DEFINE_STRUCT_CLASS_MOCK_VOID_RETURN(METHOD(fail), CLASS(KUNIT_T),
				     PARAMS(struct KUNIT_T *,
					    struct test_stream *));

DEFINE_STRUCT_CLASS_MOCK_VOID_RETURN(METHOD(mock_vprintk), CLASS(KUNIT_T),
				     PARAMS(const struct KUNIT_T *,
					    const char *,
					    struct va_format *));

static int test_init(struct MOCK(KUNIT_T) *mock_test)
{
	struct KUNIT_T *trgt = mock_get_trgt(mock_test);
	int ret;

	ret = test_init_test(trgt, "MOCK(KUNIT_T)");
	trgt->fail = fail;
	mock_set_default_action(mock_get_ctrl(mock_test),
				"fail",
				fail,
				int_return(mock_get_test(mock_test), 0));
	trgt->vprintk = mock_vprintk;
	mock_set_default_action(mock_get_ctrl(mock_test),
				"mock_vprintk",
				mock_vprintk,
				int_return(mock_get_test(mock_test), 0));
	return ret;
}

DEFINE_STRUCT_CLASS_MOCK_INIT(KUNIT_T, test_init);

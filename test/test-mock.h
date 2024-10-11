/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KUnit mock for struct KUNIT_T.
 *
 * Copyright (C) 2018, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#include <test/test.h>
#include <test/test-names.h>
#include <test/mock.h>

DECLARE_STRUCT_CLASS_MOCK_PREREQS(KUNIT_T);

DECLARE_STRUCT_CLASS_MOCK_VOID_RETURN(METHOD(fail), CLASS(KUNIT_T),
				      PARAMS(struct KUNIT_T *,
					     struct test_stream *));

DECLARE_STRUCT_CLASS_MOCK_VOID_RETURN(METHOD(mock_vprintk), CLASS(KUNIT_T),
				      PARAMS(const struct KUNIT_T *,
					     const char *,
					     struct va_format *));
DECLARE_STRUCT_CLASS_MOCK_INIT(KUNIT_T);

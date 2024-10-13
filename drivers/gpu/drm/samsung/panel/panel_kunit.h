#ifndef __PANEL_KUNIT_H__
#define __PANEL_KUNIT_H__
#include <kunit/mock.h>
#ifndef KUNIT_EXPECT_TRUE_MSG
#define KUNIT_EXPECT_TRUE_MSG(_test, _condition, _msg) KUNIT_EXPECT_TRUE(_test, _condition)
#endif
#endif /* __PANEL_KUNIT_H__ */

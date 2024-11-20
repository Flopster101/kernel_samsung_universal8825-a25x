#ifndef __PANEL_KUNIT_H__
#define __PANEL_KUNIT_H__
#if !defined(CONFIG_SEC_KUNIT)
#include "kunit_mock_dummy.h"
#else
#include <kunit/mock.h>
#endif
#endif /* __PANEL_KUNIT_H__ */

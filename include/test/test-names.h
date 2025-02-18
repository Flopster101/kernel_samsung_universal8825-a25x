// Transitional: controls which names we should use for kunit structs.

#ifdef CONFIG_KUNIT_USE_UPSTREAM_NAMES

#define KUNIT_T kunit
#define KUNIT_RESOURCE_T kunit_resource
#define KUNIT_CASE_T kunit_case
#define KUNIT_SUITE_T kunit_suite

#else

#define KUNIT_T test
#define KUNIT_RESOURCE_T test_resource
#define KUNIT_CASE_T test_case
#define KUNIT_SUITE_T test_module

#endif

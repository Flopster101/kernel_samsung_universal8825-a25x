#ifndef __KUNIT_MOCK_DUMMY_H__
#define __KUNIT_MOCK_DUMMY_H__
struct kunit {
	int dummy;
};

#define REAL_ID(func_name) __real__##func_name
#define INVOKE_ID(func_name) __invoke__##func_name
#define RETURNS(return_type) return_type
#define __mockable
#define __mockable_alias(id) __alias(id)
#define __visible_for_testing static

#ifndef PARAMS
#define PARAMS(args...) args
#endif

#define DECLARE_REDIRECT_MOCKABLE(name, return_type, param_types...) \
	return_type name(param_types)

#define DEFINE_INVOKABLE(name, return_type, RETURN_ASSIGN, param_types...)

#define DEFINE_REDIRECT_MOCKABLE_COMMON(name,                      \
		return_type,                   \
		RETURN_ASSIGN,                 \
		param_types...)                \
		return_type REAL_ID(name)(param_types);                \
		return_type name(param_types) __mockable_alias(REAL_ID(name)); \
		DEFINE_INVOKABLE(name, return_type, RETURN_ASSIGN, param_types);

#define ASSIGN() *retval =

#define DEFINE_REDIRECT_MOCKABLE(name, return_type, param_types...) \
	DEFINE_REDIRECT_MOCKABLE_COMMON(name,                  \
			return_type,               \
			ASSIGN,                \
			param_types)

#endif /* __KUNIT_MOCK_DUMMY_H__ */

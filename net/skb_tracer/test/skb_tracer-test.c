// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <kunit/mock.h>

#include <linux/netdevice.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <linux/rbtree.h>
#include <net/net_namespace.h>
#include <net/request_sock.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <net/skb_tracer.h>

struct kunit *g_test;

/* declare static mockable functions here
 * because they don't have header files.
 */
DECLARE_REDIRECT_MOCKABLE(skb_tracer_gen_mask, RETURNS(u64),
				PARAMS(struct sock *));
DECLARE_REDIRECT_MOCKABLE(print_sock_info, RETURNS(void),
				PARAMS(struct sock *, struct task_struct *));
DECLARE_REDIRECT_MOCKABLE(skb_tracer_print_skb_queue, RETURNS(void),
				PARAMS(struct sk_buff_head *));
DECLARE_REDIRECT_MOCKABLE(skb_tracer_print_skb_rb_tree, RETURNS(void),
				PARAMS(struct rb_root *));
DECLARE_REDIRECT_MOCKABLE(skb_tracer_print_skb, RETURNS(void),
				PARAMS(struct skb_tracer *, struct sk_buff *));

/* define mockable functions  */
DEFINE_FUNCTION_MOCK(skb_tracer_gen_mask, RETURNS(u64),
				PARAMS(struct sock *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(print_sock_info,
				PARAMS(struct sock *, struct task_struct *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(skb_tracer_print_skb_queue,
				PARAMS(struct sk_buff_head *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(skb_tracer_print_skb_rb_tree,
				PARAMS(struct rb_root *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(skb_tracer_print_skb,
				PARAMS(struct skb_tracer *, struct sk_buff *));

/* other externs */
extern u64 skb_tracer_sock_mask;

/*
 * This TRICK is really needed for testing skb_tracer.o
 * without declaring EXPORT_SYMBOL_xxx serise
 * even for static in kernel module,
 * which doesn't need in c++.
 */

extern u64 __get_func_mask(enum skb_tracer_location location);

static void test___get_func_mask(struct kunit *test)
{
	u64 mask = 0;
	int i;

	for (i = 0; i < STL_MAX; i++) {
		mask = __get_func_mask(i);

		KUNIT_EXPECT_EQ(test, mask, (1 << i));
	}
}

static void test_skb_tracer_func_trace(struct kunit *test)
{
	struct sock *test_sock;
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;
	u64 mask = 0x00040;

	/* Pre-requisites */
	test_sock = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	/* 1. For case, skb doesn't have 'tracer' object. */
	skb_tracer_func_trace(test_sock, test_skb, STL_IP6_OUTPUT);
	tracer = skb_ext_find(test_skb, SKB_TRACER);
	KUNIT_EXPECT_TRUE(test, !tracer);

	/* 2. For case, skb has 'tracer' object. */
	test_sock->sk_tracer_mask = mask;
	skb_tracer_mask(test_skb, mask);
	skb_tracer_func_trace(test_sock, test_skb, STL_IP6_OUTPUT);
	tracer = skb_ext_find(test_skb, SKB_TRACER);
	KUNIT_EXPECT_TRUE(test, !!tracer);
	KUNIT_EXPECT_EQ(test, STL_IP6_OUTPUT, tracer->skb_mask);

	/* Post-requisites */
	skb_ext_del(test_skb, SKB_TRACER);
	kunit_kfree(test, test_sock);
	kunit_kfree(test, test_skb);
}

static void test_skb_tracer_gen_mask(struct kunit *test)
{
	struct sock test_sock;

	u64 ret_mask;

	ret_mask = skb_tracer_gen_mask(&test_sock);

	KUNIT_EXPECT_EQ(test, ret_mask, (u64)0x1);
}

static void test_skb_tracer_mask_unmask(struct kunit *test)
{
	struct sk_buff test_skb;
	u64 mask = 0x00010;
	struct skb_tracer *tracer;

	/* 1. call "skb_tracer_mask" */
	skb_tracer_mask(&test_skb, mask);

	/* 2. tracer should have a propper mask. */
	tracer = skb_ext_find(&test_skb, SKB_TRACER);

	/* 3. test if 'tracer' exist. */
	KUNIT_EXPECT_TRUE(test, !!tracer);

	if (!tracer)
		return;

	/* 4. test if it has a propper mask value. */
	KUNIT_EXPECT_EQ(test, (tracer->queue_mask & mask), (u64)mask);

	/* 5. unmask and then test if it's value is back to 0. */
	skb_tracer_unmask(&test_skb, tracer->queue_mask);
	KUNIT_EXPECT_EQ(test, tracer->queue_mask & mask, 0);
}

extern int fd_from_sk(struct sock *sk, struct task_struct *task);

static int sock_map_fd(struct socket *sock, int flags)
{
	struct file *newfile;
	int fd = get_unused_fd_flags(flags);
	if (unlikely(fd < 0)) {
		sock_release(sock);
		return fd;
	}

	newfile = sock_alloc_file(sock, flags, NULL);
	if (!IS_ERR(newfile)) {
		fd_install(fd, newfile);
		return fd;
	}

	put_unused_fd(fd);
	return PTR_ERR(newfile);
}

static void test_fd_from_sk(struct kunit *test)
{
	struct sock *test_sk;
	struct socket *test_socket = NULL;
	struct socket *dummy_sockets[2] = {0, };
	int fd, test_fd;
	int i;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);

	/* create a 'socket' at current task
	 * and also, Two dummy 'socket's.
	 */
	for (i = 0; i < 2; i++) {
		sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
					&dummy_sockets[i]);
	}
	sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
				&test_socket);

	for (i = 0; i < 2; i++)
		sock_map_fd(dummy_sockets[i], (O_CLOEXEC | O_NONBLOCK));
	fd = sock_map_fd(test_socket, (O_CLOEXEC | O_NONBLOCK));

	test_sk->sk_socket = test_socket;

	/* call testee */
	test_fd = fd_from_sk(test_sk, current);

	/* check if retrieved 'fd' has a valid index */
	KUNIT_EXPECT_EQ(test, fd, test_fd);

	/* check if retrieved 'fd' has gone after closing */
	if (test_socket)
		sock_release(test_socket);

	fd = fd_from_sk(test_sk, current);

	KUNIT_EXPECT_TRUE(test, fd == 0);

	/* Post-requisites */
	for (i = 0; i < 2; i++)
		sock_release(dummy_sockets[i]);

	kunit_kfree(test, test_sk);
}

/* mocking 'print_sock_info' */
void REAL_ID(print_sock_info)(struct sock *sk, struct task_struct *task)
{
	/* do nothing */
	kunit_info(g_test, "mocked\n");
}

static void test_skb_tracer_init(struct kunit *test)
{
	struct mock_expectation *exp_gen_mask, *exp_print_info;
	struct sock *test_sk;
	struct socket *test_socket = NULL;
	int err;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
				&test_socket);
	if (err < 0)
		goto test_skb_tracer_init_sock_err;

	exp_gen_mask = Times(1, EXPECT_CALL(skb_tracer_gen_mask(
					ptr_eq(test, test_sk))));
	ActionOnMatch(exp_gen_mask, u64_return(test, 0x20));

	exp_print_info = Times(1, EXPECT_CALL(print_sock_info(
					ptr_eq(test, test_sk),
					ptr_eq(test, current))));
	ActionOnMatch(exp_print_info, INVOKE_REAL(test, print_sock_info));


	/* 1. expect not being crashed here. */
	test_sk->sk_socket = NULL;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, (u64)test_sk->sk_socket, 0);

	/* 2. expect not assigned new mask. */
	test_sk->sk_socket = test_socket;
	test_sk->sk_tracer_mask = 0x10;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, 0x10);

	/* 3. expect normally assigned new mask. */
	test_sk->sk_tracer_mask = 0;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, 0x20);

	/* Post-requisites */
	kernel_sock_shutdown(test_socket, SHUT_RDWR);
	sock_release(test_socket);

test_skb_tracer_init_sock_err:
	kunit_kfree(test, test_sk);
}

static void test_skb_tracer_mask_on_skb(struct kunit *test)
{
	struct sock *test_sk;
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	if (!test_skb)
		goto test_sk_failed;

	/* 1. since sk is filled with 0, it MUST do nothing. */
	skb_tracer_mask_on_skb(test_sk, test_skb);
	KUNIT_EXPECT_PTR_EQ(test, skb_ext_find(test_skb, SKB_TRACER), NULL);

	/* 2. after adding some test mask value on a sk and invoke the func,
	 *    skb MUST have same mask value as sk's.
	 */
	test_sk->sk_tracer_mask = 0x4214;
	skb_tracer_mask_on_skb(test_sk, test_skb);
	tracer = skb_ext_find(test_skb, SKB_TRACER);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tracer);
	KUNIT_EXPECT_EQ(test, tracer->sock_mask, 0x4214);
	KUNIT_EXPECT_EQ(test, tracer->skb_mask, 0);

	/* Post-requisites */
	skb_ext_del(test_skb, SKB_TRACER);

	kunit_kfree(test, test_sk);
test_sk_failed:
	kunit_kfree(test, test_skb);
}

static void test_skb_tracer_return_mask(struct kunit *test)
{
	struct sock *test_sk;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	/* 1. test_sk has nothing, it just returns. */
	skb_tracer_return_mask(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, 0);

	/* 2. test_sk has 0x10000000000 mask,
	 *    after invoking this func, it MUST has 0.
	 *    instead, skb_tracer_sock_mask has 0x10000000000.
	 */
	skb_tracer_sock_mask = 0;
	test_sk->sk_tracer_mask = 0x10000000000;
	skb_tracer_return_mask(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, 0);
	KUNIT_EXPECT_EQ(test, skb_tracer_sock_mask, 0x10000000000);

	/* Post-requisites */
	skb_tracer_sock_mask = 0xFFFFFFFFFFFFFFFF;
	kunit_kfree(test, test_sk);
}

/* mocking 'skb_tracer_print_skb_queue' */
void REAL_ID(mock_skb_tracer_print_skb)(struct skb_tracer *tracer, struct sk_buff *skb)
{
	/* do nothing */
	kunit_info(g_test, "%s: mock invoked", __func__);
}
DEFINE_INVOKABLE(mock_skb_tracer_print_skb, void, NO_ASSIGN,
			PARAMS(struct skb_tracer *, struct sk_buff *));

static void test_skb_tracer_print_skb_queue(struct kunit *test)
{
	int i;
	struct sk_buff *test_skb[4];
	struct sk_buff_head test_queue;
	struct mock_expectation *exp, *exp_queue[4];

	/* 1. test for empty queue. */
	exp = RetireOnSaturation(
		Never(
			EXPECT_CALL(
				skb_tracer_print_skb(any(test), any(test))
			)
		)
	);
	ActionOnMatch(exp, INVOKE_REAL(test, mock_skb_tracer_print_skb));

	skb_queue_head_init(&test_queue);
	skb_tracer_print_skb_queue(&test_queue);
	KUNIT_EXPECT_TRUE(test, 1);

	/* 2. enqueue 4 skbs, and make each expectation. */
	for (i = 0; i < 4; i++) {
		test_skb[i] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
		skb_ext_add(test_skb[i], SKB_TRACER);

		/* append each skb on test_queue */
		skb_queue_tail(&test_queue, test_skb[i]);

		/* each skb MUST be invoked in sequence */
		exp_queue[i] = Times(1,
					EXPECT_CALL(skb_tracer_print_skb(
						any(test),
						ptr_eq(test, test_skb[i])))
				);
	}

	InSequence(test, exp_queue[0], exp_queue[1], exp_queue[2], exp_queue[3]);
	ActionOnMatch(exp_queue[0], INVOKE_REAL(test, mock_skb_tracer_print_skb));
	ActionOnMatch(exp_queue[1], INVOKE_REAL(test, mock_skb_tracer_print_skb));
	ActionOnMatch(exp_queue[2], INVOKE_REAL(test, mock_skb_tracer_print_skb));
	ActionOnMatch(exp_queue[3], INVOKE_REAL(test, mock_skb_tracer_print_skb));

	skb_tracer_print_skb_queue(&test_queue);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	for (i = 0; i < 4; i++) {
		skb_ext_del(test_skb[i], SKB_TRACER);
		kunit_kfree(test, test_skb[i]);
	}
}

static void test_skb_tracer_print_skb_rb_tree(struct kunit *test)
{
	int i;
	struct sk_buff *test_skb[4];
	struct rb_root test_rb = RB_ROOT;
	struct mock_expectation *exp, *exp_rb[4];

	/* 1. test for empty queue. */
	exp = RetireOnSaturation(
		Never(
			EXPECT_CALL(
				skb_tracer_print_skb(any(test), any(test))
			)
		)
	);
	ActionOnMatch(exp, INVOKE_REAL(test, mock_skb_tracer_print_skb));

	skb_tracer_print_skb_rb_tree(&test_rb);
	KUNIT_EXPECT_TRUE(test, 1);

	/* 2. enqueue 4 skbs, and make each expectation. */
	for (i = 0; i < 4; i++) {
		test_skb[i] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
		skb_ext_add(test_skb[i], SKB_TRACER);
		TCP_SKB_CB(test_skb[i])->seq = i;

		/* append each skb on test_rb */
		tcp_rbtree_insert(&test_rb, test_skb[i]);

		/* each skb MUST be invoked in sequence */
		exp_rb[i] = Times(1,
					EXPECT_CALL(skb_tracer_print_skb(
						any(test),
						ptr_eq(test, test_skb[i])))
				);
	}

	InSequence(test, exp_rb[0], exp_rb[1], exp_rb[2], exp_rb[3]);
	ActionOnMatch(exp_rb[0], INVOKE_REAL(test, mock_skb_tracer_print_skb));
	ActionOnMatch(exp_rb[1], INVOKE_REAL(test, mock_skb_tracer_print_skb));
	ActionOnMatch(exp_rb[2], INVOKE_REAL(test, mock_skb_tracer_print_skb));
	ActionOnMatch(exp_rb[3], INVOKE_REAL(test, mock_skb_tracer_print_skb));

	skb_tracer_print_skb_rb_tree(&test_rb);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	for (i = 0; i < 4; i++) {
		skb_ext_del(test_skb[i], SKB_TRACER);
		kunit_kfree(test, test_skb[i]);
	}
}

/* mocking 'skb_tracer_print_skb_queue' */
void REAL_ID(mock_skb_tracer_print_skb_queue)(struct sk_buff_head *queue)
{
	/* do nothing */
	kunit_info(g_test, "%s: mock invoked", __func__);
}
DEFINE_INVOKABLE(mock_skb_tracer_print_skb_queue, void, NO_ASSIGN,
			PARAMS(struct sk_buff_head *));

/* mocking 'skb_tracer_print_skb_rb_tree' */
void REAL_ID(mock_skb_tracer_print_skb_rb_tree)(struct rb_root *queue)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_skb_tracer_print_skb_rb_tree, void, NO_ASSIGN,
			PARAMS(struct rb_root *));

extern void skb_tracer_sk_error_report(struct sock *sk);

static void test_skb_tracer_sk_error_report(struct kunit *test)
{
	struct mock_expectation *exp_queue, *exp_rbtree;
	struct sock *test_sk;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	exp_queue = Times(1, EXPECT_CALL(skb_tracer_print_skb_queue(any(test))));
	ActionOnMatch(exp_queue,
			INVOKE_REAL(test, mock_skb_tracer_print_skb_queue));

	exp_rbtree = Times(1, EXPECT_CALL(skb_tracer_print_skb_rb_tree(any(test))));
	ActionOnMatch(exp_rbtree,
			INVOKE_REAL(test, mock_skb_tracer_print_skb_rb_tree));

	/* 1. test in case of sk->sk_tracer_mask is NULL. */
	skb_tracer_sk_error_report(test_sk);
	KUNIT_EXPECT_TRUE(test, 1);

	/* 2. check if 'skb_tracer_print_skb_queue' and
	 *    'skb_tracer_print_skb_rb_tree' both functions are called. */
	test_sk->sk_tracer_mask = 0x40;
	test_sk->sk_err = 22;

	skb_tracer_sk_error_report(test_sk);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	kunit_kfree(test, test_sk);
}

static void test_print_sock_info(struct kunit *test)
{
	struct sock *test_sk;
	struct inet_sock *test_inet;
	struct task_struct *test_task = current;
	struct socket *test_socket = NULL;
	int err;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct inet_sock), GFP_KERNEL);
	if (!test_sk)
		return;

	err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
				&test_socket);
	if (err < 0)
		goto test_print_sock_info_free_sk;

	test_sk->sk_socket = test_socket;

	test_inet = inet_sk(test_sk);
	test_inet->inet_sport = 0x1010;
	test_inet->inet_dport = 0x2020;

	/* 1. */
	print_sock_info(test_sk, test_task);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	kernel_sock_shutdown(test_socket, SHUT_RDWR);
	sock_release(test_socket);

test_print_sock_info_free_sk:
	kunit_kfree(test, test_sk);
}

#define MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(func_name) \
		mock_set_default_action(mock_get_global_mock(), \
			(#func_name), func_name, INVOKE_REAL(test, func_name))

static int skb_tracer_test_init(struct kunit *test)
{
	g_test = test;

	/* set default action as INVOKE_REAL() */
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_tracer_gen_mask);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(print_sock_info);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_tracer_print_skb_queue);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_tracer_print_skb_rb_tree);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_tracer_print_skb);

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void skb_tracer_test_exit(struct kunit *test)
{
	g_test = NULL;
}

#if IS_ENABLED(CONFIG_KUNIT)
static struct kunit_case skb_tracer_test_cases[] = {
	KUNIT_CASE(test_skb_tracer_gen_mask),
	KUNIT_CASE(test_skb_tracer_mask_unmask),
	KUNIT_CASE(test___get_func_mask),
	KUNIT_CASE(test_skb_tracer_func_trace),
	KUNIT_CASE(test_fd_from_sk),
	KUNIT_CASE(test_skb_tracer_init),
	KUNIT_CASE(test_skb_tracer_mask_on_skb),
	KUNIT_CASE(test_skb_tracer_return_mask),
	KUNIT_CASE(test_skb_tracer_print_skb_queue),
	KUNIT_CASE(test_skb_tracer_print_skb_rb_tree),
	KUNIT_CASE(test_skb_tracer_sk_error_report),
	KUNIT_CASE(test_print_sock_info),
	{},
};

static struct kunit_suite skb_tracer_test_module = {
	.name = "skb_tracer_test",
	.init = skb_tracer_test_init,
	.exit = skb_tracer_test_exit,
	.test_cases = skb_tracer_test_cases,
};

#endif

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&skb_tracer_test_module);

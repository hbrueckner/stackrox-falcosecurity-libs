#include "../../event_class/event_class.h"
#include "../../helpers/proc_parsing.h"

#if defined(__NR_clone3) && defined(__NR_wait4)

#include <linux/sched.h>

TEST(SyscallExit, clone3X_father)
{
	auto evt_test = new event_test(__NR_clone3, EXIT_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/

	/* We scan proc before the BPF event is caught so we have
	 * to use `GREATER_EQUAL` in the assertions.
	 */
	struct proc_info info = {0};
	pid_t pid = ::getpid();
	if(!get_proc_info(pid, &info))
	{
		FAIL() << "Unable to get all the info from proc" << std::endl;
	}

	/* We need to use `SIGCHLD` otherwise the parent won't receive any signal
	 * when the child terminates. We use `CLONE_FILES` just to test the flags.
	 */
	struct clone_args cl_args = {0};
	cl_args.flags = CLONE_FILES;
	cl_args.exit_signal = SIGCHLD;
	pid_t ret_pid = syscall(__NR_clone3, &cl_args, sizeof(cl_args));

	if(ret_pid == 0)
	{
		/* Child terminates immediately. */
		exit(EXIT_SUCCESS);
	}

	assert_syscall_state(SYSCALL_SUCCESS, "clone3", ret_pid, NOT_EQUAL, -1);
	/* Catch the child before doing anything else. */
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	/*=============================== TRIGGER SYSCALL  ===========================*/

	evt_test->disable_capture();

	evt_test->assert_event_presence();

	if(HasFatalFailure())
	{
		return;
	}

	evt_test->parse_event();

	evt_test->assert_header();

	/*=============================== ASSERT PARAMETERS  ===========================*/

	/* Parameter 1: res (type: PT_PID)*/
	evt_test->assert_numeric_param(1, (int64_t)ret_pid);

	/* Parameter 2: exe (type: PT_CHARBUF) */
	evt_test->assert_charbuf_param(2, info.args[0]);

	/* Parameter 3: args (type: PT_CHARBUFARRAY) */
	/* Starting from `1` because the first is `exe`. */
	evt_test->assert_charbuf_array_param(3, &info.args[1]);

	/* Parameter 4: tid (type: PT_PID) */
	evt_test->assert_numeric_param(4, (int64_t)pid);

	/* Parameter 5: pid (type: PT_PID) */
	/* We are the main thread of the process so it's equal to `tid`. */
	evt_test->assert_numeric_param(5, (int64_t)pid);

	/* Parameter 6: ptid (type: PT_PID) */
	evt_test->assert_numeric_param(6, (int64_t)info.ppid);

	/* Parameter 7: cwd (type: PT_CHARBUF) */
	/* leave the current working directory empty like in the old probe. */
	evt_test->assert_empty_param(7);

	/* Parameter 8: fdlimit (type: PT_UINT64) */
	evt_test->assert_numeric_param(8, (uint64_t)info.file_rlimit.rlim_cur);

	/* Parameter 9: pgft_maj (type: PT_UINT64) */
	/* Right now we can't find a precise value to perform the assertion. */
	evt_test->assert_numeric_param(9, (uint64_t)0, GREATER_EQUAL);

	/* Parameter 10: pgft_min (type: PT_UINT64) */
	/* Right now we can't find a precise value to perform the assertion. */
	evt_test->assert_numeric_param(10, (uint64_t)0, GREATER_EQUAL);

	/* Parameter 11: vm_size (type: PT_UINT32) */
	evt_test->assert_numeric_param(11, (uint32_t)info.vm_size, GREATER_EQUAL);

	/* Parameter 12: vm_rss (type: PT_UINT32) */
	evt_test->assert_numeric_param(12, (uint32_t)info.vm_rss, GREATER_EQUAL);

	/* Parameter 13: vm_swap (type: PT_UINT32) */
	evt_test->assert_numeric_param(13, (uint32_t)info.vm_swap, GREATER_EQUAL);

	/* Parameter 14: comm (type: PT_CHARBUF) */
	evt_test->assert_charbuf_param(14, TEST_EXECUTABLE_NAME);

	/* Parameter 15: cgroups (type: PT_CHARBUFARRAY) */
	evt_test->assert_cgroup_param(15);

	/* Parameter 16: flags (type: PT_FLAGS32) */
	evt_test->assert_numeric_param(16, (uint32_t)PPM_CL_CLONE_FILES);

	/* Parameter 17: uid (type: PT_UINT32) */
	evt_test->assert_numeric_param(17, (uint32_t)info.uid);

	/* Parameter 18: gid (type: PT_UINT32) */
	evt_test->assert_numeric_param(18, (uint32_t)info.gid);

	/* Parameter 19: vtid (type: PT_PID) */
	evt_test->assert_numeric_param(19, (int64_t)info.vtid);

	/* Parameter 20: vpid (type: PT_PID) */
	evt_test->assert_numeric_param(20, (int64_t)info.vpid);

	/*=============================== ASSERT PARAMETERS  ===========================*/

	evt_test->assert_num_params_pushed(20);
}

/* In some architectures we are not able to catch the `clone exit child
 * event` from the `sys_exit` tracepoint. This is because there is no
 * default behavior among different architectures... you can find more
 * info in `driver/feature_gates.h`.
 */
#ifndef CAPTURE_SCHED_PROC_FORK
TEST(SyscallExit, clone3X_child)
{
	auto evt_test = new event_test(__NR_clone3, EXIT_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/

	/* We need to use `SIGCHLD` otherwise the parent won't receive any signal
	 * when the child terminates. We use `CLONE_FILES` just to test the flags.
	 */
	struct clone_args cl_args = {0};
	cl_args.flags = CLONE_FILES;
	cl_args.exit_signal = SIGCHLD;
	pid_t ret_pid = syscall(__NR_clone3, &cl_args, sizeof(cl_args));

	/* Child performs assertions on itself. */
	if(ret_pid == 0)
	{
		/* Please note: in the child, we cannot have a fatal failure otherwise all the following
		 * tests will crash. So we add a simple failure and the father will check for failures in
		 * the child process.
		 */

		/* We scan proc after the BPF event is caught so we have
		 * to use `LESS_EQUAL` in the assertions.
		 */
		struct proc_info info = {0};
		pid_t pid = ::getpid();
		if(!get_proc_info(pid, &info))
		{
			ADD_FAILURE() << "Unable to get all the info from proc" << std::endl;
			exit(EXIT_FAILURE);
		}

		evt_test->disable_capture();

		evt_test->assert_event_presence(pid);

		if(HasFatalFailure())
		{
			ADD_FAILURE() << "Problem during initialization";
			exit(EXIT_FAILURE);
		}

		evt_test->parse_event();

		evt_test->assert_header();

		/*=============================== ASSERT PARAMETERS  ===========================*/

		/* Parameter 1: res (type: PT_PID)*/
		evt_test->assert_numeric_param(1, (int64_t)ret_pid);

		/* Parameter 2: exe (type: PT_CHARBUF) */
		evt_test->assert_charbuf_param(2, info.args[0]);

		/* Parameter 3: args (type: PT_CHARBUFARRAY) */
		/* Starting from `1` because the first is `exe`. */
		evt_test->assert_charbuf_array_param(3, &info.args[1]);

		/* Parameter 4: tid (type: PT_PID) */
		evt_test->assert_numeric_param(4, (int64_t)pid);

		/* Parameter 5: pid (type: PT_PID) */
		/* We are the main thread of the process so it's equal to `tid`. */
		evt_test->assert_numeric_param(5, (int64_t)pid);

		/* Parameter 6: ptid (type: PT_PID) */
		evt_test->assert_numeric_param(6, (int64_t)info.ppid);

		/* Parameter 7: cwd (type: PT_CHARBUF) */
		/* leave the current working directory empty like in the old probe. */
		evt_test->assert_empty_param(7);

		/* Parameter 8: fdlimit (type: PT_UINT64) */
		evt_test->assert_numeric_param(8, (uint64_t)info.file_rlimit.rlim_cur);

		/* Parameter 9: pgft_maj (type: PT_UINT64) */
		evt_test->assert_numeric_param(9, (uint64_t)0, GREATER_EQUAL);

		/* Parameter 10: pgft_min (type: PT_UINT64) */
		evt_test->assert_numeric_param(10, (uint64_t)0, GREATER_EQUAL);

		/* Parameter 11: vm_size (type: PT_UINT32) */
		evt_test->assert_numeric_param(11, (uint32_t)info.vm_size, LESS_EQUAL);

		/* Parameter 12: vm_rss (type: PT_UINT32) */
		evt_test->assert_numeric_param(12, (uint32_t)info.vm_rss, LESS_EQUAL);

		/* Parameter 13: vm_swap (type: PT_UINT32) */
		evt_test->assert_numeric_param(13, (uint32_t)info.vm_swap, LESS_EQUAL);

		/* Parameter 14: comm (type: PT_CHARBUF) */
		evt_test->assert_charbuf_param(14, TEST_EXECUTABLE_NAME);

		/* Parameter 15: cgroups (type: PT_CHARBUFARRAY) */
		evt_test->assert_cgroup_param(15);

		/* Parameter 16: flags (type: PT_FLAGS32) */
		evt_test->assert_numeric_param(16, (uint32_t)PPM_CL_CLONE_FILES);

		/* Parameter 17: uid (type: PT_UINT32) */
		evt_test->assert_numeric_param(17, (uint32_t)info.uid);

		/* Parameter 18: gid (type: PT_UINT32) */
		evt_test->assert_numeric_param(18, (uint32_t)info.gid);

		/* Parameter 19: vtid (type: PT_PID) */
		evt_test->assert_numeric_param(19, (int64_t)info.vtid);

		/* Parameter 20: vpid (type: PT_PID) */
		evt_test->assert_numeric_param(20, (int64_t)info.vpid);

		/*=============================== ASSERT PARAMETERS  ===========================*/

		evt_test->assert_num_params_pushed(20);

		if(HasFailure())
		{
			exit(EXIT_FAILURE);
		}
		else
		{
			exit(EXIT_SUCCESS);
		}
	}

	assert_syscall_state(SYSCALL_SUCCESS, "clone3", ret_pid, NOT_EQUAL, -1);

	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE)
	{
		FAIL() << "Something in the child failed." << std::endl;
	}

	/*=============================== TRIGGER SYSCALL  ===========================*/
}
#endif /* CAPTURE_SCHED_PROC_FORK */

#endif

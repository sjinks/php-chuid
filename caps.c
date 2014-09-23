/**
 * @file caps.c
 * @version 0.6.0
 * @author Volodymyr Kolesnykov <volodymyr@wildwolf.name>
 * @brief Interface to libcap — implementation
 * @see http://www.kernel.org/doc/man-pages/online/pages/man7/capabilities.7.html
 */

#include <assert.h>
#include "caps.h"

/**
 * If we don't use libcap library, we assume that the user has all capabilities if its effective UID is 0 (the user is root)
 * and has no capabilities otherwise. However, both assumptions can be wrong if, say, AppArmor is used.
 *
 * @todo Maybe try reading /proc/&lt;pid&gt;/status file?
 * @todo In theory, cap_get_flag() may fail. Very unlikely :-) Anyway, maybe add a check?
 * @note Only @c EFFECTIVE set is checked
 */
int check_capabilities(int* restrict sys_chroot_, int* restrict setuid_)
{
	int retcode = 0;
	cap_flag_value_t can_sys_chroot, can_setuid;

#if defined(WITH_CAP_LIBRARY)
	cap_t capabilities = cap_get_proc();

	if (NULL != capabilities) {
		cap_get_flag(capabilities, CAP_SYS_CHROOT, CAP_EFFECTIVE, &can_sys_chroot);
		cap_get_flag(capabilities, CAP_SETUID,     CAP_EFFECTIVE, &can_setuid);

		cap_free(capabilities);
	}
	else {
		PHPCHUID_ERROR(E_WARNING, "cap_get_proc(): %s", strerror(errno));
		retcode        = -1;
		can_sys_chroot = CAP_CLEAR;
		can_setuid     = CAP_CLEAR;
	}
#elif defined(WITH_CAPNG_LIBRARY)
	can_sys_chroot = capng_have_capability(CAPNG_EFFECTIVE, CAP_SYS_CHROOT) ? CAP_SET : CAP_CLEAR;
	can_setuid     = capng_have_capability(CAPNG_EFFECTIVE, CAP_SETUID)     ? CAP_SET : CAP_CLEAR;
#else
	if (0 == geteuid()) {
		can_sys_chroot = CAP_SET;
		can_setuid     = CAP_SET;
	}
	else {
		can_sys_chroot = CAP_CLEAR;
		can_setuid     = CAP_CLEAR;
	}
#endif

	if (NULL != sys_chroot_) { *sys_chroot_ = (int)can_sys_chroot; }
	if (NULL != setuid_)     { *setuid_     = (int)can_setuid; }

	return retcode;
}

/**
 * @todo In theory, @c cap_set_flag() may fail, maybe add a check?
 */
int drop_capabilities_except(int num_caps, cap_value_t* cap_list)
{
	int retval = 0;

	assert(cap_list != NULL);

#ifdef DEBUG
	fprintf(stderr, "drop_capabilities_except: num_caps=%d\n", num_caps);
	{
		int i;
		for (i=0; i<num_caps; ++i) {
			fprintf(stderr, "Cap %d: %d\n", i+1, cap_list[i]);
		}
	}
#endif

#if defined(WITH_CAP_LIBRARY)
	if (num_caps > 0) {
		cap_t capabilities = cap_init();
		if (NULL != capabilities) {
			int res;

			cap_set_flag(capabilities, CAP_EFFECTIVE, num_caps, cap_list, CAP_SET);
			cap_set_flag(capabilities, CAP_PERMITTED, num_caps, cap_list, CAP_SET);
			res = cap_set_proc(capabilities);
			if (-1 == res) {
				PHPCHUID_ERROR(E_WARNING, "cap_set_proc(): %s", strerror(errno));
				retval = -1;
			}

			cap_free(capabilities);
		}
		else {
			PHPCHUID_ERROR(E_WARNING, "cap_init(): %s", strerror(errno));
			retval = -1;
		}
	}
#elif defined(WITH_CAPNG_LIBRARY)
	if (num_caps > 0) {
		int i;

		capng_clear(CAPNG_SELECT_BOTH);
		for (i=0; i<num_caps; ++i) {
			capng_update(CAPNG_ADD, CAPNG_EFFECTIVE | CAPNG_PERMITTED, cap_list[i]);
		}

		if (capng_apply(CAPNG_SELECT_BOTH) < 0) {
			PHPCHUID_ERROR(E_WARNING, "capng_apply(): %s", "failed to update privileges");
			retval = -1;
		}
	}
#endif

	return retval;
}

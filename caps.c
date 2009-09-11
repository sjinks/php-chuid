/**
 * @file
 * @version 0.2
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @brief Interface to libcap — implementation
 */

#include "caps.h"

/**
 * If we don't use libcap library, we assume that the user has all capabilities if its effective UID is 0 (the user is root)
 * and has no capabilities otherwise. However, both assumptions can be wrong if, say, AppArmor is used.
 *
 * @todo Maybe try reading /proc/&lt;pid&gt;/status file?
 * @todo In theory, cap_get_flag() may fail. Very unlikely :-) Anyway, maybe add a check?
 * @note Only @c EFFECTIVE set is checked
 */
int check_capabilities(int* restrict sys_chroot_, int* restrict dac_read_search_, int* restrict setuid_, int* restrict setgid_)
{
	int retcode = 0;
	cap_flag_value_t can_sys_chroot      = CAP_CLEAR;
	cap_flag_value_t can_dac_read_search = CAP_CLEAR;
	cap_flag_value_t can_setuid          = CAP_CLEAR;
	cap_flag_value_t can_setgid          = CAP_CLEAR;

#ifndef WITH_CAP_LIBRARY
	if (0 == geteuid()) {
		can_sys_chroot      = CAP_SET;
		can_dac_read_search = CAP_SET;
		can_setuid          = CAP_SET;
		can_setgid          = CAP_SET;
	}
#else
	cap_t capabilities = cap_get_proc();

	if (NULL != capabilities) {
		cap_get_flag(capabilities, CAP_SYS_CHROOT,      CAP_EFFECTIVE, &can_sys_chroot);
		cap_get_flag(capabilities, CAP_DAC_READ_SEARCH, CAP_EFFECTIVE, &can_dac_read_search);
		cap_get_flag(capabilities, CAP_SETGID,          CAP_EFFECTIVE, &can_setgid);
		cap_get_flag(capabilities, CAP_SETUID,          CAP_EFFECTIVE, &can_setuid);

		cap_free(capabilities);
	}
	else {
		PHPCHUID_ERROR(E_WARNING, "cap_get_proc(): %s", strerror(errno));
		retcode = -1;
	}
#endif

	if (NULL != sys_chroot_)      { *sys_chroot_      = (int)can_sys_chroot; }
	if (NULL != setuid_)          { *setuid_          = (int)can_setuid; }
	if (NULL != setgid_)          { *setgid_          = (int)can_setgid; }
	if (NULL != dac_read_search_) { *dac_read_search_ = (int)can_dac_read_search; }

	return retcode;
}

/**
 * @todo In theory, @c cap_set_flag() may fail. Maybe add a check?
 * @note There is no sense in @c CAP_SETUID and @c CAP_SETGID capabilities when ZTS is used, thats is why we drop them if ZTS is detected
 */
int drop_capabilities(void)
{
	int retval = 0;

#ifdef WITH_CAP_LIBRARY
	cap_t capabilities = cap_init();
	if (NULL != capabilities) {
		int res;
		int num_caps;
		cap_value_t cap_list[3] = { CAP_DAC_READ_SEARCH, CAP_SETGID, CAP_SETUID };

#ifdef ZTS
		num_caps = 1;
#else
		num_caps = 3;
#endif

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
#endif

	return retval;
}

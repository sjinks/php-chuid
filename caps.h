/**
 * @file
 * @version 0.5.0
 * @author Vladimir Kolesnikov <vladimir@free-sevastopol.com>
 * @brief Interface to libcap — definitions
 */

#ifndef PHPCHUID_CAPS_H_
#define PHPCHUID_CAPS_H_

#include "php_chuid.h"

#ifndef WITH_CAP_LIBRARY
/* From <sys/capability.h> */
typedef enum {
	CAP_CLEAR = 0, //!< CAP_CLEAR
	CAP_SET = 1    //!< CAP_SET
} cap_flag_value_t;

typedef int cap_value_t;

#endif

/**
 * @brief Checks for presense of @c CAP_SYS_CHROOT, @c CAP_DAC_READ_SEARCH, @c CAP_SETUID and @c CAP_SETGID capabilities
 * @param sys_chroot_ [out] Whether the process has CAP_SYS_CHROOT capability
 * @param dac_read_search_ [out] Whether the process has CAP_DAC_READ_SEARCH capability
 * @param setuid_ [out] Whether the process has CAP_SETUID capability
 * @param setgid_ [out] Whether the process has CAP_SETGID capability
 * @return Whether the call was successful
 * @retval 0 Yes
 * @retval -1 No (@c cap_get_proc() failed, @c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int check_capabilities(int* restrict sys_chroot_, int* restrict dac_read_search_, int* restrict setuid_, int* restrict setgid_);

/**
 * @brief Drops all capabilities except those in @c cap_list from the @c EFFECTIVE and @c PERMITTED sets
 * @param num_caps Number of capabilities in @c cap_list
 * @param cap_list List of the capabilities to leave; must not be @c NULL
 * @retval 0 Yes
 * @retval -1 No (@c cap_set_proc() or @c cap_init() failed, @c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int drop_capabilities(int num_caps, cap_value_t* cap_list);

#endif /* PHPCHUID_CAPS_H_ */

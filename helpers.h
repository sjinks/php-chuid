/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.2
 * @brief Helper functions — definitions
 */

#ifdef DOXYGEN
#	undef PHPCHUID_HELPERS_H_
#endif

#ifndef PHPCHUID_HELPERS_H_
#define PHPCHUID_HELPERS_H_

#include "php_chuid.h"

/**
 * @brief Disables <code>posix_set{e,}{u,g}id()</code> PHP functions if told by @c chuid.disable_posix_setuid_family
 */
PHPCHUID_VISIBILITY_HIDDEN void disable_posix_setuids(TSRMLS_D);

/**
 * @brief <code>chroot()</code>'s to the directory specified in @c chuid.global_chroot
 * @param can_chroot Whether the user has @c CAP_SYS_CHROOT capability
 * @return Whether the operation was successful
 * @retval 0 Yes
 * @retval -1 No (@c chroot() or @c chdir() failed, @c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int do_global_chroot(int can_chroot);

/**
 * @brief Finds out the UID and GID of @c nobody user.
 * @param uid UID
 * @param gid GID
 * @warning If @c getpwnam("nobody") fails, @c uid and @c gid remain unchanged
 */
PHPCHUID_VISIBILITY_HIDDEN void who_is_mr_nobody(uid_t* uid, gid_t* gid);

#endif /* HELPERS_H_ */

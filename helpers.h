/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3.3
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
PHPCHUID_VISIBILITY_HIDDEN int do_global_chroot(int can_chroot TSRMLS_DC);

/**
 * @brief Changes {R,E}UID/{R,E}GID to the owner of the DOCUMENT_ROOT
 * @return Whether operation succeeded
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 */
PHPCHUID_VISIBILITY_HIDDEN int change_uids(TSRMLS_D);

PHPCHUID_VISIBILITY_HIDDEN void deactivate(TSRMLS_D);
PHPCHUID_VISIBILITY_HIDDEN void globals_constructor(zend_chuid_globals* chuid_globals);

#endif /* PHPCHUID_HELPERS_H_ */

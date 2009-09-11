/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3
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
 * @brief Changes {R,E}UID/{R,E}GID to the owner of the DOCUMENT_ROOT
 * @return Whether operation succeeded
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 */
PHPCHUID_VISIBILITY_HIDDEN int change_uids(int method TSRMLS_DC);

PHPCHUID_VISIBILITY_HIDDEN void chuid_zend_extension_register(zend_extension* new_extension, DL_HANDLE handle);
PHPCHUID_VISIBILITY_HIDDEN int chuid_zend_remove_extension(zend_extension* extension);

#endif /* PHPCHUID_HELPERS_H_ */

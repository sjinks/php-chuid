/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@free-sevastopol.com>
 * @version 0.5.0
 * @brief Helper functions — definitions
 */

#ifndef PHPCHUID_HELPERS_H_
#define PHPCHUID_HELPERS_H_

#include "php_chuid.h"

/**
 * @brief Disables <code>posix_set{e,}{u,g}id()</code> PHP functions if told by @c chuid.disable_posix_setuid_family
 */
PHPCHUID_VISIBILITY_HIDDEN void disable_posix_setuids(TSRMLS_D);

/**
 * @brief <code>chroot()</code>'s to the directory specified by the @c root parameter
 * @param root New root directory
 * @return Whether the operation was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No (@c chroot() or @c chdir() failed, @c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int do_chroot(const char* root TSRMLS_DC);

/**
 * @brief Sets RUID/EUID/SUID and RGID/EGID/SGID
 * @param uid Real and Effective UID
 * @param gid Real and Effective GID
 * @return Whether calls to <code>my_setgids()</code>/<code>my_setuids()</code> were successful
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 */
PHPCHUID_VISIBILITY_HIDDEN int set_guids(uid_t uid, gid_t gid TSRMLS_DC);

/**
 * @brief Gets <code>DOCUMENT_ROOT</code>'s owner UID and GID
 * @param uid [out] UID to set
 * @param gid [out] GID to set
 * @note Both @c uid and @c gid must be non-null
 */
PHPCHUID_VISIBILITY_HIDDEN void get_docroot_guids(uid_t* uid, gid_t* gid TSRMLS_DC);

/**
 * @brief Deactivation function
 */
PHPCHUID_VISIBILITY_HIDDEN void deactivate(TSRMLS_D);

/**
 * @brief Globals constructor
 * @param chuid_globals Pointer to the module globals
 * @see zend_chuid_globals
 */
PHPCHUID_VISIBILITY_HIDDEN void globals_constructor(zend_chuid_globals* chuid_globals);

#endif /* PHPCHUID_HELPERS_H_ */

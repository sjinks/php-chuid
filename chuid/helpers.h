/**
 * @file
 * @author Volodymyr Kolesnykov <volodymyr@wildwolf.name>
 * @version 0.5.0
 * @brief Helper functions — definitions
 */

#ifndef PHPCHUID_HELPERS_H_
#define PHPCHUID_HELPERS_H_

#include "php_chuid.h"

/**
 * @brief Sets the Real, Effective and Saved User ID
 * @param ruid Real UID
 * @param euid Effective UID
 * @param mode Function to be used
 * @return Whether call succeeded
 * @retval 0 Yes
 * @retval -1 No (@c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int my_setuids(uid_t ruid, uid_t euid, enum change_xid_mode_t mode);

/**
 * @brief Sets the Real, Effective and Saved Group ID
 * @param rgid Real GID
 * @param egid Effective GID
 * @param mode Function to be used
 * @return Whether call succeeded
 * @retval 0 Yes
 * @retval -1 No (@c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int my_setgids(gid_t rgid, gid_t egid, enum change_xid_mode_t mode);

/**
 * @brief Disables <code>posix_set{e,}{u,g}id()</code> PHP functions if told by @c chuid.disable_posix_setuid_family
 */
PHPCHUID_VISIBILITY_HIDDEN void disable_posix_setuids();

/**
 * @brief <code>chroot()</code>'s to the directory specified by the @c root parameter
 * @param root New root directory
 * @return Whether the operation was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No (@c chroot() or @c chdir() failed, @c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int do_chroot(const char* root);

/**
 * @brief Sets RUID/EUID/SUID and RGID/EGID/SGID
 * @param uid Real and Effective UID
 * @param gid Real and Effective GID
 * @return Whether calls to <code>my_setgids()</code>/<code>my_setuids()</code> were successful
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 */
PHPCHUID_VISIBILITY_HIDDEN int set_guids(uid_t uid, gid_t gid);

/**
 * @brief Gets <code>DOCUMENT_ROOT</code>'s owner UID and GID
 * @param uid [out] UID to set
 * @param gid [out] GID to set
 * @note Both @c uid and @c gid must be non-null
 */
PHPCHUID_VISIBILITY_HIDDEN void get_docroot_guids(uid_t* uid, gid_t* gid);

/**
 * @brief Deactivation function
 */
PHPCHUID_VISIBILITY_HIDDEN void deactivate();


PHPCHUID_VISIBILITY_HIDDEN zend_bool chuid_is_auto_global(const char* name, size_t len);

#endif /* PHPCHUID_HELPERS_H_ */

/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3.6
 * @brief Compatibilty related stuff — definitions
 */

#ifdef DOXYGEN
#	undef PHPCHUID_COMPATIBILITY_H_
#endif

#ifndef PHPCHUID_COMPATIBILITY_H_
#define PHPCHUID_COMPATIBILITY_H_

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
 * @brief Gets the Real and Effective User ID
 * @param ruid Real UID
 * @param euid Effective UID
 */
PHPCHUID_VISIBILITY_HIDDEN void my_getuids(uid_t* restrict ruid, uid_t* restrict euid);

/**
 * @brief Gets the Real and Effective Group ID
 * @param rgid Real GID
 * @param egid Effective GID
 */
PHPCHUID_VISIBILITY_HIDDEN void my_getgids(gid_t* restrict rgid, gid_t* restrict egid);

#endif /* PHPCHUID_COMPATIBILITY_H_ */


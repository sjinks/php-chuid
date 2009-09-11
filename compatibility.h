/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.2
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
 * @param suid Saved UID
 * @param method If 0, use @c setuid(), <code>setresuid()</code>/<code>seteuid()</code> otherwise
 * @return Whether call succeeded
 * @retval 0 Yes
 * @retval -1 No (@c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int my_setuids(uid_t ruid, uid_t euid, uid_t suid, int method);

/**
 * @brief Sets the Real, Effective and Saved Group ID
 * @param rgid Real GID
 * @param egid Effective GID
 * @param sgid Saved GID
 * @param method If 0, use @c setgid(), <code>setresgid()</code>/<code>setegid()</code> otherwise
 * @return Whether call succeeded
 * @retval 0 Yes
 * @retval -1 No (@c errno will be set)
 */
PHPCHUID_VISIBILITY_HIDDEN int my_setgids(gid_t rgid, gid_t egid, gid_t sgid, int method);

/**
 * @brief Gets the Real and Effective User ID
 * @param ruid Real UID
 * @param euid Effective UID
 */
PHPCHUID_VISIBILITY_HIDDEN void my_getuids(uid_t* ruid, uid_t* euid);

/**
 * @brief Gets the Real and Effective Group ID
 * @param rgid Real GID
 * @param egid Effective GID
 */
PHPCHUID_VISIBILITY_HIDDEN void my_getgids(gid_t* rgid, gid_t* egid);

#endif /* PHPCHUID_COMPATIBILITY_H_ */

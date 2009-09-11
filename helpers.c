/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.2
 * @brief Helper functions — implementation
 */

#include "helpers.h"
#include "caps.h"

/**
 * @details Disables @c posix_setegid(), @c posix_seteuid(), @c posix_setgid() and @c posix_setuid() functions
 * if @c chuid_globals.disable_setuid is not zero
 * @note If @c HAVE_SETRESUID constant is not defined (i.e., the system does not have @c setresuid() call)
 * and the extension is build without @c libcap support, @c posix_kill(), @c pcntl_setpriority() and @c proc_nice()
 * functions are also disabled, because @c seteuid() changes only the effective UID,
 * not the real one, and it is Real UID that affects those functions' behavior
 */
void disable_posix_setuids(TSRMLS_D)
{
	if (0 != CHUID_G(disable_setuid)) {
		zend_disable_function("posix_setegid", sizeof("posix_setegid")-1 TSRMLS_CC);
		zend_disable_function("posix_seteuid", sizeof("posix_seteuid")-1 TSRMLS_CC);
		zend_disable_function("posix_setgid",  sizeof("posix_setgid")-1  TSRMLS_CC);
		zend_disable_function("posix_setuid",  sizeof("posix_setuid")-1  TSRMLS_CC);
#if !defined(HAVE_SETRESUID) && !defined(WITH_CAP_LIBRARY)
		zend_disable_function("pcntl_setpriority", sizeof("pcntl_setpriority")-1 TSRMLS_CC);
		zend_disable_function("posix_kill",        sizeof("posix_kill")-1        TSRMLS_CC);
		zend_disable_function("proc_nice",         sizeof("proc_nice")-1         TSRMLS_CC);
#endif
	}
}

/**
 * @note If the call to @c chroot() succeeds, the function immediately <code>chdir()</code>'s to the target directory
 * @note @c chuid_globals.global_chroot must begin with <code>/</code> — the path must be absolute
 */
int do_global_chroot(int can_chroot)
{
	int severity        = (0 == be_secure) ? E_WARNING : E_CORE_ERROR;
	char* global_chroot = CHUID_G(global_chroot);

	if (NULL != global_chroot && '\0' != *global_chroot && '/' == *global_chroot) {
		if ((int)CAP_CLEAR != can_chroot) {
			int res;

			res = chroot(global_chroot); if (0 != res) { PHPCHUID_ERROR(severity, "chroot(%s): %s", global_chroot, strerror(errno)); return FAILURE; }
			res = chdir(global_chroot);  if (0 != res) { PHPCHUID_ERROR(severity, "chdir(%s): %s", global_chroot, strerror(errno));  return FAILURE; }
		}
		else {
			PHPCHUID_ERROR(severity, "%s", "chuid module requires CAP_SYS_ROOT capability (or root privileges) for chuid.global_chroot to take effect");
			return FAILURE;
		}
	}

	return SUCCESS;
}

void who_is_mr_nobody(uid_t* uid, gid_t* gid)
{
	struct passwd* pwd = getpwnam("nobody");
	if (NULL != pwd) {
		*uid = pwd->pw_uid;
		*gid = pwd->pw_gid;
	}
	else {
		PHPCHUID_ERROR(E_WARNING, "getpwnam(nobody) failed: %s", strerror(errno));
	}
}

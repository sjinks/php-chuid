/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@free-sevastopol.com>
 * @version 0.5.0
 * @brief Helper functions — implementation
 */

#include <assert.h>
#include "helpers.h"
#include "caps.h"
#include "compatibility.h"

int sapi_is_cli = -1; /**< Whether SAPI is CLI */
int sapi_is_cgi = -1; /**< Whether SAPI is CGI */

/**
 * @brief Hash table with the names of the blacklisted functions
 * @note Gets populated in @c disable_posix_setuids() and destroyed in @c zm_shutdown_chuid() only if <code>CHUID_G(disable_setuid)</code> is not zero.
 */
HashTable blacklisted_functions;

/**
 * @brief Saved @c zend_execute_internal()
 * @note Initialized in @c disable_posix_setuids() and restored in @c zm_shutdown_chuid() only if <code>CHUID_G(disable_setuid)</code> is not zero.
 */
void (*old_execute_internal)(zend_execute_data* execute_data_ptr, int return_value_used TSRMLS_DC);

/**
 * @brief @c nobody user ID
 */
uid_t uid_nobody = 65534;

/**
 * @brief @c nogroup group ID
 */
gid_t gid_nogroup = 65534;

/**
 * @brief Function execution handler
 * @param execute_data_ptr Zend Execute Data
 * @param return_value_used Whether the return value is used
 */
static void chuid_execute_internal(zend_execute_data* execute_data_ptr, int return_value_used TSRMLS_DC)
{
	const char* lcname = ((zend_internal_function*)execute_data_ptr->function_state.function)->function_name;
	size_t lcname_len  = strlen(lcname);

#ifdef ZEND_ENGINE_2
	zend_class_entry* ce = ((zend_internal_function*)execute_data_ptr->function_state.function)->scope;

	if (NULL == ce) {
#endif

		if (0 != CHUID_G(disable_setuid)) {
			int res = zend_hash_exists(&blacklisted_functions, lcname, lcname_len+1);

			if (0 != res) {
				zend_error(E_ERROR, "%s() has been disabled for security reasons", get_active_function_name(TSRMLS_C));
				zend_bailout();
				return;
			}
		}
#ifdef ZEND_ENGINE_2
	}
#endif

	old_execute_internal(execute_data_ptr, return_value_used TSRMLS_CC);
}

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
		unsigned long int dummy = 0;
		zend_hash_init(&blacklisted_functions, 8, NULL, NULL, 1);
		zend_hash_add(&blacklisted_functions, "posix_setegid", sizeof("posix_setegid"), &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_seteuid", sizeof("posix_seteuid"), &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_setgid",  sizeof("posix_setgid"),  &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_setuid",  sizeof("posix_setuid"),  &dummy, sizeof(dummy), NULL);
#if !defined(HAVE_SETRESUID) && !defined(WITH_CAP_LIBRARY)
		zend_hash_add(&blacklisted_functions, "pcntl_setpriority", sizeof("pcntl_setpriority"), &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_kill",        sizeof("posix_kill"),        &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "proc_nice",         sizeof("proc_nice"),         &dummy, sizeof(dummy), NULL);
#endif

		old_execute_internal = zend_execute_internal;
		if (NULL == old_execute_internal) {
			old_execute_internal = execute_internal;
		}

		zend_execute_internal = chuid_execute_internal;
	}
}

/**
 * @note If the call to @c chroot() succeeds, the function immediately <code>chdir()</code>'s to the target directory
 * @note @c root must begin with <code>/</code> — the path must be absolute
 */
int do_chroot(const char* root TSRMLS_DC)
{
	if (root && *root && '/' == *root) {
		int res;

		res = chdir(root);  if (res) { PHPCHUID_ERROR(E_CORE_ERROR, "chdir(\"%s\"): %s", root, strerror(errno));  return FAILURE; }
		res = chroot(root); if (res) { PHPCHUID_ERROR(E_CORE_ERROR, "chroot(\"%s\"): %s", root, strerror(errno)); return FAILURE; }
	}

	return SUCCESS;
}

/**
 * Sets Real and Effective UIDs to @c uid, Real and Effective GIDs to @c gid, Saved UID and GID to 0
 */
int set_guids(uid_t uid, gid_t gid TSRMLS_DC)
{
	int res;
	enum change_xid_mode_t mode = CHUID_G(mode);

	if (cxm_setresxid == mode || cxm_setxid == mode) {
		res = my_setgids(gid, gid, mode);
		if (0 != res) {
			PHPCHUID_ERROR(E_CORE_ERROR, "my_setgids(%d, %d, %d): %s", gid, gid, (int)mode, strerror(errno));
			return FAILURE;
		}
	}

	res = my_setuids(uid, uid, mode);
	if (0 != res) {
		PHPCHUID_ERROR(E_CORE_ERROR, "my_setuids(%d, %d, %d): %s", uid, uid, (int)mode, strerror(errno));
		return FAILURE;
	}

	return SUCCESS;
}

/**
 * Tries to get UID and GID of the owner of the @c DOCUMENT_ROOT.
 * If @c stat() fails on the @c DOCUMENT_ROOT or @c DOCUMENT_ROOT is not set, defaults are used.
 * If default UID is 65534, UID and GID are set to @c nobody and @c nogroup
 */
void get_docroot_guids(uid_t* uid, gid_t* gid TSRMLS_DC)
{
	char* docroot;
	char* docroot_corrected;
	int res;
	struct stat statbuf;

	assert(uid != NULL);
	assert(gid != NULL);

	*gid = (gid_t)CHUID_G(default_gid);
	*uid = (uid_t)CHUID_G(default_uid);

	if (65534 == *uid) {
		*uid = uid_nobody;
		*gid = gid_nogroup;
	}

	if (NULL != sapi_module.getenv) {
		docroot = sapi_module.getenv("DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT")-1 TSRMLS_CC);
	}
	else {
		docroot = NULL;
	}

	if (NULL == docroot) {
		PHPCHUID_ERROR(E_WARNING, "%s", "Cannot get DOCUMENT_ROOT");
		return;
	}

	docroot_corrected = docroot && *docroot ? docroot : "/";

	res = stat(docroot_corrected, &statbuf);
	if (0 != res) {
		PHPCHUID_ERROR(E_WARNING, "stat(%s): %s", docroot_corrected, strerror(errno));
		return;
	}

	if (CHUID_G(never_root)) {
		if (0 != statbuf.st_uid) {
			*uid = statbuf.st_uid;
		}

		if (0 != statbuf.st_gid) {
			*gid = statbuf.st_gid;
		}
	}
	else {
		*uid = statbuf.st_uid;
		*gid = statbuf.st_gid;
	}
}

/**
 * If the module is active, sets back the original UID/GID and depending on the ini settings, escapes the chroot.
 */
void deactivate(TSRMLS_D)
{
	PHPCHUID_DEBUG("%s\n", "deactivate");

	if (1 == CHUID_G(active)) {
		int res;
		uid_t ruid = CHUID_G(ruid);
		uid_t euid = CHUID_G(euid);
		gid_t rgid = CHUID_G(rgid);
		gid_t egid = CHUID_G(egid);
		enum change_xid_mode_t mode = CHUID_G(mode);

		res = my_setuids(ruid, euid, mode);
		if (0 != res) {
			PHPCHUID_ERROR(E_ERROR, "my_setuids(%d, %d, %d): %s", ruid, euid, (int)mode, strerror(errno));
		}

		if (cxm_setresxid == mode || cxm_setxid == mode) {
			res = my_setgids(rgid, egid, mode);
			if (0 != res) {
				PHPCHUID_ERROR(E_ERROR, "my_setgids(%d, %d, %d): %s", rgid, egid, (int)mode, strerror(errno));
			}
		}

#if HAVE_FCHDIR && HAVE_CHROOT
		if (CHUID_G(per_req_chroot)) {
			int res;

			res = fchdir(CHUID_G(root_fd));
			if (res) {
				PHPCHUID_ERROR(E_ERROR, "fchdir() failed: %s", strerror(errno));
			}
			else {
				res = chroot(".");
				if (res) {
					PHPCHUID_ERROR(E_ERROR, "chroot(\".\") failed: %s", strerror(errno));
				}
			}
		}
#endif
	}
}

void globals_constructor(zend_chuid_globals* chuid_globals)
{
	struct passwd* pwd;

	assert(-1 == sapi_is_cli);
	assert(-1 == sapi_is_cgi);

	sapi_is_cli = (0 == strcmp(sapi_module.name, "cli"));
	sapi_is_cgi = (0 == strcmp(sapi_module.name, "cgi"));

	my_getuids(&chuid_globals->ruid, &chuid_globals->euid);
	my_getgids(&chuid_globals->rgid, &chuid_globals->egid);
	chuid_globals->active = 0;

	errno = 0;
	pwd   = getpwnam("nobody");
	if (NULL != pwd) {
		uid_nobody  = pwd->pw_uid;
		gid_nogroup = pwd->pw_gid;
	}
	else {
		PHPCHUID_ERROR(E_WARNING, "getpwnam(nobody) failed: %s", strerror(errno));
	}


#if HAVE_CHROOT
	chuid_globals->global_chroot  = NULL;
#if HAVE_FCHDIR
	chuid_globals->per_req_chroot = 0;
	chuid_globals->req_chroot     = NULL;
	chuid_globals->root_fd        = -1;
	chuid_globals->chrooted       = 0;
#endif
#endif
}

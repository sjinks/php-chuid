/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3.3
 * @brief Helper functions — implementation
 */

#include "helpers.h"
#include "caps.h"
#include "compatibility.h"

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
 * @brief Function execution handler
 * @param execute_data_ptr Zend Execute Data
 * @param return_value_used Whether the return value is used
 */
static void chuid_execute_internal(zend_execute_data* execute_data_ptr, int return_value_used TSRMLS_DC)
{
	char* lcname      = ((zend_internal_function*)execute_data_ptr->function_state.function)->function_name;
	size_t lcname_len = strlen(lcname);
/*	int ht            = execute_data_ptr->opline->extended_value; */
	zval* return_value;

#ifdef ZEND_ENGINE_2
	zend_class_entry* ce = ((zend_internal_function*)execute_data_ptr->function_state.function)->scope;
	int free_lcname      = 0;

	if (NULL != ce) {
		char *tmp = (char*)emalloc(lcname_len + 2 + ce->name_length + 1); /* Class::method\0 */
		memcpy(tmp,                       ce->name, ce->name_length);
		memcpy(tmp + ce->name_length,     "::",     2);
		memcpy(tmp + ce->name_length + 2, lcname,   lcname_len);
		lcname      = tmp;
		free_lcname = 1;
		lcname_len += ce->name_length + 2;
		lcname[lcname_len] = 0;
		zend_str_tolower(lcname, lcname_len);
	}
#endif

#ifdef ZEND_ENGINE_2
	return_value = (*(temp_variable*)((char*)execute_data_ptr->Ts + execute_data_ptr->opline->result.u.var)).var.ptr;
#else
	return_value = execute_data_ptr->Ts[execute_data_ptr->opline->result.u.var].var.ptr;
#endif

	if (0 != CHUID_G(disable_setuid)) {
		int res = zend_hash_exists(&blacklisted_functions, lcname, lcname_len+1);
#ifdef ZEND_ENGINE_2
		if (0 != free_lcname) {
			efree(lcname);
		}
#endif

		if (0 != res) {
			zend_error(E_ERROR, "%s() has been disabled for security reasons", get_active_function_name(TSRMLS_C));
			zend_bailout();
			return;
			/* To simulate an error instead: */
			/* RETURN_FALSE; */
		}
	}

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
		unsigned long dummy = 0;
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
 * @note @c chuid_globals.global_chroot must begin with <code>/</code> — the path must be absolute
 */
int do_global_chroot(int can_chroot TSRMLS_DC)
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

/**
 * @brief Finds out the UID and GID of @c nobody user.
 * @param uid UID
 * @param gid GID
 * @warning If @c getpwnam("nobody") fails, @c uid and @c gid remain unchanged
 */
static void who_is_mr_nobody(uid_t* uid, gid_t* gid)
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

/**
 * @brief Sets RUID/EUID/SUID and RGID/EGID/SGID
 * @param uid Real and Effective UID
 * @param gid Real and Effective GID
 * @return Whether calls to <code>my_setgids()</code>/<code>my_setuids()</code> were successful
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 *
 * Sets Real and Effective UIDs to @c uid, Real and Effective GIDs to @c gid, Saved UID and GID to 0
 */
static int do_set_guids(uid_t uid, gid_t gid TSRMLS_DC)
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
 * @brief Sets the default {R,E}UID/{R,E}GID according to the INI settings
 * @param method Which method should be used to set UIDs and GIDs
 * @return Whether call to @c do_set_guids() succeeded
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 * @see do_set_guids(), who_is_mr_nobody()
 * @note If the default UID is 65534, @c nobody user is assumed and its UID/GID are refined by @c who_is_mr_nobody()
 */
static int set_default_guids(TSRMLS_D)
{
	gid_t default_gid = (gid_t)CHUID_G(default_gid);
	uid_t default_uid = (uid_t)CHUID_G(default_uid);

	if (65534 == default_uid) {
		who_is_mr_nobody(&default_uid, &default_gid);
	}

	return do_set_guids(default_uid, default_gid TSRMLS_CC);
}

/**
 * @see set_default_guids()
 * @details Tries to change {R,E}{U,G}ID to the owner of the @c DOCUMENT_ROOT. If @c stat() fails on the @c DOCUMENT_ROOT or @c DOCUMENT_ROOT is not set, defaults are used.
 */
int change_uids(TSRMLS_D)
{
	char* docroot;
	int res;
	struct stat statbuf;
	uid_t uid;
	gid_t gid;

	if (NULL != sapi_module.getenv) {
		docroot = sapi_module.getenv("DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT")-1 TSRMLS_CC);
	}
	else {
		docroot = NULL;
	}

	if (NULL == docroot) {
		return set_default_guids(TSRMLS_C);
	}

	res = stat(docroot, &statbuf);
	if (0 != res) {
		PHPCHUID_ERROR(E_WARNING, "stat(%s): %s", docroot, strerror(errno));
		return set_default_guids(TSRMLS_C);
	}

	uid = statbuf.st_uid;
	gid = statbuf.st_gid;

	if (0 != CHUID_G(never_root)) {
		if (0 == uid) uid = (uid_t)CHUID_G(default_uid);
		if (0 == gid) gid = (gid_t)CHUID_G(default_gid);
	}

	return do_set_guids(uid, gid TSRMLS_CC);
}

void deactivate(TSRMLS_D)
{
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
	}
}

void globals_constructor(zend_chuid_globals* chuid_globals)
{
	my_getuids(&chuid_globals->ruid, &chuid_globals->euid);
	my_getgids(&chuid_globals->rgid, &chuid_globals->egid);
	chuid_globals->active = 0;
	chuid_globals->global_chroot = NULL;
}


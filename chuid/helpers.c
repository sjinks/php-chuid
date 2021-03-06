/**
 * @file
 * @author Volodymyr Kolesnykov <volodymyr@wildwolf.name>
 * @version 0.6.0
 * @brief Helper functions — implementation
 */

#include <assert.h>
#include <grp.h>
#include <Zend/zend.h>
#include <Zend/zend_string.h>
#include "helpers.h"
#include "caps.h"

int sapi_is_cli       = -1; /**< Whether SAPI is CLI */
int sapi_is_cgi       = -1; /**< Whether SAPI is CGI */
#ifdef ZTS
int sapi_is_supported = -1; /**< Whether SAPI is supported */
#endif

/**
 * @brief Hash table with the names of the blacklisted functions
 * @note Gets populated in @c disable_posix_setuids() and destroyed in @c zm_shutdown_chuid() only if <code>CHUID_G(disable_setuid)</code> is not zero.
 */
HashTable blacklisted_functions;

/**
 * @brief Saved @c zend_execute_internal()
 * @note Initialized in @c disable_posix_setuids() and restored in @c zm_shutdown_chuid() only if <code>CHUID_G(disable_setuid)</code> is not zero.
 */
void (*old_execute_internal)(zend_execute_data*, zval*);

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
static void chuid_execute_internal(zend_execute_data* execute_data_ptr, zval* return_value)
{
	zend_string* fname   = execute_data_ptr->func->common.function_name;
	zend_class_entry* ce = execute_data_ptr->func->common.scope;

	if (NULL == ce && 0 != CHUID_G(disable_setuid)) {
		int res;
		res = fname ? zend_hash_exists(&blacklisted_functions, fname) : 0;

		if (0 != res) {
			zend_error(E_ERROR, "%s() has been disabled for security reasons", get_active_function_name());
			zend_bailout();
			return;
		}
	}

	old_execute_internal(execute_data_ptr, return_value);
}

int my_setuids(uid_t ruid, uid_t euid, enum change_xid_mode_t mode)
{
	if (cxm_setuid == mode || cxm_setxid == mode) {
		return setuid(euid);
	}

	return setresuid(ruid, euid, 0);
}

int my_setgids(gid_t rgid, gid_t egid, enum change_xid_mode_t mode)
{
	if (cxm_setxid == mode) {
		return setgid(egid);
	}

	return setresgid(rgid, egid, 0);
}

/**
 * @details Disables @c posix_setegid(), @c posix_seteuid(), @c posix_setgid() and @c posix_setuid() functions
 * if @c chuid_globals.disable_setuid is not zero
 * @note If @c HAVE_SETRESUID constant is not defined (i.e., the system does not have @c setresuid() call)
 * and the extension is build without @c libcap or @c libcap-ng support, @c posix_kill(), @c pcntl_setpriority() and @c proc_nice()
 * functions are also disabled, because @c seteuid() changes only the effective UID,
 * not the real one, and it is Real UID that affects those functions' behavior
 */
void disable_posix_setuids()
{
	if (0 != CHUID_G(disable_setuid)) {
		zval dummy;
		ZVAL_LONG(&dummy, 1);
		zend_hash_init(&blacklisted_functions, 8, NULL, NULL, 1);
		zend_hash_str_add(&blacklisted_functions, "posix_setegid",     sizeof("posix_setegid")-1,     &dummy);
		zend_hash_str_add(&blacklisted_functions, "posix_seteuid",     sizeof("posix_seteuid")-1,     &dummy);
		zend_hash_str_add(&blacklisted_functions, "posix_setgid",      sizeof("posix_setgid")-1,      &dummy);
		zend_hash_str_add(&blacklisted_functions, "posix_setuid",      sizeof("posix_setuid")-1,      &dummy);
		zend_hash_str_add(&blacklisted_functions, "pcntl_setpriority", sizeof("pcntl_setpriority")-1, &dummy);
		zend_hash_str_add(&blacklisted_functions, "posix_kill",        sizeof("posix_kill")-1,        &dummy);
		zend_hash_str_add(&blacklisted_functions, "proc_nice",         sizeof("proc_nice")-1,         &dummy);

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
int do_chroot(const char* root)
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
int set_guids(uid_t uid, gid_t gid)
{
	int res;
	enum change_xid_mode_t mode = CHUID_G(mode);

	PHPCHUID_DEBUG("set_guids: mode=%d, uid=%d, gid=%d\n", (int)mode, (int)uid, (int)gid);

	if (cxm_setresxid == mode || cxm_setxid == mode) {
		res = setgroups(0, NULL);
		if (0 != res) {
			PHPCHUID_ERROR(E_CORE_WARNING, "Failed to clear the list of supplementary groups: %s", strerror(errno));
		}

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

static SAPI_INPUT_FILTER_FUNC(dummy_input_filter)
{
	if (new_val_len) {
		*new_val_len = val_len;
	}

	return 1;
}

/**
 * Tries to get UID and GID of the owner of the @c DOCUMENT_ROOT.
 * If @c stat() fails on the @c DOCUMENT_ROOT or @c DOCUMENT_ROOT is not set, defaults are used.
 * If default UID is 65534, UID and GID are set to @c nobody and @c nogroup
 */
void get_docroot_guids(uid_t* uid, gid_t* gid)
{
	char* docroot = NULL;
	char* docroot_corrected;
	int res;
	struct stat statbuf;
	zval server;

	assert(uid != NULL);
	assert(gid != NULL);

	ZVAL_UNDEF(&server);

	*gid = (gid_t)CHUID_G(default_gid);
	*uid = (uid_t)CHUID_G(default_uid);

	if (65534 == *uid) {
		*uid = uid_nobody;
		*gid = gid_nogroup;
	}

	if (NULL != sapi_module.getenv) {
		docroot = sapi_module.getenv(ZEND_STRL("DOCUMENT_ROOT"));
	}

	if (NULL == docroot && NULL != sapi_module.register_server_variables) {
		zval* value;
		zval old_server = PG(http_globals)[TRACK_VARS_SERVER];
#if PHP_MAJOR_VERSION >= 8
		unsigned int (*orig_input_filter)(int, const char*, char**, size_t, size_t*) = sapi_module.input_filter;
#else
		unsigned int (*orig_input_filter)(int, char*, char**, size_t, size_t*) = sapi_module.input_filter;
#endif
		sapi_module.input_filter = dummy_input_filter;

		array_init(&server);
		PG(http_globals)[TRACK_VARS_SERVER] = server;
		sapi_module.register_server_variables(&server);
		sapi_module.input_filter = orig_input_filter;

		value = zend_hash_str_find(Z_ARRVAL(server), ZEND_STRL("DOCUMENT_ROOT"));
		if (value && Z_TYPE_P(value) == IS_STRING) {
			docroot = Z_STRVAL_P(value);
		}

		PG(http_globals)[TRACK_VARS_SERVER] = old_server;
	}

	if (NULL == docroot) {
		PHPCHUID_ERROR(E_WARNING, "%s", "Cannot get DOCUMENT_ROOT");
		zval_ptr_dtor(&server);
		return;
	}

	docroot_corrected = (*docroot) ? docroot : "/";

	res = stat(docroot_corrected, &statbuf);
	if (0 != res) {
		PHPCHUID_ERROR(E_WARNING, "stat(%s): %s", docroot_corrected, strerror(errno));
		zval_ptr_dtor(&server);
		return;
	}

	zval_ptr_dtor(&server);

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
void deactivate()
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
	}
}

zend_bool chuid_is_auto_global(const char* name, size_t len)
{
	zend_string* n = zend_string_init(name, len, 0);
	zend_bool res  = zend_is_auto_global(n);
	zend_string_release(n);
	return res;
}

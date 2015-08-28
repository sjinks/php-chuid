/**
 * @file
 * @author Volodymyr Kolesnykov <volodymyr@wildwolf.name>
 * @version 0.6.0
 * @brief Helper functions — implementation
 */

#include <assert.h>
#include <grp.h>
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
#if PHP_VERSION_ID >= 70000
void (*old_execute_internal)(zend_execute_data*, zval* TSRMLS_DC);
#elif PHP_VERSION_ID >= 50500
void (*old_execute_internal)(zend_execute_data*, zend_fcall_info*, int TSRMLS_DC);
#else
void (*old_execute_internal)(zend_execute_data*, int TSRMLS_DC);
#endif

/**
 * @brief @c nobody user ID
 */
uid_t uid_nobody = 65534;

/**
 * @brief @c nogroup group ID
 */
gid_t gid_nogroup = 65534;

static inline void chuid_ptr_dtor(zval** v)
{
#if PHP_MAJOR_VERSION >= 7
	zval_ptr_dtor(*v);
#else
	zval_ptr_dtor(v);
#endif
}

/**
 * @brief Function execution handler
 * @param execute_data_ptr Zend Execute Data
 * @param return_value_used Whether the return value is used
 */
static void chuid_execute_internal(
	zend_execute_data* execute_data_ptr,
#if PHP_VERSION_ID >= 70000
	zval* return_value
#else
#  if PHP_VERSION_ID >= 50500
	zend_fcall_info* fci,
#  endif
	int return_value_used
#endif
	TSRMLS_DC
)
{
#if PHP_VERSION_ID >= 50300
	const
#endif
	char* lcname = ((zend_internal_function*)execute_data_ptr->function_state.function)->function_name;
	size_t lcname_len = strlen(lcname);

	zend_class_entry* ce = ((zend_internal_function*)execute_data_ptr->function_state.function)->scope;

	if (NULL == ce) {
		if (0 != CHUID_G(disable_setuid)) {
			int res = zend_hash_exists(&blacklisted_functions, lcname, lcname_len+1);

			if (0 != res) {
				zend_error(E_ERROR, "%s() has been disabled for security reasons", get_active_function_name(TSRMLS_C));
				zend_bailout();
				return;
			}
		}
	}

#if PHP_VERSION_ID >= 70000
	old_execute_internal(execute_data_ptr, return_value TSRMLS_CC);
#elif PHP_VERSION_ID >= 50500
	old_execute_internal(execute_data_ptr, fci, return_value_used TSRMLS_CC);
#else
	old_execute_internal(execute_data_ptr, return_value_used TSRMLS_CC);
#endif
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
void disable_posix_setuids(TSRMLS_D)
{
	if (0 != CHUID_G(disable_setuid)) {
#if PHP_MAJOR_VERSION >= 7
		zval dummy;
		ZVAL_LONG(&dummy, 1);
#else
		unsigned long int dummy = 0;
#endif
		zend_hash_init(&blacklisted_functions, 8, NULL, NULL, 1);
#if PHP_MAJOR_VERSION >= 7
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("posix_setegid"),     &dummy);
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("posix_seteuid"),     &dummy);
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("posix_setgid"),      &dummy);
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("posix_setuid"),      &dummy);
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("pcntl_setpriority"), &dummy);
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("posix_kill"),        &dummy);
		zend_hash_str_add(&blacklisted_functions, ZEND_STRS("proc_nice"),         &dummy);
#else
		zend_hash_add(&blacklisted_functions, "posix_setegid",     sizeof("posix_setegid"),     &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_seteuid",     sizeof("posix_seteuid"),     &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_setgid",      sizeof("posix_setgid"),      &dummy, sizeof(dummy), NULL);
		zend_hash_add(&blacklisted_functions, "posix_setuid",      sizeof("posix_setuid"),      &dummy, sizeof(dummy), NULL);
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

	PHPCHUID_DEBUG("set_guids: mode=%d, uid=%d, gid=%d\n", (int)mode, (int)uid, (int)gid);

	if (cxm_setresxid == mode || cxm_setxid == mode) {
		res = my_setgids(gid, gid, mode);
		if (0 != res) {
			PHPCHUID_ERROR(E_CORE_ERROR, "my_setgids(%d, %d, %d): %s", gid, gid, (int)mode, strerror(errno));
			return FAILURE;
		}

		if (setgroups(0, NULL)) {
			PHPCHUID_ERROR(E_CORE_WARNING, "Failed to clear the list of supplementary groups: %s", strerror(errno));
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
void get_docroot_guids(uid_t* uid, gid_t* gid TSRMLS_DC)
{
	char* docroot = NULL;
	char* docroot_corrected;
	int res;
	struct stat statbuf;
	zval* server = NULL;

	assert(uid != NULL);
	assert(gid != NULL);

	*gid = (gid_t)CHUID_G(default_gid);
	*uid = (uid_t)CHUID_G(default_uid);

	if (65534 == *uid) {
		*uid = uid_nobody;
		*gid = gid_nogroup;
	}

	if (NULL != sapi_module.getenv) {
		docroot = sapi_module.getenv(ZEND_STRL("DOCUMENT_ROOT") TSRMLS_CC);
	}

	if (NULL == docroot && NULL != sapi_module.register_server_variables) {
		zval** value;
		zval* old_server = PG(http_globals)[TRACK_VARS_SERVER];
		unsigned int (*orig_input_filter)(int arg, char *var, char **val, unsigned int val_len, unsigned int *new_val_len TSRMLS_DC) = sapi_module.input_filter;
		sapi_module.input_filter = dummy_input_filter;

		MAKE_STD_ZVAL(server);
		array_init(server);
		PG(http_globals)[TRACK_VARS_SERVER] = server;

		sapi_module.register_server_variables(server TSRMLS_CC);

		sapi_module.input_filter = orig_input_filter;

		if (SUCCESS == zend_hash_quick_find(Z_ARRVAL_P(server), ZEND_STRS("DOCUMENT_ROOT"), zend_inline_hash_func(ZEND_STRS("DOCUMENT_ROOT")), (void**)&value)) {
			if (Z_TYPE_PP(value) == IS_STRING) {
				docroot = Z_STRVAL_PP(value);
			}
		}

		PG(http_globals)[TRACK_VARS_SERVER] = old_server;
	}

	if (NULL == docroot) {
		PHPCHUID_ERROR(E_WARNING, "%s", "Cannot get DOCUMENT_ROOT");
		if (server) {
			chuid_ptr_dtor(&server);
		}

		return;
	}

	docroot_corrected = (*docroot) ? docroot : "/";

	res = stat(docroot_corrected, &statbuf);
	if (0 != res) {
		PHPCHUID_ERROR(E_WARNING, "stat(%s): %s", docroot_corrected, strerror(errno));
		if (server) {
			chuid_ptr_dtor(&server);
		}

		return;
	}

	if (server) {
		chuid_ptr_dtor(&server);
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

zend_bool chuid_is_auto_global(const char* name, size_t len TSRMLS_DC)
{
#if PHP_MAJOR_VERSION >= 7
	zend_string* n = STR_INIT(name, len, 0);
	zend_bool res  = zend_is_auto_global(n TSRMLS_CC);
	STR_RELEASE(n);
	return res;
#else
	return zend_is_auto_global(name, len TSRMLS_CC);
#endif
}

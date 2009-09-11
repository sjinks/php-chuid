#include "php_chuid.h"
#include <main/SAPI.h>
#include <ext/standard/info.h>

ZEND_DECLARE_MODULE_GLOBALS(chuid);

/**
 * @brief INI File Entries
 * <TABLE>
 * <TR><TD>@c chuid.disable_posix_setuid_family</TD><TD>@c bool</TD><TD>Disables @c posix_seteuid(), @c posix_setegid(), @c posix_setuid() and @c posix_setgid() functions</TD></TR>
 * <TR><TD>@c chuid.never_root</TD><TD>@c bool</TD><TD>Forces the change to the @c default_uid/@c default_gid of the UID/GID computes to be 0 (root)</TD></TR>
 * <TR><TD>@c chuid.default_uid</TD><TD>Default UID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the UID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TD>@c chuid.default_gid</TD><TD>Default GID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the GID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * </TABLE>
 */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("chuid.disable_posix_setuid_family", "1",     PHP_INI_SYSTEM, OnUpdateBool, disable_setuid, zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.never_root",                  "1",     PHP_INI_SYSTEM, OnUpdateBool, never_root,     zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_uid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong, default_uid,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_gid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong, default_gid,    zend_chuid_globals, chuid_globals)
PHP_INI_END()

int (*php_cgi_sapi_activate)(TSRMLS_D) = NULL;
int (*php_cgi_sapi_deactivate)(TSRMLS_D) = NULL;

/**
 * @brief Sets RUID/EUID/SUID and RGID/EGID/SGID
 * @param uid Real and Effective UID
 * @param gid Real and Effective GID
 * @return Whether calls to @c setresgid()/@c setresuid() were successful
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 *
 * Sets Real and Effective UIDs to @c uid, Real and Effective GIDs to @c gid, Saved UID and GID to 0
 */
static int do_set_guids(uid_t uid, gid_t gid)
{
	int res;

	res = setresgid(gid, gid, 0);
	if (0 != res) {
#ifdef DEBUG
		fprintf(stderr, "setresgid(%d, %d, 0): %s\n", gid, gid, strerror(errno));
#endif
		zend_error(E_ERROR, "setresgid(%d, %d, 0): %s", gid, gid, strerror(errno));
		return FAILURE;
	}

	res = setresuid(uid, uid, 0);
	if (0 != res) {
#ifdef DEBUG
		fprintf(stderr, "setresuid(%d, %d, 0): %s\n", uid, uid, strerror(errno));
#endif
		zend_error(E_ERROR, "setresuid(%d, %d, 0): %s", uid, uid, strerror(errno));
		return FAILURE;
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
#ifdef DEBUG
		fprintf(stderr, "getpwnam(nobody) failed: %s\n", strerror(errno));
#endif
		zend_error(E_WARNING, "getpwnam(nobody) failed: %s", strerror(errno));
	}
}

/**
 * @brief Sets the default {R,E}UID/{R,E}GID according to the INI settings
 * @return Whether call to @c do_set_guids() succeeded
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 * @see do_set_guids(), who_is_mr_nobody()
 * @note If the default UID is 65534, @c nobody user is assumed and its UID/GID are refined by @c who_is_mr_nobody()
 */
static int set_default_guids(void)
{
	gid_t default_gid = (gid_t)CHUID_G(default_gid);
	uid_t default_uid = (uid_t)CHUID_G(default_uid);

	if (65534 == default_uid) {
		who_is_mr_nobody(&default_uid, &default_gid);
	}

	return do_set_guids(default_uid, default_gid);
}

/**
 * @brief Changes {R,E}UID/{R,E}GID to the owner of the DOCUMENT_ROOT
 * @return Whether operation succeeded
 * @retval SUCCESS OK
 * @retval FAILURE Failure
 * @see set_default_guids()
 *
 * Tries to change {R,E}{U,G}ID to the owner of the @c DOCUMENT_ROOT. If @c stat() fails on the @c DOCUMENT_ROOT or @c DOCUMENT_ROOT is not set, defaults are used.
 */
static int change_uids(TSRMLS_D)
{
	char* docroot;
	int res;
	struct stat statbuf;
	uid_t uid;
	gid_t gid;

	docroot = sapi_module.getenv("DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT")-1 TSRMLS_CC);
	if (NULL == docroot) {
		return set_default_guids();
	}

	res = stat(docroot, &statbuf);
	if (0 != res) {
#ifdef DEBUG
		fprintf(stderr, "stat(%s): %s\n", docroot, strerror(errno));
#endif
		zend_error(E_WARNING, "stat(%s): %s", docroot, strerror(errno));
		return set_default_guids();
	}

	uid = statbuf.st_uid;
	gid = statbuf.st_gid;

	if (0 != CHUID_G(never_root)) {
		if (0 == uid) uid = (uid_t)CHUID_G(default_uid);
		if (0 == gid) gid = (gid_t)CHUID_G(default_gid);
	}

	return do_set_guids(uid, gid);
}

/**
 * @brief SAPI Activation Hook
 * @return @c SUCCESS or @c FAILURE
 */
static int sapi_cgi_activate(TSRMLS_D)
{
	if (php_cgi_sapi_activate) {
		int res = php_cgi_sapi_activate(TSRMLS_C);
		if (SUCCESS != res) {
			return res;
		}
	}

	if (0 != CHUID_G(euid)) {
		zend_error(E_WARNING, "chuid module requires that PHP be running as root");
		return SUCCESS;
	}

	if (0 != CHUID_G(disable_setuid)) {
		zend_disable_function("posix_setegid", sizeof("posix_setegid")-1 TSRMLS_CC);
		zend_disable_function("posix_seteuid", sizeof("posix_seteuid")-1 TSRMLS_CC);
		zend_disable_function("posix_setgid",  sizeof("posix_setgid")-1  TSRMLS_CC);
		zend_disable_function("posix_setuid",  sizeof("posix_setuid")-1  TSRMLS_CC);
	}

	CHUID_G(active) = 1;

	return change_uids();
}

/**
 * @brief SAPI Deactivation Hook
 * @return @c SUCCESS or @c FAILURE
 */
static int sapi_cgi_deactivate(TSRMLS_D)
{
	if (0 != CHUID_G(active)) {
		int res;

		res = setresgid(CHUID_G(rgid), CHUID_G(egid), -1);
		if (0 != res) {
#ifdef DEBUG
			fprintf(stderr, "setresgid(%d, %d, -1): %s\n", CHUID_G(rgid), CHUID_G(egid), strerror(errno));
#endif
			zend_error(E_ERROR, "setresgid(%d, %d, -1): %s", CHUID_G(rgid), CHUID_G(egid), strerror(errno));
		}

		res = setresuid(CHUID_G(ruid), CHUID_G(euid), -1);
		if (0 != res) {
#ifdef DEBUG
			fprintf(stderr, "setresuid(%d, %d, -1): %s\n", CHUID_G(ruid), CHUID_G(euid), strerror(errno));
#endif
			zend_error(E_WARNING, "setresuid(%d, %d, -1): %s", CHUID_G(ruid), CHUID_G(euid), strerror(errno));
		}
	}

	if (php_cgi_sapi_deactivate) {
		return php_cgi_sapi_deactivate(TSRMLS_C);
	}

	return SUCCESS;
}

static PHP_MINIT_FUNCTION(chuid)
{
	REGISTER_INI_ENTRIES();

	php_cgi_sapi_activate   = sapi_module.activate;
	php_cgi_sapi_deactivate = sapi_module.deactivate;
	sapi_module.activate    = sapi_cgi_activate;
	sapi_module.deactivate  = sapi_cgi_deactivate;

	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(chuid)
{
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

static PHP_GINIT_FUNCTION(chuid)
{
	uid_t dummy_uid;
	gid_t dummy_gid;

	getresuid(&chuid_globals->ruid, &chuid_globals->euid, &dummy_uid);
	getresgid(&chuid_globals->rgid, &chuid_globals->egid, &dummy_gid);
	chuid_globals->active = 0;
}

static PHP_MINFO_FUNCTION(chuid)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Change User ID Module", "enabled");
	php_info_print_table_row(2, "version", PHP_CHUID_EXTVER);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

zend_module_entry chuid_module_entry = {
    STANDARD_MODULE_HEADER_EX,
	NULL,
	NULL,
	PHP_CHUID_EXTNAME,
	NULL,
	PHP_MINIT(chuid),
	PHP_MSHUTDOWN(chuid),
	NULL,
	NULL,
	PHP_MINFO(chuid),
	PHP_CHUID_EXTVER,
	PHP_MODULE_GLOBALS(chuid),
	PHP_GINIT(chuid),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};


#ifdef COMPILE_DL_CHUID
ZEND_GET_MODULE(chuid)
#endif

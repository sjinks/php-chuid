/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3.3
 * @brief PHP CHUID Module
 */

#include "php_chuid.h"
#include <ext/standard/info.h>
#include "compatibility.h"
#include "caps.h"
#include "helpers.h"
#include "extension.h"

#ifndef PHP_GINIT
/**
 * @note For pre-5.2 PHPs which fo not have PHP_GINIT and PHP_GSHUTDOWN
 * @param chuid_globals Pointer to the extension globals
 */
static void chuid_globals_ctor(zend_chuid_globals* chuid_globals TSRMLS_DC);
#endif

#ifndef ZEND_ENGINE_2
	/**
	 * @note Zend Engine 1 has OnUpdateInt instead of OnUpdateLong
	 */
#	define OnUpdateLong OnUpdateInt
#endif

zend_bool be_secure = 1;          /**< Whether we should turn startup warnings to errors */

/**
 * @brief Module globals
 */
ZEND_DECLARE_MODULE_GLOBALS(chuid);

/**
 * @brief INI File Entries
 *
 * <TABLE>
 * <TR><TD>@c chuid.enabled</TD><TD>@c bool</TD><TD>Whether this extension should be enabled</TD></TR>
 * <TR><TD>@c chuid.disable_posix_setuid_family</TD><TD>@c bool</TD><TD>Disables @c posix_seteuid(), @c posix_setegid(), @c posix_setuid() and @c posix_setgid() functions</TD></TR>
 * <TR><TD>@c chuid.never_root</TD><TD>@c bool</TD><TD>Forces the change to the @c default_uid/@c default_gid of the UID/GID computes to be 0 (root)</TD></TR>
 * <TR><TD>@c chuid.cli_disable</TD><TD>@c bool</TD><TD>Do not try to modify UIDs/GIDs when SAPI is CLI</TD></TR>
 * <TR><TD>@c chuid.be_secure</TD><TD>@c bool</TD><TD>Turns some warnings to errors (for the sake of security)</TD></TR>
 * <TR><TD>@c chuid.no_set_gid</TD><TD>@c bool</TD><TD>Do not change GID</TD></TR>
 * <TR><TD>@c chuid.default_uid</TD><TD>@c int</TD><TD>Default UID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the UID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TD>@c chuid.default_gid</TD><TD>@c int</TD><TD>Default GID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the GID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TD>@c chuid.global_chroot</TD><TD>@c string</TD><TD>@c chroot() to this location before processing the request</TD></TR>
 * <TR><TD>@c chuid.force_gid</TD><TD>@c int</TD><TD>Force setting this GID. If positive, @c CAP_SETGID privilege will be dropped. Takes precedence over @c chuid.default_gid</TD></TR>
 * </TABLE>
 */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("chuid.enabled",                     "1",     PHP_INI_SYSTEM, OnUpdateBool,   enabled,        zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.disable_posix_setuid_family", "1",     PHP_INI_SYSTEM, OnUpdateBool,   disable_setuid, zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.never_root",                  "1",     PHP_INI_SYSTEM, OnUpdateBool,   never_root,     zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.cli_disable",                 "1",     PHP_INI_SYSTEM, OnUpdateBool,   cli_disable,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.be_secure",                   "1",     PHP_INI_SYSTEM, OnUpdateBool,   be_secure,      zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.no_set_gid",                  "0",     PHP_INI_SYSTEM, OnUpdateBool,   no_set_gid,     zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_uid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong,   default_uid,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_gid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong,   default_gid,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.global_chroot",                 NULL,    PHP_INI_SYSTEM, OnUpdateString, global_chroot,  zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.force_gid",                     "-1",    PHP_INI_SYSTEM, OnUpdateLong,   forced_gid,     zend_chuid_globals, chuid_globals)
PHP_INI_END()

/**
 * @brief Module Initialization Routine
 * @param type
 * @param module_number
 * @return Whether initialization was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No
 */
static PHP_MINIT_FUNCTION(chuid)
{
	int can_dac_read_search = -1;
	int can_chroot          = -1;
	int can_setgid          = -1;
	int can_setuid          = -1;
	int severity;
	int retval;
	long int forced_gid;
	zend_bool no_gid;
	int num_caps = 0;
	cap_value_t caps[4];

#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "MINIT");
#endif

#ifndef PHP_GINIT
#ifdef ZTS
	ts_allocate_id(&chuid_globals_id, sizeof(zend_chuid_globals), (ts_allocate_ctor)chuid_globals_ctor, NULL);
#else
	chuid_globals_ctor(&chuid_globals TSRMLS_CC);
#endif /* ZTS */
#endif /* PHP_GINIT */

	REGISTER_INI_ENTRIES();

	if (0 == CHUID_G(enabled)) {
		return SUCCESS;
	}

	be_secure  = CHUID_G(be_secure);
	severity   = (0 == be_secure) ? E_WARNING : E_CORE_ERROR;
	retval     = (0 == be_secure) ? SUCCESS : FAILURE;
	forced_gid = CHUID_G(forced_gid);
	no_gid     = CHUID_G(no_set_gid);

	disable_posix_setuids(TSRMLS_C);

	if (0 != check_capabilities(&can_chroot, &can_dac_read_search, &can_setuid, &can_setgid) && 0 != be_secure) {
		zend_error(E_CORE_ERROR, "check_capabilities() failed");
		return FAILURE;
	}

	if (FAILURE == do_global_chroot(can_chroot TSRMLS_CC) && 0 != be_secure) {
		zend_error(E_CORE_ERROR, "do_global_chroot() failed");
		return FAILURE;
	}

	if (0 == sapi_is_cli || 0 == CHUID_G(cli_disable)) {
		if ((int)CAP_CLEAR == can_dac_read_search || (int)CAP_CLEAR == can_setuid || (int)CAP_CLEAR == can_setgid) {
			PHPCHUID_ERROR(severity, "%s", "chuid module requires that these capabilities (or root privileges) be set: CAP_DAC_READ_SEARCH, CAP_SETGID, CAP_SETUID");
			return retval;
		}

		if (forced_gid > 0) {
			if (0 != setgid((gid_t)forced_gid)) {
				zend_error(E_CORE_ERROR, "setgid(%ld) failed", forced_gid);
				return FAILURE;
			}
		}

		if (0 != sapi_is_cli || 0 != sapi_is_cgi) {
			CHUID_G(mode) = (forced_gid < 1 && 0 == no_gid) ? cxm_setxid : cxm_setuid;
		}
		else {
			CHUID_G(mode) = (forced_gid < 1 && 0 == no_gid) ? cxm_setresxid : cxm_setresuid;
		}


#if defined(WITH_CAP_LIBRARY) && !defined(ZTS)
		if (forced_gid < 1 && 0 == no_gid) {
			caps[num_caps] = CAP_SETGID;
			++num_caps;
		}
#endif

#if defined(WITH_CAP_LIBRARY)
#	if !defined(ZTS)
		caps[num_caps] = CAP_SETUID;
		++num_caps;
#	endif

		caps[num_caps] = CAP_DAC_READ_SEARCH;
		++num_caps;
#endif

		if (0 != drop_capabilities(num_caps, caps)) {
			zend_error(E_CORE_ERROR, "drop_capabilities() failed");
			return FAILURE;
		}

#ifndef ZTS
		CHUID_G(active) = 1;
#endif
	}

	return SUCCESS;
}

/**
 * @brief Module Shutdown Routine
 * @param type
 * @param module_number
 * @return Whether shutdown was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No (should never be returned)
 */
static PHP_MSHUTDOWN_FUNCTION(chuid)
{
#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "MSHUTDOWN");
#endif

	if (0 != CHUID_G(enabled) && 0 != CHUID_G(disable_setuid)) {
		zend_hash_clean(&blacklisted_functions);
		zend_execute_internal = (old_execute_internal == execute_internal) ? NULL : old_execute_internal;
	}

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

#ifdef PHP_GINIT

/**
 * @brief Globals Constructor
 * @param chuid_globals Pointer to the globals container
 *
 * Gets the original values of Effective and Eeal User and Group IDs and sets @c global_chroot to @c NULL and @c active to 0.
 */
static PHP_GINIT_FUNCTION(chuid)
{
#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "GINIT");
#endif

	globals_constructor(chuid_globals);
}

#else

static void chuid_globals_ctor(zend_chuid_globals* chuid_globals TSRMLS_DC)
{
#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "Globals Constructor");
#endif

	globals_constructor(chuid_globals);
}

#endif

/**
 * @brief Module Information
 * @param zend_module
 *
 * Used by @c phpinfo()
 */
static PHP_MINFO_FUNCTION(chuid)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Change User ID Module", "enabled");
	php_info_print_table_row(2, "version", PHP_CHUID_EXTVER);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

#ifdef ZEND_MODULE_POST_ZEND_DEACTIVATE_N
/**
 * @brief Request Post Deactivate Routine
 * @return Whether shutdown was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No
 *
 * Restores UIDs and GIDs to their original values after the request has been processed.
 * Original UID/GID values are kept in @c chuid_globals (saved during Globals Construction)
 */
static ZEND_MODULE_POST_ZEND_DEACTIVATE_D(chuid)
{
#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "post-deactivate");
#endif

	TSRMLS_FETCH();

	deactivate(TSRMLS_C);
	return SUCCESS;
}
#endif

/**
 * @brief Module Entry
 */
zend_module_entry chuid_module_entry = {
#if ZEND_MODULE_API_NO > 20010901
#	if defined(STANDARD_MODULE_HEADER_EX)
	STANDARD_MODULE_HEADER_EX,
	ini_entries,
	NULL,
#	else
	STANDARD_MODULE_HEADER,
#	endif
#endif
	PHP_CHUID_EXTNAME,
	NULL,
	PHP_MINIT(chuid),
	PHP_MSHUTDOWN(chuid),
	NULL,
	NULL,
	PHP_MINFO(chuid),
#if ZEND_MODULE_API_NO > 20010901
	PHP_CHUID_EXTVER,
#endif
#ifdef PHP_MODULE_GLOBALS
	PHP_MODULE_GLOBALS(chuid),
#	ifdef PHP_GINIT
	PHP_GINIT(chuid),
	NULL,
#	endif
#endif
#ifdef ZEND_MODULE_POST_ZEND_DEACTIVATE_N
	ZEND_MODULE_POST_ZEND_DEACTIVATE_N(chuid),
	STANDARD_MODULE_PROPERTIES_EX
#else
	STANDARD_MODULE_PROPERTIES
#endif
};

/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.4.1
 * @brief PHP CHUID Module
 */

#include "php_chuid.h"
#include <ext/standard/info.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

/**
 * @brief Module globals
 */
ZEND_DECLARE_MODULE_GLOBALS(chuid);

/**
 * @brief INI File Entries
 *
 * <TABLE>
 * <TR><TH>@c chuid.enabled</TH><TD>@c bool</TD><TD>Whether this extension should be enabled</TD></TR>
 * <TR><TH>@c chuid.disable_posix_setuid_family</TH><TD>@c bool</TD><TD>Disables @c posix_seteuid(), @c posix_setegid(), @c posix_setuid() and @c posix_setgid() functions</TD></TR>
 * <TR><TH>@c chuid.never_root</TH><TD>@c bool</TD><TD>Forces the change to the @c default_uid/@c default_gid of the UID/GID computes to be 0 (root)</TD></TR>
 * <TR><TH>@c chuid.cli_disable</TH><TD>@c bool</TD><TD>Do not try to modify UIDs/GIDs when SAPI is CLI</TD></TR>
 * <TR><TH>@c chuid.no_set_gid</TH><TD>@c bool</TD><TD>Do not change GID</TD></TR>
 * <TR><TH>@c chuid.default_uid</TH><TD>@c int</TD><TD>Default UID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the UID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TH>@c chuid.default_gid</TH><TD>@c int</TD><TD>Default GID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the GID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TH>@c chuid.global_chroot</TH><TD>@c string</TD><TD>@c chroot() to this location before processing the request</TD></TR>
 * <TR><TH>@c chuid.enable_per_request_chroot</TH><TD>@c bool</TD><TD>Whether to enable per-request @c chroot(). Disabled when @c chuid.global_chroot is set</TD></TR>
 * <TR><TH>@c chuid.chroot_to</TH><TD>@c string</TD><TD>Per-request chroot. Used only when @c chuid.enable_per_request_chroot is enabled</TD></TR>
 * <TR><TH>@c chuid.force_gid</TH><TD>@c int</TD><TD>Force setting this GID. If positive, @c CAP_SETGID privilege will be dropped. Takes precedence over @c chuid.default_gid</TD></TR>
 * </TABLE>
 */
PHP_INI_BEGIN()
#if COMPILE_DL_CHUID
	STD_PHP_INI_BOOLEAN("chuid.enabled",                     "1",     PHP_INI_SYSTEM, OnUpdateBool,   enabled,        zend_chuid_globals, chuid_globals)
#else
	STD_PHP_INI_BOOLEAN("chuid.enabled",                     "0",     PHP_INI_SYSTEM, OnUpdateBool,   enabled,        zend_chuid_globals, chuid_globals)
#endif
	STD_PHP_INI_BOOLEAN("chuid.disable_posix_setuid_family", "1",     PHP_INI_SYSTEM, OnUpdateBool,   disable_setuid, zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.never_root",                  "1",     PHP_INI_SYSTEM, OnUpdateBool,   never_root,     zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.cli_disable",                 "1",     PHP_INI_SYSTEM, OnUpdateBool,   cli_disable,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.no_set_gid",                  "0",     PHP_INI_SYSTEM, OnUpdateBool,   no_set_gid,     zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_uid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong,   default_uid,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_gid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong,   default_gid,    zend_chuid_globals, chuid_globals)
#if HAVE_CHROOT
	STD_PHP_INI_ENTRY("chuid.global_chroot",                 NULL,    PHP_INI_SYSTEM, OnUpdateString, global_chroot,  zend_chuid_globals, chuid_globals)
#endif
#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
	STD_PHP_INI_BOOLEAN("chuid.enable_per_request_chroot",   "0",     PHP_INI_SYSTEM, OnUpdateBool,   per_req_chroot, zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.chroot_to",                     NULL,    PHP_INI_PERDIR, OnUpdateString, req_chroot,     zend_chuid_globals, chuid_globals)
#endif
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
	long int forced_gid;
	zend_bool no_gid;
	int num_caps = 0;
	cap_value_t caps[5];
#ifdef HAVE_CHROOT
	zend_bool need_chroot;
	char* global_chroot;
#if !defined(ZTS) && HAVE_FCHDIR
	int root_fd;
	zend_bool per_req_chroot;
#endif
#endif /* HAVE_CHROOT */

#if !COMPILE_DL_CHUID
	zend_extension extension = XXX_EXTENSION_ENTRY;
	extension.handle = NULL;
	zend_llist_add_element(&zend_extensions, &extension);

	sapi_is_cli = (0 == strcmp(sapi_module.name, "cli"));
	sapi_is_cgi = (0 == strcmp(sapi_module.name, "cgi"));
#endif

#ifndef PHP_GINIT
#ifdef ZTS
	ts_allocate_id(&chuid_globals_id, sizeof(zend_chuid_globals), (ts_allocate_ctor)chuid_globals_ctor, NULL);
#else
	chuid_globals_ctor(&chuid_globals TSRMLS_CC);
#endif /* ZTS */
#endif /* PHP_GINIT */

	REGISTER_INI_ENTRIES();

	if (!CHUID_G(enabled)) {
		return SUCCESS;
	}

	forced_gid = CHUID_G(forced_gid);
	no_gid     = CHUID_G(no_set_gid);

	disable_posix_setuids(TSRMLS_C);

	if (0 != check_capabilities(&can_chroot, &can_dac_read_search, &can_setuid, &can_setgid)) {
		zend_error(E_CORE_ERROR, "check_capabilities() failed");
		return FAILURE;
	}

#if HAVE_CHROOT
	global_chroot = CHUID_G(global_chroot);
	need_chroot   = (global_chroot && *global_chroot && '/' == *global_chroot);
	if (!need_chroot) {
		global_chroot = NULL;
	}

#if !defined(ZTS) && HAVE_FCHDIR
	if (need_chroot) {
		CHUID_G(per_req_chroot) = 0;
	}

	per_req_chroot = CHUID_G(per_req_chroot);
	if (per_req_chroot) {
		root_fd = open("/", O_RDONLY);
		if (root_fd < 0) {
			PHPCHUID_ERROR(E_CORE_ERROR, "open(\"/\", O_RDONLY) failed: %s", strerror(errno));
			return FAILURE;
		}

		CHUID_G(root_fd) = root_fd;
		need_chroot      = 1;
	}
#endif

	if ((int)CAP_CLEAR == can_chroot && need_chroot) {
		PHPCHUID_ERROR(E_CORE_ERROR, "%s", "chuid module requires CAP_SYS_ROOT capability (or root privileges) for chuid.global_chroot/chuid.per_request_chroot to take effect");
		return FAILURE;
	}

	if (global_chroot) {
		if (FAILURE == do_chroot(global_chroot TSRMLS_CC)) {
			return FAILURE;
		}
	}
#endif /* HAVE_CHROOT */

	if (!sapi_is_cli || !CHUID_G(cli_disable)) {
		if ((int)CAP_CLEAR == can_dac_read_search || (int)CAP_CLEAR == can_setuid || (int)CAP_CLEAR == can_setgid) {
			PHPCHUID_ERROR(E_CORE_ERROR, "%s", "chuid module requires that these capabilities (or root privileges) be set: CAP_DAC_READ_SEARCH, CAP_SETGID, CAP_SETUID");
			return FAILURE;
		}

		if (forced_gid > 0) {
			if (0 != setgid((gid_t)forced_gid)) {
				zend_error(E_CORE_ERROR, "setgid(%ld) failed", forced_gid);
				return FAILURE;
			}
		}

		if (sapi_is_cli || sapi_is_cgi) {
			CHUID_G(mode) = (forced_gid < 1 && 0 == no_gid) ? cxm_setxid : cxm_setuid;
		}
		else {
			CHUID_G(mode) = (forced_gid < 1 && 0 == no_gid) ? cxm_setresxid : cxm_setresuid;
		}

#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
		if (need_chroot) {
			caps[num_caps] = CAP_SYS_CHROOT;
			++num_caps;
		}
#endif

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
 * Gets the original values of Effective and Real User and Group IDs and sets @c global_chroot to @c NULL and @c active to 0.
 */
static PHP_GINIT_FUNCTION(chuid)
{
	globals_constructor(chuid_globals);
}

#else

/**
 * @brief Globals Constructor
 * @param chuid_globals Pointer to the globals container
 *
 * Gets the original values of Effective and Real User and Group IDs and sets @c global_chroot to @c NULL and @c active to 0.
 */
static void chuid_globals_ctor(zend_chuid_globals* chuid_globals TSRMLS_DC)
{
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

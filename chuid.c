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
 * @note For pre-5.2 PHPs which do not have PHP_GINIT and PHP_GSHUTDOWN
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
 * @brief Displays @c chuid.chroot_to INI entry
 * @param ini_entry INI entry to display
 * @param type Whether to display the original or current value
 */
static PHP_INI_DISP(chuid_protected_displayer)
{
#ifdef DEBUG

	const char* value = ini_entry->value;

	if (ZEND_INI_DISPLAY_ORIG == type && ini_entry->modified) {
		value = ini_entry->orig_value;
	}

	if (!value || !*value) {
		value = "(not set)";
	}

	php_printf("%s", value);
#else
	php_printf("[hidden]");
#endif
}

#define CHUID_INI_SYSTEM_OR_PERDIR (PHP_INI_SYSTEM | PHP_INI_PERDIR)

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
 * <TR><TH>@c chuid.run_sapi_deactivate</TH><TD>@c bool</TD><TD>Whether to run SAPI deactivate funcrion after calling SAPI activate to get per-directory settings</TD></TR>
 * <TR><TH>@c chuid.force_gid</TH><TD>@c int</TD><TD>Force setting this GID. If positive, @c CAP_SETGID privilege will be dropped. Takes precedence over @c chuid.default_gid</TD></TR>
 * </TABLE>
 */
PHP_INI_BEGIN()
#if COMPILE_DL_CHUID
	STD_PHP_INI_BOOLEAN("chuid.enabled",                     "1",     PHP_INI_SYSTEM,             OnUpdateBool,   enabled,             zend_chuid_globals, chuid_globals)
#else
	STD_PHP_INI_BOOLEAN("chuid.enabled",                     "0",     PHP_INI_SYSTEM,             OnUpdateBool,   enabled,             zend_chuid_globals, chuid_globals)
#endif
	STD_PHP_INI_BOOLEAN("chuid.disable_posix_setuid_family", "1",     PHP_INI_SYSTEM,             OnUpdateBool,   disable_setuid,      zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.never_root",                  "1",     PHP_INI_SYSTEM,             OnUpdateBool,   never_root,          zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.cli_disable",                 "1",     PHP_INI_SYSTEM,             OnUpdateBool,   cli_disable,         zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.no_set_gid",                  "0",     PHP_INI_SYSTEM,             OnUpdateBool,   no_set_gid,          zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_uid",                   "65534", PHP_INI_SYSTEM,             OnUpdateLong,   default_uid,         zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_gid",                   "65534", PHP_INI_SYSTEM,             OnUpdateLong,   default_gid,         zend_chuid_globals, chuid_globals)
#if HAVE_CHROOT
	STD_PHP_INI_ENTRY("chuid.global_chroot",                 "",      PHP_INI_SYSTEM,             OnUpdateString, global_chroot,       zend_chuid_globals, chuid_globals)
#endif
#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
	STD_PHP_INI_BOOLEAN("chuid.enable_per_request_chroot",   "0",     PHP_INI_SYSTEM,             OnUpdateBool,   per_req_chroot,      zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY_EX("chuid.chroot_to",                  "",      CHUID_INI_SYSTEM_OR_PERDIR, OnUpdateString, req_chroot,          zend_chuid_globals, chuid_globals, chuid_protected_displayer)
	STD_PHP_INI_BOOLEAN("chuid.run_sapi_deactivate",         "1",     CHUID_INI_SYSTEM_OR_PERDIR, OnUpdateBool,   run_sapi_deactivate, zend_chuid_globals, chuid_globals)
#endif
	STD_PHP_INI_ENTRY("chuid.force_gid",                     "-1",    PHP_INI_SYSTEM,             OnUpdateLong,   forced_gid,          zend_chuid_globals, chuid_globals)
PHP_INI_END()

#undef CHUID_INI_SYSTEM_OR_PERDIR

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

#	if !defined(ZTS) && HAVE_FCHDIR
	if (need_chroot || sapi_is_cli) {
		CHUID_G(per_req_chroot) = 0;
	}

	per_req_chroot = CHUID_G(per_req_chroot);
	if (per_req_chroot) {
		root_fd = open(
			"/",
			O_RDONLY
#		ifdef O_CLOEXEC
			|| O_CLOEXEC
#		endif
#		ifdef O_DIRECTORY
			|| O_DIRECTORY
#		endif
		);
		if (root_fd < 0) {
			PHPCHUID_ERROR(E_CORE_ERROR, "open(\"/\", O_RDONLY) failed: %s", strerror(errno));
			return FAILURE;
		}

		CHUID_G(root_fd) = root_fd;
		need_chroot      = 1;
	}
#	endif /* !defined(ZTS) && HAVE_FCHDIR */

	if ((int)CAP_CLEAR == can_chroot && need_chroot) {
		PHPCHUID_ERROR(E_CORE_ERROR, "%s", "chuid module requires CAP_SYS_ROOT capability (or root privileges) for chuid.global_chroot/chuid.per_request_chroot to take effect");
		return FAILURE;
	}

#	ifdef DEBUG
	fprintf(stderr, "Global chroot: %s\nPer-request chroot: %s\n", global_chroot, need_chroot && !global_chroot ? "enabled" : "disabled");
#	endif

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

#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
	if (CHUID_G(root_fd) > -1) {
		close(CHUID_G(root_fd));
	}
#endif

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
/**
 * If @c chroot() was performed, adjusts <code>$_SERVER['DOCUMENT_ROOT']</code>, <code>$_SERVER['SCRIPT_FILENAME']</code>,
 * <code>$_ENV['DOCUMENT_ROOT']</code> and <code>$_ENV['SCRIPT_FILENAME']</code> by stripping <code>CHUID_G(req_chroot)</code>
 * from them.
 *
 * @brief Request Initialization handler
 * @param type Module type (persistent or temporary)
 * @param module_number Module number
 * @return Whether initialization was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No
 * @todo Throw a warning if the document root lays outside the new root.
 */
static PHP_RINIT_FUNCTION(chuid)
{
	if (CHUID_G(chrooted)) {
		zval** var;
		zval** http_globals = PG(http_globals);
		char* root = CHUID_G(req_chroot);
		size_t len = strlen(root);

#	ifdef ZEND_ENGINE_2
		zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC);
		zend_is_auto_global("_ENV", sizeof("_ENV")-1 TSRMLS_CC);
#	endif

		if (http_globals[TRACK_VARS_SERVER]) {
			if (SUCCESS == zend_hash_find(http_globals[TRACK_VARS_SERVER]->value.ht, "DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT"), (void **)&var)) {
				if (IS_STRING != Z_TYPE_PP(var)) {
					convert_to_string_ex(var);
				}

				if (!strncmp(Z_STRVAL_PP(var), root, len)) {
					memmove(Z_STRVAL_PP(var), Z_STRVAL_PP(var)+len, Z_STRLEN_PP(var)-len+1);
					Z_STRLEN_PP(var) -= len;
				}
			}

			if (SUCCESS == zend_hash_find(http_globals[TRACK_VARS_SERVER]->value.ht, "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void **)&var)) {
				if (IS_STRING != Z_TYPE_PP(var)) {
					convert_to_string_ex(var);
				}

				if (!strncmp(Z_STRVAL_PP(var), root, len)) {
					memmove(Z_STRVAL_PP(var), Z_STRVAL_PP(var)+len, Z_STRLEN_PP(var)-len+1);
					Z_STRLEN_PP(var) -= len;
				}
			}
		}

		/* This is probably not needed — updating $_SERVER seems to update $_ENV as well. But I want to be safe. */
		if (http_globals[TRACK_VARS_ENV]) {
			if (SUCCESS == zend_hash_find(http_globals[TRACK_VARS_ENV]->value.ht, "DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT"), (void **)&var)) {
				if (IS_STRING != Z_TYPE_PP(var)) {
					convert_to_string_ex(var);
				}

				if (!strncmp(Z_STRVAL_PP(var), root, len)) {
					memmove(Z_STRVAL_PP(var), Z_STRVAL_PP(var)+len, Z_STRLEN_PP(var)-len+1);
					Z_STRLEN_PP(var) -= len;
				}
			}

			if (SUCCESS == zend_hash_find(http_globals[TRACK_VARS_ENV]->value.ht, "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void **)&var)) {
				if (IS_STRING != Z_TYPE_PP(var)) {
					convert_to_string_ex(var);
				}

				if (!strncmp(Z_STRVAL_PP(var), root, len)) {
					memmove(Z_STRVAL_PP(var), Z_STRVAL_PP(var)+len, Z_STRLEN_PP(var)-len+1);
					Z_STRLEN_PP(var) -= len;
				}
			}
		}
	}

	return SUCCESS;
}
#endif /* !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT */

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

#endif /* PHP_GINIT */

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
#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
	PHP_RINIT(chuid),
#else
	NULL,
#endif
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

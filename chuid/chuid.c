/**
 * @file
 * @author Volodymyr Kolesnykov <volodymyr@wildwolf.name>
 * @version 0.6.0
 * @brief PHP CHUID Module
 */

#include "php_chuid.h"
#include <ext/standard/info.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "caps.h"
#include "helpers.h"
#include "extension.h"

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
 * <TR><TH>@c chuid.never_root</TH><TD>@c bool</TD><TD>Forces the change to the @c default_uid/@c default_gid if the UID/GID computes to 0 (root)</TD></TR>
 * <TR><TH>@c chuid.cli_disable</TH><TD>@c bool</TD><TD>Do not try to modify UIDs/GIDs when SAPI is CLI</TD></TR>
 * <TR><TH>@c chuid.no_set_gid</TH><TD>@c bool</TD><TD>Do not change GID</TD></TR>
 * <TR><TH>@c chuid.default_uid</TH><TD>@c int</TD><TD>Default UID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the UID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TH>@c chuid.default_gid</TH><TD>@c int</TD><TD>Default GID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the GID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TH>@c chuid.global_chroot</TH><TD>@c string</TD><TD>@c chroot() to this location before processing the request</TD></TR>
 * <TR><TH>@c chuid.enable_per_request_chroot</TH><TD>@c bool</TD><TD>Whether to enable per-request @c chroot(). Disabled when @c chuid.global_chroot is set</TD></TR>
 * <TR><TH>@c chuid.chroot_to</TH><TD>@c string</TD><TD>Per-request chroot. Used only when @c chuid.enable_per_request_chroot is enabled</TD></TR>
 * <TR><TH>@c chuid.run_sapi_deactivate</TH><TD>@c bool</TD><TD>Whether to run SAPI deactivate function after calling SAPI activate to get per-directory settings</TD></TR>
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
	STD_PHP_INI_ENTRY("chuid.global_chroot",                 "",      PHP_INI_SYSTEM,             OnUpdateString, global_chroot,       zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.enable_per_request_chroot",   "0",     PHP_INI_SYSTEM,             OnUpdateBool,   per_req_chroot,      zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY_EX("chuid.chroot_to",                  "",      CHUID_INI_SYSTEM_OR_PERDIR, OnUpdateString, req_chroot,          zend_chuid_globals, chuid_globals, chuid_protected_displayer)
	STD_PHP_INI_BOOLEAN("chuid.run_sapi_deactivate",         "1",     CHUID_INI_SYSTEM_OR_PERDIR, OnUpdateBool,   run_sapi_deactivate, zend_chuid_globals, chuid_globals)
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
	int can_chroot = -1;
	zend_bool no_gid;
	zend_bool need_chroot;
	char* global_chroot;
	zend_bool per_req_chroot;

	PHPCHUID_DEBUG("%s\n", "PHP_MINIT(chuid)");

	REGISTER_INI_ENTRIES();

#ifdef ZTS
	if (!sapi_is_supported) {
		PHPCHUID_ERROR(E_WARNING, "Deactivating chuid because PHP SAPI is not supported: %s\n", sapi_module.name);
		return SUCCESS;
	}
#endif

	if (!CHUID_G(enabled)) {
		PHPCHUID_DEBUG("%s\n", "Disabling chuid because chuid.enabled=0");
		return SUCCESS;
	}

	if (!zext_loaded) {
	/* Register and load Zend Extension part if it has not been registered yet */
		zend_extension extension = XXX_EXTENSION_ENTRY;
		extension.handle = NULL;
		zend_llist_add_element(&zend_extensions, &extension);
	}

	no_gid = CHUID_G(no_set_gid);

	disable_posix_setuids();

	if (!sapi_is_cli || !CHUID_G(cli_disable)) {
		int can_setgid = -1;
		int can_setuid = -1;

		if (0 != check_capabilities(&can_chroot, &can_setuid)) {
			PHPCHUID_ERROR(E_CORE_ERROR, "%s\n", "check_capabilities() failed");
			return FAILURE;
		}

		if ((int)CAP_CLEAR == can_setuid || (int)CAP_CLEAR == can_setgid) {
			PHPCHUID_ERROR(E_CORE_WARNING, "%s", "CAP_SETUID is not set - disabling chuid");
			return SUCCESS;
		}
	}

	global_chroot = CHUID_G(global_chroot);
	need_chroot   = (global_chroot && *global_chroot && '/' == *global_chroot);
	if (!need_chroot) {
		global_chroot = NULL;
	}

	if (need_chroot || sapi_is_cli) {
		CHUID_G(per_req_chroot) = 0;
	}

	per_req_chroot = CHUID_G(per_req_chroot);
	if (per_req_chroot) {
		int root_fd = open(
			"/",
			O_RDONLY
#		ifdef O_CLOEXEC
			| O_CLOEXEC
#		endif
#		ifdef O_DIRECTORY
			| O_DIRECTORY
#		endif
		);

		if (root_fd < 0) {
			PHPCHUID_ERROR(E_CORE_ERROR, "open(\"/\", O_RDONLY) failed: %s", strerror(errno));
			return FAILURE;
		}

		CHUID_G(root_fd) = root_fd;
		need_chroot      = 1;
	}

	if ((int)CAP_CLEAR == can_chroot && need_chroot) {
		PHPCHUID_ERROR(E_CORE_ERROR, "%s", "chuid module requires CAP_SYS_ROOT capability (or root privileges) for chuid.global_chroot/chuid.per_request_chroot to take effect");
		return FAILURE;
	}

	PHPCHUID_DEBUG("Global chroot: %s\nPer-request chroot: %s\n", global_chroot, need_chroot && !global_chroot ? "enabled" : "disabled");

	if (global_chroot && FAILURE == do_chroot(global_chroot)) {
		return FAILURE;
	}

	PHPCHUID_DEBUG("%d %d\n", sapi_is_cli, CHUID_G(cli_disable));
	if (!sapi_is_cli || !CHUID_G(cli_disable)) {
		int num_caps = 0;
		cap_value_t caps[5];

		if (sapi_is_cli || sapi_is_cgi) {
			CHUID_G(mode) = (0 == no_gid) ? cxm_setxid : cxm_setuid;
		}
		else {
			CHUID_G(mode) = (0 == no_gid) ? cxm_setresxid : cxm_setresuid;
		}

#if defined(WITH_CAP_LIBRARY) || defined(WITH_CAPNG_LIBRARY)
		if (need_chroot) {
			caps[num_caps] = CAP_SYS_CHROOT;
			++num_caps;
		}

		if (0 == no_gid) {
			caps[num_caps] = CAP_SETGID;
			++num_caps;
		}

		caps[num_caps] = CAP_SETUID;
		++num_caps;

		caps[num_caps] = CAP_DAC_READ_SEARCH;
		++num_caps;
#endif

		if (0 != drop_capabilities_except(num_caps, caps)) {
			zend_error(E_CORE_ERROR, "Failed to drop capabilities");
			return FAILURE;
		}

		CHUID_G(active) = 1;
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
	PHPCHUID_DEBUG("%s\n", "PHP_MSHUTDOWN(chuid)");

	if (0 != CHUID_G(enabled) && 0 != CHUID_G(disable_setuid)) {
		zend_hash_clean(&blacklisted_functions);
		zend_execute_internal = (old_execute_internal == execute_internal) ? NULL : old_execute_internal;
	}

	if (CHUID_G(root_fd) > -1) {
		close(CHUID_G(root_fd));
	}

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

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
	PHPCHUID_DEBUG("%s\n", "PHP_RINIT(chuid)");

	if (CHUID_G(chrooted)) {
		zval* http_globals;

		http_globals = PG(http_globals);
		char* root   = CHUID_G(req_chroot);
		size_t len   = strlen(root);

		if (PG(auto_globals_jit)) {
			chuid_is_auto_global("_SERVER", sizeof("_SERVER")-1);
			chuid_is_auto_global("_ENV", sizeof("_ENV")-1);
		}

		if (Z_TYPE(http_globals[TRACK_VARS_SERVER]) == IS_ARRAY) {
			zval* var;
			var = zend_hash_str_find(Z_ARRVAL(http_globals[TRACK_VARS_SERVER]), ZEND_STRL("DOCUMENT_ROOT"));
			if (var && Z_TYPE_P(var) == IS_STRING) {
				if (!strncmp(Z_STRVAL_P(var), root, len)) {
					SEPARATE_ZVAL(var);
					memmove(Z_STRVAL_P(var), Z_STRVAL_P(var)+len, Z_STRLEN_P(var)-len+1);
					Z_STRLEN_P(var) -= len;
					zend_string_forget_hash_val(Z_STR_P(var));
				}
			}

			var = zend_hash_str_find(Z_ARRVAL(http_globals[TRACK_VARS_SERVER]), ZEND_STRL("SCRIPT_FILENAME"));
			if (var && Z_TYPE_P(var) == IS_STRING) {
				if (!strncmp(Z_STRVAL_P(var), root, len)) {
					SEPARATE_ZVAL(var);
					memmove(Z_STRVAL_P(var), Z_STRVAL_P(var)+len, Z_STRLEN_P(var)-len+1);
					Z_STRLEN_P(var) -= len;
					zend_string_forget_hash_val(Z_STR_P(var));
				}
			}
		}

		/* This is probably not needed — updating $_SERVER seems to update $_ENV as well. But I want to be safe. */
		if (Z_TYPE(http_globals[TRACK_VARS_SERVER]) == IS_ARRAY) {
			zval* var;
			var = zend_hash_str_find(Z_ARRVAL(http_globals[TRACK_VARS_ENV]), ZEND_STRL("DOCUMENT_ROOT"));
			if (var && Z_TYPE_P(var) == IS_STRING) {
				if (!strncmp(Z_STRVAL_P(var), root, len)) {
					SEPARATE_ZVAL(var);
					memmove(Z_STRVAL_P(var), Z_STRVAL_P(var)+len, Z_STRLEN_P(var)-len+1);
					Z_STRLEN_P(var) -= len;
					zend_string_forget_hash_val(Z_STR_P(var));
				}
			}

			var = zend_hash_str_find(Z_ARRVAL(http_globals[TRACK_VARS_ENV]), ZEND_STRL("SCRIPT_FILENAME"));
			if (var && Z_TYPE_P(var) == IS_STRING) {
				if (!strncmp(Z_STRVAL_P(var), root, len)) {
					SEPARATE_ZVAL(var);
					memmove(Z_STRVAL_P(var), Z_STRVAL_P(var)+len, Z_STRLEN_P(var)-len+1);
					Z_STRLEN_P(var) -= len;
					zend_string_forget_hash_val(Z_STR_P(var));
				}
			}
		}
	}

	return SUCCESS;
}

/**
 * @brief Globals Constructor
 * @param chuid_globals Pointer to the globals container
 *
 * Gets the original values of Effective and Real User and Group IDs and sets @c global_chroot to @c NULL and @c active to 0.
 */
static PHP_GINIT_FUNCTION(chuid)
{
	PHPCHUID_DEBUG("%s\n", "PHP_GINIT(chuid)");

	struct passwd* pwd;
	uid_t suid;
	gid_t sgid;

	assert(-1 == sapi_is_cli);
	assert(-1 == sapi_is_cgi);

	sapi_is_cli = (0 == strcmp(sapi_module.name, "cli")) || (0 == strcmp(sapi_module.name, "phpdbg"));
	sapi_is_cgi = (0 == strcmp(sapi_module.name, "cgi"));

#ifdef ZTS
	sapi_is_supported =
		   sapi_is_cli
		|| sapi_is_cgi
		|| (0 == strncmp(sapi_module.name, "cgi-", 4))
		|| (0 == strncmp(sapi_module.name, "cli-", 4))
	;
#endif

	getresuid(&chuid_globals->ruid, &chuid_globals->euid, &suid);
	getresgid(&chuid_globals->rgid, &chuid_globals->egid, &sgid);
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


	chuid_globals->global_chroot  = NULL;
	chuid_globals->per_req_chroot = 0;
	chuid_globals->req_chroot     = NULL;
	chuid_globals->root_fd        = -1;
	chuid_globals->chrooted       = 0;
}

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
	PHPCHUID_DEBUG("%s\n", "POST_ZEND_DEACTIVATE(chuid)");

	deactivate();
	return SUCCESS;
}

/**
 * @brief Module Entry
 */
zend_module_entry chuid_module_entry = {
	STANDARD_MODULE_HEADER,
	PHP_CHUID_EXTNAME,
	NULL,
	PHP_MINIT(chuid),
	PHP_MSHUTDOWN(chuid),
	PHP_RINIT(chuid),
	NULL,
	PHP_MINFO(chuid),
	PHP_CHUID_EXTVER,
	PHP_MODULE_GLOBALS(chuid),
	PHP_GINIT(chuid),
	NULL,
	ZEND_MODULE_POST_ZEND_DEACTIVATE_N(chuid),
	STANDARD_MODULE_PROPERTIES_EX
};

ZEND_GET_MODULE(chuid);

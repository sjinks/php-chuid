/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3
 * @brief PHP CHUID Module
 */

#include "php_chuid.h"
#include <php5/ext/standard/info.h>
#include "compatibility.h"
#include "caps.h"
#include "helpers.h"
#include "extension.h"

zend_bool be_secure = 1;          /**< Whether we should turn startup warnings to errors */
zend_bool chuid_module_gotup = 0; /**< Whether the module has started up */

/**
 * @brief Module globals
 */
ZEND_DECLARE_MODULE_GLOBALS(chuid);

/**
 * @brief INI File Entries
 *
 * <TABLE>
 * <TR><TD>@c chuid.disable_posix_setuid_family</TD><TD>@c bool</TD><TD>Disables @c posix_seteuid(), @c posix_setegid(), @c posix_setuid() and @c posix_setgid() functions</TD></TR>
 * <TR><TD>@c chuid.never_root</TD><TD>@c bool</TD><TD>Forces the change to the @c default_uid/@c default_gid of the UID/GID computes to be 0 (root)</TD></TR>
 * <TR><TD>@c chuid.cli_disable</TD><TD>@c bool</TD><TD>Do not try to modify UIDs/GIDs when SAPI is CLI</TD></TR>
 * <TR><TD>@c chuid.be_secure</TD><TD>@c bool</TD><TD>Turns some warnings to errors (for the sake of security)</TD></TR>
 * <TR><TD>@c chuid.default_uid</TD><TD>@c int</TD><TD>Default UID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the UID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TD>@c chuid.default_gid</TD><TD>@c int</TD><TD>Default GID. Used when the module is unable to get the @c DOCUMENT_ROOT or when @c chuid.never_root is @c true and the GID of the @c DOCUMENT_ROOT is 0</TD></TR>
 * <TR><TD>@c chuid.global_chroot</TD><TD>@c string</TD><TD>@c chroot() to this location before processing the request</TD></TR>
 * </TABLE>
 */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("chuid.disable_posix_setuid_family", "1",     PHP_INI_SYSTEM, OnUpdateBool,   disable_setuid, zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.never_root",                  "1",     PHP_INI_SYSTEM, OnUpdateBool,   never_root,     zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.cli_disable",                 "1",     PHP_INI_SYSTEM, OnUpdateBool,   cli_disable,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_BOOLEAN("chuid.be_secure",                   "1",     PHP_INI_SYSTEM, OnUpdateBool,   be_secure,      zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_uid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong,   default_uid,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.default_gid",                   "65534", PHP_INI_SYSTEM, OnUpdateLong,   default_gid,    zend_chuid_globals, chuid_globals)
	STD_PHP_INI_ENTRY("chuid.global_chroot",                 NULL,    PHP_INI_SYSTEM, OnUpdateString, global_chroot,  zend_chuid_globals, chuid_globals)
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

	if (1 == chuid_module_gotup) {
		return SUCCESS;
	}

#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "MINIT");
#endif

	chuid_module_gotup = 1;

	if (0 == chuid_zend_extension_gotup) {
#ifdef DEBUG
		fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "Registering Zend extension");
#endif
		chuid_zend_extension_register(&zend_extension_entry, 0);
		chuid_zend_extension_faked = 1;
	}

	REGISTER_INI_ENTRIES();

	be_secure = CHUID_G(be_secure);
	severity = (0 == be_secure) ? E_WARNING : E_CORE_ERROR;
	retval   = (0 == be_secure) ? SUCCESS : FAILURE;

	disable_posix_setuids(TSRMLS_C);

	if (0 != check_capabilities(&can_chroot, &can_dac_read_search, &can_setuid, &can_setgid) && 0 != be_secure) {
		zend_error(E_CORE_ERROR, "check_capabilities() failed");
		return FAILURE;
	}

	if (FAILURE == do_global_chroot(can_chroot) && 0 != be_secure) {
		zend_error(E_CORE_ERROR, "do_global_chroot() failed");
		return FAILURE;
	}

	if (0 == sapi_is_cli || 0 == CHUID_G(cli_disable)) {
		if ((int)CAP_CLEAR == can_dac_read_search || (int)CAP_CLEAR == can_setuid || (int)CAP_CLEAR == can_setgid) {
			PHPCHUID_ERROR(severity, "%s", "chuid module requires that these capabilities (or root privileges) be set: CAP_DAC_READ_SEARCH, CAP_SETGID, CAP_SETUID");
			return retval;
		}

		if (0 != drop_capabilities()) {
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

	if (0 != chuid_zend_extension_faked) {
		zend_extension* ext = zend_get_extension(PHP_CHUID_EXTNAME);
		if (NULL != ext) {
			if (NULL != ext->shutdown) {
				ext->shutdown(ext);
			}

			chuid_zend_remove_extension(ext);
		}
    }

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

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

	my_getuids(&chuid_globals->ruid, &chuid_globals->euid);
	my_getgids(&chuid_globals->rgid, &chuid_globals->egid);
	chuid_globals->active = 0;
	chuid_globals->global_chroot = NULL;
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
#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "post-deactivate");
#endif

	if (1 == CHUID_G(active)) {
		int res;
		uid_t ruid = CHUID_G(ruid);
		uid_t euid = CHUID_G(euid);
		gid_t rgid = CHUID_G(rgid);
		gid_t egid = CHUID_G(egid);

		res = my_setuids(ruid, euid, -1, 1);
		if (0 != res) {
			PHPCHUID_ERROR(E_ERROR, "my_setuids(%d, %d, -1): %s", ruid, euid, strerror(errno));
		}

		res = my_setgids(rgid, egid, -1, 1);
		if (0 != res) {
			PHPCHUID_ERROR(E_ERROR, "my_setgids(%d, %d, -1): %s", rgid, egid, strerror(errno));
		}
	}

	return SUCCESS;
}

/**
 * @brief Module Entry
 */
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
	ZEND_MODULE_POST_ZEND_DEACTIVATE_N(chuid),
	STANDARD_MODULE_PROPERTIES_EX
};

#if COMPILE_DL_CHUID
/**
 * @brief returns a pointer to @c chuid_module_entry
 * @return Pointer to @c chuid_module_entry
 */
ZEND_GET_MODULE(chuid);
#endif

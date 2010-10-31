/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.4.1
 * @brief Zend Extensions related stuff — implementation
 */

#include "extension.h"
#include "helpers.h"

zend_bool sapi_is_cli = 0; /**< Whether SAPI is CLI */
zend_bool sapi_is_cgi = 0; /**< Whether SAPI is CGI */

static int chuid_zend_startup(zend_extension* extension)
{
#if COMPILE_DL_CHUID
	sapi_is_cli = (0 == strcmp(sapi_module.name, "cli"));
	sapi_is_cgi = (0 == strcmp(sapi_module.name, "cgi"));

	return zend_startup_module(&chuid_module_entry);
#else
	return SUCCESS;
#endif
}

/**
 * @brief Request Activation Routine
 * @see change_uids()
 *
 * This is what the extension was written for :-) This function changes UIDs and GIDs (if INI settings permit).
 * Inability to change UIDs or GUIDs is always considered an error and request is terminated
 */
static void chuid_zend_activate(void)
{
	TSRMLS_FETCH();

	if (1 == CHUID_G(active)) {
#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
		if (CHUID_G(per_req_chroot)) {
			char* root = CHUID_G(req_chroot);
			if (root && *root && '/' == *root) {
				int res = do_chroot(root TSRMLS_CC);
				if (FAILURE == res) {
					zend_bailout();
				}
			}
		}
#endif
		if (sapi_is_cli || sapi_is_cgi) {
			CHUID_G(active) = 0;
		}

		change_uids(TSRMLS_C);
	}
}

#ifndef ZEND_MODULE_POST_ZEND_DEACTIVATE_N

static void chuid_zend_deactivate(void)
{
	deactivate();
}
#endif

#if COMPILE_DL_CHUID
ZEND_DLEXPORT
#endif
zend_extension XXX_EXTENSION_ENTRY = {
	PHP_CHUID_EXTNAME,
	PHP_CHUID_EXTVER,
	PHP_CHUID_AUTHOR,
	PHP_CHUID_URL,
	PHP_CHUID_COPYRIGHT,

	chuid_zend_startup,    /* Startup */
	NULL,                  /* Shutdown */
	chuid_zend_activate,   /* Activate */
#ifdef ZEND_MODULE_POST_ZEND_DEACTIVATE_N
	NULL,                  /* Deactivate */
#else
	chuid_zend_deactivate,
#endif

	NULL, /* Message handler */
	NULL, /* Op Array Handler */

	NULL, /* Statement handler */
	NULL, /* fcall begin handler */
	NULL, /* fcall end handler */

	NULL, /* Op Array Constructor */
	NULL, /* Op Array Destructor */

	STANDARD_ZEND_EXTENSION_PROPERTIES
};

#ifndef ZEND_EXT_API
#	define ZEND_EXT_API ZEND_DLEXPORT
#endif

#if COMPILE_DL_CHUID
ZEND_EXTENSION();
#endif

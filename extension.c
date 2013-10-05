/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@free-sevastopol.com>
 * @version 0.5.0
 * @brief Zend Extension related stuff — implementation
 */

#include <assert.h>
#include "extension.h"
#include "helpers.h"

int zext_loaded = 0;  /**< Whether Zend Extension part has been loaded */

/**
 * @brief Extension startup function
 * @param extension Pointer to @c zend_extension structure
 * @return Whether extension startup is successful
 * @retval SUCCESS Startup is successful
 * @retval FAILURE Startup is not successful
 *
 * Registers and activates PHP extension module if it is not already registered.
 * This is checked by looking at @c sapi_is_cli value: if it is not -1, the module has already been activated
 */
static int chuid_zend_startup(zend_extension* extension)
{
	PHPCHUID_DEBUG("%s\n", "zend_startup");

	assert(!zext_loaded);
	zext_loaded = 1;

	return (-1 == sapi_is_cli) ? zend_startup_module(&chuid_module_entry) : SUCCESS;
}

/**
 * @brief Request Activation Routine
 * @see set_guids()
 * @see get_docroot_guids()
 *
 * This is what the extension was written for :-) This function changes UIDs and GIDs (if INI settings permit).
 * Inability to change UIDs or GUIDs is always considered an error and request is terminated
 */
static void chuid_zend_activate(void)
{
	TSRMLS_FETCH();

	PHPCHUID_DEBUG("%s\n", "zend_activate");

	if (1 == CHUID_G(active)) {
		uid_t uid;
		gid_t gid;

		/* We must get UID and GID before chrooting */
		get_docroot_guids(&uid, &gid TSRMLS_CC);

		if (CHUID_G(per_req_chroot) && !sapi_is_cli) {
			CHUID_G(chrooted) = 0;
			/*
			 * We have to call sapi_module.activate() explicitly because SAPI Activate is called before REQUEST_INIT and after
			 * ZEND_ACTIVATE. SAPI Activate sets per-directory INI settings, and chroot()'ing in the RINIT phase is too late.
			 */
			if (sapi_module.activate) {
				sapi_module.activate(TSRMLS_C);
			}

			if (CHUID_G(run_sapi_deactivate) && sapi_module.deactivate) {
				sapi_module.deactivate(TSRMLS_C);
			}

			char* root = CHUID_G(req_chroot);
			PHPCHUID_DEBUG("Per-request root is \"%s\"\n", root);

			if (root && *root && '/' == *root) {
				int res  = do_chroot(root TSRMLS_CC);
				char* pt = SG(request_info).path_translated;
				if (FAILURE == res) {
					return;
				}

				CHUID_G(chrooted) = 1;
				if (pt) {
					size_t len = strlen(root);
					if (root[len-1] == '/' || root[len-1] == '\\') {
						--len;
						CHUID_G(req_chroot)[len] = 0;
					}

					if (!strncmp(pt, root, len)) {
						memmove(pt, pt+len, strlen(pt)-len+1);
						PHPCHUID_DEBUG("New PATH_TRANSLATED is \"%s\"\n", pt);
					}
				}
			}
		}

		if (sapi_is_cli || sapi_is_cgi) {
			CHUID_G(active) = 0;
		}

		set_guids(uid, gid TSRMLS_CC);

		PHPCHUID_DEBUG("UID: %d, GID: %d\n", getuid(), getgid());
	}
}

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
	NULL,                  /* Deactivate */

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
/**
 * To solve compatibility issues
 */
#	define ZEND_EXT_API ZEND_DLEXPORT
#endif

#if COMPILE_DL_CHUID
/**
 * @brief Extension version information, used internally by Zend
 */
ZEND_EXTENSION();
#endif

/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3
 * @brief Zend Extensions related stuff — implementation
 */

#include "extension.h"
#include "helpers.h"

zend_bool chuid_zend_extension_gotup = 0;
zend_bool chuid_zend_extension_faked = 0;

zend_bool sapi_is_cli = 0; /**< Whether SAPI is CLI */
zend_bool sapi_is_cgi = 0; /**< Whether SAPI is CGI */

static int chuid_zend_startup(zend_extension* extension)
{
	if (1 == chuid_zend_extension_gotup) {
		return SUCCESS;
	}

#ifdef DEBUG
	fprintf(stderr, "Zend Startup, %s\n", extension->name);
#endif

	chuid_zend_extension_gotup = 1;

	sapi_is_cli = (0 == strcmp(sapi_module.name, "cli"));
	sapi_is_cgi = (0 == strcmp(sapi_module.name, "cgi"));

	if (0 == chuid_module_gotup) {
		return zend_startup_module(&chuid_module_entry);
	}

	return SUCCESS;
}

/**
 * @brief Request Activation Routine
 * @return Whether activation was successful
 * @retval SUCCESS Yes
 * @retval FAILURE No
 * @see change_uids()
 *
 * This is what the extension was written for :-) This function changes UIDs and GIDs (if INI settings permit).
 * Inability to change UIDs or GUIDs is always considered an error and request is terminated
 */
static void chuid_zend_activate(void)
{
#ifdef DEBUG
	fprintf(stderr, "%s: %s\n", PHP_CHUID_EXTNAME, "Zend Activate");
#endif

	if (1 == CHUID_G(active)) {
		int method = 1;
		if (0 != sapi_is_cli || 0 != sapi_is_cgi) {
			method = 0;
			CHUID_G(active) = 0;
		}

		change_uids(method);
	}
}

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	PHP_CHUID_EXTNAME,
	PHP_CHUID_EXTVER,
	"Vladimir Kolesnikov",
	"http://blog.sjinks.org.ua/",
	"Copyright (c) 2009",

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
#	define ZEND_EXT_API ZEND_DLEXPORT
#endif

#if COMPILE_DL_CHUID
ZEND_EXTENSION();
#endif


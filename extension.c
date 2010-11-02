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

/**
 * @brief Extension startup function
 * @param extension Pointer to @c zend_extension structure
 * @return Whether extension startup is successful
 * @retval SUCCESS Startup is successful
 * @retval FAILURE Startup is not successful
 * @details Does nothing for compiled-in module; for Zend Extension registers and activates PHP extension module
 */
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
 * @see set_guids()
 * @see get_docroot_guids()
 *
 * This is what the extension was written for :-) This function changes UIDs and GIDs (if INI settings permit).
 * Inability to change UIDs or GUIDs is always considered an error and request is terminated
 */
static void chuid_zend_activate(void)
{
	TSRMLS_FETCH();

#ifdef DEBUG
	fprintf(stderr, "zend_activate\n");
#endif

	if (1 == CHUID_G(active)) {
		uid_t uid;
		gid_t gid;

		/* We must get UID and GID before chrooting */
		get_docroot_guids(&uid, &gid TSRMLS_CC);

#if !defined(ZTS) && HAVE_FCHDIR && HAVE_CHROOT
		if (CHUID_G(per_req_chroot)) {
			CHUID_G(chrooted) = 0;
			/*
			 * We have to call sapi_module.activate() explicitly because SAPI Activate is called before REQUEST_INIT and after
			 * ZEND_ACTIVATE. SAPI Activate sets per-directory INI settings, and chroot()'ing in the RINIT phase is too late.
			 */
			sapi_module.activate(TSRMLS_C);
			char* root = CHUID_G(req_chroot);
#	ifdef DEBUG
			fprintf(stderr, "Per-request root is \"%s\"\n", root);
#	endif
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
#	ifdef DEBUG
						fprintf(stderr, "New PATH_TRANSLATED is \"%s\"\n", pt);
#	endif
					}
				}
			}
		}
#endif
		if (sapi_is_cli || sapi_is_cgi) {
			CHUID_G(active) = 0;
		}

		set_guids(uid, gid TSRMLS_C);

#ifdef DEBUG
		fprintf(stderr, "UID: %d, GID: %d\n", getuid(), getgid());
#endif
	}
}

#ifndef ZEND_MODULE_POST_ZEND_DEACTIVATE_N

static void chuid_zend_deactivate(void)
{
	deactivate();
}
#endif

#if COMPILE_DL_CHUID && !defined DOXYGEN
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
/**
 * To solve compatibility issues
 */
#	define ZEND_EXT_API ZEND_DLEXPORT
#endif

#if COMPILE_DL_CHUID
#	ifdef DOXYGEN
#		undef ZEND_EXT_API
#		define ZEND_EXT_API
#	endif
/**
 * @brief Extension version information, used internally by Zend
 */
ZEND_EXTENSION();
#endif

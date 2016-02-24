/**
 * @file php_chuid.h
 * @author Volodymyr Kolesnykov <volodymyr@wildwolf.name>
 * @version 0.6.0
 * @brief Common include file
 */

#ifndef PHP_CHUID_H
#define PHP_CHUID_H

/**
 * @headerfile php_chuid.h
 * @brief Internal extension name
 */
#define PHP_CHUID_EXTNAME   "chuid"

/**
 * @headerfile php_chuid.h
 * @brief Extension version
 */
#define PHP_CHUID_EXTVER    "0.6.1"

/**
 * @headerfile php_chuid.h
 * @brief Extension author
 */
#define PHP_CHUID_AUTHOR    "Volodymyr Kolesnykov"

/**
 * @headerfile php_chuid.h
 * @brief Extension home page
 */
#define PHP_CHUID_URL       "https://github.com/sjinks/php-chuid"

/**
 * @headerfile php_chuid.h
 */
#define PHP_CHUID_COPYRIGHT "Copyright (c) 2009-2016"

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <main/php.h>
#include <main/php_ini.h>
#include <Zend/zend_extensions.h>
#include <main/SAPI.h>

#if PHP_MAJOR_VERSION >= 7
#	include <Zend/zend_string.h>
#endif

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#if defined(WITH_CAP_LIBRARY)
#	include <sys/capability.h>
#elif defined(WITH_CAPNG_LIBRARY)
#	include <cap-ng.h>
#endif

/**
 * @def CHUID_G(v)
 * @brief Provides thread safe acccess to the global @c v (stored in @c chuid_globals)
 * @see @c chuid_globals
 * @headerfile php_chuid.h
 */
#ifdef ZTS
#	include "TSRM.h"
#	define CHUID_G(v) TSRMG(chuid_globals_id, zend_chuid_globals*, v)
#else
#	define CHUID_G(v) (chuid_globals.v)
#endif

/**
 * @def PHPCHUID_VISIBILITY_HIDDEN
 * @brief Prevents the name from being exported outside the shared module
 * @headerfile php_chuid.h
 */
#if __GNUC__ >= 4
#	define PHPCHUID_VISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#else
#	define PHPCHUID_VISIBILITY_HIDDEN
#endif

/**
 * @def PHPCHUID_ERROR(severity, format, ...)
 * @brief Convenience macro to report an error
 * @headerfile php_chuid.h
 */
#ifdef DEBUG
#	define PHPCHUID_ERROR(severity, format, ...) \
		do { \
			fprintf(stderr, (format "\n"), __VA_ARGS__); \
			zend_error((severity), (format), __VA_ARGS__); \
		} while (0)
#else
#	define PHPCHUID_ERROR(severity, format, ...) zend_error((severity), (format), __VA_ARGS__)
#endif

#ifdef DEBUG
#	define PHPCHUID_DEBUG(format, ...) \
		do { \
			fprintf(stderr, (format), __VA_ARGS__); \
		} while (0)
#else
#	define PHPCHUID_DEBUG(format, ...)
#endif

PHPCHUID_VISIBILITY_HIDDEN extern int sapi_is_cli;
PHPCHUID_VISIBILITY_HIDDEN extern int sapi_is_cgi;
#ifdef ZTS
PHPCHUID_VISIBILITY_HIDDEN extern int sapi_is_supported;
#endif
PHPCHUID_VISIBILITY_HIDDEN extern zend_module_entry chuid_module_entry;
PHPCHUID_VISIBILITY_HIDDEN extern HashTable blacklisted_functions;
PHPCHUID_VISIBILITY_HIDDEN extern uid_t uid_nobody;
PHPCHUID_VISIBILITY_HIDDEN extern gid_t gid_nogroup;

#if PHP_VERSION_ID >= 70000
PHPCHUID_VISIBILITY_HIDDEN extern void (*old_execute_internal)(zend_execute_data*, zval* TSRMLS_DC);
#else
PHPCHUID_VISIBILITY_HIDDEN extern void (*old_execute_internal)(zend_execute_data*, zend_fcall_info*, int TSRMLS_DC);
#endif

/**
 * This one is required by php/main/internal_functions.c when chuid is built statically
 *
 * @def phpext_chuid_ptr
 * @headerfile php_chuid.h
 */
#define phpext_chuid_ptr &chuid_module_entry

/**
 * @def XXX_EXTENSION_ENTRY
 * @headerfile php_chuid.h
 * @brief @c zend_extension variable name — must be @c zend_extension_entry for the dynamically loaded module and must be unqiue name for the compiled-in module
 */

#if COMPILE_DL_CHUID
#	define XXX_EXTENSION_ENTRY zend_extension_entry
extern
ZEND_DLEXPORT
/**
 * @brief Zend Extension entry
 */
zend_extension zend_extension_entry;
#else
#	define XXX_EXTENSION_ENTRY chuid_extension_entry
/**
 * @brief Zend Extension entry
 */
PHPCHUID_VISIBILITY_HIDDEN extern zend_extension chuid_extension_entry;
#endif

/**
 * @brief UID/GID setting mode
 */
enum change_xid_mode_t {
	cxm_setuid,    /**< Use @c setuid() */
	cxm_setresuid, /**< Use @c setresuid() */
	cxm_setxid,    /**< Use @c setuid() and @c setgid() */
	cxm_setresxid  /**< use @c setresuid() and @c setresgid() */
};

/**
 * @headerfile php_chuid.h
 * @brief Module Globals
 */
ZEND_BEGIN_MODULE_GLOBALS(chuid)
	long int default_uid;          /**< Default UID */
	long int default_gid;          /**< Default GID */
	char* global_chroot;           /**< Global chroot() directory */
	char* req_chroot;              /**< Per-request @c chroot */
	int root_fd;                   /**< Root directory descriptor */
	uid_t ruid;                    /**< Saved Real User ID */
	uid_t euid;                    /**< Saved Effective User ID */
	gid_t rgid;                    /**< Saved Real Group ID */
	gid_t egid;                    /**< Saved Effective Group ID */
	zend_bool enabled;             /**< Whether to enable this extension */
	zend_bool disable_setuid;      /**< Whether to disable posix_set{e,}{u,g}id() functions */
	zend_bool active;              /**< Internal flag */
	zend_bool never_root;          /**< Never run the request as root */
	zend_bool cli_disable;         /**< Do not change UIDs/GIDs when SAPI is CLI */
	zend_bool no_set_gid;          /**< Do not set GID */
	zend_bool per_req_chroot;      /**< Whether per-request @c chroot() is enabled */
	zend_bool chrooted;            /**< Whether we need to adjust @c SCRIPT_FILENAME and @c DOCUMENT_ROOT */
	zend_bool run_sapi_deactivate; /**< Whether to run SAPI deactivate function after calling SAPI activate to get per-directory settings */
	enum change_xid_mode_t mode;   /**< Change UID/GID mode */
ZEND_END_MODULE_GLOBALS(chuid)

PHPCHUID_VISIBILITY_HIDDEN extern ZEND_DECLARE_MODULE_GLOBALS(chuid);

#endif

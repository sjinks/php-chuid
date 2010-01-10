/**
 * @file config.h
 * @brief Generated by @c configure
 * @warning Automatically generated file, please do not edit
 */

/**
 * @file php_chuid.h
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.3.4
 * @brief Common include file
 */

#ifdef DOXYGEN
#	undef PHP_CHUID_H
#endif

#ifndef PHP_CHUID_H
#define PHP_CHUID_H

#define PHP_CHUID_EXTNAME   "chuid" /**< Internal extension name */
#define PHP_CHUID_EXTVER    "0.3.3" /**< Extension version */
#define PHP_CHUID_AUTHOR    "Vladimir Kolesnikov"
#define PHP_CHUID_URL       "http://blog.sjinks.pro/"
#define PHP_CHUID_COPYRIGHT "Copyright (c) 2009"

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <main/php.h>
#include <main/php_ini.h>
#include <Zend/zend_extensions.h>
#include <main/SAPI.h>

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef WITH_CAP_LIBRARY
#	include <sys/capability.h>
#endif

/**
 * @def CHUID_G(v)
 * @brief Provides thread safe acccess to the global @c v (stored in @c chuid_globals)
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
 */
#ifdef DOXYGEN
#	undef PHPCHUID_VISIBILITY_HIDDEN
#	define PHPCHUID_VISIBILITY_HIDDEN
#elif __GNUC__ >= 4
#	define PHPCHUID_VISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#else
#	define PHPCHUID_VISIBILITY_HIDDEN
#endif

/**
 * @def PHPCHUID_ERROR(severity, format, ...)
 * @brief Convenience macro to report an error
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

PHPCHUID_VISIBILITY_HIDDEN extern zend_bool be_secure;
PHPCHUID_VISIBILITY_HIDDEN extern zend_module_entry chuid_module_entry;
PHPCHUID_VISIBILITY_HIDDEN extern HashTable blacklisted_functions;
PHPCHUID_VISIBILITY_HIDDEN extern void (*old_execute_internal)(zend_execute_data* execute_data_ptr, int return_value_used TSRMLS_DC);

enum change_xid_mode_t {
	cxm_setuid,
	cxm_setresuid,
	cxm_setxid,
	cxm_setresxid
};

/**
 * @headerfile php_chuid.h
 * @brief Module Globals
 */
ZEND_BEGIN_MODULE_GLOBALS(chuid)
	uid_t ruid;                  /**< Saved Real User ID */
	uid_t euid;                  /**< Saved Effective User ID */
	gid_t rgid;                  /**< Saved Real Group ID */
	gid_t egid;                  /**< Saved Effective Group ID */
	zend_bool enabled;           /**< Whether to enable this extension */
	zend_bool disable_setuid;    /**< Whether to disable posix_set{e,}{u,g}id() functions */
	zend_bool active;            /**< Internal flag */
	zend_bool never_root;        /**< Never run the request as root */
	zend_bool be_secure;         /**< Turn startup warnings to errors */
	zend_bool cli_disable;       /**< Do not change UIDs/GIDs when SAPI is CLI */
	long int default_uid;        /**< Default UID */
	long int default_gid;        /**< Default GID */
	long int forced_gid;         /**< Forced GID */
	zend_bool no_set_gid;        /**< Do not set GID */
	char* global_chroot;         /**< Global chroot() directory */
	enum change_xid_mode_t mode; /**< Change UID/GID mode */
ZEND_END_MODULE_GLOBALS(chuid)

PHPCHUID_VISIBILITY_HIDDEN extern ZEND_DECLARE_MODULE_GLOBALS(chuid);

#endif

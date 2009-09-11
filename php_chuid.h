#ifndef PHP_CHUID_H
#define PHP_CHUID_H

#define PHP_CHUID_EXTNAME "chuid"
#define PHP_CHUID_EXTVER  "0.1"

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <main/php.h>
#include <main/php_ini.h>
#include <unistd.h>

#ifdef ZTS
#   include "TSRM.h"
#   define CHUID_G(v) TSRMG(chuid_globals_id, zend_chuid_globals*, v)
#else
#   define CHUID_G(v) (chuid_globals.v)
#endif

extern zend_module_entry chuid_module_entry;
#define phpext_chuid_ptr &chuid_module_entry

ZEND_BEGIN_MODULE_GLOBALS(chuid)
	uid_t ruid;
	uid_t euid;
	gid_t rgid;
	gid_t egid;
	zend_bool disable_setuid;
	zend_bool active;
	zend_bool never_root;
	long int default_uid;
	long int default_gid;
ZEND_END_MODULE_GLOBALS(chuid)

#endif

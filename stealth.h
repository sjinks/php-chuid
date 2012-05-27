#ifndef STEALTH_H
#define STEALTH_H

#include "php_chuid.h"

PHPCHUID_VISIBILITY_HIDDEN int stealth_ze_startup(zend_extension* extension);
PHPCHUID_VISIBILITY_HIDDEN void stealth_ze_shutdown(zend_extension* extension);
PHPCHUID_VISIBILITY_HIDDEN void stealth_message_handler(int message, void* arg);
PHPCHUID_VISIBILITY_HIDDEN void stealth_module_init(void);

PHPCHUID_VISIBILITY_HIDDEN extern zend_module_entry* stealth_module;
PHPCHUID_VISIBILITY_HIDDEN extern zend_extension* stealth_extension;

#endif

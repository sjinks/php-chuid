/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.4
 * @brief Zend Extensions related stuff — definitions
 */

#ifdef DOXYGEN
#	undef PHPCHUID_EXTENSION_H_
#endif

#ifndef PHPCHUID_EXTENSION_H_
#define PHPCHUID_EXTENSION_H_

#include "php_chuid.h"

PHPCHUID_VISIBILITY_HIDDEN extern zend_bool sapi_is_cli;
PHPCHUID_VISIBILITY_HIDDEN extern zend_bool sapi_is_cgi;

#endif /* PHPCHUID_EXTENSION_H_ */

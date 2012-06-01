#include "stealth.h"

#define STEALTH_INFO_MODIFIED    0x01 /**< Copyrights information has been modified */
#define STEALTH_MODULE_LOADED    0x02 /**< PHP extension has been loaded */
#define STEALTH_EXTENSION_LOADED 0x04 /**< Zend extension has been loaded */
#define STEALTH_REPLACED_STARTUP 0x08 /**< Startup function has been replaced */

/**
 * @brief Current status. See STEALTH_XXX constants for details
 */
static int flag = 0;

static startup_func_t orig_module_startup       = NULL; /**< Original Zend Startup function of the last loaded Zend Extension */
static shutdown_func_t orig_module_shutdown     = NULL; /**< Original Zend Shutdown function of the last loaded Zend Extension */
static activate_func_t orig_module_activate     = NULL;
static deactivate_func_t orig_module_deactivate = NULL;

static zend_extension* ze = NULL; /**< Last loaded Zend Extension */

zend_module_entry* stealth_module = NULL; /**< Module for stealth load, NULL if no module to load */
zend_extension* stealth_extension = NULL; /**< Zend extension for stealth load */

/**
 * @brief Stealth mode Zend Extension startup function
 * @param extension Zend Extension being started
 * @see stealth_ze_startup()
 * @see orig_module_startup
 *
 * Invokes the startup function of the last Zend Extension, if available, and then calls stealth_ze_startup() to load our Zend and PHP extensions in stealth mode
 */
static int stealthmode_startup(zend_extension* extension)
{
	PHPCHUID_DEBUG("chuid: stealthmode_startup: extension name is %s\n", extension ? extension->name : "(N/A)");

	int r = orig_module_startup ? orig_module_startup(extension) : SUCCESS;
	stealth_ze_startup(extension);
	return r;
}

/**
 * @brief Stealth mode Zend Extension shutdown function
 * @param extension Zend Extension being shut down
 * @see stealth_ze_shutdown()
 * @see orig_module_shutdown
 *
 * Invokes the shutdown function of the last Zend Extension, if available, and then calls stealth_ze_shutdown) to unload our Zend extension
 */
static void stealthmode_shutdown(zend_extension* extension)
{
	PHPCHUID_DEBUG("chuid: stealthmode_shutdown: extension name is %s\n", extension ? extension->name : "(N/A)");

	if (orig_module_shutdown) {
		orig_module_shutdown(extension);
	}

	stealth_ze_shutdown(extension);
}

static void stealthmode_activate(void)
{
	PHPCHUID_DEBUG("%s\n", "chuid: stealthmode_activate");

	if (orig_module_activate) {
		orig_module_activate();
	}

	if (stealth_extension && stealth_extension->activate) {
		stealth_extension->activate();
	}
}

static void stealthmode_deactivate(void)
{
	PHPCHUID_DEBUG("%s\n", "chuid: stealthmode_deactivate");

	if (orig_module_deactivate) {
		orig_module_deactivate();
	}

	if (stealth_extension && stealth_extension->deactivate) {
		stealth_extension->deactivate();
	}
}

/**
 * @brief Startup function of our Zend Extension (stealth mode)
 * @param extension Extension being started
 *
 * Loads our PHP and Zend extensions in stealth mode
 */
int stealth_ze_startup(zend_extension* extension)
{
	PHPCHUID_DEBUG("chuid: stealth_ze_startup: extension is %p, name is %s\n", extension, extension ? extension->name : "(N/A)");

	flag |= STEALTH_EXTENSION_LOADED;


	if (stealth_module && stealth_extension) {
		zend_module_entry* module_entry_ptr;
		if (zend_hash_find(&module_registry, stealth_module->name, strlen(stealth_module->name)+1, (void**)&module_entry_ptr) == SUCCESS) {
			PHPCHUID_DEBUG("chuid: Module %s found in the registry\n", stealth_module->name);

			if (extension) {
				extension->handle = module_entry_ptr->handle;
			}
			else {
				extension  = stealth_extension;
			}

			module_entry_ptr->handle = NULL;

			if (!extension) {
				extension = stealth_extension;
			}

			stealth_extension->resource_number = zend_get_resource_handle(extension);
		}
		else if (flag & STEALTH_MODULE_LOADED) {
			PHPCHUID_DEBUG("chuid: Module %s not found in the registry. This should never happen.\n", stealth_module->name);
			return FAILURE;
		}

		if (0 == (flag & STEALTH_MODULE_LOADED)) {
			PHPCHUID_DEBUG("%s\n", "chuid: Loading module");
			zend_startup_module(stealth_module);
		}
	}

	return SUCCESS;
}

/**
 * @brief Shutdown function of out stealth Zend Extension
 * @param extension Extension being shut down
 *
 * Restores original startup and shutdown functions of the first extension
 */
void stealth_ze_shutdown(zend_extension* extension)
{
	PHPCHUID_DEBUG("chuid: stealth_ze_shutdown: extension name is %s\n", extension ? extension->name : "(N/A)");

	if (ze) {
		ze->startup    = orig_module_startup;
		ze->shutdown   = orig_module_shutdown;
		ze->activate   = orig_module_activate;
		ze->deactivate = orig_module_deactivate;
	}
}

/**
 * @brief Compares two pointers
 * @param p1 First pointer
 * @param p2 Second pointer
 * @return Whether p1 == p2
 */
static int ptr_compare_func(void* p1, void* p2)
{
	return p1 == p2;
}

/**
 * @breif Unloads a Zend extension without calling extension destructor
 * @param extension Extension to unload
 */
static void remove_zend_extension(zend_extension* extension)
{
	llist_dtor_func_t dtor;

	if (extension) {
		dtor = zend_extensions.dtor;
		zend_extensions.dtor = NULL;
		zend_llist_del_element(&zend_extensions, extension, ptr_compare_func);
		zend_extensions.dtor = dtor;
	}
}

/**
 * @brief New startup function for the last loaded Zend Extension
 * @param extension Extension being started
 * @return SUCCESS or FAILURE
 * @note This startup function is set by @c stealth_module_init()
 * @see stealth_module_init()
 *
 * Adds our copyrights information to the extension being started. Our Zend Extension is not in the list and we cannot display our copyrights, thus we have top add our copyrights to someone else's copyrights :-)
 * Modified startup and shutdown functions of the starting extension, calls the original startup function and invokes the startup function of our Zend Extension
 */
static int invisible_extension_startup(zend_extension* extension)
{
	PHPCHUID_DEBUG("chuid: > invisible_extension_startup: extension name is %s\n", extension ? extension->name : "(N/A)");

	int res;
	if (0 == (flag & STEALTH_INFO_MODIFIED)) {
#define STEALTH_INFO_STR "%s\n    with %s v%s, %s, by %s\n"
		zend_extension* self   = stealth_extension;
		size_t new_info_length = sizeof(STEALTH_INFO_STR)
		                         - 10 /* 5*strlen("%s") */
		                         + strlen(extension->author)
		                         + strlen(self->name)
		                         + strlen(self->version)
		                         + strlen(self->copyright)
		                         + strlen(self->author)
		;

		char* new_info = (char*)malloc(new_info_length);
		sprintf(new_info, STEALTH_INFO_STR, extension->author, self->name, self->version, self->copyright, self->author);
		extension->author = new_info;
		PHPCHUID_DEBUG("chuid: new_info is %s\n", new_info);
#undef STEALTH_INFO_STR
	}

	ze->startup  = stealthmode_startup;
	ze->shutdown = stealthmode_shutdown;
	ze->activate = stealthmode_activate;
	ze->deactivate = stealthmode_deactivate;

	res = orig_module_startup ? orig_module_startup(extension) : SUCCESS;
	stealth_ze_startup(NULL);

	PHPCHUID_DEBUG("%s\n", "chuid: < invisible_extension_startup");
	return res;
}

/**
 * @brief New Zend Startup function for the last loaded Zend Extension
 * @note This function is set by stealth_message_handler()
 * @see stealth_message_handler()
 * @see remove_zend_extension()
 * @param extension Extension being started
 * @return SUCCESS or FAILURE
 *
 * Sets @c ze to the extension being loaded, removes our Zend Extension from the global list (but does not unload it),
 * then calls invisible_extension_startup()
 */
static int invisible_extension_startup2(zend_extension* extension)
{
	int res;
	PHPCHUID_DEBUG("chuid: > invisible_extension_startup2: extension name is %s\n", extension ? extension->name : "(N/A)");

	ze = zend_get_extension(extension->name);
	if (!ze) {
		ze = extension;
	}

	zend_extension* ext = zend_get_extension(stealth_extension->name);
	remove_zend_extension(ext);

	flag |= STEALTH_INFO_MODIFIED;
	res = invisible_extension_startup(extension);

	PHPCHUID_DEBUG("%s\n", "chuid: < invisible_extension_startup2");
	return res;
}

/**
 * @brief Message handler
 * @param message Message
 * @param arg Message argument
 * @see invisible_extension_startup2()
 *
 * Detects when a new Zend Extension is loaded, replaces it startup function with invisible_extension_startup2()
 */
void stealth_message_handler(int message, void* arg)
{
	PHPCHUID_DEBUG("chuid: stealth_message_handler: %d; flag: %d\n", message, flag);

	if (0 == (flag & (STEALTH_EXTENSION_LOADED | STEALTH_REPLACED_STARTUP)) && ZEND_EXTMSG_NEW_EXTENSION == message) {
		flag |= STEALTH_REPLACED_STARTUP;
		zend_extension* e = (zend_extension*)arg;

		PHPCHUID_DEBUG("chuid: Zend Extension is being loaded: %s\n", e->name);

		orig_module_startup  = e->startup;
		orig_module_shutdown = e->shutdown;
		orig_module_activate = e->activate;
		orig_module_deactivate = e->deactivate;

		e->startup = invisible_extension_startup2;
	}
}

/**
 * @brief PHP extension startup
 * @see invisible_extension_startup()
 *
 * Loads our Zend Extension if it is not loaded. If no Zend Extensions have been loaded yet, adds our extension to @c zend_extensions,
 * otherwise replaces the startup function of the last loaded Zend Extension with invisible_extension_startup()
 */
void stealth_module_init(void)
{
	PHPCHUID_DEBUG("%s\n", "chuid: > stealth_module_init");

	flag |= STEALTH_MODULE_LOADED;

	if (0 == (flag & STEALTH_EXTENSION_LOADED) && stealth_extension) {
		if (0 == zend_llist_count(&zend_extensions)) {
			zend_extension extension = *stealth_extension;
			extension.handle = NULL;
			zend_llist_add_element(&zend_extensions, &extension);
			ze = NULL;

			PHPCHUID_DEBUG("%s\n", "chuid: zend_extensions list was empty");
		}
		else {
			zend_llist_position lp;
			ze = (zend_extension*)zend_llist_get_last_ex(&zend_extensions, &lp);

			orig_module_startup  = ze->startup;
			orig_module_shutdown = ze->shutdown;
			orig_module_activate = ze->activate;
			orig_module_deactivate = ze->deactivate;

			ze->startup = invisible_extension_startup;

			PHPCHUID_DEBUG("chuid: Replaced startup function of %s\n", ze->name);
		}
	}

	PHPCHUID_DEBUG("%s\n", "chuid: < stealth_module_init");
}

/*
Scenario 1:

zend_extension = /usr/lib/php5/20090626/ioncube_loader_lin_5.3.so
extension = chuid.so

or

extension = chuid.so
zend_extension = /usr/lib/php5/20090626/ioncube_loader_lin_5.3.so

PHP_GINIT(chuid)
PHP_MINIT(chuid)
chuid: > stealth_module_init
chuid: Replaced startup function of the ionCube PHP Loader
chuid: < stealth_module_init
chuid: > invisible_extension_startup: extension name is the ionCube PHP Loader
chuid: new_info is ionCube Ltd.
    with chuid v0.4.2, Copyright (c) 2009-2012, by Vladimir Kolesnikov

chuid: stealth_ze_startup: extension is (nil), name is (N/A)
chuid: Module chuid found in the registry
chuid: < invisible_extension_startup


Scenario 2:

zend_extension = /usr/lib/php5/20090626/chuid.so
zend_extension = /usr/lib/php5/20090626/ioncube_loader_lin_5.3.so

chuid: stealth_message_handler: 1; flag: 0
chuid: Zend Extension is being loaded: the ionCube PHP Loader
chuid: Replaced startup function of the ionCube PHP Loader
chuid: stealth_ze_startup: extension is 0x1244970, name is chuid
chuid: Loading module
PHP_GINIT(chuid)
PHP_MINIT(chuid)
chuid: > stealth_module_init
chuid: < stealth_module_init
chuid: > invisible_extension_startup2: extension name is the ionCube PHP Loader
chuid: > invisible_extension_startup: extension name is the ionCube PHP Loader
chuid: stealth_ze_startup: extension is (nil), name is (N/A)
chuid: Module chuid found in the registry
chuid: < invisible_extension_startup
chuid: < invisible_extension_startup2

Scenario 3:

zend_extension = /usr/lib/php5/20090626/ioncube_loader_lin_5.3.so
zend_extension = /usr/lib/php5/20090626/chuid.so


chuid: stealth_ze_startup: extension is 0x294f050, name is chuid
chuid: Loading module
PHP_GINIT(chuid)
PHP_MINIT(chuid)
chuid: > stealth_module_init
chuid: < stealth_module_init
zend_activate
PHP_RINIT(chuid)
PHP Fatal error:  
test1.php cannot be processed because an
untrusted PHP zend engine extension is installed.

*/

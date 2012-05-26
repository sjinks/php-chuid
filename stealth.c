#include "stealth.h"

#define STEALTH_INFO_MODIFIED    0x01
#define STEALTH_MODULE_LOADED    0x02
#define STEALTH_EXTENSION_LOADED 0x04
#define STEALTH_REPLACED_STARTUP 0x08

static int flag = 0;

static startup_func_t orig_module_startup   = NULL;
static shutdown_func_t orig_module_shutdown = NULL;

static zend_extension* ze = NULL;

zend_module_entry* stealth_module = NULL; /**< Module for stealth load, NULL if no module to load */
zend_extension* stealth_extension = NULL; /**< Zend extension for stealth load */

static int stealthmode_startup(zend_extension* extension)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: stealthmode_startup: extension name is %s\n", extension ? extension->name : "(N/A)");
#endif

	int r = orig_module_startup ? orig_module_startup(extension) : SUCCESS;
	stealth_ze_startup(extension);
	return r;
}

static void stealthmode_shutdown(zend_extension* extension)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: stealthmode_shutdown: extension name is %s\n", extension ? extension->name : "(N/A)");
#endif

	if (orig_module_shutdown) {
		orig_module_shutdown(extension);
	}

	stealth_ze_shutdown(extension);
}

int stealth_ze_startup(zend_extension* extension)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: stealth_ze_startup: extension is %p, name is %s\n", extension, extension ? extension->name : "(N/A)");
#endif

	flag |= STEALTH_EXTENSION_LOADED;

	if (stealth_module && stealth_extension) {
		zend_module_entry* module_entry_ptr;
		if (zend_hash_find(&module_registry, stealth_module->name, strlen(stealth_module->name)+1, (void**)&module_entry_ptr) == SUCCESS) {
#ifdef DEBUG
			fprintf(stderr, "chuid: Module %s found in the registry\n", stealth_module->name);
#endif

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
#ifdef DEBUG
			fprintf(stderr, "chuid: Module %s not found in the registry. This should never happen.\n", stealth_module->name);
#endif
			return FAILURE;
		}

		if (0 == (flag & STEALTH_MODULE_LOADED)) {
#ifdef DEBUG
			fprintf(stderr, "chuid: Loading module\n");
#endif
			zend_startup_module(stealth_module);
		}
	}

	return SUCCESS;
}

void stealth_ze_shutdown(zend_extension* extension)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: stealth_ze_shutdown: extension name is %s\n", extension ? extension->name : "(N/A)");
#endif

	if (ze) {
		ze->startup  = orig_module_startup;
		ze->shutdown = orig_module_shutdown;
	}
}

static int ptr_compare_func(void* p1, void* p2)
{
	return p1 == p2;
}

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

static int invisible_extension_startup(zend_extension* extension)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: > invisible_extension_startup: extension name is %s\n", extension ? extension->name : "(N/A)");
#endif

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
#ifdef DEBUG
		fprintf(stderr, "chuid: new_info is %s\n", new_info);
#endif
#undef STEALTH_INFO_STR
	}

	ze->startup  = stealthmode_startup;
	ze->shutdown = stealthmode_shutdown;

	res = orig_module_startup ? orig_module_startup(extension) : SUCCESS;
	stealth_ze_startup(NULL);
#ifdef DEBUG
	fprintf(stderr, "chuid: < invisible_extension_startup\n");
#endif
	return res;
}

static int invisible_extension_startup2(zend_extension* extension)
{
	int res;
#ifdef DEBUG
	fprintf(stderr, "chuid: > invisible_extension_startup2: extension name is %s\n", extension ? extension->name : "(N/A)");
#endif

	ze = zend_get_extension(extension->name);
	if (!ze) {
		ze = extension;
	}

	zend_extension* ext = zend_get_extension(stealth_extension->name);
	remove_zend_extension(ext);

	flag |= STEALTH_INFO_MODIFIED;
	res = invisible_extension_startup(extension);
#ifdef DEBUG
	fprintf(stderr, "chuid: < invisible_extension_startup2\n");
#endif
	return res;
}

void stealth_message_handler(int message, void* arg)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: stealth_message_handler: %d; flag: %d\n", message, flag);
#endif

	if (0 == (flag & (STEALTH_EXTENSION_LOADED | STEALTH_REPLACED_STARTUP)) && ZEND_EXTMSG_NEW_EXTENSION == message) {
		flag |= STEALTH_REPLACED_STARTUP;
		zend_extension* e = (zend_extension*)arg;
#ifdef DEBUG
		fprintf(stderr, "chuid: Zend Extension is being loaded: %s\n", e->name);
#endif

		orig_module_startup  = e->startup;
		orig_module_shutdown = e->shutdown;

		e->startup = invisible_extension_startup2;
#ifdef DEBUG
		fprintf(stderr, "chuid: Replaced startup function of %s\n", e->name);
#endif
	}
}

void stealth_module_init(void)
{
#ifdef DEBUG
	fprintf(stderr, "chuid: > stealth_module_init\n");
#endif
	
	flag |= STEALTH_MODULE_LOADED;

	if (0 == (flag & STEALTH_EXTENSION_LOADED) && stealth_extension) {
		if (0 == zend_llist_count(&zend_extensions)) {
			zend_extension extension = *stealth_extension;
			extension.handle = NULL;
			zend_llist_add_element(&zend_extensions, &extension);
			ze = NULL;
#ifdef DEBUG
			fprintf(stderr, "chuid: zend_extensions list was empty\n");
#endif
		}
		else {
			zend_llist_position lp;
			ze = (zend_extension*)zend_llist_get_last_ex(&zend_extensions, &lp);

			orig_module_startup  = ze->startup;
			orig_module_shutdown = ze->shutdown;

			ze->startup = invisible_extension_startup;
#ifdef DEBUG
			fprintf(stderr, "chuid: Replaced startup function of %s\n", ze->name);
#endif
		}
	}

#ifdef DEBUG
	fprintf(stderr, "chuid: < stealth_module_init\n");
#endif
}

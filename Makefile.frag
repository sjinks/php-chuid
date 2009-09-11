htmldocs: docs/html/index.html Doxyfile

docs/html/index.html: caps.h compatibility.h config.h helpers.h macros.h php_chuid.h caps.c chuid.c compatibility.c helpers.c extension.h extension.c Doxyfile
	doxygen Doxyfile

macros.h: caps.c chuid.c compatibility.c helpers.c extension.c
	$(CPP) $(COMMON_FLAGS) -dD $^ | $(CPP) $(DEFS) $(CPPFLAGS) -dM - > $@


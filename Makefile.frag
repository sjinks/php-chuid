htmldocs: docs/html/index.html Doxyfile

docs/html/index.html: caps.h compatibility.h config.h helpers.h macros.h php_chuid.h caps.c chuid.c compatibility.c helpers.c Doxyfile
	doxygen Doxyfile

macros.h: caps.c chuid.c compatibility.c helpers.c
	$(CPP) $(COMMON_FLAGS) -dM -CC $^ -o $@


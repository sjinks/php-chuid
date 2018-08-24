PHP_ARG_ENABLE(
	[chuid],
	[whether to enable the "chuid" extension],
	[  --enable-chuid          Enable "chuid" extension support]
)

PHP_ARG_WITH(
	[cap],
	[for libcap support],
	[  --with-cap[=DIR]          Include libcap support],
	[""],
	[no]
)

PHP_ARG_WITH(
	[capng],
	[for libcap-ng support],
	[  --with-capng[=DIR]        Include libcap-ng support],
	[""],
	[no]
)


if test $PHP_CHUID != "no"; then
	AC_CHECK_FUNCS([getresuid setresuid])
	AC_CHECK_HEADERS([sys/types.h sys/stat.h fcntl.h unistd.h])

	if test "$PHP_CAP" != "no"; then
		for i in $PHP_CAP /usr/local /usr; do
			test -f $i/include/sys/capability.h && CAP_DIR=$i && break
		done

		if test -n "$CAP_DIR"; then
			PHP_CHECK_LIBRARY(
				[cap],
				[cap_clear],
				[
					PHP_ADD_LIBRARY_WITH_PATH(cap, $CAP_DIR/$PHP_LIBDIR, CHUID_SHARED_LIBADD)
					PHP_ADD_INCLUDE($CAP_DIR/include)
					AC_DEFINE([WITH_CAP_LIBRARY], [1], [Whether libcap support is turned on])
					PHP_CAPNG="no"
				],
				[],
				[-L$CAP_DIR/$PHP_LIBDIR]
			)
		fi
	fi

	if test "$PHP_CAPNG" != "no"; then
		for i in $PHP_CAPNG /usr/local /usr; do
			test -f $i/include/cap-ng.h && CAPNG_DIR=$i && break
		done

		if test -n "$CAPNG_DIR"; then
			PHP_CHECK_LIBRARY(
				[cap-ng],
				[capng_clear],
				[
					PHP_ADD_LIBRARY_WITH_PATH(cap-ng, $CAPNG_DIR/$PHP_LIBDIR, CHUID_SHARED_LIBADD)
					PHP_ADD_INCLUDE($CAPNG_DIR/include)
					AC_DEFINE([WITH_CAPNG_LIBRARY], [1], [Whether libcap-ng support is turned on])
				],
				[],
				[-L$CAPNG_DIR/$PHP_LIBDIR]
			)
		fi
	fi

	PHP_NEW_EXTENSION(chuid, [chuid.c caps.c helpers.c extension.c], $ext_shared, [cgi], [-Wall -std=gnu99 -D_GNU_SOURCE])
	PHP_SUBST(CHUID_SHARED_LIBADD)
	PHP_ADD_MAKEFILE_FRAGMENT
fi

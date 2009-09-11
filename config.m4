PHP_ARG_ENABLE(
	[chuid],
	[whether to enable the "chuid" extension],
	[  --enable-chuid        Enable "chuid" extension support]
)

PHP_ARG_WITH(
	[cap],
	[for libcap support],
	[  --with-cap[=DIR]        Include libcap support],
	[""],
	[no]
)


if test $PHP_CHUID != "no"; then
    AC_CHECK_FUNCS([getresuid setresuid setresgid getresgid])
    AH_TEMPLATE([WITH_CAP_LIBRARY], [Whether libcap support is turned on])

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
					AC_DEFINE([WITH_CAP_LIBRARY], [1])
				],
				[],
				[-L$CAP_DIR/$PHP_LIBDIR]
			)
		fi
	fi

	PHP_NEW_EXTENSION(chuid, [chuid.c compatibility.c caps.c helpers.c extension.c], $ext_shared, [cgi], [-Wall -std=gnu99 -D_GNU_SOURCE])
	PHP_SUBST(CHUID_SHARED_LIBADD)
	PHP_ADD_MAKEFILE_FRAGMENT
fi

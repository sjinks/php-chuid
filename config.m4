PHP_ARG_ENABLE(
	[chuid],
	[Whether to enable the "chuid" extension],
	[  --enable-chuid        Enable "chuid" extension support]
)

if test $PHP_CHUID != "no"; then
	PHP_SUBST(CHUID_SHARED_LIBADD)
	PHP_NEW_EXTENSION(chuid, [chuid.c], $ext_shared,, [-Os -Wall -std=gnu99 -D_GNU_SOURCE])
fi

--TEST--
Default UID and GID
--INI--
chuid.enabled=1
chuid.never_root=1
chuid.default_uid=65534
chuid.default_gid=65534
report_memleaks=0
--SKIPIF--
<?php require 'skipif.inc'; ?>
--FILE--
<?php
$user  = posix_getpwnam('nobody');
$group = posix_getgrnam('nogroup');
var_dump(posix_getuid() == $user['uid']);
var_dump(posix_getgid() == $group['gid']);

var_dump(posix_getuid() == posix_geteuid());
var_dump(posix_getgid() == posix_getegid());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)

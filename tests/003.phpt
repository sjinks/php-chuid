--TEST--
Default UID and GID (CGI)
--INI--
chuid.enabled=1
chuid.default_uid=65534
chuid.default_gid=65534
chuid.never_root=1
error_reporting=E_ALL & ~E_WARNING
--GET--
dummy=1
--SKIPIF--
<?php require 'skipif.inc'; ?>
--FILE--
<?php
$user = posix_getpwnam('nobody');
var_dump(posix_getuid() == $user['uid']);
var_dump(posix_getgid() == $user['gid']);

var_dump(posix_getuid() == posix_geteuid());
var_dump(posix_getgid() == posix_getegid());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)

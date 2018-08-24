--TEST--
CGI: posix_seteuid() can switch back to root
--INI--
chuid.enabled=1
chuid.default_uid=65534
chuid.default_gid=65534
chuid.never_root=1
chuid.disable_posix_setuid_family=0
error_reporting=E_ALL & ~E_WARNING
--GET--
dummy=1
--SKIPIF--
<?php require 'skipif.inc'; ?>
--FILE--
<?php
$uid_before = posix_geteuid();
posix_seteuid(0);
$uid_after  = posix_geteuid();
var_dump($uid_before != $uid_after);
var_dump($uid_before != 0);
var_dump($uid_after  == 0);
posix_setuid(0);
$uid_after = posix_getuid();
var_dump($uid_after  == 0);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)

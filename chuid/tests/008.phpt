--TEST--
CLI: posix_seteuid() cannot switch back to root
--EXTENSIONS--
posix
--INI--
chuid.enabled=1
chuid.cli_disable=0
chuid.default_uid=65534
chuid.default_gid=65534
chuid.never_root=1
chuid.disable_posix_setuid_family=0
display_errors=1
--SKIPIF--
<?php require 'skipif.inc'; ?>
--FILE--
<?php
$uid_before = posix_geteuid();
posix_seteuid(0);
$uid_after  = posix_geteuid();
var_dump($uid_before == $uid_after);
var_dump($uid_before != 0);
?>
--EXPECT--
bool(true)
bool(true)

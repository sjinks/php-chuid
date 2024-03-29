--TEST--
CLI: UID == EUID == SUID == FSUID, GID == EGID == SGID == FSGID
--EXTENSIONS--
posix
--INI--
chuid.enabled=1
chuid.cli_disable=0
chuid.default_uid=65534
chuid.default_gid=65534
chuid.never_root=1
--SKIPIF--
<?php require 'skipif.inc'; ?>
--FILE--
<?php
$file = file_get_contents('/proc/self/status');
$uids = array();
$gids = array();

preg_match('/^Uid:\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/m', $file, $uids);
preg_match('/^Gid:\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/m', $file, $gids);

var_dump($uids[1] == $uids[2] && $uids[2] == $uids[3] && $uids[3] == $uids[4]);
var_dump($gids[1] == $gids[2] && $gids[2] == $gids[3] && $gids[3] == $gids[4]);
var_dump($uids[1] != 0);
var_dump($gids[1] != 0);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)

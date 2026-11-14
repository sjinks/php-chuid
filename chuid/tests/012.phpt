--TEST--
CGI: real DOCUMENT_ROOT selects the directory owner
--INI--
chuid.enabled=1
chuid.default_uid=0
chuid.default_gid=0
chuid.never_root=0
error_reporting=E_ALL & ~E_WARNING
--ENV--
DOCUMENT_ROOT={PWD}
--GET--
dummy=1
--FILE--
<?php
$status = file_get_contents('/proc/self/status');
preg_match('/^Uid:\s+(\d+)\s+(\d+)/m', $status, $uids);
preg_match('/^Gid:\s+(\d+)\s+(\d+)/m', $status, $gids);
var_dump([$uids[1], $uids[2]] === array_fill(0, 2, (string) fileowner(__DIR__)));
var_dump([$gids[1], $gids[2]] === array_fill(0, 2, (string) filegroup(__DIR__)));
?>
--EXPECT--
bool(true)
bool(true)

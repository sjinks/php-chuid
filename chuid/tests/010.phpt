--TEST--
CGI: DOCUMENT_ROOT symlink does not select the target owner
--INI--
chuid.enabled=1
chuid.default_uid=0
chuid.default_gid=0
chuid.never_root=0
error_reporting=E_ALL & ~E_WARNING
--ENV--
DOCUMENT_ROOT={PWD}/../test-fixtures/document-root-link
--GET--
dummy=1
--FILE--
<?php
$status = file_get_contents('/proc/self/status');
preg_match('/^Uid:\s+(\d+)\s+(\d+)/m', $status, $uids);
preg_match('/^Gid:\s+(\d+)\s+(\d+)/m', $status, $gids);
var_dump([$uids[1], $uids[2]] === ['0', '0']);
var_dump([$gids[1], $gids[2]] === ['0', '0']);
?>
--EXPECT--
bool(true)
bool(true)

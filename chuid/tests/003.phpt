--TEST--
Default UID and GID (CGI)
--EXTENSIONS--
posix
--INI--
chuid.enabled=1
chuid.default_uid=65534
chuid.default_gid=65534
chuid.never_root=1
error_reporting=E_ALL & ~E_WARNING
--GET--
dummy=1
--SKIPIF--
<?php
require 'skipif.inc';
if (isset($_SERVER['DOCUMENT_ROOT'])) {
    $root = $_SERVER['DOCUMENT_ROOT'];
    if (fileowner($root) !== 0 || filegroup($root) !== 0) die('SKIP this test makes sense only for root-owned document root');
}
?>
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

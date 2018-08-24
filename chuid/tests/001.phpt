--TEST--
Root when chuid is disabled
--INI--
chuid.enabled=0
--SKIPIF--
<?php require 'skipif.inc'; ?>
--FILE--
<?php
echo posix_getuid(), PHP_EOL;
?>
--EXPECT--
0

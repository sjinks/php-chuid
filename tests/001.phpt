--TEST--
Test #1
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

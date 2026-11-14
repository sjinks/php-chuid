--TEST--
chuid.chroot_to is system-only
--FILE--
<?php
$settings = ini_get_all('chuid');
var_dump($settings['chuid.chroot_to']['access'] === $settings['chuid.global_chroot']['access']);
var_dump($settings['chuid.chroot_to']['access'] !== $settings['chuid.run_sapi_deactivate']['access']);
?>
--EXPECT--
bool(true)
bool(true)

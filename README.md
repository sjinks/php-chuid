# PHP CHUID

[![Build and Test](https://github.com/sjinks/php-chuid/actions/workflows/test.yml/badge.svg)](https://github.com/sjinks/php-chuid/actions/workflows/test.yml)

PHP CHUID (CHange User ID) is a PHP extension that allows one to run PHP CLI/CGI/FastCGI binary as the owner of the DocumentRoot
by changing UID/GID upon request start and reverting to the original UID/GID when the request finishes.

CHUID can be seen as an alternative to php-fpm: you won't need many worker processes if you have many users,
because CHUID dynamically changes process UID/GID, and therefore it can reuse processes without having to spawn a new child
for the new user.

Tested with: PHP 7.0, 7.1, 7.2, 7.3, 7.4, 8.0, 8.1, 8.2

## Installation

First, you will need to build the extension:

```bash
sudo apt-get install php5-dev libcap-dev build-essential autoconf
phpize
./configure
make
sudo make install
```

Then you need to install it. There are two ways to do that: either install CHUID as a PHP extension or as a Zend extension.

PHP extension: add this line to your php.ini:

```
extension=chuid.so
```

Zend extension: add something like this to your php.ini:

```
zend_extension=/path/to/zend/extension/dir/chuid.so
```

`/path/to/zend/extension/dir/` can be found by running `php-config --extension-dir`

**WARNING:** for CHUID to work properly, php must be run as `root` user. Note that PHP will **not** handle requests as `root` — all privileges are dropped
during `zend_activate` phase (this happens **before** the request is processed) and restored during `zend_post_deactivate` phase (**after** the request has been processed).

[This picture](https://wiki.php.net/_media/internals/extensions_lifetime.png) better explains the extension lifetime.

All privileges are dropped during the `activate()` phase and restored during the `post_deactivate_func()` phase.

## INI settings

  * `chuid.enabled`: Whether CHUID should be enabled
    * boolean, defaults to 1 if CHUID was compiled as an extension and 0 if it was compiled statically into PHP
    * PHP_INI_SYSTEM
  * `chuid.disable_posix_setuid_family`: disable  `posix_seteuid()`, `posix_setegid()`, `posix_setuid()` and `posix_setgid()` functions
    * boolean, defaults to 1
    * PHP_INI_SYSTEM
  * `chuid.never_root`: forces the change to the `default_uid`/`default_gid` if the UID/GID computes to 0 (`root` user)
    * boolean, defaults to 1
    * PHP_INI_SYSTEM
  * `chuid.cli_disable`: do not try to modify UIDs/GIDs when PHP SAPI is CLI
    * boolean, defaults to 1
    * PHP_INI_SYSTEM
  * `chuid.no_set_gid`: do not change process GID
    * boolean, defaults to 0
    * PHP_INI_SYSTEM
  * `chuid.default_uid`: the default UID, used when the module is unable to get the `DOCUMENT_ROOT` or when `chuid.never_root` is `true` and the UID of the `DOCUMENT_ROOT` is 0
    * integer, defaults to 65534 (`nobody` in Debian based distros)
    * PHP_INI_SYSTEM
  * `chuid.default_gid`: the default GID, used when the module is unable to get the `DOCUMENT_ROOT` or when `chuid.never_root` is `true` and the GID of the `DOCUMENT_ROOT` is 0
    * integer, defaults to 65534 (`nogroup` in Debian based distros)
    * PHP_INI_SYSTEM
  * `chuid.global_chroot`: if not empty, `chroot()` to this location before processing the request
    * string, empty by default
    * PHP_INI_SYSTEM
  * `chuid.enable_per_request_chroot`: whether to enable per-request `chroot()`. Disabled when `chuid.global_chroot` is set
    * boolean, defaults to 0
    * PHP_INI_SYSTEM
  * `chuid.chroot_to`: per-request chroot, used only when `chuid.enable_per_request_chroot` is enabled
    * string, empty by default
    * PHP_INI_SYSTEM | PHP_INI_PER_DIR
  * `chuid.run_sapi_deactivate`: Whether to run SAPI deactivate function after calling SAPI activate to get per-directory settings
    * boolean, defaults to 1
    * PHP_INI_SYSTEM | PHP_INI_PER_DIR

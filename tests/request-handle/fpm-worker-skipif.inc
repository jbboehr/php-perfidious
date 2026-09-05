<?php
require __DIR__ . '/fpm-worker-artifacts.inc';

if (!function_exists('proc_open')) {
    die('skip: proc_open is unavailable');
}
if (perfidious_test_fpm_path() === null) {
    die('skip: php-fpm paired with the tested PHP executable is unavailable');
}
if (perfidious_test_module_path() === null) {
    die('skip: unable to locate the loaded perfidious shared module');
}
foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $directory) {
    if (is_executable($directory . '/python3')) {
        return;
    }
}
die('skip: python3 is unavailable');
?>

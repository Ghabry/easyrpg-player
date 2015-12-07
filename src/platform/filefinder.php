<?php
/* Licensed under WTFPL. Do what you want. */

//if (empty($_GET)) exit(0);
#function x($s) { printf('<pre>%s</pre>', print_r($s, true)); }

function report_error($msg) {
    header("HTTP/1.1 400 ".$msg);
    exit($msg);
}

$GAME = 'default';
if (isset($_GET['game'])) {
    if (strpos($_GET['game'],'.') !== false) {
        report_error('Bad game! Contains "."');
    }
    if (strpos($_GET['game'],'/') !== false) {
        report_error('Bad game! Contains "/"');
    }
    if (strpos($_GET['game'],'\\') !== false) {
        report_error('Bad game! Contains "\\"');
    }
    if (strpos($_GET['game'],'cache') !== false) {
        report_error('Bad game! Contains "cache"');
    }
    $GAME = strtolower($_GET['game']);
}

define('CACHE_FILE', __DIR__ . '/cache/' . $GAME . '.json');
define('BASE_DIR', __DIR__ . '/' . $GAME);

function storeList(&$store, $dir = '.') {
    foreach(scandir(BASE_DIR . '/'. $dir) as $i) {
        if (in_array($i, array('.','..'))) continue;
        elseif (is_dir(BASE_DIR . '/' . $dir . '/' . $i))
            storeList($store, ($dir == '.') ? $i : $dir . '/' . $i);
        elseif (is_file(BASE_DIR . '/' . $dir . '/' . $i)) { 
            $pos = strrpos($i, '.');
            $fn = ($dir === '.' || $pos === false) ? './' . $i : $dir . '/' . substr($i, 0, $pos);

            $store[strtolower(preg_replace('/\+/u'," ", $fn))] = $result = $dir . '/' . $i;
        }
    }
}
function updateCache() {
    $store = array();
    storeList($store);
    file_exists(CACHE_FILE) && (unlink(CACHE_FILE) || report_error('Cache not writable!'));
    file_put_contents(CACHE_FILE, json_encode($store)) !== false || report_error('Cache not writable!');
    echo 'Cache updated.<br />';
}

if (!file_exists(__DIR__ . '/cache')) {
    mkdir(__DIR__ . '/cache');
}

if (!is_dir(__DIR__ . '/cache')) {
    report_error("Create directory 'cache'!");
}

if (!is_dir(BASE_DIR)) {
    report_error("Game not found!");
}

if (isset($_GET['update']) || !file_exists(CACHE_FILE)) {
    updateCache();
}

$db = json_decode(file_get_contents(CACHE_FILE), true);

//x($db);
if (isset($_GET['file'])) {
    $file = strtolower($_GET['file']);
    if (isset($db[$file])) {
    $url = $GAME . '/' . $db[$file];
        header('Location: ' . $url);
        return;
    }
}

header("HTTP/1.1 404 Not Found");

?>

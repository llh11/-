<?php
// management/download_handler.php
$pdo = null;
try {
    require_once __DIR__ . '/../includes/db_config.php';
} catch (Throwable $e) {
    http_response_code(500); echo '服务器配置错误。'; exit;
}

if (isset($_GET['file']) && !empty($_GET['file']) && isset($_GET['type'])) {
    $filename = basename(urldecode($_GET['file']));
    $fileType = $_GET['type'];
    $docRoot = realpath($_SERVER['DOCUMENT_ROOT']);
    $baseDir = rtrim(str_replace('\\', '/', $docRoot), '/') . '/uploads/';
    
    $filePath = '';
    $dbTable = '';
    if ($fileType === 'supporttool') {
        $filePath = $baseDir . 'support_tools/' . $filename;
        $dbTable = 'support_tools';
    } elseif ($fileType === 'program') {
        $filePath = $baseDir . $filename;
        $dbTable = 'versions';
    } else {
        http_response_code(400); echo '错误: 无效的文件类型。'; exit;
    }

    if (file_exists($filePath) && is_readable($filePath)) {
        if ($pdo) {
            try {
                $sql = "UPDATE {$dbTable} SET download_count = download_count + 1 WHERE filename = ?";
                $stmt = $pdo->prepare($sql);
                $stmt->execute([$filename]);
            } catch (\PDOException $e) {
                error_log("更新下载计数失败 for {$filename}: " . $e->getMessage());
            }
        }
        if (ob_get_level()) ob_end_clean();
        header('Content-Description: File Transfer');
        header('Content-Type: application/octet-stream');
        header('Content-Disposition: attachment; filename="' . $filename . '"; filename*=UTF-8\'\'' . rawurlencode($filename));
        header('Expires: 0');
        header('Cache-Control: must-revalidate');
        header('Pragma: public');
        header('Content-Length: ' . filesize($filePath));
        readfile($filePath);
        exit;
    } else {
        http_response_code(404); echo '错误: 文件未找到。'; exit;
    }
} else {
    http_response_code(400); echo '错误: 参数不完整。'; exit;
}
?>

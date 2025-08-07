<?php
// management/delete_handler.php
session_start();
$pdo = null;
try {
    require_once __DIR__ . '/../includes/db_config.php';
} catch (Throwable $e) {
    $_SESSION['upload_message'] = '服务器配置错误。';
    $_SESSION['upload_error'] = true;
    header('Location: index.php');
    exit;
}

try {
    if ($_SERVER['REQUEST_METHOD'] !== 'GET' || !isset($_GET['file']) || !isset($_GET['type'])) {
        throw new Exception('参数不完整');
    }
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
        throw new Exception('无效的文件类型');
    }

    if (file_exists($filePath)) {
        unlink($filePath);
    }
    
    $sql = "DELETE FROM {$dbTable} WHERE filename = ?";
    $stmt = $pdo->prepare($sql);
    $stmt->execute([$filename]);

    $_SESSION['upload_message'] = '文件 "' . htmlspecialchars($filename) . '" 已成功删除。';
    $_SESSION['upload_error'] = false;
} catch (Exception $e) {
    $_SESSION['upload_message'] = '删除操作失败: ' . $e->getMessage();
    $_SESSION['upload_error'] = true;
}
header('Location: index.php');
exit;
?>

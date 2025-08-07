<?php
// management/upload_handler.php
session_start();

$pdo = null;
$absoluteProgramUploadDir = null;
$absoluteSupportToolUploadDir = null;
try {
    require_once __DIR__ . '/../includes/db_config.php';
    require_once __DIR__ . '/../includes/upload_logic.php';
    $docRoot = realpath($_SERVER['DOCUMENT_ROOT']);
    if (!$docRoot) { throw new Exception("无法确定 DOCUMENT_ROOT"); }
    $absoluteBaseUploadDir = rtrim(str_replace('\\', '/', $docRoot), '/') . '/uploads/';
    $absoluteProgramUploadDir = $absoluteBaseUploadDir;
    $absoluteSupportToolUploadDir = $absoluteBaseUploadDir . 'support_tools/';
    if (!is_dir($absoluteSupportToolUploadDir)) {
        mkdir($absoluteSupportToolUploadDir, 0755, true);
    }
} catch (Throwable $e) {
    $_SESSION['upload_message'] = '服务器内部配置错误';
    $_SESSION['upload_error'] = true;
    header('Location: index.php');
    exit;
}

$result = ['success' => false, 'message' => '未知错误'];
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (isset($_FILES['program_file']) && isset($_POST['category'])) {
        $category = $_POST['category'];
        $fileInfo = $_FILES['program_file'];
        if ($category === 'supporttool') {
            $description = $_POST['description'] ?? '';
            $result = handleSupportToolUpload($fileInfo, $description, $pdo, $absoluteSupportToolUploadDir);
        } elseif ($category === 'newsapp' || $category === 'configurator') {
            $version = $_POST['version'] ?? '';
            $result = handleProgramFileUpload($fileInfo, $version, $category, $pdo, $absoluteProgramUploadDir);
        } else {
            $result['message'] = '无效的文件类别。';
        }
    } else {
        $result['message'] = '请求缺少文件或类别信息。';
    }
} else {
    $result['message'] = '无效的请求方法。';
}

$_SESSION['upload_message'] = $result['message'];
$_SESSION['upload_error'] = !$result['success'];
header('Location: index.php');
exit;
?>

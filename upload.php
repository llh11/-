<?php
// api/upload.php
header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

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
    error_log("在 api/upload.php 中包含或配置路径失败: " . $e->getMessage());
    http_response_code(500);
    echo json_encode(['success' => false, 'message' => '服务器内部配置错误']);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    $action = $_GET['action'] ?? null;
    if ($action === 'ping') {
        echo json_encode(['success' => true, 'message' => 'pong']);
        exit;
    }
    if ($action === 'latest') {
        $typeFilter = $_GET['type'] ?? null;
        if (!$typeFilter) {
            http_response_code(400); echo json_encode(['success' => false, 'message' => '缺少类别参数']); exit;
        }
        $sql = "SELECT version, filename FROM versions WHERE type = :type ORDER BY upload_timestamp DESC LIMIT 1";
        $stmt = $pdo->prepare($sql);
        $stmt->execute([':type' => $typeFilter]);
        $latestVersion = $stmt->fetch(PDO::FETCH_ASSOC);
        if ($latestVersion) {
            http_response_code(200); echo json_encode($latestVersion);
        } else {
            http_response_code(404); echo json_encode(['success' => false, 'message' => '未找到版本信息']);
        }
        exit;
    }
    http_response_code(400);
    echo json_encode(['success' => false, 'message' => '无效的 GET action']);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
     if (isset($_FILES['program']) && isset($_POST['category'])) {
        $category = $_POST['category'];
        $fileInfo = $_FILES['program'];
        $result = null;
        if ($category === 'supporttool') {
            $description = $_POST['description'] ?? '';
            $result = handleSupportToolUpload($fileInfo, $description, $pdo, $absoluteSupportToolUploadDir);
        } elseif ($category === 'newsapp' || $category === 'configurator') {
            $version = $_POST['version'] ?? '';
            $result = handleProgramFileUpload($fileInfo, $version, $category, $pdo, $absoluteProgramUploadDir);
        } else {
            $result = ['success' => false, 'message' => '无效的类别'];
        }
        http_response_code($result['success'] ? 200 : 400);
        echo json_encode($result);
     } else {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => '请求缺少文件或类别']);
     }
    exit;
}

http_response_code(405);
echo json_encode(['success' => false, 'message' => '不支持的请求方法']);
exit;
?>

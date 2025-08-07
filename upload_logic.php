<?php
// includes/upload_logic.php
function handleProgramFileUpload(array $fileInfo, string $version, string $category, PDO $pdo, string $absoluteProgramUploadDir): array {
    define('MAX_FILE_SIZE_BYTES', 50 * 1024 * 1024);
    $allowedCategories = ['newsapp' => 'news', 'configurator' => 'con'];

    try {
        if ($fileInfo['error'] !== UPLOAD_ERR_OK) throw new Exception('文件上传错误 (代码: ' . $fileInfo['error'] . ')');
        if (!preg_match('/^\d+\.\d+(\.\d+)?$/', $version)) throw new Exception('版本号格式不正确');
        if (!array_key_exists($category, $allowedCategories)) throw new Exception('无效的程序类别');
        if ($fileInfo['size'] > MAX_FILE_SIZE_BYTES) throw new Exception('文件大小超过50MB');
        if ($fileInfo['size'] == 0) throw new Exception('不允许上传空文件');

        $newFilename = $allowedCategories[$category] . '-' . $version . '.exe';
        $targetPath = rtrim(str_replace('\\', '/', $absoluteProgramUploadDir), '/') . '/' . $newFilename;
        $checksum = md5_file($fileInfo['tmp_name']);

        if (!move_uploaded_file($fileInfo['tmp_name'], $targetPath)) throw new Exception('无法移动上传的文件');

        $sql = "INSERT INTO versions (version, filename, filesize, checksum, upload_timestamp, type) VALUES (?, ?, ?, ?, NOW(), ?)
                ON DUPLICATE KEY UPDATE version = VALUES(version), filesize = VALUES(filesize), checksum = VALUES(checksum), upload_timestamp = NOW(), download_count = 0";
        $stmt = $pdo->prepare($sql);
        if (!$stmt->execute([$version, $newFilename, $fileInfo['size'], $checksum, $category])) {
            unlink($targetPath);
            throw new Exception('数据库操作失败');
        }
        return ['success' => true, 'message' => '程序文件已成功上传。', 'filename' => $newFilename];
    } catch (Exception $e) {
        return ['success' => false, 'message' => '处理失败: ' . $e->getMessage(), 'filename' => null];
    }
}

function handleSupportToolUpload(array $fileInfo, string $description, PDO $pdo, string $absoluteSupportToolUploadDir): array {
    if (!defined('MAX_FILE_SIZE_BYTES')) define('MAX_FILE_SIZE_BYTES', 50 * 1024 * 1024);
    try {
        if ($fileInfo['error'] !== UPLOAD_ERR_OK) throw new Exception('文件上传错误 (代码: ' . $fileInfo['error'] . ')');
        if (empty(trim($description))) throw new Exception('支持工具描述不能为空');
        if ($fileInfo['size'] > MAX_FILE_SIZE_BYTES) throw new Exception('文件大小超过50MB');
        if ($fileInfo['size'] == 0) throw new Exception('不允许上传空文件');

        $safeFilename = preg_replace('/[^a-zA-Z0-9_.-]/', '_', basename($fileInfo['name']));
        $targetPath = rtrim(str_replace('\\', '/', $absoluteSupportToolUploadDir), '/') . '/' . $safeFilename;
        
        if (file_exists($targetPath)) throw new Exception('名为 "' . htmlspecialchars($safeFilename) . '" 的文件已存在。');

        $checksum = md5_file($fileInfo['tmp_name']);
        if (!move_uploaded_file($fileInfo['tmp_name'], $targetPath)) throw new Exception('无法移动上传的文件');

        $fileMimeType = mime_content_type($targetPath) ?: strtolower(pathinfo($safeFilename, PATHINFO_EXTENSION));

        $sql = "INSERT INTO support_tools (filename, description, filesize, filetype, checksum, upload_timestamp) VALUES (?, ?, ?, ?, ?, NOW())";
        $stmt = $pdo->prepare($sql);
        if (!$stmt->execute([$safeFilename, $description, $fileInfo['size'], $fileMimeType, $checksum])) {
            unlink($targetPath);
            throw new Exception('数据库操作失败');
        }
        return ['success' => true, 'message' => '支持工具已成功上传。', 'filename' => $safeFilename];
    } catch (Exception $e) {
        return ['success' => false, 'message' => '处理失败: ' . $e->getMessage(), 'filename' => null];
    }
}
?>

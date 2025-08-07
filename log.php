<?php
// api/log.php (ÐÂÎÄ¼þ)
// Endpoint for client application to send real-time log messages.

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$log_dir = null;
try {
    $docRoot = realpath($_SERVER['DOCUMENT_ROOT']);
    if (!$docRoot) {
        throw new Exception("Could not determine DOCUMENT_ROOT.");
    }
    $log_dir = rtrim(str_replace('\\', '/', $docRoot), '/') . '/logs/';

    if (!is_dir($log_dir)) {
        if (!mkdir($log_dir, 0755, true)) {
            throw new Exception("Log directory does not exist and could not be created.");
        }
    }
    if (!is_writable($log_dir)) {
        throw new Exception("Log directory is not writable.");
    }
} catch (Exception $e) {
    error_log("Log API Configuration Error: " . $e->getMessage());
    http_response_code(500);
    echo json_encode(['success' => false, 'message' => 'Server configuration error (Log Directory)']);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);

    $deviceId = $input['device_id'] ?? null;
    $logMessage = $input['message'] ?? null;

    if (empty($deviceId) || !isset($logMessage)) {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => 'Missing or empty device_id or message']);
        exit;
    }

    $safe_device_id = preg_replace('/[^a-zA-Z0-9_-]/', '', $deviceId);
    if (empty($safe_device_id)) {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => 'Invalid device_id format']);
        exit;
    }

    $log_file_path = $log_dir . $safe_device_id . '.log';
    $formatted_message = $logMessage . PHP_EOL;

    if (file_put_contents($log_file_path, $formatted_message, FILE_APPEND | LOCK_EX) !== false) {
        http_response_code(200);
        echo json_encode(['success' => true, 'message' => 'Log received']);
    } else {
        error_log("Log API: Failed to write to log file for device {$safe_device_id} at path {$log_file_path}");
        http_response_code(500);
        echo json_encode(['success' => false, 'message' => 'Server error: Could not write to log file']);
    }

} else {
    http_response_code(405);
    echo json_encode(['success' => false, 'message' => 'Invalid request method. Only POST is allowed.']);
}

exit;
?>

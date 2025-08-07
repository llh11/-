<?php
// api/heartbeat.php
header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$pdo = null;
try {
    require_once __DIR__ . '/../includes/db_config.php';
} catch (Throwable $e) {
    error_log("Heartbeat API DB Config Error: " . $e->getMessage());
    http_response_code(500);
    echo json_encode(['success' => false, 'message' => 'Server configuration error (DB)']);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);

    $deviceId = $input['device_id'] ?? null;
    $appVersion = $input['version'] ?? null;
    $ipAddress = $_SERVER['REMOTE_ADDR'] ?? null;
    if (isset($_SERVER['HTTP_X_FORWARDED_FOR'])) {
        $ipAddress = explode(',', $_SERVER['HTTP_X_FORWARDED_FOR'])[0];
    }

    if (empty($deviceId)) {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => 'Missing or empty device_id']);
        exit;
    }

    if ($pdo instanceof PDO) {
        try {
            $sql = "INSERT INTO device_status (device_identifier, ip_address, app_version, last_seen, first_seen)
                    VALUES (:device_id, :ip, :version, NOW(), NOW())
                    ON DUPLICATE KEY UPDATE
                        ip_address = VALUES(ip_address),
                        app_version = VALUES(app_version),
                        last_seen = NOW()";

            $stmt = $pdo->prepare($sql);
            $stmt->execute([
                ':device_id' => $deviceId,
                ':ip' => $ipAddress,
                ':version' => $appVersion
            ]);
            http_response_code(200);
            echo json_encode(['success' => true, 'message' => 'Heartbeat received']);
        } catch (Throwable $e) {
            error_log("Heartbeat API DB Error: " . $e->getMessage() . " | Device: " . $deviceId);
            http_response_code(500);
            echo json_encode(['success' => false, 'message' => 'Server database error during heartbeat processing']);
        }
    } else {
        http_response_code(500);
        echo json_encode(['success' => false, 'message' => 'Server database connection error']);
    }
} else {
    http_response_code(405);
    echo json_encode(['success' => false, 'message' => 'Invalid request method. Only POST is allowed.']);
}
exit;
?>

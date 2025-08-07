<?php
// management/index.php v4.6 (Even Taller sticky headers)

if (session_status() == PHP_SESSION_NONE) {
    session_start();
}

// Ensure the output is treated as UTF-8
header('Content-Type: text/html; charset=utf-8');

$db_error_message = null;
$pdo = null;
try {
    require_once __DIR__ . '/../includes/db_config.php';
} catch (Throwable $e) {
    $db_error_message = "数据库配置错误，无法加载页面。";
    error_log("DB Config Error in management/index.php: " . $e->getMessage());
}

function formatBytes($bytes, $precision = 2) {
    $bytes = (float)$bytes;
    if ($bytes <= 0) return '0 B';
    $units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
    $pow = floor(log($bytes) / log(1024));
    $pow = min($pow, count($units) - 1);
    $bytes /= pow(1024, $pow);
    return round($bytes, $precision) . ' ' . $units[$pow];
}

$feedback_message = ''; $is_error = false;
if (isset($_SESSION['upload_message'])) {
    $feedback_message = $_SESSION['upload_message'];
    $is_error = isset($_SESSION['upload_error']) && $_SESSION['upload_error'];
    unset($_SESSION['upload_message']); unset($_SESSION['upload_error']);
}
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>服务器管理面板 v4.6</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css" rel="stylesheet">
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --main-bg-color: #f3f4f6;
            --card-bg-color: #ffffff;
            --text-color-primary: #1f2937;
            --text-color-secondary: #4b5563;
            --accent-color: #3b82f6;
            --accent-color-hover: #2563eb;
            --border-color: #e5e7eb;
        }
        body {
            background-color: var(--main-bg-color);
            color: var(--text-color-primary);
            font-family: 'Inter', '微软雅黑', sans-serif;
        }
        .card {
            background-color: var(--card-bg-color);
            border-radius: 0.75rem;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -2px rgba(0, 0, 0, 0.1);
            transition: all 0.3s ease-in-out;
            border: 1px solid var(--border-color);
        }
        .card:hover {
            box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.1);
            transform: translateY(-2px);
        }
        .btn-primary {
            background-color: var(--accent-color);
            color: white;
            font-weight: 600;
            padding: 0.75rem 1.5rem;
            border-radius: 0.5rem;
            transition: background-color 0.2s ease;
        }
        .btn-primary:hover {
            background-color: var(--accent-color-hover);
        }
        #drop-zone {
            border: 2px dashed var(--border-color);
            border-radius: 0.5rem;
            padding: 2rem;
            text-align: center;
            cursor: pointer;
            transition: background-color 0.2s ease, border-color 0.2s ease;
        }
        #drop-zone.dragover {
            border-color: var(--accent-color);
            background-color: #eff6ff;
        }
        #file-input { display: none; }
        select {
            -webkit-appearance: none; -moz-appearance: none; appearance: none;
            background-image: url('data:image/svg+xml;charset=US-ASCII,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%22292.4%22%20height%3D%22292.4%22%3E%3Cpath%20fill%3D%22%236B7280%22%20d%3D%22M287%2069.4a17.6%2017.6%200%200%200-13-5.4H18.4c-5%200-9.3%201.8-12.9%205.4A17.6%2017.6%200%200%200%200%2082.2c0%205%201.8%209.3%205.4%2012.9l128%20127.9c3.6%203.6%207.8%205.4%2012.8%205.4s9.2-1.8%2012.8-5.4L287%2095c3.5-3.5%205.4-7.8%205.4-12.8%200-5-1.9-9.2-5.5-12.8z%22%2F%3E%3C%2Fsvg%3E');
            background-repeat: no-repeat; background-position: right .7em top 50%; background-size: .65em auto; padding-right: 2.5em;
        }
        .table-container { max-height: 75vh; overflow-y: auto; }
        .table-container thead th { position: sticky; top: 0; z-index: 10; background-color: #f9fafb; }
        /* Restored and improved sticky position for category header */
        .table-container .category-header { 
            position: sticky;
            top: 60px; /* Adjusted based on main header's new height (py-6) */
            z-index: 9;
            background-color: #e5e7eb; 
            font-weight: bold; 
            text-align: center; 
            padding: 1rem; /* Increased padding */
        }
        #log-content {
            background-color: #111827; color: #d1d5db; font-family: 'Courier New', Courier, monospace;
            height: 400px; overflow-y: scroll; white-space: pre-wrap; word-wrap: break-word;
            border-radius: 0.5rem; padding: 1rem;
        }
        #log-content::-webkit-scrollbar { width: 8px; }
        #log-content::-webkit-scrollbar-track { background: #1f2937; }
        #log-content::-webkit-scrollbar-thumb { background-color: #4b5563; border-radius: 4px; }
    </style>
</head>
<body class="antialiased">
    <div class="container mx-auto p-4 md:p-8">
        <header class="mb-8 flex flex-col md:flex-row justify-between items-start md:items-center">
            <h1 class="text-4xl font-bold text-gray-800">服务器管理面板</h1>
            <div id="connection-status" class="flex items-center space-x-2 mt-2 md:mt-0">
                <div class="status-dot h-3 w-3 rounded-full bg-gray-400" title="正在检测..."></div>
                <span class="text-sm font-medium text-gray-600" id="server-ip">检测中...</span>
            </div>
        </header>

        <?php if ($feedback_message): ?>
        <div id="feedback-alert" class="transition-opacity duration-300 <?php echo $is_error ? 'bg-red-100 border-red-400 text-red-700' : 'bg-green-100 border-green-400 text-green-700'; ?> px-4 py-3 rounded-lg relative mb-6" role="alert">
            <strong class="font-bold"><?php echo $is_error ? '错误' : '成功'; ?>!</strong>
            <span class="block sm:inline"><?php echo htmlspecialchars($feedback_message); ?></span>
            <span class="absolute top-0 bottom-0 right-0 px-4 py-3" onclick="document.getElementById('feedback-alert').style.display='none';">
                <i class="fa-solid fa-xmark cursor-pointer"></i>
            </span>
        </div>
        <?php endif; ?>
        
        <?php if ($db_error_message): ?>
        <div class="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded-lg relative mb-6" role="alert">
             <strong class="font-bold">数据库错误!</strong> <span class="block sm:inline"><?php echo htmlspecialchars($db_error_message); ?></span>
        </div>
        <?php else: ?>
        <main class="grid grid-cols-1 lg:grid-cols-3 gap-8">
            <!-- Left Column -->
            <div class="lg:col-span-1 flex flex-col gap-8">
                <div class="card p-6">
                    <h2 class="text-xl font-semibold mb-4 text-gray-700 border-b pb-3"><i class="fas fa-upload mr-2"></i>上传新文件</h2>
                    <form action="upload_handler.php" method="post" enctype="multipart/form-data" id="upload-form" class="space-y-4 pt-4">
                        <div>
                             <label class="block text-gray-700 text-sm font-bold mb-2" for="category">文件类别 <span class="text-red-500">*</span></label>
                             <select class="shadow-sm border rounded w-full py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:ring-2 focus:ring-blue-500 bg-white" id="category" name="category" required>
                                 <option value="" disabled selected>-- 请选择类别 --</option>
                                 <option value="newsapp">新闻程序 (newsapp)</option>
                                 <option value="configurator">配置工具 (configurator)</option>
                                 <option value="supporttool">支持工具</option>
                             </select>
                        </div>
                        <div id="program-version-group">
                             <label class="block text-gray-700 text-sm font-bold mb-2" for="version">版本号 <span class="text-red-500">*</span></label>
                             <input class="shadow-sm appearance-none border rounded w-full py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:ring-2 focus:ring-blue-500" id="version" name="version" type="text" placeholder="例如: 1.0 或 1.2.3" pattern="\d+\.\d+(\.\d+)?">
                        </div>
                        <div id="support-tool-description-group" style="display: none;">
                             <label class="block text-gray-700 text-sm font-bold mb-2" for="description">工具描述 <span class="text-red-500">*</span></label>
                             <textarea class="shadow-sm appearance-none border rounded w-full py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:ring-2 focus:ring-blue-500" id="description" name="description" placeholder="输入支持工具的描述信息" rows="3"></textarea>
                        </div>
                        <div>
                             <label class="block text-gray-700 text-sm font-bold mb-2" for="program">选择文件或拖拽 <span class="text-red-500">*</span></label>
                             <div id="drop-zone">
                                 <i class="fas fa-cloud-arrow-up text-3xl text-gray-400"></i>
                                 <p class="mt-2">将文件拖拽到这里</p>
                                 <p class="text-xs text-gray-500 mt-1" id="file-type-hint">仅允许 .exe 文件</p>
                                 <input type="file" id="file-input" name="program_file" required>
                                 <p id="file-name" class="mt-2 text-sm text-blue-600 font-medium"></p>
                             </div>
                        </div>
                        <button class="btn-primary w-full" type="submit">
                            <i class="fas fa-paper-plane mr-2"></i>上传文件
                        </button>
                    </form>
                </div>
                <div class="card p-6">
                    <h2 class="text-xl font-semibold mb-4 text-gray-700 border-b pb-3"><i class="fas fa-server mr-2"></i>设备连接状态</h2>
                    <div class="space-y-3 text-center pt-4">
                        <div class="flex justify-around items-center">
                            <div>
                                <p class="text-3xl font-bold" id="total-devices">--</p>
                                <p class="text-sm text-gray-500">总设备</p>
                            </div>
                            <div>
                                <p class="text-3xl font-bold text-green-500" id="online-devices">--</p>
                                <p class="text-sm text-green-500">在线</p>
                            </div>
                            <div>
                                <p class="text-3xl font-bold text-red-500" id="offline-devices">--</p>
                                <p class="text-sm text-red-500">离线</p>
                            </div>
                        </div>
                        <div class="border-t pt-3 mt-4">
                            <h3 class="text-sm font-semibold text-gray-600 mb-2">最近活跃 IP</h3>
                            <div id="recent-ips-list" class="text-xs text-gray-500 h-20 overflow-y-auto">
                                <p>正在加载...</p>
                            </div>
                        </div>
                        <p class="text-xs text-gray-400 mt-2" id="status-last-updated">正在加载...</p>
                    </div>
                </div>
            </div>

            <!-- Right Column -->
            <div class="lg:col-span-2 flex flex-col gap-8">
                <div class="card p-6">
                    <div class="flex justify-between items-center mb-4 border-b pb-3">
                        <h2 class="text-xl font-semibold text-gray-700"><i class="fas fa-list-check mr-2"></i>已上传文件列表</h2>
                        <div class="text-right">
                            <span class="text-sm font-medium text-gray-600">总流量:</span>
                            <span class="text-lg font-bold text-indigo-600" id="total-traffic-display">加载中...</span>
                        </div>
                    </div>
                    <div class="table-container">
                        <table class="min-w-full leading-normal table-auto">
                            <thead>
                                <tr>
                                    <th class="px-5 py-6 border-b-2 border-gray-200 text-left text-xs font-semibold text-gray-600 uppercase tracking-wider">文件名与时间</th>
                                    <th class="px-5 py-6 border-b-2 border-gray-200 text-left text-xs font-semibold text-gray-600 uppercase tracking-wider">描述/类型</th>
                                    <th class="px-5 py-6 border-b-2 border-gray-200 text-right text-xs font-semibold text-gray-600 uppercase tracking-wider">下载</th>
                                    <th class="px-5 py-6 border-b-2 border-gray-200 text-center text-xs font-semibold text-gray-600 uppercase tracking-wider">操作</th>
                                </tr>
                            </thead>
                            <tbody class="text-gray-900" id="file-list-body">
                                <tr><td colspan="4" class="text-center p-5"><i class="fas fa-spinner fa-spin mr-2"></i>正在加载文件列表...</td></tr>
                            </tbody>
                        </table>
                    </div>
                </div>
                 <div class="card p-6">
                    <h2 class="text-xl font-semibold mb-4 text-gray-700 border-b pb-3"><i class="fas fa-receipt mr-2"></i>实时日志</h2>
                    <div class="flex items-center space-x-4 mb-4 pt-4">
                        <label for="log-selector" class="text-sm font-medium">选择设备日志:</label>
                        <select id="log-selector" class="flex-grow shadow-sm border rounded py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:ring-2 focus:ring-blue-500 bg-white">
                            <option>暂无日志文件</option>
                        </select>
                        <button id="refresh-logs-btn" class="text-gray-500 hover:text-blue-500 transition p-2 rounded-full hover:bg-gray-100" title="刷新日志列表">
                            <i class="fas fa-sync-alt"></i>
                        </button>
                    </div>
                    <div id="log-content">
                        <span class="text-gray-500">请选择一个日志文件查看内容。</span>
                    </div>
                </div>
            </div>
        </main>
        <?php endif; ?>
    </div>

    <script>
    document.addEventListener('DOMContentLoaded', () => {
        const categorySelect = document.getElementById('category');
        const versionGroup = document.getElementById('program-version-group');
        const descriptionGroup = document.getElementById('support-tool-description-group');
        const versionInput = document.getElementById('version');
        const descriptionInput = document.getElementById('description');
        const fileInput = document.getElementById('file-input');
        const fileTypeHint = document.getElementById('file-type-hint');
        const dropZone = document.getElementById('drop-zone');
        const fileNameDisplay = document.getElementById('file-name');
        const logSelector = document.getElementById('log-selector');
        const logContent = document.getElementById('log-content');
        const refreshLogsBtn = document.getElementById('refresh-logs-btn');

        const apiFetch = async (url, options = {}) => {
            try {
                const response = await fetch(url, { cache: 'no-cache', ...options });
                if (!response.ok) {
                    const errorText = await response.text();
                    throw new Error(`Network response was not ok (${response.status}): ${errorText}`);
                }
                return await response.json();
            } catch (error) {
                console.error(`API Fetch Error for ${url}:`, error);
                throw error;
            }
        };
        
        const checkConnection = () => {
            const statusDot = document.querySelector('#connection-status .status-dot');
            const serverIpDisplay = document.getElementById('server-ip');
            if (!statusDot || !serverIpDisplay) return;
            
            statusDot.className = 'status-dot h-3 w-3 rounded-full bg-yellow-400 animate-pulse';
            serverIpDisplay.textContent = '检测中...';
            
            apiFetch('../api/upload.php?action=ping')
                .then(data => {
                    statusDot.classList.remove('animate-pulse');
                    if (data?.success) {
                        statusDot.className = 'status-dot h-3 w-3 rounded-full bg-green-500';
                        serverIpDisplay.textContent = 'API 已连接';
                    } else { throw new Error('Ping failed'); }
                })
                .catch(() => {
                    statusDot.classList.remove('animate-pulse');
                    statusDot.className = 'status-dot h-3 w-3 rounded-full bg-red-500';
                    serverIpDisplay.textContent = '连接失败';
                });
        };

        const updateDeviceStatus = () => {
            const [totalEl, onlineEl, offlineEl, ipsEl, updatedEl] = [
                document.getElementById('total-devices'), document.getElementById('online-devices'),
                document.getElementById('offline-devices'), document.getElementById('recent-ips-list'),
                document.getElementById('status-last-updated')
            ];
            apiFetch('../api/status.php?action=get_counts')
                .then(data => {
                    if (data?.success) {
                        totalEl.textContent = data.total ?? 'N/A';
                        onlineEl.textContent = data.online ?? 'N/A';
                        offlineEl.textContent = data.offline ?? 'N/A';
                        ipsEl.innerHTML = '';
                        if (data.recent_ips?.length > 0) {
                            const ul = document.createElement('ul');
                            data.recent_ips.forEach(ip => {
                                const li = document.createElement('li');
                                li.textContent = ip;
                                ul.appendChild(li);
                            });
                            ipsEl.appendChild(ul);
                        } else { ipsEl.innerHTML = '<p>暂无记录</p>'; }
                        updatedEl.textContent = `最后更新: ${new Date().toLocaleTimeString()}`;
                    } else { throw new Error(data.message || 'Failed to fetch status'); }
                })
                .catch(() => {
                    [totalEl, onlineEl, offlineEl].forEach(el => el.textContent = '错误');
                    ipsEl.innerHTML = '<p class="text-red-500">获取失败</p>';
                    updatedEl.textContent = '更新失败';
                });
        };

        const updateFileList = () => {
            const fileListBody = document.getElementById('file-list-body');
            const totalTrafficDisplay = document.getElementById('total-traffic-display');
            
            const categoryNames = {
                'newsapp': '新闻程序 (newsapp)',
                'configurator': '配置工具 (configurator)',
                'supporttool': '支持工具',
            };

            apiFetch('../api/status.php?action=get_files')
                .then(data => {
                    if (data?.success) {
                        fileListBody.innerHTML = '';
                        let contentRendered = false;
                        const { filesByType = {} } = data;
                        const displayOrder = ['newsapp', 'configurator', 'supporttool'];

                        displayOrder.forEach(typeKey => {
                            if (filesByType[typeKey]?.length > 0) {
                                contentRendered = true;
                                const categoryDisplayName = categoryNames[typeKey] || `未知类别 (${typeKey})`;
                                const headerRow = `<tr><td colspan="4" class="category-header text-gray-700 text-sm">${categoryDisplayName}</td></tr>`;
                                fileListBody.insertAdjacentHTML('beforeend', headerRow);

                                filesByType[typeKey].forEach(file => {
                                    const deleteConfirmMsg = `确定要删除文件 '${(file.filename || '').replace(/'/g, "\\'")}' 及其数据库记录吗？`;
                                    const fileRow = `
                                        <tr class="hover:bg-gray-50">
                                            <td class="px-5 py-3 border-b border-gray-200 bg-white text-sm">
                                                <p class="whitespace-no-wrap break-all font-medium" title="${file.filename || ''}">${file.filename || 'N/A'}</p>
                                                <p class="text-xs text-gray-500">${new Date(file.upload_timestamp).toLocaleString()}</p>
                                            </td>
                                            <td class="px-5 py-3 border-b border-gray-200 bg-white text-sm max-w-xs truncate" title="${file.item_type === 'supporttool' ? (file.description || '') : (file.type || '')}">
                                                ${file.item_type === 'supporttool' ? (file.description || 'N/A') : (file.type || 'N/A')}
                                            </td>
                                            <td class="px-5 py-3 border-b border-gray-200 bg-white text-sm text-right">
                                                <p class="whitespace-no-wrap">${file.download_count || '0'}</p>
                                            </td>
                                            <td class="px-5 py-3 border-b border-gray-200 bg-white text-sm text-center whitespace-no-wrap">
                                                <a href="download_handler.php?file=${encodeURIComponent(file.filename)}&type=${file.item_type}" class="text-blue-600 hover:text-blue-900 mr-3" title="下载"><i class="fas fa-download"></i></a>
                                                <a href="delete_handler.php?file=${encodeURIComponent(file.filename)}&type=${file.item_type}" class="text-red-600 hover:text-red-900" title="删除" onclick="return confirm('${deleteConfirmMsg}');"><i class="fas fa-trash-alt"></i></a>
                                            </td>
                                        </tr>`;
                                    fileListBody.insertAdjacentHTML('beforeend', fileRow);
                                });
                            }
                        });

                        if (!contentRendered) {
                            fileListBody.innerHTML = '<tr><td colspan="4" class="text-center p-5 text-gray-500">没有文件记录。</td></tr>';
                        }
                        totalTrafficDisplay.textContent = data.totalTrafficFormatted || 'N/A';
                    } else { throw new Error(data.message || 'Failed to fetch file list'); }
                })
                .catch(err => {
                    fileListBody.innerHTML = `<tr><td colspan="4" class="text-center p-5 text-red-500">加载文件列表失败: ${err.message}</td></tr>`;
                });
        };
        
        const updateLogList = async () => {
            if (!logSelector) return;
            try {
                const data = await apiFetch('../api/status.php?action=get_logs');
                logSelector.innerHTML = '';
                if (data?.success && data.logs?.length > 0) {
                    data.logs.forEach(logFile => {
                        const option = document.createElement('option');
                        option.value = logFile;
                        option.textContent = logFile;
                        logSelector.appendChild(option);
                    });
                    updateLogContent();
                } else {
                    logSelector.innerHTML = '<option>暂无日志文件</option>';
                    logContent.innerHTML = '<span class="text-gray-500">请选择一个日志文件查看内容。</span>';
                }
            } catch (error) {
                logSelector.innerHTML = '<option>加载日志列表失败</option>';
            }
        };

        const updateLogContent = async () => {
            if (!logSelector || !logContent) return;
            const selectedLog = logSelector.value;
            if (!selectedLog || selectedLog.includes('失败') || selectedLog.includes('暂无')) {
                logContent.innerHTML = '<span class="text-gray-500">请选择一个有效的日志文件。</span>';
                return;
            }
            logContent.innerHTML = '<i class="fas fa-spinner fa-spin mr-2 text-gray-400"></i>正在加载日志内容...';
            try {
                const data = await apiFetch(`../api/status.php?action=get_log_content&file=${encodeURIComponent(selectedLog)}`);
                if (data?.success) {
                    logContent.textContent = data.content || '(日志文件为空)';
                    logContent.scrollTop = logContent.scrollHeight;
                } else {
                    logContent.textContent = `加载日志失败: ${data.message || '未知错误'}`;
                }
            } catch (error) {
                logContent.textContent = `加载日志时发生网络错误: ${error.message}`;
            }
        };

        if (categorySelect) {
            categorySelect.addEventListener('change', function() {
                const isSupportTool = this.value === 'supporttool';
                versionGroup.style.display = isSupportTool ? 'none' : 'block';
                descriptionGroup.style.display = isSupportTool ? 'block' : 'none';
                versionInput.required = !isSupportTool;
                descriptionInput.required = isSupportTool;
                fileInput.accept = isSupportTool ? '' : '.exe,application/octet-stream,application/x-msdownload,application/vnd.microsoft.portable-executable';
                fileTypeHint.textContent = isSupportTool ? '允许任何文件类型' : '仅允许 .exe 文件';
            });
            categorySelect.dispatchEvent(new Event('change'));
        }

        if (dropZone && fileInput && fileNameDisplay) {
            dropZone.addEventListener('click', () => fileInput.click());
            fileInput.addEventListener('change', () => { fileNameDisplay.textContent = fileInput.files.length > 0 ? `已选择: ${fileInput.files[0].name}` : ''; });
            ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(evName => {
                dropZone.addEventListener(evName, e => { e.preventDefault(); e.stopPropagation(); }, false);
                document.body.addEventListener(evName, e => { e.preventDefault(); e.stopPropagation(); }, false);
            });
            ['dragenter', 'dragover'].forEach(evName => dropZone.addEventListener(evName, () => dropZone.classList.add('dragover'), false));
            ['dragleave', 'drop'].forEach(evName => dropZone.addEventListener(evName, () => dropZone.classList.remove('dragover'), false));
            dropZone.addEventListener('drop', e => {
                const files = e.dataTransfer.files;
                if (files.length > 0) { fileInput.files = files; fileNameDisplay.textContent = `已选择: ${files[0].name}`; }
            }, false);
        }

        logSelector?.addEventListener('change', updateLogContent);
        refreshLogsBtn?.addEventListener('click', updateLogList);
        
        checkConnection();
        updateDeviceStatus();
        updateFileList();
        updateLogList();
        setInterval(checkConnection, 30000);
        setInterval(updateDeviceStatus, 15000);
        setInterval(updateFileList, 60000);
    });
    </script>
</body>
</html>

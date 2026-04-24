-- ============================================
-- MyMuduo Chat 增量数据库迁移脚本
-- 版本：v0.7 → v1.0 (增量部分)
-- 日期：2026-04-25
-- ============================================

USE chat;

-- ============================================
-- 1. 优化 allgroup 表（添加群公告功能）
-- ============================================

ALTER TABLE allgroup
ADD COLUMN announcement TEXT COMMENT '群公告';

ALTER TABLE allgroup
ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间';

-- ============================================
-- 2. 创建 message 表（统一消息存储）
-- ============================================

CREATE TABLE IF NOT EXISTS message (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '消息ID',
    from_id INT NOT NULL COMMENT '发送者ID',
    to_id INT NOT NULL COMMENT '接收者ID（一对一）或群组ID（群聊）',
    msg_type ENUM('private', 'group') NOT NULL COMMENT '消息类型：private-私聊 group-群聊',
    content TEXT NOT NULL COMMENT '消息内容',
    is_recalled TINYINT(1) DEFAULT 0 COMMENT '是否已撤回 0-否 1-是',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间',

    INDEX idx_to_id (to_id),
    INDEX idx_from_id (from_id),
    INDEX idx_created_at (created_at),
    INDEX idx_msg_type_to_id (msg_type, to_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='统一消息存储表';

-- ============================================
-- 3. 创建 message_read_status 表（消息已读状态）
-- ============================================

CREATE TABLE IF NOT EXISTS message_read_status (
    message_id BIGINT NOT NULL COMMENT '消息ID',
    user_id INT NOT NULL COMMENT '用户ID',
    is_read TINYINT(1) DEFAULT 0 COMMENT '是否已读 0-未读 1-已读',
    read_at TIMESTAMP NULL COMMENT '阅读时间',

    PRIMARY KEY (message_id, user_id),
    INDEX idx_user_id (user_id),
    INDEX idx_user_unread (user_id, is_read)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息已读状态表';

-- ============================================
-- 4. 数据迁移：offlinemessage → message
-- ============================================

INSERT INTO message (from_id, to_id, msg_type, content, created_at)
SELECT
    JSON_EXTRACT(message, '$.id') AS from_id,
    userid AS to_id,
    'private' AS msg_type,
    JSON_EXTRACT(message, '$.msg') AS content,
    created_at
FROM offlinemessage
WHERE JSON_VALID(message) = 1
  AND JSON_EXTRACT(message, '$.msgid') = 20  -- CHAT_MSG = 20
ON DUPLICATE KEY UPDATE id=id;

INSERT INTO message (from_id, to_id, msg_type, content, created_at)
SELECT
    JSON_EXTRACT(message, '$.id') AS from_id,
    JSON_EXTRACT(message, '$.groupid') AS to_id,
    'group' AS msg_type,
    JSON_EXTRACT(message, '$.msg') AS content,
    created_at
FROM offlinemessage
WHERE JSON_VALID(message) = 1
  AND JSON_EXTRACT(message, '$.msgid') = 34  -- GROUP_CHAT_MSG = 34
ON DUPLICATE KEY UPDATE id=id;

-- ============================================
-- 6. 验证迁移结果
-- ============================================

SELECT 'friend 表记录数' AS info, COUNT(*) AS count FROM friend
UNION ALL
SELECT 'allgroup 表记录数', COUNT(*) FROM allgroup
UNION ALL
SELECT 'message 表记录数', COUNT(*) FROM message
UNION ALL
SELECT 'offlinemessage 表记录数', COUNT(*) FROM offlinemessage;

SELECT '增量迁移完成！v0.7 → v1.0' AS status;

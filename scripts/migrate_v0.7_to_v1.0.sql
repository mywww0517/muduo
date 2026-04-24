-- ============================================
-- MyMuduo Chat 数据库迁移脚本
-- 版本：v0.7 → v1.0
-- 日期：2026-04-25
-- 说明：添加好友管理、群组管理、消息功能增强所需的表结构
-- ============================================

USE chat;

-- ============================================
-- 1. 优化 friend 表（添加备注和黑名单功能）
-- ============================================

-- 添加字段（如果字段已存在会报错，可忽略）
ALTER TABLE friend
ADD COLUMN remark VARCHAR(50) DEFAULT '' COMMENT '好友备注名';

ALTER TABLE friend
ADD COLUMN is_blocked TINYINT(1) DEFAULT 0 COMMENT '是否拉黑 0-否 1-是';

ALTER TABLE friend
ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '添加好友时间';

-- 为 friend 表添加索引，优化查询性能
ALTER TABLE friend
ADD INDEX idx_userid_blocked (userid, is_blocked);

-- ============================================
-- 2. 优化 allgroup 表（添加群公告功能）
-- ============================================

ALTER TABLE allgroup
ADD COLUMN announcement TEXT COMMENT '群公告';

ALTER TABLE allgroup
ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间';

-- ============================================
-- 3. 创建 message 表（统一消息存储）
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
-- 4. 创建 message_read_status 表（消息已读状态）
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
-- 5. 数据迁移：offlinemessage → message
-- ============================================

-- 将离线消息迁移到新的 message 表
-- 注意：offlinemessage 表中的 message 字段是 JSON 格式，需要解析
-- 这里假设 JSON 格式为：{"msgid":5,"id":1,"name":"user1","toid":2,"msg":"hello","time":"..."}

INSERT INTO message (from_id, to_id, msg_type, content, created_at)
SELECT
    JSON_EXTRACT(message, '$.id') AS from_id,
    userid AS to_id,
    'private' AS msg_type,
    JSON_EXTRACT(message, '$.msg') AS content,
    created_at
FROM offlinemessage
WHERE JSON_VALID(message) = 1
  AND JSON_EXTRACT(message, '$.msgid') = 5  -- CHAT_MSG = 5
ON DUPLICATE KEY UPDATE id=id;  -- 避免重复插入

-- 群聊离线消息迁移
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

-- 查看各表记录数
SELECT 'friend 表记录数' AS info, COUNT(*) AS count FROM friend
UNION ALL
SELECT 'allgroup 表记录数', COUNT(*) FROM allgroup
UNION ALL
SELECT 'message 表记录数', COUNT(*) FROM message
UNION ALL
SELECT 'offlinemessage 表记录数', COUNT(*) FROM offlinemessage;

-- 查看 friend 表结构
SHOW COLUMNS FROM friend;

-- 查看 allgroup 表结构
SHOW COLUMNS FROM allgroup;

-- 查看 message 表结构
SHOW COLUMNS FROM message;

-- 查看 message_read_status 表结构
SHOW COLUMNS FROM message_read_status;

-- ============================================
-- 7. 回滚脚本（如需回滚，执行以下语句）
-- ============================================

/*
-- 回滚 friend 表
ALTER TABLE friend
DROP COLUMN IF EXISTS remark,
DROP COLUMN IF EXISTS is_blocked,
DROP COLUMN IF EXISTS created_at;

DROP INDEX IF EXISTS idx_userid_blocked ON friend;

-- 回滚 allgroup 表
ALTER TABLE allgroup
DROP COLUMN IF EXISTS announcement,
DROP COLUMN IF EXISTS created_at;

-- 删除新表
DROP TABLE IF EXISTS message_read_status;
DROP TABLE IF EXISTS message;
*/

-- ============================================
-- 迁移完成
-- ============================================

SELECT '数据库迁移完成！v0.7 → v1.0' AS status;

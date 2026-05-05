-- v2.0 AI 用户初始化脚本

-- 创建 AI 用户（特殊用户 ID: 999999）
INSERT INTO user (id, name, password, state)
VALUES (999999, 'AI', '', 'online')
ON DUPLICATE KEY UPDATE state='online';

-- AI 自动加入所有现有群组
INSERT INTO groupuser (groupid, userid, grouprole)
SELECT id, 999999, 'normal' FROM allgroup
WHERE NOT EXISTS (
    SELECT 1 FROM groupuser
    WHERE groupid = allgroup.id AND userid = 999999
);

-- 验证
SELECT * FROM user WHERE id = 999999;
SELECT COUNT(*) as ai_joined_groups FROM groupuser WHERE userid = 999999;

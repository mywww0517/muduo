#pragma once
#include <string>

class Group {
public:
    Group(int id = -1, const std::string& name = "", const std::string& desc = "")
        : id_(id), name_(name), desc_(desc) {}

    void setId(int id) { id_ = id; }
    void setName(const std::string& name) { name_ = name; }
    void setDesc(const std::string& desc) { desc_ = desc; }

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& desc() const { return desc_; }

private:
    int id_;
    std::string name_;
    std::string desc_;
};

class GroupUser {
public:
    GroupUser(int groupid = -1, int userid = -1, const std::string& role = "normal")
        : groupid_(groupid), userid_(userid), role_(role) {}

    void setGroupId(int id) { groupid_ = id; }
    void setUserId(int id) { userid_ = id; }
    void setRole(const std::string& role) { role_ = role; }

    int groupId() const { return groupid_; }
    int userId() const { return userid_; }
    const std::string& role() const { return role_; }

private:
    int groupid_;
    int userid_;
    std::string role_;
};

#pragma once
#include <string>

class User {
public:
    User(int id = -1,
         const std::string& name = "",
         const std::string& pwd = "",
         const std::string& state = "offline")
        : id_(id), name_(name), password_(pwd), state_(state), remark_(""), is_blocked_(false) {}

    void setId(int id) {id_ = id;}
    void setName(const std::string& name) {name_ = name;}
    void setPassword(const std::string& pwd) {password_ = pwd;}
    void setState(const std::string& state) {state_ = state;}
    void setRemark(const std::string& remark) {remark_ = remark;}
    void setIsBlocked(bool is_blocked) {is_blocked_ = is_blocked;}

    int id() const {return id_;}
    const std::string& name() const {return name_;}
    const std::string& password() const {return password_;}
    const std::string& state() const {return state_;}
    const std::string& remark() const {return remark_;}
    bool isBlocked() const {return is_blocked_;}

private:
    int id_;
    std::string name_;
    std::string password_;
    std::string state_;
    std::string remark_;
    bool is_blocked_;
};
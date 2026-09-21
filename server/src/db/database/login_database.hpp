#pragma once

#include <mariadb/mysql.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "connection/sql_connection.hpp"

namespace lit::db {
enum class LoginSTMT : std::uint8_t {
    AddUser = 0x01,
    GetUserByName = 0x02,
    GetUserByID = 0x03,
    RemoveUserByName = 0x04,
};

struct LoginConfig {
    static constexpr std::uint8_t kNumStmts = 255;
    static constexpr std::uint8_t kLoginSize = 64;
    static constexpr std::uint8_t kPasswordHashSize = 255;
    static constexpr std::uint8_t kPasswordSaltSize = 64;
    static constexpr std::uint8_t kPasswordAlgoSize = 16;
};

struct LoginData {
    std::uint64_t id;
    char login[LoginConfig::kLoginSize];
    std::uint8_t password_hash[LoginConfig::kPasswordHashSize];
    std::uint8_t password_salt[LoginConfig::kPasswordSaltSize];
    char password_algo[LoginConfig::kPasswordAlgoSize];

    unsigned long login_length;
    unsigned long hash_length;
    unsigned long salt_length;
    unsigned long algo_length;
};

class LoginDataBase {
  public:
    explicit LoginDataBase(SQLConnection&& connection);
    LoginDataBase(const LoginDataBase&) = delete;
    LoginData& operator=(const LoginDataBase&) = delete;
    ~LoginDataBase();

    void prepare_statements();

    bool insert(std::string login, const std::string& password);
    std::unique_ptr<LoginData> get(std::string login);
    bool remove(std::string login);

  private:
    void init_stmts(std::size_t size);
    void prepare_stmt(MYSQL* db, LoginSTMT code, const std::string& sql);

    SQLConnection connection_;
    std::vector<MYSQL_STMT*> stmts_;
};
}  // namespace lit::db

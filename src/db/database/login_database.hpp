#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <memory>

#include <mariadb/mysql.h>

#include "connection/sql_connection.hpp"

namespace ep::db
{
  enum class LoginSTMT: std::uint8_t {
    AddUser                 = 0x01,
    GetUserByName           = 0x02,
    GetUserByID             = 0x03,
    RemoveUserByName        = 0x04,
  };

  struct LoginConfig {
    static constexpr std::uint8_t kNumStmts           = 255;
    static constexpr std::uint8_t kLoginSize          = 64;
    static constexpr std::uint8_t kPasswordHashSize   = 255;
    static constexpr std::uint8_t kPasswordSaltSize   = 64;
    static constexpr std::uint8_t kPasswordAlgoSize   = 16;
  };

  struct LoginData {
    std::uint64_t id_;
    char login_[LoginConfig::kLoginSize];
    std::uint8_t password_hash_[LoginConfig::kPasswordHashSize];
    std::uint8_t password_salt_[LoginConfig::kPasswordSaltSize];
    char password_algo_[LoginConfig::kPasswordAlgoSize];

    unsigned long login_length_;
    unsigned long hash_length_;
    unsigned long salt_length_;
    unsigned long algo_length_;
  };

class LoginDataBase {
  public:
    explicit LoginDataBase(SQLConnection&& connection);
    LoginDataBase(const LoginDataBase&) = delete;
    LoginData& operator=(const LoginDataBase&) = delete;
    ~LoginDataBase();

    void PrepareStatements(MYSQL* db);

    bool Insert(std::string login, const std::string& password);
    std::unique_ptr<LoginData> Get(std::string login);
    bool Remove(std::string login);

  private:
    void InitStmts(std::size_t size);
    void PrepareSTMT(MYSQL* db, LoginSTMT code, const std::string& sql);

    SQLConnection connection_;
    std::vector<MYSQL_STMT*> stmts_;
  };
}

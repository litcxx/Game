#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include <mariadb/mysql.h>

#include "connection/sql_connection.hpp"

namespace ep::db
{
  enum class LoginSTMT: std::uint8_t {
    AddUser
  };

  class LoginDataBase {
  public:
    explicit LoginDataBase(std::unique_ptr<SQLConnection> connection);

    void PrepareStatements(MYSQL* db);
  private:
    void PrepareSTMT(MYSQL* db, LoginSTMT code, const std::string& sql);

    std::unique_ptr<SQLConnection> connection_;
    std::vector<MYSQL_STMT*> stmts_;
  };
}

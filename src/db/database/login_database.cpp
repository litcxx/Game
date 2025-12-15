#include "login_database.hpp"
#include "mariadb_com.h"

#include <cstdint>
#include <limits>

#include <spdlog/spdlog.h>

namespace ep::db
{
  LoginDataBase::LoginDataBase(std::unique_ptr<SQLConnection> connection) :
    connection_(std::move(connection))
  {
    stmts_.resize(std::numeric_limits<std::uint8_t>::max());
    PrepareStatements(connection->GetDB());
  }

  void LoginDataBase::PrepareSTMT(MYSQL* db, LoginSTMT code, const std::string& sql)
  {
    MYSQL_STMT* stmt = mysql_stmt_init(db);
    if (stmt == nullptr) {
      spdlog::error("mysql_stmt_init: {}", mysql_error(db));
      stmts_[static_cast<std::uint8_t>(code)] = nullptr;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
      spdlog::error("mysql_stmt_prepare: {}", mysql_error(db));
      if (mysql_stmt_close(stmt) != 0) {
        spdlog::error("mysql_stmt_close: {}", mysql_error(db));
      }
      stmts_[static_cast<std::uint8_t>(code)] = nullptr;
    } else {
      stmts_[static_cast<std::uint8_t>(code)] = stmt;
    }
  }

  void LoginDataBase::PrepareStatements(MYSQL* db)
  {
    PrepareSTMT(connection_->GetDB(), LoginSTMT::AddUser, "INSERT INTO  users (login, password) VALUES (?, ?)");
  }

  void LoginDataBase::Insert(UserData data)
  {
    MYSQL_BIND bind[2];
    std::memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = data.name_;
    bind[0].buffer_length = strlen(data.name_);

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = data.password_;
    bind[1].buffer_length = strlen(data.password_);

    MYSQL_STMT* stmt = stmts_[static_cast<std::uint8_t>(LoginSTMT::AddUser)];
    if (mysql_stmt_bind_param(stmt, bind))
      return spdlog::error("mysql_stmt_bind_param:\nname: {}\npassword: {}", data.name_, data.password_);

    if (mysql_stmt_execute(stmt))
      return spdlog::error("mysql_stmt_execute:\nname: {}\npassword: {}", data.name_, data.password_);
  }

  std::optional<UserData> Get(std::string name)
  {
    return std::nullopt;
  }
}

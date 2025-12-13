#include "login_database.hpp"

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
}

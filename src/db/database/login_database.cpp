#include "login_database.hpp"

#include <spdlog/spdlog.h>

#include "mariadb_com.h"
#include "sha256.hpp"

namespace ep::db
{
  LoginDataBase::LoginDataBase(SQLConnection&& connection) :
    connection_(std::move(connection))
  {
    InitStmts(LoginConfig::kNumStmts);
    PrepareStatements(connection.GetDB());
  }

  LoginDataBase::~LoginDataBase()
  {
    for (std::size_t i = 0; i < stmts_.size(); i++) {
      if (stmts_[i] != nullptr)
      mysql_stmt_close(stmts_[i]);
    }
  }

  void LoginDataBase::InitStmts(std::size_t size)
  {
    stmts_.resize(size);
    for (auto& e : stmts_)
      e = nullptr;
  }

  void LoginDataBase::PrepareSTMT(MYSQL* db, LoginSTMT code, const std::string& sql)
  {
    MYSQL_STMT* stmt = mysql_stmt_init(db);
    if (stmt == nullptr) {
      spdlog::error("mysql_stmt_init: {}", mysql_error(db));
      stmts_[static_cast<std::size_t>(code)] = nullptr;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
      spdlog::error("mysql_stmt_prepare: {}", mysql_error(db));
      if (mysql_stmt_close(stmt) != 0) {
        spdlog::error("mysql_stmt_close: {}", mysql_error(db));
      }
      stmts_[static_cast<std::size_t>(code)] = nullptr;
    } else {
      stmts_[static_cast<std::size_t>(code)] = stmt;
    }
  }

  void LoginDataBase::PrepareStatements(MYSQL* db)
  {
    PrepareSTMT(connection_.GetDB(), LoginSTMT::AddUser, "INSERT INTO  accounts (login, password_hash, password_salt, password_algo) VALUES (?, ?, ?, ?);");
    PrepareSTMT(connection_.GetDB(), LoginSTMT::GetUserByName, "SELECT id, login, password_hash, password_salt, password_algo FROM accounts WHERE login = ? LIMIT 1;");
    PrepareSTMT(connection_.GetDB(), LoginSTMT::RemoveUserByName, "DELETE FROM accounts WHERE login = ? LIMIT 1;");
  }

  bool LoginDataBase::Insert(std::string login, const std::string& password)
  {
    MYSQL_STMT* stmt = stmts_[static_cast<std::size_t>(LoginSTMT::AddUser)];
    if (stmt == nullptr) {
      spdlog::error("STMT prepare state error, `AddUser` not found: {}", mysql_stmt_error(stmt));
      return false;
    }

    auto salt = crypto::GenerateSalt();
    if (!salt) {
      spdlog::error("Generate salt error:\nfile: {}\nline: {}", __FILE__, __LINE__);
      return false;
    }

    auto hash = crypto::Hash(password, salt.value());
    if (!hash) {
      spdlog::error("Hash error:\nfile {}\nline: {}", __FILE__, __LINE__);
      return false;
    }

    std::string algo{"sha256"};

    MYSQL_BIND params[4];
    std::memset(params, 0, sizeof(params));

    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = login.data();
    params[0].buffer_length = login.size();

    params[1].buffer_type = MYSQL_TYPE_BLOB;
    params[1].buffer = hash.value().data();
    params[1].buffer_length = hash.value().size();

    params[2].buffer_type = MYSQL_TYPE_BLOB;
    params[2].buffer = salt.value().data();
    params[2].buffer_length = salt.value().size();

    params[3].buffer_type = MYSQL_TYPE_STRING;
    params[3].buffer = algo.data();
    params[3].buffer_length = algo.size();

    if (mysql_stmt_bind_param(stmt, params)) {
      spdlog::error("mysql_stmt_bind_param error: {}", mysql_stmt_error(stmt));
      return false;
    }

    if (mysql_stmt_execute(stmt)) {
      spdlog::error("mysql_stmt_execute error: {}", mysql_stmt_error(stmt));
      return false;
    }

    return true;
  }

  std::unique_ptr<LoginData> LoginDataBase::Get(std::string login)
  {
    MYSQL_STMT* stmt = stmts_[static_cast<std::size_t>(LoginSTMT::GetUserByName)];
    if (stmt == nullptr) {
      spdlog::error("STMT prepare state error, `GetUserByName` not found\nfile: {} line: {}", __FILE__, __LINE__);
      return {};
    }

    MYSQL_BIND params;
    std::memset(&params, 0, sizeof(params));
  
    params.buffer_type = MYSQL_TYPE_STRING;
    params.buffer = login.data();
    params.buffer_length = login.size();

    if (mysql_stmt_bind_param(stmt, &params)) {
      spdlog::error("mysql_stmt_bind_params error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return {};
    }

    if (mysql_stmt_execute(stmt)) {
      spdlog::error("mysql_stmt_execute error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return {};
    }
    
    auto res = std::make_unique<LoginData>();
    MYSQL_BIND results[5];
    std::memset(results, 0, sizeof(results));

    results[0].buffer_type = MYSQL_TYPE_LONGLONG;
    results[0].buffer = &res->id_;
    results[0].buffer_length = sizeof(res->id_);

    results[1].buffer_type = MYSQL_TYPE_STRING;
    results[1].buffer = (void*)res->login_;
    results[1].buffer_length = sizeof(res->login_);
    results[1].length = &res->login_length_;

    results[2].buffer_type = MYSQL_TYPE_BLOB;
    results[2].buffer = res->password_hash_;
    results[2].buffer_length = sizeof(res->password_hash_);
    results[2].length = &res->hash_length_;

    results[3].buffer_type = MYSQL_TYPE_BLOB;
    results[3].buffer = res->password_salt_;
    results[3].buffer_length = sizeof(res->password_salt_);
    results[3].length = &res->salt_length_;

    results[4].buffer_type = MYSQL_TYPE_STRING;
    results[4].buffer = res->password_algo_;
    results[4].buffer_length = sizeof(res->password_algo_);
    results[4].length = &res->algo_length_;

    if (mysql_stmt_bind_result(stmt, results)) {
      spdlog::error("mysql_stmt_bind_result error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return {};
    }

    if (mysql_stmt_store_result(stmt)) {
      spdlog::error("mysql_stmt_store_result error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return {};
    }

    int rc = mysql_stmt_fetch(stmt);
    if (rc == MYSQL_NO_DATA) {
      spdlog::warn("Attempt to query a non-existent user: {}\nfile: {} line: {}", login, __FILE__, __LINE__);
      return {};
    }

    if (rc != 0) {
      spdlog::error("mysql_stmt_fetch error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return {};
    }

    return res;
  }

  bool LoginDataBase::Remove(std::string login)
  {
    MYSQL_STMT* stmt = stmts_[static_cast<std::size_t>(LoginSTMT::RemoveUserByName)];
    if (stmt == nullptr) {
      spdlog::error("STMT prepare state error, `RemoveUserByName` not found\nfile: {} line: {}", __FILE__, __LINE__);
      return false;
    }

    MYSQL_BIND params;
    std::memset(&params, 0, sizeof(params));
  
    params.buffer_type = MYSQL_TYPE_STRING;
    params.buffer = login.data();
    params.buffer_length = login.size();

    if (mysql_stmt_bind_param(stmt, &params)) {
      spdlog::error("mysql_stmt_bind_params error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return false;
    }

    if (mysql_stmt_execute(stmt)) {
      spdlog::error("mysql_stmt_execute error: {}\nfile: {} line: {}", mysql_stmt_error(stmt), __FILE__, __LINE__);
      return false;
    }

    return true;
  }
}

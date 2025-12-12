#include "sql_connection.hpp"

namespace ep::db
{
  SQLConnection::SQLConnection(const std::string& table_name) :
    table_name_(table_name),
    db_(nullptr)
  {}

  SQLConnection::~SQLConnection()
  {
    if (db_)
      mysql_close(db_);
  }
  
  
  SQLConnection::SQLConnection(SQLConnection&& other) : db_(other.db_)
  {
    other.db_ = nullptr;
  }

  
  SQLConnection& SQLConnection::operator=(SQLConnection&& other)
  {
    if (this == &other)
      return *this;
    db_ = other.db_;
    other.db_ = nullptr;
    return *this;
  }

  
  std::optional<SQLConnection> SQLConnection::Load(const std::string& host, const std::string& user, 
                                         const std::string& pswd, const std::string& db_name, 
                                         const std::string& table_name)
  {
    SQLConnection db(table_name);
    if (!db.Init())
      return std::nullopt;
    if (!db.Connect(host, user, pswd, db_name))
      return std::nullopt;

     return std::optional<SQLConnection>(std::move(db));
  }

  
  bool SQLConnection::Init()
  {
    spdlog::info("Init database...");
    db_ = mysql_init(nullptr);
    if (db_ == nullptr) {
      spdlog::error("mysql_init error: {}", mysql_error(db_));
      return false;
    }
    spdlog::info("Sucess");
    return true;
  }

  
  bool SQLConnection::Connect(const std::string& host, const std::string& user, 
                         const std::string& pswd, const std::string& db_name)
  {
    spdlog::info("Connect to database...");
    if (mysql_real_connect(db_, host.c_str(), user.c_str(), pswd.c_str(), db_name.c_str(), 0, nullptr, 0) == nullptr) {
      spdlog::error("mysql_real_connect error: {}", mysql_error(db_));
      return false;
    }
    spdlog::info("Sucess");
    return true;
  }
}

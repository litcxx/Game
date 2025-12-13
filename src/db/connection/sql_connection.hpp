#pragma once

#include <string>
#include <optional>

#include <mariadb/mysql.h>

#include "i_connection.hpp"

namespace ep::db 
{
  class SQLConnection: public IConnection {
  public:
    ~SQLConnection();
    SQLConnection(const SQLConnection&) = delete;
    SQLConnection& operator=(const SQLConnection&) = delete;
    SQLConnection(SQLConnection&& other);
    SQLConnection& operator=(SQLConnection&& other);

    static std::optional<SQLConnection> Load(const std::string& host, const std::string& user, 
                                        const std::string& password, const std::string& db_name,
                                        const std::string& table_name);
  private:
    explicit SQLConnection(const std::string& table_name);

    bool Init();
    bool Connect(const std::string& host, const std::string& user, 
                 const std::string& password, const std::string& db_name);

    const std::string table_name_;
    MYSQL* db_;
  };
}

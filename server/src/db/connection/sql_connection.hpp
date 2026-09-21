#pragma once

#include <mariadb/mysql.h>

#include <optional>
#include <string>

#include "i_connection.hpp"

namespace lit::db {
class SQLConnection : public IConnection {
  public:
    ~SQLConnection();
    SQLConnection(const SQLConnection&) = delete;
    SQLConnection& operator=(const SQLConnection&) = delete;
    SQLConnection(SQLConnection&& other);
    SQLConnection& operator=(SQLConnection&& other);

    static std::optional<SQLConnection> load(const std::string& host, const std::string& user,
                                             const std::string& password,
                                             const std::string& db_name);

    MYSQL* get_db() { return db_; }

  private:
    SQLConnection();

    bool init();
    bool connect(const std::string& host, const std::string& user, const std::string& password,
                 const std::string& db_name);

    MYSQL* db_;
};
}  // namespace lit::db

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>

#include <spdlog/spdlog.h>

#include "connection/sql_connection.hpp"
#include "database/login_database.hpp"
#include "config/config.hpp"
#include "spdlog/common.h"

int main(int argc, char** argv)
{
  using namespace ep;

  if (argc != 2) {
    spdlog::error("Usage: {} <config>", argv[0]);
    return EXIT_FAILURE;
  }

  // Load config
  auto config = Config::GetInstance(argv[1]);

  // Load sql_connection
  auto con = ep::db::SQLConnection::Load(
    config->accounts_db_config_.host_,
    config->accounts_db_config_.user_,
    config->accounts_db_config_.password_,
    config->accounts_db_config_.db_name_
  );

  if (!con) {
    spdlog::error("SQLConnection::Load(): Failed");
    return EXIT_FAILURE;
  }
  spdlog::info("SQLConnection::Load(): Success");

  db::LoginDataBase login_db(std::move(con.value()));

  std::string login("user");
  std::string password("1234567");

  if (!login_db.Insert(login, password)) {
    spdlog::error("LoginDataBase::Insert(): Failed");
    return EXIT_FAILURE;
  }
  spdlog::info("LoginDataBase::Insert(): Success");

  auto data = login_db.Get(login);
  if (!data) {
    spdlog::error("LoginDataBase::Get(): Failed");
    return EXIT_FAILURE;
  }
  spdlog::info("LoginDataBase::Get(): Success");

  if (!login_db.Remove(login)) {
    spdlog::error("LoginDataBase::Remove(): Failed");
    return EXIT_FAILURE;
  }
  spdlog::info("LoginDataBase::Remove(): Success");

  return EXIT_SUCCESS;
}

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "connection/sql_connection.hpp"
#include "config/config.hpp"
#include "spdlog/common.h"

int main(int argc, char** argv)
{
  if (argc != 2) {
    spdlog::error("Usage: {} <config>", argv[0]);
    return EXIT_FAILURE;
  }

  // Load config
  auto config = ep::Config::GetInstance(argv[1]);

  // Load sql_connection
  auto db = ep::db::SQLConnection::Load(
    config->login_db_config_.host_,
    config->login_db_config_.user_,
    config->login_db_config_.password_,
    config->login_db_config_.db_name_
  );

  if (!db) {
    spdlog::error("Load config error:\n"\
      "host: {}\nuser: {}\npasswprd: {}\ndatabase name: {}\n",
      config->login_db_config_.host_,
      config->login_db_config_.user_,
      config->login_db_config_.password_,
      config->login_db_config_.db_name_
    );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

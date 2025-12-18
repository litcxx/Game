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

  return EXIT_SUCCESS;
}

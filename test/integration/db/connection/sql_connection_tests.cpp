#include <spdlog/spdlog.h>

#include <cstdlib>

#include "config/config.hpp"
#include "connection/sql_connection.hpp"
#include "spdlog/common.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        spdlog::error("Usage: {} <config>", argv[0]);
        return EXIT_FAILURE;
    }

    // Load config
    auto config = ep::Config::get_instance(argv[1]);

    // Load sql_connection
    auto con = ep::db::SQLConnection::load(
        config->accounts_db_config.host, config->accounts_db_config.user,
        config->accounts_db_config.password, config->accounts_db_config.db_name);

    if (!con) {
        spdlog::error("SQLConnection::Load(): Failed");
        return EXIT_FAILURE;
    }
    spdlog::info("SQLConnection::Load(): Success");

    return EXIT_SUCCESS;
}

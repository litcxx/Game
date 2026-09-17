#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "config/config.hpp"
#include "connection/sql_connection.hpp"
#include "database/login_database.hpp"
#include "spdlog/common.h"

int main(int argc, char** argv) {
    using namespace ep;

    if (argc != 2) {
        spdlog::error("Usage: {} <config>", argv[0]);
        return EXIT_FAILURE;
    }

    // Load config
    auto config = Config::get_instance(argv[1]);

    // Load sql_connection
    auto con = ep::db::SQLConnection::load(
        config->accounts_db_config.host, config->accounts_db_config.user,
        config->accounts_db_config.password, config->accounts_db_config.db_name);

    if (!con) {
        spdlog::error("SQLConnection::Load(): Failed");
        return EXIT_FAILURE;
    }
    spdlog::info("SQLConnection::Load(): Success");

    db::LoginDataBase login_db(std::move(con.value()));

    std::string login("user");
    std::string password("1234567");

    if (!login_db.insert(login, password)) {
        spdlog::error("LoginDataBase::Insert(): Failed");
        return EXIT_FAILURE;
    }
    spdlog::info("LoginDataBase::Insert(): Success");

    auto data = login_db.get(login);
    if (!data) {
        spdlog::error("LoginDataBase::Get(): Failed");
        return EXIT_FAILURE;
    }
    spdlog::info("LoginDataBase::Get(): Success");

    if (!login_db.remove(login)) {
        spdlog::error("LoginDataBase::Remove(): Failed");
        return EXIT_FAILURE;
    }
    spdlog::info("LoginDataBase::Remove(): Success");

    return EXIT_SUCCESS;
}

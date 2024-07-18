#include <iostream>
#include <mysql/jdbc.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

struct Config {
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_schema;
    std::string mg_path = ".";
    std::string mg_table = "__migrations";
};

void print_help() {
    std::cout << "Usage: mygrator <mysql_host> <mysql_user> <mysql_pass> <mysql_schema> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "ABOUT" << std::endl;
    std::cout <<
            "MyGrator is a very basic database migrations tool for MySQL. It looks for SQL files in a folder and executes them one by one and saves the history in the database. If a migration fails, the tool will stop running and report the migration that has failed."
            << std::endl;
    std::cout << std::endl;
    std::cout << "OPTIONS" << std::endl;
    std::cout << "-p        The migration folder. If not specified the current working directory will be used." <<
            std::endl;
    std::cout <<
            "-t        The migration table name. If not specified the default will be used which is '__migrations'." <<
            std::endl;
}

bool parse_args(int argc, char **argv, Config &config) {
    if (argc == 2 && ((std::string) argv[1] == "-h" || (std::string) argv[1] == "--help")) {
        print_help();
        return false;
    }

    if (argc < 5) {
        std::cerr << "Not enough arguments provided" << std::endl;
        return false;
    }

    config.db_host = argv[1];
    config.db_user = argv[2];
    config.db_pass = argv[3];
    config.db_schema = argv[4];

    for (int i = 5; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-p") {
            config.mg_path = argv[i + 1];
            i++;
        }

        if (arg == "-t") {
            config.mg_table = argv[i + 1];
            i++;
        }
    }

    return true;
}

sql::Connection *connect(const std::string &host, const std::string &user, const std::string &pass, const std::string &schema) {
    sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
    const char* host_cstr = host.c_str();
    const char* user_cstr = user.c_str();
    const char* pass_cstr = pass.c_str();
    const char* schema_cstr = schema.c_str();

    sql::ConnectOptionsMap options;
    options["hostName"] = host_cstr;
    options["userName"] = user_cstr;
    options["password"] = pass_cstr;
    options["schema"] = schema_cstr;
    options["CLIENT_MULTI_STATEMENTS"] = true;

    sql::Connection *conn = driver->connect(options);
    conn->setSchema(schema);

    return conn;
}

void close(sql::Connection *conn) {
    conn->close();
    delete conn;
}

bool init_migrations(Config &config) {
    try {
        std::string sql_create = "CREATE TABLE IF NOT EXISTS " + config.mg_table + " ("
                                 "id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,"
                                 "name VARCHAR(255) NOT NULL"
                                 ");";

        sql::Connection *conn = connect(config.db_host, config.db_user, config.db_pass, config.db_schema);
        sql::Statement *stmt = conn->createStatement();
        stmt->execute(sql_create);
        stmt->close();
        conn->close();

        delete stmt;
        delete conn;
    } catch (sql::SQLException &e) {
        std::cerr << e.what() << std::endl;
        return false;
    }

    return true;
}

bool migration_exist(Config &config, const std::string &mg_name) {
    bool exists = false;

    try {
        sql::Connection *conn = connect(config.db_host, config.db_user, config.db_pass, config.db_schema);
        sql::Statement *stmt = conn->createStatement();
        sql::ResultSet *res = stmt->
                executeQuery("SELECT * FROM " + config.mg_table + " WHERE name = '" + mg_name + "'");

        if (res->next()) {
            exists = true;
        }

        res->close();
        stmt->close();
        conn->close();

        delete res;
        delete stmt;
        delete conn;
    } catch (sql::SQLException &e) {
        std::cerr << e.what() << std::endl;
        return true;
    }

    return exists;
}

bool run_migration(Config &config, const std::string &name, const fs::path &path) {
    bool success = true;
    std::ifstream file(path);
    std::stringstream buffer;

    if (file.is_open()) {
        buffer << file.rdbuf();
        file.close();
    } else {
        std::cerr << "Failed to read file " << path << std::endl;
        return false;
    }

    std::cout << "Running migration " << name << std::endl;

    try {
        sql::Connection *conn = connect(config.db_host, config.db_user, config.db_pass, config.db_schema);
        sql::Statement *stmt = conn->createStatement();
        stmt->execute(buffer.str());

        stmt->close();
        conn->close();

        delete stmt;
        delete conn;
    } catch (sql::SQLException &e) {
        std::cerr << e.what() << std::endl;
        success = false;
    }

    return success;
}

bool add_migration(Config &config, const std::string &name) {
    bool success = true;

    try {
        sql::Connection *conn = connect(config.db_host, config.db_user, config.db_pass, config.db_schema);
        sql::Statement *stmt = conn->createStatement();
        stmt->execute("INSERT INTO " + config.mg_table + " (name) VALUES ('" + name + "');");

        stmt->close();
        conn->close();

        delete stmt;
        delete conn;
    } catch (sql::SQLException &e) {
        std::cerr << e.what() << std::endl;
        success = false;
    }

    return success;
}

bool run_migrations(Config &config) {
    fs::directory_iterator dir_it(config.mg_path);
    std::set<fs::path> files;

    if (!dir_it->exists()) {
        std::cerr << "Migration folder not found";
        return false;
    }

    for (const fs::directory_entry &dir_entry: dir_it) {
        files.insert(dir_entry.path());
    }

    for (const fs::path &path: files) {
        std::string ext = path.extension().string();
        std::string filename = path.filename().string();
        std::string name = filename.substr(0, filename.find_last_of('.'));

        if (ext != ".sql") {
            continue;
        }

        if (migration_exist(config, name)) {
            continue;
        }

        if (!run_migration(config, name, path)) {
            return false;
        }

        if (!add_migration(config, name)) {
            return false;
        }
    }

    return true;
}

bool run(int argc, char **argv) {
    Config config;

    if (!parse_args(argc, argv, config)) {
        return false;
    }

    try {
        if (!init_migrations(config)) {
            return false;
        }

        if (!run_migrations(config)) {
            return false;
        }
    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    try {
        bool result = run(argc, argv);

        if (result) {
            return 0;
        } else {
            return 1;
        }
    } catch (std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 2;
    }
}

#include <iostream>
#include <mysql/jdbc.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

struct Config {
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_scheme;
    std::string mg_path = ".";
};

void print_help() {

}

bool parse_args(int argc, char **argv, Config &config) {
    if (argc == 2 && ((std::string) argv[1] == "-h" || (std::string) argv[1] == "--help")) {
        print_help();
        return false;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        if (arg == "--host") {
            config.db_host = argv[i + 1];
            i++;
        }

        if (arg == "--user") {
            config.db_user = argv[i + 1];
            i++;
        }

        if (arg == "--pass") {
            config.db_pass = argv[i + 1];
            i++;
        }

        if (arg == "--scheme") {
            config.db_scheme = argv[i + 1];
            i++;
        }

        if (arg == "--path") {
            config.mg_path = argv[i + 1];
            i++;
        }
    }

    if (config.db_host.empty() || config.db_user.empty() || config.db_pass.empty() || config.db_scheme.empty()) {
        std::cerr << "Missing required options" << std::endl;
        return false;
    }

    return true;
}

sql::Connection *connect(std::string &host, std::string &user, std::string &pass, std::string &scheme) {
    sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
    sql::Connection *conn = driver->connect("tcp://" + host + ":3306", user, pass);
    conn->setSchema(scheme);

    return conn;
}

void close(sql::Connection *conn) {
    conn->close();
    delete conn;
}

bool execute(sql::Connection *conn, std::string sql) {
    sql::Statement *stmt = conn->createStatement();
    bool result = false;

    try {
        stmt->execute("START TRANSACTION");
        stmt->execute(sql);
        stmt->execute("COMMIT");

        result = true;
    } catch (sql::SQLException &ex) {
        stmt->execute("ROLLBACK");
        std::cerr << ex.what() << std::endl;
    }

    delete stmt;
    return result;
}

sql::ResultSet *query(sql::Connection *conn, std::string sql) {
    sql::Statement *stmt = conn->createStatement();
    sql::ResultSet *res = nullptr;

    try {
        res = stmt->executeQuery(sql);
    } catch (sql::SQLException &ex) {
        std::cerr << ex.what() << std::endl;
    }

    delete stmt;
    return res;
}

bool init_migrations(sql::Connection *conn) {
    std::string sql_create = "CREATE TABLE IF NOT EXISTS __migrations ("
                             "id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,"
                             "name VARCHAR(255) NOT NULL"
                             ")";

    if (!execute(conn, sql_create)) {
        return false;
    }

    return true;
}

bool migration_exist(sql::Connection* conn, std::string mg_name) {
    sql::ResultSet* res = query(conn, "select * from __migrations WHERE name = '" + mg_name + "'");

    if (res == nullptr) {
        return false;
    }

    if (res->rowsCount() > 0) {
        return true;
    }

    return false;
}

bool run_migration(sql::Connection* conn, std::string name, std::string path) {
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

    if (!execute(conn, buffer.str())) {
        return false;
    }

    if (!execute(conn, "INSERT INTO __migrations (name) VALUES ('" + name + "')")) {
        std::cerr << "Failed to add migration " << name << " to history" << std::endl;
        return false;
    }

    return true;
}

bool run_migrations(sql::Connection *conn, std::string folder) {
    fs::directory_iterator files(folder);

    if (!files->exists()) {
        std::cerr << "Migration folder not found";
        return false;
    }

    for (fs::directory_entry entry : files) {
        fs::path path = entry.path();
        std::string ext = path.extension().string();
        std::string filename = path.filename().string();
        std::string name = filename.substr(0, filename.find_last_of("."));

        if (entry.is_directory() || ext != ".sql") {
            continue;
        }

        if (migration_exist(conn, name)) {
            continue;
        }

        if (!run_migration(conn, name, path)) {
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
        auto conn = connect(config.db_host, config.db_user, config.db_pass, config.db_scheme);

        if (!init_migrations(conn)) {
            return false;
        }

        if (!run_migrations(conn, config.mg_path)) {
            return false;
        }

        close(conn);
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
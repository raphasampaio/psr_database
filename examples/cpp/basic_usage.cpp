#include <psr_database/psr_database.h>

#include <iostream>

int main() {
    try {
        // Open an in-memory database
        psr::Database db(":memory:");
        std::cout << "Database opened successfully\n";

        // Create a table
        db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)");
        std::cout << "Table created\n";

        // Insert some data
        db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");
        db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')");
        std::cout << "Inserted " << db.changes() << " rows\n";
        std::cout << "Last insert rowid: " << db.last_insert_rowid() << "\n";

        // Query data
        auto result = db.execute("SELECT * FROM users ORDER BY name");
        std::cout << "\nUsers:\n";
        std::cout << "------\n";

        for (const auto& row : result) {
            auto id = row.get_int(0);
            auto name = row.get_string(1);
            auto email = row.get_string(2);

            std::cout << "ID: " << (id ? *id : 0) << ", Name: " << (name ? *name : "")
                      << ", Email: " << (email ? *email : "") << "\n";
        }

        // Transaction example
        db.begin_transaction();
        db.execute("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')");
        db.commit();
        std::cout << "\nTransaction committed\n";

        // Parameterized query
        auto search = db.execute("SELECT * FROM users WHERE name = ?", {psr::Value{"Alice"}});
        std::cout << "\nSearch result: " << search.row_count() << " rows\n";

        db.close();
        std::cout << "Database closed\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

#!/usr/bin/env julia
# Example usage of the PSR Database Julia binding

using PsrDatabase

function main()
    # Open an in-memory database
    db = Database(":memory:")
    println("Database opened successfully")

    # Create a table
    execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)")
    println("Table created")

    # Insert some data
    execute(db, "INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")
    execute(db, "INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")
    println("Inserted rows, last rowid: $(last_insert_rowid(db))")

    # Query data
    result = execute(db, "SELECT * FROM users ORDER BY name")
    println("\nUsers ($(row_count(result)) rows):")
    println("-" ^ 40)

    for row in result
        println("ID: $(row["id"]), Name: $(row["name"]), Email: $(row["email"])")
    end

    # Transaction example
    begin_transaction(db)
    execute(db, "INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')")
    commit(db)
    println("\nTransaction committed")

    # Count rows
    result = execute(db, "SELECT COUNT(*) as count FROM users")
    for row in result
        println("Total users: $(row["count"])")
    end

    close!(db)
    println("Database closed")
end

main()

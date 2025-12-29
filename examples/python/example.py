#!/usr/bin/env python3
"""Example usage of the PSR Database Python binding."""

from psr_database import Database

def main():
    # Open an in-memory database
    db = Database(":memory:")
    print("Database opened successfully")

    # Create a table
    db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)")
    print("Table created")

    # Insert some data
    db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")
    db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")
    print(f"Inserted rows, last rowid: {db.last_insert_rowid}")

    # Query data
    result = db.execute("SELECT * FROM users ORDER BY name")
    print(f"\nUsers ({len(result)} rows):")
    print("-" * 40)

    for row in result:
        print(f"ID: {row[0]}, Name: {row[1]}, Email: {row[2]}")

    # Using with statement for automatic cleanup
    with Database(":memory:") as db2:
        db2.execute("CREATE TABLE test (value INTEGER)")
        db2.execute("INSERT INTO test VALUES (42)")
        result = db2.execute("SELECT * FROM test")
        print(f"\nTest value: {result[0][0]}")

    # Parameterized query
    db.execute("INSERT INTO users (name, email) VALUES (?, ?)", ["Charlie", "charlie@example.com"])

    # Transaction example
    db.begin_transaction()
    db.execute("INSERT INTO users (name, email) VALUES ('Dave', 'dave@example.com')")
    db.commit()
    print("\nTransaction committed")

    # Count rows
    result = db.execute("SELECT COUNT(*) FROM users")
    print(f"Total users: {result[0][0]}")

    db.close()
    print("Database closed")


if __name__ == "__main__":
    main()

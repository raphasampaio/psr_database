#!/usr/bin/env lua
-- Example usage of the PSR Database Lua binding

local psr = require("psr_database")

-- Open an in-memory database
local db, err = psr.open(":memory:")
if not db then
    print("Error opening database: " .. err)
    os.exit(1)
end
print("Database opened successfully")

-- Create a table
local result, err = db:execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)")
if not result then
    print("Error creating table: " .. err)
    os.exit(1)
end
print("Table created")

-- Insert some data
db:execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")
db:execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")
print("Inserted rows, last rowid: " .. db:last_insert_rowid())

-- Query data
result = db:execute("SELECT * FROM users ORDER BY name")
print("\nUsers (" .. result:row_count() .. " rows):")
print(string.rep("-", 40))

for i = 1, result:row_count() do
    local row = result:get_row(i)
    print(string.format("ID: %d, Name: %s, Email: %s", row.id, row.name, row.email))
end

-- Transaction example
local ok, err = db:begin_transaction()
if ok then
    db:execute("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')")
    db:commit()
    print("\nTransaction committed")
else
    print("Error beginning transaction: " .. err)
end

-- Count rows
result = db:execute("SELECT COUNT(*) as count FROM users")
local row = result:get_row(1)
print("Total users: " .. row.count)

-- Access by index
print("\nAccessing by index:")
result = db:execute("SELECT * FROM users LIMIT 1")
local first_row = result[1]
print("First user: " .. first_row.name)

db:close()
print("\nDatabase closed")

print("\nVersion: " .. psr.version)

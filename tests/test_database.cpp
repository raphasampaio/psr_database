#include <gtest/gtest.h>

#include <psr_database/database.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = (fs::temp_directory_path() / "psr_test.db").string();
    }

    void TearDown() override {
        if (fs::exists(test_db_path_)) {
            fs::remove(test_db_path_);
        }
    }

    std::string test_db_path_;
};

TEST_F(DatabaseTest, OpenAndClose) {
    psr::Database db(test_db_path_);
    EXPECT_TRUE(db.is_open());
    EXPECT_EQ(db.path(), test_db_path_);

    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseTest, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseTest, CreateTable) {
    psr::Database db(test_db_path_);

    auto result = db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.row_count(), 0u);
}

TEST_F(DatabaseTest, InsertAndSelect) {
    psr::Database db(test_db_path_);

    db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

    db.execute("INSERT INTO users (name, age) VALUES ('Alice', 30)");
    EXPECT_EQ(db.last_insert_rowid(), 1);
    EXPECT_EQ(db.changes(), 1);

    db.execute("INSERT INTO users (name, age) VALUES ('Bob', 25)");
    EXPECT_EQ(db.last_insert_rowid(), 2);

    auto result = db.execute("SELECT * FROM users ORDER BY id");
    EXPECT_EQ(result.row_count(), 2u);
    EXPECT_EQ(result.column_count(), 3u);

    EXPECT_EQ(result.columns()[0], "id");
    EXPECT_EQ(result.columns()[1], "name");
    EXPECT_EQ(result.columns()[2], "age");

    EXPECT_EQ(result[0].get_int(0), 1);
    EXPECT_EQ(result[0].get_string(1), "Alice");
    EXPECT_EQ(result[0].get_int(2), 30);

    EXPECT_EQ(result[1].get_int(0), 2);
    EXPECT_EQ(result[1].get_string(1), "Bob");
    EXPECT_EQ(result[1].get_int(2), 25);
}

TEST_F(DatabaseTest, ParameterizedQuery) {
    psr::Database db(":memory:");

    db.execute("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL)");

    db.execute("INSERT INTO items (name, price) VALUES (?, ?)",
               {psr::Value{"Widget"}, psr::Value{19.99}});

    auto result = db.execute("SELECT * FROM items WHERE name = ?", {psr::Value{"Widget"}});

    EXPECT_EQ(result.row_count(), 1u);
    EXPECT_EQ(result[0].get_string(1), "Widget");
    EXPECT_DOUBLE_EQ(*result[0].get_double(2), 19.99);
}

TEST_F(DatabaseTest, NullValues) {
    psr::Database db(":memory:");

    db.execute("CREATE TABLE nullable (id INTEGER PRIMARY KEY, value TEXT)");
    db.execute("INSERT INTO nullable (value) VALUES (NULL)");

    auto result = db.execute("SELECT * FROM nullable");
    EXPECT_EQ(result.row_count(), 1u);
    EXPECT_TRUE(result[0].is_null(1));
    EXPECT_FALSE(result[0].get_string(1).has_value());
}

TEST_F(DatabaseTest, Transaction) {
    psr::Database db(":memory:");

    db.execute("CREATE TABLE counter (value INTEGER)");
    db.execute("INSERT INTO counter (value) VALUES (0)");

    db.begin_transaction();
    db.execute("UPDATE counter SET value = 1");

    auto result1 = db.execute("SELECT value FROM counter");
    EXPECT_EQ(result1[0].get_int(0), 1);

    db.rollback();

    auto result2 = db.execute("SELECT value FROM counter");
    EXPECT_EQ(result2[0].get_int(0), 0);
}

TEST_F(DatabaseTest, TransactionCommit) {
    psr::Database db(":memory:");

    db.execute("CREATE TABLE counter (value INTEGER)");
    db.execute("INSERT INTO counter (value) VALUES (0)");

    db.begin_transaction();
    db.execute("UPDATE counter SET value = 42");
    db.commit();

    auto result = db.execute("SELECT value FROM counter");
    EXPECT_EQ(result[0].get_int(0), 42);
}

TEST_F(DatabaseTest, BlobData) {
    psr::Database db(":memory:");

    db.execute("CREATE TABLE blobs (id INTEGER PRIMARY KEY, data BLOB)");

    std::vector<uint8_t> blob_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    db.execute("INSERT INTO blobs (data) VALUES (?)", {psr::Value{blob_data}});

    auto result = db.execute("SELECT data FROM blobs");
    EXPECT_EQ(result.row_count(), 1u);

    auto retrieved = result[0].get_blob(0);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(*retrieved, blob_data);
}

TEST_F(DatabaseTest, MoveSemantics) {
    psr::Database db1(":memory:");
    db1.execute("CREATE TABLE test (id INTEGER)");

    psr::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_open());

    // db1 is in moved-from state, but should not crash
    db2.execute("INSERT INTO test (id) VALUES (1)");
    auto result = db2.execute("SELECT * FROM test");
    EXPECT_EQ(result.row_count(), 1u);
}

TEST_F(DatabaseTest, InvalidSqlThrows) {
    psr::Database db(":memory:");
    EXPECT_THROW(db.execute("INVALID SQL STATEMENT"), std::runtime_error);
}

TEST_F(DatabaseTest, ResultIteration) {
    psr::Database db(":memory:");

    db.execute("CREATE TABLE numbers (n INTEGER)");
    for (int i = 1; i <= 5; ++i) {
        db.execute("INSERT INTO numbers (n) VALUES (?)", {psr::Value{static_cast<int64_t>(i)}});
    }

    auto result = db.execute("SELECT n FROM numbers ORDER BY n");

    int expected = 1;
    for (const auto& row : result) {
        EXPECT_EQ(row.get_int(0), expected);
        ++expected;
    }
    EXPECT_EQ(expected, 6);
}

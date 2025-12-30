#include <psr/c/result.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace fs = std::filesystem;

class CApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = (fs::temp_directory_path() / "psr_c_test.db").string();
    }

    void TearDown() override {
        if (fs::exists(test_db_path_)) {
            fs::remove(test_db_path_);
        }
    }

    std::string test_db_path_;
};

TEST_F(CApiTest, OpenAndClose) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(test_db_path_.c_str(), &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);
    EXPECT_EQ(psr_database_is_open(db), 1);

    psr_database_close(db);
}

TEST_F(CApiTest, OpenInMemory) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);

    psr_database_close(db);
}

TEST_F(CApiTest, OpenNullPath) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(nullptr, &error);

    EXPECT_EQ(db, nullptr);
    EXPECT_EQ(error, PSR_ERROR_INVALID_ARGUMENT);
}

TEST_F(CApiTest, ExecuteQuery) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);
    ASSERT_NE(db, nullptr);

    psr_result_t* result =
        psr_database_execute(db, "CREATE TABLE test (id INTEGER PRIMARY KEY)", &error);
    EXPECT_EQ(error, PSR_OK);
    EXPECT_NE(result, nullptr);

    psr_result_free(result);
    psr_database_close(db);
}

TEST_F(CApiTest, InsertAndSelect) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);
    ASSERT_NE(db, nullptr);

    psr_result_t* r1 = psr_database_execute(
        db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)", &error);
    psr_result_free(r1);

    psr_result_t* r2 =
        psr_database_execute(db, "INSERT INTO users (name, age) VALUES ('Alice', 30)", &error);
    psr_result_free(r2);

    EXPECT_EQ(psr_database_last_insert_rowid(db), 1);
    EXPECT_EQ(psr_database_changes(db), 1);

    psr_result_t* result = psr_database_execute(db, "SELECT * FROM users", &error);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(psr_result_row_count(result), 1u);
    EXPECT_EQ(psr_result_column_count(result), 3u);

    EXPECT_STREQ(psr_result_column_name(result, 0), "id");
    EXPECT_STREQ(psr_result_column_name(result, 1), "name");
    EXPECT_STREQ(psr_result_column_name(result, 2), "age");

    int64_t id;
    EXPECT_EQ(psr_result_get_int(result, 0, 0, &id), PSR_OK);
    EXPECT_EQ(id, 1);

    const char* name = psr_result_get_string(result, 0, 1);
    EXPECT_STREQ(name, "Alice");

    int64_t age;
    EXPECT_EQ(psr_result_get_int(result, 0, 2, &age), PSR_OK);
    EXPECT_EQ(age, 30);

    psr_result_free(result);
    psr_database_close(db);
}

TEST_F(CApiTest, ValueTypes) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);
    ASSERT_NE(db, nullptr);

    psr_result_t* r1 = psr_database_execute(
        db, "CREATE TABLE types (i INTEGER, f REAL, t TEXT, n INTEGER)", &error);
    psr_result_free(r1);

    psr_result_t* r2 =
        psr_database_execute(db, "INSERT INTO types VALUES (42, 3.14, 'hello', NULL)", &error);
    psr_result_free(r2);

    psr_result_t* result = psr_database_execute(db, "SELECT * FROM types", &error);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(psr_result_get_type(result, 0, 0), PSR_TYPE_INTEGER);
    EXPECT_EQ(psr_result_get_type(result, 0, 1), PSR_TYPE_FLOAT);
    EXPECT_EQ(psr_result_get_type(result, 0, 2), PSR_TYPE_TEXT);
    EXPECT_EQ(psr_result_get_type(result, 0, 3), PSR_TYPE_NULL);

    EXPECT_EQ(psr_result_is_null(result, 0, 0), 0);
    EXPECT_EQ(psr_result_is_null(result, 0, 3), 1);

    int64_t i;
    EXPECT_EQ(psr_result_get_int(result, 0, 0, &i), PSR_OK);
    EXPECT_EQ(i, 42);

    double f;
    EXPECT_EQ(psr_result_get_double(result, 0, 1, &f), PSR_OK);
    EXPECT_DOUBLE_EQ(f, 3.14);

    psr_result_free(result);
    psr_database_close(db);
}

TEST_F(CApiTest, Transaction) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);
    ASSERT_NE(db, nullptr);

    psr_result_t* r1 = psr_database_execute(db, "CREATE TABLE counter (value INTEGER)", &error);
    psr_result_free(r1);

    psr_result_t* r2 = psr_database_execute(db, "INSERT INTO counter VALUES (0)", &error);
    psr_result_free(r2);

    EXPECT_EQ(psr_database_begin_transaction(db), PSR_OK);

    psr_result_t* r3 = psr_database_execute(db, "UPDATE counter SET value = 1", &error);
    psr_result_free(r3);

    EXPECT_EQ(psr_database_rollback(db), PSR_OK);

    psr_result_t* result = psr_database_execute(db, "SELECT value FROM counter", &error);
    ASSERT_NE(result, nullptr);

    int64_t value;
    EXPECT_EQ(psr_result_get_int(result, 0, 0, &value), PSR_OK);
    EXPECT_EQ(value, 0);

    psr_result_free(result);
    psr_database_close(db);
}

TEST_F(CApiTest, ErrorHandling) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);
    ASSERT_NE(db, nullptr);

    psr_result_t* result = psr_database_execute(db, "INVALID SQL", &error);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(error, PSR_ERROR_QUERY);

    const char* msg = psr_database_error_message(db);
    EXPECT_NE(msg, nullptr);
    EXPECT_NE(strlen(msg), 0u);

    psr_database_close(db);
}

TEST_F(CApiTest, ErrorStrings) {
    EXPECT_STREQ(psr_error_string(PSR_OK), "Success");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_INVALID_ARGUMENT), "Invalid argument");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_DATABASE), "Database error");
    EXPECT_STREQ(psr_error_string(PSR_ERROR_QUERY), "Query error");
}

TEST_F(CApiTest, Version) {
    const char* version = psr_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(CApiTest, IndexOutOfRange) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(":memory:", &error);
    ASSERT_NE(db, nullptr);

    psr_result_t* r1 = psr_database_execute(db, "CREATE TABLE test (id INTEGER)", &error);
    psr_result_free(r1);

    psr_result_t* r2 = psr_database_execute(db, "INSERT INTO test VALUES (1)", &error);
    psr_result_free(r2);

    psr_result_t* result = psr_database_execute(db, "SELECT * FROM test", &error);
    ASSERT_NE(result, nullptr);

    int64_t value;
    EXPECT_EQ(psr_result_get_int(result, 100, 0, &value), PSR_ERROR_INDEX_OUT_OF_RANGE);
    EXPECT_EQ(psr_result_get_int(result, 0, 100, &value), PSR_ERROR_INDEX_OUT_OF_RANGE);

    EXPECT_EQ(psr_result_column_name(result, 100), nullptr);

    psr_result_free(result);
    psr_database_close(db);
}

// Migration C API tests
class CApiMigrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = (fs::temp_directory_path() / "psr_c_migration_test.db").string();
        test_schema_path_ = fs::temp_directory_path() / "psr_c_test_schema";

        // Clean up from previous runs
        if (fs::exists(test_db_path_)) {
            fs::remove(test_db_path_);
        }
        if (fs::exists(test_schema_path_)) {
            fs::remove_all(test_schema_path_);
        }

        // Create schema directory
        fs::create_directories(test_schema_path_);
    }

    void TearDown() override {
        if (fs::exists(test_db_path_)) {
            fs::remove(test_db_path_);
        }
        if (fs::exists(test_schema_path_)) {
            fs::remove_all(test_schema_path_);
        }
    }

    void create_migration(int version, const std::string& sql) {
        auto dir = test_schema_path_ / std::to_string(version);
        fs::create_directories(dir);
        std::ofstream(dir / "up.sql") << sql;
    }

    std::string test_db_path_;
    fs::path test_schema_path_;
};

TEST_F(CApiMigrationTest, FromSchemaBasic) {
    create_migration(1, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT);");
    create_migration(2, "CREATE TABLE posts (id INTEGER PRIMARY KEY, title TEXT);");

    psr_error_t error;
    psr_database_t* db = psr_database_from_schema(
        test_db_path_.c_str(), test_schema_path_.string().c_str(), &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);
    EXPECT_EQ(psr_database_current_version(db), 2);

    // Verify tables exist
    psr_result_t* result = psr_database_execute(
        db, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name", &error);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(psr_result_row_count(result), 2u);

    psr_result_free(result);
    psr_database_close(db);
}

TEST_F(CApiMigrationTest, FromSchemaEmpty) {
    psr_error_t error;
    psr_database_t* db = psr_database_from_schema(
        test_db_path_.c_str(), test_schema_path_.string().c_str(), &error);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(error, PSR_OK);
    EXPECT_EQ(psr_database_current_version(db), 0);

    psr_database_close(db);
}

TEST_F(CApiMigrationTest, FromSchemaNullArgs) {
    psr_error_t error;

    psr_database_t* db1 = psr_database_from_schema(nullptr, test_schema_path_.string().c_str(), &error);
    EXPECT_EQ(db1, nullptr);
    EXPECT_EQ(error, PSR_ERROR_INVALID_ARGUMENT);

    psr_database_t* db2 = psr_database_from_schema(test_db_path_.c_str(), nullptr, &error);
    EXPECT_EQ(db2, nullptr);
    EXPECT_EQ(error, PSR_ERROR_INVALID_ARGUMENT);
}

TEST_F(CApiMigrationTest, CurrentVersionAndSetVersion) {
    psr_error_t error;
    psr_database_t* db = psr_database_open(test_db_path_.c_str(), &error);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(psr_database_current_version(db), 0);

    EXPECT_EQ(psr_database_set_version(db, 5), PSR_OK);
    EXPECT_EQ(psr_database_current_version(db), 5);

    EXPECT_EQ(psr_database_set_version(db, 10), PSR_OK);
    EXPECT_EQ(psr_database_current_version(db), 10);

    psr_database_close(db);
}

TEST_F(CApiMigrationTest, MigrationErrorString) {
    EXPECT_STREQ(psr_error_string(PSR_ERROR_MIGRATION), "Migration error");
}

TEST_F(CApiMigrationTest, FromSchemaInvalidPath) {
    psr_error_t error;
    psr_database_t* db = psr_database_from_schema(
        test_db_path_.c_str(), "/nonexistent/path", &error);

    EXPECT_EQ(db, nullptr);
    EXPECT_EQ(error, PSR_ERROR_MIGRATION);
}

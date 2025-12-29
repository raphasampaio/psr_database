"""Tests for the Database class."""

import os
import tempfile
import pytest

from psr_database import Database


class TestDatabaseOpen:
    """Tests for database opening and closing."""

    def test_open_in_memory(self):
        """Test opening an in-memory database."""
        db = Database(":memory:")
        assert db.is_open()
        db.close()

    def test_open_file(self):
        """Test opening a file database."""
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
            path = f.name

        try:
            db = Database(path)
            assert db.is_open()
            assert db.path == path
            db.close()
        finally:
            if os.path.exists(path):
                os.remove(path)

    def test_close(self):
        """Test closing a database."""
        db = Database(":memory:")
        assert db.is_open()
        db.close()
        assert not db.is_open()

    def test_context_manager(self):
        """Test using database as context manager."""
        with Database(":memory:") as db:
            assert db.is_open()
            db.execute("CREATE TABLE test (id INTEGER)")
        assert not db.is_open()


class TestDatabaseExecute:
    """Tests for query execution."""

    def test_create_table(self):
        """Test creating a table."""
        db = Database(":memory:")
        result = db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
        assert result.empty()
        db.close()

    def test_insert(self):
        """Test inserting data."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
        db.execute("INSERT INTO users (name) VALUES ('Alice')")
        assert db.last_insert_rowid == 1
        assert db.changes == 1
        db.close()

    def test_select(self):
        """Test selecting data."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
        db.execute("INSERT INTO users (name) VALUES ('Alice')")
        db.execute("INSERT INTO users (name) VALUES ('Bob')")

        result = db.execute("SELECT * FROM users ORDER BY id")
        assert result.row_count() == 2
        assert result.column_count() == 2
        assert result.columns() == ["id", "name"]
        db.close()

    def test_parameterized_query(self):
        """Test parameterized queries."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
        db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 30])
        db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Bob", 25])

        result = db.execute("SELECT * FROM users WHERE age > ?", [26])
        assert result.row_count() == 1
        assert result[0][1] == "Alice"
        db.close()

    def test_invalid_sql(self):
        """Test that invalid SQL raises an exception."""
        db = Database(":memory:")
        with pytest.raises(RuntimeError):
            db.execute("INVALID SQL STATEMENT")
        db.close()


class TestDatabaseTransaction:
    """Tests for transaction support."""

    def test_commit(self):
        """Test committing a transaction."""
        db = Database(":memory:")
        db.execute("CREATE TABLE counter (value INTEGER)")
        db.execute("INSERT INTO counter VALUES (0)")

        db.begin_transaction()
        db.execute("UPDATE counter SET value = 42")
        db.commit()

        result = db.execute("SELECT value FROM counter")
        assert result[0][0] == 42
        db.close()

    def test_rollback(self):
        """Test rolling back a transaction."""
        db = Database(":memory:")
        db.execute("CREATE TABLE counter (value INTEGER)")
        db.execute("INSERT INTO counter VALUES (0)")

        db.begin_transaction()
        db.execute("UPDATE counter SET value = 42")
        db.rollback()

        result = db.execute("SELECT value FROM counter")
        assert result[0][0] == 0
        db.close()


class TestDatabaseDataTypes:
    """Tests for different data types."""

    def test_integer(self):
        """Test integer values."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value INTEGER)")
        db.execute("INSERT INTO data VALUES (?)", [42])

        result = db.execute("SELECT value FROM data")
        assert result[0][0] == 42
        db.close()

    def test_float(self):
        """Test float values."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value REAL)")
        db.execute("INSERT INTO data VALUES (?)", [3.14159])

        result = db.execute("SELECT value FROM data")
        assert abs(result[0][0] - 3.14159) < 0.0001
        db.close()

    def test_string(self):
        """Test string values."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES (?)", ["Hello, World!"])

        result = db.execute("SELECT value FROM data")
        assert result[0][0] == "Hello, World!"
        db.close()

    def test_bytes(self):
        """Test blob/bytes values."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value BLOB)")
        blob = bytes([0xDE, 0xAD, 0xBE, 0xEF])
        db.execute("INSERT INTO data VALUES (?)", [blob])

        result = db.execute("SELECT value FROM data")
        assert result[0][0] == blob
        db.close()

    def test_null(self):
        """Test NULL values."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES (NULL)")

        result = db.execute("SELECT value FROM data")
        assert result[0][0] is None
        db.close()

    def test_null_parameter(self):
        """Test NULL as parameter."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES (?)", [None])

        result = db.execute("SELECT value FROM data")
        assert result[0][0] is None
        db.close()

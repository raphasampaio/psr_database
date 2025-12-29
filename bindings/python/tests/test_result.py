"""Tests for the Result and Row classes."""

import pytest

from psr_database import Database


class TestResult:
    """Tests for the Result class."""

    def test_empty_result(self):
        """Test an empty result."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER)")
        result = db.execute("SELECT * FROM users")

        assert result.empty()
        assert result.row_count() == 0
        assert not result  # __bool__
        assert len(result) == 0
        db.close()

    def test_non_empty_result(self):
        """Test a non-empty result."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER, name TEXT)")
        db.execute("INSERT INTO users VALUES (1, 'Alice')")
        db.execute("INSERT INTO users VALUES (2, 'Bob')")
        result = db.execute("SELECT * FROM users")

        assert not result.empty()
        assert result.row_count() == 2
        assert result  # __bool__
        assert len(result) == 2
        db.close()

    def test_columns(self):
        """Test getting column names."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER, name TEXT, email TEXT)")
        db.execute("INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')")
        result = db.execute("SELECT * FROM users")

        assert result.column_count() == 3
        assert result.columns() == ["id", "name", "email"]
        db.close()

    def test_iteration(self):
        """Test iterating over results."""
        db = Database(":memory:")
        db.execute("CREATE TABLE numbers (n INTEGER)")
        for i in range(1, 6):
            db.execute("INSERT INTO numbers VALUES (?)", [i])

        result = db.execute("SELECT n FROM numbers ORDER BY n")

        values = [row[0] for row in result]
        assert values == [1, 2, 3, 4, 5]
        db.close()

    def test_index_access(self):
        """Test accessing rows by index."""
        db = Database(":memory:")
        db.execute("CREATE TABLE users (id INTEGER, name TEXT)")
        db.execute("INSERT INTO users VALUES (1, 'Alice')")
        db.execute("INSERT INTO users VALUES (2, 'Bob')")
        result = db.execute("SELECT * FROM users ORDER BY id")

        assert result[0][0] == 1
        assert result[0][1] == "Alice"
        assert result[1][0] == 2
        assert result[1][1] == "Bob"
        db.close()


class TestRow:
    """Tests for the Row class."""

    def test_column_count(self):
        """Test getting column count from row."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (a INTEGER, b TEXT, c REAL)")
        db.execute("INSERT INTO data VALUES (1, 'test', 3.14)")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row.column_count() == 3
        db.close()

    def test_get_int(self):
        """Test getting integer value."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value INTEGER)")
        db.execute("INSERT INTO data VALUES (42)")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row.get_int(0) == 42
        db.close()

    def test_get_float(self):
        """Test getting float value."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value REAL)")
        db.execute("INSERT INTO data VALUES (3.14)")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert abs(row.get_float(0) - 3.14) < 0.01
        db.close()

    def test_get_string(self):
        """Test getting string value."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES ('hello')")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row.get_string(0) == "hello"
        db.close()

    def test_get_bytes(self):
        """Test getting blob value."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value BLOB)")
        blob = bytes([1, 2, 3, 4])
        db.execute("INSERT INTO data VALUES (?)", [blob])
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row.get_bytes(0) == blob
        db.close()

    def test_is_null(self):
        """Test checking for NULL."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES (NULL)")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row.is_null(0)
        assert row[0] is None
        db.close()

    def test_is_not_null(self):
        """Test checking non-NULL value."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES ('test')")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert not row.is_null(0)
        db.close()

    def test_index_access_auto_type(self):
        """Test index access with automatic type conversion."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (i INTEGER, f REAL, t TEXT)")
        db.execute("INSERT INTO data VALUES (42, 3.14, 'hello')")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row[0] == 42
        assert abs(row[1] - 3.14) < 0.01
        assert row[2] == "hello"
        db.close()

    def test_wrong_type_returns_none(self):
        """Test that getting wrong type returns None."""
        db = Database(":memory:")
        db.execute("CREATE TABLE data (value TEXT)")
        db.execute("INSERT INTO data VALUES ('hello')")
        result = db.execute("SELECT * FROM data")

        row = result[0]
        assert row.get_int(0) is None  # string, not int
        assert row.get_string(0) == "hello"
        db.close()

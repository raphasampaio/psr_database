import 'package:test/test.dart';
import 'package:psr_database/psr_database.dart';
import 'package:psr_database/src/bindings.dart';

void main() {
  setUpAll(() {
    // Initialize bindings - adjust path as needed for test environment
    initializeBindings();
  });

  group('Result', () {
    test('empty result', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER)').dispose();

      final result = db.execute('SELECT * FROM users');
      expect(result.isEmpty, isTrue);
      expect(result.isNotEmpty, isFalse);
      expect(result.rowCount, equals(0));

      result.dispose();
      db.close();
    });

    test('non-empty result', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER, name TEXT)').dispose();
      db.execute("INSERT INTO users VALUES (1, 'Alice')").dispose();
      db.execute("INSERT INTO users VALUES (2, 'Bob')").dispose();

      final result = db.execute('SELECT * FROM users');
      expect(result.isEmpty, isFalse);
      expect(result.isNotEmpty, isTrue);
      expect(result.rowCount, equals(2));

      result.dispose();
      db.close();
    });

    test('gets columns', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER, name TEXT, email TEXT)').dispose();
      db.execute("INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')").dispose();

      final result = db.execute('SELECT * FROM users');
      expect(result.columnCount, equals(3));
      expect(result.columns, equals(['id', 'name', 'email']));

      result.dispose();
      db.close();
    });

    test('gets row as map', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER, name TEXT)').dispose();
      db.execute("INSERT INTO users VALUES (1, 'Alice')").dispose();

      final result = db.execute('SELECT * FROM users');
      final row = result.getRow(0);

      expect(row['id'], equals(1));
      expect(row['name'], equals('Alice'));

      result.dispose();
      db.close();
    });

    test('converts to list', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE numbers (n INTEGER)').dispose();
      for (var i = 1; i <= 3; i++) {
        db.execute('INSERT INTO numbers VALUES ($i)').dispose();
      }

      final result = db.execute('SELECT n FROM numbers ORDER BY n');
      final list = result.toList();

      expect(list.length, equals(3));
      expect(list[0]['n'], equals(1));
      expect(list[1]['n'], equals(2));
      expect(list[2]['n'], equals(3));

      result.dispose();
      db.close();
    });

    test('gets value types', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (i INTEGER, f REAL, t TEXT, n INTEGER)').dispose();
      db.execute("INSERT INTO data VALUES (42, 3.14, 'hello', NULL)").dispose();

      final result = db.execute('SELECT * FROM data');

      expect(result.getType(0, 0), equals(PSR_TYPE_INTEGER));
      expect(result.getType(0, 1), equals(PSR_TYPE_FLOAT));
      expect(result.getType(0, 2), equals(PSR_TYPE_TEXT));
      expect(result.getType(0, 3), equals(PSR_TYPE_NULL));

      result.dispose();
      db.close();
    });
  });

  group('Value Access', () {
    test('gets integer value', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value INTEGER)').dispose();
      db.execute('INSERT INTO data VALUES (42)').dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.getInt(0, 0), equals(42));

      result.dispose();
      db.close();
    });

    test('gets double value', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value REAL)').dispose();
      db.execute('INSERT INTO data VALUES (3.14)').dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.getDouble(0, 0), closeTo(3.14, 0.01));

      result.dispose();
      db.close();
    });

    test('gets string value', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value TEXT)').dispose();
      db.execute("INSERT INTO data VALUES ('hello')").dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.getString(0, 0), equals('hello'));

      result.dispose();
      db.close();
    });

    test('checks null value', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value TEXT)').dispose();
      db.execute('INSERT INTO data VALUES (NULL)').dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.isNull(0, 0), isTrue);
      expect(result.getString(0, 0), isNull);

      result.dispose();
      db.close();
    });

    test('checks non-null value', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value TEXT)').dispose();
      db.execute("INSERT INTO data VALUES ('test')").dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.isNull(0, 0), isFalse);

      result.dispose();
      db.close();
    });

    test('gets multiple values from row', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (i INTEGER, f REAL, t TEXT)').dispose();
      db.execute("INSERT INTO data VALUES (42, 3.14, 'hello')").dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.getInt(0, 0), equals(42));
      expect(result.getDouble(0, 1), closeTo(3.14, 0.01));
      expect(result.getString(0, 2), equals('hello'));

      result.dispose();
      db.close();
    });

    test('handles multiple rows', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER, name TEXT)').dispose();
      db.execute("INSERT INTO users VALUES (1, 'Alice')").dispose();
      db.execute("INSERT INTO users VALUES (2, 'Bob')").dispose();

      final result = db.execute('SELECT * FROM users ORDER BY id');
      expect(result.getInt(0, 0), equals(1));
      expect(result.getString(0, 1), equals('Alice'));
      expect(result.getInt(1, 0), equals(2));
      expect(result.getString(1, 1), equals('Bob'));

      result.dispose();
      db.close();
    });
  });

  group('Disposal', () {
    test('disposes result', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value INTEGER)').dispose();
      db.execute('INSERT INTO data VALUES (1)').dispose();

      final result = db.execute('SELECT * FROM data');
      expect(result.rowCount, equals(1));
      result.dispose();

      // Accessing disposed result should throw
      expect(() => result.rowCount, throwsStateError);
      db.close();
    });
  });
}

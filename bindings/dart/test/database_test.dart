import 'dart:io';
import 'package:test/test.dart';
import 'package:psr_database/psr_database.dart';
import 'package:psr_database/src/bindings.dart';

void main() {
  setUpAll(() {
    // Initialize bindings - adjust path as needed for test environment
    initializeBindings();
  });

  group('Database Open/Close', () {
    test('opens in-memory database', () {
      final db = Database.open(':memory:');
      expect(db.isOpen, isTrue);
      db.close();
    });

    test('closes database', () {
      final db = Database.open(':memory:');
      expect(db.isOpen, isTrue);
      db.close();
      expect(db.isOpen, isFalse);
    });

    test('opens file database', () {
      final path = '${Directory.systemTemp.path}/test_${DateTime.now().millisecondsSinceEpoch}.db';
      try {
        final db = Database.open(path);
        expect(db.isOpen, isTrue);
        expect(db.path, equals(path));
        db.close();
      } finally {
        final file = File(path);
        if (file.existsSync()) {
          file.deleteSync();
        }
      }
    });
  });

  group('Query Execution', () {
    test('creates table', () {
      final db = Database.open(':memory:');
      final result = db.execute('CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)');
      expect(result.isEmpty, isTrue);
      result.dispose();
      db.close();
    });

    test('inserts data', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)').dispose();
      db.execute("INSERT INTO users (name) VALUES ('Alice')").dispose();

      expect(db.lastInsertRowid, equals(1));
      expect(db.changes, equals(1));
      db.close();
    });

    test('selects data', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)').dispose();
      db.execute("INSERT INTO users (name) VALUES ('Alice')").dispose();
      db.execute("INSERT INTO users (name) VALUES ('Bob')").dispose();

      final result = db.execute('SELECT * FROM users ORDER BY id');
      expect(result.rowCount, equals(2));
      expect(result.columnCount, equals(2));
      expect(result.columns, equals(['id', 'name']));

      result.dispose();
      db.close();
    });

    test('throws on invalid SQL', () {
      final db = Database.open(':memory:');
      expect(() => db.execute('INVALID SQL STATEMENT'), throwsA(isA<QueryException>()));
      db.close();
    });
  });

  group('Transactions', () {
    test('commits transaction', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE counter (value INTEGER)').dispose();
      db.execute('INSERT INTO counter VALUES (0)').dispose();

      db.beginTransaction();
      db.execute('UPDATE counter SET value = 42').dispose();
      db.commit();

      final result = db.execute('SELECT value FROM counter');
      expect(result.getInt(0, 0), equals(42));

      result.dispose();
      db.close();
    });

    test('rolls back transaction', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE counter (value INTEGER)').dispose();
      db.execute('INSERT INTO counter VALUES (0)').dispose();

      db.beginTransaction();
      db.execute('UPDATE counter SET value = 42').dispose();
      db.rollback();

      final result = db.execute('SELECT value FROM counter');
      expect(result.getInt(0, 0), equals(0));

      result.dispose();
      db.close();
    });

    test('transaction helper commits on success', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE counter (value INTEGER)').dispose();
      db.execute('INSERT INTO counter VALUES (0)').dispose();

      db.transaction(() {
        db.execute('UPDATE counter SET value = 42').dispose();
      });

      final result = db.execute('SELECT value FROM counter');
      expect(result.getInt(0, 0), equals(42));

      result.dispose();
      db.close();
    });

    test('transaction helper rolls back on error', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE counter (value INTEGER)').dispose();
      db.execute('INSERT INTO counter VALUES (0)').dispose();

      expect(() {
        db.transaction(() {
          db.execute('UPDATE counter SET value = 42').dispose();
          throw Exception('Test error');
        });
      }, throwsException);

      final result = db.execute('SELECT value FROM counter');
      expect(result.getInt(0, 0), equals(0));

      result.dispose();
      db.close();
    });
  });

  group('Data Types', () {
    test('handles integers', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value INTEGER)').dispose();
      db.execute('INSERT INTO data VALUES (42)').dispose();

      final result = db.execute('SELECT value FROM data');
      expect(result.getInt(0, 0), equals(42));

      result.dispose();
      db.close();
    });

    test('handles floats', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value REAL)').dispose();
      db.execute('INSERT INTO data VALUES (3.14159)').dispose();

      final result = db.execute('SELECT value FROM data');
      expect(result.getDouble(0, 0), closeTo(3.14159, 0.0001));

      result.dispose();
      db.close();
    });

    test('handles strings', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value TEXT)').dispose();
      db.execute("INSERT INTO data VALUES ('Hello, World!')").dispose();

      final result = db.execute('SELECT value FROM data');
      expect(result.getString(0, 0), equals('Hello, World!'));

      result.dispose();
      db.close();
    });

    test('handles NULL', () {
      final db = Database.open(':memory:');
      db.execute('CREATE TABLE data (value TEXT)').dispose();
      db.execute('INSERT INTO data VALUES (NULL)').dispose();

      final result = db.execute('SELECT value FROM data');
      expect(result.isNull(0, 0), isTrue);
      expect(result.getString(0, 0), isNull);

      result.dispose();
      db.close();
    });
  });

  group('Version', () {
    test('returns version string', () {
      expect(version, isA<String>());
      expect(version, isNotEmpty);
    });
  });
}

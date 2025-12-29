// Example usage of the PSR Database Dart binding

import 'package:psr_database/psr_database.dart';
import 'package:psr_database/src/bindings.dart';

void main() {
  // Initialize bindings (specify library path if needed)
  initializeBindings();

  print('PSR Database version: $version');

  // Open an in-memory database
  final db = Database.open(':memory:');
  print('Database opened successfully');

  try {
    // Create a table
    db.execute('CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)');
    print('Table created');

    // Insert some data
    db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");
    db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')");
    print('Inserted rows, last rowid: ${db.lastInsertRowid}');

    // Query data
    final result = db.execute('SELECT * FROM users ORDER BY name');
    print('\nUsers (${result.rowCount} rows):');
    print('-' * 40);

    for (final row in result.toList()) {
      print('ID: ${row['id']}, Name: ${row['name']}, Email: ${row['email']}');
    }
    result.dispose();

    // Transaction example
    db.transaction(() {
      db.execute("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')");
    });
    print('\nTransaction committed');

    // Count rows
    final countResult = db.execute('SELECT COUNT(*) as count FROM users');
    print('Total users: ${countResult.getInt(0, 0)}');
    countResult.dispose();

    // Access individual values
    final selectResult = db.execute('SELECT * FROM users WHERE name = "Alice"');
    if (selectResult.isNotEmpty) {
      print('\nAlice\'s email: ${selectResult.getString(0, 2)}');
    }
    selectResult.dispose();

  } catch (e) {
    print('Error: $e');
  } finally {
    db.close();
    print('\nDatabase closed');
  }
}

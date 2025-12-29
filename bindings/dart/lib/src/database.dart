import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'bindings.dart';
import 'exceptions.dart';
import 'result.dart';

/// A SQLite database connection.
class Database {
  DatabasePtr _ptr;
  bool _disposed = false;
  final String path;

  Database._(this._ptr, this.path);

  /// Opens a database at the given path.
  ///
  /// Use `:memory:` for an in-memory database.
  factory Database.open(String path) {
    final pathPtr = path.toNativeUtf8();
    final errorPtr = calloc<Int32>();

    try {
      final ptr = bindings.openDatabase(pathPtr, errorPtr);
      final error = errorPtr.value;

      if (ptr == nullptr || error != PSR_OK) {
        throw DatabaseException(
          'Failed to open database: ${bindings.errorString(error).toDartString()}',
          error,
        );
      }

      return Database._(ptr, path);
    } finally {
      calloc.free(pathPtr);
      calloc.free(errorPtr);
    }
  }

  void _checkDisposed() {
    if (_disposed) {
      throw StateError('Database has been closed');
    }
  }

  /// Returns true if the database is open.
  bool get isOpen {
    if (_disposed) return false;
    return bindings.isOpen(_ptr) != 0;
  }

  /// Closes the database connection.
  void close() {
    if (!_disposed) {
      bindings.closeDatabase(_ptr);
      _disposed = true;
    }
  }

  /// Executes a SQL statement and returns the result.
  Result execute(String sql) {
    _checkDisposed();

    final sqlPtr = sql.toNativeUtf8();
    final errorPtr = calloc<Int32>();

    try {
      final resultPtr = bindings.execute(_ptr, sqlPtr, errorPtr);
      final error = errorPtr.value;

      if (resultPtr == nullptr && error != PSR_OK) {
        final msgPtr = bindings.errorMessage(_ptr);
        final message = msgPtr != nullptr ? msgPtr.toDartString() : 'Unknown error';
        throw QueryException(message, sql);
      }

      return Result.fromPointer(resultPtr);
    } finally {
      calloc.free(sqlPtr);
      calloc.free(errorPtr);
    }
  }

  /// Gets the rowid of the last inserted row.
  int get lastInsertRowid {
    _checkDisposed();
    return bindings.lastInsertRowid(_ptr);
  }

  /// Gets the number of rows changed by the last statement.
  int get changes {
    _checkDisposed();
    return bindings.changes(_ptr);
  }

  /// Begins a transaction.
  void beginTransaction() {
    _checkDisposed();
    final result = bindings.beginTransaction(_ptr);
    if (result != PSR_OK) {
      throw DatabaseException('Failed to begin transaction', result);
    }
  }

  /// Commits the current transaction.
  void commit() {
    _checkDisposed();
    final result = bindings.commit(_ptr);
    if (result != PSR_OK) {
      throw DatabaseException('Failed to commit transaction', result);
    }
  }

  /// Rolls back the current transaction.
  void rollback() {
    _checkDisposed();
    final result = bindings.rollback(_ptr);
    if (result != PSR_OK) {
      throw DatabaseException('Failed to rollback transaction', result);
    }
  }

  /// Gets the last error message.
  String get errorMessage {
    _checkDisposed();
    final msgPtr = bindings.errorMessage(_ptr);
    if (msgPtr == nullptr) return '';
    return msgPtr.toDartString();
  }

  /// Executes a function within a transaction.
  ///
  /// If the function throws, the transaction is rolled back.
  /// Otherwise, the transaction is committed.
  T transaction<T>(T Function() action) {
    beginTransaction();
    try {
      final result = action();
      commit();
      return result;
    } catch (e) {
      rollback();
      rethrow;
    }
  }
}

/// Gets the library version.
String get version {
  return bindings.version().toDartString();
}

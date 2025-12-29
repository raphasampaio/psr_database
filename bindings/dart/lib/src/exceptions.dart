/// Exception thrown when a database operation fails.
class DatabaseException implements Exception {
  final String message;
  final int errorCode;

  DatabaseException(this.message, [this.errorCode = 0]);

  @override
  String toString() => 'DatabaseException: $message (code: $errorCode)';
}

/// Exception thrown when a query fails.
class QueryException implements Exception {
  final String message;
  final String? sql;

  QueryException(this.message, [this.sql]);

  @override
  String toString() {
    if (sql != null) {
      return 'QueryException: $message\nSQL: $sql';
    }
    return 'QueryException: $message';
  }
}

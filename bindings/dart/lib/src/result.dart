import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'bindings.dart';

/// Represents a query result with rows and columns.
class Result {
  final ResultPtr _ptr;
  bool _disposed = false;

  Result._(this._ptr);

  /// Creates a Result from a native pointer.
  factory Result.fromPointer(ResultPtr ptr) {
    final result = Result._(ptr);
    return result;
  }

  void _checkDisposed() {
    if (_disposed) {
      throw StateError('Result has been disposed');
    }
  }

  /// Gets the number of rows in the result.
  int get rowCount {
    _checkDisposed();
    return bindings.resultRowCount(_ptr);
  }

  /// Gets the number of columns in the result.
  int get columnCount {
    _checkDisposed();
    return bindings.resultColumnCount(_ptr);
  }

  /// Returns true if the result is empty.
  bool get isEmpty => rowCount == 0;

  /// Returns true if the result is not empty.
  bool get isNotEmpty => rowCount > 0;

  /// Gets the column names.
  List<String> get columns {
    _checkDisposed();
    final count = columnCount;
    final result = <String>[];
    for (var i = 0; i < count; i++) {
      final namePtr = bindings.resultColumnName(_ptr, i);
      if (namePtr != nullptr) {
        result.add(namePtr.toDartString());
      }
    }
    return result;
  }

  /// Checks if a value is null.
  bool isNull(int row, int col) {
    _checkDisposed();
    return bindings.resultIsNull(_ptr, row, col) != 0;
  }

  /// Gets the type of a value.
  int getType(int row, int col) {
    _checkDisposed();
    return bindings.resultGetType(_ptr, row, col);
  }

  /// Gets an integer value.
  int? getInt(int row, int col) {
    _checkDisposed();
    if (isNull(row, col)) return null;

    final valuePtr = calloc<Int64>();
    try {
      final result = bindings.resultGetInt(_ptr, row, col, valuePtr);
      if (result == PSR_OK) {
        return valuePtr.value;
      }
      return null;
    } finally {
      calloc.free(valuePtr);
    }
  }

  /// Gets a double value.
  double? getDouble(int row, int col) {
    _checkDisposed();
    if (isNull(row, col)) return null;

    final valuePtr = calloc<Double>();
    try {
      final result = bindings.resultGetDouble(_ptr, row, col, valuePtr);
      if (result == PSR_OK) {
        return valuePtr.value;
      }
      return null;
    } finally {
      calloc.free(valuePtr);
    }
  }

  /// Gets a string value.
  String? getString(int row, int col) {
    _checkDisposed();
    if (isNull(row, col)) return null;

    final strPtr = bindings.resultGetString(_ptr, row, col);
    if (strPtr == nullptr) return null;
    return strPtr.toDartString();
  }

  /// Gets a blob value.
  Uint8List? getBlob(int row, int col) {
    _checkDisposed();
    if (isNull(row, col)) return null;

    final sizePtr = calloc<Size>();
    try {
      final dataPtr = bindings.resultGetBlob(_ptr, row, col, sizePtr);
      if (dataPtr == nullptr) return null;

      final size = sizePtr.value;
      if (size == 0) return Uint8List(0);

      return Uint8List.fromList(dataPtr.asTypedList(size));
    } finally {
      calloc.free(sizePtr);
    }
  }

  /// Gets a row as a map of column names to values.
  Map<String, dynamic> getRow(int row) {
    _checkDisposed();
    final cols = columns;
    final result = <String, dynamic>{};

    for (var col = 0; col < cols.length; col++) {
      final type = getType(row, col);
      switch (type) {
        case PSR_TYPE_NULL:
          result[cols[col]] = null;
          break;
        case PSR_TYPE_INTEGER:
          result[cols[col]] = getInt(row, col);
          break;
        case PSR_TYPE_FLOAT:
          result[cols[col]] = getDouble(row, col);
          break;
        case PSR_TYPE_TEXT:
          result[cols[col]] = getString(row, col);
          break;
        case PSR_TYPE_BLOB:
          result[cols[col]] = getBlob(row, col);
          break;
      }
    }
    return result;
  }

  /// Gets all rows as a list of maps.
  List<Map<String, dynamic>> toList() {
    _checkDisposed();
    final result = <Map<String, dynamic>>[];
    for (var i = 0; i < rowCount; i++) {
      result.add(getRow(i));
    }
    return result;
  }

  /// Disposes the result and frees native memory.
  void dispose() {
    if (!_disposed) {
      bindings.resultFree(_ptr);
      _disposed = true;
    }
  }
}

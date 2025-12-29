import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

// Error codes
const int PSR_OK = 0;
const int PSR_ERROR_INVALID_ARGUMENT = -1;
const int PSR_ERROR_DATABASE = -2;
const int PSR_ERROR_QUERY = -3;
const int PSR_ERROR_NO_MEMORY = -4;
const int PSR_ERROR_NOT_OPEN = -5;
const int PSR_ERROR_INDEX_OUT_OF_RANGE = -6;

// Value types
const int PSR_TYPE_NULL = 0;
const int PSR_TYPE_INTEGER = 1;
const int PSR_TYPE_FLOAT = 2;
const int PSR_TYPE_TEXT = 3;
const int PSR_TYPE_BLOB = 4;

// Opaque pointer types
typedef DatabasePtr = Pointer<Void>;
typedef ResultPtr = Pointer<Void>;

// Native function typedefs
typedef NativeOpenDb = DatabasePtr Function(
    Pointer<Utf8> path, Pointer<Int32> error);
typedef DartOpenDb = DatabasePtr Function(
    Pointer<Utf8> path, Pointer<Int32> error);

typedef NativeCloseDb = Void Function(DatabasePtr db);
typedef DartCloseDb = void Function(DatabasePtr db);

typedef NativeIsOpen = Int32 Function(DatabasePtr db);
typedef DartIsOpen = int Function(DatabasePtr db);

typedef NativeExecute = ResultPtr Function(
    DatabasePtr db, Pointer<Utf8> sql, Pointer<Int32> error);
typedef DartExecute = ResultPtr Function(
    DatabasePtr db, Pointer<Utf8> sql, Pointer<Int32> error);

typedef NativeLastInsertRowid = Int64 Function(DatabasePtr db);
typedef DartLastInsertRowid = int Function(DatabasePtr db);

typedef NativeChanges = Int32 Function(DatabasePtr db);
typedef DartChanges = int Function(DatabasePtr db);

typedef NativeTransaction = Int32 Function(DatabasePtr db);
typedef DartTransaction = int Function(DatabasePtr db);

typedef NativeErrorMessage = Pointer<Utf8> Function(DatabasePtr db);
typedef DartErrorMessage = Pointer<Utf8> Function(DatabasePtr db);

typedef NativeResultFree = Void Function(ResultPtr result);
typedef DartResultFree = void Function(ResultPtr result);

typedef NativeResultRowCount = Size Function(ResultPtr result);
typedef DartResultRowCount = int Function(ResultPtr result);

typedef NativeResultColumnCount = Size Function(ResultPtr result);
typedef DartResultColumnCount = int Function(ResultPtr result);

typedef NativeResultColumnName = Pointer<Utf8> Function(
    ResultPtr result, Size col);
typedef DartResultColumnName = Pointer<Utf8> Function(ResultPtr result, int col);

typedef NativeResultGetType = Int32 Function(
    ResultPtr result, Size row, Size col);
typedef DartResultGetType = int Function(ResultPtr result, int row, int col);

typedef NativeResultIsNull = Int32 Function(ResultPtr result, Size row, Size col);
typedef DartResultIsNull = int Function(ResultPtr result, int row, int col);

typedef NativeResultGetInt = Int32 Function(
    ResultPtr result, Size row, Size col, Pointer<Int64> value);
typedef DartResultGetInt = int Function(
    ResultPtr result, int row, int col, Pointer<Int64> value);

typedef NativeResultGetDouble = Int32 Function(
    ResultPtr result, Size row, Size col, Pointer<Double> value);
typedef DartResultGetDouble = int Function(
    ResultPtr result, int row, int col, Pointer<Double> value);

typedef NativeResultGetString = Pointer<Utf8> Function(
    ResultPtr result, Size row, Size col);
typedef DartResultGetString = Pointer<Utf8> Function(
    ResultPtr result, int row, int col);

typedef NativeResultGetBlob = Pointer<Uint8> Function(
    ResultPtr result, Size row, Size col, Pointer<Size> size);
typedef DartResultGetBlob = Pointer<Uint8> Function(
    ResultPtr result, int row, int col, Pointer<Size> size);

typedef NativeErrorString = Pointer<Utf8> Function(Int32 error);
typedef DartErrorString = Pointer<Utf8> Function(int error);

typedef NativeVersion = Pointer<Utf8> Function();
typedef DartVersion = Pointer<Utf8> Function();

/// Bindings to the native psr_database_c library.
class PsrDatabaseBindings {
  late final DynamicLibrary _library;

  late final DartOpenDb openDatabase;
  late final DartCloseDb closeDatabase;
  late final DartIsOpen isOpen;
  late final DartExecute execute;
  late final DartLastInsertRowid lastInsertRowid;
  late final DartChanges changes;
  late final DartTransaction beginTransaction;
  late final DartTransaction commit;
  late final DartTransaction rollback;
  late final DartErrorMessage errorMessage;

  late final DartResultFree resultFree;
  late final DartResultRowCount resultRowCount;
  late final DartResultColumnCount resultColumnCount;
  late final DartResultColumnName resultColumnName;
  late final DartResultGetType resultGetType;
  late final DartResultIsNull resultIsNull;
  late final DartResultGetInt resultGetInt;
  late final DartResultGetDouble resultGetDouble;
  late final DartResultGetString resultGetString;
  late final DartResultGetBlob resultGetBlob;

  late final DartErrorString errorString;
  late final DartVersion version;

  PsrDatabaseBindings({String? libraryPath}) {
    _library = _loadLibrary(libraryPath);
    _bindFunctions();
  }

  DynamicLibrary _loadLibrary(String? path) {
    if (path != null) {
      return DynamicLibrary.open(path);
    }

    if (Platform.isWindows) {
      return DynamicLibrary.open('psr_database_c.dll');
    } else if (Platform.isLinux) {
      return DynamicLibrary.open('libpsr_database_c.so');
    } else if (Platform.isMacOS) {
      return DynamicLibrary.open('libpsr_database_c.dylib');
    }
    throw UnsupportedError('Platform not supported');
  }

  void _bindFunctions() {
    openDatabase = _library
        .lookup<NativeFunction<NativeOpenDb>>('psr_database_open')
        .asFunction();

    closeDatabase = _library
        .lookup<NativeFunction<NativeCloseDb>>('psr_database_close')
        .asFunction();

    isOpen = _library
        .lookup<NativeFunction<NativeIsOpen>>('psr_database_is_open')
        .asFunction();

    execute = _library
        .lookup<NativeFunction<NativeExecute>>('psr_database_execute')
        .asFunction();

    lastInsertRowid = _library
        .lookup<NativeFunction<NativeLastInsertRowid>>(
            'psr_database_last_insert_rowid')
        .asFunction();

    changes = _library
        .lookup<NativeFunction<NativeChanges>>('psr_database_changes')
        .asFunction();

    beginTransaction = _library
        .lookup<NativeFunction<NativeTransaction>>(
            'psr_database_begin_transaction')
        .asFunction();

    commit = _library
        .lookup<NativeFunction<NativeTransaction>>('psr_database_commit')
        .asFunction();

    rollback = _library
        .lookup<NativeFunction<NativeTransaction>>('psr_database_rollback')
        .asFunction();

    errorMessage = _library
        .lookup<NativeFunction<NativeErrorMessage>>('psr_database_error_message')
        .asFunction();

    resultFree = _library
        .lookup<NativeFunction<NativeResultFree>>('psr_result_free')
        .asFunction();

    resultRowCount = _library
        .lookup<NativeFunction<NativeResultRowCount>>('psr_result_row_count')
        .asFunction();

    resultColumnCount = _library
        .lookup<NativeFunction<NativeResultColumnCount>>(
            'psr_result_column_count')
        .asFunction();

    resultColumnName = _library
        .lookup<NativeFunction<NativeResultColumnName>>('psr_result_column_name')
        .asFunction();

    resultGetType = _library
        .lookup<NativeFunction<NativeResultGetType>>('psr_result_get_type')
        .asFunction();

    resultIsNull = _library
        .lookup<NativeFunction<NativeResultIsNull>>('psr_result_is_null')
        .asFunction();

    resultGetInt = _library
        .lookup<NativeFunction<NativeResultGetInt>>('psr_result_get_int')
        .asFunction();

    resultGetDouble = _library
        .lookup<NativeFunction<NativeResultGetDouble>>('psr_result_get_double')
        .asFunction();

    resultGetString = _library
        .lookup<NativeFunction<NativeResultGetString>>('psr_result_get_string')
        .asFunction();

    resultGetBlob = _library
        .lookup<NativeFunction<NativeResultGetBlob>>('psr_result_get_blob')
        .asFunction();

    errorString = _library
        .lookup<NativeFunction<NativeErrorString>>('psr_error_string')
        .asFunction();

    version = _library
        .lookup<NativeFunction<NativeVersion>>('psr_version')
        .asFunction();
  }
}

/// Global bindings instance.
late final PsrDatabaseBindings bindings;

/// Initialize the bindings with an optional library path.
void initializeBindings({String? libraryPath}) {
  bindings = PsrDatabaseBindings(libraryPath: libraryPath);
}

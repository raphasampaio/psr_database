"""PSR Database - A SQLite wrapper library with multi-language bindings."""

import ctypes
import os
import sys

_pkg_dir = os.path.dirname(__file__)

# Preload the shared library before importing the extension module
# This ensures the library is found regardless of system library paths
if sys.platform == "win32":
    # On Windows, add the package directory to DLL search path
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(_pkg_dir)
    _lib_path = os.path.join(_pkg_dir, "libpsr_database.dll")
    if os.path.exists(_lib_path):
        ctypes.CDLL(_lib_path)
elif sys.platform == "darwin":
    _lib_path = os.path.join(_pkg_dir, "libpsr_database.dylib")
    if os.path.exists(_lib_path):
        ctypes.CDLL(_lib_path)
else:
    # Linux and other Unix-like systems
    _lib_path = os.path.join(_pkg_dir, "libpsr_database.so")
    if os.path.exists(_lib_path):
        ctypes.CDLL(_lib_path)

from ._psr_database import Database, Result, Row, __version__

__all__ = ["Database", "Result", "Row", "__version__"]

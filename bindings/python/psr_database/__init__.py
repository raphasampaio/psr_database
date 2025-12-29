"""PSR Database - A SQLite wrapper library with multi-language bindings."""

import os
import sys

# On Windows, add the package directory to DLL search path
if sys.platform == "win32":
    _pkg_dir = os.path.dirname(__file__)
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(_pkg_dir)

from ._psr_database import Database, Result, Row, __version__

__all__ = ["Database", "Result", "Row", "__version__"]

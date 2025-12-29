#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <psr_database/database.h>
#include <psr_database/result.h>

namespace py = pybind11;

PYBIND11_MODULE(_psr_database, m) {
    m.doc() = "PSR Database - SQLite wrapper library";

    // Value type (using Python's native types via automatic conversion)
    // pybind11 will handle the variant conversion automatically

    // Row class
    py::class_<psr::Row>(m, "Row")
        .def("column_count", &psr::Row::column_count)
        .def("is_null", &psr::Row::is_null)
        .def(
            "get_int",
            [](const psr::Row& row, size_t index) -> py::object {
                auto val = row.get_int(index);
                if (val)
                    return py::int_(*val);
                return py::none();
            },
            py::arg("index"))
        .def(
            "get_float",
            [](const psr::Row& row, size_t index) -> py::object {
                auto val = row.get_double(index);
                if (val)
                    return py::float_(*val);
                return py::none();
            },
            py::arg("index"))
        .def(
            "get_string",
            [](const psr::Row& row, size_t index) -> py::object {
                auto val = row.get_string(index);
                if (val)
                    return py::str(*val);
                return py::none();
            },
            py::arg("index"))
        .def(
            "get_bytes",
            [](const psr::Row& row, size_t index) -> py::object {
                auto val = row.get_blob(index);
                if (val)
                    return py::bytes(reinterpret_cast<const char*>(val->data()), val->size());
                return py::none();
            },
            py::arg("index"))
        .def(
            "__getitem__",
            [](const psr::Row& row, size_t index) -> py::object {
                if (row.is_null(index)) {
                    return py::none();
                }
                const auto& value = row[index];
                if (std::holds_alternative<int64_t>(value)) {
                    return py::int_(std::get<int64_t>(value));
                } else if (std::holds_alternative<double>(value)) {
                    return py::float_(std::get<double>(value));
                } else if (std::holds_alternative<std::string>(value)) {
                    return py::str(std::get<std::string>(value));
                } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
                    const auto& blob = std::get<std::vector<uint8_t>>(value);
                    return py::bytes(reinterpret_cast<const char*>(blob.data()), blob.size());
                }
                return py::none();
            },
            py::arg("index"));

    // Result class
    py::class_<psr::Result>(m, "Result")
        .def("empty", &psr::Result::empty)
        .def("row_count", &psr::Result::row_count)
        .def("column_count", &psr::Result::column_count)
        .def("columns", &psr::Result::columns)
        .def(
            "__getitem__", [](const psr::Result& result, size_t index) { return result[index]; },
            py::arg("index"))
        .def("__len__", &psr::Result::row_count)
        .def(
            "__iter__",
            [](const psr::Result& result) {
                return py::make_iterator(result.begin(), result.end());
            },
            py::keep_alive<0, 1>())
        .def("__bool__", [](const psr::Result& result) { return !result.empty(); });

    // Database class
    py::class_<psr::Database>(m, "Database")
        .def(py::init<const std::string&>(), py::arg("path"))
        .def("is_open", &psr::Database::is_open)
        .def("close", &psr::Database::close)
        .def(
            "execute",
            [](psr::Database& db, const std::string& sql) { return db.execute(sql); },
            py::arg("sql"))
        .def(
            "execute",
            [](psr::Database& db, const std::string& sql, const py::list& params) {
                std::vector<psr::Value> values;
                for (const auto& param : params) {
                    if (param.is_none()) {
                        values.emplace_back(nullptr);
                    } else if (py::isinstance<py::int_>(param)) {
                        values.emplace_back(param.cast<int64_t>());
                    } else if (py::isinstance<py::float_>(param)) {
                        values.emplace_back(param.cast<double>());
                    } else if (py::isinstance<py::str>(param)) {
                        values.emplace_back(param.cast<std::string>());
                    } else if (py::isinstance<py::bytes>(param)) {
                        std::string bytes = param.cast<std::string>();
                        values.emplace_back(
                            std::vector<uint8_t>(bytes.begin(), bytes.end()));
                    } else {
                        throw std::runtime_error("Unsupported parameter type");
                    }
                }
                return db.execute(sql, values);
            },
            py::arg("sql"), py::arg("params"))
        .def("last_insert_rowid", &psr::Database::last_insert_rowid)
        .def("changes", &psr::Database::changes)
        .def("begin_transaction", &psr::Database::begin_transaction)
        .def("commit", &psr::Database::commit)
        .def("rollback", &psr::Database::rollback)
        .def_property_readonly("path", &psr::Database::path)
        .def("error_message", &psr::Database::error_message)
        .def("__enter__", [](psr::Database& db) -> psr::Database& { return db; })
        .def(
            "__exit__",
            [](psr::Database& db, py::object, py::object, py::object) { db.close(); });

    // Module level version
    m.attr("__version__") = "1.0.0";
}

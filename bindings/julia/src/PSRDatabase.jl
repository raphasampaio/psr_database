module PSRDatabase

using Dates
using DataFrames

export Database, DatabaseException
export create_empty_db_from_schema, open_database, close!
export create_element!, get_element_id
export execute, transaction

# Find the library path
const LIBNAME = Sys.iswindows() ? "libpsr_database_c" : "libpsr_database_c"

function find_library()
    # Search paths for the DLL
    search_paths = [
        # Build directory (development)
        joinpath(@__DIR__, "..", "..", "..", "build", "bin"),
        joinpath(@__DIR__, "..", "..", "..", "build", "lib"),
        # Installed location
        joinpath(@__DIR__, "..", "lib"),
        joinpath(@__DIR__, "..", "bin"),
    ]

    for path in search_paths
        libpath = joinpath(path, LIBNAME * (Sys.iswindows() ? ".dll" : ".so"))
        if isfile(libpath)
            return libpath
        end
    end

    # Fall back to system path
    return LIBNAME
end

const LIBPATH = Ref{String}()

function __init__()
    LIBPATH[] = find_library()
end

# Error codes from C API
const PSR_OK = Cint(0)
const PSR_ERROR_INVALID_ARGUMENT = Cint(-1)
const PSR_ERROR_DATABASE = Cint(-2)
const PSR_ERROR_QUERY = Cint(-3)
const PSR_ERROR_NO_MEMORY = Cint(-4)
const PSR_ERROR_NOT_OPEN = Cint(-5)
const PSR_ERROR_INDEX_OUT_OF_RANGE = Cint(-6)
const PSR_ERROR_MIGRATION = Cint(-7)

# Log levels
const PSR_LOG_DEBUG = Cint(0)
const PSR_LOG_INFO = Cint(1)
const PSR_LOG_WARN = Cint(2)
const PSR_LOG_ERROR = Cint(3)
const PSR_LOG_OFF = Cint(4)

# Value types
const PSR_TYPE_NULL = Cint(0)
const PSR_TYPE_INTEGER = Cint(1)
const PSR_TYPE_FLOAT = Cint(2)
const PSR_TYPE_TEXT = Cint(3)
const PSR_TYPE_BLOB = Cint(4)

# Custom exception
struct DatabaseException <: Exception
    message::String
end

Base.showerror(io::IO, e::DatabaseException) = print(io, "DatabaseException: ", e.message)

function check_error(error_code::Cint, db::Ptr{Cvoid}=C_NULL)
    if error_code != PSR_OK
        if db != C_NULL
            msg_ptr = ccall((:psr_database_error_message, LIBPATH[]), Cstring, (Ptr{Cvoid},), db)
            if msg_ptr != C_NULL
                msg = unsafe_string(msg_ptr)
                if !isempty(msg)
                    throw(DatabaseException(msg))
                end
            end
        end
        throw(DatabaseException("Error code: $error_code"))
    end
end

# Database handle wrapper
mutable struct Database
    handle::Ptr{Cvoid}
    path::String

    function Database(handle::Ptr{Cvoid}, path::String)
        db = new(handle, path)
        finalizer(db) do d
            if d.handle != C_NULL
                ccall((:psr_database_close, LIBPATH[]), Cvoid, (Ptr{Cvoid},), d.handle)
                d.handle = C_NULL
            end
        end
        return db
    end
end

function Base.show(io::IO, db::Database)
    status = db.handle != C_NULL ? "open" : "closed"
    print(io, "Database(\"$(db.path)\", $status)")
end

"""
    open_database(path::String; log_level=PSR_LOG_OFF) -> Database

Open an existing SQLite database.
"""
function open_database(path::String; log_level::Cint=PSR_LOG_OFF)
    error_ref = Ref{Cint}(PSR_OK)
    handle = ccall(
        (:psr_database_open, LIBPATH[]),
        Ptr{Cvoid},
        (Cstring, Cint, Ptr{Cint}),
        path, log_level, error_ref
    )
    if handle == C_NULL
        throw(DatabaseException("Failed to open database: $path"))
    end
    return Database(handle, path)
end

"""
    create_empty_db_from_schema(db_path::String, schema_path::String; force::Bool=false, log_level=PSR_LOG_OFF) -> Database

Create a new database from a SQL schema file.
If `force=true`, delete existing database file first.
"""
function create_empty_db_from_schema(
    db_path::String,
    schema_path::String;
    force::Bool=false,
    log_level::Cint=PSR_LOG_OFF
)
    if force && isfile(db_path)
        # On Windows, SQLite files can remain locked briefly after closing
        # Retry deletion with GC to help release handles
        for attempt in 1:5
            try
                GC.gc()
                rm(db_path)
                break
            catch e
                if attempt == 5
                    rethrow()
                end
                sleep(0.1 * attempt)
            end
        end
    end

    if isfile(db_path)
        throw(DatabaseException("Database already exists: $db_path"))
    end

    if !isfile(schema_path)
        throw(DatabaseException("Schema file not found: $schema_path"))
    end

    # Read schema SQL
    schema_sql = read(schema_path, String)

    # Create empty database
    db = open_database(db_path; log_level=log_level)

    # Execute schema statements
    try
        for stmt in split_sql_statements(schema_sql)
            stmt = strip(stmt)
            if !isempty(stmt)
                execute(db, stmt)
            end
        end
    catch e
        close!(db)
        rm(db_path; force=true)
        rethrow()
    end

    return db
end

"""
Split SQL into individual statements, respecting string literals.
"""
function split_sql_statements(sql::String)
    statements = String[]
    current = IOBuffer()
    in_string = false
    string_char = '\0'

    i = 1
    while i <= length(sql)
        c = sql[i]

        # Handle string literals
        if (c == '\'' || c == '"') && (i == 1 || sql[i-1] != '\\')
            if !in_string
                in_string = true
                string_char = c
            elseif c == string_char
                in_string = false
            end
        end

        if c == ';' && !in_string
            stmt = String(take!(current))
            stmt = strip(stmt)
            if !isempty(stmt)
                push!(statements, stmt)
            end
        else
            write(current, c)
        end

        i += 1
    end

    # Handle last statement without trailing semicolon
    stmt = String(take!(current))
    stmt = strip(stmt)
    if !isempty(stmt)
        push!(statements, stmt)
    end

    return statements
end

"""
    close!(db::Database)

Close the database connection.
"""
function close!(db::Database)
    if db.handle != C_NULL
        ccall((:psr_database_close, LIBPATH[]), Cvoid, (Ptr{Cvoid},), db.handle)
        db.handle = C_NULL
    end
    return nothing
end

"""
    execute(db::Database, sql::String) -> Nothing

Execute a SQL statement.
"""
function execute(db::Database, sql::AbstractString)
    error_ref = Ref{Cint}(PSR_OK)
    result = ccall(
        (:psr_database_execute, LIBPATH[]),
        Ptr{Cvoid},
        (Ptr{Cvoid}, Cstring, Ptr{Cint}),
        db.handle, sql, error_ref
    )
    check_error(error_ref[], db.handle)
    if result != C_NULL
        ccall((:psr_result_free, LIBPATH[]), Cvoid, (Ptr{Cvoid},), result)
    end
    return nothing
end

"""
    transaction(f::Function, db::Database)

Execute function `f` within a database transaction.
"""
function transaction(f::Function, db::Database)
    error_ref = ccall((:psr_database_begin_transaction, LIBPATH[]), Cint, (Ptr{Cvoid},), db.handle)
    check_error(error_ref, db.handle)

    try
        result = f()
        error_ref = ccall((:psr_database_commit, LIBPATH[]), Cint, (Ptr{Cvoid},), db.handle)
        check_error(error_ref, db.handle)
        return result
    catch e
        ccall((:psr_database_rollback, LIBPATH[]), Cint, (Ptr{Cvoid},), db.handle)
        rethrow()
    end
end

"""
    get_element_id(db::Database, collection::String, label::String) -> Int64

Look up an element ID by its label.
"""
function get_element_id(db::Database, collection::String, label::String)
    error_ref = Ref{Cint}(PSR_OK)
    id = ccall(
        (:psr_database_get_element_id, LIBPATH[]),
        Int64,
        (Ptr{Cvoid}, Cstring, Cstring, Ptr{Cint}),
        db.handle, collection, label, error_ref
    )
    check_error(error_ref[], db.handle)
    return id
end

# Element builder wrapper
mutable struct ElementBuilder
    handle::Ptr{Cvoid}

    function ElementBuilder()
        handle = ccall((:psr_element_create, LIBPATH[]), Ptr{Cvoid}, ())
        if handle == C_NULL
            throw(DatabaseException("Failed to create element builder"))
        end
        builder = new(handle)
        finalizer(builder) do b
            if b.handle != C_NULL
                ccall((:psr_element_free, LIBPATH[]), Cvoid, (Ptr{Cvoid},), b.handle)
                b.handle = C_NULL
            end
        end
        return builder
    end
end

# Time series builder wrapper
mutable struct TimeSeriesBuilder
    handle::Ptr{Cvoid}

    function TimeSeriesBuilder()
        handle = ccall((:psr_time_series_create, LIBPATH[]), Ptr{Cvoid}, ())
        if handle == C_NULL
            throw(DatabaseException("Failed to create time series builder"))
        end
        builder = new(handle)
        finalizer(builder) do b
            if b.handle != C_NULL
                ccall((:psr_time_series_free, LIBPATH[]), Cvoid, (Ptr{Cvoid},), b.handle)
                b.handle = C_NULL
            end
        end
        return builder
    end
end

# Set scalar values on element
function element_set!(builder::ElementBuilder, column::String, value::Nothing)
    error_ref = ccall(
        (:psr_element_set_null, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring),
        builder.handle, column
    )
    check_error(error_ref)
end

function element_set!(builder::ElementBuilder, column::String, value::Missing)
    element_set!(builder, column, nothing)
end

function element_set!(builder::ElementBuilder, column::String, value::Integer)
    error_ref = ccall(
        (:psr_element_set_int, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Int64),
        builder.handle, column, Int64(value)
    )
    check_error(error_ref)
end

function element_set!(builder::ElementBuilder, column::String, value::AbstractFloat)
    error_ref = ccall(
        (:psr_element_set_double, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Float64),
        builder.handle, column, Float64(value)
    )
    check_error(error_ref)
end

function element_set!(builder::ElementBuilder, column::String, value::AbstractString)
    # Validate date format if column name contains "date"
    if occursin("date", lowercase(column))
        validate_date_string(column, value)
    end
    error_ref = ccall(
        (:psr_element_set_string, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Cstring),
        builder.handle, column, value
    )
    check_error(error_ref)
end

"""
Validate that a string is a valid date/datetime format.
Accepts: "YYYY-MM-DD" or "YYYY-MM-DD HH:MM:SS"
"""
function validate_date_string(column::String, value::AbstractString)
    # Use regex to validate format strictly
    # Pattern for YYYY-MM-DD HH:MM:SS
    datetime_pattern = r"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$"
    # Pattern for YYYY-MM-DD
    date_pattern = r"^\d{4}-\d{2}-\d{2}$"

    if !occursin(datetime_pattern, value) && !occursin(date_pattern, value)
        throw(DatabaseException("Invalid date format for column '$column': $value. Expected 'YYYY-MM-DD' or 'YYYY-MM-DD HH:MM:SS'"))
    end

    # Also validate that it parses to a real date (e.g., not month 13)
    try
        if occursin(datetime_pattern, value)
            Dates.DateTime(value, dateformat"yyyy-mm-dd HH:MM:SS")
        else
            Dates.Date(value, dateformat"yyyy-mm-dd")
        end
    catch
        throw(DatabaseException("Invalid date value for column '$column': $value. Date components out of range."))
    end
end

function element_set!(builder::ElementBuilder, column::String, value::DateTime)
    # Format as ISO 8601
    str = Dates.format(value, dateformat"yyyy-mm-dd HH:MM:SS")
    element_set!(builder, column, str)
end

function element_set!(builder::ElementBuilder, column::String, value::Date)
    throw(DatabaseException("Date type not supported, use DateTime instead for column '$column'"))
end

# Set vector values on element
function element_set!(builder::ElementBuilder, column::String, values::AbstractVector{<:Integer})
    if isempty(values)
        throw(DatabaseException("Empty vector not allowed for column '$column'"))
    end
    int_values = Int64.(values)
    error_ref = ccall(
        (:psr_element_set_int_array, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Int64}, Csize_t),
        builder.handle, column, int_values, length(int_values)
    )
    check_error(error_ref)
end

function element_set!(builder::ElementBuilder, column::String, values::AbstractVector{<:AbstractFloat})
    if isempty(values)
        throw(DatabaseException("Empty vector not allowed for column '$column'"))
    end
    # Handle missing values - replace with NaN or throw
    float_values = Float64[]
    for v in values
        if ismissing(v)
            push!(float_values, NaN)
        else
            push!(float_values, Float64(v))
        end
    end
    error_ref = ccall(
        (:psr_element_set_double_array, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Float64}, Csize_t),
        builder.handle, column, float_values, length(float_values)
    )
    check_error(error_ref)
end

function element_set!(builder::ElementBuilder, column::String, values::AbstractVector{<:Union{Missing, AbstractFloat}})
    if isempty(values)
        throw(DatabaseException("Empty vector not allowed for column '$column'"))
    end
    float_values = Float64[]
    for v in values
        if ismissing(v)
            push!(float_values, NaN)
        else
            push!(float_values, Float64(v))
        end
    end
    error_ref = ccall(
        (:psr_element_set_double_array, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Float64}, Csize_t),
        builder.handle, column, float_values, length(float_values)
    )
    check_error(error_ref)
end

function element_set!(builder::ElementBuilder, column::String, values::AbstractVector{<:AbstractString})
    if isempty(values)
        throw(DatabaseException("Empty vector not allowed for column '$column'"))
    end
    # Convert to vector of C strings
    cstrings = Vector{Cstring}(undef, length(values))
    GC.@preserve values begin
        for i in eachindex(values)
            cstrings[i] = Base.unsafe_convert(Cstring, values[i])
        end
        error_ref = ccall(
            (:psr_element_set_string_array, LIBPATH[]),
            Cint,
            (Ptr{Cvoid}, Cstring, Ptr{Cstring}, Csize_t),
            builder.handle, column, cstrings, length(cstrings)
        )
        check_error(error_ref)
    end
end

# Handle empty vectors
function element_set!(builder::ElementBuilder, column::String, values::AbstractVector{Any})
    if isempty(values)
        throw(DatabaseException("Empty vector not allowed for column '$column'"))
    end
    # Try to infer type from first element
    first_val = first(values)
    if first_val isa Integer
        element_set!(builder, column, Int64.(values))
    elseif first_val isa AbstractFloat
        element_set!(builder, column, Float64.(values))
    elseif first_val isa AbstractString
        element_set!(builder, column, String.(values))
    else
        throw(DatabaseException("Unsupported vector element type: $(typeof(first_val))"))
    end
end

# Time series support
function time_series_add_column!(ts::TimeSeriesBuilder, name::String, values::AbstractVector{<:Integer})
    int_values = Int64.(values)
    error_ref = ccall(
        (:psr_time_series_add_int_column, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Int64}, Csize_t),
        ts.handle, name, int_values, length(int_values)
    )
    check_error(error_ref)
end

function time_series_add_column!(ts::TimeSeriesBuilder, name::String, values::AbstractVector{<:AbstractFloat})
    float_values = Float64[]
    for v in values
        if ismissing(v)
            push!(float_values, NaN)
        else
            push!(float_values, Float64(v))
        end
    end
    error_ref = ccall(
        (:psr_time_series_add_double_column, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Float64}, Csize_t),
        ts.handle, name, float_values, length(float_values)
    )
    check_error(error_ref)
end

function time_series_add_column!(ts::TimeSeriesBuilder, name::String, values::AbstractVector{<:Union{Missing, AbstractFloat}})
    float_values = Float64[]
    for v in values
        if ismissing(v)
            push!(float_values, NaN)
        else
            push!(float_values, Float64(v))
        end
    end
    error_ref = ccall(
        (:psr_time_series_add_double_column, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Float64}, Csize_t),
        ts.handle, name, float_values, length(float_values)
    )
    check_error(error_ref)
end

function time_series_add_column!(ts::TimeSeriesBuilder, name::String, values::AbstractVector{<:AbstractString})
    cstrings = Vector{Cstring}(undef, length(values))
    GC.@preserve values begin
        for i in eachindex(values)
            cstrings[i] = Base.unsafe_convert(Cstring, values[i])
        end
        error_ref = ccall(
            (:psr_time_series_add_string_column, LIBPATH[]),
            Cint,
            (Ptr{Cvoid}, Cstring, Ptr{Cstring}, Csize_t),
            ts.handle, name, cstrings, length(cstrings)
        )
        check_error(error_ref)
    end
end

function time_series_add_column!(ts::TimeSeriesBuilder, name::String, values::AbstractVector{DateTime})
    str_values = [Dates.format(v, dateformat"yyyy-mm-dd HH:MM:SS") for v in values]
    time_series_add_column!(ts, name, str_values)
end

function element_add_time_series!(builder::ElementBuilder, group::String, ts::TimeSeriesBuilder)
    error_ref = ccall(
        (:psr_element_add_time_series, LIBPATH[]),
        Cint,
        (Ptr{Cvoid}, Cstring, Ptr{Cvoid}),
        builder.handle, group, ts.handle
    )
    check_error(error_ref)
end

"""
    create_element!(db::Database, table::String; kwargs...) -> Int64

Create a new element in the specified table with the given field values.
Returns the ID of the created element.

# Arguments
- `db`: Database connection
- `table`: Table name
- `kwargs`: Field name-value pairs. Values can be:
  - Scalars: String, Int, Float, DateTime
  - Vectors: Vector{Int}, Vector{Float}, Vector{String}
  - Time series: DataFrame with date_time column

# Example
```julia
create_element!(db, "Plant";
    label = "Plant 1",
    capacity = 100.0,
    some_vector = [1.0, 2.0, 3.0]
)
```
"""
function create_element!(db::Database, table::String; kwargs...)
    builder = ElementBuilder()

    for (key, value) in kwargs
        column = String(key)

        if value isa DataFrame
            # Time series - the key is the group name
            ts = TimeSeriesBuilder()
            for col_name in names(value)
                col_data = value[!, col_name]
                time_series_add_column!(ts, col_name, col_data)
            end
            element_add_time_series!(builder, column, ts)
        elseif value isa AbstractVector
            # Check if it's a vector of scalars (not strings that might be labels)
            if !isempty(value) && first(value) isa AbstractFloat
                element_set!(builder, column, value)
            elseif !isempty(value) && first(value) isa Integer
                element_set!(builder, column, value)
            elseif !isempty(value) && first(value) isa AbstractString
                element_set!(builder, column, value)
            elseif isempty(value)
                throw(DatabaseException("Empty vector not allowed for column '$column'"))
            else
                throw(DatabaseException("Unsupported vector type for column '$column': $(typeof(value))"))
            end
        else
            # Scalar value
            element_set!(builder, column, value)
        end
    end

    error_ref = Ref{Cint}(PSR_OK)
    id = ccall(
        (:psr_database_create_element, LIBPATH[]),
        Int64,
        (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Ptr{Cint}),
        db.handle, table, builder.handle, error_ref
    )
    check_error(error_ref[], db.handle)

    return id
end

end # module

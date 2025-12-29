module PsrDatabase

export Database, Result, execute, close!, is_open, last_insert_rowid, changes
export begin_transaction, commit, rollback
export row_count, column_count, columns, get_int, get_float, get_string, is_null

# Library loading
const LIBRARY_NAME = Sys.iswindows() ? "psr_database_c" : "libpsr_database_c"

function get_library_path()
    # Try to find the library in common locations
    paths = [
        joinpath(@__DIR__, "..", "lib", LIBRARY_NAME),
        joinpath(@__DIR__, "..", "..", "..", "build", "lib", LIBRARY_NAME),
        LIBRARY_NAME  # System path
    ]

    for path in paths
        lib_path = path * (Sys.iswindows() ? ".dll" : (Sys.isapple() ? ".dylib" : ".so"))
        if isfile(lib_path)
            return lib_path
        end
    end

    return LIBRARY_NAME  # Fall back to system lookup
end

const libpsr = Ref{Ptr{Nothing}}(C_NULL)

function __init__()
    lib_path = get_library_path()
    libpsr[] = Libdl.dlopen(lib_path)
end

using Libdl

# Error codes
const PSR_OK = 0
const PSR_ERROR_INVALID_ARGUMENT = -1
const PSR_ERROR_DATABASE = -2
const PSR_ERROR_QUERY = -3
const PSR_ERROR_NO_MEMORY = -4
const PSR_ERROR_NOT_OPEN = -5
const PSR_ERROR_INDEX_OUT_OF_RANGE = -6

# Value types
const PSR_TYPE_NULL = 0
const PSR_TYPE_INTEGER = 1
const PSR_TYPE_FLOAT = 2
const PSR_TYPE_TEXT = 3
const PSR_TYPE_BLOB = 4

# Opaque pointer types
const DatabasePtr = Ptr{Nothing}
const ResultPtr = Ptr{Nothing}

# Wrapper types
mutable struct Database
    ptr::DatabasePtr
    path::String

    function Database(path::AbstractString)
        error_ref = Ref{Cint}(0)
        ptr = ccall(
            Libdl.dlsym(libpsr[], :psr_database_open),
            DatabasePtr,
            (Cstring, Ptr{Cint}),
            path, error_ref
        )

        if ptr == C_NULL
            error("Failed to open database: error code $(error_ref[])")
        end

        db = new(ptr, String(path))
        finalizer(db) do x
            if x.ptr != C_NULL
                ccall(
                    Libdl.dlsym(libpsr[], :psr_database_close),
                    Cvoid,
                    (DatabasePtr,),
                    x.ptr
                )
                x.ptr = C_NULL
            end
        end
        return db
    end
end

struct Result
    ptr::ResultPtr
    row_count::Int
    column_count::Int
    columns::Vector{String}

    function Result(ptr::ResultPtr)
        if ptr == C_NULL
            return new(ptr, 0, 0, String[])
        end

        nrows = ccall(
            Libdl.dlsym(libpsr[], :psr_result_row_count),
            Csize_t,
            (ResultPtr,),
            ptr
        )

        ncols = ccall(
            Libdl.dlsym(libpsr[], :psr_result_column_count),
            Csize_t,
            (ResultPtr,),
            ptr
        )

        cols = String[]
        for i in 0:(ncols-1)
            name = ccall(
                Libdl.dlsym(libpsr[], :psr_result_column_name),
                Cstring,
                (ResultPtr, Csize_t),
                ptr, i
            )
            push!(cols, unsafe_string(name))
        end

        result = new(ptr, Int(nrows), Int(ncols), cols)
        finalizer(result) do r
            if r.ptr != C_NULL
                ccall(
                    Libdl.dlsym(libpsr[], :psr_result_free),
                    Cvoid,
                    (ResultPtr,),
                    r.ptr
                )
            end
        end
        return result
    end
end

# Database functions
function close!(db::Database)
    if db.ptr != C_NULL
        ccall(
            Libdl.dlsym(libpsr[], :psr_database_close),
            Cvoid,
            (DatabasePtr,),
            db.ptr
        )
        db.ptr = C_NULL
    end
    nothing
end

function is_open(db::Database)::Bool
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_database_is_open),
        Cint,
        (DatabasePtr,),
        db.ptr
    )
    return result != 0
end

function execute(db::Database, sql::AbstractString)::Result
    error_ref = Ref{Cint}(0)
    ptr = ccall(
        Libdl.dlsym(libpsr[], :psr_database_execute),
        ResultPtr,
        (DatabasePtr, Cstring, Ptr{Cint}),
        db.ptr, sql, error_ref
    )

    if ptr == C_NULL && error_ref[] != PSR_OK
        msg = ccall(
            Libdl.dlsym(libpsr[], :psr_database_error_message),
            Cstring,
            (DatabasePtr,),
            db.ptr
        )
        error("Query failed: $(unsafe_string(msg))")
    end

    return Result(ptr)
end

function last_insert_rowid(db::Database)::Int64
    return ccall(
        Libdl.dlsym(libpsr[], :psr_database_last_insert_rowid),
        Int64,
        (DatabasePtr,),
        db.ptr
    )
end

function changes(db::Database)::Int
    return ccall(
        Libdl.dlsym(libpsr[], :psr_database_changes),
        Cint,
        (DatabasePtr,),
        db.ptr
    )
end

function begin_transaction(db::Database)
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_database_begin_transaction),
        Cint,
        (DatabasePtr,),
        db.ptr
    )
    if result != PSR_OK
        error("Failed to begin transaction")
    end
    nothing
end

function commit(db::Database)
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_database_commit),
        Cint,
        (DatabasePtr,),
        db.ptr
    )
    if result != PSR_OK
        error("Failed to commit transaction")
    end
    nothing
end

function rollback(db::Database)
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_database_rollback),
        Cint,
        (DatabasePtr,),
        db.ptr
    )
    if result != PSR_OK
        error("Failed to rollback transaction")
    end
    nothing
end

# Result functions
row_count(r::Result) = r.row_count
column_count(r::Result) = r.column_count
columns(r::Result) = r.columns

function is_null(r::Result, row::Int, col::Int)::Bool
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_result_is_null),
        Cint,
        (ResultPtr, Csize_t, Csize_t),
        r.ptr, row - 1, col - 1  # Convert to 0-based
    )
    return result != 0
end

function get_int(r::Result, row::Int, col::Int)::Union{Int64, Nothing}
    value_ref = Ref{Int64}(0)
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_result_get_int),
        Cint,
        (ResultPtr, Csize_t, Csize_t, Ptr{Int64}),
        r.ptr, row - 1, col - 1, value_ref
    )
    return result == PSR_OK ? value_ref[] : nothing
end

function get_float(r::Result, row::Int, col::Int)::Union{Float64, Nothing}
    value_ref = Ref{Float64}(0.0)
    result = ccall(
        Libdl.dlsym(libpsr[], :psr_result_get_double),
        Cint,
        (ResultPtr, Csize_t, Csize_t, Ptr{Float64}),
        r.ptr, row - 1, col - 1, value_ref
    )
    return result == PSR_OK ? value_ref[] : nothing
end

function get_string(r::Result, row::Int, col::Int)::Union{String, Nothing}
    ptr = ccall(
        Libdl.dlsym(libpsr[], :psr_result_get_string),
        Cstring,
        (ResultPtr, Csize_t, Csize_t),
        r.ptr, row - 1, col - 1
    )
    return ptr == C_NULL ? nothing : unsafe_string(ptr)
end

# Iteration support
Base.length(r::Result) = r.row_count
Base.isempty(r::Result) = r.row_count == 0

function Base.iterate(r::Result, state=1)
    if state > r.row_count
        return nothing
    end

    row = Dict{String, Any}()
    for (i, col) in enumerate(r.columns)
        if is_null(r, state, i)
            row[col] = nothing
        else
            type = ccall(
                Libdl.dlsym(libpsr[], :psr_result_get_type),
                Cint,
                (ResultPtr, Csize_t, Csize_t),
                r.ptr, state - 1, i - 1
            )

            if type == PSR_TYPE_INTEGER
                row[col] = get_int(r, state, i)
            elseif type == PSR_TYPE_FLOAT
                row[col] = get_float(r, state, i)
            elseif type == PSR_TYPE_TEXT
                row[col] = get_string(r, state, i)
            else
                row[col] = nothing
            end
        end
    end

    return (row, state + 1)
end

# Version
function version()::String
    ptr = ccall(
        Libdl.dlsym(libpsr[], :psr_version),
        Cstring,
        ()
    )
    return unsafe_string(ptr)
end

end # module

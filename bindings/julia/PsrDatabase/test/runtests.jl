using Test
using PsrDatabase

@testset "PsrDatabase Tests" begin

    @testset "Database Open/Close" begin
        @testset "In-memory database" begin
            db = Database(":memory:")
            @test is_open(db)
            close!(db)
            @test !is_open(db)
        end

        @testset "File database" begin
            path = tempname() * ".db"
            try
                db = Database(path)
                @test is_open(db)
                close!(db)
            finally
                isfile(path) && rm(path)
            end
        end
    end

    @testset "Query Execution" begin
        @testset "Create table" begin
            db = Database(":memory:")
            result = execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
            @test row_count(result) == 0
            close!(db)
        end

        @testset "Insert and select" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
            execute(db, "INSERT INTO users (name) VALUES ('Alice')")
            @test last_insert_rowid(db) == 1
            @test changes(db) == 1

            execute(db, "INSERT INTO users (name) VALUES ('Bob')")
            @test last_insert_rowid(db) == 2

            result = execute(db, "SELECT * FROM users ORDER BY id")
            @test row_count(result) == 2
            @test column_count(result) == 2
            @test columns(result) == ["id", "name"]
            close!(db)
        end

        @testset "Invalid SQL throws error" begin
            db = Database(":memory:")
            @test_throws Exception execute(db, "INVALID SQL STATEMENT")
            close!(db)
        end
    end

    @testset "Transactions" begin
        @testset "Commit" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE counter (value INTEGER)")
            execute(db, "INSERT INTO counter VALUES (0)")

            begin_transaction(db)
            execute(db, "UPDATE counter SET value = 42")
            commit(db)

            result = execute(db, "SELECT value FROM counter")
            @test get_int(result, 1, 1) == 42
            close!(db)
        end

        @testset "Rollback" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE counter (value INTEGER)")
            execute(db, "INSERT INTO counter VALUES (0)")

            begin_transaction(db)
            execute(db, "UPDATE counter SET value = 42")
            rollback(db)

            result = execute(db, "SELECT value FROM counter")
            @test get_int(result, 1, 1) == 0
            close!(db)
        end
    end

    @testset "Data Types" begin
        @testset "Integer" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE data (value INTEGER)")
            execute(db, "INSERT INTO data VALUES (42)")

            result = execute(db, "SELECT value FROM data")
            @test get_int(result, 1, 1) == 42
            close!(db)
        end

        @testset "Float" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE data (value REAL)")
            execute(db, "INSERT INTO data VALUES (3.14159)")

            result = execute(db, "SELECT value FROM data")
            @test isapprox(get_float(result, 1, 1), 3.14159, atol=0.0001)
            close!(db)
        end

        @testset "String" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE data (value TEXT)")
            execute(db, "INSERT INTO data VALUES ('Hello, World!')")

            result = execute(db, "SELECT value FROM data")
            @test get_string(result, 1, 1) == "Hello, World!"
            close!(db)
        end

        @testset "NULL" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE data (value TEXT)")
            execute(db, "INSERT INTO data VALUES (NULL)")

            result = execute(db, "SELECT value FROM data")
            @test is_null(result, 1, 1)
            @test get_string(result, 1, 1) === nothing
            close!(db)
        end
    end

    @testset "Result Iteration" begin
        @testset "Empty result" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE users (id INTEGER)")
            result = execute(db, "SELECT * FROM users")

            @test isempty(result)
            @test length(result) == 0
            close!(db)
        end

        @testset "Iterate rows" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE numbers (n INTEGER)")
            for i in 1:5
                execute(db, "INSERT INTO numbers VALUES ($i)")
            end

            result = execute(db, "SELECT n FROM numbers ORDER BY n")
            @test length(result) == 5

            values = Int64[]
            for row in result
                push!(values, row["n"])
            end
            @test values == [1, 2, 3, 4, 5]
            close!(db)
        end
    end

    @testset "Value Access" begin
        @testset "Get values by row and column" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE data (i INTEGER, f REAL, t TEXT)")
            execute(db, "INSERT INTO data VALUES (42, 3.14, 'hello')")

            result = execute(db, "SELECT * FROM data")

            @test get_int(result, 1, 1) == 42
            @test isapprox(get_float(result, 1, 2), 3.14, atol=0.01)
            @test get_string(result, 1, 3) == "hello"
            close!(db)
        end

        @testset "Wrong type returns nothing" begin
            db = Database(":memory:")
            execute(db, "CREATE TABLE data (value TEXT)")
            execute(db, "INSERT INTO data VALUES ('hello')")

            result = execute(db, "SELECT * FROM data")

            @test get_int(result, 1, 1) === nothing  # string, not int
            @test get_string(result, 1, 1) == "hello"
            close!(db)
        end
    end

    @testset "Version" begin
        v = version()
        @test v isa String
        @test !isempty(v)
    end

end

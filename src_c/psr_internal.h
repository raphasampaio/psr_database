#ifndef PSR_INTERNAL_H
#define PSR_INTERNAL_H

#include "psr_database/database.h"
#include "psr_database/result.h"
#include <string>

// Internal wrapper structures (shared between psr_database.cpp and psr_result.cpp)
struct psr_database {
    psr::Database db;
    std::string last_error;

    explicit psr_database(const std::string& path) : db(path) {}
};

struct psr_result {
    psr::Result result;

    explicit psr_result(psr::Result r) : result(std::move(r)) {}
};

#endif // PSR_INTERNAL_H

#ifndef HGDB_RTL_OBJECT_HH
#define HGDB_RTL_OBJECT_HH

#include "../../src/rtl.hh"
#include "slang/binding/OperatorExpressions.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/symbols/PortSymbols.h"
#include "slang/symbols/VariableSymbols.h"

struct QueryObject {
public:
    explicit QueryObject(hgdb::rtl::DesignDatabase *db) : db(db) {}
    hgdb::rtl::DesignDatabase *db;
    virtual ~QueryObject() = default;
};

struct QueryArray : QueryObject {
public:
    template <typename T>
    QueryArray(hgdb::rtl::DesignDatabase *db, const T &begin, const T &end)
        : QueryObject(db), list(begin, end) {}
    hgdb::rtl::DesignDatabase *db = nullptr;
    std::vector<QueryObject> list;
};

struct InstanceObject : public QueryObject {
public:
    InstanceObject(hgdb::rtl::DesignDatabase *db, const slang::InstanceSymbol *instance)
        : QueryObject(db), instance(instance) {}
    // this holds instance information
    const slang::InstanceSymbol *instance;
};

struct VariableObject : public QueryObject {
public:
    VariableObject(hgdb::rtl::DesignDatabase *db, const slang::ValueSymbol *variable)
        : QueryObject(db), variable(variable) {}
    const slang::ValueSymbol *variable;
};

struct PortObject : public VariableObject {
public:
    PortObject(hgdb::rtl::DesignDatabase *db, const slang::PortSymbol *port)
        : VariableObject(db, port), port(port) {}
    const slang::PortSymbol *port = nullptr;
};

#endif  // HGDB_RTL_OBJECT_HH

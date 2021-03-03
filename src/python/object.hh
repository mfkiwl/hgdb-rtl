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
    virtual ~QueryObject() = default;
};

struct QueryArray : public QueryObject {};

struct RTLQueryObject {
public:
    explicit RTLQueryObject(hgdb::rtl::DesignDatabase *db) : db(db) {}
    hgdb::rtl::DesignDatabase *db;
};

struct RTLQueryArray : QueryArray, RTLQueryObject {
public:
    template <typename T>
    RTLQueryArray(hgdb::rtl::DesignDatabase *db, const T &begin, const T &end)
        : RTLQueryObject(db), list(begin, end) {}
    hgdb::rtl::DesignDatabase *db = nullptr;
    std::vector<RTLQueryObject> list;
};

struct InstanceObject : public RTLQueryObject {
public:
    InstanceObject(hgdb::rtl::DesignDatabase *db, const slang::InstanceSymbol *instance)
        : RTLQueryObject(db), instance(instance) {}
    // this holds instance information
    const slang::InstanceSymbol *instance;
};

struct VariableObject : public RTLQueryObject {
public:
    VariableObject(hgdb::rtl::DesignDatabase *db, const slang::ValueSymbol *variable)
        : RTLQueryObject(db), variable(variable) {}
    const slang::ValueSymbol *variable;
};

struct PortObject : public VariableObject {
public:
    PortObject(hgdb::rtl::DesignDatabase *db, const slang::PortSymbol *port)
        : VariableObject(db, port), port(port) {}
    const slang::PortSymbol *port = nullptr;
};

#endif  // HGDB_RTL_OBJECT_HH

#ifndef HGDB_RTL_PYTHON_RTL_HH
#define HGDB_RTL_PYTHON_RTL_HH

#include "data_source.hh"
#include "object.hh"
#include "slang/binding/OperatorExpressions.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/symbols/PortSymbols.h"
#include "slang/symbols/VariableSymbols.h"

struct RTLQueryObject : public QueryObject {
public:
    explicit RTLQueryObject(hgdb::rtl::DesignDatabase *db) : db(db) {}
    hgdb::rtl::DesignDatabase *db;
};

struct RTLQueryArray : QueryArray, RTLQueryObject {
public:
    template <typename T>
    RTLQueryArray(hgdb::rtl::DesignDatabase *db, const T &begin, const T &end)
        : RTLQueryObject(db), rtl_list(begin, end) {}
    hgdb::rtl::DesignDatabase *db = nullptr;
    std::vector<std::shared_ptr<RTLQueryObject>> rtl_list;
    [[nodiscard]] uint64_t size() const override { return rtl_list.size(); }
    [[nodiscard]] QueryObject *get(uint64_t idx) const override { return rtl_list[idx].get(); }
    void add(const std::shared_ptr<QueryObject> &obj) override;
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

class InstanceSelector : public Selector {
public:
    explicit InstanceSelector(hgdb::rtl::DesignDatabase &db);

private:
    std::vector<std::shared_ptr<InstanceObject>> instances_;
};

class VariableSelector : public Selector {
public:
    explicit VariableSelector(hgdb::rtl::DesignDatabase &db);

private:
    std::vector<std::shared_ptr<VariableObject>> variables_;
};

class PortSelector : public Selector {
public:
    explicit PortSelector(hgdb::rtl::DesignDatabase &db);

private:
    std::vector<std::shared_ptr<PortSelector>> ports_;
};

class RTL : public DataSource {
public:
    inline RTL() : DataSource(DataSourceType::RTL) {}
    // methods to add files to the compilation unit
    inline void add_include_dir(const std::string &path) { include_dirs.emplace_back(path); }
    inline void add_system_include_dir(const std::string &path) {
        include_sys_dirs_.emplace_back(path);
    }
    inline void add_file(const std::string &path) { files_.emplace_back(path); }
    inline void add_macro(const std::string &name, const std::string &value) {
        macros_.emplace(name, value);
    }
    inline void set_top(const std::string &top) { top_ = top; }

    [[nodiscard]] std::unique_ptr<slang::Compilation> compile() const;

    [[nodiscard]] inline std::vector<py::handle> provides() const override {
        return {py::type::of<InstanceObject>(), py::type::of<VariableObject>(),
                py::type::of<PortObject>()};
    }

    std::shared_ptr<Selector> get_selector(py::handle handle) override;

    inline void on_added(Ooze *) override;

private:
    std::vector<std::string> include_dirs;
    std::vector<std::string> include_sys_dirs_;
    std::vector<std::string> files_;
    std::map<std::string, std::string> macros_;
    std::string top_;

    // the compilation object
    std::unique_ptr<slang::Compilation> compilation_;
    std::unique_ptr<hgdb::rtl::DesignDatabase> db_;
};

#endif  // HGDB_RTL_PYTHON_RTL_HH

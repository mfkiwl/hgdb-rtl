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
    enum class RTLKind { Instance, Variable, Port };
    RTLQueryObject() = delete;
    explicit RTLQueryObject(hgdb::rtl::DesignDatabase *db, const slang::Symbol *symbol,
                            RTLKind kind)
        : db(db), symbol(symbol), kind(kind) {}
    hgdb::rtl::DesignDatabase *db;

    const slang::Symbol *symbol;
    RTLKind kind;
};

struct InstanceObject : public RTLQueryObject {
public:
    InstanceObject() = delete;
    InstanceObject(hgdb::rtl::DesignDatabase *db, const slang::InstanceSymbol *instance)
        : RTLQueryObject(db, instance, RTLKind::Instance), instance(instance) {}
    // this holds instance information
    const slang::InstanceSymbol *instance = nullptr;

    [[nodiscard]] std::map<std::string, py::object> values() const override;

    [[maybe_unused]] [[nodiscard]] static bool is_kind(RTLKind kind) {
        return kind == RTLKind::Instance;
    }
};

struct VariableObject : public RTLQueryObject {
public:
    VariableObject() = delete;
    VariableObject(hgdb::rtl::DesignDatabase *db, const slang::ValueSymbol *variable)
        : RTLQueryObject(db, variable, RTLKind::Variable), variable(variable) {}
    const slang::ValueSymbol *variable = nullptr;

    [[nodiscard]] std::map<std::string, py::object> values() const override;

    [[maybe_unused]] static bool is_kind(RTLKind kind) {
        return kind == RTLKind::Port || kind == RTLKind::Variable;
    }
};

struct PortObject : public VariableObject {
public:
    PortObject() = delete;
    PortObject(hgdb::rtl::DesignDatabase *db, const slang::PortSymbol *port)
        : VariableObject(db, port), port(port) {
        kind = RTLKind::Port;
    }
    const slang::PortSymbol *port = nullptr;

    [[nodiscard]] std::map<std::string, py::object> values() const override;

    [[maybe_unused]] static bool is_kind(RTLKind kind) { return kind == RTLKind::Port; }
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

    void compile();

    [[nodiscard]] inline std::vector<py::handle> provides() const override {
        return {py::type::of<InstanceObject>(), py::type::of<VariableObject>(),
                py::type::of<PortObject>()};
    }

    std::shared_ptr<QueryArray> get_selector(py::handle handle) override;

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
    std::unique_ptr<slang::SourceManager> source_manager_;
};

#endif  // HGDB_RTL_PYTHON_RTL_HH

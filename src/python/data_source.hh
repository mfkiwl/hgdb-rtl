#ifndef HGDB_RTL_DATA_SOURCE_HH
#define HGDB_RTL_DATA_SOURCE_HH

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "object.hh"
#include "query.hh"
#include "slang/compilation/Compilation.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

namespace py = pybind11;

enum class DataSourceType { RTL, Mapping, ValueChange, Log };

class Ooze;

class DataSource {
public:
    explicit DataSource(DataSourceType type) : type(type) {}

    DataSourceType type;

    [[nodiscard]] virtual std::vector<py::handle> provides() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Selector> get_selector(py::handle handle) = 0;

    virtual void on_added(Ooze *) {}
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

    std::unique_ptr<Selector> get_selector(py::handle handle) override;

    inline void on_added(Ooze *) override { compilation_ = compile(); }

private:
    std::vector<std::string> include_dirs;
    std::vector<std::string> include_sys_dirs_;
    std::vector<std::string> files_;
    std::map<std::string, std::string> macros_;
    std::string top_;

    // the compilation object
    std::unique_ptr<slang::Compilation> compilation_;
};

class Ooze {
public:
    Ooze() = default;
    void add_source(const std::shared_ptr<DataSource> &source);

private:
    std::vector<std::shared_ptr<DataSource>> sources_;
    std::map<py::handle, std::function<std::unique_ptr<Selector>(py::handle)>> selector_providers_;
};

void init_data_source(py::module &m);

#endif  // HGDB_RTL_DATA_SOURCE_HH

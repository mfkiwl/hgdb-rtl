#ifndef HGDB_RTL_DATA_SOURCE_HH
#define HGDB_RTL_DATA_SOURCE_HH

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "slang/compilation/Compilation.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

namespace py = pybind11;

enum class DataSourceType { RTL, Mapping, ValueChange, Log };

class DataSource {
public:
    explicit DataSource(DataSourceType type) : type_(type) {}

protected:
    DataSourceType type_;
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

private:
    std::vector<std::string> include_dirs;
    std::vector<std::string> include_sys_dirs_;
    std::vector<std::string> files_;
    std::map<std::string, std::string> macros_;
    std::string top_;
};

void init_data_source(py::module &m);

#endif  // HGDB_RTL_DATA_SOURCE_HH

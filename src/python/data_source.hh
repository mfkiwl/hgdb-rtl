#ifndef HGDB_RTL_DATA_SOURCE_HH
#define HGDB_RTL_DATA_SOURCE_HH

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "object.hh"
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
    [[nodiscard]] virtual std::shared_ptr<QueryArray> get_selector(py::handle handle) = 0;

    virtual void on_added(Ooze *) {}
};

class Ooze {
public:
    Ooze() = default;
    void add_source(const std::shared_ptr<DataSource> &source);

    std::vector<std::shared_ptr<DataSource>> sources;
    struct SelectorProvider {
        py::handle handle;
        std::function<std::shared_ptr<QueryArray>(py::handle)> func;
    };
    std::vector<SelectorProvider> selector_providers;
};

void init_data_source(py::module &m);

#endif  // HGDB_RTL_DATA_SOURCE_HH

#include "log.hh"

namespace py = pybind11;

void clear_cache() {
    LogItem::clear_cache();
}

void init_util(py::module &m) {
    m.def("clear_cache", &clear_cache);
}
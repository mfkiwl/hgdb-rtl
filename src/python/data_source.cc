#include "data_source.hh"

void init_data_source(py::module &m) {
    py::class_<DataSource>(m, "DataSource");
}
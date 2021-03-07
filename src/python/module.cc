#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void init_object(py::module &m);
void init_data_source(py::module &m);
void init_rtl(py::module &m);
void init_query_helper_function(py::module &m);
void init_vcd(py::module &m);

PYBIND11_MODULE(ooze, m) {
    init_object(m);
    init_data_source(m);
    init_rtl(m);
    init_query_helper_function(m);
    init_vcd(m);
}
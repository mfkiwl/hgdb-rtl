#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

void init_object(py::module &m);

PYBIND11_MODULE(ooze, m) { init_object(m);
}
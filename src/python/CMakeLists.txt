set(PYBIND11_CPP_STANDARD -std=c++20)

# tentative name ooze
pybind11_add_module(ooze module.cc object.cc)
target_link_libraries(ooze PRIVATE hgdb-rtl)
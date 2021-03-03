#include "data_source.hh"

void Ooze::add_source(const std::shared_ptr<DataSource> &source) {
    // need to register provider type
    sources.emplace_back(source);
    // register selected type
    auto const types = source->provides();
    auto func = [=](py::handle handle) -> std::unique_ptr<Selector> {
        return source->get_selector(handle);
    };
    for (auto const &t : types) {
        // need to register selector object
        selector_providers.emplace(t, func);
    }
}

void init_data_source(py::module &m) {
    py::class_<DataSource, std::shared_ptr<DataSource>>(m, "DataSource")
        .def_property_readonly("type", [](const DataSource &source) { return source.type; });

    py::class_<Ooze>(m, "Ooze")
        .def("add_source", &Ooze::add_source)
        .def("select", [](Ooze *ooze, const py::list &types) {
            auto query_array = std::make_unique<SelectorQueryArray>();
            // need to find registered types
            for (auto const &t : types) {
                if (ooze->selector_providers.find(t) == ooze->selector_providers.end()) {
                    throw py::type_error();
                }
                auto const &select_func = ooze->selector_providers.at(t);
                auto selector = select_func(t);
                if (selector) {
                    // need to add it to the selector
                    query_array->selectors_.emplace_back(std::move(selector));
                }
            }
            return std::move(query_array);
        });
}
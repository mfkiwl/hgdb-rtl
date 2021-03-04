#include "data_source.hh"

void Ooze::add_source(const std::shared_ptr<DataSource> &source) {
    // need to register provider type
    sources.emplace_back(source);
    // register selected type
    auto const types = source->provides();
    auto func = [=](py::handle handle) -> std::shared_ptr<QueryArray> {
        return source->get_selector(handle);
    };
    for (auto const &t : types) {
        // need to register selector object
        selector_providers.emplace_back(SelectorProvider{t, func});
    }
    source->on_added(this);
}

void init_data_source(py::module &m) {
    py::class_<DataSource, std::shared_ptr<DataSource>>(m, "DataSource")
        .def_property_readonly("type", [](const DataSource &source) { return source.type; });

    py::class_<Ooze>(m, "Ooze")
        .def(py::init<>())
        .def("add_source", &Ooze::add_source, py::arg("data_source"))
        .def("select", [](Ooze *ooze, const py::args &types) -> std::shared_ptr<QueryObject> {
            auto query_array = std::make_shared<QueryArray>();
            // need to find registered types
            for (auto const &t : types) {
                for (auto const &provider : ooze->selector_providers) {
                    if (provider.handle.is(t)) {
                        auto selector = provider.func(t);
                        if (selector) {
                            // need to add it to the selector
                            query_array->add(selector);
                        }
                    }
                }
            }
            return query_array->size() == 1 ? query_array->get(0) : query_array;
        });
}
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
        selector_providers.emplace_back(SelectorProvider{source.get(), t, func});
    }
    source->on_added(this);
}

std::shared_ptr<QueryObject> bind(DataSource *src, const std::shared_ptr<QueryObject> &obj,
                                  const py::object &type) {
    // depends on if the object is an array or not
    if (obj->is_array()) {
        auto result = std::make_shared<QueryArray>(obj->ooze);
        auto const &array = std::reinterpret_pointer_cast<QueryArray>(obj);
        for (auto const &entry : array->data) {
            auto r = bind(src, entry, type);
            if (r) {
                result->add(r);
            }
        }
        return flatten_size_one_array(array);
    } else {
        return src->bind(obj, type);
    }
}

std::shared_ptr<QueryObject> ooze_bind(const Ooze &ooze, const std::shared_ptr<QueryObject> &obj,
                                       const py::object &type) {
    // first need to find out which data source to serve
    DataSource *src = nullptr;
    for (auto const &provider : ooze.selector_providers) {
        if (provider.handle.is(type)) {
            src = provider.src;
            break;
        }
    }
    if (!src) {
        auto str = py::str(type);
        auto s = str.cast<std::string>();
        throw std::runtime_error("Unable to find data source for type " + s);
    }
    return bind(src, obj, type);
}

std::shared_ptr<QueryObject> ooze_select(Ooze *ooze, const py::args &types) {
    auto query_array = std::make_shared<QueryArray>(ooze);
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
}

void init_ooze(py::module &m) {
    py::class_<Ooze>(m, "Ooze")
        .def(py::init<>())
        .def("add_source", &Ooze::add_source, py::arg("data_source"))
        .def("select", &ooze_select)
        .def("bind", &ooze_bind, py::arg("object"), py::arg("type"))
        .def(
            "provider",
            [](const Ooze &ooze, const py::object &type) -> DataSource * {
                for (auto const &provider : ooze.selector_providers) {
                    if (provider.handle.is(type)) {
                        return provider.src;
                    }
                }
                return nullptr;
            },
            py::return_value_policy::reference_internal)
        .def(
            "object",
            [](Ooze &ooze, const std::map<std::string, py::object> &values) {
                return std::make_shared<GenericQueryObject>(&ooze, values);
            },
            py::arg("values"))
        .def(
            "array",
            [](Ooze &ooze, const std::vector<std::shared_ptr<QueryObject>> &array) {
                return std::make_shared<QueryArray>(&ooze, array);
            },
            py::arg("array"));
}

void init_data_source(py::module &m) {
    py::class_<DataSource, std::shared_ptr<DataSource>>(m, "DataSource")
        .def_property_readonly("type", [](const DataSource &source) { return source.type; });

    init_ooze(m);
}
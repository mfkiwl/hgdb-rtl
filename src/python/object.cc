#include "object.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <sstream>

#include "query.hh"

namespace py = pybind11;

std::shared_ptr<QueryObject> QueryObject::map(
    const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) {
    return mapper(this);
}

std::shared_ptr<QueryObject> QueryArray::map(
    const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) {
    auto result = std::make_shared<QueryArray>();
    auto len = size();
    for (uint64_t i = 0; i < len; i++) {
        auto obj = get(i);
        // cast it here so that it will register in python
        auto py_obj = py::cast(obj);
        (void)(py_obj);
        auto new_obj = mapper(obj.get());
        if (new_obj) {
            result->add(new_obj);
        }
    }
    return result;
}

void QueryArray::add(const std::shared_ptr<QueryObject> &obj) { data.emplace_back(obj); }

std::shared_ptr<QueryObject> flatten_size_one_array(const std::shared_ptr<QueryObject> &obj) {
    if (obj->is_array()) {
        auto array = std::reinterpret_pointer_cast<QueryArray>(obj);
        if (array->empty()) {
            return nullptr;
        } else if (array->size() == 1) {
            return array->get(0);
        }
    }
    return obj;
}

std::shared_ptr<QueryObject> filter_query_object(
    const std::shared_ptr<QueryObject> &obj,
    const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
    auto filter = Filter(func);
    auto r = filter.apply(obj);
    return flatten_size_one_array(r);
}

std::shared_ptr<QueryObject> filter_query_object_kwargs(const std::shared_ptr<QueryObject> &obj,
                                                        const py::kwargs &kwargs) {
    // need to construct the function
    auto func = [&](const std::shared_ptr<QueryObject> &target) -> bool {
        auto py_obj = py::cast(target);
        for (auto const &[name, value] : kwargs) {  // NOLINT
            if (!py::hasattr(py_obj, name)) return false;
            auto const &v = py_obj.attr(name);
            if (!value.equal(v)) return false;
        }
        return true;
    };

    auto filter = Filter(func);
    auto r = filter.apply(obj);
    return flatten_size_one_array(r);
}

std::shared_ptr<QueryObject> select_type(const std::shared_ptr<QueryObject> &obj,
                                         const py::object &type) {
    // if it's not an array it's easy
    if (!obj->is_array()) {
        auto py_obj = py::cast(obj);
        if (py_obj.get_type().is(type)) {
            return obj;
        } else {
            return nullptr;
        }
    }

    auto array = std::reinterpret_pointer_cast<QueryArray>(obj);
    auto result = std::make_shared<QueryArray>();
    for (auto const &entry : array->data) {
        auto py_obj = py::cast(entry);
        if (entry->is_array()) {
            // recursively calls itself
            auto r = select_type(entry, type);
            if (!r) continue;
            if (r->is_array()) {
                auto const &r_array = std::reinterpret_pointer_cast<QueryArray>(r);
                if (!r_array->empty()) {
                    result->add(r);
                }
            } else {
                result->add(r);
            }
        } else {
            if (py_obj.get_type().is(type)) {
                result->add(entry);
            }
        }
    }
    // we also flatten out one layer if it's nested size 1 loop
    return flatten_size_one_array(result);
}

std::shared_ptr<QueryObject> map_object(
    const std::shared_ptr<QueryObject> &obj,
    const std::function<std::shared_ptr<QueryObject>(const std::shared_ptr<QueryObject> &)> &func) {
    if (obj->is_array()) {
        auto const &array = std::reinterpret_pointer_cast<QueryArray>(obj);
        auto result = std::make_shared<QueryArray>();
        for (auto const &entry : array->data) {
            auto o = map_object(entry, func);
            if (o) {
                result->add(o);
            }
        }
        return flatten_size_one_array(result);
    } else {
        auto o = func(obj);
        return o;
    }
}

GenericQueryObject::GenericQueryObject(const std::shared_ptr<QueryObject> &obj) {
    if (obj->is_array()) throw std::runtime_error("Cannot convert an array to generic object");
    auto const values = obj->values();
    for (auto const &[key, value] : values) {
        attrs.emplace(key, value);
    }
}

class GenericAttributeError : public std::runtime_error {
public:
    explicit GenericAttributeError(const std::string &str) : std::runtime_error(str) {}
};

void compute_hash_keys(const std::vector<std::string> &join_keys,
                       const std::shared_ptr<QueryArray> &array,
                       std::map<uint64_t, std::vector<std::shared_ptr<QueryObject>>> &hash_map) {
    for (auto const &entry : array->data) {
        if (entry->is_array()) {
            throw std::runtime_error(
                "Multi-dimensional array join not supported. Please flatten the array first!");
        }
        auto py_obj = py::cast(entry);
        // need to use each join keys to figure out the actual hash
        uint64_t hash = 0;
        bool no_key = false;
        for (uint64_t i = 0; i < join_keys.size(); i++) {
            auto const &key = join_keys[i];
            if (py::hasattr(py_obj, key.c_str())) {
                auto h = py::hash(py_obj.attr(key.c_str()));
                hash ^= h << i;
            } else {
                no_key = true;
                break;
            }
        }
        if (!no_key) {
            hash_map[hash].emplace_back(entry);
        }
    }
}

std::shared_ptr<QueryObject> merge_object(const std::shared_ptr<QueryObject> &obj1,
                                          const std::shared_ptr<QueryObject> &obj2) {
    auto result = std::make_shared<GenericQueryObject>(obj1);
    auto const values = obj2->values();
    for (auto const &[key, value] : values) {
        if (result->attrs.find(key) == result->attrs.end()) {
            result->attrs.emplace(key, value);
        }
    }

    return result;
}

std::shared_ptr<QueryObject> join_object(const std::shared_ptr<QueryObject> &obj1,
                                         const std::shared_ptr<QueryObject> &obj2,
                                         const std::vector<std::string> &join_keys) {
    auto result = std::make_shared<QueryArray>();
    std::shared_ptr<QueryArray> array1, array2;
    // we turn two objects into arrays
    if (obj1->is_array()) {
        array1 = std::reinterpret_pointer_cast<QueryArray>(obj1);
    } else {
        array1 = std::make_shared<QueryArray>();
        array1->add(obj1);
    }
    if (obj2->is_array()) {
        array2 = std::reinterpret_pointer_cast<QueryArray>(obj2);
    } else {
        array2 = std::make_shared<QueryArray>();
        array2->add(obj2);
    }

    // we use hash join
    std::map<uint64_t, std::vector<std::shared_ptr<QueryObject>>> hash1;
    std::map<uint64_t, std::vector<std::shared_ptr<QueryObject>>> hash2;

    compute_hash_keys(join_keys, array1, hash1);
    compute_hash_keys(join_keys, array2, hash2);

    // compute the result
    for (auto const &[hash, result1] : hash1) {
        if (hash2.find(hash) != hash2.end()) {
            auto const &result2 = hash2.at(hash);
            // do a cross product here
            for (auto const &entry1 : result1) {
                for (auto const &entry2 : result2) {
                    auto r = merge_object(entry1, entry2);
                    result->add(r);
                }
            }
        }
    }

    return flatten_size_one_array(result);
}

std::shared_ptr<QueryObject> query_object_select(const std::shared_ptr<QueryObject> &obj,
                                                 const py::args &args) {
    if (args.size() == 1) {
        // only one of them, try if it's a type
        py::object t = args[0];
        // a little bit hacky here since pybind doesn't provide `type` primitive
        auto tp_name = std::string(t.ptr()->ob_type->tp_name);
        if (tp_name == "pybind11_type") {
            auto r = select_type(obj, t);
            return r;
        }
    }
    if (obj->is_array()) {
        auto array = std::reinterpret_pointer_cast<QueryArray>(obj);
        auto r = std::make_shared<QueryArray>();
        for (auto const &entry : array->data) {
            auto selected = query_object_select(entry, args);
            r->add(selected);
        }
        return flatten_size_one_array(r);
    } else {
        // based on whether it is an array or not
        // create a generic object based on each arg select
        auto r = std::make_shared<GenericQueryObject>();
        auto py_obj = py::cast(obj);
        auto res = py::cast(r);
        for (auto const &arg : args) {
            auto value = py::getattr(py_obj, arg);
            auto arg_str = py::cast<std::string>(arg);
            // notice that the following doesn't work with Python
            // res.attr(arg) = value;
            // as a result, we have to fold everything into the attr
            r->attrs.emplace(arg_str, value);
        }
        return r;
    }
}

void init_query_object(py::module &m) {
    auto obj = py::class_<QueryObject, std::shared_ptr<QueryObject>>(m, "QueryObject");
    obj.def("map", &QueryObject::map);
    obj.def("__repr__", [](const QueryObject &obj) {
        // if str() is not implemented, we create values from the dict
        auto s = obj.str();
        if (!s.empty()) return py::str(s);
        // pretty print the values
        // we will just use json to print out stuff
        py::dict dict;
        auto const values = obj.values();
        for (auto const &[name, value] : values) {
            dict[name.c_str()] = value;
        }
        return py::str(dict);
    });

    // the filter part
    obj.def(
        "filter",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
            return filter_query_object(obj, func);
        },
        py::arg("predicate"));
    obj.def("filter", &filter_query_object_kwargs);
    // notice that where is the same as filter
    obj.def(
        "where",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
            return filter_query_object(obj, func);
        },
        py::arg("predicate"));
    obj.def("where", &filter_query_object_kwargs);
    obj.def("select", &query_object_select);
    obj.def(
        "map",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<std::shared_ptr<QueryObject>(const std::shared_ptr<QueryObject> &)>
               &func) { return map_object(obj, func); });
    obj.def("__eq__",
            [](const std::shared_ptr<QueryObject> &a, const std::shared_ptr<QueryObject> &b) {
                return py::str(py::cast(a)).equal(py::str(py::cast(b)));
            });

    obj.def("__hash__", [](const std::shared_ptr<QueryObject> &obj) {
        return py::hash(py::str(py::cast(obj)));
    });

    // join option
    obj.def(
        "join",
        [](const std::shared_ptr<QueryObject> &obj, const std::shared_ptr<QueryObject> &other,
           const py::args &keys) {
            auto join_keys = py::cast<std::vector<std::string>>(keys);
            return join_object(obj, other, join_keys);
        },
        py::arg("other"));
}

void init_query_array(py::module &m) {
    auto array = py::class_<QueryArray, QueryObject, std::shared_ptr<QueryArray>>(m, "QueryArray");

    // implement array interface
    array.def("__len__", [](const QueryArray &array) { return array.size(); })
        .def(
            "__getitem__",
            [](const QueryArray &array, uint64_t index) {
                if (index >= array.size()) {
                    throw py::index_error();
                }
                return array.get(index);
            },
            py::return_value_policy::reference)
        .def("__iter__", [](QueryArray &array) { return py::make_iterator(array.data); });
    array.def("__repr__", [](const QueryArray &array) {
        py::list list;
        for (auto const &value : array.data) {
            auto obj = py::cast(value);
            list.append(obj);
        }
        return py::str(list);
    });

    // we also take a list of query objects and make it into a query array
    array.def(py::init([](std::vector<std::shared_ptr<QueryObject>> values) {
                  auto r = std::make_shared<QueryArray>();
                  r->data = std::move(values);
                  return r;
              }),
              py::arg("values"));
}

void init_generic_query_object(py::module &m) {
    // as stated here:
    // https://pybind11.readthedocs.io/en/stable/classes.html#dynamic-attributes
    // we pay a little bit performance cost, which makes the performance the same as native
    // Python due to the usage or dynamic attributes
    auto obj = py::class_<GenericQueryObject, QueryObject, std::shared_ptr<GenericQueryObject>>(
        m, "GenericQueryObject", py::dynamic_attr());
    // custom repr logic to show all attributes
    obj.def("__repr__", [](const GenericQueryObject &obj) {
        py::dict dict;
        for (auto const &[name, value] : obj.attrs) {
            dict[name.c_str()] = value;
        }
        return py::str(dict);
    });
    // we allow a generic query object to be constructed from dict
    obj.def(py::init([](std::map<std::string, py::object> attrs) {
                auto r = std::make_shared<GenericQueryObject>();
                r->attrs = std::move(attrs);
                return r;
            }),
            py::arg("attributes"));

    // we translate our custom attribute error to standard attribute error
    py::register_exception<GenericAttributeError>(m, "GenericAttributeError", PyExc_AttributeError);
    py::register_exception_translator([](std::exception_ptr p) {  // NOLINT
        try {
            if (p) std::rethrow_exception(p);
        } catch (const GenericAttributeError &e) {
            PyErr_SetString(PyExc_AttributeError, e.what());
        }
    });
    // get attribute
    obj.def("__getattr__", [](const GenericQueryObject &obj, const std::string &name) {
        if (obj.attrs.find(name) == obj.attrs.end()) {
            // this will throw an error
            std::string error = "Object has no attribute '" + name + "'";
            throw GenericAttributeError(error);
        }
        return obj.attrs.at(name);
    });
}

void init_object(py::module &m) {
    init_query_object(m);
    init_query_array(m);
    init_generic_query_object(m);
}

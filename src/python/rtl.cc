#include "rtl.hh"

#include <fmt/format.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <slang/diagnostics/DiagnosticEngine.h>

#include <iostream>

#include "data_source.hh"
#include "object.hh"

namespace py = pybind11;

std::shared_ptr<QueryArray> create_instance_array(Ooze *ooze, hgdb::rtl::DesignDatabase &db) {
    auto result = std::make_shared<QueryArray>(ooze);
    auto const &instances = db.instances();
    result->data.reserve(instances.size());
    for (auto const *inst : instances) {
        result->data.emplace_back(std::make_shared<InstanceObject>(ooze, &db, inst));
    }
    return result;
}

std::shared_ptr<QueryArray> create_variable_array(Ooze *ooze, hgdb::rtl::DesignDatabase &db) {
    auto result = std::make_shared<QueryArray>(ooze);
    auto const &variables = db.variables();
    result->data.reserve(variables.size());
    for (auto const *v : variables) {
        result->data.emplace_back(std::make_shared<VariableObject>(ooze, &db, v));
    }
    return result;
}

std::shared_ptr<QueryArray> create_port_array(Ooze *ooze, hgdb::rtl::DesignDatabase &db) {
    auto result = std::make_shared<QueryArray>(ooze);
    auto const &ports = db.ports();
    result->data.reserve(ports.size());
    for (auto const *p : ports) {
        result->data.emplace_back(std::make_shared<PortObject>(ooze, &db, p));
    }
    return result;
}

std::map<std::string, py::object> InstanceObject::values() const {
    std::string path;
    instance->getHierarchicalPath(path);
    return {{"name", py::cast(instance->name)},
            {"path", py::cast(path)},
            {"definition", py::cast(instance->body.name)}};
}

std::map<std::string, py::object> VariableObject::values() const {
    std::string path;
    variable->getHierarchicalPath(path);
    return {{"name", py::cast(variable->name)}, {"path", py::cast(path)}};
}

std::map<std::string, py::object> PortObject::values() const {
    std::string path;
    port->getHierarchicalPath(path);
    return {{"name", py::cast(port->name)}, {"path", py::cast(path)}};
}

void RTL::compile() {
    bool has_error = false;
    source_manager_ = std::make_unique<slang::SourceManager>();
    for (const std::string &dir : include_dirs) {
        try {
            source_manager_->addUserDirectory(string_view(dir));
        } catch (const std::exception &) {
            throw std::runtime_error(fmt::format("include directory {0} does not exist", dir));
        }
    }

    for (const std::string &dir : include_sys_dirs_) {
        try {
            source_manager_->addSystemDirectory(string_view(dir));
        } catch (const std::exception &) {
            throw std::runtime_error(fmt::format("include directory {0} does not exist", dir));
        }
    }
    slang::PreprocessorOptions preprocessor_options;
    // compute defines
    std::vector<std::string> defines;
    defines.reserve(macros_.size());
    for (auto const &[name, value] : macros_) {
        defines.emplace_back(fmt::format("{0}={1}", name, value.empty() ? "1" : value));
    }
    preprocessor_options.predefines = defines;
    preprocessor_options.predefineSource = "<command-line>";

    slang::LexerOptions lexer_options;
    slang::ParserOptions parser_options;
    slang::CompilationOptions compilation_options;
    compilation_options.suppressUnused = true;
    compilation_options.disableInstanceCaching = true;
    if (!top_.empty()) {
        compilation_options.topModules.emplace(top_);
    }

    slang::Bag options;
    options.set(preprocessor_options);
    options.set(lexer_options);
    options.set(parser_options);
    options.set(compilation_options);

    std::vector<slang::SourceBuffer> buffers;
    for (const std::string &file : files_) {
        slang::SourceBuffer buffer = source_manager_->readSource(file);
        if (!buffer) {
            has_error = true;
            continue;
        }

        buffers.push_back(buffer);
    }

    compilation_ = std::make_unique<slang::Compilation>(options);
    for (const slang::SourceBuffer &buffer : buffers)
        compilation_->addSyntaxTree(
            slang::SyntaxTree::fromBuffer(buffer, *source_manager_, options));
    slang::DiagnosticEngine diag_engine(*source_manager_);
    // issuing all diagnosis
    for (auto const &diag : compilation_->getAllDiagnostics()) diag_engine.issue(diag);
    if (!has_error) {
        has_error = diag_engine.getNumErrors() != 0;
    }
    // load the source
    if (has_error) {
        std::cerr << "Error when parsing RTL files. Design database is incomplete" << std::endl;
    }
}

std::shared_ptr<QueryArray> RTL::get_selector(py::handle handle) {
    // based on what type it is
    if (handle.is(py::type::of<InstanceObject>())) {
        // create instance selector
        return create_instance_array(ooze_, *db_);
    } else if (handle.is(py::type::of<VariableObject>())) {
        return create_variable_array(ooze_, *db_);
    } else if (handle.is(py::type::of<PortObject>())) {
        return create_port_array(ooze_, *db_);
    }
    return nullptr;
}

void RTL::on_added(Ooze *ooze) {
    compile();
    db_ = std::make_unique<hgdb::rtl::DesignDatabase>(*compilation_);
    ooze_ = ooze;
}

std::shared_ptr<QueryObject> RTL::bind(const std::shared_ptr<QueryObject> &obj,
                                       const py::object &type) {
    // based on which type it asks for, we create a new instances
    auto const &py_obj = py::cast(obj);
    if (!py::hasattr(py_obj, "path")) return nullptr;
    auto const &py_path = py_obj.attr("path");
    auto const &path = py_path.cast<std::string>();
    auto const &symbol = db_->select(path);
    if (!symbol) return nullptr;

    if (type.is(py::type::of<InstanceObject>())) {
        // need to bind based on the path
        if (!slang::InstanceSymbol::isKind(symbol->kind)) {
            return nullptr;
        }
        auto const &inst = symbol->as<slang::InstanceSymbol>();
        return std::make_shared<InstanceObject>(ooze_, db_.get(), &inst);
    } else if (type.is(py::type::of<VariableObject>())) {
        if (!slang::VariableSymbol::isKind(symbol->kind)) {
            return nullptr;
        }
        auto const &v = symbol->as<slang::VariableSymbol>();
        return std::make_shared<VariableObject>(ooze_, db_.get(), &v);
    } else if (type.is(py::type::of<PortObject>())) {
        auto const *port = db_->get_port(symbol);
        if (!port) {
            return nullptr;
        }
        return std::make_shared<PortObject>(ooze_, db_.get(), port);
    }

    return nullptr;
}

template <typename T>
std::unique_ptr<InstanceObject> get_parent_instance(Ooze *ooze, const T &obj) {
    const slang::InstanceSymbol *inst = nullptr;
    if constexpr (std::is_base_of<slang::ValueSymbol, T>::value) {
        inst = obj.db->get_parent_instance(obj.variable);

    } else if constexpr (std::is_same<slang::InstanceSymbol, T>::value) {
        inst = obj.db->get_parent_instance(obj.instance);
    }

    if (!inst) return nullptr;
    auto ptr = std::make_unique<InstanceObject>(ooze, obj.db, inst);
    return ptr;
}

void init_instance_object(py::module &m) {
    auto cls =
        py::class_<InstanceObject, RTLQueryObject, std::shared_ptr<InstanceObject>>(m, "Instance");
    // we don't allow users to construct it by themself
    cls.def_property_readonly("name", [](const InstanceObject &obj) { return obj.instance->name; })
        .def_property_readonly(
            "definition",
            [](const std::shared_ptr<InstanceObject> &obj) { return obj->instance->body.name; })
        .def_property_readonly(
            "parent", [](const InstanceObject &obj) { return get_parent_instance(obj.ooze, obj); });
}

void init_variable_object(py::module &m) {
    auto cls =
        py::class_<VariableObject, RTLQueryObject, std::shared_ptr<VariableObject>>(m, "Variable");
    cls.def_property_readonly("name", [](const VariableObject &obj) { return obj.variable->name; })
        .def_property_readonly("instance",
                               [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent_instance(obj.ooze, obj);
                               })
        .def_property_readonly("parent",
                               [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent_instance(obj.ooze, obj);
                               });
}

void init_port_object(py::module &m) {
    auto cls = py::class_<PortObject, VariableObject, std::shared_ptr<PortObject>>(m, "Port");
    cls.def_property_readonly("direction", [](const PortObject &port) -> std::string {
        switch (port.port->direction) {
            case (slang::ArgumentDirection::In):
                return "in";
            case (slang::ArgumentDirection::Out):
                return "out";
            case (slang::ArgumentDirection::InOut):
                return "inout";
            case (slang::ArgumentDirection::Ref):
                return "ref";
        }
        throw std::runtime_error("Unknown port direction");
    });
}

void init_rtl_object(py::module &m) {
    auto cls = py::class_<RTLQueryObject, QueryObject, std::shared_ptr<RTLQueryObject>>(
        m, "RTLQueryObject");
    cls.def_property_readonly("path", [](const RTLQueryObject &obj) {
        std::string name;
        obj.symbol->getHierarchicalPath(name);
        return name;
    });
}

bool inside_instance(const std::shared_ptr<RTLQueryObject> &obj,
                     const std::shared_ptr<QueryObject> &parent) {
    if (parent->is_array()) {
        auto const &array = std::reinterpret_pointer_cast<QueryArray>(parent);
        for (auto const &entry : array->data) {
            if (inside_instance(obj, entry)) return true;
        }
    }
    auto const &inst = std::dynamic_pointer_cast<InstanceObject>(parent);
    if (!inst) return false;
    auto const *instance = inst->instance;
    switch (obj->kind) {
        case RTLQueryObject::RTLKind::Instance: {
            auto const &child = std::reinterpret_pointer_cast<InstanceObject>(obj);
            return hgdb::rtl::DesignDatabase::symbol_inside(child->instance, instance);
        }
        case RTLQueryObject::RTLKind::Variable:
        case RTLQueryObject::RTLKind::Port: {
            auto const &child = std::reinterpret_pointer_cast<VariableObject>(obj);
            return hgdb::rtl::DesignDatabase::symbol_inside(child->variable, instance);
        }
    }
    return false;
}

std::shared_ptr<QueryObject> get_source(const std::shared_ptr<QueryObject> &target) {
    if (target->is_array()) {
        auto const &array = std::reinterpret_pointer_cast<QueryArray>(target);
        auto result = std::make_shared<QueryArray>(target->ooze);
        for (auto const &entry : array->data) {
            auto mapped = get_source(entry);
            if (mapped) {
                result->add(mapped);
            }
        }
        return flatten_size_one_array(result);
    } else {
        auto rtl_obj = std::dynamic_pointer_cast<RTLQueryObject>(target);
        if (!rtl_obj) return nullptr;
        // notice that variable can be upgraded to port
        const slang::PortSymbol *port_symbol = hgdb::rtl::DesignDatabase::get_port(rtl_obj->symbol);
        if (!port_symbol) {
            return nullptr;
        }
        auto sources = rtl_obj->db->get_source_instances(port_symbol);
        if (sources.empty()) {
            return nullptr;
        } else if (sources.size() == 1) {
            auto const *inst = *sources.begin();
            return std::make_shared<InstanceObject>(target->ooze, rtl_obj->db, inst);
        } else {
            auto result = std::make_shared<QueryArray>(target->ooze);
            for (auto const *inst : sources) {
                return std::make_shared<InstanceObject>(target->ooze, rtl_obj->db, inst);
            }
            return result;
        }
    }
}

bool get_source_of_helper(const std::shared_ptr<QueryObject> &target,
                          const std::shared_ptr<QueryObject> &obj) {
    // recursive helper
    if (target->is_array()) {
        auto const &array = std::reinterpret_pointer_cast<QueryArray>(target);
        for (auto const &entry : array->data) {
            auto r = get_source_of_helper(target, entry);
            if (r) return true;
        }
    } else {
        // we will try to upgrade the symbol to port if possible
        auto target_symbol = std::dynamic_pointer_cast<VariableObject>(target);
        if (!target_symbol) return false;
        auto const *port = hgdb::rtl::DesignDatabase::get_port(target_symbol->symbol);
        if (!port) return false;
        auto obj_inst = std::dynamic_pointer_cast<InstanceObject>(obj);
        if (!obj_inst) return false;
        auto sources = target_symbol->db->get_source_instances(port);
        return sources.find(obj_inst->instance) != sources.end();
    }
    return false;
}

std::function<bool(const std::shared_ptr<QueryObject> &)> get_source_of(
    const std::shared_ptr<QueryObject> &target) {
    auto result = [=](const std::shared_ptr<QueryObject> &obj) -> bool {
        return get_source_of_helper(target, obj);
    };
    return result;
}

void init_helper_functions(py::module &m) {
    m.def(
        "inside",
        [](const std::shared_ptr<QueryObject> &parent) {
            // need to explicitly spell out the type, otherwise pybind will fail to convert
            std::function<bool(const std::shared_ptr<RTLQueryObject> &)> func =
                [=](const std::shared_ptr<RTLQueryObject> &obj) {
                    return inside_instance(obj, parent);
                };
            return func;
        },
        py::arg("parent"));

    m.def("source", &get_source, py::arg("target"));

    m.def(
        "source_of",
        [](const std::shared_ptr<QueryObject> &target) { return get_source_of(target); },
        py::arg("target"));
}

void init_rtl(py::module &m) {
    py::class_<RTL, DataSource, std::shared_ptr<RTL>>(m, "RTL")
        .def(py::init<>())
        .def("add_include_dir", &RTL::add_include_dir, py::arg("path"))
        .def("add_system_include_dir", &RTL::add_system_include_dir, py::arg("path"))
        .def("add_file", &RTL::add_file, py::arg("path"))
        .def("add_macro", &RTL::add_macro, py::arg("name"), py::arg("value"))
        .def("set_top", &RTL::set_top, py::arg("top_name"));

    init_rtl_object(m);
    init_instance_object(m);
    init_variable_object(m);
    init_port_object(m);

    init_helper_functions(m);
}

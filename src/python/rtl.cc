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

std::shared_ptr<QueryArray> create_instance_array(hgdb::rtl::DesignDatabase &db) {
    auto result = std::make_shared<QueryArray>();
    auto const &instances = db.instances();
    result->data.reserve(instances.size());
    for (auto const *inst : instances) {
        result->data.emplace_back(std::make_shared<InstanceObject>(&db, inst));
    }
    return result;
}

std::shared_ptr<QueryArray> create_variable_array(hgdb::rtl::DesignDatabase &db) {
    auto result = std::make_shared<QueryArray>();
    auto const &variables = db.variables();
    result->data.reserve(variables.size());
    for (auto const *v : variables) {
        result->data.emplace_back(std::make_shared<VariableObject>(&db, v));
    }
    return result;
}

std::shared_ptr<QueryArray> create_port_array(hgdb::rtl::DesignDatabase &db) {
    auto result = std::make_shared<QueryArray>();
    auto const &ports = db.ports();
    result->data.reserve(ports.size());
    for (auto const *p : ports) {
        result->data.emplace_back(std::make_shared<PortObject>(&db, p));
    }
    return result;
}

std::map<std::string, std::string> InstanceObject::values() const {
    std::string path;
    instance->getHierarchicalPath(path);
    auto def_name = std::string(instance->body.name);
    return {{"name", std::string(instance->name)},
            {"path", path},
            {"definition", def_name}};
}

std::map<std::string, std::string> VariableObject::values() const {
    std::string path;
    variable->getHierarchicalPath(path);
    return {{"name", std::string(variable->name)}, {"path", path}};
}

std::map<std::string, std::string> PortObject::values() const {
    std::string path;
    port->getHierarchicalPath(path);
    return {{"name", std::string(port->name)}, {"path", path}};
}

std::unique_ptr<slang::Compilation> RTL::compile() const {
    bool has_error = false;
    slang::SourceManager source_manager;
    for (const std::string &dir : include_dirs) {
        try {
            source_manager.addUserDirectory(string_view(dir));
        } catch (const std::exception &) {
            throw std::runtime_error(fmt::format("include directory {0} does not exist", dir));
        }
    }

    for (const std::string &dir : include_sys_dirs_) {
        try {
            source_manager.addSystemDirectory(string_view(dir));
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
        slang::SourceBuffer buffer = source_manager.readSource(file);
        if (!buffer) {
            has_error = true;
            continue;
        }

        buffers.push_back(buffer);
    }

    auto compilation = std::make_unique<slang::Compilation>(options);
    for (const slang::SourceBuffer &buffer : buffers)
        compilation->addSyntaxTree(slang::SyntaxTree::fromBuffer(buffer, source_manager, options));
    slang::DiagnosticEngine diag_engine(source_manager);
    // issuing all diagnosis
    for (auto const &diag : compilation->getAllDiagnostics()) diag_engine.issue(diag);
    if (!has_error) {
        has_error = diag_engine.getNumErrors() != 0;
    }
    // load the source
    if (has_error) {
        std::cerr << "Error when parsing RTL files. Design database is incomplete" << std::endl;
    }

    return std::move(compilation);
}

std::shared_ptr<QueryArray> RTL::get_selector(py::handle handle) {
    // based on what type it is
    if (handle.is(py::type::of<InstanceObject>())) {
        // create instance selector
        return create_instance_array(*db_);
    } else if (handle.is(py::type::of<VariableObject>())) {
        return create_variable_array(*db_);
    } else if (handle.is(py::type::of<PortObject>())) {
        return create_port_array(*db_);
    }
    return nullptr;
}

void RTL::on_added(Ooze *) {
    compilation_ = compile();
    db_ = std::make_unique<hgdb::rtl::DesignDatabase>(*compilation_);
}

template <typename T>
std::unique_ptr<InstanceObject> get_parent_instance(const T &obj) {
    const slang::InstanceSymbol *inst = nullptr;
    if constexpr (std::is_base_of<slang::ValueSymbol, T>::value) {
        inst = obj.db->get_parent_instance(obj.variable);

    } else if constexpr (std::is_same<slang::InstanceSymbol, T>::value) {
        inst = obj.db->get_parent_instance(obj.instance);
    }

    if (!inst) return nullptr;
    auto ptr = std::make_unique<InstanceObject>(obj.db, inst);
    return std::move(ptr);
}

void init_instance_object(py::module &m) {
    auto cls =
        py::class_<InstanceObject, RTLQueryObject, std::shared_ptr<InstanceObject>>(m, "Instance");
    // we don't allow users to construct it by themself
    cls.def_property_readonly(
           "name", [](const InstanceObject &obj) { return std::string(obj.instance->name); })
        .def_property_readonly("path",
                               [](const InstanceObject &obj) {
                                   std::string name;
                                   obj.instance->getHierarchicalPath(name);
                                   return name;
                               })
        .def("__eq__",
             [](const InstanceObject &obj, const RTLQueryObject &other) {
                 auto const *o = dynamic_cast<const InstanceObject *>(&other);
                 if (o) {
                     return o->instance == obj.instance;
                 }
                 return false;
             })
        .def("__hash__",
             [](const InstanceObject &obj) {
                 return
                     // we use the instance addr as hash value, which should be consistent with the
                     // __eq__ implementation
                     (uint64_t)obj.instance;
             })
        .def_property_readonly(
            "definition",
            [](const InstanceObject &obj) { return obj.instance->getDefinition().name; })
        .def_property_readonly("parent",
                               [](const InstanceObject &obj) { return get_parent_instance(obj); });
}

void init_variable_object(py::module &m) {
    auto cls =
        py::class_<VariableObject, RTLQueryObject, std::shared_ptr<VariableObject>>(m, "Variable");
    cls.def_property_readonly("name", [](const VariableObject &obj) { return obj.variable->name; })
        .def_property_readonly("instance",
                               [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent_instance(obj);
                               })
        .def_property_readonly("parent",
                               [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent_instance(obj);
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
}

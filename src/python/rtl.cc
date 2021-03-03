#include <fmt/format.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <slang/diagnostics/DiagnosticEngine.h>

#include <iostream>

#include "data_source.hh"
#include "object.hh"

namespace py = pybind11;

std::unique_ptr<slang::Compilation> RTL::compile() const {
    bool has_error = false;
    slang::SourceManager source_manager;
    for (const std::string& dir : include_dirs) {
        try {
            source_manager.addUserDirectory(string_view(dir));
        } catch (const std::exception&) {
            throw std::runtime_error(fmt::format("include directory {0} does not exist", dir));
        }
    }

    for (const std::string& dir : include_sys_dirs_) {
        try {
            source_manager.addSystemDirectory(string_view(dir));
        } catch (const std::exception&) {
            throw std::runtime_error(fmt::format("include directory {0} does not exist", dir));
        }
    }
    slang::PreprocessorOptions preprocessor_options;
    // compute defines
    std::vector<std::string> defines;
    defines.reserve(macros_.size());
    for (auto const& [name, value] : macros_) {
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
    for (const std::string& file : files_) {
        slang::SourceBuffer buffer = source_manager.readSource(file);
        if (!buffer) {
            has_error = true;
            continue;
        }

        buffers.push_back(buffer);
    }

    auto compilation = std::make_unique<slang::Compilation>(options);
    for (const slang::SourceBuffer& buffer : buffers)
        compilation->addSyntaxTree(slang::SyntaxTree::fromBuffer(buffer, source_manager, options));
    slang::DiagnosticEngine diag_engine(source_manager);
    // issuing all diagnosis
    for (auto const& diag : compilation->getAllDiagnostics()) diag_engine.issue(diag);
    if (!has_error) {
        has_error = diag_engine.getNumErrors() != 0;
    }
    // load the source
    if (has_error) {
        std::cerr << "Error when parsing RTL files. Design database is incomplete" << std::endl;
    }

    return std::move(compilation);
}

std::unique_ptr<Selector> RTL::get_selector(py::handle handle) {
    return std::unique_ptr<Selector>();
}

void init_rtl(py::module& m) {
    py::class_<RTL, DataSource, std::shared_ptr<RTL>>(m, "RTL")
        .def(py::init<>())
        .def("add_include_dir", &RTL::add_include_dir, py::arg("path"))
        .def("add_system_include_dir", &RTL::add_system_include_dir, py::arg("path"))
        .def("add_file", &RTL::add_file, py::arg("path"))
        .def("add_macro", &RTL::add_macro, py::arg("name"), py::arg("value"))
        .def("set_top", &RTL::set_top, py::arg("top_name"));
}
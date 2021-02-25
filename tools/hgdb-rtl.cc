//------------------------------------------------------------------------------
// driver.cpp
// Entry point for the primary driver program.
//
// File is under the MIT license; see LICENSE for details
//------------------------------------------------------------------------------

#include <iostream>

#include "fmt/color.h"
#include "slang/compilation/Compilation.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/ASTSerializer.h"
#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/Json.h"
#include "slang/text/SourceManager.h"
#include "slang/util/CommandLine.h"
#include "slang/util/OS.h"
#include "slang/util/String.h"
#include "slang/util/Version.h"

using namespace slang;

static constexpr auto errorColor = fmt::terminal_color::bright_red;

bool runPreprocessor(SourceManager& sourceManager, const Bag& options,
                     const std::vector<SourceBuffer>& buffers, bool includeComments,
                     bool includeDirectives) {
    BumpAllocator alloc;
    Diagnostics diagnostics;
    Preprocessor preprocessor(sourceManager, alloc, diagnostics, options);

    for (auto it = buffers.rbegin(); it != buffers.rend(); it++) preprocessor.pushSource(*it);

    SyntaxPrinter output;
    output.setIncludeComments(includeComments);
    output.setIncludeDirectives(includeDirectives);

    while (true) {
        Token token = preprocessor.next();
        output.print(token);
        if (token.kind == TokenKind::EndOfFile) break;
    }

    OS::print("{}\n", output.str());
    return true;
}

void printMacros(SourceManager& sourceManager, const Bag& options,
                 const std::vector<SourceBuffer>& buffers) {
    BumpAllocator alloc;
    Diagnostics diagnostics;
    Preprocessor preprocessor(sourceManager, alloc, diagnostics, options);

    for (auto it = buffers.rbegin(); it != buffers.rend(); it++) preprocessor.pushSource(*it);

    while (true) {
        Token token = preprocessor.next();
        if (token.kind == TokenKind::EndOfFile) break;
    }

    for (const auto *macro : preprocessor.getDefinedMacros()) {
        SyntaxPrinter printer;
        printer.setIncludeComments(false);
        printer.setIncludeTrivia(false);
        printer.print(macro->name);

        printer.setIncludeTrivia(true);
        if (macro->formalArguments) printer.print(*macro->formalArguments);
        printer.print(macro->body);

        OS::print("{}\n", printer.str());
    }
}

SourceBuffer readSource(SourceManager& sourceManager, const std::string& file) {
    SourceBuffer buffer = sourceManager.readSource(widen(file));
    if (!buffer) {
        OS::printE(fg(errorColor), "error: ");
        OS::printE("no such file or directory: '{}'\n", file);
    }
    return buffer;
}

bool loadAllSources(Compilation& compilation, SourceManager& sourceManager,
                    const std::vector<SourceBuffer>& buffers, const Bag& options, bool singleUnit,
                    const std::vector<std::string>& libraryFiles,
                    const std::vector<std::string>& libDirs,
                    const std::vector<std::string>& libExts) {
    if (singleUnit) {
        compilation.addSyntaxTree(SyntaxTree::fromBuffers(buffers, sourceManager, options));
    } else {
        for (const SourceBuffer& buffer : buffers)
            compilation.addSyntaxTree(SyntaxTree::fromBuffer(buffer, sourceManager, options));
    }

    bool ok = true;
    for (const auto& file : libraryFiles) {
        SourceBuffer buffer = readSource(sourceManager, file);
        if (!buffer) {
            ok = false;
            continue;
        }

        auto tree = SyntaxTree::fromBuffer(buffer, sourceManager, options);
        tree->isLibrary = true;
        compilation.addSyntaxTree(tree);
    }

    if (libDirs.empty()) return ok;

    std::vector<fs::path> directories;
    directories.reserve(libDirs.size());
    for (const auto& dir : libDirs) directories.emplace_back(widen(dir));

    flat_hash_set<string_view> uniqueExtensions;
    uniqueExtensions.emplace(".v"sv);
    uniqueExtensions.emplace(".sv"sv);
    for (const auto& ext : libExts) uniqueExtensions.emplace(ext);

    std::vector<fs::path> extensions;
    for (auto ext : uniqueExtensions) extensions.emplace_back(widen(ext));

    // If library directories are specified, see if we have any unknown instantiations
    // for which we should search for additional source files to load.
    flat_hash_set<string_view> definitionNames;
    auto addDefNames = [&](const std::shared_ptr<SyntaxTree>& tree) {
        for (const auto& [n, meta] : tree->getMetadata().nodeMap) {
            const auto *decl = &n->as<ModuleDeclarationSyntax>();
            string_view name = decl->header->name.valueText();
            if (!name.empty()) definitionNames.emplace(name);
        }
    };

    for (const auto& tree : compilation.getSyntaxTrees()) addDefNames(tree);

    flat_hash_set<string_view> missingNames;
    for (const auto& tree : compilation.getSyntaxTrees()) {
        for (auto name : tree->getMetadata().globalInstances) {
            if (definitionNames.find(name) == definitionNames.end()) missingNames.emplace(name);
        }
    }

    // Keep loading new files as long as we are making forward progress.
    flat_hash_set<string_view> nextMissingNames;
    while (true) {
        for (auto name : missingNames) {
            SourceBuffer buffer;
            for (auto& dir : directories) {
                fs::path path(dir);
                path /= name;

                for (auto& ext : extensions) {
                    path.replace_extension(ext);
                    if (!sourceManager.isCached(path)) {
                        buffer = sourceManager.readSource(path);
                        if (buffer) break;
                    }
                }

                if (buffer) break;
            }

            if (buffer) {
                auto tree = SyntaxTree::fromBuffer(buffer, sourceManager, options);
                tree->isLibrary = true;
                compilation.addSyntaxTree(tree);

                addDefNames(tree);
                for (auto instName : tree->getMetadata().globalInstances) {
                    if (definitionNames.find(instName) == definitionNames.end())
                        nextMissingNames.emplace(instName);
                }
            }
        }

        if (nextMissingNames.empty()) break;

        missingNames = nextMissingNames;
    }

    return ok;
}

template <typename TArgs>
int driverMain(int argc, TArgs argv, bool suppressColorsStdout, bool suppressColorsStderr) try {
    CommandLine cmdLine;

    // General
    optional<bool> showHelp;
    optional<bool> showVersion;
    optional<bool> quiet;
    cmdLine.add("-h,--help", showHelp, "Display available options");
    cmdLine.add("--version", showVersion, "Display version information and exit");
    cmdLine.add("-q,--quiet", quiet, "Suppress non-essential output");

    // Output control
    optional<bool> onlyPreprocess;
    optional<bool> onlyParse;
    optional<bool> onlyMacros;
    cmdLine.add("-E,--preprocess", onlyPreprocess,
                "Only run the preprocessor (and print preprocessed files to stdout)");
    cmdLine.add("--macros-only", onlyMacros, "Print a list of found macros and exit");
    cmdLine.add("--parse-only", onlyParse,
                "Stop after parsing input files, don't perform elaboration or type checking");

    // Include paths
    std::vector<std::string> includeDirs;
    std::vector<std::string> includeSystemDirs;
    std::vector<std::string> libDirs;
    std::vector<std::string> libExts;
    cmdLine.add("-I,--include-directory", includeDirs, "Additional include search paths", "<dir>");
    cmdLine.add("--isystem", includeSystemDirs, "Additional system include search paths", "<dir>");
    cmdLine.add("-y,--libdir", libDirs,
                "Library search paths, which will be searched for missing modules", "<dir>");
    cmdLine.add("-Y,--libext", libExts, "Additional library file extensions to search", "<ext>");

    // Preprocessor
    optional<bool> includeComments;
    optional<bool> includeDirectives;
    optional<uint32_t> maxIncludeDepth;
    std::vector<std::string> defines;
    std::vector<std::string> undefines;
    cmdLine.add("-D,--define-macro", defines,
                "Define <macro> to <value> (or 1 if <value> ommitted) in all source files",
                "<macro>=<value>");
    cmdLine.add("-U,--undefine-macro", undefines,
                "Undefine macro name at the start of all source files", "<macro>");
    cmdLine.add("--comments", includeComments, "Include comments in preprocessed output (with -E)");
    cmdLine.add("--directives", includeDirectives,
                "Include compiler directives in preprocessed output (with -E)");
    cmdLine.add("--max-include-depth", maxIncludeDepth,
                "Maximum depth of nested include files allowed", "<depth>");

    // Parsing
    optional<uint32_t> maxParseDepth;
    optional<uint32_t> maxLexerErrors;
    cmdLine.add("--max-parse-depth", maxParseDepth,
                "Maximum depth of nested language constructs allowed", "<depth>");
    cmdLine.add("--max-lexer-errors", maxLexerErrors,
                "Maximum number of errors that can occur during lexing before the rest of the file "
                "is skipped",
                "<count>");

    // Compilation
    optional<uint32_t> maxInstanceDepth;
    optional<uint32_t> maxGenerateSteps;
    optional<uint32_t> maxConstexprDepth;
    optional<uint32_t> maxConstexprSteps;
    optional<uint32_t> maxConstexprBacktrace;
    optional<std::string> minTypMax;
    std::vector<std::string> topModules;
    std::vector<std::string> paramOverrides;
    cmdLine.add("--max-hierarchy-depth", maxInstanceDepth, "Maximum depth of the design hierarchy",
                "<depth>");
    cmdLine.add("--max-generate-steps", maxGenerateSteps,
                "Maximum number of steps that can occur during generate block "
                "evaluation before giving up",
                "<steps>");
    cmdLine.add("--max-constexpr-depth", maxConstexprDepth,
                "Maximum depth of a constant evaluation call stack", "<depth>");
    cmdLine.add("--max-constexpr-steps", maxConstexprSteps,
                "Maximum number of steps that can occur during constant "
                "evaluation before giving up",
                "<steps>");
    cmdLine.add("--constexpr-backtrace-limit", maxConstexprBacktrace,
                "Maximum number of frames to show when printing a constant evaluation "
                "backtrace; the rest will be abbreviated",
                "<limit>");
    cmdLine.add("-T,--timing", minTypMax,
                "Select which value to consider in min:typ:max expressions", "min|typ|max");
    cmdLine.add("--top", topModules,
                "One or more top-level modules to instantiate "
                "(instead of figuring it out automatically)",
                "<name>");
    cmdLine.add("-G", paramOverrides,
                "One or more parameter overrides to apply when"
                "instantiating top-level modules",
                "<name>=<value>");

    // File list
    optional<bool> singleUnit;
    std::vector<std::string> sourceFiles;
    cmdLine.add("--single-unit", singleUnit, "Treat all input files as a single compilation unit");
    cmdLine.setPositional(sourceFiles, "files");

    std::vector<std::string> libraryFiles;
    cmdLine.add("-v", libraryFiles,
                "One or more library files, which are separate compilation units "
                "where modules are not automatically instantiated.",
                "<filename>");

    if (!cmdLine.parse(argc, argv)) {
        for (const auto& err : cmdLine.getErrors()) OS::printE("{}\n", err);
        return 1;
    }

    if (showHelp == true) {
        OS::print("{}", cmdLine.getHelpText("slang SystemVerilog compiler"));
        return 0;
    }

    if (showVersion == true) {
        OS::print("slang version {}.{}.{}\n", VersionInfo::getMajor(), VersionInfo::getMinor(),
                  VersionInfo::getRevision());
        return 0;
    }

    bool showColors = !suppressColorsStderr && OS::fileSupportsColors(stderr);

    if (showColors) {
        OS::setStderrColorsEnabled(true);
        if (!suppressColorsStdout && OS::fileSupportsColors(stdout))
            OS::setStdoutColorsEnabled(true);
    }

    bool anyErrors = false;
    SourceManager sourceManager;
    for (const std::string& dir : includeDirs) {
        try {
            sourceManager.addUserDirectory(string_view(dir));
        } catch (const std::exception&) {
            OS::printE(fg(errorColor), "error: ");
            OS::printE("include directory '{}' does not exist\n", dir);
            anyErrors = true;
        }
    }

    for (const std::string& dir : includeSystemDirs) {
        try {
            sourceManager.addSystemDirectory(string_view(dir));
        } catch (const std::exception&) {
            OS::printE(fg(errorColor), "error: ");
            OS::printE("include directory '{}' does not exist\n", dir);
            anyErrors = true;
        }
    }

    PreprocessorOptions ppoptions;
    ppoptions.predefines = defines;
    ppoptions.undefines = undefines;
    ppoptions.predefineSource = "<command-line>";
    if (maxIncludeDepth.has_value()) ppoptions.maxIncludeDepth = *maxIncludeDepth;

    LexerOptions loptions;
    if (maxLexerErrors.has_value()) loptions.maxErrors = *maxLexerErrors;

    ParserOptions poptions;
    if (maxParseDepth.has_value()) poptions.maxRecursionDepth = *maxParseDepth;

    CompilationOptions coptions;
    coptions.suppressUnused = false;
    if (maxInstanceDepth.has_value()) coptions.maxInstanceDepth = *maxInstanceDepth;
    if (maxGenerateSteps.has_value()) coptions.maxGenerateSteps = *maxGenerateSteps;
    if (maxConstexprDepth.has_value()) coptions.maxConstexprDepth = *maxConstexprDepth;
    if (maxConstexprSteps.has_value()) coptions.maxConstexprSteps = *maxConstexprSteps;
    if (maxConstexprBacktrace.has_value()) coptions.maxConstexprBacktrace = *maxConstexprBacktrace;

    for (auto& name : topModules) coptions.topModules.emplace(name);
    for (auto& opt : paramOverrides) coptions.paramOverrides.emplace_back(opt);

    if (minTypMax.has_value()) {
        if (minTypMax == "min")
            coptions.minTypMax = MinTypMax::Min;
        else if (minTypMax == "typ")
            coptions.minTypMax = MinTypMax::Typ;
        else if (minTypMax == "max")
            coptions.minTypMax = MinTypMax::Max;
        else {
            OS::printE(fg(errorColor), "error: ");
            OS::printE("invalid value for timing option: '{}'", *minTypMax);
            return 1;
        }
    }

    Bag options;
    options.set(ppoptions);
    options.set(loptions);
    options.set(poptions);
    options.set(coptions);

    std::vector<SourceBuffer> buffers;
    for (const std::string& file : sourceFiles) {
        SourceBuffer buffer = readSource(sourceManager, file);
        if (!buffer) {
            anyErrors = true;
            continue;
        }

        buffers.push_back(buffer);
    }

    if (anyErrors) return 2;

    if (buffers.empty()) {
        OS::printE(fg(errorColor), "error: ");
        OS::printE("no input files\n");
        return 3;
    }

    if (onlyParse.has_value() + onlyPreprocess.has_value() + onlyMacros.has_value() > 1) {
        OS::printE(fg(errorColor), "error: ");
        OS::printE("can only specify one of --preprocess, --macros-only, --parse-only");
        return 4;
    }

    try {
        if (onlyPreprocess == true) {
            anyErrors = !runPreprocessor(sourceManager, options, buffers, includeComments == true,
                                         includeDirectives == true);
        } else if (onlyMacros == true) {
            printMacros(sourceManager, options, buffers);
        } else {
            Compilation compilation(options);
            anyErrors = !loadAllSources(compilation, sourceManager, buffers, options,
                                        singleUnit == true, libraryFiles, libDirs, libExts);
        }
    } catch (const std::exception& e) {
        OS::printE("internal compiler error: {}\n", e.what());
        return 4;
    }

    return anyErrors ? 1 : 0;
} catch (const std::exception& e) {
    OS::printE("{}\n", e.what());
    return 5;
}

int main(int argc, char** argv) { return driverMain(argc, argv, false, false); }

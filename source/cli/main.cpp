#include "qvm/common/Error.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/common/Version.hpp"
#include "qvm/compiler/Compiler.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/decompiler/Exporter.hpp"
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include "qvm/decompiler/StringExtractor.hpp"
#include "qvm/runtime/QvmRuntime.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

void printVersion() {
    std::cout << qvm::kAppName << " (q3vm CLI) version " << qvm::kVersion << "\n"
              << "Official Quake III Arena QVM Toolchain & Decompiler\n";
}

void printHelp() {
    printVersion();
    std::cout << R"(
Usage:
  q3vm <command> [options]

Commands:
  compile       Compile Quake III C source code into a QVM file
  run           Run compiled QVM bytecode with the embedded q3vm interpreter
  decompile     Analyze, disassemble and generate pseudocode for a QVM file
  info          Display QVM header and section details
  disassemble   Disassemble QVM bytecode into readable assembly
  pseudocode    Reconstruct C-like pseudocode from QVM bytecode
  export        Export both disassembly and reconstructed pseudocode

Options:
  --help, -h       Display this help information
  --version, -v    Display application version

Compile Options:
  --module <name>   Module type: qagame, cgame, ui, custom (default: qagame)
  --source <path>   Path to project source directory or single C source file
  --file <path>     Alias for --source (single C file or directory)
  --files <f1>...   Explicit list of selected source files (.c/.asm) to compile
  --output <dir>    Path to output directory (default: ./build)
  --name <filename> Custom output filename (e.g. mymod.qvm)
  --sdk <path>      Path to Quake III SDK root or code directory
  --toolchain <dir> Custom directory containing LCC/Q3ASM binaries
  -D<macro>         Define preprocessor macro (e.g. -DDEBUG -DQ3_VM)
  -I<path>          Add include search directory
  --debug           Include debug symbols (-Wf-g)
  --verbose         Enable verbose output from compiler and assembler
  --clean           Clean intermediate assembly files (.asm)

Run Options:
  --module <name>   Module type whose syscall table the QVM expects:
                    qagame, cgame, ui, custom (default: qagame)
  --command <n>     Argument passed to vmMain() (default: 0 = init)
  --trace          Log every system trap the bytecode performs

Disassemble / Pseudocode / Export Options:
  --output <path>     Output file path
  --assembly <path>   Assembly output file path (for export command)
  --pseudocode <path> Pseudocode output file path (for export command)
  --module <name>     Module type context (qagame, cgame, ui)

Examples:
  q3vm compile g_main.c --output ./build --name test.qvm
  q3vm compile --module qagame --source ./code/game --output ./build
  q3vm compile --file ./source/mymod.c -DDEBUG -I./include --output ./build
  q3vm run ./qagame.qvm
  q3vm run ./cgame.qvm --module cgame --trace
  q3vm info ./qagame.qvm
  q3vm disassemble ./qagame.qvm --output ./qagame.asm
  q3vm pseudocode ./qagame.qvm --output ./qagame.c
  q3vm export ./qagame.qvm --output ./build/out/
  q3vm compile --module qagame --source ./code/game --output ./build
  q3vm info ./qagame.qvm
  q3vm disassemble ./qagame.qvm --output ./qagame.asm
  q3vm pseudocode ./qagame.qvm --output ./qagame.c
  q3vm export ./qagame.qvm --assembly ./qagame.asm --pseudocode ./qagame.c
)";
}

int handleCompile(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

    if (args.empty()) {
        std::cerr << "Error: missing source operand for 'compile'.\n";
        return 1;
    }

    qvm::CompilerConfig cfg;
    cfg.module = qvm::ModuleType::QaGame;
    std::string src;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--module" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            cfg.module = qvm::stringToModuleType(args[++i]);
        } else if (a == "--source" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            src = args[++i];
        } else if (a == "--file" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            src = args[++i];
        } else if (a == "--files" && i + 1 < args.size() && !args[i + 1].starts_with("-")) {
            ++i;
            while (i < args.size() && !args[i].starts_with("-")) {
                cfg.selectedFiles.push_back(args[i]);
                ++i;
            }
            --i; // outer loop advances to the next option
        } else if (a == "--output" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            cfg.outputDir = args[++i];
        } else if (a == "--name" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            cfg.outputName = args[++i];
        } else if (a == "--sdk" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            cfg.sdkDir = args[++i];
        } else if (a == "--toolchain" && i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            cfg.toolchainDir = args[++i];
        } else if (a == "--debug") {
            cfg.debugSymbols = true;
        } else if (a == "--verbose") {
            cfg.verbose = true;
        } else if (a == "--clean") {
            cfg.cleanIntermediate = true;
        } else if (a == "-D" && i + 1 < args.size()) {
            cfg.preprocessorDefines.push_back(args[++i]);
        } else if (a == "-I" && i + 1 < args.size()) {
            cfg.includePaths.push_back(args[++i]);
        } else if (a.size() > 2 && a[0] == '-' && a[1] == 'D') {
            cfg.preprocessorDefines.push_back(a.substr(2));
        } else if (a.size() > 2 && a[0] == '-' && a[1] == 'I') {
            cfg.includePaths.push_back(a.substr(2));
        } else if (!src.empty()) {
            // trailing non-flag; treat as source
            src = a;
        } else if (src.empty()) {
            src = a;
        }
    }

    if (src.empty()) {
        std::cerr << "Error: compile requires a source path.\n";
        return 1;
    }
    cfg.sourceDir = src;

    std::cout << "Compiling " << qvm::moduleTypeToString(cfg.module)
              << " from " << cfg.sourceDir.string() << "...\n";

    auto progress = [](int current, int total, std::string_view msg) {
        std::cout << "[" << current << "/" << total << "] " << msg << "\n";
    };
    auto log = [](std::string_view line, bool isErr) {
        if (isErr) std::cerr << line << "\n";
        else std::cout << line << "\n";
    };

    auto res = qvm::Compiler::compile(cfg, progress, log);
    if (res.hasError()) {
        std::cerr << "\nBuild FAILED: " << res.error().message() << "\n";
        return 1;
    }

    const auto& report = res.value();
    std::cout << "\nBuild SUCCEEDED in " << report.durationSeconds << " seconds.\n"
              << "QVM Output: " << report.outputQvmPath.string() << " ("
              << report.qvmFileSize << " bytes)\n";
    return 0;
}

int handleInfo(const std::string& qvmFile) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }

    const auto& parsed = parsedRes.value();
    std::cout << parsed.header.formatInfo() << "\n";

    auto strings = qvm::StringExtractor::extractAll(parsed.sections);
    auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions);

    std::cout << "Analysis Summary:\n"
              << "  Decoded Instructions: " << parsed.instructions.size() << "\n"
              << "  Identified Functions: " << functions.size() << "\n"
              << "  Extracted Strings:    " << strings.size() << "\n";

    return 0;
}

int handleDisassemble(const std::string& qvmFile, const std::string& outFile, qvm::ModuleType modType) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }

    std::string disasm = qvm::Exporter::generateDisassembly(parsedRes.value(), modType);

    if (outFile.empty()) {
        std::cout << disasm;
    } else {
        if (auto err = qvm::FileUtils::writeFileText(outFile, disasm); err.isError()) {
            std::cerr << "Error writing disassembly: " << err.message() << "\n";
            return 1;
        }
        std::cout << "Disassembly written to " << outFile << "\n";
    }

    return 0;
}

int handlePseudocode(const std::string& qvmFile, const std::string& outFile, qvm::ModuleType modType) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }

    const auto& parsed = parsedRes.value();
    auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions);
    std::string pseudocode =
        qvm::PseudocodeGenerator::generate(parsed, functions, modType);

    if (outFile.empty()) {
        std::cout << pseudocode;
    } else {
        if (auto err = qvm::FileUtils::writeFileText(outFile, pseudocode); err.isError()) {
            std::cerr << "Error writing pseudocode: " << err.message() << "\n";
            return 1;
        }
        std::cout << "Pseudocode written to " << outFile << "\n";
    }

    return 0;
}

int handleExport(const std::string& qvmFile, const std::string& asmFile,
                const std::string& cFile, qvm::ModuleType modType) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }

    const auto& parsed = parsedRes.value();
    if (!asmFile.empty()) {
        std::string disasm = qvm::Exporter::generateDisassembly(parsed, modType);
        if (auto err = qvm::FileUtils::writeFileText(asmFile, disasm); err.isError()) {
            std::cerr << "Error writing disassembly: " << err.message() << "\n";
            return 1;
        }
        std::cout << "Disassembly written to " << asmFile << "\n";
    }
    if (!cFile.empty()) {
        auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions);
        std::string pseudocode =
            qvm::PseudocodeGenerator::generate(parsed, functions, modType);
        if (auto err = qvm::FileUtils::writeFileText(cFile, pseudocode); err.isError()) {
            std::cerr << "Error writing pseudocode: " << err.message() << "\n";
            return 1;
        }
        std::cout << "Pseudocode written to " << cFile << "\n";
    }

    return 0;
}

int handleDecompile(const std::string& qvmFile, qvm::ModuleType modType) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }

    const auto& parsed = parsedRes.value();
    const auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions);
    std::cout << qvm::Exporter::generateDisassembly(parsed, modType)
              << "\n\n"
              << qvm::PseudocodeGenerator::generate(parsed, functions, modType);
    return 0;
}

int handleRun(const std::string& qvmFile, int argc, char** argv) {
    qvm::RuntimeOptions opts;
    std::string moduleName;
    bool trace = false;
    int command = 0;

    for (int i = 3; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--module" && i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            moduleName = argv[++i];
        } else if (a == "--command" && i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            try {
                command = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Error: --command requires an integer value.\n";
                return 1;
            }
        } else if (a == "--trace") {
            trace = true;
        }
    }

    if (!moduleName.empty()) {
        opts.module = qvm::runtimeModuleFromString(moduleName);
    }
    opts.command = command;
    opts.traceSyscalls = trace;

    auto result = qvm::QvmRuntime::run(qvmFile, opts,
        [](std::string_view line, bool isError) {
            if (isError) {
                std::cerr << "QVM error: " << line << "\n";
            } else {
                std::cout << line << "\n";
            }
        });

    if (result.hasError()) {
        std::cerr << "Run FAILED: " << result.error().message() << "\n";
        return 1;
    }

    const auto& r = result.value();
    if (!r.success) {
        std::cerr << "QVM terminated with error: " << r.error << "\n";
        for (const auto& traceLine : r.trace) {
            std::cerr << "  " << traceLine << "\n";
        }
        return 2;
    }

    std::cout << "\n[VM exited] " << r.syscallCount
              << (r.trace.empty() ? " syscalls, <1 ms\n" : " syscalls, " + std::to_string((int)r.durationMs) + " ms\n");
    for (const auto& t : r.trace) {
        std::cout << "  " << t << "\n";
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string_view command = argv[1];

    if (command == "--help" || command == "-h" || command == "help") {
        printHelp();
        return 0;
    }

    if (command == "--version" || command == "-v" || command == "version") {
        printVersion();
        return 0;
    }

    if (command == "compile") {
        return handleCompile(argc, argv);
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Error: 'run' requires a QVM file operand.\n";
            return 1;
        }
        return handleRun(argv[2], argc, argv);
    }

    // Commands that operate on an existing QVM file.
    if (argc < 3) {
        std::cerr << "Error: missing required file operand for command '" << command
                  << "'.\n";
        return 1;
    }

    std::string qvmFile = argv[2];
    std::string outFile;
    std::string asmFile;
    std::string cFile;
    qvm::ModuleType modType = qvm::ModuleType::QaGame;

    for (int i = 3; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--output" && i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            outFile = argv[++i];
        } else if (a == "--assembly" && i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            asmFile = argv[++i];
        } else if (a == "--pseudocode" && i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            cFile = argv[++i];
        } else if (a == "--module" && i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            modType = qvm::stringToModuleType(argv[++i]);
        }
    }

    if (command == "info") {
        return handleInfo(qvmFile);
    }
    if (command == "disassemble") {
        return handleDisassemble(qvmFile, outFile, modType);
    }
    if (command == "pseudocode") {
        return handlePseudocode(qvmFile, outFile, modType);
    }
    if (command == "export") {
        // Keep the documented `export file.qvm --output directory/` shortcut
        // useful while retaining the explicit --assembly/--pseudocode form.
        if (!outFile.empty() && asmFile.empty() && cFile.empty()) {
            const std::filesystem::path outputDir(outFile);
            const std::string stem = std::filesystem::path(qvmFile).stem().string();
            asmFile = (outputDir / (stem + ".asm")).string();
            cFile = (outputDir / (stem + ".c")).string();
        }
        if (asmFile.empty() && cFile.empty()) {
            std::cerr << "Error: export requires --output, --assembly, or --pseudocode.\n";
            return 1;
        }
        return handleExport(qvmFile, asmFile, cFile, modType);
    }
    if (command == "decompile") {
        return handleDecompile(qvmFile, modType);
    }

    std::cerr << "Error: unknown command '" << command << "'. Run 'q3vm --help' for usage.\n";
    return 1;
}

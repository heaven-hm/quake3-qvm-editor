#include "qvm/common/Error.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/common/Version.hpp"
#include "qvm/compiler/Compiler.hpp"
#include "qvm/decompiler/Exporter.hpp"
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include "qvm/decompiler/StringExtractor.hpp"
#include <iostream>
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

Disassemble / Pseudocode / Export Options:
  --output <path>     Output file path
  --assembly <path>   Assembly output file path (for export command)
  --pseudocode <path> Pseudocode output file path (for export command)
  --module <name>     Module type context (qagame, cgame, ui)

Examples:
  q3vm compile g_main.c --output ./build --name test.qvm
  q3vm compile --module qagame --source ./code/game --output ./build
  q3vm compile --file ./source/mymod.c -DDEBUG -I./include --output ./build
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
    auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions, modType);
    std::string pseudocode = qvm::PseudocodeGenerator::generate(parsed, functions, modType);

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

int handleExport(const std::string& qvmFile, const std::string& asmFile, const std::string& cFile, qvm::ModuleType modType) {
    if (asmFile.empty() && cFile.empty()) {
        std::cerr << "Error: export command requires --assembly, --pseudocode, or --output options.\n";
        return 1;
    }

    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }

    if (auto err = qvm::Exporter::exportBoth(parsedRes.value(), asmFile, cFile, modType); err.isError()) {
        std::cerr << "Error exporting files: " << err.message() << "\n";
        return 1;
    }

    if (!asmFile.empty()) std::cout << "Assembly exported to: " << asmFile << "\n";
    if (!cFile.empty()) std::cout << "Pseudocode exported to: " << cFile << "\n";
    return 0;
}

int handleDecompile(const std::string& qvmFile, qvm::ModuleType modType) {
    handleInfo(qvmFile);
    std::filesystem::path p(qvmFile);
    auto base = p.parent_path() / p.stem();
    std::string asmOut = base.string() + ".asm";
    std::string cOut = base.string() + ".c";

    std::cout << "\nExporting disassembly and pseudocode...\n";
    return handleExport(qvmFile, asmOut, cOut, modType);
}

int handleCompile(const std::vector<std::string>& args) {
    qvm::CompilerConfig config;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--module" && i + 1 < args.size()) {
            config.module = qvm::stringToModuleType(args[++i]);
        } else if (args[i] == "--files" && i + 1 < args.size()) {
            while (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
                config.selectedFiles.push_back(args[++i]);
            }
        } else if ((args[i] == "--source" || args[i] == "--file") && i + 1 < args.size()) {
            config.sourceDir = args[++i];
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            config.outputDir = args[++i];
        } else if (args[i] == "--name" && i + 1 < args.size()) {
            config.outputName = args[++i];
        } else if (args[i] == "--sdk" && i + 1 < args.size()) {
            config.sdkDir = args[++i];
        } else if (args[i] == "--toolchain" && i + 1 < args.size()) {
            config.toolchainDir = args[++i];
        } else if (args[i] == "--debug") {
            config.debugSymbols = true;
        } else if (args[i] == "--verbose") {
            config.verbose = true;
        } else if (args[i] == "--clean") {
            config.cleanIntermediate = true;
        } else if (args[i].starts_with("-D")) {
            config.preprocessorDefines.push_back(args[i]);
        } else if ((args[i] == "--define" || args[i] == "--defines") && i + 1 < args.size()) {
            config.preprocessorDefines.push_back(args[++i]);
        } else if (args[i].starts_with("-I")) {
            config.includePaths.push_back(args[i].substr(2));
        } else if ((args[i] == "--include" || args[i] == "--includes") && i + 1 < args.size()) {
            config.includePaths.push_back(args[++i]);
        } else if (!args[i].starts_with("--") && config.sourceDir.empty()) {
            config.sourceDir = args[i];
        }
    }

    if (config.sourceDir.empty() && !config.selectedFiles.empty()) {
        config.sourceDir = config.selectedFiles[0].parent_path();
        if (config.sourceDir.empty()) {
            config.sourceDir = ".";
        }
    }

    if (config.sourceDir.empty()) {
        std::cerr << "Error: source path (directory or C file) is required for compilation.\n";
        return 1;
    }

    if (std::filesystem::is_regular_file(config.sourceDir) && config.sourceDir.extension() == ".c") {
        config.module = qvm::ModuleType::Custom;
    }

    std::cout << "Compiling " << qvm::moduleTypeToString(config.module)
              << " from " << config.sourceDir.string() << "...\n";

    auto progress = [](int current, int total, std::string_view msg) {
        std::cout << "[" << current << "/" << total << "] " << msg << "\n";
    };

    auto log = [](std::string_view line, bool isErr) {
        if (isErr) {
            std::cerr << line << "\n";
        } else {
            std::cout << line << "\n";
        }
    };

    auto res = qvm::Compiler::compile(config, progress, log);
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

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h" || command == "help") {
        printHelp();
        return 0;
    }

    if (command == "--version" || command == "-v" || command == "version") {
        printVersion();
        return 0;
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (command == "compile") {
        return handleCompile(args);
    }

    if (args.empty()) {
        std::cerr << "Error: missing required file operand for command '" << command << "'.\n";
        return 1;
    }

    std::string qvmFile = args[0];
    std::string outFile;
    std::string asmFile;
    std::string cFile;
    qvm::ModuleType modType = qvm::ModuleType::QaGame;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--output" && i + 1 < args.size() && !args[i+1].starts_with("--")) {
            outFile = args[++i];
        } else if (args[i] == "--assembly") {
            if (i + 1 < args.size() && !args[i+1].starts_with("--")) {
                asmFile = args[++i];
            } else {
                asmFile = (std::filesystem::path(qvmFile).parent_path() / (std::filesystem::path(qvmFile).stem().string() + ".asm")).string();
            }
        } else if (args[i] == "--pseudocode") {
            if (i + 1 < args.size() && !args[i+1].starts_with("--")) {
                cFile = args[++i];
            } else {
                cFile = (std::filesystem::path(qvmFile).parent_path() / (std::filesystem::path(qvmFile).stem().string() + ".c")).string();
            }
        } else if (args[i] == "--module" && i + 1 < args.size() && !args[i+1].starts_with("--")) {
            modType = qvm::stringToModuleType(args[++i]);
        }
    }

    if (command == "export" && asmFile.empty() && cFile.empty() && !outFile.empty()) {
        std::filesystem::path outP(outFile);
        std::filesystem::path qvmP(qvmFile);
        if (std::filesystem::is_directory(outP) || outP.extension().empty()) {
            std::filesystem::create_directories(outP);
            asmFile = (outP / (qvmP.stem().string() + ".asm")).string();
            cFile = (outP / (qvmP.stem().string() + ".c")).string();
        } else if (outP.extension() == ".asm") {
            asmFile = outFile;
        } else if (outP.extension() == ".c") {
            cFile = outFile;
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
        return handleExport(qvmFile, asmFile, cFile, modType);
    }
    if (command == "decompile") {
        return handleDecompile(qvmFile, modType);
    }

    std::cerr << "Error: unknown command '" << command << "'. Run 'q3vm --help' for usage.\n";
    return 1;
}

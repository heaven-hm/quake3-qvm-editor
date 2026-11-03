#include "qvm/compiler/BuildPipeline.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/format/QvmHeader.hpp"
#include <chrono>
#include <iostream>

namespace qvm {

Result<BuildReport> BuildPipeline::execute(
    const CompilerConfig& config,
    BuildProgressCallback progressCallback,
    BuildLogCallback logCallback) {

    auto startTime = std::chrono::high_resolution_clock::now();
    BuildReport report;

    // 1. Detect toolchain
    auto toolchainRes = ToolchainRunner::detectToolchain(config.toolchainDir);
    if (toolchainRes.hasError()) {
        return toolchainRes.error();
    }
    ToolchainRunner runner(toolchainRes.value());

    // 2. Validate source directory
    if (!std::filesystem::exists(config.sourceDir)) {
        return Error(ErrorCode::InvalidProjectDirectory,
                     "Source directory does not exist: " + config.sourceDir.string());
    }

    // 3. Setup output directory
    std::filesystem::path outDir = config.outputDir.empty() ?
        (config.sourceDir / "build") : config.outputDir;
    if (auto err = FileUtils::ensureDirectory(outDir); err.isError()) {
        return err;
    }

    std::filesystem::path intermediateDir = outDir / "intermediate";
    if (auto err = FileUtils::ensureDirectory(intermediateDir); err.isError()) {
        return err;
    }

    // 4. Resolve module files
    ModuleFileList moduleFiles = ModuleConfig::getModuleFileList(config.module);
    std::string baseName = config.outputName.empty() ? moduleFiles.defaultOutputBaseName : config.outputName;
    if (baseName.ends_with(".qvm")) {
        baseName = baseName.substr(0, baseName.size() - 4);
    }
    std::filesystem::path finalQvmPath = outDir / (baseName + ".qvm");
    report.outputQvmPath = finalQvmPath;

    // Resolve include directories
    std::vector<std::filesystem::path> includeDirs = config.includePaths;
    for (const auto& relInc : moduleFiles.defaultRelativeIncludes) {
        auto incPath = config.sourceDir / relInc;
        if (std::filesystem::exists(incPath)) {
            includeDirs.push_back(incPath);
        }
        // Also check if sourceDir is the parent repository (code/...)
        auto codeIncPath = config.sourceDir / "code" / relInc;
        if (std::filesystem::exists(codeIncPath)) {
            includeDirs.push_back(codeIncPath);
        }
    }
    includeDirs.push_back(config.sourceDir);

    // Resolve preprocessor defines
    std::vector<std::string> defines = moduleFiles.defaultDefines;
    for (const auto& def : config.preprocessorDefines) {
        defines.push_back(def);
    }

    // Gather C source files
    std::vector<std::filesystem::path> sourceCFiles;
    std::vector<std::filesystem::path> sourceAsmFiles;

    if (config.module == ModuleType::Custom) {
        // Collect all .c and .asm files in sourceDir
        for (const auto& entry : std::filesystem::recursive_directory_iterator(config.sourceDir)) {
            if (entry.is_regular_file()) {
                if (entry.path().extension() == ".c") {
                    sourceCFiles.push_back(entry.path());
                } else if (entry.path().extension() == ".asm") {
                    sourceAsmFiles.push_back(entry.path());
                }
            }
        }
        // Ensure entry-point source (g_main.c, cg_main.c, ui_main.c, main.c) is first
        auto mainIt = std::find_if(sourceCFiles.begin(), sourceCFiles.end(), [](const auto& p) {
            auto fn = p.filename().string();
            return fn == "g_main.c" || fn == "cg_main.c" || fn == "ui_main.c" || fn == "main.c";
        });
        if (mainIt != sourceCFiles.end() && mainIt != sourceCFiles.begin()) {
            std::rotate(sourceCFiles.begin(), mainIt, mainIt + 1);
        }
    } else {
        // Use official file order
        sourceCFiles = ModuleConfig::resolveSourceFiles(config.sourceDir, moduleFiles);
        // Also resolve syscall and asm files
        for (const auto& asmName : moduleFiles.asmFiles) {
            auto asmPath = config.sourceDir / asmName;
            if (!std::filesystem::exists(asmPath)) {
                // Check subdirectories
                for (const auto& sub : {"game", "cgame", "ui", "code/game", "code/cgame", "code/ui"}) {
                    auto subPath = config.sourceDir / sub / asmName;
                    if (std::filesystem::exists(subPath)) {
                        asmPath = subPath;
                        break;
                    }
                }
            }
            if (std::filesystem::exists(asmPath)) {
                sourceAsmFiles.push_back(asmPath);
            }
        }
    }

    // Filter to existing C files
    std::vector<std::filesystem::path> existingCFiles;
    for (const auto& cFile : sourceCFiles) {
        if (std::filesystem::exists(cFile)) {
            existingCFiles.push_back(cFile);
        } else {
            report.warnings.push_back("Source file not found (skipped): " + cFile.string());
        }
    }

    if (existingCFiles.empty()) {
        return Error(ErrorCode::InvalidProjectDirectory,
                     "No valid C source files found to compile in: " + config.sourceDir.string());
    }

    report.totalFiles = static_cast<int>(existingCFiles.size());
    std::vector<std::filesystem::path> generatedAsmFiles;

    // 5. Stage 1: Compile C files to ASM
    if (logCallback) {
        logCallback("=== Stage 1: Compiling C sources to assembly ===", false);
    }

    for (size_t i = 0; i < existingCFiles.size(); ++i) {
        const auto& cFile = existingCFiles[i];
        auto outAsm = intermediateDir / (cFile.stem().string() + ".asm");

        if (progressCallback) {
            std::string msg = "Compiling (" + std::to_string(i + 1) + "/" +
                              std::to_string(existingCFiles.size()) + "): " + cFile.filename().string();
            progressCallback(static_cast<int>(i), static_cast<int>(existingCFiles.size() + 1), msg);
        }

        auto res = runner.compileCFile(cFile, outAsm, includeDirs, defines, config.debugSymbols, logCallback);
        report.lccOutputs.push_back(res.combinedOutput);

        if (!res.success || !std::filesystem::exists(outAsm)) {
            report.errors.push_back("Compilation failed for " + cFile.filename().string() + ": " + res.combinedOutput);
            return Error(ErrorCode::CompilationFailed,
                         "Compilation failed for " + cFile.filename().string() + "\n" + res.combinedOutput);
        }

        generatedAsmFiles.push_back(outAsm);
        report.filesCompiled++;
    }

    // Add supporting asm files
    for (const auto& asmFile : sourceAsmFiles) {
        generatedAsmFiles.push_back(asmFile);
    }

    // 6. Stage 2: Assemble ASM files to QVM
    if (progressCallback) {
        progressCallback(static_cast<int>(existingCFiles.size()),
                         static_cast<int>(existingCFiles.size() + 1),
                         "Assembling QVM with Q3ASM...");
    }

    if (logCallback) {
        logCallback("=== Stage 2: Assembling QVM binary with Q3ASM ===", false);
    }

    auto asmRes = runner.assembleQvm(generatedAsmFiles, finalQvmPath, outDir, config.verbose, logCallback);
    report.q3asmOutput = asmRes.combinedOutput;

    if (!asmRes.success || !std::filesystem::exists(finalQvmPath)) {
        report.errors.push_back("Q3ASM assembly failed: " + asmRes.combinedOutput);
        return Error(ErrorCode::AssemblyFailed, "Assembly to QVM failed:\n" + asmRes.combinedOutput);
    }

    // 7. Validate generated QVM file
    auto qvmBytesRes = FileUtils::readFileBinary(finalQvmPath);
    if (qvmBytesRes.hasError()) {
        return qvmBytesRes.error();
    }

    auto headerRes = QvmHeader::parse(qvmBytesRes.value());
    if (headerRes.hasError()) {
        return headerRes.error();
    }

    if (auto valErr = headerRes.value().validate(qvmBytesRes.value().size()); valErr.isError()) {
        return valErr;
    }

    report.qvmFileSize = qvmBytesRes.value().size();
    report.success = true;

    // Optional cleanup
    if (config.cleanIntermediate) {
        std::error_code ec;
        std::filesystem::remove_all(intermediateDir, ec);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    report.durationSeconds = std::chrono::duration<double>(endTime - startTime).count();

    if (progressCallback) {
        progressCallback(static_cast<int>(existingCFiles.size() + 1),
                         static_cast<int>(existingCFiles.size() + 1),
                         "Compilation completed successfully!");
    }

    if (logCallback) {
        logCallback("=== QVM Generated Successfully: " + finalQvmPath.string() + " (" +
                    std::to_string(report.qvmFileSize) + " bytes) ===", false);
    }

    return report;
}

} // namespace qvm

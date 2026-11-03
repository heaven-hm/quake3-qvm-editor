#include "qvm/compiler/BuildPipeline.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/format/QvmHeader.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <set>

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

    bool isScratchBuild = (config.sourceDir.string().find("scratch") != std::string::npos);
    bool isSingleFile = std::filesystem::is_regular_file(config.sourceDir);
    std::filesystem::path intermediateDir = (isScratchBuild || isSingleFile) ?
        (std::filesystem::temp_directory_path() / "quake3_qvm_editor_scratch" / "intermediate") :
        (outDir / "intermediate");
    std::error_code ec;
    std::filesystem::remove_all(intermediateDir, ec);
    if (auto err = FileUtils::ensureDirectory(intermediateDir); err.isError()) {
        return err;
    }

    // 4. Resolve module files & enforce mapping
    // cgame:  source: mod.c, syscall: cg_syscalls.asm, output: cgame.qvm
    // qagame: source: mod.c, syscall: g_syscalls.asm,  output: qagame.qvm
    // ui:     source: mod.c, syscall: ui_syscalls.asm, output: ui.qvm
    CompilerConfig activeConfig = config;
    if (activeConfig.module == ModuleType::Custom) {
        if (!activeConfig.outputName.empty()) {
            auto deduced = ModuleConfig::deduceModuleFromOutputName(activeConfig.outputName);
            if (deduced != ModuleType::Custom) {
                activeConfig.module = deduced;
            }
        }
        // A single module source file (cgame.c, game.c, ui.c, ...) implies its module.
        if (activeConfig.module == ModuleType::Custom && isSingleFile &&
            activeConfig.sourceDir.extension() == ".c") {
            auto deduced = ModuleConfig::deduceModuleFromSourceOrSyscall(
                activeConfig.sourceDir.filename().string());
            if (deduced != ModuleType::Custom) {
                activeConfig.module = deduced;
            }
        }
    }

    auto mapping = ModuleConfig::getModuleMapping(activeConfig.module);
    ModuleFileList moduleFiles = ModuleConfig::getModuleFileList(activeConfig.module);

    // Module-aware single-file build:
    //   `q3vm compile <module>.c` resolves the whole module (cg_main.c + bg/q shared
    //   sources + syscalls) and treats the given file as the custom vmMain entry.
    //   The stock entry's vmMain is renamed and a tiny forwarded-entry stub is placed
    //   first so the QVM entry point (code offset 0) is correct.
    if (isSingleFile && activeConfig.sourceDir.extension() == ".c" &&
        activeConfig.module != ModuleType::Custom && activeConfig.entryOverrideFile.empty()) {
        auto parentDir = activeConfig.sourceDir.parent_path();
        if (parentDir.empty()) {
            parentDir = ".";
        }
        auto probe = ModuleConfig::resolveSourceFiles(parentDir, moduleFiles);
        bool moduleEntryFound = !probe.empty() && std::filesystem::exists(probe.front());
        if (moduleEntryFound) {
            activeConfig.entryOverrideFile = activeConfig.sourceDir;
            activeConfig.sourceDir = parentDir;
            if (activeConfig.outputDir.empty()) {
                outDir = parentDir / "build";
                if (auto dirErr = FileUtils::ensureDirectory(outDir); dirErr.isError()) {
                    return dirErr;
                }
            }
        }
    }

    std::string baseName = activeConfig.outputName.empty() ? mapping.outputFile : activeConfig.outputName;
    if (baseName.ends_with(".qvm")) {
        baseName = baseName.substr(0, baseName.size() - 4);
    }
    std::filesystem::path finalQvmPath = outDir / (baseName + ".qvm");
    report.outputQvmPath = finalQvmPath;

    // Resolve include directories
    std::vector<std::filesystem::path> includeDirs = activeConfig.includePaths;
    for (const auto& relInc : moduleFiles.defaultRelativeIncludes) {
        auto incPath = activeConfig.sourceDir / relInc;
        if (std::filesystem::exists(incPath)) {
            includeDirs.push_back(incPath);
        }
        // Also check if sourceDir is the parent repository (code/...)
        auto codeIncPath = activeConfig.sourceDir / "code" / relInc;
        if (std::filesystem::exists(codeIncPath)) {
            includeDirs.push_back(codeIncPath);
        }
    }
    includeDirs.push_back(activeConfig.sourceDir);
    includeDirs = FileUtils::resolveQuake3IncludeDirs(includeDirs, activeConfig.sdkDir);

    // Resolve preprocessor defines
    std::vector<std::string> defines = moduleFiles.defaultDefines;
    for (const auto& def : activeConfig.preprocessorDefines) {
        if (std::find(defines.begin(), defines.end(), def) == defines.end()) {
            defines.push_back(def);
        }
    }
    if (activeConfig.module == ModuleType::CGame) {
        if (std::find(defines.begin(), defines.end(), "-DCGAME") == defines.end()) {
            defines.push_back("-DCGAME");
        }
    }

    // Gather C source files
    std::vector<std::filesystem::path> sourceCFiles;
    std::vector<std::filesystem::path> sourceAsmFiles;
    // Optional per-file extra preprocessor defines (e.g. renaming a stock vmMain).
    std::map<std::filesystem::path, std::vector<std::string>> fileExtraDefines;
    auto canonPath = [](const std::filesystem::path& p) {
        return std::filesystem::absolute(p).lexically_normal();
    };

    if (!activeConfig.selectedFiles.empty()) {
        // Compile ONLY selected files and explicitly configured dependencies
        for (const auto& file : activeConfig.selectedFiles) {
            auto p = file;
            if (p.is_relative() && !activeConfig.sourceDir.empty()) {
                auto candidate = activeConfig.sourceDir / p;
                if (std::filesystem::exists(candidate)) {
                    p = candidate;
                }
            }
            if (p.extension() == ".c") {
                sourceCFiles.push_back(p);
            } else if (p.extension() == ".asm") {
                sourceAsmFiles.push_back(p);
            }
        }
        // Ensure entry-point source (mod.c, g_main.c, cg_main.c, ui_main.c, main.c) is first if present
        auto mainIt = std::find_if(sourceCFiles.begin(), sourceCFiles.end(), [](const auto& p) {
            auto fn = p.filename().string();
            return fn == "mod.c" || fn == "g_main.c" || fn == "cg_main.c" || fn == "ui_main.c" || fn == "main.c";
        });
        if (mainIt != sourceCFiles.end() && mainIt != sourceCFiles.begin()) {
            std::rotate(sourceCFiles.begin(), mainIt, mainIt + 1);
        }

        // If no explicit ASM was selected, resolve companion syscall ASM from sourceDir or include paths
        if (sourceAsmFiles.empty() && std::filesystem::is_directory(activeConfig.sourceDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(activeConfig.sourceDir)) {
                if (entry.is_regular_file()) {
                    auto fname = entry.path().filename().string();
                    if (fname == mapping.syscallFile) {
                        sourceAsmFiles.push_back(entry.path());
                        break;
                    }
                }
            }
            if (sourceAsmFiles.empty()) {
                for (const auto& inc : includeDirs) {
                    if (std::filesystem::exists(inc / mapping.syscallFile)) {
                        sourceAsmFiles.push_back(inc / mapping.syscallFile);
                        break;
                    }
                }
            }
        }
    } else if (std::filesystem::is_regular_file(activeConfig.sourceDir)) {
        if (activeConfig.sourceDir.extension() == ".c") {
            sourceCFiles.push_back(activeConfig.sourceDir);
        } else if (activeConfig.sourceDir.extension() == ".asm") {
            sourceAsmFiles.push_back(activeConfig.sourceDir);
        }
        // Look for companion asm files (e.g. matching syscall) in same directory
        auto parent = activeConfig.sourceDir.parent_path();
        if (std::filesystem::is_directory(parent)) {
            for (const auto& entry : std::filesystem::directory_iterator(parent)) {
                if (entry.is_regular_file()) {
                    auto fname = entry.path().filename().string();
                    if (entry.path().extension() == ".asm" && entry.path() != activeConfig.sourceDir &&
                        (fname == mapping.syscallFile || fname == activeConfig.sourceDir.stem().string() + ".asm")) {
                        sourceAsmFiles.push_back(entry.path());
                    }
                }
            }
        }
        if (sourceAsmFiles.empty()) {
            for (const auto& inc : includeDirs) {
                if (std::filesystem::exists(inc / mapping.syscallFile)) {
                    sourceAsmFiles.push_back(inc / mapping.syscallFile);
                    break;
                }
            }
        }
    } else if (activeConfig.module == ModuleType::Custom) {
        // Look only in top-level sourceDir, skip intermediate/build dirs, and do NOT auto-link bg_lib.c
        for (const auto& entry : std::filesystem::directory_iterator(activeConfig.sourceDir)) {
            if (entry.is_regular_file()) {
                auto fname = entry.path().filename().string();
                if (fname == "bg_lib.c" || fname == "bg_lib.asm") {
                    continue; // Skip automatic bg_lib.c inclusion
                }
                if (entry.path().extension() == ".c") {
                    sourceCFiles.push_back(entry.path());
                } else if (entry.path().extension() == ".asm") {
                    sourceAsmFiles.push_back(entry.path());
                }
            }
        }
        // Ensure entry-point source (mod.c, g_main.c, cg_main.c, ui_main.c, main.c) is first
        auto mainIt = std::find_if(sourceCFiles.begin(), sourceCFiles.end(), [](const auto& p) {
            auto fn = p.filename().string();
            return fn == "mod.c" || fn == "g_main.c" || fn == "cg_main.c" || fn == "ui_main.c" || fn == "main.c";
        });
        if (mainIt != sourceCFiles.end() && mainIt != sourceCFiles.begin()) {
            std::rotate(sourceCFiles.begin(), mainIt, mainIt + 1);
        }
    } else {
        // Use official file order
        sourceCFiles = ModuleConfig::resolveSourceFiles(activeConfig.sourceDir, moduleFiles);

        // Some trees relocate the particle system: stock Q3 keeps it in cg_marks.c,
        // while ioq3/Team-Arena-derived trees use cg_particles.c. Include the
        // alternative only when no already-resolved source defines those symbols.
        if (activeConfig.module == ModuleType::CGame) {
            auto providesDefinition = [](const std::vector<std::filesystem::path>& files,
                                         std::string_view definition) {
                for (const auto& f : files) {
                    if (!std::filesystem::is_regular_file(f)) {
                        continue;
                    }
                    auto text = FileUtils::readFileText(f);
                    if (text.hasValue() && text.value().find(definition) != std::string::npos) {
                        return true;
                    }
                }
                return false;
            };

            if (!providesDefinition(sourceCFiles, "void CG_AddParticles")) {
                for (const auto& sub : {"cgame", ".", "code/cgame"}) {
                    auto particles = activeConfig.sourceDir / sub / "cg_particles.c";
                    if (std::filesystem::exists(particles)) {
                        sourceCFiles.push_back(particles);
                        break;
                    }
                }
            }
        }

        // A QVM module must supply its own C runtime (vsprintf, memcpy, strcmp, ...).
        // The official builds link bg_lib.c for this; resolve it from the source tree.
        const bool alreadyHasBgLib = std::any_of(sourceCFiles.begin(), sourceCFiles.end(),
            [](const std::filesystem::path& p) { return p.stem().string() == "bg_lib"; });
        if (!alreadyHasBgLib) {
            for (const auto& sub : {"game", "qcommon", ".", "cgame", "code/game", "code/qcommon"}) {
                auto bgLib = activeConfig.sourceDir / sub / "bg_lib.c";
                if (std::filesystem::exists(bgLib)) {
                    sourceCFiles.push_back(bgLib);
                    break;
                }
            }
        }

        // Custom vmMain override: keep the stock entry's other symbols but rename its
        // vmMain, and put a forwarded-entry stub ahead of everything so the QVM entry
        // point (code offset 0) is a valid vmMain.
        if (!activeConfig.entryOverrideFile.empty()) {
            const bool userIsStockEntry =
                activeConfig.entryOverrideFile.filename().string() == moduleFiles.entryPointFile;
            if (!userIsStockEntry) {
                for (const auto& f : sourceCFiles) {
                    if (f.filename().string() == moduleFiles.entryPointFile) {
                        fileExtraDefines[canonPath(f)] = {"-DvmMain=q3vm_stock_vmMain"};
                    }
                }

                auto stubFile = intermediateDir / "q3vm_entry_stub.c";
                const std::string stubSource =
                    "int q3vm_user_vmMain(int,int,int,int,int,int,int,int,int,int,int,int,int);\n"
                    "int vmMain(int command,int arg0,int arg1,int arg2,int arg3,int arg4,int arg5,"
                    "int arg6,int arg7,int arg8,int arg9,int arg10,int arg11)\n"
                    "{\n"
                    "    return q3vm_user_vmMain(command,arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11);\n"
                    "}\n";
                if (auto writeErr = FileUtils::writeFileText(stubFile, stubSource); writeErr.isError()) {
                    return writeErr;
                }

                auto userEntry = canonPath(activeConfig.entryOverrideFile);
                fileExtraDefines[userEntry] = {"-DvmMain=q3vm_user_vmMain"};
                sourceCFiles.insert(sourceCFiles.begin(), userEntry);
                sourceCFiles.insert(sourceCFiles.begin(), stubFile);
            }
        }

        // Also resolve syscall and asm files
        for (const auto& asmName : moduleFiles.asmFiles) {
            auto asmPath = activeConfig.sourceDir / asmName;
            if (!std::filesystem::exists(asmPath)) {
                // Check subdirectories
                for (const auto& sub : {"game", "cgame", "q3_ui", "ui", "qcommon",
                                        "code/game", "code/cgame", "code/q3_ui", "code/ui", "code/qcommon"}) {
                    auto subPath = activeConfig.sourceDir / sub / asmName;
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

        std::vector<std::string> fileDefines = defines;
        auto extraIt = fileExtraDefines.find(canonPath(cFile));
        if (extraIt != fileExtraDefines.end()) {
            fileDefines.insert(fileDefines.end(), extraIt->second.begin(), extraIt->second.end());
        }

        auto res = runner.compileCFile(cFile, outAsm, includeDirs, fileDefines, config.debugSymbols, logCallback);
        report.lccOutputs.push_back(res.combinedOutput);

        if (!res.success || !std::filesystem::exists(outAsm)) {
            report.errors.push_back("Compilation failed for " + cFile.filename().string() + ": " + res.combinedOutput);
            return Error(ErrorCode::CompilationFailed,
                         "Compilation failed for " + cFile.filename().string() + "\n" + res.combinedOutput);
        }

        generatedAsmFiles.push_back(outAsm);
        report.filesCompiled++;
    }

    // Deduplicate ASM files to guarantee no duplicate files are passed to Q3ASM
    std::vector<std::filesystem::path> finalAsmFiles;
    auto addUniqueAsm = [&](const std::filesystem::path& p) {
        auto absP = std::filesystem::absolute(p);
        for (const auto& existing : finalAsmFiles) {
            std::error_code eqEc;
            if (std::filesystem::equivalent(existing, absP, eqEc) || existing == absP) {
                return;
            }
        }
        finalAsmFiles.push_back(absP);
    };

    for (const auto& asmFile : generatedAsmFiles) {
        addUniqueAsm(asmFile);
    }
    for (const auto& asmFile : sourceAsmFiles) {
        addUniqueAsm(asmFile);
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

    auto asmRes = runner.assembleQvm(finalAsmFiles, finalQvmPath, outDir, config.verbose, logCallback);
    report.q3asmCommand = asmRes.commandLine;
    report.q3asmOutput = asmRes.combinedOutput;

    if (!asmRes.success || !std::filesystem::exists(finalQvmPath)) {
        std::string errDetail = "Q3ASM assembly failed:\n[Q3ASM Command]: " + asmRes.commandLine + "\n" + asmRes.combinedOutput;
        report.errors.push_back(errDetail);
        return Error(ErrorCode::AssemblyFailed, errDetail);
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
    if (config.cleanIntermediate || isScratchBuild || isSingleFile) {
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

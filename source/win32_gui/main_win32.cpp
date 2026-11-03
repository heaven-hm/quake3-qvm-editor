#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include "qvm/common/Error.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/common/ProcessRunner.hpp"
#include "qvm/common/Version.hpp"
#include "qvm/compiler/Compiler.hpp"
#include "qvm/decompiler/Exporter.hpp"
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include "qvm/decompiler/StringExtractor.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

// Control IDs
enum ControlId {
    ID_TAB_MAIN = 1001,
    // Decompiler controls
    ID_BTN_OPEN_QVM = 1002,
    ID_LBL_QVM_PATH = 1003,
    ID_BTN_EXPORT_ALL = 1004,
    ID_BTN_VIEW_FULL_ASM = 1005,
    ID_BTN_VIEW_FUNC_ASM = 1006,
    ID_BTN_VIEW_PSEUDO = 1007,
    ID_BTN_VIEW_STRINGS = 1008,
    ID_BTN_VIEW_HEADER = 1009,
    ID_LIST_FUNCTIONS = 1010,
    ID_EDIT_DECOMP_VIEW = 1011,
    // Compiler controls
    ID_COMBO_MODULE = 1020,
    ID_EDIT_INCLUDES = 1021,
    ID_EDIT_OUTDIR = 1022,
    ID_EDIT_C_SOURCE = 1023,
    ID_BTN_LOAD_C = 1024,
    ID_BTN_SAVE_C = 1025,
    ID_BTN_SELECT_SDK = 1026,
    ID_BTN_COMPILE_C = 1027,
    ID_EDIT_BUILD_LOG = 1028,
    // Menu items
    IDM_FILE_OPEN_QVM = 2001,
    IDM_FILE_LOAD_C = 2002,
    IDM_FILE_EXPORT = 2003,
    IDM_FILE_EXIT = 2004,
    IDM_BUILD_COMPILE = 2005,
    IDM_HELP_ABOUT = 2006
};

enum ViewMode {
    VIEW_FULL_ASM,
    VIEW_FUNC_ASM,
    VIEW_PSEUDOCODE,
    VIEW_STRINGS,
    VIEW_HEADER
};

// Application state
struct AppState {
    HINSTANCE hInstance = nullptr;
    HWND hWndMain = nullptr;
    HWND hTab = nullptr;
    HFONT hUiFont = nullptr;
    HFONT hMonoFont = nullptr;

    // Decompiler controls
    HWND hBtnOpenQvm = nullptr;
    HWND hLblQvmPath = nullptr;
    HWND hBtnExportAll = nullptr;
    HWND hBtnViewFullAsm = nullptr;
    HWND hBtnViewFuncAsm = nullptr;
    HWND hBtnViewPseudo = nullptr;
    HWND hBtnViewStrings = nullptr;
    HWND hBtnViewHeader = nullptr;
    HWND hListFunctions = nullptr;
    HWND hEditDecompView = nullptr;

    // Compiler controls
    HWND hLblModule = nullptr;
    HWND hComboModule = nullptr;
    HWND hLblIncludes = nullptr;
    HWND hEditIncludes = nullptr;
    HWND hLblOutDir = nullptr;
    HWND hEditOutDir = nullptr;
    HWND hBtnLoadC = nullptr;
    HWND hBtnSaveC = nullptr;
    HWND hBtnSelectSdk = nullptr;
    HWND hBtnCompileC = nullptr;
    HWND hEditCSource = nullptr;
    HWND hLblLog = nullptr;
    HWND hEditBuildLog = nullptr;

    // Data
    int currentTab = 0;
    ViewMode currentView = VIEW_FULL_ASM;
    std::filesystem::path currentQvmPath;
    qvm::ParsedQvm currentParsed;
    std::vector<qvm::FunctionInfo> functions;
    std::vector<qvm::ExtractedString> strings;
    std::string fullAsmText;
    std::string pseudocodeText;
    std::string headerText;
    std::string stringsText;
};

AppState g_app;

const char* kDefaultCTemplate = 
"/*\n"
" * Quake III Arena QVM Source Code\n"
" */\n\n"
"void trap_Print(const char *text);\n\n"
"int vmMain(int command, int arg0, int arg1, int arg2, int arg3,\n"
"           int arg4, int arg5, int arg6, int arg7,\n"
"           int arg8, int arg9, int arg10, int arg11) {\n"
"    switch (command) {\n"
"        case 0: // GAME_INIT\n"
"            trap_Print(\"^2[QVM Editor]^7 Module initialized successfully!\\n\");\n"
"            return 0;\n\n"
"        case 1: // GAME_RUN_FRAME\n"
"            return 0;\n\n"
"        case 2: // GAME_SHUTDOWN\n"
"            trap_Print(\"^1[QVM Editor]^7 Module shutdown.\\n\");\n"
"            return 0;\n"
"    }\n\n"
"    return -1;\n"
"}\n";

std::wstring toWString(std::string_view str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], size);
    return result;
}

std::string toString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

void setWindowText(HWND hwnd, const std::string& text) {
    std::wstring w = toWString(text);
    SetWindowTextW(hwnd, w.c_str());
}

std::string getWindowText(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return "";
    std::wstring w(len + 1, 0);
    GetWindowTextW(hwnd, &w[0], len + 1);
    w.resize(len);
    return toString(w);
}

void appendLog(const std::string& text) {
    std::string current = getWindowText(g_app.hEditBuildLog);
    if (!current.empty() && current.back() != '\n') {
        current += "\r\n";
    }
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            current += "\r\n";
        } else {
            current += text[i];
        }
    }
    setWindowText(g_app.hEditBuildLog, current);
    SendMessageW(g_app.hEditBuildLog, EM_SETSEL, (WPARAM)current.size(), (LPARAM)current.size());
    SendMessageW(g_app.hEditBuildLog, EM_SCROLLCARET, 0, 0);
}

void updateDecompilerDisplay() {
    std::string displayText;
    switch (g_app.currentView) {
        case VIEW_FULL_ASM:
            displayText = g_app.fullAsmText;
            break;
        case VIEW_FUNC_ASM: {
            int sel = (int)SendMessageW(g_app.hListFunctions, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app.functions.size()) {
                const auto& fn = g_app.functions[sel];
                std::ostringstream ss;
                ss << "; ================================================================\n"
                   << "; Function: " << fn.name << "\n"
                   << "; Address:  0x" << std::hex << fn.startOffset << " - 0x" << fn.endOffset << std::dec << "\n"
                   << "; Instructions: " << (fn.endIndex - fn.startIndex) << "\n"
                   << "; ================================================================\n\n";
                for (size_t i = fn.startIndex; i < fn.endIndex && i < g_app.currentParsed.instructions.size(); ++i) {
                    ss << g_app.currentParsed.instructions[i].formatAssembly() << "\n";
                }
                displayText = ss.str();
            } else {
                displayText = "; Please select a function from the list on the left.";
            }
            break;
        }
        case VIEW_PSEUDOCODE:
            displayText = g_app.pseudocodeText;
            break;
        case VIEW_STRINGS:
            displayText = g_app.stringsText;
            break;
        case VIEW_HEADER:
            displayText = g_app.headerText;
            break;
    }

    std::string winText;
    winText.reserve(displayText.size() * 11 / 10);
    for (size_t i = 0; i < displayText.size(); ++i) {
        if (displayText[i] == '\n' && (i == 0 || displayText[i-1] != '\r')) {
            winText += "\r\n";
        } else {
            winText += displayText[i];
        }
    }

    setWindowText(g_app.hEditDecompView, winText);
}

void loadQvmFile(const std::filesystem::path& path) {
    auto parsedRes = qvm::QvmParser::parseFile(path);
    if (parsedRes.hasError()) {
        MessageBoxW(g_app.hWndMain, toWString("Failed to open QVM:\n" + parsedRes.error().message()).c_str(),
                    L"Error", MB_ICONERROR);
        return;
    }

    g_app.currentQvmPath = path;
    g_app.currentParsed = parsedRes.value();
    g_app.functions = qvm::FunctionAnalyzer::analyzeFunctions(g_app.currentParsed.instructions, qvm::ModuleType::QaGame);
    g_app.strings = qvm::StringExtractor::extractAll(g_app.currentParsed.sections);

    // Build Header Text
    g_app.headerText = g_app.currentParsed.header.formatInfo();

    // Build Full Assembly
    g_app.fullAsmText = qvm::Exporter::generateDisassembly(g_app.currentParsed, qvm::ModuleType::QaGame);

    // Build Pseudocode
    g_app.pseudocodeText = qvm::PseudocodeGenerator::generate(g_app.currentParsed, g_app.functions, qvm::ModuleType::QaGame);

    // Build Strings Text
    std::ostringstream strSs;
    strSs << "; Extracted Strings from DATA and LIT sections (" << g_app.strings.size() << " strings)\n\n";
    for (const auto& s : g_app.strings) {
        strSs << "[" << (s.inLitSection ? "LIT" : "DATA") << " + 0x" << std::hex << s.offset << std::dec << "] \"" << s.text << "\"\n";
    }
    g_app.stringsText = strSs.str();

    // Update controls
    setWindowText(g_app.hLblQvmPath, path.filename().string() + " (" + std::to_string(g_app.currentParsed.instructions.size()) + " instructions)");

    // Populate functions list
    SendMessageW(g_app.hListFunctions, LB_RESETCONTENT, 0, 0);
    for (const auto& fn : g_app.functions) {
        char buf[256];
        size_t count = (fn.endIndex > fn.startIndex) ? (fn.endIndex - fn.startIndex) : 0;
        snprintf(buf, sizeof(buf), "[0x%06X] %s (%zu instrs)", (unsigned int)fn.startOffset, fn.name.c_str(), count);
        SendMessageW(g_app.hListFunctions, LB_ADDSTRING, 0, (LPARAM)toWString(buf).c_str());
    }
    if (!g_app.functions.empty()) {
        SendMessageW(g_app.hListFunctions, LB_SETCURSEL, 0, 0);
    }

    updateDecompilerDisplay();
}

void openQvmFileDialog() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.hWndMain;
    ofn.lpstrFilter = L"Quake III QVM Files (*.qvm)\0*.qvm\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        loadQvmFile(std::filesystem::path(filename));
    }
}

void openCFileDialog() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.hWndMain;
    ofn.lpstrFilter = L"C Source Files (*.c;*.h)\0*.c;*.h\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        std::ifstream ifs(filename);
        if (ifs) {
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            std::string winText;
            for (size_t i = 0; i < content.size(); ++i) {
                if (content[i] == '\n' && (i == 0 || content[i-1] != '\r')) winText += "\r\n";
                else winText += content[i];
            }
            setWindowText(g_app.hEditCSource, winText);
            appendLog("Loaded C file: " + toString(filename));
        }
    }
}

void saveCFileDialog() {
    wchar_t filename[MAX_PATH] = L"g_main.c";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.hWndMain;
    ofn.lpstrFilter = L"C Source Files (*.c)\0*.c\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        std::string content = getWindowText(g_app.hEditCSource);
        std::ofstream ofs(filename);
        if (ofs) {
            ofs << content;
            appendLog("Saved C file: " + toString(filename));
        }
    }
}

void selectSdkDialog() {
    BROWSEINFOW bi{};
    bi.hwndOwner = g_app.hWndMain;
    bi.lpszTitle = L"Select Quake III Arena SDK or code directory:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != 0) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            std::string sdkPath = toString(path);
            setWindowText(g_app.hEditIncludes, sdkPath);
            appendLog("Selected Quake III SDK: " + sdkPath);
            appendLog("Auto-resolving sibling headers (qcommon, cgame, game, ui)...");
        }
        CoTaskMemFree(pidl);
    }
}

void exportAllDialog() {
    if (g_app.currentParsed.instructions.empty()) {
        MessageBoxW(g_app.hWndMain, L"Please open a QVM file first before exporting.", L"Info", MB_ICONINFORMATION);
        return;
    }

    auto outDir = g_app.currentQvmPath.parent_path() / "decompiled";
    std::filesystem::create_directories(outDir);

    auto baseName = g_app.currentQvmPath.stem().string();
    auto asmPath = outDir / (baseName + ".asm");
    auto cPath = outDir / (baseName + ".c");
    auto strPath = outDir / (baseName + "_strings.txt");

    std::ofstream(asmPath) << g_app.fullAsmText;
    std::ofstream(cPath) << g_app.pseudocodeText;
    std::ofstream(strPath) << g_app.stringsText;

    std::string msg = "Successfully exported files to:\n" + outDir.string() + "\n\n"
                    + "- " + asmPath.filename().string() + "\n"
                    + "- " + cPath.filename().string() + "\n"
                    + "- " + strPath.filename().string();
    MessageBoxW(g_app.hWndMain, toWString(msg).c_str(), L"Export Succeeded", MB_ICONINFORMATION);
}

void compileEditorCode() {
    std::string sourceCode = getWindowText(g_app.hEditCSource);
    if (sourceCode.empty()) {
        MessageBoxW(g_app.hWndMain, L"Source code is empty.", L"Warning", MB_ICONWARNING);
        return;
    }

    setWindowText(g_app.hEditBuildLog, "");
    appendLog("=== Starting QVM Compilation ===");

    // Determine target module
    int modSel = (int)SendMessageW(g_app.hComboModule, CB_GETCURSEL, 0, 0);
    qvm::ModuleType modType = qvm::ModuleType::QaGame;
    std::string modName = "qagame";
    if (modSel == 1) { modType = qvm::ModuleType::CGame; modName = "cgame"; }
    else if (modSel == 2) { modType = qvm::ModuleType::Ui; modName = "ui"; }
    else if (modSel == 3) { modType = qvm::ModuleType::Custom; modName = "custom"; }

    // Output dir
    std::string outDirStr = getWindowText(g_app.hEditOutDir);
    if (outDirStr.empty()) outDirStr = "./build";
    std::filesystem::path outDir(outDirStr);
    std::filesystem::create_directories(outDir);

    // Save temporary C file outside user's output directory
    auto scratchDir = std::filesystem::temp_directory_path() / "quake3_qvm_editor_scratch";
    std::filesystem::create_directories(scratchDir);
    auto cFilePath = scratchDir / "g_main.c";

    // Ensure newline at EOF
    if (!sourceCode.empty() && sourceCode.back() != '\n') {
        sourceCode += "\n";
    }

    {
        std::ofstream ofs(cFilePath, std::ios::binary);
        ofs.write(sourceCode.data(), sourceCode.size());
    }
    appendLog("Wrote scratch source: " + cFilePath.string());

    // Build include dirs
    std::vector<std::filesystem::path> includeDirs;
    std::string incStr = getWindowText(g_app.hEditIncludes);
    std::filesystem::path sdkPath;
    if (!incStr.empty()) {
        std::istringstream iss(incStr);
        std::string item;
        while (std::getline(iss, item, ';')) {
            if (!item.empty()) {
                includeDirs.push_back(item);
                if (sdkPath.empty()) sdkPath = item;
            }
        }
    }
    includeDirs = qvm::FileUtils::resolveQuake3IncludeDirs(includeDirs, sdkPath);

    // Configure compiler
    qvm::CompilerConfig config;
    config.module = modType;
    config.sourceDir = cFilePath;
    config.outputName = modName + ".qvm";
    config.outputDir = outDir;
    config.sdkDir = sdkPath;
    config.includePaths = includeDirs;
    config.preprocessorDefines.push_back("-DQ3_VM");
    config.cleanIntermediate = false;

    auto progressCb = [](int current, int total, std::string_view msg) {
        appendLog("[" + std::to_string(current) + "/" + std::to_string(total) + "] " + std::string(msg));
    };

    auto logCb = [](std::string_view line, bool) {
        appendLog(std::string(line));
    };

    auto compileRes = qvm::Compiler::compile(config, progressCb, logCb);
    if (compileRes.hasError()) {
        appendLog("\n=== BUILD FAILED ===");
        appendLog(compileRes.error().message());
        MessageBoxW(g_app.hWndMain, toWString("Compilation failed!\n\n" + compileRes.error().message()).c_str(),
                    L"Compilation Error", MB_ICONERROR);
    } else {
        const auto& report = compileRes.value();
        auto finalQvm = report.outputQvmPath;
        appendLog("\n=== BUILD SUCCESSFUL ===");
        appendLog("QVM Output: " + finalQvm.string());

        int res = MessageBoxW(g_app.hWndMain,
            toWString("QVM compilation succeeded!\n\nOutput: " + finalQvm.string() + "\n\nWould you like to open and inspect this QVM in the Decompiler now?").c_str(),
            L"Success", MB_YESNO | MB_ICONINFORMATION);
        if (res == IDYES) {
            SendMessageW(g_app.hTab, TCM_SETCURSEL, 0, 0);
            NMHDR nm;
            nm.hwndFrom = g_app.hTab;
            nm.idFrom = ID_TAB_MAIN;
            nm.code = TCN_SELCHANGE;
            SendMessageW(g_app.hWndMain, WM_NOTIFY, (WPARAM)ID_TAB_MAIN, (LPARAM)&nm);
            loadQvmFile(finalQvm);
        }
    }
}

void showAboutDialog() {
    std::string aboutMsg = 
        "Quake III Arena QVM Editor v" + std::string(qvm::kVersion) + "\n\n"
        "Official Quake III Arena QVM Toolchain & Decompiler Suite\n\n"
        "Portions Copyright (C) 1999-2005 Id Software, Inc.\n"
        "Distributed under the GNU General Public License (GPLv2).\n\n"
        "Features:\n"
        "- Bytecode Decompiler, Disassembler & C Pseudocode Generator\n"
        "- Native Quake III Arena C Toolchain (LCC, Q3CPP, Q3RCC, Q3ASM)\n"
        "- C Code Scratchpad & Live QVM Compiler\n";
    MessageBoxW(g_app.hWndMain, toWString(aboutMsg).c_str(), L"About Quake 3 QVM Editor", MB_ICONINFORMATION);
}

void switchTab(int tabIndex) {
    g_app.currentTab = tabIndex;
    int showDecomp = (tabIndex == 0) ? SW_SHOW : SW_HIDE;
    int showComp = (tabIndex == 1) ? SW_SHOW : SW_HIDE;

    // Decompiler controls
    ShowWindow(g_app.hBtnOpenQvm, showDecomp);
    ShowWindow(g_app.hLblQvmPath, showDecomp);
    ShowWindow(g_app.hBtnExportAll, showDecomp);
    ShowWindow(g_app.hBtnViewFullAsm, showDecomp);
    ShowWindow(g_app.hBtnViewFuncAsm, showDecomp);
    ShowWindow(g_app.hBtnViewPseudo, showDecomp);
    ShowWindow(g_app.hBtnViewStrings, showDecomp);
    ShowWindow(g_app.hBtnViewHeader, showDecomp);
    ShowWindow(g_app.hListFunctions, showDecomp);
    ShowWindow(g_app.hEditDecompView, showDecomp);

    // Compiler controls
    ShowWindow(g_app.hLblModule, showComp);
    ShowWindow(g_app.hComboModule, showComp);
    ShowWindow(g_app.hLblIncludes, showComp);
    ShowWindow(g_app.hEditIncludes, showComp);
    ShowWindow(g_app.hLblOutDir, showComp);
    ShowWindow(g_app.hEditOutDir, showComp);
    ShowWindow(g_app.hBtnLoadC, showComp);
    ShowWindow(g_app.hBtnSaveC, showComp);
    ShowWindow(g_app.hBtnSelectSdk, showComp);
    ShowWindow(g_app.hBtnCompileC, showComp);
    ShowWindow(g_app.hEditCSource, showComp);
    ShowWindow(g_app.hLblLog, showComp);
    ShowWindow(g_app.hEditBuildLog, showComp);
}

void resizeControls(int width, int height) {
    if (!g_app.hTab) return;

    SetWindowPos(g_app.hTab, nullptr, 10, 10, width - 20, height - 20, SWP_NOZORDER);

    int tabContentTop = 45;
    int contentWidth = width - 40;
    int contentHeight = height - 65;

    // Tab 0: Decompiler Layout
    SetWindowPos(g_app.hBtnOpenQvm, nullptr, 20, tabContentTop, 140, 28, SWP_NOZORDER);
    SetWindowPos(g_app.hLblQvmPath, nullptr, 170, tabContentTop + 5, contentWidth - 360, 20, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnExportAll, nullptr, width - 200, tabContentTop, 170, 28, SWP_NOZORDER);

    int viewBtnTop = tabContentTop + 35;
    int btnW = 130;
    SetWindowPos(g_app.hBtnViewFullAsm, nullptr, 20, viewBtnTop, btnW, 26, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnViewFuncAsm, nullptr, 20 + btnW + 5, viewBtnTop, btnW, 26, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnViewPseudo, nullptr, 20 + (btnW + 5) * 2, viewBtnTop, btnW, 26, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnViewStrings, nullptr, 20 + (btnW + 5) * 3, viewBtnTop, btnW, 26, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnViewHeader, nullptr, 20 + (btnW + 5) * 4, viewBtnTop, btnW, 26, SWP_NOZORDER);

    int splitTop = viewBtnTop + 35;
    int splitH = contentHeight - (splitTop - tabContentTop);
    int listW = 280;
    SetWindowPos(g_app.hListFunctions, nullptr, 20, splitTop, listW, splitH, SWP_NOZORDER);
    SetWindowPos(g_app.hEditDecompView, nullptr, 20 + listW + 10, splitTop, contentWidth - listW - 10, splitH, SWP_NOZORDER);

    // Tab 1: Compiler Layout
    int cTop = tabContentTop;
    SetWindowPos(g_app.hLblModule, nullptr, 20, cTop + 4, 60, 20, SWP_NOZORDER);
    SetWindowPos(g_app.hComboModule, nullptr, 85, cTop, 140, 150, SWP_NOZORDER);
    SetWindowPos(g_app.hLblIncludes, nullptr, 235, cTop + 4, 90, 20, SWP_NOZORDER);
    SetWindowPos(g_app.hEditIncludes, nullptr, 330, cTop, contentWidth - 580, 24, SWP_NOZORDER);
    SetWindowPos(g_app.hLblOutDir, nullptr, width - 260, cTop + 4, 60, 20, SWP_NOZORDER);
    SetWindowPos(g_app.hEditOutDir, nullptr, width - 195, cTop, 165, 24, SWP_NOZORDER);

    int editorTop = cTop + 35;
    int editorH = (contentHeight - 120) * 60 / 100;
    SetWindowPos(g_app.hEditCSource, nullptr, 20, editorTop, contentWidth, editorH, SWP_NOZORDER);

    int btnBarTop = editorTop + editorH + 10;
    SetWindowPos(g_app.hBtnLoadC, nullptr, 20, btnBarTop, 120, 28, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnSaveC, nullptr, 150, btnBarTop, 120, 28, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnSelectSdk, nullptr, 280, btnBarTop, 170, 28, SWP_NOZORDER);
    SetWindowPos(g_app.hBtnCompileC, nullptr, width - 210, btnBarTop, 180, 30, SWP_NOZORDER);

    int logTop = btnBarTop + 38;
    int logH = contentHeight - (logTop - tabContentTop);
    SetWindowPos(g_app.hLblLog, nullptr, 20, logTop - 18, 150, 16, SWP_NOZORDER);
    SetWindowPos(g_app.hEditBuildLog, nullptr, 20, logTop, contentWidth, logH, SWP_NOZORDER);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            // Set App fonts
            g_app.hUiFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_app.hMonoFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

            // Main Tab Control
            g_app.hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
                WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
                0, 0, 0, 0, hWnd, (HMENU)ID_TAB_MAIN, g_app.hInstance, nullptr);
            SendMessageW(g_app.hTab, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            TCITEMW tie;
            tie.mask = TCIF_TEXT;
            tie.pszText = (LPWSTR)L"  QVM Decompiler & Disassembler  ";
            SendMessageW(g_app.hTab, TCM_INSERTITEMW, 0, (LPARAM)&tie);
            tie.pszText = (LPWSTR)L"  C Code Editor & Compiler  ";
            SendMessageW(g_app.hTab, TCM_INSERTITEMW, 1, (LPARAM)&tie);

            // Tab 0 Controls: Decompiler
            g_app.hBtnOpenQvm = CreateWindowExW(0, L"BUTTON", L"Open QVM File...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_OPEN_QVM, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnOpenQvm, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hLblQvmPath = CreateWindowExW(0, L"STATIC", L"No QVM loaded.",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
                0, 0, 0, 0, hWnd, (HMENU)ID_LBL_QVM_PATH, g_app.hInstance, nullptr);
            SendMessageW(g_app.hLblQvmPath, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnExportAll = CreateWindowExW(0, L"BUTTON", L"Export All to Folder...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_EXPORT_ALL, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnExportAll, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnViewFullAsm = CreateWindowExW(0, L"BUTTON", L"Full Assembly",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_VIEW_FULL_ASM, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnViewFullAsm, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnViewFuncAsm = CreateWindowExW(0, L"BUTTON", L"Selected Func Asm",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_VIEW_FUNC_ASM, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnViewFuncAsm, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnViewPseudo = CreateWindowExW(0, L"BUTTON", L"C Pseudocode",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_VIEW_PSEUDO, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnViewPseudo, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnViewStrings = CreateWindowExW(0, L"BUTTON", L"Extracted Strings",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_VIEW_STRINGS, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnViewStrings, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnViewHeader = CreateWindowExW(0, L"BUTTON", L"Header & Info",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)ID_BTN_VIEW_HEADER, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnViewHeader, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hListFunctions = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                0, 0, 0, 0, hWnd, (HMENU)ID_LIST_FUNCTIONS, g_app.hInstance, nullptr);
            SendMessageW(g_app.hListFunctions, WM_SETFONT, (WPARAM)g_app.hMonoFont, TRUE);

            g_app.hEditDecompView = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_DECOMP_VIEW, g_app.hInstance, nullptr);
            SendMessageW(g_app.hEditDecompView, WM_SETFONT, (WPARAM)g_app.hMonoFont, TRUE);

            // Tab 1 Controls: Compiler
            g_app.hLblModule = CreateWindowExW(0, L"STATIC", L"Module:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, hWnd, nullptr, g_app.hInstance, nullptr);
            SendMessageW(g_app.hLblModule, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hComboModule = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                0, 0, 0, 0, hWnd, (HMENU)ID_COMBO_MODULE, g_app.hInstance, nullptr);
            SendMessageW(g_app.hComboModule, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);
            SendMessageW(g_app.hComboModule, CB_ADDSTRING, 0, (LPARAM)L"Game (qagame)");
            SendMessageW(g_app.hComboModule, CB_ADDSTRING, 0, (LPARAM)L"CGame (cgame)");
            SendMessageW(g_app.hComboModule, CB_ADDSTRING, 0, (LPARAM)L"UI (ui)");
            SendMessageW(g_app.hComboModule, CB_ADDSTRING, 0, (LPARAM)L"Custom");
            SendMessageW(g_app.hComboModule, CB_SETCURSEL, 0, 0);

            g_app.hLblIncludes = CreateWindowExW(0, L"STATIC", L"Include Paths:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, hWnd, nullptr, g_app.hInstance, nullptr);
            SendMessageW(g_app.hLblIncludes, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hEditIncludes = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_INCLUDES, g_app.hInstance, nullptr);
            SendMessageW(g_app.hEditIncludes, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hLblOutDir = CreateWindowExW(0, L"STATIC", L"Output:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, hWnd, nullptr, g_app.hInstance, nullptr);
            SendMessageW(g_app.hLblOutDir, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hEditOutDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"./build",
                WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_OUTDIR, g_app.hInstance, nullptr);
            SendMessageW(g_app.hEditOutDir, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hEditCSource = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_C_SOURCE, g_app.hInstance, nullptr);
            SendMessageW(g_app.hEditCSource, WM_SETFONT, (WPARAM)g_app.hMonoFont, TRUE);
            setWindowText(g_app.hEditCSource, kDefaultCTemplate);

            g_app.hBtnLoadC = CreateWindowExW(0, L"BUTTON", L"Load C File...",
                WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)ID_BTN_LOAD_C, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnLoadC, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnSaveC = CreateWindowExW(0, L"BUTTON", L"Save C File...",
                WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)ID_BTN_SAVE_C, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnSaveC, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnSelectSdk = CreateWindowExW(0, L"BUTTON", L"Select Quake 3 SDK...",
                WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)ID_BTN_SELECT_SDK, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnSelectSdk, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hBtnCompileC = CreateWindowExW(0, L"BUTTON", L"▶ Compile to QVM",
                WS_CHILD | BS_PUSHBUTTON | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)ID_BTN_COMPILE_C, g_app.hInstance, nullptr);
            SendMessageW(g_app.hBtnCompileC, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hLblLog = CreateWindowExW(0, L"STATIC", L"Build Output Log:",
                WS_CHILD | SS_LEFT, 0, 0, 0, 0, hWnd, nullptr, g_app.hInstance, nullptr);
            SendMessageW(g_app.hLblLog, WM_SETFONT, (WPARAM)g_app.hUiFont, TRUE);

            g_app.hEditBuildLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_BUILD_LOG, g_app.hInstance, nullptr);
            SendMessageW(g_app.hEditBuildLog, WM_SETFONT, (WPARAM)g_app.hMonoFont, TRUE);

            switchTab(0);
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            resizeControls(width, height);
            break;
        }

        case WM_NOTIFY: {
            NMHDR* pnm = (NMHDR*)lParam;
            if (pnm->idFrom == ID_TAB_MAIN && pnm->code == TCN_SELCHANGE) {
                int cur = (int)SendMessageW(g_app.hTab, TCM_GETCURSEL, 0, 0);
                switchTab(cur);
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == ID_LIST_FUNCTIONS && wmEvent == LBN_SELCHANGE) {
                if (g_app.currentView == VIEW_FUNC_ASM) {
                    updateDecompilerDisplay();
                }
                break;
            }

            switch (wmId) {
                case ID_BTN_OPEN_QVM:
                case IDM_FILE_OPEN_QVM:
                    openQvmFileDialog();
                    break;

                case ID_BTN_EXPORT_ALL:
                case IDM_FILE_EXPORT:
                    exportAllDialog();
                    break;

                case ID_BTN_VIEW_FULL_ASM:
                    g_app.currentView = VIEW_FULL_ASM;
                    updateDecompilerDisplay();
                    break;

                case ID_BTN_VIEW_FUNC_ASM:
                    g_app.currentView = VIEW_FUNC_ASM;
                    updateDecompilerDisplay();
                    break;

                case ID_BTN_VIEW_PSEUDO:
                    g_app.currentView = VIEW_PSEUDOCODE;
                    updateDecompilerDisplay();
                    break;

                case ID_BTN_VIEW_STRINGS:
                    g_app.currentView = VIEW_STRINGS;
                    updateDecompilerDisplay();
                    break;

                case ID_BTN_VIEW_HEADER:
                    g_app.currentView = VIEW_HEADER;
                    updateDecompilerDisplay();
                    break;

                case ID_BTN_LOAD_C:
                case IDM_FILE_LOAD_C:
                    openCFileDialog();
                    break;

                case ID_BTN_SAVE_C:
                    saveCFileDialog();
                    break;

                case ID_BTN_SELECT_SDK:
                    selectSdkDialog();
                    break;

                case ID_BTN_COMPILE_C:
                case IDM_BUILD_COMPILE:
                    compileEditorCode();
                    break;

                case IDM_HELP_ABOUT:
                    showAboutDialog();
                    break;

                case IDM_FILE_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            break;
        }

        case WM_DESTROY: {
            if (g_app.hUiFont) DeleteObject(g_app.hUiFont);
            if (g_app.hMonoFont) DeleteObject(g_app.hMonoFont);
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

HMENU createAppMenu() {
    HMENU hMenu = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN_QVM, L"&Open QVM File...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_LOAD_C, L"&Open C Source...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXPORT, L"&Export Disassembly & Pseudocode...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"E&xit");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hBuild = CreatePopupMenu();
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_COMPILE, L"&Compile to QVM\tF5");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBuild, L"&Build");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"&About Quake 3 QVM Editor...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hMenu;
}

int runGui(HINSTANCE hInstance, int nCmdShow) {
    g_app.hInstance = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!wcex.hIcon) wcex.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"Quake3QvmEditorWinClass";
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!wcex.hIconSm) wcex.hIconSm = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    RegisterClassExW(&wcex);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1040;
    int winH = 740;
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    HMENU hMenu = createAppMenu();

    std::wstring title = L"Quake 3 QVM Editor v" + toWString(qvm::kVersion) + L" - Official Quake III Arena QVM Toolchain";

    HWND hWnd = CreateWindowExW(
        0, L"Quake3QvmEditorWinClass", title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        posX, posY, winW, winH,
        nullptr, hMenu, hInstance, nullptr);

    if (!hWnd) return FALSE;

    g_app.hWndMain = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

int runCli(int argc, char* argv[]);

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;

    if (argc > 1) {
        if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
            FILE* fp;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
            std::ios::sync_with_stdio();
        }
        return runCli(argc, argv);
    }

    return runGui(hInstance, nCmdShow);
}

namespace {

void printVersion() {
    std::cout << qvm::kAppName << " version " << qvm::kVersion << "\n"
              << "Official Quake III Arena QVM Toolchain & Decompiler Suite\n";
}

void printHelp() {
    printVersion();
    std::cout << R"(
Usage:
  quake3-qvm-editor [command] [options]

If run with no arguments, launches the Desktop GUI Editor.

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
  --output <dir>    Path to output directory (default: ./build)
  --name <filename> Custom output filename (e.g. mymod.qvm)
  -I<path>          Add include search directory
  -D<macro>         Define preprocessor macro
  --verbose         Enable verbose output
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
    auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions, qvm::ModuleType::QaGame);
    std::cout << "Analysis Summary:\n"
              << "  Decoded Instructions: " << parsed.instructions.size() << "\n"
              << "  Identified Functions: " << functions.size() << "\n"
              << "  Extracted Strings:    " << strings.size() << "\n";
    return 0;
}

int handleDisassemble(const std::string& qvmFile, const std::string& outFile) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }
    std::string disasm = qvm::Exporter::generateDisassembly(parsedRes.value(), qvm::ModuleType::QaGame);
    if (outFile.empty()) {
        std::cout << disasm;
    } else {
        std::ofstream ofs(outFile);
        ofs << disasm;
        std::cout << "Disassembly written to " << outFile << "\n";
    }
    return 0;
}

int handlePseudocode(const std::string& qvmFile, const std::string& outFile) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }
    const auto& parsed = parsedRes.value();
    auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions, qvm::ModuleType::QaGame);
    auto code = qvm::PseudocodeGenerator::generate(parsed, functions, qvm::ModuleType::QaGame);
    if (outFile.empty()) {
        std::cout << code << "\n";
    } else {
        std::ofstream ofs(outFile);
        ofs << code << "\n";
        std::cout << "Pseudocode written to " << outFile << "\n";
    }
    return 0;
}

int handleExport(const std::string& qvmFile, const std::string& outDir) {
    auto parsedRes = qvm::QvmParser::parseFile(qvmFile);
    if (parsedRes.hasError()) {
        std::cerr << "Error: " << parsedRes.error().message() << "\n";
        return 1;
    }
    auto targetDir = outDir.empty() ? std::filesystem::path("./decompiled") : std::filesystem::path(outDir);
    std::filesystem::create_directories(targetDir);
    auto base = targetDir / std::filesystem::path(qvmFile).stem();
    auto err = qvm::Exporter::exportBoth(parsedRes.value(), base.string() + ".asm", base.string() + ".c", qvm::ModuleType::QaGame);
    if (err.isError()) {
        std::cerr << "Export failed: " << err.message() << "\n";
        return 1;
    }
    std::cout << "Successfully exported QVM to: " << targetDir.string() << "\n";
    return 0;
}

int runCli(int argc, char* argv[]) {
    std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        printHelp();
        return 0;
    }
    if (command == "--version" || command == "-v") {
        printVersion();
        return 0;
    }

    if (command == "info" && argc > 2) {
        return handleInfo(argv[2]);
    }
    if (command == "disassemble" && argc > 2) {
        std::string out = (argc > 3) ? argv[3] : "";
        return handleDisassemble(argv[2], out);
    }
    if (command == "pseudocode" && argc > 2) {
        std::string out = (argc > 3) ? argv[3] : "";
        return handlePseudocode(argv[2], out);
    }
    if (command == "export" && argc > 2) {
        std::string out = (argc > 3) ? argv[3] : "";
        return handleExport(argv[2], out);
    }
    if (command == "compile") {
        std::filesystem::path source;
        std::filesystem::path outputDir = "./build";
        std::string customName;
        std::vector<std::filesystem::path> includeDirs;
        std::vector<std::string> defines;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--source" || arg == "--file") {
                if (i + 1 < argc) source = argv[++i];
            } else if (arg == "--output") {
                if (i + 1 < argc) outputDir = argv[++i];
            } else if (arg == "--name") {
                if (i + 1 < argc) customName = argv[++i];
            } else if (arg.rfind("-I", 0) == 0) {
                includeDirs.push_back(arg.substr(2));
            } else if (arg.rfind("-D", 0) == 0) {
                defines.push_back(arg.substr(2));
            } else if (source.empty() && arg[0] != '-') {
                source = arg;
            }
        }

        if (source.empty()) {
            std::cerr << "Error: No source file specified for compile.\n";
            return 1;
        }

        qvm::CompilerConfig config;
        config.module = qvm::ModuleType::QaGame;
        config.sourceDir = source;
        config.outputName = customName.empty() ? "qagame.qvm" : customName;
        config.outputDir = outputDir;
        config.includePaths = includeDirs;
        config.preprocessorDefines = defines;

        auto progress = [](int current, int total, std::string_view msg) {
            std::cout << "[" << current << "/" << total << "] " << msg << "\n";
        };

        auto log = [](std::string_view line, bool isErr) {
            if (isErr) std::cerr << line << "\n";
            else std::cout << line << "\n";
        };

        auto res = qvm::Compiler::compile(config, progress, log);
        if (res.hasError()) {
            std::cerr << "Compilation failed: " << res.error().message() << "\n";
            return 1;
        }
        std::cout << "Successfully built: " << res.value().outputQvmPath.string() << "\n";
        return 0;
    }

    printHelp();
    return 1;
}

} // namespace

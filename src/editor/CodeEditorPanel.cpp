#include "CodeEditorPanel.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include "scripting/PythonStubs.hpp"

// Helper to generate a custom Python Language Definition
TextEditor::LanguageDefinition GetPythonLanguageDefinition() {
    TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::CPlusPlus();
    
    lang.mKeywords.clear();
    lang.mIdentifiers.clear();

    // Python specific Keywords
    const char* pythonKeywords[] = { 
        "and", "as", "assert", "break", "class", "continue", "def", "del", "elif", "else", 
        "except", "False", "finally", "for", "from", "global", "if", "import", "in", "is", 
        "lambda", "None", "nonlocal", "not", "or", "pass", "raise", "return", "True", "try", 
        "while", "with", "yield" 
    };
    for (const auto& k : pythonKeywords) {
        lang.mKeywords.insert(k);
    }

    // Python Built-ins
    TextEditor::Identifier builtInId;
    builtInId.mDeclaration = "Built-in";
    lang.mIdentifiers.insert({"print", builtInId});
    lang.mIdentifiers.insert({"self", builtInId});
    lang.mIdentifiers.insert({"super", builtInId});

    // Dynamic Foxvoid Engine API Parsing
    TextEditor::Identifier apiId;
    apiId.mDeclaration = "Foxvoid API";

    std::string stubsContent = FOXVOID_PYI_CONTENT; 

    // Regex to match "class ClassName" (captures the word after 'class')
    std::regex classRegex(R"(\bclass\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    auto classBegin = std::sregex_iterator(stubsContent.begin(), stubsContent.end(), classRegex);
    auto classEnd = std::sregex_iterator();

    for (std::sregex_iterator i = classBegin; i != classEnd; ++i) {
        std::smatch match = *i;
        if (match.size() >= 2) {
            std::string className = match[1].str();
            lang.mIdentifiers.insert({className, apiId});
        }
    }

    // Bonus: Regex to match "def function_name" to highlight API methods too!
    std::regex defRegex(R"(\bdef\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    auto defBegin = std::sregex_iterator(stubsContent.begin(), stubsContent.end(), defRegex);
    auto defEnd = std::sregex_iterator();

    for (std::sregex_iterator i = defBegin; i != defEnd; ++i) {
        std::smatch match = *i;
        if (match.size() >= 2) {
            std::string funcName = match[1].str();
            // We ignore '__init__' to avoid coloring basic python constructors as Foxvoid API
            if (funcName != "__init__") {
                lang.mIdentifiers.insert({funcName, apiId});
            }
        }
    }

    // 4. Fix the comments and formatting
    lang.mSingleLineComment = "#";
    lang.mCommentStart = "\"\"\"";
    lang.mCommentEnd = "\"\"\"";

    lang.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
    lang.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("\\'\\\\?[^']\\'", TextEditor::PaletteIndex::String));
    lang.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    lang.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", TextEditor::PaletteIndex::Number));
    lang.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
    lang.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", TextEditor::PaletteIndex::Punctuation));

    lang.mName = "Python";
    return lang;
}

CodeEditorPanel::CodeEditorPanel() {
    // Setup the text editor for Python
    m_editor.SetLanguageDefinition(GetPythonLanguageDefinition());

    m_editor.SetPalette(TextEditor::GetDarkPalette());
}

void CodeEditorPanel::OpenFile(const std::filesystem::path& path, EditorViewMode& currentViewMode) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "[CodeEditor] Cannot open file, does not exist: " << path.string() << std::endl;
        return;
    }

    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // Inject the file content into the text editor
        m_editor.SetText(buffer.str());
        m_currentFilePath = path;
        file.close();
        
        std::cout << "[CodeEditor] Opened: " << path.filename().string() << std::endl;
        
        // Automatically switch the user's view to the Code tab
        currentViewMode = EditorViewMode::Code;
    }
}

void CodeEditorPanel::SaveFile() {
    if (m_currentFilePath.empty()) return;

    std::ofstream file(m_currentFilePath);
    if (file.is_open()) {
        // Retrieve the modified text and save it
        file << m_editor.GetText();
        file.close();
        std::cout << "[CodeEditor] Saved: " << m_currentFilePath.filename().string() << std::endl;
    }
}

void CodeEditorPanel::Draw(EditorViewMode& currentViewMode) {
    // Build the window title dynamically (Show a '*' if the file is modified)
    std::string windowTitle = "Code Editor";
    if (!m_currentFilePath.empty()) {
        windowTitle += " - " + m_currentFilePath.filename().string();
        if (m_editor.IsTextChanged()) {
            windowTitle += " *";
        }
    }

    windowTitle += "###CodeEditorWindow";

    ImGui::Begin(windowTitle.c_str());

    // Toolbar for the Code Editor
    if (ImGui::Button("Save") || (ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))) {
        SaveFile();
    }
    
    ImGui::Separator();

    if (m_currentFilePath.empty()) {
        ImGui::TextDisabled("Double-click a .py file in the Project Panel to open it.");
    } else {
        // Render the advanced text editor widget taking the remaining space
        m_editor.Render("PythonEditor");
    }

    ImGui::End();
}

/** Copyright 2022 Mitchell. See LICENSE. */

#include <assert.h>
#include <string.h>
#if !_WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include <algorithm> // std::replace
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <utility> // std::pair
#include <vector>

#include "ILexer.h"
#include "Scintilla.h"

#include "PropSetSimple.h"
#include "LexerModule.h"
#include "DefaultLexer.h"

#include "tree_sitter/api.h"

#include "TSLexia.h"

using namespace Scintilla;
using namespace Lexilla;

#if !_WIN32
using Function = void *;
using Module = void *;
#else
using Function = FARPROC;
using Module = HMODULE;
#endif
using TSLanguageFactory = TSLanguage *(*)(void);

namespace {
// Default highlight capture names to style numbers.
const std::vector<std::pair<std::string, int>> defaultStyles = {
  {"keyword", TSLEXIA_KEYWORD},
  {"operator", TSLEXIA_OPERATOR},
  {"delimiter", TSLEXIA_OPERATOR},
  {"string", TSLEXIA_STRING},
  {"constant", TSLEXIA_CONSTANT},
  {"number", TSLEXIA_NUMBER},
  {"function", TSLEXIA_FUNCTION},
  {"function.special", TSLEXIA_FUNCTION},
  {"property", TSLEXIA_PROPERTY},
  {"label", TSLEXIA_LABEL},
  {"type", TSLEXIA_TYPE},
  {"variable", TSLEXIA_VARIABLE},
  {"comment", TSLEXIA_COMMENT},
};
} // namespace

/** The Tree-sitter Scintilla lexer. */
class TSLexia : public DefaultLexer {
  /** The set of properties for the lexer. */
  PropSetSimple props;

  std::vector<TSLanguage *> languages; // static pointers, so no memory management needed
  std::vector<std::unique_ptr<TSParser, decltype(&ts_parser_delete)>> parsers;
  std::vector<std::unique_ptr<TSQuery, decltype(&ts_query_delete)>> queries;
  std::map<std::string, int> styles;

public:
  // Lexer property keys.
  static constexpr const char *LexerErrorKey = "lexer.ts.error";

  /** Constructor. */
  TSLexia(const char *paths);

  /** Destructor. */
  virtual ~TSLexia() = default;

  /** Destroys the lexer object. */
  void SCI_METHOD Release() override;

  /**
   * Specifies the path to the query file for the n-th language parser.  This query file contains
   * patterns that match nodes and use named captures for assigning styles to those nodes. The
   * `styles` map contains the capture name to style number mapping.
   * `SetIdentifiers()` is used to assign unknown capture names to styles.
   * See https://tree-sitter.github.io/tree-sitter/using-parsers#pattern-matching-with-queries
   * for more information on query syntax.
   * @param n The n-th language parser to use the given query file for.
   * @param path The path to the query file to load.
   * @see SetIdentifiers
   */
  Sci_Position SCI_METHOD WordListSet(int n, const char *path) override;

  /**
   * Lexes the Scintilla document.
   * @param startPos The position in the document to start lexing at.
   * @param lengthDoc The number of bytes in the document to lex.
   * @param initStyle The initial style at position *startPos* in the document.
   * @param buffer The document interface.
   */
  void SCI_METHOD Lex(
    Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *buffer) override;

  /**
   * Folds the Scintilla document.
   * @param startPos The position in the document to start folding at.
   * @param lengthDoc The number of bytes in the document to fold.
   * @param initStyle The initial style at position *startPos* in the document.
   * @param buffer The document interface.
   */
  void SCI_METHOD Fold(
    Sci_PositionU startPos, Sci_Position lengthDoc, int, IDocument *buffer) override;

  /**
   * Sets the *key* lexer property to *value*.
   * @param key The string property key.
   * @param val The string value.
   */
  Sci_Position SCI_METHOD PropertySet(const char *key, const char *value) override;

  /**
   * Assigns capture name *name* to style number *style*.
   * @param style Style number to use for *name*.
   * @param name Capture name to assign *style* to.
   */
  void SCI_METHOD SetIdentifiers(int style, const char *name) override;

  /**
   * Returns the lexer property for *key*.
   * @param key The string property key.
   */
  const char *SCI_METHOD PropertyGet(const char *key) override;
};

TSLexia::TSLexia(const char *paths) : DefaultLexer("tree-sitter", 0) {
  std::string_view view{paths};
  size_t start, end = 0;
  while ((start = view.find_first_not_of(';', end)) != std::string::npos) {
    end = view.find(';', start);
    const std::string path{view.substr(start, end - start)};

    auto name = std::filesystem::path(path).stem().string();
    if (name.find("lib") == 0) name = name.substr(3); // strip "lib"
    std::replace(name.begin(), name.end(), '-', '_');
    std::string errmsg;
#if !_WIN32
    Module lib = dlopen(path.c_str(), RTLD_LAZY);
#else
    Module lib = LoadLibraryA(path.c_str());
#endif
    if (!lib) {
      errmsg = "Cannot open parser: ";
      errmsg += path;
      PropertySet(LexerErrorKey, errmsg.c_str());
      return;
    }
#if !_WIN32
    Function f = dlsym(lib, name.c_str());
#else
    Function f = GetProcAddress(lib, name.c_str());
#endif
    if (!f) {
      errmsg = "Cannot find parser symbol: ";
      errmsg += name;
      PropertySet(LexerErrorKey, errmsg.c_str());
      return;
    }
    languages.push_back(((TSLanguageFactory)f)());
    parsers.emplace_back(ts_parser_new(), ts_parser_delete);
    ts_parser_set_language(parsers.back().get(), languages.back());
    queries.emplace_back(nullptr, ts_query_delete);
  }

  for (const auto &pair : defaultStyles) styles[pair.first] = pair.second;
}

void SCI_METHOD TSLexia::Release() { delete this; }

Sci_Position SCI_METHOD TSLexia::WordListSet(int n, const char *path) {
  if (static_cast<size_t>(n) >= languages.size()) return -1; // TODO: log error

  std::ifstream file{path};
  if (!file.is_open()) return -1; // TODO: log error
  std::string s{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  file.close();

  uint32_t offset;
  TSQueryError error;
  queries[n].reset(ts_query_new(languages[n], s.data(), s.length(), &offset, &error));
  std::string errmsg;
  if (!queries[n]) {
    errmsg = "Query ";
    switch (error) {
    case TSQueryErrorSyntax: errmsg += "syntax"; break;
    case TSQueryErrorNodeType: errmsg += "node type"; break;
    case TSQueryErrorField: errmsg += "field"; break;
    case TSQueryErrorCapture: errmsg += "capture"; break;
    case TSQueryErrorStructure: errmsg += "structure"; break;
    case TSQueryErrorLanguage: errmsg += "language"; break;
    default: break;
    }
    errmsg += " error at ";
    errmsg += std::to_string(offset);
  }
  PropertySet(LexerErrorKey, errmsg.c_str());
  return 0;
}

void SCI_METHOD TSLexia::Lex(
  Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *buffer) {
  buffer->StartStyling(startPos);
  buffer->SetStyleFor(lengthDoc, STYLE_DEFAULT);

  for (size_t i = 0; i < languages.size(); i++) {
    const auto &parser = parsers[i];
    const auto &query = queries[i];

    const TSTree *tree =
      ts_parser_parse_string(parser.get(), nullptr, buffer->BufferPointer(), buffer->Length());
    std::unique_ptr<TSQueryCursor, decltype(&ts_query_cursor_delete)> cursor{
      ts_query_cursor_new(), ts_query_cursor_delete};
    ts_query_cursor_exec(cursor.get(), query.get(), ts_tree_root_node(tree));
    TSQueryMatch match;

    Sci_PositionU endStyled = startPos;
    uint32_t index;
    while (ts_query_cursor_next_capture(cursor.get(), &match, &index)) {
      const TSQueryCapture &capture = match.captures[index];
      const Sci_PositionU nodeStartPos = ts_node_start_byte(capture.node);
      const Sci_PositionU nodeEndPos = ts_node_end_byte(capture.node);
      if (nodeEndPos <= endStyled) continue; // already styled

      uint32_t length;
      const TSQueryPredicateStep *predicates =
        ts_query_predicates_for_pattern(query.get(), match.pattern_index, &length);
      bool failedPredicate = false;
      for (index = 0; index + 2 < length; index += 3) {
        const TSQueryPredicateStep &op = predicates[index];
        const TSQueryPredicateStep &capture = predicates[index + 1];
        const TSQueryPredicateStep &arg = predicates[index + 2];
        if (op.type != TSQueryPredicateStepTypeString ||
          capture.type != TSQueryPredicateStepTypeCapture ||
          arg.type != TSQueryPredicateStepTypeString) {
          failedPredicate = true; // TODO: log error
          break;
        }

        uint32_t valueLength;
        const std::string_view op_name =
          ts_query_string_value_for_id(query.get(), op.value_id, &valueLength);
        const std::string_view capture_name =
          ts_query_capture_name_for_id(query.get(), capture.value_id, &valueLength);
        const std::string_view arg_value =
          ts_query_string_value_for_id(query.get(), arg.value_id, &valueLength);

        const std::string_view text{
          buffer->BufferPointer() + nodeStartPos, nodeEndPos - nodeStartPos};

        if ((op_name == "eq?" && text != arg_value) ||
          (op_name == "not-eq?" && text == arg_value)) {
          failedPredicate = true;
          break;
        } else if (op_name == "match?" || op_name == "not-match?") {
          const std::regex re(arg_value.data(), arg_value.length());
          const bool matched = std::regex_match(text.begin(), text.end(), re);
          if ((op_name == "match?" && !matched) || (op_name == "not-match?" && matched)) {
            failedPredicate = true;
            break;
          }
        }
      } // TODO: log error for invalid/ignored predicates
      if (failedPredicate) continue;

      const std::string name = ts_query_capture_name_for_id(query.get(), capture.index, &length);
      if (auto it = styles.find(name); it != styles.end()) {
        buffer->StartStyling(nodeStartPos);
        buffer->SetStyleFor(nodeEndPos - nodeStartPos, it->second);
      }
      endStyled = nodeEndPos;
      if (endStyled >= startPos + lengthDoc) break; // done
    }
  }
}

void SCI_METHOD TSLexia::Fold(
  Sci_PositionU startPos, Sci_Position lengthDoc, int, IDocument *buffer) {}

Sci_Position SCI_METHOD TSLexia::PropertySet(const char *key, const char *value) {
  props.Set(key, value);
  return -1; // do not re-lex
}

void SCI_METHOD TSLexia::SetIdentifiers(int style, const char *name) {
  if (style > 255) return; // TODO: log error
  styles[std::string(name)] = style;
}

const char *SCI_METHOD TSLexia::PropertyGet(const char *key) { return props.Get(key); }

#if (_WIN32 && !NO_DLL)
#define EXPORT_FUNCTION __declspec(dllexport)
#define CALLING_CONVENTION __stdcall
#else
#define EXPORT_FUNCTION __attribute__((visibility("default")))
#define CALLING_CONVENTION
#endif // _WIN32

extern "C" {

/** Returns 1, the number of lexers defined in this file. */
EXPORT_FUNCTION int CALLING_CONVENTION GetLexerCount() { return 1; }

/**
 * Copies the name of the lexer into buffer *name* of size *len*.
 * @param index 0, the lexer number to get the name of.
 * @param name The buffer to copy the name of the lexer into.
 * @param len The size of *name*.
 */
EXPORT_FUNCTION void CALLING_CONVENTION GetLexerName(unsigned int index, char *name, int len) {
  *name = '\0';
  if ((index == 0) && (len > static_cast<int>(strlen("tree-sitter")))) strcpy(name, "tree-sitter");
}

/** Returns the lexer namespace used by Tree-sitter. */
EXPORT_FUNCTION const char *CALLING_CONVENTION GetNameSpace() { return "tree-sitter"; }

/**
 * Creates and returns a new lexer for the Tree-sitter parser(s) in path list *paths*.
 * Parser filenames are of the form "libtree-sitter-[language].{so,dll}", where the "lib"
 * prefix is optional and "language" is the name of the parser. Inside each shared library is
 * a function symbol "tree_sitter_[language]" that returns a TSLanguage* pointer.
 * @param paths A list of paths to the Tree-sitter parser(s) to load, separated by ';'.
 * @usage CreateLexer("/path/to/libtree-sitter-c.so")
 * @usage CreateLexer("/path/to/libtree-sitter-html.so;/path/to/libtree-sitter-css.so")
 */
EXPORT_FUNCTION ILexer5 *CALLING_CONVENTION CreateLexer(const char *path) {
  auto lexer = new TSLexia(path);
  if (strlen(lexer->PropertyGet(TSLexia::LexerErrorKey)) > 0) {
    lexer->Release();
    return nullptr;
  }
  return lexer;
}
}

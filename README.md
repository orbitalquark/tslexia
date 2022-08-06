# TSLexia

TSLexia is a [Scintilla][] lexer that makes use of [Tree-sitter][] parsers.

[Scintilla]: https://scintilla.org
[Tree-sitter]: https://tree-sitter.github.io/tree-sitter

## Compile

TSLexia requires only a C compiler and a C++ compiler that supports C++17.

    make deps
    make # or 'make win' for Windows dlls
    make parsers

There are now *libtslexia.so* and *libtree-sitter-[name].so* (or *TSLexia.dll* and
*tree-sitter-[name].dll*) libraries you can use with your Scintilla application.

## Usage

1. In your application, `#include "TSLexia.h"`.
2. Dynamically load the *libtslexia.so* or *TSLexia.dll* library if needed.
3. Call `CreateLexer()` with the path to a Tree-sitter parser,
  e.g. `CreateLexer("/path/to/libtree-sitter-c.so")`. For multi-language
  files or for parsers that depend on others, use a ';'-separated list of paths,
  e.g. `CreateLexer("libtree-sitter-c.so;libtree-sitter-cpp.so")`. Make sure the order is from
  generic to specific or parent to child.
4. Use the returned lexer with Scintilla's `SCI_SETILEXER` message to set it.
5. Use Scintilla's `SCI_SETKEYWORDS` message to pass in a Tree-sitter query file that matches
  nodes and uses named captures for assigning styles to those nodes, e.g.  `SendScintilla(sci,
  SCI_SETKEYWORDS, 0, (sptr_t)"/path/to/tslexia/queries/c.scm")`. When using multiple parsers,
  use the index of the parser passed to `CreateLexer()`.
6. Define style settings for TSLexia's `TSLEXIA_*` styles (see *TSLexia.h*).
7. That's it!

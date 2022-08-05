# TSLexia

TSLexia is a [Scintilla][] lexer that makes use of [Tree-sitter][] parsers.

[Scintilla]: https://scintilla.org
[Tree-sitter]: https://tree-sitter.github.io/tree-sitter

## Compile

    make deps
    make # or 'make win' for Windows dlls
    make parsers

There are now *libtslexia.so* and *libtree-sitter-[name].so* (or *TSLexia.dll* and
*tree-sitter-[name].dll* libraries) you can use with your Scintilla application.

## Usage

1. In your application, `#include "TSLexia.h"` or dynamically load the *libtslexia.so* or
  *TSLexia.dll* library.
2. Call `CreateLexer()` with the path to a Tree-sitter parser,
  e.g. `CreateLexer("/path/to/libtree-sitter-c.so")`.
3. Use the returned lexer with Scintilla's `SCI_SETILEXER` message to set it.
4. Use Scintilla's `SCI_SETKEYWORDS` message to pass in a Tree-sitter query file that matches
  nodes and uses named captures for assigning styles to those nodes, e.g.
  `SendScintilla(sci, SCI_SETKEYWORDS, 0, (uptr_t)"/path/to/tslexia/queries/c.scm")`.
5. Define style settings for TSLexia's `TSLEXIA_*` styles (see *TSLexia.h*).
6. That's it!

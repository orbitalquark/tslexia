# Copyright 2022 Mitchell. See LICENSE.

ifeq (win, $(MAKECMDGOALS))
  CC := x86_64-w64-mingw32-gcc
  CXX := x86_64-w64-mingw32-g++
  LDFLAGS := -g -static -mwindows -s TSLexia.def -Wl,--enable-stdcall-fixup
  LDLIBS :=
  so_prefix :=
  so := dll
  lexer := TSLexia.dll
else
  CC := gcc -fPIC
  CXX := g++ -fPIC
  LDFLAGS := -g -Wl,-soname,libtslexia.so -Wl,-fvisibility=hidden
  LDLIBS := -ldl
  so_prefix := lib
  so := so
  lexer := libtslexia.so
endif
WGET = wget -O $@

# Scintilla and Lexilla.
sci_flags = --std=c++17 -g -pedantic -Iscintilla/include -Ilexilla/include -Ilexilla/lexlib \
  -DSCI_LEXER -W -Wall -Wno-unused
lex_objs = PropSetSimple.o WordList.o LexerModule.o LexerSimple.o LexerBase.o Accessor.o \
  DefaultLexer.o

# Tree-sitter.
ts_flags = -Itree-sitter/lib/include -Itree-sitter/lib/src
ts_objs = lib.o

all: $(lexer)
win: $(lexer)
deps: scintilla lexilla tree-sitter

$(lex_objs): %.o: lexilla/lexlib/%.cxx ; $(CXX) $(sci_flags) -c $<
$(ts_objs): %.o: tree-sitter/lib/src/%.c ; $(CC) -Os $(ts_flags) -c $<
TSLexia.o: TSLexia.cxx ; $(CXX) -g $(sci_flags) $(ts_flags) -c $<
$(lexer): $(lex_objs) $(ts_objs) TSLexia.o ; $(CXX) -g -shared -o $@ $(LDFLAGS) $(LDLIBS) $^
clean: ; rm -f *.o *.so *.dll

# External dependencies.

github_dl = https://github.com/tree-sitter/$(1)/archive/refs/tags/$(patsubst $(1)-%,v%, $@)

ts_zip = tree-sitter-0.20.6.zip
$(ts_zip): ; $(WGET) $(call github_dl,tree-sitter)
tree-sitter: | $(ts_zip) ; unzip -d $@ $| && mv $@/*/* $@

# Language parsers.

.PHONY: parsers
languages := c
parsers: $(addprefix $(so_prefix)tree-sitter-,$(languages)).$(so)

c_zip = tree-sitter-c-0.20.2.zip
$(c_zip): ; $(WGET) $(call github_dl,tree-sitter-c)
parsers/tree-sitter-c: | $(c_zip)
	unzip -d $@ $| && mv $@/*/* $@
	cp $@/queries/highlights.scm queries/$(patsubst tree-sitter-%,%, $(notdir $@)).scm
libtree-sitter-c.so: parsers/tree-sitter-c ; $(CC) -shared -o $@ -I $</src $</src/*.c

/** Copyright 2022 Mitchell. See LICENSE. */

#ifndef TSLEXIA_H
#define TSLEXIA_H

#ifdef __cplusplus
#include "ILexer.h"
#define ILEXER5 Scintilla::ILexer5
extern "C" {
#else
#define ILEXER5 void
#endif

ILEXER5 *CreateLexer(const char *paths);

enum {
  TSLEXIA_KEYWORD,
  TSLEXIA_OPERATOR,
  TSLEXIA_STRING,
  TSLEXIA_CONSTANT,
  TSLEXIA_NUMBER,
  TSLEXIA_FUNCTION,
  TSLEXIA_PROPERTY,
  TSLEXIA_LABEL,
  TSLEXIA_TYPE,
  TSLEXIA_VARIABLE,
  TSLEXIA_COMMENT,
  TSLEXIA_MAX
};

#ifdef __cplusplus
}
#endif

#endif

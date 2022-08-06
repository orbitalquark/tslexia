(shebang) @comment

(break_statement) @keyword
[
"do"
"else"
"elseif"
"end"
"for"
"function"
"goto"
"if"
"in"
"local"
"repeat"
"return"
"then"
"until"
"while"
] @keyword

[
"#"
"%"
"&"
"("
")"
"*"
"+"
"-"
"."
".."
"/"
"//"
":"
"<"
"<<"
"<="
"="
"=="
">"
">="
">>"
"["
"]"
"^"
"{"
"|"
"}"
"~"
"~="
] @operator

["," ";"] @delimiter

["and" "not" "or"] @keyword.operator

(local_function_definition_statement
  name: (identifier) @function)

(function_definition_statement
  name: (identifier) @function)

(goto_statement
  name: (identifier) @label)
(label_statement) @label

(attribute) @property

(call
  function: [
    (variable)
    method: (identifier)
  ] @function)

(string) @string

(number) @number

(true) @constant
(false) @constant
(nil) @constant

((identifier) @error
  (#match? @error "break|do|else|elseif|end|for|function|goto|if|in|local|repeat|return|then|until|while|true|false|nil"))

(identifier) @variable

(comment) @comment

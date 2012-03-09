S := T_INT T_WORD '(' ')' '{' VarDecl+ '}'
VarDecl+ := VarDecl | VarDecl VarDecl
VarDecl := T_INT T_WORD '=' Exp ';'
Exp := '!' T_NUMBER | T_NUMBER

S := Decl+
Decl := VarDecl | FunDecl
VarDecl := Type T_WORD '=' Exp
FunDecl := RetType T_WORD '(' FArgs ')' '{' Stmt+ '}' | RetType T_WORD '(' ')' '{' Stmt+ '}' | RetType T_WORD '(' FArgs ')' '{' VarDecl+ Stmt+ '}' | RetType T_WORD '(' ')' '{' VarDecl+ Stmt+ '}'
RetType := Type | T_VOID
Type := T_INT | T_BOOL | '(' Type ',' Type ')' | '[' Type ']' | T_WORD
FArgs := Type T_WORD | Type T_WORD ',' FArgs
Stmt := '{' '}' | '{' Stmt+ '}' | T_IF '(' Exp ')' Stmt T_ELSE Stmt | T_IF '(' Exp ')' Stmt | T_WHILE '(' Exp ')' Stmt | T_WORD '=' Exp ';' | FunCall ';' | T_RETURN Exp ';'
Exp := T_WORD | Exp Op2 Exp | Op1 Exp | T_NUMBER | T_FALSE | T_TRUE | '(' Exp ')' | FunCall | '[' ']' | '(' Exp ',' Exp ')'
FunCall := T_WORD '(' ')' | T_WORD '(' ActArgs ')'
ActArgs := Exp | Exp ',' ActArgs
Op2 := '+' | '-' | '*' | '/' | '%' | T_EQ | '<' | '>' | T_LTE | T_GTE | T_NE | T_AND | T_OR | ':'
Op1 := '!' | '-'
Decl+ := Decl | Decl Decl+
Stmt+ := Stmt | Stmt Stmt+
VarDecl+ := VarDecl | VarDecl VarDecl+

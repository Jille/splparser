S := Decl+
Decl := VarDecl | FunDecl
VarDecl := Type T_WORD '=' Exp ';'
FunDecl := RetType T_WORD '(' FArgs ')' '{' Stmt+ '}' | RetType T_WORD '(' ')' '{' Stmt+ '}' | RetType T_WORD '(' FArgs ')' '{' VarDecl+ Stmt+ '}' | RetType T_WORD '(' ')' '{' VarDecl+ Stmt+ '}'
RetType := Type | T_VOID
Type := T_INT | T_BOOL | '(' Type ',' Type ')' | '[' Type ']' | T_WORD
FArgs := Type T_WORD | Type T_WORD ',' FArgs
Stmt := '{' '}' | '{' Stmt+ '}' | If | While | Assignment | FunCall ';' | Return
If := T_IF '(' Exp ')' Stmt T_ELSE Stmt | T_IF '(' Exp ')' Stmt
While := T_WHILE '(' Exp ')' Stmt
Return := T_RETURN Exp ';' | T_RETURN ';'
Assignment := T_WORD '=' Exp ';'
FunCall := T_WORD '(' ')' | T_WORD '(' ActArgs ')'
ActArgs := Exp | Exp ',' ActArgs
CompOp := T_EQ | '<' | '>' | T_LTE | T_GTE | T_NE | T_AND | T_OR
AddOp := '+' | '-'
MultOp := '*' | '/' | '%'
Exp := Exp2 ':' Exp | Exp2
Exp2 := '!' Exp2 | Exp3
Exp3 := Exp4 CompOp Exp3 | Exp4
Exp4 := Exp5 AddOp Exp4 | Exp5
Exp5 := Exp6 MultOp Exp5 | Exp6
Exp6 := '-' Exp6 | Exp7
Exp7 := T_WORD | T_NUMBER | T_FALSE | T_TRUE | '(' Exp ')' | FunCall | '[' ']' | '(' Exp ',' Exp ')'
Decl+ := Decl | Decl Decl+
Stmt+ := Stmt | Stmt Stmt+
VarDecl+ := VarDecl | VarDecl VarDecl+

%{
    #include "cilisp.h"
    //#define ylog(r, p, t) {printf("BISON: %s ::= %s (%p)\n", #r, #p, t);}
    #define ylog(r, p, t) {}
    int yylex();
    void yyerror(char*, ...);
%}

%union {
    double dval;
    int ival;
    char *id;
    struct ast_node *astNode;  // NOTE: AST_NODES hold either AST_NUMBER or AST_FUNCTION
                               // in nodeptr->data.number/function
    struct symbol_table_node *symNode;
};                             

%token <ival> FUNC
%token <dval> INT DOUBLE
%token <id> SYMBOL
%token QUIT EOL EOFT LPAREN RPAREN LET

%type <astNode> number s_expr f_expr s_expr_list s_expr_section  
%type <symNode> let_elem let_list let_section

%%

program:
    s_expr EOL {
        ylog(program, s_expr EOL, 0);
        if ($1) {
            printRetVal(eval($1));
            freeNode($1);
        }
        YYACCEPT;
    }
    | s_expr EOFT {
        ylog(program, s_expr EOFT, 0);
        if ($1) {
            printRetVal(eval($1));
            freeNode($1);
        }
        exit(EXIT_SUCCESS);
    }
    | EOL {
        ylog(program, EOL, 0);
        YYACCEPT;  // paranoic; main skips blank lines
    }
    | EOFT {
        ylog(program, EOFT, 0);
        exit(EXIT_SUCCESS);
    };


s_expr:
    f_expr {
        ylog(s_expr, f_expr, $1);
        $$ = $1;
    }
    | number {
        ylog(s_expr, number, $1);
        $$ = $1;
    }
    | SYMBOL {
        ylog(s_expr, SYMBOL, $1);
        $$ = createSymbolNode($1); 
    }
    | LPAREN let_section s_expr RPAREN {
        ylog(s_expr, LPAREN let_section s_expr RPAREN, $2);
        $$ = createScopeNode($2, $3);
    }
    | QUIT {
        ylog(s_expr, QUIT, 0);
        exit(EXIT_SUCCESS);
    }
    | error {
        ylog(s_expr, error, 0);
        yyerror("unexpected token");
        $$ = NULL;
    };

f_expr:
    LPAREN FUNC s_expr_section RPAREN {
        ylog(f_expr, LPAREN  FUNC s_expr_section RPAREN, $3);
        $$ = createFunctionNode($2, $3);
    };

s_expr_section:
    s_expr_list {
        ylog(s_expr_section, s_expr_list, $1);
        $$ = $1;
    }
    |   { ylog(s_expr_section, <empty>, 0); $$ = NULL; };  

let_section:
    LPAREN LET let_list RPAREN {
        ylog(let_section, LPAREN let let_list RPAREN, 0);
        // TODO: idk if I should create a symbol or scope node or just do this:
        $$ = $3;
    };

s_expr_list:
    s_expr {
        ylog(s_expr_list, s_expr, $1);
        $$ = $1;
    }
    | s_expr s_expr_list {
        ylog(s_expr_list, s_expr s_expr_list, $1);
        // Add new s_expr to list
        $$ = addExpressionToList($1, $2);
    };
                        // Creates a symbol table list
let_list:
    let_elem {
        ylog(let_list, let_elem, $1);
        $$ = $1;
    }
    | let_elem let_list {
        ylog(let_list, let_elem let_list, 0);
        $$ = addSymbolToList($1, $2);
    };


let_elem:       
    LPAREN SYMBOL s_expr RPAREN {                           // TODO: What if the same symbol is defined twice
        ylog(let_elem, LPAREN SYMBOL s_expr RPAREN, $3); 
        $$ = createSymbolTableNode($2, $3);
    };

number: 
    INT {
        ylog(number, INT, 0);
        $$ = createNumberNode($1, INT_TYPE);
    }
    |
    DOUBLE {
        ylog(number, DOUBLE, 0);
        $$ = createNumberNode($1, DOUBLE_TYPE);
    };
%%


%{
    #include "cilisp.h"
    //#define ylog(r, p, t) {printf("BISON: %s ::= %s (%d)\n", #r, #p, t);}
    #define ylog(r, p, t) {}
    int yylex();
    void yyerror(char*, ...);
%}

%union {
    double dval;
    int ival;
    struct ast_node *astNode;  // NOTE: AST_NODES hold either AST_NUMBER or AST_FUNCTION
};                             // in nodeptr->data.number/function

%token <ival> FUNC
%token <dval> INT DOUBLE
%token QUIT EOL EOFT LPAREN RPAREN

%type <astNode> number s_expr f_expr s_expr_list s_expr_section  


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
        // Evaluate f_expr
        $$ = $1;
    }
    | number {
        ylog(s_expr, number, $1);
        // Evaluate number TODO: make sure number is AST_NODE*
        $$ = $1;
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
    LPAREN FUNC s_expr_section RPAREN {                 // TODO: Should I be tokenizing parens?
        ylog(f_expr, LPAREN  FUNC s_expr_section RPAREN, $3);
        // NOTE: I think all I need to do here is this
        // 
        $$ = createFunctionNode($2, $3);
    };

s_expr_section:
    s_expr_list {
        ylog(s_expr_section, s_expr_list, $1);
        $$ = $1;
    }
    |   { ylog(s_expr_section, <empty>, 0); $$ = NULL; };  

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


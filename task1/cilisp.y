%{
    #include "cilisp.h"
    #define ylog(r, p) {printf("BISON: %s ::= %s \n", #r, #p);}
    int yylex();
    void yyerror(char*, ...);
%}
  // NOTE: All terminals are stored in AST_NODES
  // AST_NODES hold either AST_NUMBER or AST_FUNCTION in nodeptr->data.number/function

%union {
    double dval;
    int ival;
    struct ast_node *astNode;
};

%token <ival> FUNC
%token <dval> INT DOUBLE
%token QUIT EOL EOFT

%type <astNode> s_expr number s_expr_list s_expr_section // TODO: Idk if sec. is supposed to be
                                                         // here data structure for sec?

%%

program:
    s_expr EOL {
        ylog(program, s_expr EOL);
        if ($1) {
            printRetVal(eval($1));
            freeNode($1);
        }
        YYACCEPT;
    }
    | s_expr EOFT {
        ylog(program, s_expr EOFT);
        if ($1) {
            printRetVal(eval($1));
            freeNode($1);
        }
        exit(EXIT_SUCCESS);
    }
    | EOL {
        ylog(program, EOL);
        YYACCEPT;  // paranoic; main skips blank lines
    }
    | EOFT {
        ylog(program, EOFT);
        exit(EXIT_SUCCESS);
    };


s_expr:
    QUIT {
        ylog(s_expr, QUIT);
        exit(EXIT_SUCCESS);
    }
    | error {
        ylog(s_expr, error);
        yyerror("unexpected token");
        $$ = NULL;
    };

%%


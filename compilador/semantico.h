/* Analise de Escopo (cap. 5), de Tipos (cap. 6) e Geracao de Codigo (cap. 7). */

#ifndef SEMANTICO_H
#define SEMANTICO_H

#include <string>
#include <vector>

/* naturezas dos identificadores (fig. 6.1) */
typedef enum { NO_KIND_DEF_ = -1, VAR_, PARAM_, FUNCTION_, FIELD_,
               ARRAY_TYPE_, STRUCT_TYPE_, ALIAS_TYPE_, SCALAR_TYPE_,
               UNIVERSAL_ } t_kind;

#define IS_TYPE_KIND(k)  ((k) == ARRAY_TYPE_ || (k) == STRUCT_TYPE_ || \
                          (k) == ALIAS_TYPE_ || (k) == SCALAR_TYPE_)

/* fig. 6.2 sem a uniao: em C++11 um std::vector nao pode estar numa union
 * sem construtores manuais. Cada campo so e usado pelo kind indicado. */
struct object {
    int     nName;                     /* token secundario do nome          */
    t_kind  eKind;
    object *pType;                     /* Var, Param, Field                 */
    object *pRetType;                  /* Function                          */
    std::vector<object *> vParams;     /* Function (no lugar de pNext)      */
    object *pElemType;                 /* Array                             */
    int     nNumElems;                 /* Array                             */
    std::vector<object *> vFields;     /* Struct (no lugar de pNext)        */
    object *pBaseType;                 /* Alias                             */
    int     nIndex;                    /* Var, Param: posicao no frame;
                                          Field: deslocamento na struct;
                                          Function: numero da funcao (cap. 7) */
    int     nSize;                     /* Array, Struct: tamanho em palavras */

    explicit object(int name = -1, t_kind kind = NO_KIND_DEF_)
        : nName(name), eKind(kind), pType(nullptr), pRetType(nullptr),
          pElemType(nullptr), nNumElems(0), pBaseType(nullptr),
          nIndex(0), nSize(1) {}
};

extern object *pInteger, *pChar, *pBool, *pString, *pUniversal;

/* tabela de simbolos (fig. 5.3 a 5.7) */
int     NewBlock(void);
int     EndBlock(void);
object *Define(int aName);
object *Search(int aName);
object *Find(int aName);

bool    CheckTypes(object *t1, object *t2);
object *BaseType(object *t);        /* segue a cadeia de alias            */
int     Size(object *t);            /* tamanho do tipo em palavras (cap. 7) */

/* funcoes na ordem de declaracao: Functions[f]->nIndex == f */
extern std::vector<object *> Functions;

/* erros: lexicos e sintaticos tambem sao registrados aqui */
typedef enum {
    ERR_LEXICAL, ERR_SYNTAX,
    ERR_REDECL, ERR_NOT_DECL,                                     /* escopo */
    ERR_TYPE_EXPECTED, ERR_BOOL_TYPE_EXPECTED, ERR_TYPE_MISMATCH,  /* tipos  */
    ERR_INVALID_TYPE, ERR_KIND_NOT_STRUCT, ERR_FIELD_NOT_DECL,
    ERR_KIND_NOT_ARRAY, ERR_INVALID_INDEX_TYPE, ERR_KIND_NOT_VAR,
    ERR_KIND_NOT_FUNC, ERR_TOO_MANY_ARGS, ERR_PARAM_TYPE, ERR_TOO_FEW_ARGS,
    ERR_RETURN_TYPE, ERR_NOT_IN_LOOP
} t_error;

struct t_pos { int line, col; };

struct t_errorMsg {
    t_error     code;
    t_pos       pos;
    std::string text;
};

extern std::vector<t_errorMsg> Errors;
extern t_pos RulePos;            /* inicio da regra reduzida, fornecido pelo sintatico */
extern int   nShiftedSecondary;  /* token secundario do ultimo terminal empilhado      */

void        Error(t_error code, const std::string &arg = "");
const char *ErrorName(t_error code);

void InitSemantics(void);
void Semantics(int rule);

#endif /* SEMANTICO_H */

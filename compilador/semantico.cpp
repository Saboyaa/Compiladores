/* Analise de Escopo (cap. 5) e de Tipos (cap. 6) por meio da funcao
 * Semantics(), chamada pelo analisador sintatico a cada reducao (fig. 4.6).
 * Cada caso do switch traz, em comentario, a figura do livro que implementa
 * e as correcoes adotadas (README, secao 7). */

#include <cstdio>
#include <deque>
#include "gramatica.h"
#include "semantico.h"

/* ------------------------------------------------------------------ */
/* tipos pre-definidos (fig. 6.3)                                     */
/* ------------------------------------------------------------------ */

static object int_(-1, SCALAR_TYPE_), char_(-1, SCALAR_TYPE_),
              bool_(-1, SCALAR_TYPE_), string_(-1, SCALAR_TYPE_),
              universal_(-1, UNIVERSAL_);      /* fig. 6.3 usa SCALAR_TYPE_ */

object *pInteger   = &int_;
object *pChar      = &char_;
object *pBool      = &bool_;
object *pString    = &string_;
object *pUniversal = &universal_;

/* ------------------------------------------------------------------ */
/* tabela de simbolos: pilha de niveis (fig. 5.1 a 5.7)                */
/* ------------------------------------------------------------------ */

static std::vector<std::vector<object *> > SymbolTable;
static std::deque<object> objects;   /* push_back nao move os objetos ja criados */

int NewBlock(void)
{
    SymbolTable.push_back(std::vector<object *>());
    return (int)SymbolTable.size() - 1;
}

int EndBlock(void)
{
    SymbolTable.pop_back();
    return (int)SymbolTable.size() - 1;
}

object *Define(int aName)
{
    objects.push_back(object(aName));
    object *obj = &objects.back();
    SymbolTable.back().push_back(obj);
    return obj;
}

/* a busca no nivel vai do ultimo para o primeiro: so faz diferenca apos
 * uma redeclaracao, que passa a valer para os usos seguintes */
static object *SearchLevel(int level, int aName)
{
    const std::vector<object *> &objs = SymbolTable[level];
    for (int i = (int)objs.size() - 1; i >= 0; --i)
        if (objs[i]->nName == aName)
            return objs[i];
    return nullptr;
}

object *Search(int aName)
{
    return SearchLevel((int)SymbolTable.size() - 1, aName);
}

object *Find(int aName)
{
    object *obj = nullptr;
    for (int i = (int)SymbolTable.size() - 1; i >= 0 && obj == nullptr; --i)
        obj = SearchLevel(i, aName);
    return obj;
}

/* ------------------------------------------------------------------ */
/* compatibilidade de tipos: por nome, com os alias resolvidos         */
/* ------------------------------------------------------------------ */

/* um alias e sinonimo do seu tipo base (cap. 6, fig. 6.9) */
static object *BaseType(object *t)
{
    while (t->eKind == ALIAS_TYPE_)
        t = t->pBaseType;
    return t;
}

bool CheckTypes(object *t1, object *t2)
{
    t1 = BaseType(t1);
    t2 = BaseType(t2);

    if (t1 == t2)
        return true;
    if (t1->eKind == UNIVERSAL_ || t2->eKind == UNIVERSAL_)
        return true;
    return false;
}

/* ------------------------------------------------------------------ */
/* erros                                                               */
/* ------------------------------------------------------------------ */

std::vector<t_errorMsg> Errors;
t_pos RulePos;
int   nShiftedSecondary;

static const struct { const char *name, *text; } errorInfo[] = {
    { "ERR_LEXICAL",            "lexema invalido" },
    { "ERR_SYNTAX",             "erro de sintaxe: %s" },
    { "ERR_REDECL",             "identificador '%s' ja declarado neste escopo" },
    { "ERR_NOT_DECL",           "identificador '%s' nao declarado" },
    { "ERR_TYPE_EXPECTED",      "'%s' nao e um tipo" },
    { "ERR_BOOL_TYPE_EXPECTED", "expressao booleana esperada" },
    { "ERR_TYPE_MISMATCH",      "tipos incompativeis" },
    { "ERR_INVALID_TYPE",       "tipo invalido para o operador" },
    { "ERR_KIND_NOT_STRUCT",    "acesso a campo de algo que nao e struct" },
    { "ERR_FIELD_NOT_DECL",     "campo '%s' nao declarado na struct" },
    { "ERR_KIND_NOT_ARRAY",     "indexacao de algo que nao e array nem string" },
    { "ERR_INVALID_INDEX_TYPE", "indice deve ser inteiro" },
    { "ERR_KIND_NOT_VAR",       "'%s' nao e variavel nem parametro" },
    { "ERR_KIND_NOT_FUNC",      "'%s' nao e funcao" },
    { "ERR_TOO_MANY_ARGS",      "argumentos demais na chamada" },
    { "ERR_PARAM_TYPE",         "argumento %s de tipo incompativel com o parametro" },
    { "ERR_TOO_FEW_ARGS",       "argumentos de menos na chamada" },
    { "ERR_RETURN_TYPE",        "tipo do valor retornado incompativel com o da funcao" },
    { "ERR_NOT_IN_LOOP",        "'%s' fora de comando de repeticao" },
};

const char *ErrorName(t_error code)
{
    return errorInfo[code].name;
}

void Error(t_error code, const std::string &arg)
{
    char text[512];

    snprintf(text, sizeof(text), errorInfo[code].text, arg.c_str());
    Errors.push_back({ code, RulePos, text });
}

static std::string Name(int nName)
{
    const char *s = getName(nName);
    return s ? s : "?";
}

/* ------------------------------------------------------------------ */
/* pilha semantica (fig. 4.7 e 4.8)                                    */
/* ------------------------------------------------------------------ */

/* atributos de todos os nao terminais numa so estrutura; cada nao
 * terminal usa apenas os seus (README, secao 5) */
struct t_attrib {
    int     nont;
    int     name;                    /* IDD, IDU, ID                  */
    object *obj;                     /* IDD, IDU                      */
    object *type;                    /* T, E, L, R, Y, F, LV, MC, ... */
    std::vector<object *> list;      /* LI, DC, LP                    */
    int     val;                     /* NUM, CHR, TRUE, FALSE         */
    object *func;                    /* MC, LE: funcao chamada        */
    int     n;                       /* LE: argumentos ja verificados */
    bool    err;                     /* MC, LE: nao verificar args    */
    t_pos   pos;                     /* inicio do simbolo no fonte    */

    t_attrib() : nont(0), name(-1), obj(nullptr), type(nullptr), val(0),
                 func(nullptr), n(0), err(false), pos() {}
};

static std::vector<t_attrib> StackSem;

static const t_attrib &TopSem(int offFromTop)
{
    return StackSem[StackSem.size() - 1 - offFromTop];
}

static void PopSem(int nToPop)
{
    StackSem.resize(StackSem.size() - nToPop);
}

static void PushSem(const t_attrib &attrib)
{
    StackSem.push_back(attrib);
}

/* ------------------------------------------------------------------ */
/* Semantics                                                           */
/* ------------------------------------------------------------------ */

static object *curFunction;     /* funcao cujo corpo esta sendo analisado */
static int     nLoopDepth;      /* comandos de repeticao abertos          */

void InitSemantics(void)
{
    SymbolTable.clear();
    objects.clear();
    StackSem.clear();
    Errors.clear();
    curFunction = nullptr;
    nLoopDepth  = 0;
    NewBlock();                 /* nivel 0: funcoes e tipos */
}

/* numero de nao terminais no lado direito: tamanho do no a desempilhar */
static int NonTerminalsOf(int rule)
{
    int n = 0;
    for (int s : Rules[rule].right)
        if (!IsTerminal(s))
            n++;
    return n;
}

/* condicao de comando: o erro aponta para a expressao E */
static void CheckCondition(const t_attrib &E)
{
    RulePos = E.pos;
    if (!CheckTypes(E.type, pBool))
        Error(ERR_BOOL_TYPE_EXPECTED);
}

/* operandos de mesmo tipo e o tipo aceito pelo operador (fig. 6.20) */
static object *Arithmetic(object *t1, object *t2, bool acceptString)
{
    if (!CheckTypes(t1, t2))
        Error(ERR_TYPE_MISMATCH);
    if (!CheckTypes(t1, pInteger) && !(acceptString && CheckTypes(t1, pString)))
        Error(ERR_INVALID_TYPE);
    return t1;
}

static object *Relational(object *t1, object *t2)        /* fig. 6.19 */
{
    if (!CheckTypes(t1, t2))
        Error(ERR_TYPE_MISMATCH);
    return pBool;
}

static object *Logical(object *t1, object *t2)           /* fig. 6.18 */
{
    if (!CheckTypes(t1, pBool))
        Error(ERR_BOOL_TYPE_EXPECTED);
    if (!CheckTypes(t2, pBool))
        Error(ERR_BOOL_TYPE_EXPECTED);
    return pBool;
}

static object *IntegerOperand(object *t)                 /* fig. 6.21 */
{
    if (!CheckTypes(t, pInteger))
        Error(ERR_INVALID_TYPE);
    return pInteger;
}

/* verifica o argumento LE.n (a partir de 0) da chamada (fig. 6.23) */
static void CheckArgument(t_attrib &LE, const t_attrib &E)
{
    RulePos = E.pos;
    if (LE.err)
        return;
    if (LE.n >= (int)LE.func->vParams.size()) {
        Error(ERR_TOO_MANY_ARGS);
        LE.err = true;
        return;
    }
    if (!CheckTypes(LE.func->vParams[LE.n]->pType, E.type))
        Error(ERR_PARAM_TYPE, std::to_string(LE.n + 1));
    LE.n++;
}

void Semantics(int rule)
{
    t_attrib A;                 /* no do lado esquerdo da regra */
    object  *p, *t;
    t_pos    posOfRule = RulePos;

    switch (rule) {

    /* ---------------- identificadores (fig. 5.8 e 6.4) ---------------- */
    case IDD_ID:
        A.name = nShiftedSecondary;
        if (Search(A.name) != nullptr)
            Error(ERR_REDECL, Name(A.name));
        p = Define(A.name);               /* objeto novo: o original nao muda */
        p->eKind = NO_KIND_DEF_;
        A.obj = p;
        break;

    case IDU_ID:
        A.name = nShiftedSecondary;
        if ((p = Find(A.name)) == nullptr) {
            Error(ERR_NOT_DECL, Name(A.name));
            p = Define(A.name);
            p->eKind = UNIVERSAL_;        /* evita erros de tipo em cascata */
        }
        A.obj = p;
        break;

    case ID_ID:                           /* campo: resolvido em LV_FIELD */
        A.name = nShiftedSecondary;
        break;

    /* ---------------- marcadores de escopo (fig. 5.9, 6.11, 6.14, 7.18) */
    case NB_RULE:
        NewBlock();
        break;

    case NF_RULE:                         /* DF -> 'function' IDD NF ... */
        p = TopSem(0).obj;
        p->eKind = FUNCTION_;
        NewBlock();                       /* parametros e locais no mesmo nivel */
        break;

    case MF_RULE: {                       /* fig. 6.13: ... LP ')' ':' T MF */
        const t_attrib &T = TopSem(0), &LP = TopSem(1), &IDD = TopSem(3);
        p = IDD.obj;
        p->pRetType = T.type;
        p->vParams  = LP.list;
        curFunction = p;                  /* funcao completa antes do corpo */
        break;
    }

    case ML_RULE:                         /* inicio do corpo de um laco */
        nLoopDepth++;
        break;

    case MC_RULE:                         /* fig. 6.23: F -> IDU MC '(' LE ')' */
        p = TopSem(0).obj;
        if (p->eKind == FUNCTION_) {
            A.type = p->pRetType;
            A.func = p;
            A.err  = false;
        } else {
            if (p->eKind != UNIVERSAL_)
                Error(ERR_KIND_NOT_FUNC, Name(p->nName));
            A.type = pUniversal;
            A.err  = true;
        }
        break;

    /* ---------------- tipos (fig. 6.5) ---------------- */
    case T_INTEGER: A.type = pInteger; break;
    case T_CHAR:    A.type = pChar;    break;
    case T_BOOLEAN: A.type = pBool;    break;
    case T_STRING:  A.type = pString;  break;

    case T_IDU:
        p = TopSem(0).obj;
        if (IS_TYPE_KIND(p->eKind) || p->eKind == UNIVERSAL_) {
            A.type = p;
        } else {
            Error(ERR_TYPE_EXPECTED, Name(p->nName));
            A.type = pUniversal;
        }
        break;

    /* ---------------- literais (fig. 6.7) ---------------- */
    case TRUE_RULE:  A.type = pBool;    A.val = 1; break;
    case FALSE_RULE: A.type = pBool;    A.val = 0; break;
    case CHR_RULE:   A.type = pChar;    A.val = getCharConst(nShiftedSecondary); break;
    case STR_RULE:   A.type = pString;  A.val = nShiftedSecondary;               break;
    case NUM_RULE:   A.type = pInteger; A.val = getIntConst(nShiftedSecondary);  break;

    /* ---------------- declaracoes (fig. 6.6 e 6.8 a 6.13) ---------------- */
    case LI_IDD:
        A.list.push_back(TopSem(0).obj);
        break;

    case LI_LIST:
        A.list = TopSem(1).list;
        A.list.push_back(TopSem(0).obj);
        break;

    case DV_RULE:
        t = TopSem(0).type;
        for (object *v : TopSem(1).list) {
            v->eKind = VAR_;
            v->pType = t;
        }
        break;

    case DT_ARRAY: {                      /* 'type' IDD '=' 'array' '[' NUM ']' 'of' T */
        p = TopSem(2).obj;
        p->eKind     = ARRAY_TYPE_;
        p->nNumElems = TopSem(1).val;
        p->pElemType = TopSem(0).type;
        break;
    }

    case DT_ALIAS:
        p = TopSem(1).obj;
        p->eKind     = ALIAS_TYPE_;
        p->pBaseType = TopSem(0).type;
        break;

    case DC_LI:
    case DC_LIST:                         /* [DC ';'] LI ':' T */
        t = TopSem(0).type;
        for (object *f : TopSem(1).list) {
            f->eKind = FIELD_;
            f->pType = t;
        }
        if (rule == DC_LIST)
            A.list = TopSem(2).list;
        A.list.insert(A.list.end(), TopSem(1).list.begin(), TopSem(1).list.end());
        break;

    case DT_STRUCT:                       /* 'type' IDD '=' 'struct' NB '{' DC '}' */
        p = TopSem(2).obj;
        p->eKind   = STRUCT_TYPE_;
        p->vFields = TopSem(0).list;
        EndBlock();
        break;

    case LP_IDD:
    case LP_LIST:                         /* [LP ','] IDD ':' T */
        p = TopSem(1).obj;
        p->eKind = PARAM_;
        p->pType = TopSem(0).type;
        if (rule == LP_LIST)
            A.list = TopSem(2).list;
        A.list.push_back(p);
        break;

    case DF_RULE:
        EndBlock();
        curFunction = nullptr;
        break;

    /* ---------------- comandos (fig. 6.14 e 6.15) ---------------- */
    case S_BLOCK:
        EndBlock();
        break;

    case S_IF:        CheckCondition(TopSem(1)); break;   /* E S         */
    case S_IF_ELSE:   CheckCondition(TopSem(2)); break;   /* E S S       */
    case S_WHILE:                                         /* E ML S      */
        CheckCondition(TopSem(2));
        nLoopDepth--;
        break;
    case S_DO_WHILE:                                      /* ML S E      */
        CheckCondition(TopSem(0));
        nLoopDepth--;
        break;

    case S_ASSIGN:                                        /* LV E        */
        if (!CheckTypes(TopSem(1).type, TopSem(0).type))
            Error(ERR_TYPE_MISMATCH);
        break;

    case S_BREAK:
    case S_CONTINUE:
        if (nLoopDepth == 0)
            Error(ERR_NOT_IN_LOOP, rule == S_BREAK ? "break" : "continue");
        break;

    case S_RETURN:                                        /* extensao    */
        RulePos = TopSem(0).pos;
        if (!CheckTypes(curFunction->pRetType, TopSem(0).type))
            Error(ERR_RETURN_TYPE);
        break;

    /* ---------------- expressoes (fig. 6.18 a 6.21) ---------------- */
    case E_AND:
    case E_OR:     A.type = Logical(TopSem(1).type, TopSem(0).type);    break;

    case L_LT: case L_GT: case L_LE: case L_GE: case L_EQ: case L_NE:
                   A.type = Relational(TopSem(1).type, TopSem(0).type); break;

    case R_PLUS:   A.type = Arithmetic(TopSem(1).type, TopSem(0).type, true);  break;
    case R_MINUS:
    case Y_TIMES:
    case Y_DIVIDE: A.type = Arithmetic(TopSem(1).type, TopSem(0).type, false); break;

    case F_PRE_INC: case F_PRE_DEC: case F_POS_INC: case F_POS_DEC:
    case F_MINUS:
        A.type = IntegerOperand(TopSem(0).type);
        break;

    case F_NOT:
        if (!CheckTypes(TopSem(0).type, pBool))
            Error(ERR_INVALID_TYPE);
        A.type = pBool;
        break;

    /* regras de renomeacao: copiam o tipo */
    case E_L: case L_R: case R_Y: case Y_F: case F_LV: case F_PAREN:
    case F_TRUE: case F_FALSE: case F_CHR: case F_STR: case F_NUM:
        A.type = TopSem(0).type;
        break;

    /* ---------------- chamada de funcao (fig. 6.23) ---------------- */
    case LE_E:                                            /* MC E        */
        A.func = TopSem(1).func;
        A.err  = TopSem(1).err;
        A.n    = 0;
        CheckArgument(A, TopSem(0));
        break;

    case LE_LIST:                                         /* LE E        */
        A = TopSem(1);
        CheckArgument(A, TopSem(0));
        break;

    case F_CALL: {                                        /* IDU MC LE   */
        const t_attrib &MC = TopSem(1), &LE = TopSem(0);
        A.type = MC.type;
        if (!LE.err && LE.n < (int)LE.func->vParams.size())
            Error(ERR_TOO_FEW_ARGS);
        break;
    }

    /* ---------------- left-values (fig. 6.22) ---------------- */
    case LV_FIELD:                                        /* LV ID       */
        t = BaseType(TopSem(1).type);
        A.type = pUniversal;
        if (t->eKind != STRUCT_TYPE_) {
            if (t->eKind != UNIVERSAL_)
                Error(ERR_KIND_NOT_STRUCT);
        } else {
            p = nullptr;
            for (object *f : t->vFields)
                if (f->nName == TopSem(0).name) { p = f; break; }
            if (p == nullptr)
                Error(ERR_FIELD_NOT_DECL, Name(TopSem(0).name));
            else
                A.type = p->pType;
        }
        break;

    case LV_INDEX:                                        /* LV E        */
        t = BaseType(TopSem(1).type);
        if (t == pString) {
            A.type = pChar;
        } else if (t->eKind != ARRAY_TYPE_) {
            if (t->eKind != UNIVERSAL_)
                Error(ERR_KIND_NOT_ARRAY);
            A.type = pUniversal;
        } else {
            A.type = t->pElemType;
        }
        if (!CheckTypes(TopSem(0).type, pInteger))
            Error(ERR_INVALID_INDEX_TYPE);
        break;

    case LV_IDU:
        p = TopSem(0).obj;
        if (p->eKind != VAR_ && p->eKind != PARAM_) {
            if (p->eKind != UNIVERSAL_)
                Error(ERR_KIND_NOT_VAR, Name(p->nName));
            A.type = pUniversal;
        } else {
            A.type = p->pType;
        }
        break;

    /* regras sem atributos: P, LDE, DE, B, LDV, LS */
    default:
        break;
    }

    /* convencao do cap. 4.1: todo nao terminal empilha um no */
    PopSem(NonTerminalsOf(rule));
    A.nont = Rules[rule].left;
    A.pos  = posOfRule;
    PushSem(A);
}

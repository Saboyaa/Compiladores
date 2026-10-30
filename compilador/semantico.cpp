/* Analise de Escopo (cap. 5), de Tipos (cap. 6) e Geracao de Codigo
 * (cap. 7) por meio da funcao Semantics(), chamada pelo analisador
 * sintatico a cada reducao (fig. 4.6).
 * Cada caso do switch traz, em comentario, a figura do livro que implementa
 * e as correcoes adotadas (README, secao 7). */

#include <cstdio>
#include <deque>
#include "codigo.h"
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
object *BaseType(object *t)
{
    while (t->eKind == ALIAS_TYPE_)
        t = t->pBaseType;
    return t;
}

/* escalares (inclusive string) ocupam uma palavra; arrays e structs, a
 * soma dos elementos/campos, calculada em DT (cap. 7) */
int Size(object *t)
{
    t = BaseType(t);
    return (t->eKind == ARRAY_TYPE_ || t->eKind == STRUCT_TYPE_) ? t->nSize : 1;
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
    int     cst;                     /* NUM, CHR, STR: indice no pool */
    object *func;                    /* MC, LE: funcao chamada        */
    int     n;                       /* LE: argumentos ja verificados;
                                        NB: topo do frame ao abrir o bloco */
    bool    err;                     /* MC, LE: nao verificar args    */
    int     label;                   /* MT, ME, MA, MO                */
    bool    simple;                  /* LV -> IDU: variavel simples   */
    int     ref;                     /* LV -> IDU: indice do LOAD_REF */
    t_pos   pos;                     /* inicio do simbolo no fonte    */

    t_attrib() : nont(0), name(-1), obj(nullptr), type(nullptr), val(0),
                 cst(-1), func(nullptr), n(0), err(false), label(0),
                 simple(false), ref(-1), pos() {}
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

std::vector<object *> Functions;

/* comando de repeticao aberto: rotulos do inicio, do continue e do break.
 * No while o continue volta ao inicio; no do-while o seu rotulo e o do
 * teste. Os rotulos do break e do continue so sao criados se usados (0). */
struct t_loop {
    int  start, cont, brk;
};

static object *curFunction;     /* funcao cujo corpo esta sendo analisado */
static std::vector<t_loop> Loops;
static int     nVarTop;         /* proxima posicao livre no frame         */
static int     nVarMax;         /* maior topo do frame na funcao          */
static int     nBeginFunc;      /* indice do BEGIN_FUNC a completar em DF */

void InitSemantics(void)
{
    SymbolTable.clear();
    objects.clear();
    StackSem.clear();
    Errors.clear();
    Functions.clear();
    Loops.clear();
    ResetCode();
    curFunction = nullptr;
    nVarTop = nVarMax = nBeginFunc = 0;
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

/* operando de LV na pilha: ++LV e --LV deixam o valor novo (cap. 7) */
static void IncDec(t_op op)
{
    Emit(I_DUP);
    Emit(I_DUP);
    Emit(I_DE_REF, 1);
    Emit(op);
    Emit(I_STORE_REF, 1);
    Emit(I_DE_REF, 1);
}

static t_op RelationalOp(int rule)
{
    switch (rule) {
    case L_LT: return I_LT;
    case L_GT: return I_GT;
    case L_LE: return I_LE;
    case L_GE: return I_GE;
    case L_EQ: return I_EQ;
    default:   return I_NE;
    }
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
        A.n = nVarTop;                    /* o bloco libera o espaco em S_BLOCK */
        break;

    case NF_RULE:                         /* DF -> 'function' IDD NF ... */
        p = TopSem(0).obj;
        p->eKind  = FUNCTION_;
        p->nIndex = (int)Functions.size();
        Functions.push_back(p);
        NewBlock();                       /* parametros e locais no mesmo nivel */
        break;

    case MF_RULE: {                       /* fig. 6.13: ... LP ')' ':' T MF */
        const t_attrib &T = TopSem(0), &LP = TopSem(1), &IDD = TopSem(3);
        p = IDD.obj;
        p->pRetType = T.type;
        p->vParams  = LP.list;
        curFunction = p;                  /* funcao completa antes do corpo */

        nVarTop = 0;                      /* frame: parametros e depois locais */
        for (object *param : p->vParams) {
            param->nIndex = nVarTop;
            nVarTop += Size(param->pType);
        }
        nVarMax = nVarTop;
        nBeginFunc = Emit(I_BEGIN_FUNC, p->nIndex, nVarTop, 0);  /* locais: em DF */
        break;
    }

    /* ---------------- rotulos dos comandos (cap. 7) ---------------- */
    case ML_RULE:                         /* 'do' ML S ...               */
    case MW_RULE: {                       /* 'while' MW '(' E ')' ...    */
        t_loop l;
        l.start = NewLabel();
        l.cont  = rule == MW_RULE ? l.start : 0;
        l.brk   = 0;
        Loops.push_back(l);
        Emit(I_LABEL, l.start);
        break;
    }

    case MD_RULE:                         /* 'do' ML S 'while' MD '(' E ')' */
        if (Loops.back().cont != 0)
            Emit(I_LABEL, Loops.back().cont);
        break;

    case MT_RULE:                         /* '(' E ')' MT S: desvia se falso */
        A.label = NewLabel();
        Emit(I_FJMP_FW, A.label);
        break;

    case ME_RULE:                         /* ... MT S 'else' ME S          */
        A.label = NewLabel();
        Emit(I_JMP_FW, A.label);
        Emit(I_LABEL, TopSem(1).label);
        break;

    case MA_RULE:                         /* E '&&' MA L: curto-circuito   */
    case MO_RULE:                         /* E '||' MO L                   */
        A.label = NewLabel();
        Emit(I_DUP);
        Emit(rule == MA_RULE ? I_FJMP_FW : I_TJMP_FW, A.label);
        Emit(I_POP);
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
    /* cst: indice no pool, operando de LOAD_CONST (cap. 7) */
    case TRUE_RULE:  A.type = pBool;    A.val = 1; break;
    case FALSE_RULE: A.type = pBool;    A.val = 0; break;
    case CHR_RULE:   A.type = pChar;    A.val = getCharConst(nShiftedSecondary);
                     A.cst = nShiftedSecondary;                                  break;
    case STR_RULE:   A.type = pString;  A.val = A.cst = nShiftedSecondary;      break;
    case NUM_RULE:   A.type = pInteger; A.val = getIntConst(nShiftedSecondary);
                     A.cst = nShiftedSecondary;                                  break;

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
            v->eKind  = VAR_;
            v->pType  = t;
            v->nIndex = nVarTop;          /* posicao no frame (cap. 7) */
            nVarTop  += Size(t);
        }
        if (nVarTop > nVarMax)
            nVarMax = nVarTop;
        break;

    case DT_ARRAY: {                      /* 'type' IDD '=' 'array' '[' NUM ']' 'of' T */
        p = TopSem(2).obj;
        p->eKind     = ARRAY_TYPE_;
        p->nNumElems = TopSem(1).val;
        p->pElemType = TopSem(0).type;
        p->nSize     = p->nNumElems * Size(p->pElemType);
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
        p->nSize   = 0;                   /* deslocamento de cada campo */
        for (object *f : p->vFields) {
            f->nIndex = p->nSize;
            p->nSize += Size(f->pType);
        }
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
        Emit(I_END_FUNC);
        Code[nBeginFunc].c = nVarMax - Code[nBeginFunc].b;
        EndBlock();
        curFunction = nullptr;
        break;

    /* ---------------- comandos (fig. 6.14 e 6.15) ---------------- */
    case S_BLOCK:                                         /* NB B        */
        EndBlock();
        nVarTop = TopSem(1).n;            /* o espaco do bloco e reaproveitado */
        break;

    case S_IF:                                            /* E MT S      */
        CheckCondition(TopSem(2));
        Emit(I_LABEL, TopSem(1).label);
        break;
    case S_IF_ELSE:                                       /* E MT S ME S */
        CheckCondition(TopSem(4));
        Emit(I_LABEL, TopSem(1).label);
        break;
    case S_WHILE: {                                       /* MW E MT S   */
        const t_loop &l = Loops.back();
        CheckCondition(TopSem(2));
        Emit(I_JMP_BW, l.start);
        Emit(I_LABEL, TopSem(1).label);
        if (l.brk != 0)
            Emit(I_LABEL, l.brk);
        Loops.pop_back();
        break;
    }
    case S_DO_WHILE: {                                    /* ML S MD E   */
        const t_loop &l = Loops.back();
        CheckCondition(TopSem(0));
        Emit(I_TJMP_BW, l.start);
        if (l.brk != 0)
            Emit(I_LABEL, l.brk);
        Loops.pop_back();
        break;
    }

    case S_ASSIGN: {                                      /* LV E        */
        const t_attrib &LV = TopSem(1);
        if (!CheckTypes(LV.type, TopSem(0).type))
            Error(ERR_TYPE_MISMATCH);
        if (LV.simple && Size(LV.type) == 1) {            /* LOAD_REF i ... STORE_REF 1 */
            Code.erase(Code.begin() + LV.ref);
            Emit(I_STORE_VAR, LV.obj->nIndex);
        } else {
            Emit(I_STORE_REF, Size(LV.type));
        }
        break;
    }

    case S_BREAK:
    case S_CONTINUE:
        if (Loops.empty()) {
            Error(ERR_NOT_IN_LOOP, rule == S_BREAK ? "break" : "continue");
        } else if (rule == S_BREAK) {
            t_loop &l = Loops.back();
            if (l.brk == 0)
                l.brk = NewLabel();
            Emit(I_JMP_FW, l.brk);
        } else {
            t_loop &l = Loops.back();     /* while: volta ao inicio; do: vai ao teste */
            if (l.cont == 0)
                l.cont = NewLabel();
            Emit(l.cont == l.start ? I_JMP_BW : I_JMP_FW, l.cont);
        }
        break;

    case S_RETURN:                                        /* extensao    */
        RulePos = TopSem(0).pos;
        if (!CheckTypes(curFunction->pRetType, TopSem(0).type))
            Error(ERR_RETURN_TYPE);
        Emit(I_RETURN, Size(curFunction->pRetType));
        break;

    /* ---------------- expressoes (fig. 6.18 a 6.21) ---------------- */
    case E_AND:                                           /* E MA L      */
    case E_OR:
        A.type = Logical(TopSem(2).type, TopSem(0).type);
        Emit(I_LABEL, TopSem(1).label);
        break;

    case L_LT: case L_GT: case L_LE: case L_GE: case L_EQ: case L_NE:
        A.type = Relational(TopSem(1).type, TopSem(0).type);
        Emit(RelationalOp(rule), Size(TopSem(1).type));
        break;

    case R_PLUS:
        A.type = Arithmetic(TopSem(1).type, TopSem(0).type, true);
        Emit(BaseType(A.type) == pString ? I_CONCAT : I_ADD);
        break;
    case R_MINUS:
    case Y_TIMES:
    case Y_DIVIDE:
        A.type = Arithmetic(TopSem(1).type, TopSem(0).type, false);
        Emit(rule == R_MINUS ? I_SUB : rule == Y_TIMES ? I_MUL : I_DIV);
        break;

    case F_PRE_INC: case F_PRE_DEC: case F_POS_INC: case F_POS_DEC:
    case F_MINUS:
        A.type = IntegerOperand(TopSem(0).type);
        if (rule == F_MINUS) {
            Emit(I_NEG);
        } else {
            bool inc = rule == F_PRE_INC || rule == F_POS_INC;
            IncDec(inc ? I_INC : I_DEC);
            if (rule == F_POS_INC || rule == F_POS_DEC)   /* devolve o valor antigo */
                Emit(inc ? I_DEC : I_INC);
        }
        break;

    case F_NOT:
        if (!CheckTypes(TopSem(0).type, pBool))
            Error(ERR_INVALID_TYPE);
        A.type = pBool;
        Emit(I_NOT);
        break;

    case F_LV: {                          /* valor do left-value */
        const t_attrib &LV = TopSem(0);
        A.type = LV.type;
        if (LV.simple && Size(LV.type) == 1)
            Code.back() = { I_LOAD_VAR, LV.obj->nIndex, 0, 0, "" };  /* era LOAD_REF */
        else
            Emit(I_DE_REF, Size(LV.type));
        break;
    }

    case F_TRUE:  A.type = pBool; Emit(I_LOAD_TRUE);  break;
    case F_FALSE: A.type = pBool; Emit(I_LOAD_FALSE); break;
    case F_CHR:
    case F_NUM:
        A.type = TopSem(0).type;
        Emit(I_LOAD_CONST, TopSem(0).cst, TopSem(0).val,
             rule == F_CHR ? CONST_CHAR : CONST_INT);
        break;
    case F_STR:
        A.type = pString;
        Emit(I_LOAD_CONST, TopSem(0).cst, 0, CONST_STRING);
        Code.back().s = getStringConst(TopSem(0).cst);
        break;

    /* regras de renomeacao: copiam o tipo */
    case E_L: case L_R: case R_Y: case Y_F: case F_PAREN:
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
        if (MC.func != nullptr)           /* argumentos ja estao na pilha */
            Emit(I_CALL, MC.func->nIndex);
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
            if (p == nullptr) {
                Error(ERR_FIELD_NOT_DECL, Name(TopSem(0).name));
            } else {
                A.type = p->pType;
                if (p->nIndex != 0)
                    Emit(I_OFFSET, p->nIndex);
            }
        }
        break;

    case LV_INDEX:                                        /* LV E        */
        t = BaseType(TopSem(1).type);
        if (t == pString) {
            A.type = pChar;
            Emit(I_STR_INDEX);
        } else if (t->eKind != ARRAY_TYPE_) {
            if (t->eKind != UNIVERSAL_)
                Error(ERR_KIND_NOT_ARRAY);
            A.type = pUniversal;
        } else {
            A.type = t->pElemType;
            Emit(I_INDEX, Size(t->pElemType), t->nNumElems);   /* b: limite */
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
        A.obj    = p;
        A.simple = true;
        A.ref    = Emit(I_LOAD_REF, p->nIndex);
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

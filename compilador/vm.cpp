/* Maquina virtual de pilha (cap. 7). Ha uma pilha de operandos e uma
 * memoria onde ficam os frames: cada chamada reserva, no topo da memoria,
 * os parametros seguidos das variaveis locais. LOAD_REF i empilha o
 * endereco fp + i; os enderecos sao posicoes na memoria. */

#include <cstdlib>
#include <cstring>
#include <map>
#include "codigo.h"
#include "vm.h"

extern "C" {
#include "lexer.h"
}

t_value Num(int n)
{
    return { t_value::NUM, n, -1, "" };
}

t_value Str(const std::string &s)
{
    return { t_value::STR, 0, -1, s };
}

static t_value Addr(int a)
{
    return { t_value::ADDR, a, -1, "" };
}

[[noreturn]] static void Fail(const std::string &msg)
{
    throw std::runtime_error(msg);
}

/* string nao inicializado e NUM 0, com s vazio: compara como "" */
static int Compare(const t_value &x, const t_value &y)
{
    if (x.kind == t_value::STR || y.kind == t_value::STR)
        return x.s.compare(y.s);
    return (x.n > y.n) - (x.n < y.n);
}

struct t_frame {
    int retPc;              /* -1: volta para Execute */
    int fp;                 /* frame de quem chamou   */
};

static const long MAX_STEPS = 100000000L;

std::vector<t_value> Execute(int f, const std::vector<t_value> &args)
{
    std::map<int, int>   labels;        /* rotulo -> indice da instrucao */
    std::map<int, int>   entries;       /* funcao -> BEGIN_FUNC          */
    std::vector<t_value> stack(args), mem;
    std::vector<t_frame> frames;
    int  pc, fp = 0;
    long steps = 0;

    for (int i = 0; i < (int)Code.size(); i++) {
        if (Code[i].op == I_LABEL)
            labels[Code[i].a] = i;
        else if (Code[i].op == I_BEGIN_FUNC)
            entries[Code[i].a] = i;
    }
    if (entries.count(f) == 0)
        Fail("funcao inexistente");

    frames.push_back({ -1, 0 });
    pc = entries[f];

    auto pop = [&]() {
        if (stack.empty())
            Fail("pilha de operandos vazia");
        t_value v = stack.back();
        stack.pop_back();
        return v;
    };
    auto popWords = [&](int n) {
        if ((int)stack.size() < n)
            Fail("pilha de operandos vazia");
        std::vector<t_value> w(stack.end() - n, stack.end());
        stack.resize(stack.size() - n);
        return w;
    };
    auto memAt = [&](int a, int n) -> t_value * {
        if (a < 0 || a + n > (int)mem.size())
            Fail("acesso fora da memoria");
        return &mem[a];
    };
    auto strChar = [&](const t_value &addr) -> std::string & {
        std::string &s = memAt(addr.n, 1)->s;
        if (addr.chr < 0 || addr.chr >= (int)s.size())
            Fail("indice " + std::to_string(addr.chr) + " fora do string");
        return s;
    };

    for (;;) {
        if (++steps > MAX_STEPS)
            Fail("limite de passos excedido");
        const t_instr &in = Code[pc++];

        switch (in.op) {
        case I_LABEL:
            break;

        /* ---------------- funcoes ---------------- */
        case I_BEGIN_FUNC: {                    /* a: funcao, b: params, c: locais */
            std::vector<t_value> params = popWords(in.b);
            fp = (int)mem.size();
            mem.insert(mem.end(), params.begin(), params.end());
            mem.resize(mem.size() + in.c, Num(0));
            break;
        }
        case I_CALL:
            frames.push_back({ pc, fp });
            pc = entries[in.a];
            break;
        case I_RETURN: {                        /* a: palavras do valor */
            std::vector<t_value> ret = popWords(in.a);
            t_frame fr = frames.back();
            frames.pop_back();
            mem.resize(fp);
            fp = fr.fp;
            if (fr.retPc < 0)
                return ret;
            stack.insert(stack.end(), ret.begin(), ret.end());
            pc = fr.retPc;
            break;
        }
        case I_END_FUNC:
            Fail("fim da funcao sem return");

        /* ---------------- desvios ---------------- */
        case I_JMP_FW: case I_JMP_BW:
            pc = labels[in.a];
            break;
        case I_TJMP_FW: case I_TJMP_BW:
            if (pop().n != 0)
                pc = labels[in.a];
            break;
        case I_FJMP_FW: case I_FJMP_BW:
            if (pop().n == 0)
                pc = labels[in.a];
            break;

        /* ---------------- memoria ---------------- */
        case I_LOAD_REF:
            stack.push_back(Addr(fp + in.a));
            break;
        case I_LOAD_VAR:
            stack.push_back(*memAt(fp + in.a, 1));
            break;
        case I_STORE_VAR:
            *memAt(fp + in.a, 1) = pop();
            break;
        case I_DE_REF: {                        /* a: palavras */
            t_value addr = pop();
            if (addr.chr >= 0) {
                stack.push_back(Num((unsigned char)strChar(addr)[addr.chr]));
            } else {
                t_value *p = memAt(addr.n, in.a);
                stack.insert(stack.end(), p, p + in.a);
            }
            break;
        }
        case I_STORE_REF: {                     /* a: palavras */
            std::vector<t_value> w = popWords(in.a);
            t_value addr = pop();
            if (addr.chr >= 0)
                strChar(addr)[addr.chr] = (char)w[0].n;
            else
                std::copy(w.begin(), w.end(), memAt(addr.n, in.a));
            break;
        }
        case I_OFFSET:
            if (stack.empty())
                Fail("pilha de operandos vazia");
            stack.back().n += in.a;
            break;
        case I_INDEX: {                         /* a: tamanho do elemento, b: limite */
            int i = pop().n;
            if (i < 0 || i >= in.b)
                Fail("indice " + std::to_string(i) + " fora do array");
            if (stack.empty())
                Fail("pilha de operandos vazia");
            stack.back().n += i * in.a;
            break;
        }
        case I_STR_INDEX: {
            int i = pop().n;
            if (stack.empty())
                Fail("pilha de operandos vazia");
            stack.back().chr = i;
            break;
        }

        /* ---------------- constantes e pilha ---------------- */
        case I_LOAD_CONST:
            stack.push_back(in.c == CONST_STRING ? Str(in.s) : Num(in.b));
            break;
        case I_LOAD_TRUE:  stack.push_back(Num(1)); break;
        case I_LOAD_FALSE: stack.push_back(Num(0)); break;
        case I_DUP:
            if (stack.empty())
                Fail("pilha de operandos vazia");
            stack.push_back(stack.back());
            break;
        case I_POP:
            pop();
            break;

        /* ---------------- operadores ---------------- */
        case I_ADD: case I_SUB: case I_MUL: case I_DIV: {
            int y = pop().n, x = pop().n;
            if (in.op == I_DIV && y == 0)
                Fail("divisao por zero");
            stack.push_back(Num(in.op == I_ADD ? x + y : in.op == I_SUB ? x - y :
                                in.op == I_MUL ? x * y : x / y));
            break;
        }
        case I_CONCAT: {
            std::string y = pop().s, x = pop().s;
            stack.push_back(Str(x + y));
            break;
        }
        case I_NEG: stack.push_back(Num(-pop().n));    break;
        case I_INC: stack.push_back(Num(pop().n + 1)); break;
        case I_DEC: stack.push_back(Num(pop().n - 1)); break;
        case I_NOT: stack.push_back(Num(!pop().n));    break;

        case I_LT: case I_GT: case I_LE: case I_GE: case I_EQ: case I_NE: {
            int n = in.a > 1 ? in.a : 1;        /* structs e arrays: palavra a palavra */
            std::vector<t_value> y = popWords(n), x = popWords(n);
            int c = 0;
            for (int i = 0; i < n && c == 0; i++)
                c = Compare(x[i], y[i]);
            bool r = in.op == I_LT ? c < 0  : in.op == I_GT ? c > 0 :
                     in.op == I_LE ? c <= 0 : in.op == I_GE ? c >= 0 :
                     in.op == I_EQ ? c == 0 : c != 0;
            stack.push_back(Num(r));
            break;
        }
        }
    }
}

/* ------------------------------------------------------------------ */
/* argumentos e resultados de sslc --executar                          */
/* ------------------------------------------------------------------ */

object *FindFunction(const char *name)
{
    for (object *f : Functions) {
        const char *s = getName(f->nName);
        if (s != NULL && strcmp(s, name) == 0)
            return f;
    }
    return nullptr;
}

/* so tipos escalares podem vir da linha de comando */
bool ParseArg(object *type, const char *text, std::vector<t_value> &out)
{
    char *end;

    type = BaseType(type);
    if (type == pInteger) {
        long n = strtol(text, &end, 10);
        if (*text == '\0' || *end != '\0')
            return false;
        out.push_back(Num((int)n));
    } else if (type == pChar) {
        if (strlen(text) != 1)
            return false;
        out.push_back(Num((unsigned char)text[0]));
    } else if (type == pBool) {
        if (strcmp(text, "true") != 0 && strcmp(text, "false") != 0)
            return false;
        out.push_back(Num(strcmp(text, "true") == 0));
    } else if (type == pString) {
        out.push_back(Str(text));
    } else {
        return false;
    }
    return true;
}

static std::string Format(object *type, const std::vector<t_value> &w, size_t &pos)
{
    std::string r;

    type = BaseType(type);
    if (type->eKind == ARRAY_TYPE_) {
        r = "[";
        for (int i = 0; i < type->nNumElems; i++)
            r += (i ? ", " : "") + Format(type->pElemType, w, pos);
        return r + "]";
    }
    if (type->eKind == STRUCT_TYPE_) {
        r = "{";
        for (size_t i = 0; i < type->vFields.size(); i++)
            r += (i ? ", " : "") + std::string(getName(type->vFields[i]->nName)) +
                 ": " + Format(type->vFields[i]->pType, w, pos);
        return r + "}";
    }

    const t_value &v = w[pos++];
    if (type == pChar)
        return std::string("'") + (char)v.n + "'";
    if (type == pBool)
        return v.n ? "true" : "false";
    if (type == pString)
        return "\"" + v.s + "\"";
    return std::to_string(v.n);
}

std::string FormatValue(object *type, const std::vector<t_value> &words)
{
    size_t pos = 0;
    return Format(type, words, pos);
}

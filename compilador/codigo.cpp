/* Area de codigo e listagem das instrucoes da maquina virtual (cap. 7). */

#include "codigo.h"
#include "semantico.h"

extern "C" {
#include "lexer.h"
}

std::vector<t_instr> Code;
static int nLabels;

void ResetCode(void)
{
    Code.clear();
    nLabels = 0;
}

int Emit(t_op op, int a, int b, int c)
{
    Code.push_back({ op, a, b, c, "" });
    return (int)Code.size() - 1;
}

int NewLabel(void)
{
    return ++nLabels;
}

static const char *opNames[] = {
    "LABEL",
    "BEGIN_FUNC", "END_FUNC", "RETURN", "CALL",
    "JMP_FW", "JMP_BW", "TJMP_FW", "TJMP_BW", "FJMP_FW", "FJMP_BW",
    "LOAD_REF", "LOAD_VAR", "STORE_VAR",
    "DE_REF", "STORE_REF",
    "OFFSET", "INDEX", "STR_INDEX",
    "LOAD_CONST", "LOAD_TRUE", "LOAD_FALSE",
    "DUP", "POP",
    "ADD", "SUB", "MUL", "DIV", "NEG", "INC", "DEC", "CONCAT", "NOT",
    "LT", "GT", "LE", "GE", "EQ", "NE"
};

const char *OpName(t_op op)
{
    return opNames[op];
}

static const char *FuncName(int f)
{
    const char *s = getName(Functions[f]->nName);
    return s ? s : "?";
}

/* uma instrucao por linha; os rotulos ficam na coluna 0 */
void PrintCode(FILE *out)
{
    char arg[64];

    for (const t_instr &i : Code) {
        std::string comment;

        arg[0] = '\0';
        switch (i.op) {
        case I_LABEL:
            fprintf(out, "L%d:\n", i.a);
            continue;
        case I_BEGIN_FUNC:
            if (&i != &Code[0])
                fprintf(out, "\n");
            snprintf(arg, sizeof(arg), "%d, %d, %d", i.a, i.b, i.c);
            comment = FuncName(i.a);
            break;
        case I_CALL:
            snprintf(arg, sizeof(arg), "%d", i.a);
            comment = FuncName(i.a);
            break;
        case I_JMP_FW: case I_JMP_BW: case I_TJMP_FW: case I_TJMP_BW:
        case I_FJMP_FW: case I_FJMP_BW:
            snprintf(arg, sizeof(arg), "L%d", i.a);
            break;
        case I_RETURN: case I_LOAD_REF: case I_LOAD_VAR: case I_STORE_VAR:
        case I_DE_REF: case I_STORE_REF: case I_OFFSET: case I_INDEX:
            snprintf(arg, sizeof(arg), "%d", i.a);
            break;
        case I_LOAD_CONST:
            snprintf(arg, sizeof(arg), "%d", i.a);
            if (i.c == CONST_STRING)
                comment = "\"" + i.s + "\"";
            else if (i.c == CONST_CHAR)
                comment = std::string("'") + (char)i.b + "'";
            else
                comment = std::to_string(i.b);
            break;
        case I_LT: case I_GT: case I_LE: case I_GE: case I_EQ: case I_NE:
            if (i.a > 1)                     /* comparacao de structs/arrays */
                snprintf(arg, sizeof(arg), "%d", i.a);
            break;
        default:
            break;
        }

        std::string line = std::string("    ") + OpName(i.op);
        if (arg[0] != '\0')
            line += std::string(" ") + arg;
        if (!comment.empty()) {
            line.resize(line.size() < 28 ? 28 : line.size() + 1, ' ');
            line += "; " + comment;
        }
        fprintf(out, "%s\n", line.c_str());
    }
}

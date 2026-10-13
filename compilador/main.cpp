/* Driver do compilador da SSL (VF).
 *   sslc arquivo.ssl            compila: lexico, sintatico, escopo e tipos
 *   sslc --tokens arquivo.ssl   lista os tokens, como o analisador da VC
 *   sslc --tabela               estatisticas e conflitos da tabela la-LR(1) */

#include <cstdio>
#include <cstring>
#include "lalr.h"
#include "sintatico.h"

/* mesma saida do main.c da VC */
static int PrintTokens(FILE *f)
{
    int nErros = 0;

    printf("%-5s %-20s %-8s %s\n", "LINHA", "TOKEN", "SEC", "VALOR");
    printf("---------------------------------------------------------\n");

    for (const t_tokenInfo &t : ReadTokens(f)) {
        if (t.token == ENDOFFILE)
            break;
        printf("%-5d %-20s ", t.pos.line, tokenName(t.token));

        switch (t.token) {
        case ID:
            printf("%-8d %s\n", t.secondary, getName(t.secondary));
            break;
        case NUMERAL:
            printf("%-8d %d\n", t.secondary, getIntConst(t.secondary));
            break;
        case CHARACTER:
            printf("%-8d '%c'\n", t.secondary, getCharConst(t.secondary));
            break;
        case STRINGVAL:
            printf("%-8d \"%s\"\n", t.secondary, getStringConst(t.secondary));
            break;
        case UNKNOWN:
            printf("%-8s %s\n", "-", "<lexema invalido>");
            nErros++;
            break;
        default:
            printf("%-8s\n", "-");
            break;
        }
    }

    if (nErros > 0) {
        printf("\n%d lexema(s) invalido(s) encontrado(s).\n", nErros);
        return 1;
    }
    printf("\nAnalise lexica concluida sem erros.\n");
    return 0;
}

static int Usage(const char *prog)
{
    fprintf(stderr, "uso: %s [--tokens] <arquivo.ssl>\n"
                    "     %s --tabela\n", prog, prog);
    return 2;
}

int main(int argc, char *argv[])
{
    bool        tokens = false;
    const char *path;

    if (argc == 2 && strcmp(argv[1], "--tabela") == 0) {
        BuildTables();
        PrintTableStats();
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--tokens") == 0)
        tokens = true;
    else if (argc != 2 || argv[1][0] == '-')
        return Usage(argv[0]);
    path = argv[argc - 1];

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "erro: nao foi possivel abrir '%s'\n", path);
        return 2;
    }

    int status;
    if (tokens) {
        status = PrintTokens(f);
    } else {
        Compile(f);
        for (const t_errorMsg &e : Errors)
            printf("%s:%d:%d: %s [%s]\n", path, e.pos.line, e.pos.col,
                   e.text.c_str(), ErrorName(e.code));
        if (Errors.empty())
            printf("Compilacao concluida sem erros.\n");
        else
            printf("\n%d erro(s) encontrado(s).\n", (int)Errors.size());
        status = Errors.empty() ? 0 : 1;
    }
    fclose(f);
    return status;
}

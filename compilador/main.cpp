/* Driver do compilador da SSL.
 *   sslc arquivo.ssl                      compila e grava o codigo em arquivo.vm
 *   sslc --codigo arquivo.ssl             compila e lista o codigo na saida
 *   sslc --executar arquivo.ssl f a1 ...  compila e executa f(a1, ...) na VM
 *   sslc --tokens arquivo.ssl             lista os tokens, como o analisador da VC
 *   sslc --tabela                         estatisticas e conflitos da tabela la-LR(1) */

#include <cstdio>
#include <cstring>
#include <string>
#include "codigo.h"
#include "lalr.h"
#include "sintatico.h"
#include "vm.h"

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
    fprintf(stderr, "uso: %s [--tokens | --codigo] <arquivo.ssl>\n"
                    "     %s --executar <arquivo.ssl> <funcao> [argumentos...]\n"
                    "     %s --tabela\n", prog, prog, prog);
    return 2;
}

/* executa f(argv...) com o codigo ja gerado */
static int Run(int argc, char *argv[])
{
    object *f = FindFunction(argv[0]);
    if (f == nullptr) {
        fprintf(stderr, "erro: funcao '%s' nao declarada\n", argv[0]);
        return 2;
    }
    if (argc - 1 != (int)f->vParams.size()) {
        fprintf(stderr, "erro: '%s' tem %d parametro(s)\n", argv[0], (int)f->vParams.size());
        return 2;
    }

    std::vector<t_value> args;
    for (int i = 1; i < argc; i++)
        if (!ParseArg(f->vParams[i - 1]->pType, argv[i], args)) {
            fprintf(stderr, "erro: argumento %d invalido: '%s'\n", i, argv[i]);
            return 2;
        }

    try {
        printf("%s\n", FormatValue(f->pRetType, Execute(f->nIndex, args)).c_str());
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "erro de execucao: %s\n", e.what());
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    enum { COMPILE, TOKENS, CODE, RUN } mode = COMPILE;
    const char *path;

    if (argc == 2 && strcmp(argv[1], "--tabela") == 0) {
        BuildTables();
        PrintTableStats();
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--tokens") == 0)
        mode = TOKENS;
    else if (argc == 3 && strcmp(argv[1], "--codigo") == 0)
        mode = CODE;
    else if (argc >= 4 && strcmp(argv[1], "--executar") == 0)
        mode = RUN;
    else if (argc != 2 || argv[1][0] == '-')
        return Usage(argv[0]);
    path = mode == RUN ? argv[2] : argv[argc - 1];

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "erro: nao foi possivel abrir '%s'\n", path);
        return 2;
    }

    if (mode == TOKENS) {
        int status = PrintTokens(f);
        fclose(f);
        return status;
    }

    Compile(f);
    fclose(f);
    for (const t_errorMsg &e : Errors)
        printf("%s:%d:%d: %s [%s]\n", path, e.pos.line, e.pos.col,
               e.text.c_str(), ErrorName(e.code));
    if (!Errors.empty()) {
        printf("\n%d erro(s) encontrado(s).\n", (int)Errors.size());
        return 1;
    }

    if (mode == CODE) {
        PrintCode(stdout);
        return 0;
    }
    if (mode == RUN)
        return Run(argc - 3, argv + 3);

    /* arquivo.ssl -> arquivo.vm */
    std::string out = path;
    size_t dot = out.rfind('.');
    if (dot != std::string::npos && out.find('/', dot) == std::string::npos)
        out.resize(dot);
    out += ".vm";

    FILE *vm = fopen(out.c_str(), "w");
    if (vm == NULL) {
        fprintf(stderr, "erro: nao foi possivel criar '%s'\n", out.c_str());
        return 2;
    }
    PrintCode(vm);
    fclose(vm);
    printf("Compilacao concluida sem erros: codigo em %s\n", out.c_str());
    return 0;
}

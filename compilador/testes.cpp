/* Testes: tabela la-LR(1), programas validos e um caso para cada erro
 * lexico, sintatico, de escopo e de tipo. Cada caso confere a lista exata de
 * erros, o que tambem verifica a ausencia de erros em cascata. A geracao de
 * codigo e conferida pela listagem de um trecho e executando os programas
 * na maquina virtual. */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "codigo.h"
#include "lalr.h"
#include "sintatico.h"
#include "vm.h"

static int nTests = 0;

static std::vector<t_error> CompileText(const std::string &src)
{
    FILE *f = tmpfile();
    assert(f != NULL);
    fputs(src.c_str(), f);
    rewind(f);
    Compile(f);
    fclose(f);

    std::vector<t_error> codes;
    for (const t_errorMsg &e : Errors)
        codes.push_back(e.code);
    return codes;
}

static void Expect(const char *name, const std::string &src,
                   const std::vector<t_error> &expected)
{
    std::vector<t_error> got = CompileText(src);

    nTests++;
    if (got != expected) {
        printf("FALHOU: %s\n  esperado:", name);
        for (t_error e : expected) printf(" %s", ErrorName(e));
        printf("\n  obtido:  ");
        for (const t_errorMsg &e : Errors)
            printf(" %s(%d:%d %s)", ErrorName(e.code), e.pos.line, e.pos.col,
                   e.text.c_str());
        printf("\n");
        fflush(stdout);
    }
    assert(got == expected);
}

/* declaracoes comuns e um corpo de funcao onde os comandos sao inseridos */
static const std::string Prelude =
    "type Ponto   = struct { x, y: integer }\n"
    "type Vetor   = array [5] of integer\n"
    "type Inteiro = integer\n"
    "type Nome    = string\n"
    "function g(a: integer, b: char): boolean\n"
    "{ var r: boolean; r = a > 0; return r; }\n";

static std::string InBody(const std::string &stmts)
{
    return Prelude +
        "function f(n: integer): integer\n"
        "{\n"
        "    var i: integer; var b: boolean; var c: char; var s: string;\n"
        "    var p: Ponto; var v: Vetor; var k: Inteiro; var nm: Nome;\n"
        "    " + stmts + "\n"
        "    return n;\n"
        "}\n";
}

static const std::vector<t_error> OK;

/* ------------------------------------------------------------------ */

static void TestTable(void)
{
    BuildTables();
    assert(ShiftReduce.size() == 1);                       /* else pendente */
    assert(ShiftReduce[0].symbol == ELSE);
    assert(IS_SHIFT(ShiftReduce[0].kept));
    assert(RULE(ShiftReduce[0].discarded) == S_IF);
    assert(ReduceReduce.empty());
    assert(Action[0][P_] > 0 && FINAL == Action[Action[0][P_]][ENDOFFILE]);
    assert(strcmp(SymbolName(UNKNOWN), "?") == 0);         /* nomes alinhados */
    assert(strcmp(SymbolName(START_), "S'") == 0);
    nTests++;
}

static void TestLexer(void)
{
    FILE *f = tmpfile();
    fputs("return x;\n  a /b", f);
    rewind(f);
    std::vector<t_tokenInfo> t = ReadTokens(f);
    fclose(f);

    assert(t.size() == 7);
    assert(t[0].token == RETURN && t[0].pos.line == 1 && t[0].pos.col == 1);
    assert(t[1].token == ID     && t[1].pos.col == 8);
    assert(t[3].token == ID     && t[3].pos.line == 2 && t[3].pos.col == 3);
    assert(t[4].token == DIVIDE && t[4].pos.line == 2 && t[4].pos.col == 5);
    assert(t[6].token == ENDOFFILE);
    nTests++;
}

static void TestValidFiles(void)
{
    const char *files[] = { "exemplos/validos/fatorial.ssl",
                            "exemplos/validos/strings.ssl",
                            "exemplos/validos/bsort.ssl",
                            "exemplos/validos/tipos.ssl" };

    for (const char *path : files) {
        FILE *f = fopen(path, "r");
        assert(f != NULL);
        bool ok = Compile(f);
        fclose(f);
        if (!ok)
            printf("FALHOU: %s: %s\n", path, Errors[0].text.c_str()), fflush(stdout);
        assert(ok);
        nTests++;
    }
}

static void TestValid(void)
{
    Expect("prelude", InBody(""), OK);
    Expect("alias compativel com o tipo base", InBody("k = n; n = k + 1; k = k++;"), OK);
    Expect("alias de string indexado", InBody("nm = s; c = nm[0]; s = nm + \"x\";"), OK);
    Expect("campo, indice e string", InBody("p.x = v[2]; c = s[p.y]; s = s + s;"), OK);
    Expect("relacionais e logicos", InBody("b = (n < 1 || c == 'a') && !b != false;"), OK);
    Expect("incrementos", InBody("i = ++i + i-- - --v[1] * p.x++;"), OK);
    Expect("chamada", InBody("b = g(n, c) && g(-1, s[0]);"), OK);
    Expect("else pendente", InBody("if (b) if (!b) i = 1; else i = 2; else i = 3;"), OK);
    Expect("break e continue em lacos",
           InBody("while (b) { var t: integer; if (b) break; continue; }"
                  " do if (b) continue; while (b);"), OK);
    Expect("sombreamento em bloco",
           InBody("{ var n: boolean; n = true; } { var n: char; n = 'x'; }"), OK);
    Expect("recursao",
           "function f(n: integer): integer\n"
           "{ var r: integer; r = 1; if (n > 1) r = n * f(n - 1); return r; }\n",
           OK);
    Expect("parametro com o nome da funcao",
           "function f(f: integer): integer { var x: integer; x = f; return x; }", OK);
    Expect("tipos de retorno estruturados",
           Prelude + "function h(a: Ponto): Ponto { var q: Ponto; q = a; return q; }\n"
                     "function w(a: Vetor): integer { var q: Ponto; q = h(q); return a[0]; }",
           OK);
}

/* ------------------------------------------------------------------ */

static void TestLexicalAndSyntax(void)
{
    Expect("lexico: & isolado", InBody("i = n & 1;"), { ERR_LEXICAL });
    Expect("lexico impede o sintatico", InBody("i = n @ 1 return;"), { ERR_LEXICAL });
    Expect("sintatico: falta ';'", InBody("i = n i = 1;"), { ERR_SYNTAX });
    Expect("sintatico: bloco sem declaracao", InBody("{ i = 1; }"), { ERR_SYNTAX });
    Expect("sintatico: funcao sem parametros",
           "function f(): integer { var i: integer; i = 0; return i; }", { ERR_SYNTAX });
    Expect("sintatico: ';' apos declaracao de tipo",
           "type T = integer;", { ERR_SYNTAX });
    Expect("sintatico: variavel global",
           "var x: integer;", { ERR_SYNTAX });
    Expect("sintatico: arquivo vazio", "", { ERR_SYNTAX });

    /* posicao e tokens esperados na mensagem */
    CompileText("function f(n: integer): integer\n{ var i: integer\n i = 1; return n; }");
    assert(Errors.size() == 1 && Errors[0].pos.line == 3 && Errors[0].pos.col == 2);
    assert(Errors[0].text.find("encontrado Id 'i'") != std::string::npos);
    assert(Errors[0].text.substr(Errors[0].text.find("esperado:")) == "esperado: ';'");
}

static void TestScope(void)
{
    Expect("redeclaracao de variavel", InBody("{ var a, a: integer; a = 1; }"),
           { ERR_REDECL });
    Expect("local com o nome de parametro: vale a ultima, o parametro nao muda",
           "function f(n: integer): integer { var n: char; n = 'a'; return 1; }\n"
           "function h(x: integer): integer { var y: integer; y = f(x); return y; }",
           { ERR_REDECL });
    Expect("parametros repetidos",
           "function f(n: integer, n: char): integer { var x: integer; x = 1; return x; }",
           { ERR_REDECL });
    Expect("funcao redeclarada",
           Prelude + "function g(n: integer): integer { var x: integer; x = 1; return x; }",
           { ERR_REDECL });
    Expect("campo repetido",
           "type T = struct { a: integer; a: char }", { ERR_REDECL });
    Expect("nao declarado: um erro so", InBody("i = z; z = 2; i = z + 1;"),
           { ERR_NOT_DECL });
    Expect("tipo nao declarado",
           "function f(n: X): integer { var y: integer; y = n + 1; return y; }",
           { ERR_NOT_DECL });
    Expect("funcao nao declarada", InBody("i = h(1, 2) + 1;"), { ERR_NOT_DECL });
    Expect("uso antes da declaracao",
           "function a(n: integer): integer { var x: integer; x = b(n); return x; }\n"
           "function b(n: integer): integer { var x: integer; x = n; return x; }",
           { ERR_NOT_DECL });
    Expect("variavel de bloco fora do bloco",
           InBody("{ var t: integer; t = 1; } i = t;"), { ERR_NOT_DECL });
    Expect("campo nao e visivel como variavel", InBody("i = x;"), { ERR_NOT_DECL });
    Expect("struct com campo de mesmo nome do tipo",
           "type a = struct { a: integer }", OK);
}

static void TestTypes(void)
{
    Expect("ERR_TYPE_EXPECTED", InBody("{ var t: g; t = 1; t = t + 1; }"),
           { ERR_TYPE_EXPECTED });
    Expect("ERR_TYPE_EXPECTED: variavel como tipo",
           InBody("{ var t: i; t = 1; }"), { ERR_TYPE_EXPECTED });
    Expect("tipo recursivo", "type T = struct { prox: T }", { ERR_TYPE_EXPECTED });

    Expect("ERR_BOOL_TYPE_EXPECTED: if", InBody("if (n) i = 1;"), { ERR_BOOL_TYPE_EXPECTED });
    Expect("ERR_BOOL_TYPE_EXPECTED: if-else", InBody("if (s) i = 1; else i = 2;"),
           { ERR_BOOL_TYPE_EXPECTED });
    Expect("ERR_BOOL_TYPE_EXPECTED: while", InBody("while (c) i = 1;"),
           { ERR_BOOL_TYPE_EXPECTED });
    Expect("ERR_BOOL_TYPE_EXPECTED: do-while", InBody("do i = 1; while (p);"),
           { ERR_BOOL_TYPE_EXPECTED });
    Expect("ERR_BOOL_TYPE_EXPECTED: &&", InBody("b = 1 && b || n;"),
           { ERR_BOOL_TYPE_EXPECTED, ERR_BOOL_TYPE_EXPECTED });

    Expect("ERR_TYPE_MISMATCH: atribuicao", InBody("i = c;"), { ERR_TYPE_MISMATCH });
    Expect("ERR_TYPE_MISMATCH: relacional", InBody("b = n < 'a';"), { ERR_TYPE_MISMATCH });
    Expect("ERR_TYPE_MISMATCH: aritmetica", InBody("i = n + s;"),
           { ERR_TYPE_MISMATCH });
    Expect("ERR_TYPE_MISMATCH: structs de mesma forma",
           "type A = struct { x: integer }\ntype B = struct { x: integer }\n"
           "function f(a: A, b: B): integer { var r: integer; a = b; r = 0; return r; }",
           { ERR_TYPE_MISMATCH });
    Expect("ERR_TYPE_MISMATCH: arrays de mesma forma",
           "type A = array [3] of integer\ntype B = array [3] of integer\n"
           "function f(a: A, b: B): integer { var r: integer; a = b; r = 0; return r; }",
           { ERR_TYPE_MISMATCH });

    Expect("ERR_INVALID_TYPE: + em boolean", InBody("b = b + b;"), { ERR_INVALID_TYPE });
    Expect("ERR_INVALID_TYPE: - em string", InBody("s = s - s;"), { ERR_INVALID_TYPE });
    Expect("ERR_INVALID_TYPE: * e / em char", InBody("c = c * c; c = c / c;"),
           { ERR_INVALID_TYPE, ERR_INVALID_TYPE });
    Expect("ERR_INVALID_TYPE: ++ e --", InBody("i = ++b + c-- + --s + p++;"),
           { ERR_INVALID_TYPE, ERR_INVALID_TYPE, ERR_INVALID_TYPE, ERR_INVALID_TYPE });
    Expect("ERR_INVALID_TYPE: unarios", InBody("i = -b; b = !n;"),
           { ERR_INVALID_TYPE, ERR_INVALID_TYPE });

    Expect("ERR_KIND_NOT_STRUCT", InBody("i = v.x;"), { ERR_KIND_NOT_STRUCT });
    Expect("ERR_FIELD_NOT_DECL", InBody("i = p.z;"), { ERR_FIELD_NOT_DECL });
    Expect("ERR_KIND_NOT_ARRAY", InBody("i = p[0];"), { ERR_KIND_NOT_ARRAY });
    Expect("ERR_INVALID_INDEX_TYPE", InBody("i = v[c]; c = s[b];"),
           { ERR_INVALID_INDEX_TYPE, ERR_INVALID_INDEX_TYPE });
    Expect("ERR_KIND_NOT_VAR: funcao", InBody("f = 1;"), { ERR_KIND_NOT_VAR });
    Expect("ERR_KIND_NOT_VAR: tipo", InBody("i = Ponto;"), { ERR_KIND_NOT_VAR });
    Expect("ERR_KIND_NOT_FUNC", InBody("i = n(1);"), { ERR_KIND_NOT_FUNC });
    Expect("ERR_TOO_MANY_ARGS", InBody("b = g(1, 'a', 2, 3);"), { ERR_TOO_MANY_ARGS });
    Expect("ERR_PARAM_TYPE", InBody("b = g('a', 1);"), { ERR_PARAM_TYPE, ERR_PARAM_TYPE });
    Expect("ERR_TOO_FEW_ARGS", InBody("b = g(1);"), { ERR_TOO_FEW_ARGS });
    Expect("ERR_RETURN_TYPE", InBody("return b;"), { ERR_RETURN_TYPE });
    Expect("ERR_NOT_IN_LOOP",
           InBody("break; while (b) i = 1; continue; if (b) break;"),
           { ERR_NOT_IN_LOOP, ERR_NOT_IN_LOOP, ERR_NOT_IN_LOOP });

    /* tipo universal: um erro na origem e nenhum erro derivado */
    Expect("universal: variavel nao declarada",
           InBody("i = u.x + u[1] + u; u = s;"), { ERR_NOT_DECL });
    Expect("universal: resultado de chamada a nao funcao",
           InBody("i = n(1) + 1; s = n(2);"), { ERR_KIND_NOT_FUNC, ERR_KIND_NOT_FUNC });
    Expect("universal: campo inexistente",
           InBody("i = p.z.w[3]; s = p.z;"), { ERR_FIELD_NOT_DECL, ERR_FIELD_NOT_DECL });

    /* erros sao acumulados e aparecem na ordem do fonte */
    CompileText(InBody("i = c;\n b = 1;\n i = z;"));
    assert(Errors.size() == 3);
    assert(Errors[0].pos.line < Errors[1].pos.line && Errors[1].pos.line < Errors[2].pos.line);
    nTests++;
}

/* ------------------------------------------------------------------ */
/* geracao de codigo (cap. 7)                                          */
/* ------------------------------------------------------------------ */

static void CompileOk(const std::string &src)
{
    std::vector<t_error> got = CompileText(src);
    if (!got.empty())
        printf("FALHOU: %s\n", Errors[0].text.c_str()), fflush(stdout);
    assert(got.empty());
}

static void CompileFileOk(const char *path)
{
    FILE *f = fopen(path, "r");
    assert(f != NULL);
    bool ok = Compile(f);
    fclose(f);
    assert(ok);
}

/* executa a funcao e formata o resultado como o sslc --executar */
static std::string Call(const char *func, const std::vector<t_value> &args)
{
    object *f = FindFunction(func);
    assert(f != nullptr);
    return FormatValue(f->pRetType, Execute(f->nIndex, args));
}

static void ExpectRun(const char *func, const std::vector<t_value> &args,
                      const std::string &expected)
{
    std::string got = Call(func, args);

    nTests++;
    if (got != expected)
        printf("FALHOU: %s: esperado %s, obtido %s\n", func, expected.c_str(),
               got.c_str()), fflush(stdout);
    assert(got == expected);
}

static void ExpectRuntimeError(const char *func, const std::vector<t_value> &args,
                               const char *msg)
{
    bool thrown = false;
    try {
        Call(func, args);
    } catch (const std::runtime_error &e) {
        thrown = std::string(e.what()).find(msg) != std::string::npos;
    }
    nTests++;
    assert(thrown);
}

static std::string Listing(void)
{
    FILE *f = tmpfile();
    PrintCode(f);
    rewind(f);
    std::string text;
    int c;
    while ((c = fgetc(f)) != EOF)
        text += (char)c;
    fclose(f);
    return text;
}

static void TestCodeListing(void)
{
    CompileOk("function f(n: integer): integer\n"
              "{ var i: integer; i = 0; while (i < n && true) i = i + 1; return i; }");
    std::string expected =
        "    BEGIN_FUNC 0, 1, 1      ; f\n"
        "    LOAD_CONST 0            ; 0\n"
        "    STORE_VAR 1\n"
        "L1:\n"
        "    LOAD_VAR 1\n"
        "    LOAD_VAR 0\n"
        "    LT\n"
        "    DUP\n"
        "    FJMP_FW L2\n"
        "    POP\n"
        "    LOAD_TRUE\n"
        "L2:\n"
        "    FJMP_FW L3\n"
        "    LOAD_VAR 1\n"
        "    LOAD_CONST 1            ; 1\n"
        "    ADD\n"
        "    STORE_VAR 1\n"
        "    JMP_BW L1\n"
        "L3:\n"
        "    LOAD_VAR 1\n"
        "    RETURN 1\n"
        "    END_FUNC\n";
    std::string got = Listing();
    if (got != expected)
        printf("FALHOU: listagem\n%s", got.c_str()), fflush(stdout);
    assert(got == expected);
    nTests++;

    /* frame: parametros, locais e reaproveitamento do espaco dos blocos */
    CompileOk("type P = struct { x, y: integer }\n"
              "function f(a: P, n: integer): integer\n"
              "{ var i: integer; { var q: P; q = a; } { var j: integer; j = 1; }"
              " i = a.y; return i; }");
    assert(Code[0].op == I_BEGIN_FUNC && Code[0].b == 3 && Code[0].c == 3);
    nTests++;
}

static void TestExamples(void)
{
    CompileFileOk("exemplos/validos/fatorial.ssl");
    ExpectRun("fatorial", { Num(0) }, "1");
    ExpectRun("fatorial", { Num(5) }, "120");
    ExpectRun("somaFat", { Num(4) }, "33");

    CompileFileOk("exemplos/validos/strings.ssl");
    ExpectRun("strcmp", { Str("abc"), Str("abd"), Num(3) }, "-1");
    ExpectRun("strcmp", { Str("abc"), Str("abc"), Num(3) }, "0");   /* && em curto-circuito */
    ExpectRun("strcmp", { Str("b"), Str("a"), Num(1) }, "1");
    ExpectRun("strinv", { Str("abcde"), Num(5) }, "\"edcba\"");

    CompileFileOk("exemplos/validos/tipos.ssl");
    ExpectRun("distancia", { Num(1), Num(5), Str("a"), Num(4), Num(1), Str("b") }, "7");
    ExpectRun("mdc", { Num(84), Num(36) }, "12");
    ExpectRun("rotulo", { Num(1), Num(1), Str("x"), Str("P") }, "\"Px!\"");
    ExpectRun("rotulo", { Num(1), Num(2), Str("x"), Str("Q") }, "\"Qx\"");
    ExpectRun("soma", { Num(10) }, "25");                  /* continue no do-while */
    ExpectRun("soma", { Num(300) }, "2500");               /* break no do-while    */
    ExpectRun("sombra", { Num(5) }, "true");
    ExpectRun("sombra", { Num(-5) }, "false");
}

static void TestExecution(void)
{
    /* bSort do exemplo 7.4.4 com uma funcao que monta o array */
    CompileOk("type intArray = array [5] of integer\n"
              "function bSort(a: intArray, n: integer): intArray\n"
              "{ var i: integer; i = -1;\n"
              "  while (++i < n) { var j: integer; j = i;\n"
              "    while (++j < n) if (a[i] > a[j])\n"
              "      { var t: integer; t = a[i]; a[i] = a[j]; a[j] = t; } }\n"
              "  return a; }\n"
              "function teste(n: integer): intArray\n"
              "{ var v, w: intArray; var i: integer; i = 0;\n"
              "  while (i < 5) { var k: integer; k = i; v[i++] = (k * 7 + 3) / 5 - k * k; }\n"
              "  w = bSort(v, n); return w; }\n"
              "function igual(n: integer): boolean\n"
              "{ var v, w: intArray; var i: integer; i = 0;\n"
              "  while (i < 5) { var k: integer; k = i++; v[k] = k; w[k] = k; }\n"
              "  if (n > 0) w[n - 1] = 99;\n"
              "  return v == w; }\n");
    ExpectRun("teste", { Num(5) }, "[-10, -5, -1, 0, 1]");
    ExpectRun("teste", { Num(0) }, "[0, 1, -1, -5, -10]");
    ExpectRun("igual", { Num(0) }, "true");                /* EQ 5: arrays inteiros */
    ExpectRun("igual", { Num(3) }, "false");
    ExpectRuntimeError("igual", { Num(6) }, "fora do array");

    /* structs como valor: copia, parametro e retorno */
    CompileOk("type P = struct { x, y: integer; nome: string }\n"
              "type Ps = array [2] of P\n"
              "function mover(p: P, d: integer): P\n"
              "{ var q: P; q = p; q.x = q.x + d; q.nome = q.nome + \"'\"; return q; }\n"
              "function f(d: integer): Ps\n"
              "{ var v: Ps; v[0].x = 1; v[0].y = 2; v[0].nome = \"a\";\n"
              "  v[1] = mover(v[0], d); v[0].y = v[0].y * 10; return v; }\n");
    ExpectRun("f", { Num(5) }, "[{x: 1, y: 20, nome: \"a\"}, {x: 6, y: 2, nome: \"a'\"}]");

    /* incrementos, operadores, else pendente e curto-circuito */
    CompileOk("function inc(n: integer): integer\n"
              "{ var i, a, b, c, d: integer; i = n;\n"
              "  a = i++; b = ++i; c = i--; d = --i;\n"
              "  return a * 1000 + b * 100 + c * 10 + d; }\n"
              "function ops(n: integer): integer\n"
              "{ var r: integer; r = -n * 3 - 8 / 2 + (n - 1) * (n + 1); return r; }\n"
              "function cmp(c: char): integer\n"
              "{ var r: integer; r = 0;\n"
              "  if (c >= 'a') if (c <= 'z') r = 1; else r = 2; else r = 3;\n"
              "  return r; }\n"
              "function div(n: integer): boolean\n"
              "{ var b: boolean; b = n != 0 && 10 / n > 1 || n == 0; return !!b; }\n"
              "function laco(n: integer): integer\n"
              "{ var i, s: integer; i = 0; s = 0;\n"
              "  while (true) { var t: integer; i = i + 1; if (i > n) break;\n"
              "    t = i / 2 * 2; if (t == i) continue; s = s + i; }\n"
              "  return s; }\n"
              "function semRetorno(n: integer): integer\n"
              "{ var i: integer; if (n > 0) return n; i = 0; }\n"
              "function zero(n: integer): integer\n"
              "{ var i: integer; i = n / 0; return i; }\n"
              "function str(n: integer): string\n"
              "{ var s: string; s = \"abc\"; s[n] = 'z'; return s; }\n");
    ExpectRun("inc", { Num(1) }, "1331");
    ExpectRun("ops", { Num(3) }, "-5");
    ExpectRun("cmp", { Num('m') }, "1");
    ExpectRun("cmp", { Num('~') }, "2");
    ExpectRun("cmp", { Num('A') }, "3");
    ExpectRun("div", { Num(0) }, "true");                  /* sem divisao por zero */
    ExpectRun("div", { Num(20) }, "false");
    ExpectRun("div", { Num(2) }, "true");
    ExpectRun("laco", { Num(10) }, "25");                  /* break e continue no while */
    ExpectRun("semRetorno", { Num(4) }, "4");
    ExpectRun("str", { Num(1) }, "\"azc\"");
    ExpectRuntimeError("semRetorno", { Num(0) }, "sem return");
    ExpectRuntimeError("zero", { Num(1) }, "divisao por zero");
    ExpectRuntimeError("str", { Num(3) }, "fora do string");
}

int main(void)
{
    TestTable();
    TestLexer();
    TestValidFiles();
    TestValid();
    TestLexicalAndSyntax();
    TestScope();
    TestTypes();
    TestCodeListing();
    TestExamples();
    TestExecution();
    printf("testes: %d ok\n", nTests);
    return 0;
}

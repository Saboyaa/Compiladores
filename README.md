# Compiladores — Simple Script Language (SSL)

Trabalho da disciplina de Compiladores (Prof. Luiz Guedes – IME).

| Pasta | Entrega | Conteúdo |
|---|---|---|
| [`analisador/`](analisador/README.md) | VC | analisador léxico em C99 (inalterado, exceto um flag no `Makefile`) |
| [`compilador/`](compilador/README.md) | VF | análise sintática la-LR(1), de escopo e de tipos, geração de código para máquina virtual de pilha e um interpretador dela, reaproveitando o léxico da VC |

```
cd analisador && make test
cd compilador && make test
```

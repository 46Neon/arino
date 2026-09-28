# Ariño — frontend AST e IL en binarios nativos

Prototipo de infraestructura para Ariño, un lenguaje cuya sintaxis visible se mantiene en español. El repositorio distribuye artefactos binarios nativos; no conserva fuentes C ni ensamblador de las implementaciones nativas.

## Lexer

- `libarino_lexer.so`: biblioteca Linux x86-64 / System V ABI.
- `arino_lexer_tests`: ejecutable de pruebas nativo que carga la biblioteca desde su propio directorio.
- El lexer reconoce palabras de acción, entidades de IA, operadores lógicos, símbolos y números decimales con signo opcional; preserva los lexemas como vistas zero-copy al búfer original.

## Frontend AST (subconjunto v1)

`arino_ast_compiler` consume el lexer y compila expresiones aritméticas I64/F64 y asignaciones separadas por punto y coma. Reconoce las formas visibles en español —por ejemplo, `sumar 2 y 3`, `2 más 3` y `dividir 8 con 2`—, conserva spans del fuente, valida tipos y variables, pliega expresiones constantes seguras y emite Ariño IL v2. Incluye un volcado AST y un modo de análisis sin emisión. La [especificación AST v1](docs/ARINO_AST_V1.md) documenta nodos, alias, gramática admitida y límites.

La IL es interna: no sustituye la sintaxis pública en español. `arino_vm` es una VM nativa Linux x86-64. El compilador emite bytecode `.arino` con firma de cinco bytes `ARINO` y formato IL v2; ese bytecode no es código máquina de la CPU. La VM mantiene lectura de archivos IL v1 con firma `ARIL` y acepta también la ruta heredada `.aril`. Consulta el [contrato IL v2](docs/ARINO_IL_V2.md).

Uso desde la raíz del repositorio:

```sh
chmod +x arino_ast_compiler arino_vm
printf '%s' 'sumar 2 y 3' > suma.ari
./arino_ast_compiler --ast suma.ari
./arino_ast_compiler --check suma.ari
./arino_ast_compiler --compile suma.ari suma.arino
./arino_vm --verify suma.arino
./arino_vm --run suma.arino
```

## VM IL v2

La VM valida encabezado y bytecode (opcodes, operandos, índices y destinos de salto), y detecta overflow, división por cero, tipos incompatibles, underflow y agotamiento del límite de instrucciones. Las fixtures `.arino` prueban IL v2; una fixture v1 se conserva para comprobar compatibilidad. CI confirma también que las rutas `.aril` antiguas siguen siendo legibles.

CI valida ELF x86-64, ejecuta pruebas del lexer, comprueba la emisión y ejecución de IL v2, la lectura de v1 y los errores semánticos del subconjunto AST; también publica binarios y fixtures como artefacto descargable.

## Límites

Este frontend aún no analiza toda la gramática de Ariño. No admite bloques, scopes anidados, condiciones, bucles, funciones/llamadas ni sintaxis específica de tensores o IA; tampoco aplica promociones implícitas entre I64 y F64. El hecho de que la VM IL tenga instrucciones de control de flujo no implica que este frontend genere esas construcciones. Por tanto, el AST y el compilador son funcionales para el subconjunto documentado, no un compilador integral de Ariño. Los binarios publicados son específicos de Linux x86-64 y no son portables a otras arquitecturas.

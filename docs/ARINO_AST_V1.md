# Ariño AST v1 — nodos, semántica y emisión de IL

## Estado

El repositorio incorpora `arino_ast_compiler`, un ejecutable nativo Linux x86-64 que usa la biblioteca binaria existente `libarino_lexer.so`. Implementa un AST real en memoria, normalización de operadores en español, análisis de tipos/variables, plegado de constantes y emisión de archivos `.arino` ejecutables por `arino_il_vm`. `.aril` queda como extensión heredada compatible; la cabecera binaria sigue identificando el formato ARIL v1.

La sintaxis pública sigue en español. Esta versión implementa el subconjunto de expresiones aritméticas y asignaciones descrito aquí; no es todavía un parser de todas las construcciones que Ariño podría incorporar.

## 1. Nodos y almacenamiento

Cada nodo vive en una arena contigua y tiene un opcode de 16 bits, tipo de valor, span del fuente y puntero a un arreglo de punteros hijos. Los spans son offsets en bytes UTF-8 del fuente original; los lexemas y nombres se conservan como vistas zero-copy al búfer para los mensajes de diagnóstico y el volcado del AST.

| Offset x86-64 | Campo | Ancho |
|---:|---|---:|
| 0 | `opcode` binario | u16 |
| 2 | `value_type` (0 VOID, 1 I64, 2 F64) | u8 |
| 3 | `flags` (bit 0: plegado) | u8 |
| 4 | `source_begin` | u32 |
| 8 | `source_end` | u32 |
| 12 | `child_count` | u16 |
| 14 | `local_slot` (0xFFFF si no aplica) | u16 |
| 16 | `payload` (bits de literal o dato semántico) | u64 |
| 24 | `children` (dirección de arreglo de `Node *`) | 8 bytes |
| 32 | `name` (dirección del lexema fuente) | 8 bytes |
| 40 | `name_length` | u32 |
| 44 | `scope_id` | u32 |

Tamaño del nodo: 48 bytes. El ejecutable verifica tamaño y offsets del ABI antes de analizar. Los arreglos de hijos también salen de una arena contigua; no se hace una reserva individual por nodo. Las arenas preasignadas admiten hasta 16 384 nodos, 32 768 enlaces a hijos, 8 192 tokens, 2 048 variables y 65 536 bytes de fuente. Si se excede un límite, el compilador falla con diagnóstico, sin escribir un `.arino` parcial.

Los opcodes del AST son identificadores binarios propios del frontend, distintos de los opcodes de la VM:

| Opcode AST | Nodo |
|---:|---|
| 0001 | PROGRAM |
| 0002 | I64 |
| 0003 | F64 |
| 0004 | VARIABLE |
| 0010 | ADD |
| 0011 | SUB |
| 0012 | MUL |
| 0013 | DIV |
| 0020 | ASSIGN |

## 2. Tokens y gramática del subconjunto

El frontend llama al lexer real y consume sus categorías y spans. Los literales enteros se convierten a I64 con comprobación de rango; los decimales con punto usan los bits IEEE-754 binary64 que ya produjo el lexer. Las demás palabras se consideran identificadores salvo que sean alias de operador.

Los siguientes lexemas, en minúsculas, se normalizan al mismo opcode funcional:

| Nodo AST | Alias aceptados |
|---|---|
| ADD | `sumar`, `añadir`, `adicionar`, `más`, `mas` |
| SUB | `restar`, `quitar`, `sustraer`, `menos` |
| MUL | `multiplicar`, `por`, `veces` |
| DIV | `dividir`, `entre`, `partido` |

Gramática v1 del compilador:

```text
programa       := sentencia ( ";" sentencia )* ";"?
sentencia      := identificador "=" expresión | expresión
expresión      := primaria (operador primaria)*
primaria       := entero | decimal | identificador | "(" expresión ")"
                 | alias expresión ("y" | "con") expresión
```

Los operadores infijos ADD/SUB tienen precedencia menor que MUL/DIV y todos son asociativos a izquierda. La forma prefija permite escribir, por ejemplo, `sumar 2 y 3` o `dividir 8 con 2`. El punto y coma separa sentencias. No se modifica ni se traduce la sintaxis visible: el AST es una estructura interna.

## 3. Tabla de símbolos, tipos y scope

La pasada semántica recorre el árbol antes de emitir código. La tabla de símbolos contiene nombre como span del fuente, slot local, tipo y ámbito. El ámbito v1 es el módulo actual; la estructura de scope conserva un enlace `parent` para poder agregar scopes anidados cuando la gramática incorpore bloques.

La primera asignación declara implícitamente una variable y fija su tipo; lecturas antes de esa asignación se rechazan. Las reasignaciones deben conservar el tipo. I64 y F64 son tipos distintos: no se promocionan automáticamente. ADD/SUB/MUL/DIV requieren dos operandos numéricos del mismo tipo. Cada símbolo recibe un slot local u16 en orden de primera asignación. Así, `x = 7; x más 8` verifica `x`, la carga por slot y el tipo I64 antes de emitir `LOAD`/`STORE`.

## 4. Plegado de constantes

Tras la validación semántica, el optimizador visita el árbol en postorden. Si los dos hijos de ADD/SUB/MUL/DIV son literales del mismo tipo, evalúa la operación durante la compilación y reemplaza el nodo por un único literal, conservando el span humano y marcando el bit de plegado. Variables nunca se pliegan. Para I64, se pliega solo si no hay overflow; una división por cero tampoco se pliega. Esos casos quedan como operaciones IL para que la VM informe la trampa correspondiente. F64 conserva semántica IEEE-754.

## 5. Emisión postorden hacia Ariño IL v1

La emisión recorre expresiones en postorden: primero hijo izquierdo, luego hijo derecho y luego la instrucción. Las instrucciones usan exactamente el formato de `docs/ARINO_IL_V1.md`; no se define una segunda ISA.

| Nodo AST | Emisión IL |
|---|---|
| I64 | `PUSH_I64` + inmediato little-endian de 8 bytes |
| F64 | `PUSH_F64` + binary64 little-endian de 8 bytes |
| VARIABLE | `LOAD slot` |
| ASSIGN | emitir RHS, luego `STORE slot` |
| ADD/SUB/MUL/DIV I64 | emitir hijos y `ADD_I64` / `SUB_I64` / `MUL_I64` / `DIV_I64` |
| ADD/SUB/MUL/DIV F64 | emitir hijos y `ADD_F64` / `SUB_F64` / `MUL_F64` / `DIV_F64` |
| PROGRAM | emitir sentencias; descartar con `POP` los resultados de expresiones no finales; terminar con `HALT` |

El opcode de la VM ocupa un byte y cada formato de instrucción tiene operandos de ancho fijo; el tamaño total varía según el opcode (por ejemplo, `PUSH_I64` mide 9 bytes y `ADD_I64` 1), tal como requiere el bytecode v1 existente. El emisor calcula la profundidad máxima real de la pila y escribe `local_count`, `max_stack` y `entry_ip` en el encabezado ARIL. El pool de constantes queda disponible para futuras optimizaciones; v1 de este frontend emite literales inmediatos.

## 6. Uso y depuración

Ejecuta desde la raíz del repositorio, donde está `libarino_lexer.so`:

```sh
chmod +x arino_ast_compiler arino_il_vm
printf '%s' 'sumar 2 y 3' > suma.ari
./arino_ast_compiler --ast suma.ari
./arino_ast_compiler --check suma.ari
./arino_ast_compiler --compile suma.ari suma.arino
./arino_il_vm --verify suma.arino
./arino_il_vm --disasm suma.arino
./arino_il_vm --run suma.arino
```

`--ast` imprime opcode binario, tipo, span original, contexto y nodos hijos. `--check` ejecuta análisis semántico sin emitir bytecode. `--compile` valida, pliega constantes y escribe `.arino`; después se puede usar `--verify`, `--disasm`, `--run` o `--trace` del VM. Los errores de sintaxis/tipo/scope incluyen el offset en bytes del fuente.

## Límites de v1

El compilador cubre expresiones I64/F64, paréntesis, las familias aritméticas indicadas, asignaciones y un scope de módulo. Todavía no genera condiciones, bucles, funciones, llamadas, tipos de tensor/IA ni bloques anidados, aunque la VM IL v1 ya tiene algunas instrucciones de control de flujo. El parser/AST es funcional para este subconjunto y está preparado para ampliar nodos y scopes; no debe confundirse con el compilador integral de todo Ariño. El repositorio distribuye el compilador como binario y no guarda fuentes C o ensamblador.

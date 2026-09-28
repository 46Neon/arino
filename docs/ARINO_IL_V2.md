# Ariño IL v2.0 — contrato binario

## Estado y compatibilidad de sintaxis

Esta especificación define el formato del bytecode y el contrato de ejecución de la VM. Es una capa interna: **no cambia la sintaxis visible en español de Ariño**. El flujo previsto es fuente Ariño en español → lexer/parser → AST → IL binario.

El formato actual usa archivos `.arino` con firma ASCII de cinco bytes `ARINO` y versión 2.0. La VM también lee el formato v1 con firma `ARIL`, incluso si el archivo tiene el sufijo heredado `.aril`. Ambos son bytecode para la VM nativa Linux x86-64, no instrucciones nativas de la CPU. La IL sigue siendo interna y no altera la sintaxis pública en español.

## 1. ISA stack-based

Cada instrucción comienza con un opcode único de un byte. Los operandos multibyte son little-endian. Los valores tienen etiqueta de tipo: I64, F64 o BOOL.

| Opcode | Mnemonic | Operando | Efecto |
|---|---|---|---|
| 00 | HALT | — | Detiene ejecución; conserva resultado en la cima, si existe. |
| 01 | PUSH_I64 | inmediato de 8 bytes | Empuja entero con signo de 64 bits. |
| 02 | PUSH_F64 | bits de 8 bytes | Empuja IEEE-754 binary64. |
| 03 | PUSH_CONST | índice u32 | Empuja constante tipada. |
| 04 | POP | — | Descarta el valor superior. |
| 05 | DUP | — | Duplica el superior. |
| 06 | SWAP | — | Intercambia los dos superiores. |
| 10 | ADD_I64 | — | I64,I64 → I64. |
| 11 | SUB_I64 | — | I64,I64 → I64; calcula izquierdo menos derecho. |
| 12 | MUL_I64 | — | I64,I64 → I64. |
| 13 | DIV_I64 | — | I64,I64 → I64; división truncada hacia cero. |
| 14 | ADD_F64 | — | F64,F64 → F64. |
| 15 | SUB_F64 | — | F64,F64 → F64. |
| 16 | MUL_F64 | — | F64,F64 → F64. |
| 17 | DIV_F64 | — | F64,F64 → F64; semántica IEEE-754. |
| 18 | EQ_I64 | — | I64,I64 → BOOL. |
| 19 | LT_I64 | — | I64,I64 → BOOL. |
| 1A | EQ_F64 | — | F64,F64 → BOOL. |
| 1B | LT_F64 | — | F64,F64 → BOOL. |
| 20 | JMP | desplazamiento s32 | Salto relativo incondicional. |
| 21 | JZ | desplazamiento s32 | Desapila BOOL y salta si es falso. |
| 22 | JNZ | desplazamiento s32 | Desapila BOOL y salta si es verdadero. |
| 30 | LOAD | slot u16 | Carga variable local; pila: → valor tipado. |
| 31 | STORE | slot u16 | Guarda el valor superior en una variable local. |

`JMP`, `JZ` y `JNZ` miden el desplazamiento desde el byte posterior al operando. `JZ/JNZ` aceptan BOOL únicamente. `LOAD/STORE` acceden a slots de la trama local, no a punteros arbitrarios. El entero usa complemento a dos. Desbordamiento I64, división por cero, pila insuficiente, tipos incompatibles y slot no válido producen una trampa con IP y motivo. Comparaciones F64 siguen IEEE-754: NaN no es igual a nada y no satisface LT.

## 2. Archivo `.arino` (firma `ARINO`, v2)

Todos los offsets son desde el principio del archivo. IL v2 usa campos little-endian y offsets/tamaños de 32 bits. La firma ASCII `ARINO` tiene cinco bytes.

| Offset | Tamaño | Campo |
|---:|---:|---|
| 0 | 5 | Firma ASCII `ARINO` |
| 5 | 2 | Versión mayor: 2 |
| 7 | 2 | Versión menor: 0 |
| 9 | 2 | Tamaño del encabezado: 48 |
| 11 | 2 | Flags: cero en v2 |
| 13 | 4 | Offset de constantes |
| 17 | 4 | Tamaño de constantes |
| 21 | 4 | Cantidad de constantes |
| 25 | 4 | Offset del código |
| 29 | 4 | Tamaño del código |
| 33 | 2 | Cantidad de slots locales |
| 35 | 2 | Máximo de valores en la pila |
| 37 | 4 | IP inicial, relativo al segmento de código |
| 41 | 7 | Reservado: cero |

Cada constante tiene etiqueta u8 (01 I64, 02 F64), flags u8 en cero, reservado u16 en cero, longitud u32, payload y padding cero hasta el siguiente límite de 8 bytes. I64/F64 usan 8 bytes. El código contiene instrucciones consecutivas. Las secciones de constantes y código comienzan en offsets alineados a 8 bytes; **no hay padding entre instrucciones**. Así se conserva un fetch secuencial compacto. La VM decodifica operandos byte a byte, sin presuponer alineación del inmediato.

El verificador rechaza firma/versión/flags/reservados inválidos, secciones fuera del archivo, opcode desconocido, inmediato truncado, índice fuera de rango o destino de salto que no sea inicio de instrucción. Como compatibilidad, también acepta el encabezado v1 de 40 bytes con firma `ARIL`; las nuevas compilaciones siempre escriben v2. Un archivo contiene una función de entrada; llamadas y funciones múltiples quedan para una extensión posterior.

## 3. Máquina virtual

La VM mantiene IP relativo al código, pila de operandos, slots locales, límite `max_stack` y estado de trampa. Cada valor lógico tiene payload de 64 bits y etiqueta de tipo; la implementación puede guardar etiquetas y payloads en arreglos paralelos. `SP` apunta al próximo espacio libre: push valida capacidad e incrementa; pop valida no vacío y decrementa.

Ciclo Fetch–Decode–Execute:

1. Comprobar que IP esté dentro del código y leer el opcode; incrementar IP.
2. Decodificar los operandos en little-endian, comprobando límites.
3. Validar tipos/stack effect y ejecutar.
4. Para un salto, calcular destino = IP posterior al operando + s32; comprobar que sea una frontera de instrucción.
5. Detener en HALT o registrar una trampa con el IP de la instrucción que falló.

El resultado de HALT es el valor superior de la pila, si la hay. El registro de ejecución para depuración debe incluir IP, opcode y motivo de cualquier trampa.

## 4. AST y símbolos

El repositorio incorpora `arino_ast_compiler`, un parser/AST y emisor AST→IL funcionales **para el subconjunto de expresiones aritméticas y asignaciones descrito en [ARINO_AST_V1.md](ARINO_AST_V1.md)**. El frontend actual normaliza alias de operadores españoles a nodos binarios AST, conserva spans UTF-8 del fuente, valida tipos y variables, pliega constantes seguras y emite IL v2; la lectura IL v1 queda solo como compatibilidad. La sintaxis pública sigue en español; la IL permanece interna. El AST no implica que se admita toda la gramática de Ariño.

El frontend v1 tiene un ámbito de módulo, asigna slots locales u16 y no permite uso antes de asignar ni reasignación con tipo distinto. La VM no almacena nombres: cada slot representa la dirección lógica `frame_base + slot × 8` para el payload; la VM mantiene además su etiqueta de tipo. El diseño del símbolo conserva un enlace a scope padre para extensión futura, pero los ámbitos anidados no están implementados.

El emisor recorre los hijos de expresiones en postorden y selecciona la variante I64/F64 según el tipo validado. El IL conserva exactamente el formato existente: cada opcode es de un byte y los operandos de cada formato tienen ancho fijo; **la longitud total de instrucción depende del opcode** (por ejemplo, `PUSH_I64` ocupa 9 bytes y `ADD_I64`, 1). No se define una ISA alternativa. La emisión de condiciones/saltos del AST queda fuera del subconjunto actual, aunque la VM ya incluya instrucciones de control de flujo.

## 5. Desensamblador

La VM proporciona `--disasm` (offset de código, bytes, mnemonic y operandos) y `--trace` (ejecución con estado de pila e IP). Los offsets son relativos al inicio del código.

```text
00000000  01 06 00 00 00 00 00 00 00  PUSH_I64 6       ; [] -> [I64]
00000009  01 07 00 00 00 00 00 00 00  PUSH_I64 7       ; [I64] -> [I64,I64]
00000012  10                             ADD_I64         ; [I64,I64] -> [I64]
00000013  31 00 00                       STORE 0         ; [I64] -> []
00000016  30 00 00                       LOAD 0          ; [] -> [I64]
00000019  01 0A 00 00 00 00 00 00 00  PUSH_I64 10      ; [I64] -> [I64,I64]
00000022  11                             SUB_I64         ; [I64,I64] -> [I64]
00000023  00                             HALT            ; resultado: 3
```

## Fixture binario

La fixture `arino_il_demo.arino` usa firma ARINO v2, sin constantes, `local_count=1`, `max_stack=2`, código de 36 bytes. Calcula 6+7, guarda/carga el slot 0, resta 10 y termina con I64 3. SHA-256: `993dde07620905c987062deb4d4f14e4013e44d5144416a5714e9c72ed46993d`.

## VM y pruebas disponibles

El ejecutable `arino_il_vm` ofrece (tras descargarlo o clonarlo, ejecutar `chmod +x arino_il_vm` para habilitar permisos de ejecución):

```sh
./arino_il_vm --verify archivo.arino
./arino_il_vm --disasm archivo.arino
./arino_il_vm --run archivo.arino
./arino_il_vm --trace archivo.arino
./arino_il_vm --max-steps 20 archivo.arino
```

El verificador comprueba encabezado v2 o v1 legado, secciones, opcodes e inmediatos, índices y destinos de salto en fronteras de instrucción. La VM ejecuta la ISA documentada y detecta trampas como overflow I64, división por cero, underflow, tipos incompatibles, locals no inicializados y límite de instrucciones. GitHub Actions verifica el formato ELF x86-64, verifica/ejecuta fixtures válidas con resultados exactos, confirma rechazos y traps, y publica el VM junto con las fixtures `.arino` como artefacto y comprueba una ruta temporal con sufijo `.aril` heredado.

Fixtures `.arino` válidas: `arino_il_demo.arino` (resultado 3 con LOAD/STORE), `arino_il_loop.arino` (bucle y ramas, 10), `arino_il_stack.arino` (DUP/SWAP/POP, 5), `arino_il_branch.arino` (EQ_I64/JNZ, 1), `arino_il_float.arino` (constantes y aritmética/comparaciones F64), `arino_il_constants.arino` (pool I64, 84), `arino_il_f64_imm.arino` (PUSH_F64, 4) y `arino_il_div_i64.arino` (20/4, 5). Las fixtures negativas `.arino` comprueban saltos inválidos, opcode desconocido, inmediato truncado, división por cero, underflow, tipos incorrectos, overflow y agotamiento del límite de pasos.

**Límite:** el frontend Ariño→AST→IL existe y se valida en CI solo para el subconjunto de aritmética y asignaciones definido en `ARINO_AST_V1.md`. No analiza ni emite bloques, condiciones, bucles, funciones/llamadas, scopes anidados o sintaxis de tensores/IA. El flujo no equivale a un compilador integral de Ariño.

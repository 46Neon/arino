# Ariño IL v1.0 — contrato binario

## Estado y compatibilidad de sintaxis

Esta especificación define el formato del bytecode y el contrato de ejecución de la VM. Es una capa interna: **no cambia la sintaxis visible en español de Ariño**. El flujo previsto es fuente Ariño en español → lexer/parser → AST → IL binario.

Esta entrega incorpora la especificación y el artefacto binario de prueba `arino_il_demo.aril`. **Todavía no incluye una VM ni un compilador AST→IL ejecutables**; por tanto, el fixture está validado estructuralmente, pero aún no puede ejecutarse en una VM de Ariño. El bytecode no es código máquina de la CPU: lo ejecutará una VM nativa.

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

## 2. Archivo `.aril`

Todos los offsets son desde el principio del archivo. Versión 1 usa campos little-endian y offsets/tamaños de 32 bits.

| Offset | Tamaño | Campo |
|---:|---:|---|
| 0 | 4 | Magic ASCII `ARIL` |
| 4 | 2 | Versión mayor: 1 |
| 6 | 2 | Versión menor: 0 |
| 8 | 2 | Tamaño del encabezado: 40 |
| 10 | 2 | Flags: cero en v1 |
| 12 | 4 | Offset de constantes |
| 16 | 4 | Tamaño de constantes |
| 20 | 4 | Cantidad de constantes |
| 24 | 4 | Offset del código |
| 28 | 4 | Tamaño del código |
| 32 | 2 | Cantidad de slots locales |
| 34 | 2 | Máximo de valores en la pila |
| 36 | 4 | IP inicial, relativo al segmento de código |

Cada constante tiene etiqueta u8 (01 I64, 02 F64), flags u8 en cero, reservado u16 en cero, longitud u32, payload y padding cero hasta el siguiente límite de 8 bytes. I64/F64 usan 8 bytes. El código contiene instrucciones consecutivas. Las secciones de constantes y código comienzan en offsets alineados a 8 bytes; **no hay padding entre instrucciones**. Así se conserva un fetch secuencial compacto. La VM decodifica operandos byte a byte, sin presuponer alineación del inmediato.

El verificador rechaza magic/versión/flags inválidos, secciones fuera del archivo, opcode desconocido, inmediato truncado, índice fuera de rango o destino de salto que no sea inicio de instrucción. En v1, un archivo contiene una función de entrada; llamadas y funciones múltiples quedan para una extensión posterior.

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

El compilador no traduce palabras directamente a bytes: primero valida semántica y tipos del AST, luego emite IL tipada. Para una suma, emite expresión izquierda, expresión derecha y ADD_I64 o ADD_F64 según el tipo. Una condición emite comparación a BOOL y después JZ/JNZ. Asignaciones usan STORE; lecturas, LOAD. Los saltos se emiten inicialmente con etiquetas y se parchean al conocer el destino; el desplazamiento se calcula desde el final del operando s32.

Tabla de símbolos temporal del compilador: nombre, ámbito, tipo, slot u16, mutabilidad y span del fuente (archivo/línea/columna). Cada nombre se resuelve a un slot dentro de su ámbito. El bytecode no almacena nombres: cada slot representa la dirección lógica `frame_base + slot × 8` para el payload; la VM mantiene además su etiqueta de tipo. Al salir de un ámbito, el compilador puede reutilizar slots no vivos. La cantidad máxima asignada se escribe en `local_count`.

## 5. Desensamblador

Formato recomendado por instrucción: offset de código, bytes originales, mnemonic y operandos decodificados; el modo trace agrega pila antes/después, slots tocados e IP siguiente. Los offsets son relativos al inicio del código.

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

`arino_il_demo.aril` tiene encabezado ARIL v1, sin constantes, `local_count=1`, `max_stack=2`, código de 36 bytes. Calcula 6+7, guarda/carga el slot 0, resta 10 y termina con I64 3. SHA-256: `ec2f9a207b69501577cf7eef9d60f6ce60ad2a1cbbc4f5785904600ff7d61f7a`.

**Límite actual:** se creó la especificación y el fixture binario; el repositorio aún no tiene VM, verificador ni compilador AST→IL ejecutables. No considerar el IL operativo hasta implementar esos componentes y pasar pruebas de ejecución.

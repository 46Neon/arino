# Especificación OPT-IR v1 para Ariño

## Estado y alcance

Este documento define una arquitectura para optimizar una IR interna TAC/SSA y producir código máquina nativo. No describe una implementación ya existente. La IL v2 actual de Ariño es bytecode para una VM de pila, no código de CPU; se conserva como formato compatible y VM de referencia mientras se incorpora un backend nativo.

## 1. Contrato de IR y datos

Cada unidad compilable se representa como un CFG de bloques básicos con terminadores explícitos. Identificadores únicos dentro de la unidad:

- `ValueId`: valor temporal tipado.
- `BlockId`: bloque del CFG.
- `InstId`: instrucción.

Estructuras requeridas:

```text
Instruction {
  id: InstId
  opcode
  defs: ValueId[]
  uses: ValueId[]
  type
  effects: PURE | READ_MEMORY | WRITE_MEMORY | CALL | CONTROL | VOLATILE
  may_trap: bool
  source_span_utf8: [start, end)
}

BasicBlock {
  id: BlockId
  instructions[]
  predecessors: BlockId[]
  successors: BlockId[]
  terminator
}

Phi {
  dst: ValueId
  incoming: (predecessor: BlockId, value: ValueId)[]
}
```

### Invariantes del verificador

1. En SSA, cada `ValueId` tiene una única definición y todo uso tiene definición válida.
2. Cada `phi` tiene una entrada por predecesor; cada operando se considera un uso en su arista entrante.
3. Tipos de operandos, resultados y clases de registro son compatibles con el opcode.
4. Efectos laterales, accesos a memoria y posibilidad de trap están anotados explícitamente.
5. Los terminadores y las aristas del CFG coinciden; el destino de cada salto es un bloque válido.
6. Cada reescritura conserva los spans y las reglas observables de overflow, NaN, signed zero y traps.

Todos los pases deben invocar el verificador en compilaciones de prueba. En modo de desarrollo, verificar también la IR tras cada pase.

## 2. Use-Def y Live Variable Analysis

Índices mantenidos por el compilador:

```text
def[ValueId]  -> InstId
uses[ValueId] -> lista de {InstId, posición_de_operando, arista_phi}
```

Las mutaciones pasan por operaciones centrales (`replace_use`, `replace_all_uses`, `erase_instruction`) para mantener los índices coherentes. Si la entrada es TAC con variables reasignables, convertir primero a SSA. Como análisis previo opcional, calcular *reaching definitions* con:

```text
IN[B]  = unión de OUT[P] para P en pred(B)
OUT[B] = GEN[B] unión (IN[B] - KILL[B])
```

Para liveness por bloque, calcular `USE[B]` como usos previos a una definición local y `DEF[B]` como todas las definiciones:

```text
OUT[B] = unión de IN[S] para S en succ(B)
IN[B]  = USE[B] unión (OUT[B] - DEF[B])
```

Resolver en orden postorden inverso con un worklist de bloques. Si cambia `IN[B]`, reinsertar los predecesores. Para liveness por instrucción, recorrer hacia atrás: guardar `LIVE_AFTER`, quitar defs, agregar uses y guardar `LIVE_BEFORE`. Los operandos de `phi` se agregan a la salida del predecesor que corresponde a cada entrada.

Representar conjuntos como bitsets por `ValueId`. Recalcular tras cambios de usos/defs o aristas; invalidar explícitamente los análisis derivados cuando un pase altera el CFG.

## 3. Peephole local

Ejecutar primero en TAC/SSA y después de selección/asignación en IR de máquina. Cada regla debe indicar su precondición semántica y su coste en el target.

| Patrón | Reescritura | Restricción |
|---|---|---|
| `t = x + 0`, `t = x * 1` | `t = x` | Tipo y comportamiento numérico idénticos. |
| `t = c1 op c2` | `t = c_resultado` | No alterar traps, overflow ni semántica IEEE-754. |
| `t = MOV x` | Sustituir usos de `t` por `x` | `x` debe seguir disponible. |
| `STORE s,v; LOAD s` | Reusar `v` | Sin escritura intermedia ni alias posible. |
| `JMP` al bloque siguiente | Eliminar `JMP` | Destino y layout deben coincidir. |
| comparación + branch | Fusionar si el ISA lo ofrece | Preservar flags y usos posteriores. |

No eliminar cómputos muertos que puedan trapear, salvo que el análisis pruebe que el trap es imposible y que la reescritura preserva la semántica. No bajar `x * 2^k` a shift si el target no conserva el comportamiento de overflow de Ariño. El coste se obtiene de `TargetInfo` (latencia, throughput, tamaño, uops y presión de registros), no solo del número de instrucciones.

## 4. Propagación global de constantes (SCCP)

Usar SCCP sobre SSA. Estado por valor:

```text
UNDEF
CONST(valor tipado)
OVERDEFINED
```

Mantener worklists separados para instrucciones SSA y aristas/bloques ejecutables:

1. Marcar ejecutable el bloque de entrada.
2. Evaluar instrucciones solo en bloques ejecutables y actualizar el estado de sus defs.
3. Evaluar cada `phi` con entradas cuyas aristas sean ejecutables.
4. Si una condición de branch es constante, activar solo la arista tomada; si es `OVERDEFINED`, activar todas las salidas posibles.
5. Reprocesar consumidores cuando cambia el estado de un valor.
6. Al estabilizar, reemplazar usos constantes, plegar operaciones válidas y simplificar ramas.

No propagar constantes a través de memoria sin análisis de alias/MemorySSA. La evaluación constante debe respetar tipos, overflow, NaN, signed zero y traps. Una operación con trap observable no se borra solo porque su resultado no se use.

## 5. Eliminación global de código muerto

Aplicar DCE en SSA tras SCCP y repetir hasta que no haya cambios:

1. Crear raíces con retornos, stores observables, llamadas con efectos, operaciones volátiles, terminadores y operaciones que puedan trapear.
2. Marcar transitivamente defs usados por esas raíces.
3. Borrar instrucciones puras no marcadas.
4. Eliminar bloques inalcanzables, reparar `phi` y simplificar branches.
5. Volver a ejecutar SCCP/DCE si quedaron nuevas oportunidades.

En TAC/MIR no SSA, una instrucción puede borrarse si todas sus defs están muertas y carece de efectos observables. Tratar stores globales y cargas con alias de forma conservadora hasta disponer de MemorySSA o `mod/ref` suficiente.

## 6. Asignación de registros por coloreo

Entrada: IR de máquina con temporales virtuales, liveness y clases de registros. Construir el grafo de interferencia desde los valores vivos después de cada definición; conectar `d` con todos los valores vivos simultáneamente. Omitir inicialmente la arista entre origen/destino de un `MOV` para facilitar coalescing.

Aplicar una variante iterada Chaitin–Briggs:

1. **Simplify:** apilar nodos de grado menor que `K`, donde `K` es el número de registros asignables de esa clase.
2. **Coalesce:** fusionar copias compatibles si la fusión no vuelve el grafo no coloreable.
3. **Freeze:** retirar movimientos que bloquean la simplificación.
4. **Spill candidate:** elegir un valor de bajo coste, usando frecuencia estimada de bloque y usos/defs ponderados por profundidad de loop frente al grado.
5. **Select:** desapilar y asignar un color que no usen sus vecinos.
6. **Rewrite spills:** insertar loads/stores de slots, crear temporales y repetir liveness/asignación.

`TargetInfo` debe definir registros GPR/SIMD, reservados, fijos, clobbered por llamadas, preservados por ABI, restricciones de opcode y coste de spill. Tratar operandos `phi` como usos en aristas. Tras colorear, materializar copias paralelas en esas aristas; dividir aristas críticas si hace falta y resolver ciclos mediante temporal o slot auxiliar.

## 7. Orden de pases

1. Verificar HIR tipado y efectos.
2. Bajar a TAC/CFG; construir SSA y dominadores.
3. Verificar SSA; generar índices Use-Def.
4. Simplificación local, plegado básico y propagación de copias.
5. SCCP.
6. GVN/CSE opcional, con equivalencia tipada y reglas de memoria conservadoras.
7. DCE y simplificación del CFG.
8. Repetir 4–7 hasta punto fijo o límite documentado de iteraciones.
9. Seleccionar/legalizar instrucciones para el target.
10. Peephole de máquina y liveness.
11. Asignación de registros, resolución de `phi` y spills.
12. Peephole post-asignación, planificación local y relajación de saltos.
13. Emitir código, relocations y metadatos; validar el artefacto.

## 8. Métricas, pruebas y aceptación

Cada pase registra antes/después:

- instrucciones, bloques y aristas;
- bytes de código, constantes y relocations;
- presión máxima por clase, spills y bytes de spill;
- tamaño máximo del frame y uso de stack;
- ciclos estimados por el modelo del target;
- ciclos, instrucciones retiradas, IPC, branch misses y cache misses medidos;
- tiempo y memoria máxima del compilador.

Medir con igual corpus, target y flags, varias ejecuciones, mediana y dispersión. En bare metal, usar TSC serializado o PMU si está disponible y documentar frecuencia/condiciones. Medir el código nativo, no el tiempo de la VM, para atribuir ciclos al backend.

Gates de corrección:

- IR verifier antes/después de cada pase en CI.
- Pruebas diferenciales antes/después del optimizador.
- Casos explícitos de overflow, división por cero, NaN, signed zero, aliasing y efectos laterales.
- Fuzzing de IR bien tipado y malformed-IR rejection.
- Benchmarks separados de pruebas de corrección; no aceptar una mejora estadística dudosa ni una regresión de semántica.

## 9. Integración con Ariño

El IL v2 publicado es una IR de pila para la VM y hoy cubre generación escalar; no tiene asignación de registros ni es código de CPU. El parser reciente puede construir AST de agentes/condiciones, pero esas formas aún no llegan a un backend. La ruta prevista para código nativo es:

```text
AST/HIR tipado → CFG SSA interno → optimizador OPT-IR → IR de máquina x86-64
→ asignación de registros → emisor nativo
```

El IL v2 y la VM pueden conservarse como formato de compatibilidad y oráculo de ejecución mientras se valida el backend nativo. Esta especificación no afirma que esos pases o la generación nativa ya estén implementados.

## 10. Restricción de implementación binaria

- El optimizador y el backend se implementan y distribuyen como código máquina binario nativo; el repositorio público no incorpora implementaciones en C, ensamblador, Python, Bash u otros lenguajes, ni generadores temporales escritos en esos lenguajes.
- No se sustituye el compilador por un wrapper interpretado ni se agrega un ejecutable auxiliar externo que se convierta en una ruta paralela al compilador.
- La integración debe formar parte del artefacto binario nativo del compilador, conservar el flujo anterior que ya esté soportado y verificarse ejecutando el binario publicado contra pruebas positivas, negativas y diferenciales.
- La CI valida y ejecuta el artefacto binario exacto; no debe presentar una reconstrucción desde código fuente no aprobado como si fuera la implementación del optimizador.
- El target debe fijarse antes de generar instrucciones: el ELF actual es x86-64 Linux con dependencias dinámicas; cualquier target bare-metal x86/BIOS requiere contrato de arranque, mapa de memoria, interrupciones, salida de traps y ABI definidos por separado.

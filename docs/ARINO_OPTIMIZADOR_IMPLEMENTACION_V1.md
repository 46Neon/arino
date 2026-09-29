# Especificación de implementación del optimizador Ariño v1

## Estado

Especificación de implementación del optimizador interno OPT-IR/TAC/SSA. Complementa `docs/ARINO_OPTIMIZADOR_IR_V1.md`; no afirma que el optimizador, el backend nativo ni la imagen bare-metal ya estén implementados.

## 1. Objetivo y límites

El optimizador es una etapa interna del compilador Ariño. No se crea un ejecutable, biblioteca, servicio ni ruta de compilación separados para él. Sus estructuras, pasadas, verificador y selección de destino se integran dentro del compilador unificado de Ariño.

La sintaxis pública del lenguaje permanece en español. TAC/SSA es una representación privada del compilador y no se expone como lenguaje fuente ni reemplaza IL v2.

La imagen de distribución final debe integrar los componentes necesarios —punto de entrada mínimo, compilador, optimizador, runtime y adaptadores— en una sola imagen nativa. Las etapas pueden estar modularizadas dentro de esa imagen, pero no distribuidas como ejecutables dependientes entre sí.

Esta especificación no permite implementar la lógica de producción en C, ensamblador, Python, Bash ni otro lenguaje, ni usar generadores temporales en esos lenguajes. El mecanismo autorizado de bootstrap binario es una precondición de implementación.

## 2. Compatibilidad y línea base

1. `docs/ARINO_OPTIMIZADOR_IR_V1.md` define las invariantes, análisis y familias de optimización; esta especificación define orden, integración y aceptación.
2. `docs/ARINO_IL_V2.md` sigue siendo el contrato de IL v2 y de la VM. La optimización no cambia su firma, encabezado, opcodes ni compatibilidad v1.
3. IL v2 es bytecode de VM, no instrucciones x86-64. La asignación de registros, peephole de máquina y emisión nativa pertenecen a un backend nativo separado dentro del mismo compilador, y no se declararán implementados antes de que exista y pase pruebas ese backend.
4. El alcance funcional de cada gate queda limitado a construcciones del lenguaje que el frontend realmente analiza, tipa y baja. No se afirmará soporte integral por tener optimizaciones o HIR documental para una construcción.

## 3. Ubicación en el pipeline

Pipeline objetivo:

```text
fuente Ariño en español
→ lexer/parser
→ AST
→ análisis semántico y HIR tipado
→ lowering a CFG/TAC/SSA interno
→ verificador OPT-IR
→ pasadas optimizadoras
→ lowering de destino
→ emisor existente compatible o backend nativo
```

El optimizador corre dentro de la invocación normal de compilación. No se permite invocarlo como proceso externo ni pasar archivos intermedios a un optimizador independiente. La opción de depuración, si se añade, solo puede volcar una representación diagnóstica; no constituye una interfaz de producción ni cambia la sintaxis de Ariño.

## 4. Gates de implementación

### G0 — Bootstrap y reproducibilidad

- Identificar y aprobar el proceso binario que reconstruye el compilador unificado.
- Registrar artefactos de entrada, versiones, target, parámetros y hashes.
- Repetir la construcción y comparar el artefacto resultante.
- No modificar un ELF opaco ni usar su aplanado como sustituto de reconstrucción.

**Gate:** existe un camino autorizado y repetible que produce el ejecutable exacto que se probará.

### G1 — IR interna mínima

- Definir estructuras internas para unidades, bloques, instrucciones, ValueId, tipos, spans y efectos.
- Bajar solo el subconjunto fuente que ya sea funcional de extremo a extremo.
- Preservar spans UTF-8 y semántica observable de tipos, traps, overflow y F64.
- Mantener la memoria temporal de IR separada del layout de AST y del heap de ejecución.

**Gate:** el lowering representa correctamente programas válidos e inválidos del subconjunto y el verificador rechaza IR malformada.

### G2 — Verificador e infraestructura de pasadas

- Verificar definiciones/usos, tipos, aristas CFG, terminadores y Phi.
- Mantener o invalidar índices Use-Def y análisis derivados después de cada mutación.
- Ejecutar el verificador antes y después de cada pase en pruebas.
- Hacer determinista el orden de recorridos, identificadores y emisión.

**Gate:** pruebas negativas de IR corrupta son rechazadas sin crash, acceso fuera de rango ni artefacto parcial.

### G3 — Optimizaciones independientes del destino

Implementar en este orden, con pruebas aisladas y diferenciales:

1. plegado de constantes y simplificaciones locales con semántica exacta;
2. propagación de copias y peephole de TAC/SSA;
3. índices Use-Def y análisis de liveness;
4. SCCP y simplificación de ramas;
5. DCE seguro, conservando efectos y traps observables;
6. repetición hasta punto fijo con límite determinista de iteraciones.

No aplicar una reescritura si cambia overflow, división por cero, NaN, signed zero, aliasing, orden de efectos o diagnósticos/traps definidos por Ariño.

**Gate:** el corpus sin optimizar y optimizado produce el mismo resultado, trap y salida observable para cada caso cubierto.

### G4 — Integración de salida compatible

- Mantener el formato IL v2 y las fixtures v1/v2 existentes.
- Aplicar únicamente transformaciones que puedan expresarse correctamente en la ruta de emisión soportada.
- No añadir opcodes ni modificar el archivo `.arino` para facilitar al optimizador.

**Gate:** la VM verifica y ejecuta el IL emitido; resultados y traps coinciden entre compilación optimizada y no optimizada.

### G5 — Backend nativo y destino bare-metal

Este gate comienza solo tras fijar CPU, ABI, contrato de imagen y runtime del target.

- Añadir `TargetInfo` dentro del compilador para registros, clases, restricciones, llamadas y costes del target acordado.
- Bajar OPT-IR a IR de máquina; ejecutar selección, liveness, asignación de registros, spills y emisión nativa conforme a OPT-IR v1.
- Integrar el código generado, cargador y adaptadores en la imagen única; ningún módulo ejecutable externo.
- Conservar IL/VM como compatibilidad/oráculo mientras cada feature tenga cobertura diferencial.

**Gate:** imagen única arranca en el emulador objetivo y el programa nativo coincide con el resultado del camino de referencia para el corpus soportado.

## 5. Contrato de fallos y publicación de artefactos

- Un error de parseo, tipado, verificación, optimización o emisión termina la compilación con diagnóstico estable.
- No publicar archivos incompletos: construir en memoria o en región temporal controlada y confirmar el artefacto solo tras validación completa.
- Toda imagen de salida incluye versión/target definidos por su contenedor; offsets y tamaños se validan con aritmética sin desbordamiento.
- El optimizador nunca concede permisos de ejecución a datos o bytecode de VM. Las secciones nativas, cuando existan, siguen el contrato W^X del runtime.

## 6. Pruebas de aceptación

### Corrección

- Ejecutar todas las pruebas actuales de lexer, parser, AST, semántica, IL y VM.
- Probar cada pase con casos positivos y negativos y verificar IR antes/después.
- Comparar ejecución optimizada y no optimizada para resultado, stdout/diagnóstico, traps y límites.
- Incluir overflow I64, división por cero, NaN, signed zero, variables locales, ramas, loops solo cuando el frontend los soporte, efectos y aliasing.
- Rechazar opcode/IR/target malformado y saltos o referencias inválidas sin producir artefacto parcial.

### Rendimiento

- Medir compilación y código generado por separado.
- Comparar mismo corpus, target y condiciones; informar tamaño, tiempo, memoria y métricas disponibles con mediana y dispersión.
- No declarar una mejora por menos instrucciones si hay regresión de semántica, tamaño o rendimiento medido.

### CI y revisión

- Los checks deben identificar el SHA exacto del artefacto binario probado.
- CI puede orquestar pruebas, pero no implementar ni generar el optimizador en un lenguaje prohibido.
- Una etapa no se cierra por documentación, un subconjunto aislado o checks que no ejercitan el optimizador real.
- Cambios por fase en una PR con alcance explícito; no modificar `main` directamente.

## 7. Bloqueos y decisiones pendientes

1. Mecanismo autorizado y reproducible para reconstruir el núcleo binario opaco.
2. CPU/placa y ABI bare-metal concretos; QEMU/SeaBIOS solo es un perfil de prueba provisional.
3. Primer subconjunto fuente que se baja realmente a TAC/SSA y su cobertura completa.
4. Límites de memoria/IR derivados del target, no inventados en esta especificación.
5. Cuándo se habilita G5: requiere backend nativo funcional, contrato de imagen y adaptadores integrados en la VM.

Hasta cerrar G0, esta especificación es normativa para el trabajo futuro, pero no constituye una implementación ejecutable del optimizador.

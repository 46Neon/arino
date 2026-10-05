# Ariño: fuente conceptual en español y compilación a binario nativo

**Estado:** propuesta para una implementación futura. Este documento no añade sintaxis aceptada ni código ejecutable.

## 1. Propósito y alcance

Aquí, «interfaz española» significa la **interfaz del lenguaje fuente**: la forma en que una persona escribe un programa Ariño en español corriente. No se refiere principalmente a una interfaz gráfica. La misma fuente Ariño debe ser interpretada por el compilador de Ariño y convertirse en un binario nativo ejecutable para el destino elegido.

Este documento describe esa futura cadena completa: desde una explicación conceptual en español hasta análisis, representación intermedia, optimización, generación de instrucciones de máquina, enlazado, runtime y ejecución. El objetivo no es traducir el texto con un modelo de lenguaje ni pasarlo por un transpilador escrito en otro lenguaje. La gramática del español aceptado debe ser precisa, y la implementación de producción del compilador y sus componentes debe ser nativa. Las dependencias nativas de runtime ya aceptadas por el contrato del proyecto, como libc/libm, no convierten esos lenguajes en lenguajes de implementación del compilador.

La entrega pedida es **solo de documentación**: no cambia el parser, el backend, el runtime, los binarios, la gramática aceptada ni afirma que la compilación nativa de estas frases ya funcione.

La primera regla de diseño de la futura fuente es que **la frase completa puede ser el programa**. No tiene que parecer un bloque de código. El ejemplo de referencia para la futura sintaxis de cálculo es:

> Predice qué resultado da cada una de estas entradas: uno, dos y tres; cuatro, cinco y seis. Para cada entrada, calcula dos cantidades. La primera es el primer valor más el tercero más uno. La segunda es el segundo valor más el tercero más dos. Si alguna cantidad queda por debajo de cero, tómala como cero. La respuesta es el doble de la primera cantidad más el triple de la segunda, más cuatro.

El ejemplo es un objetivo de diseño, no una capacidad actualmente implementada por el compilador oficial.

## 2. Principios de diseño

1. **Primero el concepto, no el bloque.** La forma normal debe ser una explicación seguida, con frases y conectores que expresen intención, orden, condición y resultado. No se deben exigir llaves, declaraciones de tipos o nombres técnicos si el concepto puede inferirse sin ambigüedad.
2. **Español corriente para describir lo que se quiere.** El usuario debe poder expresar propósitos, ejemplos, contexto, reglas y resultados con palabras comprensibles. No obligarlo a escribir `tensor`, `densa`, `bias`, `slot` u otros términos internos.
3. **Números reconocibles como parámetros.** Los parámetros cuantitativos se escriben como numerales y entre paréntesis, por ejemplo, «durante (20) épocas» o «con una tasa de (0.01)». La gramática debe distinguir estos valores de las palabras que describen el concepto. El convenio exacto para decimales, signos, unidades y listas debe fijarse antes de implementar la gramática general.
4. **Contextos distintos, formas distintas.** Entrenar un modelo y construir un agente de IA son intenciones diferentes. Ariño debe identificar cuál se está describiendo y analizar cada una con reglas y resultados semánticos propios.
5. **Agrupación explícita cuando haga falta.** La prosa es preferible a los bloques. Si una construcción realmente requiere abrir una llave `{`, la llave de cierre `}` será obligatoria; nunca se cerrará una estructura de manera implícita.
6. **Claridad antes que adivinación.** El compilador puede aceptar variantes lingüísticas previstas, pero debe señalar una frase ambigua en vez de inventar su sentido. No debe prometer que cualquier párrafo arbitrario en español se puede compilar.
7. **La representación interna no dicta la forma visible.** El árbol sintáctico, la representación intermedia, los tensores, las ranuras y los opcodes son herramientas internas. No deben filtrarse a la sintaxis cotidiana.
8. **No presentar como implementado lo que solo está diseñado.** Cada función documentada debe indicar si es propuesta, prototipo ejecutado o capacidad ya integrada en la ruta oficial.

## 3. Dos contextos de lenguaje

### 3.1 Entrenamiento de un modelo

El inicio de la descripción debe dejar claro que la intención es entrenar un modelo. La gramática de ese contexto debe reconocer, cuando estén implementados, el objetivo, los datos de entrenamiento, la arquitectura o método, los parámetros numéricos, la validación y el resultado que se espera guardar.

Ejemplo de prosa objetivo:

> Entrena un modelo que distinga fotografías de gatos y perros. Usa estos ejemplos de aprendizaje: … Comprueba el resultado con estos ejemplos aparte: … Entrénalo durante (20) épocas, con grupos de (8) ejemplos y una tasa de aprendizaje de (0.01). Guarda el modelo resultante con el nombre Clasificador.

Los paréntesis identifican los valores cuantitativos. Los puntos suspensivos indican datos que el ejemplo no especifica; no son una forma de sintaxis ejecutable. La palabra «entrena» solo será aceptada cuando exista una ruta real de entrenamiento; no se debe interpretar una declaración descriptiva como si hubiera entrenado pesos.

El análisis semántico debe comprobar, según el método disponible, que los datos y etiquetas tengan formas compatibles, que los parámetros estén dentro de los rangos admitidos y que el conjunto de validación no se confunda con el de entrenamiento. Debe producir errores en español con la frase y el dato que causaron el problema.

### 3.2 Construcción de un agente de IA

El inicio debe dejar claro que se está construyendo un agente, no entrenando un modelo. Este contexto describe su propósito, la información que debe tener en cuenta, el modelo que puede utilizar, sus herramientas y sus límites.

Ejemplo de prosa objetivo:

> Construye un agente que ayude a las personas a organizar sus tareas. Ten en cuenta que prefiere explicaciones claras y breves. Puede consultar el calendario y proponer horarios, pero debe pedir confirmación antes de cambiar una cita. Usa el modelo Conversador.

El agente describe un comportamiento y una relación con herramientas; por sí solo no entrena ni modifica los parámetros del modelo citado. El análisis debe separar las instrucciones del agente, el texto de contexto, las referencias a modelos y las autorizaciones de herramientas. Una acción externa debe requerir una autorización expresamente definida por el sistema anfitrión.

### 3.3 La diferencia no debe depender de una palabra aislada

La frase de apertura establece el contexto semántico principal: entrenamiento de modelo, construcción de agente o cálculo/inferencia. Las frases posteriores se interpretan dentro de ese contexto. Así, «contexto» en una definición de agente no se confunde con datos de entrenamiento; «usa este modelo» en un agente no inicia entrenamiento; y una mención de una cantidad no se convierte automáticamente en un parámetro de aprendizaje.

Si una oración mezcla dos intenciones —por ejemplo, entrenar un modelo y crear un agente que lo use— el programa deberá expresar ambas como pasos conceptuales separados y enlazados por nombre. El compilador no debe mezclarlas en una sola operación opaca.

## 4. Texto, números y delimitadores

### 4.1 Texto en español

Las descripciones, los objetivos, los contextos y las reglas pueden redactarse como texto normal. La futura interfaz debe distinguir el texto que se entrega a un modelo o agente de las instrucciones que Ariño interpreta. Las cadenas de contenido deben tener límites reconocibles para que una oración citada no se ejecute como una orden del compilador.

La interfaz no debe obligar a convertir una explicación en una lista de etiquetas técnicas. Cuando una frase tenga más de una lectura válida, la herramienta pedirá precisión e indicará las interpretaciones posibles.

### 4.2 Parámetros numéricos

Los parámetros cuantitativos se escriben con cifras y entre paréntesis, integrados en la oración: «durante (20) épocas», «grupos de (8) ejemplos», «tasa de aprendizaje de (0.01)». La gramática debe conservar el valor numérico exacto y su unidad o función semántica; no basta con reconocer dígitos sin saber qué parámetro representan.

Hay que definir y probar explícitamente: enteros, decimales, signo negativo, notación científica si se admite, separadores de miles, unidades y listas de parámetros. Para evitar que una coma decimal se confunda con un separador de lista, el formato numérico debe ser único por versión y documentarse en español. El ejemplo `(0.01)` es ilustrativo, no una decisión irrevocable sobre la escritura decimal.

### 4.3 Llaves

La escritura conceptual no usará llaves por defecto. Si una versión de la gramática las utiliza para agrupar un bloque complejo, cada `{` debe tener su `}` correspondiente. El analizador debe detectar llaves ausentes, extras o en orden incorrecto, señalar su posición y rechazar la fuente en vez de completar el bloque por intuición.

La interfaz gráfica puede mostrar la llave correspondiente al mover el cursor y resaltar la pareja, pero esta ayuda no sustituye la verificación del compilador.

## 5. Proceso desde la prosa hasta el programa nativo

La conversión no debe ser una traducción directa de palabras españolas a bytes. Debe conservar el sentido de cada etapa y permitir diagnosticar dónde falló.

1. **Recepción de la fuente.** Leer el texto en UTF-8, conservar posiciones de línea y columna y normalizar Unicode de forma coherente sin cambiar las palabras visibles del usuario.
2. **Reconocimiento de la intención.** Identificar el contexto principal —entrenamiento, agente, cálculo u otra operación soportada— a partir de la construcción completa, no de una coincidencia accidental con una palabra.
3. **Análisis léxico.** Reconocer palabras, conectores, nombres, números parentizados, signos y límites de texto. Guardar la ubicación de cada elemento para explicar errores en el fragmento original.
4. **Análisis de la gramática española controlada.** Agrupar las frases en acciones, condiciones, entradas, parámetros y resultados. La gramática inicial debe ser deliberadamente acotada: español natural y legible, pero con relaciones suficientemente precisas para que dos lecturas no produzcan operaciones diferentes.
5. **Construcción de una representación semántica.** Crear nodos distintos para, por ejemplo, objetivo de entrenamiento, conjunto de datos, parámetro numérico, contexto de agente, herramienta autorizada, condición, operación aritmética y resultado. Los nombres de estos nodos son internos y no obligan al usuario a escribirlos.
6. **Resolución de nombres y comprobación semántica.** Resolver referencias a modelos, agentes y datos; comprobar tipos, dimensiones, compatibilidad y permisos. Rechazar referencias inexistentes, parámetros fuera de rango, formas incompatibles y acciones no autorizadas.
7. **Representación intermedia tipada.** Convertir el significado validado en una representación independiente de la redacción española. Esto permite que distintas frases admitidas describan la misma operación y que un cambio de sintaxis no altere el backend.
8. **Optimización verificable.** Aplicar únicamente transformaciones cuya equivalencia pueda demostrarse con pruebas. Conservar trazabilidad desde cada operación resultante hasta la frase de origen para depuración y auditoría.
9. **Bajada a operaciones ejecutables.** Para inferencia, convertir los cálculos en operaciones numéricas soportadas. Para entrenamiento, usar una ruta distinta que implemente de verdad la actualización de parámetros, pérdida y validación. Para agentes, generar una configuración y un ciclo de ejecución que conecten modelo, contexto, herramientas y límites.
10. **Generación de código máquina nativo.** El backend de Ariño debe bajar la representación intermedia a instrucciones de máquina del destino y emitir el binario final —por ejemplo, ELF para Linux o PE para Windows— respetando su ABI y resolviendo solo las dependencias nativas permitidas. La interfaz en español es la fuente de Ariño: el compilador de Ariño la analiza y genera directamente el artefacto nativo; no se requiere traducirla primero a C, C++, Rust, Python, Bash u otro lenguaje para que otro compilador haga el trabajo. El frontend, parser, análisis semántico, IR, optimizador, backend, VM/runtime y bootstrap de producción deben cumplir el contrato de implementación nativa del proyecto. Tener una representación intermedia o un bundle de IL no demuestra todavía que esa cadena integrada de binario nativo exista.
11. **Ejecución y validación.** Ejecutar el artefacto en el runtime correspondiente, comprobar sus resultados contra casos de referencia y verificar que errores de entrada no generen un binario parcial que parezca válido.
12. **Reproducibilidad.** Registrar versión del compilador, versión de gramática, plataforma, opciones, artefactos y hashes. La reconstrucción doble B1/B2 debe coincidir antes de declarar reproducible una ruta de bootstrap.

## 6. Interfaz de escritura y explicación

La interfaz futura debe tratar el párrafo como una unidad de significado y, a la vez, dejar que el usuario comprenda cómo se interpretó.

- Mostrar el texto original sin reescribirlo silenciosamente.
- Permitir seleccionar una frase y ver su interpretación en palabras corrientes: «esto fija el número de épocas» o «esto autoriza consultar el calendario».
- Separar visualmente texto descriptivo, cifras parentizadas, referencias a nombres y delimitadores, sin colorear cada palabra española como si fuera una palabra clave de programación.
- Ofrecer sugerencias de continuación en español cuando el usuario las solicite, no insertar pasos no pedidos.
- Presentar errores como: «No encuentro el conjunto de validación mencionado en esta frase», con la oración exacta y una corrección posible; no exponer solo códigos internos de error.
- Explicar diferencias entre advertencia y error. Una advertencia nunca debe fingir que el entrenamiento, la llamada a una herramienta o la compilación ya ocurrió.
- Conservar la forma natural en el editor, en la salida de consola y en la documentación generada.
- Ofrecer vista técnica opcional del árbol, IR, opcodes o código máquina para depuración, sin exigirla para escribir programas.
- Cuando se edite una llave, resaltar su pareja y avisar inmediatamente si falta el cierre.
- Proporcionar pruebas previas y una vista de resultados, pero distinguir claramente simulación, verificación y ejecución real.

## 7. Diagnósticos, seguridad y ambigüedad

Una gramática expresiva puede ocultar errores si el compilador intenta «adivinar» intenciones. Por eso:

- Cada frase aceptada debe corresponder a una regla gramatical documentada y a una operación semántica definida.
- Una frase no admitida debe producir un error localizado, no una conversión aproximada a otro comando.
- Si dos interpretaciones son posibles, Ariño debe pedir que se aclare el texto o mostrar las lecturas antes de compilar.
- El texto aportado como datos, ejemplos, contexto o contenido citado no puede cambiar la gramática ni autorizar herramientas por sí mismo.
- Las herramientas del agente deben declararse y limitarse por capacidades. El texto «puede enviar» no debe superar las autorizaciones del entorno.
- La entrada numérica debe validarse antes de compilar; rangos, precisión y desbordamientos deben ser explícitos.
- El compilador debe rechazar llaves desequilibradas, referencias sin resolver y operaciones no soportadas, sin emitir artefactos engañosos.

## 8. Pruebas de aceptación antes de ampliar la gramática

Cada etapa debe traer pruebas ejecutables, no solo ejemplos en documentación.

1. El párrafo de predicción citado en este documento se reconoce de manera determinista y produce los resultados especificados para entradas de referencia.
2. Una frase sobre entrenamiento entra en el analizador de entrenamiento; una frase sobre un agente entra en el analizador de agentes. Variar el cuerpo no debe hacer que los contextos se intercambien.
3. Los parámetros numéricos parentizados conservan valor, precisión, unidad y asociación con el nombre del parámetro.
4. Las llaves correctas se aceptan; las llaves sin pareja o mal anidadas se rechazan con línea y columna.
5. Los textos citados como contenido no se interpretan como órdenes del programa.
6. Los nombres inexistentes, dimensiones incompatibles, datos incompletos y herramientas no autorizadas producen errores en español.
7. Casos ambiguos se rechazan o solicitan aclaración; no se elige una lectura silenciosamente.
8. Pruebas diferenciales comparan la interpretación semántica con resultados de referencia y, cuando exista una ruta previa equivalente, con sus artefactos.
9. Las pruebas de la cadena nativa comprueban ejecución real y reconstrucción reproducible; documentación y empaquetado por sí solos no cuentan como capacidad implementada.
10. La batería debe incluir casos avanzados relacionados con modelos, tensores, formas, parámetros, contexto y topología, además de casos inválidos.

## 9. Plan de implementación futuro

**Etapa A — Contrato de sintaxis.** Cerrar ejemplos canónicos de prosa, reglas numéricas, delimitadores, ambigüedad y mensajes de error. Ninguna gramática se considera definitiva antes de pruebas con usuarios.

**Etapa B — Analizador de prosa para una operación acotada.** Elegir una tarea completa —por ejemplo, el cálculo de predicción del ejemplo de referencia— y aceptar solo las construcciones documentadas. Validar la interpretación y el resultado extremo a extremo.

**Etapa C — Separación de contextos.** Añadir analizadores semánticos y representaciones distintas para entrenamiento de modelos y creación de agentes. No anunciar entrenamiento real hasta que el runtime actualice parámetros y lo demuestre con pruebas.

**Etapa D — Tipos, datos y diagnósticos.** Ampliar las formas admitidas, los valores numéricos, referencias, listas y errores localizados, preservando el significado de las etapas anteriores.

**Etapa E — Compilador nativo integrado.** Incorporar la gramática conceptual española al frontend general de Ariño y conectarla al AST/representación semántica, IR, optimizador, backend y runtime oficiales. Todos esos componentes de producción deben estar implementados en código máquina nativo conforme al contrato del proyecto; la fuente en español no debe depender de un transpilador externo. La integración debe ser el camino oficial de compilación, no un traductor o puente lateral.

**Etapa F — Binario nativo y bootstrap reproducible.** Generar directamente binarios nativos para cada destino soportado y cumplir los gates de reconstrucción, hashes, pruebas y comparación B1/B2. Un binario de referencia, un bundle de IL o una ruta que solo interprete la prosa no sustituye la reconstrucción e integración del compilador y runtime nativos.

**Etapa G — Experiencia de autoría.** Después de estabilizar la gramática, añadir resaltado sensible al contexto, ayudas, explicación del análisis, navegación de errores y vista opcional de representaciones técnicas.

Las etapas pueden avanzar por incrementos revisables, pero ninguna puede declarar completado el objetivo global de Ariño por sí sola.

## 10. Estado actual y límites conocidos

La gramática descrita arriba es una dirección futura, no la sintaxis oficial ya soportada por Ariño. El parser experimental Stage42 usa una forma explícita de declaraciones de datos, modelo y capas, con perfiles acotados; no acepta aún el párrafo natural de referencia. Los puentes Stage43–47 convierten perfiles experimentales a IL v2 o paquetes de programas escalares y no se integran con `arino_ast_compiler --compile`. El flujo tensorial oficial continúa sin soporte completo y G0 permanece abierto.

Por tanto, este diseño no declara implementación de entrenamiento, soporte general de agentes, interfaz narrativa aceptada, generación nativa tensorial integrada ni cierre de G0. Cada capacidad debe cambiar de estado solo después de ser implementada, probada y conectada a la ruta general.

## 11. Alcance de esta entrega

Este archivo está pensado para una futura solicitud de extracción **solo de documentación**. No incluye cambios al compilador, runtime, backend, ejecutables, gramática aceptada, pruebas de código ni configuración de compilación. Una futura implementación debe recibir cambios separados, con alcance, pruebas y criterios de aceptación propios.

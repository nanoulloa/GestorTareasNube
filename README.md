# Gestor de Tareas en la Nube

Proyecto final de Estructuras de Datos II, Universidad CENFOTEC. Caso 3.

Autor: Mariano Ulloa Jiménez

El sistema maneja proyectos y tareas con dependencias y prioridades. Un planificador revisa qué
tareas ya pueden ejecutarse, toma siempre la más urgente y la manda por una cola a varios
trabajadores que corren al mismo tiempo. Los resultados vuelven por otra cola.

Todas las estructuras están hechas a mano en C++17. No uso `std::map`, `std::set`,
`std::unordered_map`, `std::priority_queue`, `std::queue` ni `std::list`; solo `std::vector`
y arreglos para cosas simples.

## Estructuras

| Estructura | Qué guarda | Para qué |
|---|---|---|
| Árbol B+ de orden 4 | Todas las tareas, llave proyecto + tarea | Listar un proyecto o un rango siguiendo las hojas enlazadas |
| Árbol rojinegro | Solo las tareas listas, por prioridad | Sacar la más urgente, cancelar o cambiar prioridad en O(log n) |
| Hash extensible | Todas las tareas, por código | Encontrar una tarea por código en O(1) promedio |
| Grafo dirigido | Las dependencias | Rechazar ciclos con DFS y liberar tareas con Kahn |
| Colas enlazadas | Mensajes de tareas y resultados | Comunicar al planificador con los trabajadores |

## Cómo abrirlo en Visual Studio 2022

1. Instalar la carga de trabajo "Desarrollo para el escritorio con C++" (trae CMake).
2. Archivo > Abrir > Carpeta, y escoger esta carpeta. Visual Studio detecta el `CMakeLists.txt`.
3. Arriba, en el selector de destino, escoger `GestorTareas.exe` y presionar F5.
4. Para las pruebas: Prueba > Ejecutar todas las pruebas, o escoger `Pruebas.exe` como destino.

## Cómo compilar desde la terminal

```
cmake -S . -B build
cmake --build build
./build/GestorTareas
```

Pruebas:

```
ctest --test-dir build --output-on-failure
```

En el menú, la opción 11 corre una demostración con 3 proyectos y 15 tareas.

## Integración continua

Cada vez que se sube un cambio, GitHub Actions compila el proyecto en Linux y en Windows,
corre las pruebas y publica el ejecutable como artefacto. El flujo está en
`.github/workflows/ci.yml`.

## Carpetas

```
src/estructuras/   Árbol B+, rojinegro, hash extensible, grafo, cola enlazada y modelo
src/nube/          Cola de mensajes segura entre hilos y repositorio de datos
src/logica/        Planificador, trabajadores, simulador de fallos y escenario de la demo
src/consola/       Menú, demostración y vistas de cada estructura
tests/             Pruebas de cada estructura y del planificador
docs/              Informe técnico
```

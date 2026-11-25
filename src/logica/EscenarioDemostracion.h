#pragma once

#include <string>
#include <vector>

#include "Planificador.h"

// Datos de ejemplo de la demostración: tres proyectos y quince tareas con dependencias.
// El proyecto APP es el ejemplo "Lanzar app" del avance.
namespace escenario {

struct DatosTarea {
    const char* proyecto;
    const char* titulo;
    int prioridad;
    int duracionMs;
};

struct DatosDependencia {
    int tarea;
    int dependeDe;
};

struct DependenciaRechazada {
    int tarea;
    int dependeDe;
    const char* motivo;
};

inline const std::vector<Proyecto>& proyectos() {
    static const std::vector<Proyecto> lista = {{"APP", "Lanzar app"}, {"WEB", "Sitio web"}, {"DAT", "Migración de datos"}};
    return lista;
}

inline const std::vector<DatosTarea>& tareas() {
    static const std::vector<DatosTarea> lista = {
        {"APP", "Compilar el código", 8, 300},             // T-0001
        {"APP", "Correr las pruebas", 8, 400},             // T-0002
        {"APP", "Escribir la documentación", 2, 250},      // T-0003
        {"APP", "Publicar la app", 5, 200},                // T-0004
        {"WEB", "Diseñar las pantallas", 6, 300},          // T-0005
        {"WEB", "Programar el frontend", 7, 400},          // T-0006
        {"WEB", "Programar la API", 9, 400},               // T-0007
        {"WEB", "Integrar frontend y API", 6, 300},        // T-0008
        {"WEB", "Pruebas de carga", 4, 250},               // T-0009
        {"WEB", "Desplegar el sitio", 5, 200},             // T-0010
        {"DAT", "Respaldar la base vieja", 10, 300},       // T-0011
        {"DAT", "Escribir scripts de migración", 7, 300},  // T-0012
        {"DAT", "Migrar los datos", 6, 400},               // T-0013
        {"DAT", "Validar los datos", 5, 250},              // T-0014
        {"DAT", "Apagar el sistema viejo", 3, 200},        // T-0015
    };
    return lista;
}

// (tarea, depende de): la primera no empieza hasta que termine la segunda.
inline const std::vector<DatosDependencia>& dependencias() {
    static const std::vector<DatosDependencia> lista = {
        {2, 1}, {4, 2}, {4, 3}, {6, 5}, {8, 6}, {8, 7}, {9, 8}, {10, 9},
        {13, 11}, {13, 12}, {14, 13}, {15, 14}, {10, 14}, {15, 10},
    };
    return lista;
}

inline const std::vector<DependenciaRechazada>& dependenciasRechazadas() {
    static const std::vector<DependenciaRechazada> lista = {
        {1, 4, "ciclo corto: T-0004 ya espera a T-0001"},
        {11, 15, "ciclo largo: T-0015 espera a T-0014, que espera a T-0013, que espera a T-0011"},
        {2, 1, "dependencia repetida"},
    };
    return lista;
}

// Semilla elegida para que haya reintentos pero ninguna tarea falle del todo.
inline OpcionesEjecucion opciones() {
    OpcionesEjecucion o;
    o.trabajadores = 3;
    o.probabilidadFallo = 0.25;
    o.maximoReintentos = 2;
    o.semilla = 69227;
    return o;
}

inline void cargarProyectosYTareas(Planificador& planificador) {
    for (const Proyecto& p : proyectos()) planificador.crearProyecto(p.codigo, p.nombre);
    for (const DatosTarea& t : tareas()) planificador.crearTarea(t.proyecto, t.titulo, t.prioridad, t.duracionMs);
}

inline void cargarDependencias(Planificador& planificador) {
    for (const DatosDependencia& d : dependencias()) planificador.agregarDependencia(formatearCodigo(d.tarea), formatearCodigo(d.dependeDe));
}

}  // namespace escenario

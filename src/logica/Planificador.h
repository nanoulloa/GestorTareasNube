#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../estructuras/ArbolBPlus.h"
#include "../estructuras/ArbolRojoNegro.h"
#include "../estructuras/GrafoDependencias.h"
#include "../estructuras/HashExtensible.h"
#include "../estructuras/Modelo.h"
#include "../nube/ColaNube.h"
#include "../nube/Repositorio.h"

struct OpcionesEjecucion {
    int trabajadores = 3;
    double probabilidadFallo = 0.2;  // entre 0 y 1
    int maximoReintentos = 2;
    unsigned semilla = 2026;         // con la misma semilla los fallos salen iguales
};

struct ResumenEjecucion {
    std::vector<std::string> ordenDespacho;     // cada vez que una tarea salió del rojinegro a la cola
    std::vector<std::string> ordenCompletadas;
    std::vector<std::string> fallidas;
    std::vector<std::string> quedaronBloqueadas;
    int intentosFallidos = 0;
    int reintentos = 0;
    long long duracionMs = 0;
};

// Decide si un intento falla. Solo depende de la tarea, el intento y la semilla,
// así sale igual sin importar qué trabajador la tome. No guarda estado: es seguro entre hilos.
class SimuladorFallos {
public:
    SimuladorFallos(double probabilidad, unsigned semilla);
    bool debeFallar(const std::string& codigo, int intento) const;

private:
    double probabilidad_;
    unsigned semilla_;
};

// Clase central: tiene las cuatro estructuras y coordina el flujo. El B+ guarda todas las
// tareas por proyecto, el hash las encuentra por código, el grafo lleva las dependencias
// y el rojinegro ordena las que ya pueden correr. Solo esta clase cambia el estado de
// las tareas; los trabajadores se comunican por las colas.
class Planificador {
public:
    static constexpr int ORDEN_BPLUS = 4;

    Planificador();
    Planificador(std::unique_ptr<IColaMensajes<MensajeTarea>> colaTareas,
                 std::unique_ptr<IColaMensajes<MensajeResultado>> colaResultados,
                 std::unique_ptr<IRepositorio> repositorio);

    // Bitácora de lo que pasa en cada estructura; la consola la imprime.
    void alRegistrar(std::function<void(const std::string&)> funcion) { bitacora_ = std::move(funcion); }

    Resultado crearProyecto(const std::string& codigo, const std::string& nombre);
    const Proyecto* buscarProyecto(const std::string& codigo) const;
    Resultado crearTarea(const std::string& codigoProyecto, const std::string& titulo, int prioridad, int duracionMs = 300);
    Tarea* buscarTarea(const std::string& codigo) const;
    Resultado agregarDependencia(const std::string& codigoTarea, const std::string& codigoDependeDe);
    Resultado cancelarTarea(const std::string& codigo);
    Resultado cambiarPrioridad(const std::string& codigo, int nuevaPrioridad);

    std::vector<Tarea*> listarRango(const LlaveTarea& desde, const LlaveTarea& hasta, int* hojasVisitadas = nullptr) const;
    std::vector<Tarea*> todasLasTareas() const;
    std::vector<Tarea*> tareasListasPorUrgencia() const;
    std::vector<std::string> ordenDeEjecucion() { return grafo_.ordenTopologico(); }

    ResumenEjecucion ejecutar(const OpcionesEjecucion& opciones);

    const ArbolBPlus<LlaveTarea, Tarea*>& arbolTareas() const { return arbolTareas_; }
    const ArbolRojoNegro<LlavePrioridad, Tarea*>& tareasListas() const { return tareasListas_; }
    const HashExtensible<Tarea*>& indiceCodigos() const { return indiceCodigos_; }
    GrafoDependencias& grafo() { return grafo_; }
    IColaMensajes<MensajeTarea>& colaTareas() { return *colaTareas_; }
    IColaMensajes<MensajeResultado>& colaResultados() { return *colaResultados_; }
    IRepositorio& repositorio() { return *repositorio_; }
    const std::vector<Proyecto>& proyectos() const { return proyectos_; }
    int cantidadTareas() const { return indiceCodigos_.cantidad(); }

private:
    ArbolBPlus<LlaveTarea, Tarea*> arbolTareas_{ORDEN_BPLUS};
    ArbolRojoNegro<LlavePrioridad, Tarea*> tareasListas_;
    HashExtensible<Tarea*> indiceCodigos_;
    GrafoDependencias grafo_;
    std::vector<Proyecto> proyectos_;
    std::vector<std::unique_ptr<Tarea>> almacen_;  // dueño de la memoria de las tareas
    std::vector<Tarea*> enVuelo_;                  // publicadas en la cola o ejecutándose
    std::unique_ptr<IColaMensajes<MensajeTarea>> colaTareas_;
    std::unique_ptr<IColaMensajes<MensajeResultado>> colaResultados_;
    std::unique_ptr<IRepositorio> repositorio_;
    std::function<void(const std::string&)> bitacora_;
    int siguienteNumero_ = 1;
    long long siguienteSecuencia_ = 1;

    void registrar(const std::string& texto) const;
    void pasarALista(Tarea* tarea);
    void liberarTareas(const std::vector<std::string>& codigos);
    void despachar(Tarea* tarea, ResumenEjecucion& resumen);
    void procesarResultado(const MensajeResultado& mensaje, ResumenEjecucion& resumen, const OpcionesEjecucion& opciones);
};

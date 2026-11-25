#pragma once

#include <string>
#include <vector>

#include "HashExtensible.h"

// Vértice del grafo con su lista de adyacencia.
// Una arista A -> B significa que B no puede empezar hasta que A termine.
struct NodoGrafo {
    std::string codigo;
    std::vector<NodoGrafo*> dependientes;   // lista de adyacencia: las que esperan a esta
    std::vector<NodoGrafo*> dependencias;   // las que esta espera
    int pendientes = 0;                     // contador de Kahn: dependencias sin completar
    bool completada = false;
    int marcaVisita = 0;                    // marcas para los recorridos
    int colorDfs = 0;
    int gradoTemporal = 0;
};

enum class ResultadoDependencia { Agregada, TareaNoExiste, MismaTarea, Repetida, FormaCiclo };

// Grafo dirigido de dependencias. Para encontrar un vértice por código usa su propio hash extensible.
class GrafoDependencias {
public:
    GrafoDependencias() = default;
    ~GrafoDependencias();
    GrafoDependencias(const GrafoDependencias&) = delete;
    GrafoDependencias& operator=(const GrafoDependencias&) = delete;

    bool agregarTarea(const std::string& codigo);
    bool contiene(const std::string& codigo) const { return indice_.contiene(codigo); }
    NodoGrafo* obtener(const std::string& codigo) const;
    int pendientes(const std::string& codigo) const;
    int cantidadVertices() const { return static_cast<int>(vertices_.size()); }
    int cantidadAristas() const { return aristas_; }
    const std::vector<NodoGrafo*>& vertices() const { return vertices_; }

    // "tarea" depende de "dependeDe": agrega la arista dependeDe -> tarea si no forma un ciclo.
    ResultadoDependencia agregarDependencia(const std::string& tarea, const std::string& dependeDe);

    // DFS de tres colores sobre todo el grafo.
    bool tieneCiclo();

    // Paso de Kahn en vivo: devuelve las dependientes que quedaron sin pendientes.
    std::vector<std::string> completarTarea(const std::string& codigo);

    // Quita la tarea y sus aristas; devuelve las dependientes que quedaron libres.
    bool eliminarTarea(const std::string& codigo, std::vector<std::string>& liberadas);

    // Orden de ejecución completo con el algoritmo de Kahn.
    std::vector<std::string> ordenTopologico();

private:
    HashExtensible<NodoGrafo*> indice_;
    std::vector<NodoGrafo*> vertices_;
    int aristas_ = 0;
    int visitaActual_ = 0;

    bool existeCamino(NodoGrafo* desde, NodoGrafo* hasta);
    bool visitarBuscando(NodoGrafo* nodo, NodoGrafo* objetivo);
    static bool visitarConColores(NodoGrafo* nodo);
};

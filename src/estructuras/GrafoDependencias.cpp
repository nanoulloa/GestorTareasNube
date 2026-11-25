#include "GrafoDependencias.h"

#include <algorithm>

#include "ColaEnlazada.h"

namespace {
const int BLANCO = 0;
const int GRIS = 1;
const int NEGRO = 2;

void quitarDe(std::vector<NodoGrafo*>& lista, NodoGrafo* nodo) {
    lista.erase(std::remove(lista.begin(), lista.end(), nodo), lista.end());
}
}  // namespace

GrafoDependencias::~GrafoDependencias() {
    for (NodoGrafo* nodo : vertices_) delete nodo;
}

bool GrafoDependencias::agregarTarea(const std::string& codigo) {
    if (indice_.contiene(codigo)) return false;
    NodoGrafo* nodo = new NodoGrafo();
    nodo->codigo = codigo;
    indice_.insertar(codigo, nodo);
    vertices_.push_back(nodo);
    return true;
}

NodoGrafo* GrafoDependencias::obtener(const std::string& codigo) const {
    NodoGrafo* nodo = nullptr;
    return indice_.buscar(codigo, nodo) ? nodo : nullptr;
}

int GrafoDependencias::pendientes(const std::string& codigo) const {
    NodoGrafo* nodo = obtener(codigo);
    return nodo == nullptr ? 0 : nodo->pendientes;
}

ResultadoDependencia GrafoDependencias::agregarDependencia(const std::string& codigoTarea, const std::string& codigoDependeDe) {
    NodoGrafo* tarea = obtener(codigoTarea);
    NodoGrafo* dependeDe = obtener(codigoDependeDe);
    if (tarea == nullptr || dependeDe == nullptr) return ResultadoDependencia::TareaNoExiste;
    if (tarea == dependeDe) return ResultadoDependencia::MismaTarea;
    for (NodoGrafo* n : dependeDe->dependientes) {
        if (n == tarea) return ResultadoDependencia::Repetida;
    }
    // Si desde la tarea ya se llega a dependeDe, la nueva arista cerraría un ciclo.
    if (existeCamino(tarea, dependeDe)) return ResultadoDependencia::FormaCiclo;

    dependeDe->dependientes.push_back(tarea);
    tarea->dependencias.push_back(dependeDe);
    if (!dependeDe->completada) tarea->pendientes++;
    aristas_++;
    return ResultadoDependencia::Agregada;
}

bool GrafoDependencias::existeCamino(NodoGrafo* desde, NodoGrafo* hasta) {
    visitaActual_++;
    return visitarBuscando(desde, hasta);
}

// DFS que se detiene apenas encuentra el objetivo.
bool GrafoDependencias::visitarBuscando(NodoGrafo* nodo, NodoGrafo* objetivo) {
    if (nodo == objetivo) return true;
    nodo->marcaVisita = visitaActual_;
    for (NodoGrafo* siguiente : nodo->dependientes) {
        if (siguiente->marcaVisita != visitaActual_ && visitarBuscando(siguiente, objetivo)) return true;
    }
    return false;
}

// Si se llega a un nodo gris (todavía en la pila de llamadas) hay una arista de regreso: ciclo.
bool GrafoDependencias::tieneCiclo() {
    for (NodoGrafo* nodo : vertices_) nodo->colorDfs = BLANCO;
    for (NodoGrafo* nodo : vertices_) {
        if (nodo->colorDfs == BLANCO && visitarConColores(nodo)) return true;
    }
    return false;
}

bool GrafoDependencias::visitarConColores(NodoGrafo* nodo) {
    nodo->colorDfs = GRIS;
    for (NodoGrafo* siguiente : nodo->dependientes) {
        if (siguiente->colorDfs == GRIS) return true;
        if (siguiente->colorDfs == BLANCO && visitarConColores(siguiente)) return true;
    }
    nodo->colorDfs = NEGRO;
    return false;
}

std::vector<std::string> GrafoDependencias::completarTarea(const std::string& codigo) {
    std::vector<std::string> liberadas;
    NodoGrafo* nodo = obtener(codigo);
    if (nodo == nullptr || nodo->completada) return liberadas;
    nodo->completada = true;
    for (NodoGrafo* dependiente : nodo->dependientes) {
        dependiente->pendientes--;
        if (dependiente->pendientes == 0) liberadas.push_back(dependiente->codigo);
    }
    return liberadas;
}

bool GrafoDependencias::eliminarTarea(const std::string& codigo, std::vector<std::string>& liberadas) {
    liberadas.clear();
    NodoGrafo* nodo = obtener(codigo);
    if (nodo == nullptr) return false;
    for (NodoGrafo* dependiente : nodo->dependientes) {
        quitarDe(dependiente->dependencias, nodo);
        aristas_--;
        if (!nodo->completada) {
            dependiente->pendientes--;
            if (dependiente->pendientes == 0) liberadas.push_back(dependiente->codigo);
        }
    }
    for (NodoGrafo* dependencia : nodo->dependencias) {
        quitarDe(dependencia->dependientes, nodo);
        aristas_--;
    }
    quitarDe(vertices_, nodo);
    indice_.eliminar(codigo);
    delete nodo;
    return true;
}

// Se empieza por las tareas sin dependencias y, al sacar cada una, se le baja el grado
// a sus dependientes. Si no salen todas es porque hay un ciclo.
std::vector<std::string> GrafoDependencias::ordenTopologico() {
    std::vector<std::string> orden;
    ColaEnlazada<NodoGrafo*> listas;
    for (NodoGrafo* nodo : vertices_) {
        nodo->gradoTemporal = static_cast<int>(nodo->dependencias.size());
        if (nodo->gradoTemporal == 0) listas.encolar(nodo);
    }
    NodoGrafo* actual = nullptr;
    while (listas.desencolar(actual)) {
        orden.push_back(actual->codigo);
        for (NodoGrafo* dependiente : actual->dependientes) {
            dependiente->gradoTemporal--;
            if (dependiente->gradoTemporal == 0) listas.encolar(dependiente);
        }
    }
    return orden;
}

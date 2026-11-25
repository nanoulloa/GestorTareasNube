// Pruebas de cada estructura y del planificador. Uso un mini marco propio para no
// depender de librerías externas: cada prueba es una función y VERIFICAR anota los fallos.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../src/estructuras/ArbolBPlus.h"
#include "../src/estructuras/ArbolRojoNegro.h"
#include "../src/estructuras/ColaEnlazada.h"
#include "../src/estructuras/GrafoDependencias.h"
#include "../src/estructuras/HashExtensible.h"
#include "../src/logica/EscenarioDemostracion.h"
#include "../src/logica/Planificador.h"
#include "../src/nube/ColaNube.h"

namespace {

int fallosPrueba = 0;

#define VERIFICAR(condicion)                                                     \
    do {                                                                         \
        if (!(condicion)) {                                                      \
            std::printf("    falla en linea %d: %s\n", __LINE__, #condicion);    \
            fallosPrueba++;                                                      \
        }                                                                        \
    } while (0)

template <typename Arbol>
bool valido(const Arbol& arbol) {
    std::string error;
    bool ok = arbol.esValido(error);
    if (!ok) std::printf("    estructura invalida: %s\n", error.c_str());
    return ok;
}

std::vector<int> llavesBPlus(const ArbolBPlus<int, int>& arbol) {
    std::vector<int> llaves;
    for (const auto& par : arbol.recorrerTodo()) llaves.push_back(par.first);
    return llaves;
}

std::vector<int> llavesRN(const ArbolRojoNegro<int, int>& arbol) {
    std::vector<int> llaves;
    for (const auto& par : arbol.recorrerEnOrden()) llaves.push_back(par.first);
    return llaves;
}

// ===================== Árbol B+ =====================

void bplusVacio() {
    ArbolBPlus<int, int> arbol;
    int v = 0;
    VERIFICAR(arbol.vacio());
    VERIFICAR(arbol.altura() == 0);
    VERIFICAR(!arbol.buscar(5, v));
    VERIFICAR(!arbol.eliminar(5));
    VERIFICAR(arbol.recorrerRango(0, 100).empty());
    VERIFICAR(valido(arbol));
}

void bplusUnElemento() {
    ArbolBPlus<int, int> arbol;
    int v = 0;
    VERIFICAR(arbol.insertar(7, 70));
    VERIFICAR(arbol.buscar(7, v) && v == 70);
    VERIFICAR(!arbol.insertar(7, 99));  // repetida
    VERIFICAR(arbol.eliminar(7));
    VERIFICAR(arbol.vacio());
    VERIFICAR(valido(arbol));
}

void bplusDivisionesEnCascada() {
    ArbolBPlus<int, int> arbol(4);
    for (int i = 1; i <= 3; i++) arbol.insertar(i, i);
    VERIFICAR(arbol.altura() == 1);
    arbol.insertar(4, 4);
    VERIFICAR(arbol.altura() == 2);
    VERIFICAR(arbol.obtenerNiveles()[0][0]->claves[0] == 3);  // la primera llave de la hoja derecha sube
    for (int i = 5; i <= 40; i++) {
        arbol.insertar(i, i);
        VERIFICAR(valido(arbol));
    }
    VERIFICAR(arbol.altura() >= 3);
}

void bplusRedistribucionYFusion() {
    ArbolBPlus<int, int> conPrestamo(4);
    for (int i = 1; i <= 5; i++) conPrestamo.insertar(i, i);  // hojas [1,2] [3,4,5]
    conPrestamo.eliminar(1);
    VERIFICAR(conPrestamo.redistribuciones() == 1);
    VERIFICAR(conPrestamo.obtenerNiveles()[0][0]->claves[0] == 4);
    VERIFICAR(valido(conPrestamo));

    ArbolBPlus<int, int> conFusion(4);
    for (int i = 1; i <= 4; i++) conFusion.insertar(i, i);  // hojas [1,2] [3,4]
    conFusion.eliminar(1);
    VERIFICAR(conFusion.fusiones() == 1);
    VERIFICAR(conFusion.altura() == 1);  // la raíz bajó un nivel
    VERIFICAR(valido(conFusion));
}

void bplusBorrarLaRaizYVaciar() {
    ArbolBPlus<int, int> arbol(4);
    for (int i = 1; i <= 60; i++) arbol.insertar(i, i);
    int llaveRaiz = arbol.obtenerNiveles()[0][0]->claves[0];
    int v = 0;
    VERIFICAR(arbol.eliminar(llaveRaiz));
    VERIFICAR(!arbol.buscar(llaveRaiz, v));
    VERIFICAR(valido(arbol));
    std::mt19937 azar(11);
    std::vector<int> restantes;
    for (int i = 1; i <= 60; i++) {
        if (i != llaveRaiz) restantes.push_back(i);
    }
    std::shuffle(restantes.begin(), restantes.end(), azar);
    for (int llave : restantes) {
        VERIFICAR(arbol.eliminar(llave));
        VERIFICAR(valido(arbol));
    }
    VERIFICAR(arbol.vacio());
}

void bplusRangoConHojasEnlazadas() {
    ArbolBPlus<int, int> arbol(4);
    for (int i = 1; i <= 30; i++) arbol.insertar(i * 10, i);
    int hojas = 0;
    auto rango = arbol.recorrerRango(45, 120, &hojas);
    VERIFICAR(rango.size() == 8 && rango.front().first == 50 && rango.back().first == 120);
    VERIFICAR(hojas >= 2);
    VERIFICAR(arbol.recorrerRango(120, 45).empty());
}

void bplusLlaveCompuesta() {
    ArbolBPlus<LlaveTarea, int> arbol(4);
    const char* proyectos[] = {"WEB", "APP", "DAT"};
    for (int n = 1; n <= 30; n++) arbol.insertar({proyectos[n % 3], n}, n);
    auto deApp = arbol.recorrerRango(LlaveTarea::inicioDe("APP"), LlaveTarea::finDe("APP"));
    VERIFICAR(deApp.size() == 10);
    for (const auto& par : deApp) VERIFICAR(par.first.proyecto == "APP");
}

void bplusAleatorioContraListaOrdenada() {
    for (int orden = 3; orden <= 5; orden++) {
        ArbolBPlus<int, int> arbol(orden);
        std::vector<int> referencia;
        std::mt19937 azar(orden);
        for (int paso = 0; paso < 4000; paso++) {
            int llave = static_cast<int>(azar() % 600);
            auto pos = std::lower_bound(referencia.begin(), referencia.end(), llave);
            bool existe = pos != referencia.end() && *pos == llave;
            if (azar() % 10 < 6) {
                VERIFICAR(arbol.insertar(llave, llave) == !existe);
                if (!existe) referencia.insert(pos, llave);
            } else {
                VERIFICAR(arbol.eliminar(llave) == existe);
                if (existe) referencia.erase(pos);
            }
            if (paso % 200 == 0) VERIFICAR(valido(arbol) && llavesBPlus(arbol) == referencia);
        }
        VERIFICAR(valido(arbol));
        VERIFICAR(llavesBPlus(arbol) == referencia);
    }
}

// ===================== Rojinegro =====================

void rnVacioYUnElemento() {
    ArbolRojoNegro<int, int> arbol;
    int k = 0, v = 0;
    VERIFICAR(!arbol.extraerMaximo(k, v));
    VERIFICAR(!arbol.eliminar(1));
    arbol.insertar(5, 50);
    VERIFICAR(!arbol.insertar(5, 51));
    VERIFICAR(arbol.extraerMaximo(k, v) && k == 5 && v == 50);
    VERIFICAR(arbol.vacio());
    VERIFICAR(valido(arbol));
}

void rnTresEnLineaRotan() {
    ArbolRojoNegro<int, int> arbol;
    arbol.insertar(1, 1);
    arbol.insertar(2, 2);
    arbol.insertar(3, 3);
    VERIFICAR(arbol.rotaciones() == 1);
    int raiz = -1;
    arbol.recorrerParaDibujar([&raiz](const NodoRN<int, int>* nodo, int profundidad) {
        if (profundidad == 0) raiz = nodo->llave;
    });
    VERIFICAR(raiz == 2);
    VERIFICAR(valido(arbol));
}

void rnBorrarLaRaizHastaVaciar() {
    ArbolRojoNegro<int, int> arbol;
    for (int i = 0; i < 64; i++) arbol.insertar(i, i);
    VERIFICAR(arbol.altura() <= 12);
    while (!arbol.vacio()) {
        int raiz = -1;
        arbol.recorrerParaDibujar([&raiz](const NodoRN<int, int>* nodo, int profundidad) {
            if (profundidad == 0) raiz = nodo->llave;
        });
        VERIFICAR(arbol.eliminar(raiz));
        VERIFICAR(valido(arbol));
    }
}

void rnMismaPrioridadSaleLaMasVieja() {
    ArbolRojoNegro<LlavePrioridad, std::string> arbol;
    arbol.insertar({5, 3}, "T-0003");
    arbol.insertar({5, 1}, "T-0001");
    arbol.insertar({9, 7}, "T-0007");
    arbol.insertar({5, 2}, "T-0002");
    std::vector<std::string> orden;
    LlavePrioridad llave;
    std::string codigo;
    while (arbol.extraerMaximo(llave, codigo)) orden.push_back(codigo);
    VERIFICAR((orden == std::vector<std::string>{"T-0007", "T-0001", "T-0002", "T-0003"}));
}

void rnAleatorioContraListaOrdenada() {
    ArbolRojoNegro<int, int> arbol;
    std::vector<int> referencia;
    std::mt19937 azar(3);
    for (int paso = 0; paso < 5000; paso++) {
        int llave = static_cast<int>(azar() % 800);
        auto pos = std::lower_bound(referencia.begin(), referencia.end(), llave);
        bool existe = pos != referencia.end() && *pos == llave;
        int accion = static_cast<int>(azar() % 20);
        if (accion < 11) {
            VERIFICAR(arbol.insertar(llave, llave) == !existe);
            if (!existe) referencia.insert(pos, llave);
        } else if (accion < 17) {
            VERIFICAR(arbol.eliminar(llave) == existe);
            if (existe) referencia.erase(pos);
        } else {
            int k = 0, v = 0;
            bool hay = arbol.extraerMaximo(k, v);
            VERIFICAR(hay == !referencia.empty());
            if (hay) {
                VERIFICAR(k == referencia.back());
                referencia.pop_back();
            }
        }
        if (paso % 200 == 0) VERIFICAR(valido(arbol) && llavesRN(arbol) == referencia);
    }
    VERIFICAR(llavesRN(arbol) == referencia);
}

// ===================== Hash extensible =====================

uint32_t hashNumerico(const std::string& clave) { return static_cast<uint32_t>(std::stoul(clave)); }

void hashVacioYUnElemento() {
    HashExtensible<int> hash;
    int v = 0;
    VERIFICAR(hash.profundidadGlobal() == 0 && hash.tamanoDirectorio() == 1);
    VERIFICAR(!hash.buscar("T-0001", v));
    VERIFICAR(!hash.eliminar("T-0001"));
    VERIFICAR(hash.insertar("T-0001", 1));
    VERIFICAR(!hash.insertar("T-0001", 2));
    VERIFICAR(hash.buscar("T-0001", v) && v == 1);
    VERIFICAR(hash.eliminar("T-0001"));
    VERIFICAR(valido(hash));
}

void hashDivisionYDuplicacion() {
    HashExtensible<int> hash(4, hashNumerico);
    for (int i = 0; i <= 3; i++) hash.insertar(std::to_string(i), i);
    VERIFICAR(hash.profundidadGlobal() == 0);
    hash.insertar("4", 4);
    VERIFICAR(hash.profundidadGlobal() == 1 && hash.duplicaciones() == 1 && hash.divisiones() == 1);
    VERIFICAR(hash.cubetaEn(0)->cantidad() == 3 && hash.cubetaEn(1)->cantidad() == 2);
    VERIFICAR(valido(hash));
}

void hashDivisionesEnCascada() {
    HashExtensible<int> hash(4, hashNumerico);
    for (int clave : {0, 8, 16, 24, 32}) hash.insertar(std::to_string(clave), clave);
    VERIFICAR(hash.profundidadGlobal() == 4 && hash.tamanoDirectorio() == 16);
    VERIFICAR(valido(hash));
}

void hashMismoHashNoDivideSinFin() {
    HashExtensible<int> hash(4, [](const std::string&) { return 7u; });
    for (int i = 0; i < 10; i++) VERIFICAR(hash.insertar("K" + std::to_string(i), i));
    VERIFICAR(hash.cantidad() == 10 && hash.profundidadGlobal() == 0);
    VERIFICAR(valido(hash));
}

void hashEliminarFusionaYReduce() {
    HashExtensible<int> hash(4, hashNumerico);
    for (int i = 0; i <= 4; i++) hash.insertar(std::to_string(i), i);
    hash.eliminar("1");
    VERIFICAR(hash.fusiones() == 1 && hash.reducciones() == 1 && hash.profundidadGlobal() == 0);
    VERIFICAR(valido(hash));
}

void hashAleatorioContraReferencia() {
    HashExtensible<int> hash;
    std::vector<std::string> referencia;
    std::mt19937 azar(2);
    for (int paso = 0; paso < 4000; paso++) {
        std::string clave = formatearCodigo(1 + static_cast<int>(azar() % 700));
        auto pos = std::lower_bound(referencia.begin(), referencia.end(), clave);
        bool existe = pos != referencia.end() && *pos == clave;
        if (azar() % 10 < 6) {
            VERIFICAR(hash.insertar(clave, 1) == !existe);
            if (!existe) referencia.insert(pos, clave);
        } else {
            VERIFICAR(hash.eliminar(clave) == existe);
            if (existe) referencia.erase(pos);
        }
        if (paso % 200 == 0) VERIFICAR(valido(hash));
    }
    auto claves = hash.claves();
    std::sort(claves.begin(), claves.end());
    VERIFICAR(claves == referencia);
}

// ===================== Grafo =====================

void grafoVacioYCasosBorde() {
    GrafoDependencias grafo;
    VERIFICAR(grafo.ordenTopologico().empty());
    VERIFICAR(!grafo.tieneCiclo());
    VERIFICAR(grafo.agregarDependencia("A", "B") == ResultadoDependencia::TareaNoExiste);
    grafo.agregarTarea("A");
    grafo.agregarTarea("B");
    VERIFICAR(!grafo.agregarTarea("A"));
    VERIFICAR(grafo.agregarDependencia("A", "A") == ResultadoDependencia::MismaTarea);
    VERIFICAR(grafo.agregarDependencia("B", "A") == ResultadoDependencia::Agregada);
    VERIFICAR(grafo.agregarDependencia("B", "A") == ResultadoDependencia::Repetida);
    VERIFICAR(grafo.pendientes("B") == 1);
}

void grafoRechazaCiclos() {
    GrafoDependencias grafo;
    for (const char* c : {"A", "B", "C", "D", "E"}) grafo.agregarTarea(c);
    grafo.agregarDependencia("B", "A");
    grafo.agregarDependencia("C", "B");
    grafo.agregarDependencia("D", "C");
    grafo.agregarDependencia("E", "D");
    VERIFICAR(grafo.agregarDependencia("A", "B") == ResultadoDependencia::FormaCiclo);  // ciclo corto
    VERIFICAR(grafo.agregarDependencia("A", "E") == ResultadoDependencia::FormaCiclo);  // ciclo largo
    VERIFICAR(grafo.agregarDependencia("E", "A") == ResultadoDependencia::Agregada);
    VERIFICAR(!grafo.tieneCiclo());
}

void grafoEjemploLanzarApp() {
    GrafoDependencias grafo;
    for (const char* c : {"T1", "T2", "T3", "T4"}) grafo.agregarTarea(c);
    grafo.agregarDependencia("T2", "T1");
    grafo.agregarDependencia("T4", "T2");
    grafo.agregarDependencia("T4", "T3");
    VERIFICAR(grafo.pendientes("T4") == 2);
    VERIFICAR(grafo.agregarDependencia("T1", "T4") == ResultadoDependencia::FormaCiclo);
    VERIFICAR((grafo.completarTarea("T1") == std::vector<std::string>{"T2"}));
    VERIFICAR(grafo.completarTarea("T3").empty());
    VERIFICAR((grafo.completarTarea("T2") == std::vector<std::string>{"T4"}));
}

void grafoEliminarLiberaDependientes() {
    GrafoDependencias grafo;
    for (const char* c : {"A", "B", "C", "D"}) grafo.agregarTarea(c);
    grafo.agregarDependencia("B", "A");
    grafo.agregarDependencia("C", "B");
    grafo.agregarDependencia("D", "B");
    grafo.agregarDependencia("D", "C");
    std::vector<std::string> liberadas;
    VERIFICAR(grafo.eliminarTarea("B", liberadas));
    VERIFICAR((liberadas == std::vector<std::string>{"C"}));
    VERIFICAR(grafo.cantidadAristas() == 1 && grafo.pendientes("D") == 1);
}

void grafoAleatorioKahnValido() {
    GrafoDependencias grafo;
    const int total = 300;
    for (int i = 0; i < total; i++) grafo.agregarTarea("N" + std::to_string(i));
    std::mt19937 azar(5);
    int ciclos = 0;
    for (int k = 0; k < 1500; k++) {
        auto r = grafo.agregarDependencia("N" + std::to_string(azar() % total), "N" + std::to_string(azar() % total));
        if (r == ResultadoDependencia::FormaCiclo) ciclos++;
    }
    VERIFICAR(ciclos > 0);
    VERIFICAR(!grafo.tieneCiclo());
    auto orden = grafo.ordenTopologico();
    VERIFICAR(static_cast<int>(orden.size()) == total);
    std::vector<int> posicion(total);
    for (int i = 0; i < total; i++) posicion[std::stoi(orden[i].substr(1))] = i;
    for (const NodoGrafo* nodo : grafo.vertices()) {
        for (const NodoGrafo* dep : nodo->dependencias) {
            VERIFICAR(posicion[std::stoi(dep->codigo.substr(1))] < posicion[std::stoi(nodo->codigo.substr(1))]);
        }
    }
}

// ===================== Colas =====================

void colaEnlazadaFifo() {
    ColaEnlazada<int> cola;
    int v = 0;
    VERIFICAR(cola.vacia() && !cola.desencolar(v));
    for (int i = 1; i <= 5; i++) cola.encolar(i);
    VERIFICAR(cola.desencolar(v) && v == 1);
    cola.encolar(6);
    VERIFICAR((cola.aVector() == std::vector<int>{2, 3, 4, 5, 6}));
}

void colaNubeVariosHilosSinPerderMensajes() {
    ColaNube<int> cola("prueba");
    const int total = 20000;
    std::vector<int> recibidos(total, 0);
    std::mutex candado;
    std::vector<std::thread> consumidores;
    for (int c = 0; c < 4; c++) {
        consumidores.emplace_back([&] {
            int valor = 0;
            while (cola.consumir(valor)) {
                std::lock_guard<std::mutex> l(candado);
                recibidos[valor]++;
            }
        });
    }
    std::vector<std::thread> productores;
    for (int p = 0; p < 4; p++) {
        productores.emplace_back([&cola, p] {
            for (int i = 0; i < 5000; i++) cola.publicar(p * 5000 + i);
        });
    }
    for (auto& h : productores) h.join();
    while (cola.tamano() > 0) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    cola.cerrar();
    for (auto& h : consumidores) h.join();
    bool todosUnaVez = std::all_of(recibidos.begin(), recibidos.end(), [](int veces) { return veces == 1; });
    VERIFICAR(todosUnaVez);
    VERIFICAR(cola.consumidos() == total);
}

// ===================== Planificador =====================

void planificadorValidaciones() {
    Planificador p;
    VERIFICAR(!p.crearProyecto("A", "Corto").exito);
    VERIFICAR(!p.crearProyecto("AP P", "Espacio").exito);
    VERIFICAR(!p.crearProyecto("APP", "").exito);
    VERIFICAR(p.crearProyecto(" app ", "Lanzar app").exito);
    VERIFICAR(!p.crearProyecto("APP", "Repetido").exito);
    VERIFICAR(!p.crearTarea("XYZ", "Titulo", 5, 0).exito);
    VERIFICAR(!p.crearTarea("APP", "", 5, 0).exito);
    VERIFICAR(!p.crearTarea("APP", "Titulo", 0, 0).exito);
    VERIFICAR(!p.crearTarea("APP", "Titulo", 11, 0).exito);
    VERIFICAR(!p.crearTarea("APP", "Titulo", 5, -1).exito);
    Resultado r = p.crearTarea("APP", "Compilar", 8, 0);
    VERIFICAR(r.exito && r.tarea->codigo == "T-0001" && r.tarea->estado == EstadoTarea::Lista);
    VERIFICAR(p.buscarTarea("1") == r.tarea && p.buscarTarea("t-1") == r.tarea && p.buscarTarea("abc") == nullptr);
    VERIFICAR(p.repositorio().cantidadTareas() == 1);
}

void planificadorDependenciasCancelarYPrioridad() {
    Planificador p;
    p.crearProyecto("APP", "Lanzar app");
    Tarea* a = p.crearTarea("APP", "A", 5, 0).tarea;
    Tarea* b = p.crearTarea("APP", "B", 2, 0).tarea;
    Tarea* c = p.crearTarea("APP", "C", 5, 0).tarea;
    VERIFICAR(p.agregarDependencia(b->codigo, a->codigo).exito);
    VERIFICAR(b->estado == EstadoTarea::Bloqueada && p.tareasListas().cantidad() == 2);
    VERIFICAR(!p.agregarDependencia(a->codigo, b->codigo).exito);  // ciclo
    VERIFICAR(!p.agregarDependencia(b->codigo, a->codigo).exito);  // repetida
    VERIFICAR(p.cambiarPrioridad(c->codigo, 10).exito);
    LlavePrioridad llave;
    Tarea* masUrgente = nullptr;
    VERIFICAR(p.tareasListas().verMaximo(llave, masUrgente) && masUrgente == c);
    VERIFICAR(!p.cambiarPrioridad(c->codigo, 11).exito);
    VERIFICAR(p.cancelarTarea(a->codigo).exito);
    VERIFICAR(p.buscarTarea("T-0001") == nullptr && b->estado == EstadoTarea::Lista);
    VERIFICAR(!p.cancelarTarea("T-0099").exito);
}

void planificadorRespetaPrioridadConUnTrabajador() {
    Planificador p;
    p.crearProyecto("APP", "Lanzar app");
    for (int prioridad : {3, 9, 5, 9, 1}) p.crearTarea("APP", "Tarea", prioridad, 0);
    OpcionesEjecucion op;
    op.trabajadores = 1;
    op.probabilidadFallo = 0;
    ResumenEjecucion r = p.ejecutar(op);
    VERIFICAR((r.ordenDespacho == std::vector<std::string>{"T-0002", "T-0004", "T-0003", "T-0001", "T-0005"}));
}

void planificadorReintentosLimitados() {
    Planificador p;
    p.crearProyecto("APP", "Lanzar app");
    Tarea* a = p.crearTarea("APP", "A", 5, 0).tarea;
    Tarea* b = p.crearTarea("APP", "B", 5, 0).tarea;
    p.agregarDependencia(b->codigo, a->codigo);
    OpcionesEjecucion op;
    op.trabajadores = 2;
    op.probabilidadFallo = 1.0;
    op.maximoReintentos = 2;
    ResumenEjecucion r = p.ejecutar(op);
    VERIFICAR(a->estado == EstadoTarea::Fallida && a->intentos == 3);
    VERIFICAR(b->estado == EstadoTarea::Bloqueada);
    VERIFICAR(r.reintentos == 2 && r.quedaronBloqueadas.size() == 1);
    VERIFICAR(p.colaTareas().tamano() == 0 && p.colaResultados().tamano() == 0);
}

void planificadorDemostracionCompleta() {
    Planificador p;
    escenario::cargarProyectosYTareas(p);
    escenario::cargarDependencias(p);
    for (const auto& d : escenario::dependenciasRechazadas()) {
        VERIFICAR(!p.agregarDependencia(formatearCodigo(d.tarea), formatearCodigo(d.dependeDe)).exito);
    }
    ResumenEjecucion r = p.ejecutar(escenario::opciones());
    VERIFICAR(r.ordenCompletadas.size() == 15 && r.fallidas.empty() && r.reintentos == 5);
    VERIFICAR(r.ordenDespacho[0] == "T-0011");
}

void planificadorCasoGrandeMilTareas() {
    Planificador p;
    p.crearProyecto("P1", "Uno");
    p.crearProyecto("P2", "Dos");
    p.crearProyecto("P3", "Tres");
    std::mt19937 azar(1000);
    const int total = 1000;
    for (int i = 1; i <= total; i++) p.crearTarea("P" + std::to_string(1 + azar() % 3), "Tarea", 1 + static_cast<int>(azar() % 10), 0);
    int aristas = 0;
    for (int i = 2; i <= total; i++) {
        int cuantas = static_cast<int>(azar() % 4);
        for (int k = 0; k < cuantas; k++) {
            int anterior = 1 + static_cast<int>(azar() % (i - 1));  // solo anteriores: nunca hay ciclos
            if (p.agregarDependencia(formatearCodigo(i), formatearCodigo(anterior)).exito) aristas++;
        }
    }
    OpcionesEjecucion op;
    op.trabajadores = 4;
    op.probabilidadFallo = 0.05;
    op.maximoReintentos = 3;
    auto inicio = std::chrono::steady_clock::now();
    ResumenEjecucion r = p.ejecutar(op);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - inicio).count();
    std::printf("    1000 tareas, %d dependencias, %lld ms\n", aristas, static_cast<long long>(ms));
    VERIFICAR(aristas > 1000);
    VERIFICAR(ms < 20000);
    VERIFICAR(static_cast<int>(r.ordenCompletadas.size() + r.fallidas.size() + r.quedaronBloqueadas.size()) == total);
    std::vector<int> posicion(total + 1, -1);
    for (size_t i = 0; i < r.ordenCompletadas.size(); i++) posicion[std::stoi(r.ordenCompletadas[i].substr(2))] = static_cast<int>(i);
    for (const NodoGrafo* nodo : p.grafo().vertices()) {
        int pos = posicion[std::stoi(nodo->codigo.substr(2))];
        if (pos < 0) continue;
        for (const NodoGrafo* dep : nodo->dependencias) {
            int posDep = posicion[std::stoi(dep->codigo.substr(2))];
            VERIFICAR(posDep >= 0 && posDep < pos);
        }
    }
    VERIFICAR(p.tareasListas().vacio());
    VERIFICAR(valido(p.arbolTareas()) && valido(p.indiceCodigos()));
}

struct Prueba {
    const char* nombre;
    void (*funcion)();
};

}  // namespace

int main() {
    const Prueba pruebas[] = {
        {"B+: arbol vacio", bplusVacio},
        {"B+: un solo elemento y llave repetida", bplusUnElemento},
        {"B+: divisiones en cascada", bplusDivisionesEnCascada},
        {"B+: redistribucion y fusion", bplusRedistribucionYFusion},
        {"B+: borrar la raiz y vaciar", bplusBorrarLaRaizYVaciar},
        {"B+: rango con hojas enlazadas", bplusRangoConHojasEnlazadas},
        {"B+: llave compuesta proyecto:tarea", bplusLlaveCompuesta},
        {"B+: aleatorio contra lista ordenada", bplusAleatorioContraListaOrdenada},
        {"Rojinegro: vacio y un elemento", rnVacioYUnElemento},
        {"Rojinegro: tres en linea rotan", rnTresEnLineaRotan},
        {"Rojinegro: borrar la raiz hasta vaciar", rnBorrarLaRaizHastaVaciar},
        {"Rojinegro: empate sale la mas vieja", rnMismaPrioridadSaleLaMasVieja},
        {"Rojinegro: aleatorio contra lista ordenada", rnAleatorioContraListaOrdenada},
        {"Hash: vacio y un elemento", hashVacioYUnElemento},
        {"Hash: division y duplicacion del directorio", hashDivisionYDuplicacion},
        {"Hash: divisiones en cascada", hashDivisionesEnCascada},
        {"Hash: mismo hash no divide sin fin", hashMismoHashNoDivideSinFin},
        {"Hash: eliminar fusiona y reduce", hashEliminarFusionaYReduce},
        {"Hash: aleatorio contra referencia", hashAleatorioContraReferencia},
        {"Grafo: vacio, misma tarea y repetida", grafoVacioYCasosBorde},
        {"Grafo: rechaza ciclos", grafoRechazaCiclos},
        {"Grafo: ejemplo Lanzar app", grafoEjemploLanzarApp},
        {"Grafo: eliminar libera dependientes", grafoEliminarLiberaDependientes},
        {"Grafo: aleatorio, Kahn da orden valido", grafoAleatorioKahnValido},
        {"Cola enlazada: FIFO", colaEnlazadaFifo},
        {"Cola nube: varios hilos sin perder mensajes", colaNubeVariosHilosSinPerderMensajes},
        {"Planificador: validaciones", planificadorValidaciones},
        {"Planificador: dependencias, cancelar y prioridad", planificadorDependenciasCancelarYPrioridad},
        {"Planificador: prioridad con un trabajador", planificadorRespetaPrioridadConUnTrabajador},
        {"Planificador: reintentos limitados", planificadorReintentosLimitados},
        {"Planificador: demostracion completa", planificadorDemostracionCompleta},
        {"Planificador: caso grande de 1000 tareas", planificadorCasoGrandeMilTareas},
    };

    int pasaron = 0;
    int total = 0;
    for (const Prueba& prueba : pruebas) {
        fallosPrueba = 0;
        prueba.funcion();
        total++;
        if (fallosPrueba == 0) pasaron++;
        std::printf("[%s] %s\n", fallosPrueba == 0 ? "OK" : "FALLA", prueba.nombre);
    }
    std::printf("\n%d de %d pruebas pasaron.\n", pasaron, total);
    return pasaron == total ? 0 : 1;
}

#include "Vista.h"

#include <algorithm>
#include <cstdio>
#include <iostream>

namespace vista {

namespace {
const int ANCHO_LINEA = 100;
const int MAXIMO_NODOS_POR_NIVEL = 40;
const int MAXIMO_LINEAS = 60;

// Cuenta caracteres visibles (las tildes en UTF-8 ocupan dos bytes).
int anchoVisible(const std::string& texto) {
    int ancho = 0;
    for (unsigned char c : texto) {
        if ((c & 0xC0) != 0x80) ancho++;
    }
    return ancho;
}

std::string aDerecha(const std::string& texto, int ancho) {
    int visible = anchoVisible(texto);
    return visible >= ancho ? texto : std::string(ancho - visible, ' ') + texto;
}

std::string listaCodigos(const std::vector<NodoGrafo*>& nodos) {
    if (nodos.empty()) return "-";
    std::vector<std::string> codigos;
    for (const NodoGrafo* n : nodos) codigos.push_back(n->codigo);
    return unir(codigos, ", ");
}

void imprimirEnFilas(const std::string& etiqueta, const std::vector<std::string>& textos, const std::string& separador) {
    std::string linea = etiqueta;
    std::string sangria(etiqueta.size(), ' ');
    int mostrados = std::min(static_cast<int>(textos.size()), MAXIMO_NODOS_POR_NIVEL);
    for (int i = 0; i < mostrados; i++) {
        std::string pieza = (i > 0 ? separador : " ") + textos[i];
        if (anchoVisible(linea) + anchoVisible(pieza) > ANCHO_LINEA && linea.size() > sangria.size()) {
            std::cout << linea << "\n";
            linea = sangria;
        }
        linea += pieza;
    }
    if (static_cast<int>(textos.size()) > mostrados) linea += " ... (" + std::to_string(textos.size() - mostrados) + " nodos más)";
    std::cout << linea << "\n";
}
}  // namespace

std::string unir(const std::vector<std::string>& textos, const std::string& separador) {
    std::string resultado;
    for (size_t i = 0; i < textos.size(); i++) {
        if (i > 0) resultado += separador;
        resultado += textos[i];
    }
    return resultado;
}

std::string ajustar(const std::string& texto, int ancho) {
    int visible = anchoVisible(texto);
    return visible >= ancho ? texto : texto + std::string(ancho - visible, ' ');
}

// Corta un texto largo sin partir una letra con tilde.
std::string recortarA(const std::string& texto, int largo) {
    if (anchoVisible(texto) <= largo) return texto;
    std::string resultado;
    int visibles = 0;
    for (size_t i = 0; i < texto.size(); i++) {
        unsigned char c = static_cast<unsigned char>(texto[i]);
        if ((c & 0xC0) != 0x80) {
            if (visibles == largo - 3) break;
            visibles++;
        }
        resultado += texto[i];
    }
    return resultado + "...";
}

void titulo(const std::string& texto) { std::cout << "\n=== " << texto << " ===\n"; }
void subtitulo(const std::string& texto) { std::cout << "-- " << texto << " --\n"; }
void exito(const std::string& texto) { std::cout << "OK: " << texto << "\n"; }
void error(const std::string& texto) { std::cout << "Error: " << texto << "\n"; }
void info(const std::string& texto) { std::cout << texto << "\n"; }

void mostrar(const Resultado& resultado) {
    if (resultado.exito) {
        exito(resultado.mensaje);
    } else {
        error(resultado.mensaje);
    }
}

// ===================== Tareas =====================

void tablaTareas(const std::vector<Tarea*>& tareas) {
    if (tareas.empty()) {
        info("(no hay tareas)");
        return;
    }
    std::cout << ajustar("Código", 8) << " " << ajustar("Proy.", 8) << " " << aDerecha("Prio", 4) << " " << ajustar("Estado", 12) << " "
              << aDerecha("Int.", 4) << "  Título\n";
    std::cout << std::string(75, '-') << "\n";
    for (const Tarea* t : tareas) {
        std::cout << ajustar(t->codigo, 8) << " " << ajustar(t->proyecto, 8) << " " << aDerecha(std::to_string(t->prioridad), 4) << " "
                  << ajustar(nombreEstado(t->estado), 12) << " " << aDerecha(std::to_string(t->intentos), 4) << "  "
                  << recortarA(t->titulo, 35) << "\n";
    }
    info("Total: " + std::to_string(tareas.size()));
}

void detalleTarea(Planificador& planificador, Tarea* tarea) {
    const auto& hash = planificador.indiceCodigos();
    int entrada = hash.indiceDe(tarea->codigo);
    NodoGrafo* nodo = planificador.grafo().obtener(tarea->codigo);
    char hexadecimal[16];
    std::snprintf(hexadecimal, sizeof hexadecimal, "0x%08X", hash.calcularHash(tarea->codigo));

    std::cout << "Código:        " << tarea->codigo << "\n";
    std::cout << "Título:        " << tarea->titulo << "\n";
    std::cout << "Proyecto:      " << tarea->proyecto << "\n";
    std::cout << "Prioridad:     " << tarea->prioridad << "\n";
    std::cout << "Estado:        " << nombreEstado(tarea->estado) << "\n";
    std::cout << "Intentos:      " << tarea->intentos << "\n";
    std::cout << "Duración:      " << tarea->duracionMs << " ms\n";
    if (nodo != nullptr) {
        std::cout << "Depende de:    " << listaCodigos(nodo->dependencias) << "\n";
        std::cout << "La esperan:    " << listaCodigos(nodo->dependientes) << "\n";
        std::cout << "Pendientes:    " << nodo->pendientes << "\n";
    }
    subtitulo("Dónde está en las estructuras");
    std::cout << "Hash:          FNV-1a = " << hexadecimal << ", últimos " << hash.profundidadGlobal() << " bits = "
              << hash.indiceEnBinario(entrada) << " -> cubeta C" << hash.cubetaEn(entrada)->id << "\n";
    std::cout << "Árbol B+:      llave " << tarea->llave().texto() << "\n";
    if (tarea->estado == EstadoTarea::Lista) {
        std::cout << "Rojinegro:     llave p" << tarea->prioridad << "#" << tarea->secuencia << " (está lista)\n";
    } else {
        std::cout << "Rojinegro:     no está (solo entran las tareas listas)\n";
    }
}

// ===================== Árbol B+ =====================

void arbolBPlus(Planificador& planificador) {
    const auto& arbol = planificador.arbolTareas();
    subtitulo("Árbol B+ de orden " + std::to_string(arbol.orden()) + " (todas las tareas, llave proyecto:tarea)");
    info("Llaves: " + std::to_string(arbol.cantidad()) + ", altura: " + std::to_string(arbol.altura()) + ", divisiones: " +
         std::to_string(arbol.divisiones()) + ", redistribuciones: " + std::to_string(arbol.redistribuciones()) + ", fusiones: " +
         std::to_string(arbol.fusiones()) + ".");
    if (arbol.vacio()) {
        info("(árbol vacío)");
        return;
    }
    auto niveles = arbol.obtenerNiveles();
    for (size_t n = 0; n < niveles.size(); n++) {
        bool sonHojas = n == niveles.size() - 1;
        std::string etiqueta = n == 0 ? "Raíz" : (sonHojas ? "Hojas" : "Nivel " + std::to_string(n + 1));
        std::vector<std::string> textos;
        for (const auto* nodo : niveles[n]) {
            std::string texto = "[";
            for (int i = 0; i < nodo->cantidad; i++) {
                if (i > 0) texto += nodo->esHoja ? " " : " | ";
                texto += nodo->claves[i].texto();
            }
            textos.push_back(texto + "]");
        }
        imprimirEnFilas(ajustar(etiqueta, 8), textos, sonHojas ? " -> " : "   ");
    }
    info("Las hojas están enlazadas (->): para listar un rango se baja una vez y se avanza por ellas.");
}

// ===================== Rojinegro =====================

void arbolRojoNegro(Planificador& planificador) {
    const auto& arbol = planificador.tareasListas();
    subtitulo("Árbol rojinegro (solo tareas listas, ordenadas por prioridad)");
    info("Nodos: " + std::to_string(arbol.cantidad()) + ", altura: " + std::to_string(arbol.altura()) + ", rotaciones hechas: " +
         std::to_string(arbol.rotaciones()) + ".");
    if (arbol.vacio()) {
        info("(no hay tareas listas)");
        return;
    }
    info("Dibujado acostado: la raíz a la izquierda y la derecha arriba. La primera línea es la más urgente.");
    int lineas = 0;
    arbol.recorrerParaDibujar([&lineas](const NodoRN<LlavePrioridad, Tarea*>* nodo, int profundidad) {
        lineas++;
        if (lineas > MAXIMO_LINEAS) return;
        bool rojo = nodo->color == ColorNodo::Rojo;
        std::cout << std::string(profundidad * 6, ' ') << nodo->valor->codigo << " p" << nodo->valor->prioridad << (rojo ? " (R)" : " (N)");
        if (profundidad == 0) std::cout << "  <- raíz";
        std::cout << "\n";
    });
    if (lineas > MAXIMO_LINEAS) info("... (" + std::to_string(lineas - MAXIMO_LINEAS) + " nodos más)");
}

// ===================== Hash extensible =====================

void hash(Planificador& planificador) {
    const auto& h = planificador.indiceCodigos();
    subtitulo("Hash extensible (índice por código de tarea)");
    info("Claves: " + std::to_string(h.cantidad()) + ", profundidad global: " + std::to_string(h.profundidadGlobal()) +
         ", entradas del directorio: " + std::to_string(h.tamanoDirectorio()) + ", cubetas: " + std::to_string(h.cantidadCubetas()) +
         " (capacidad " + std::to_string(h.capacidadCubeta()) + ").");
    info("Divisiones: " + std::to_string(h.divisiones()) + ", duplicaciones del directorio: " + std::to_string(h.duplicaciones()) +
         ", fusiones: " + std::to_string(h.fusiones()) + ", reducciones del directorio: " + std::to_string(h.reducciones()) + ".");
    std::cout << ajustar("Entrada", 10) << " " << ajustar("Cubeta", 7) << " " << aDerecha("Local", 5) << "  Claves\n";
    int mostrar = std::min(h.tamanoDirectorio(), MAXIMO_LINEAS);
    for (int i = 0; i < mostrar; i++) {
        const auto* cubeta = h.cubetaEn(i);
        std::string inicio = ajustar(h.indiceEnBinario(i), 10) + " " + ajustar("C" + std::to_string(cubeta->id), 7) + " " +
                             aDerecha(std::to_string(cubeta->profundidadLocal), 5) + "  ";
        if (!h.esEntradaPrincipal(i)) {
            int principal = i & ((1 << cubeta->profundidadLocal) - 1);
            std::cout << inicio << "(la misma cubeta que " << h.indiceEnBinario(principal) << ")\n";
            continue;
        }
        std::cout << inicio << (cubeta->claves.empty() ? "(vacía)" : unir(cubeta->claves, ", ")) << "\n";
    }
    if (h.tamanoDirectorio() > mostrar) info("... (" + std::to_string(h.tamanoDirectorio() - mostrar) + " entradas más)");
}

// ===================== Grafo =====================

void grafo(Planificador& planificador) {
    GrafoDependencias& g = planificador.grafo();
    subtitulo("Grafo de dependencias (lista de adyacencia)");
    info("Tareas: " + std::to_string(g.cantidadVertices()) + ", aristas: " + std::to_string(g.cantidadAristas()) +
         ". ¿Tiene ciclos según el DFS? " + (g.tieneCiclo() ? "Sí" : "No") + ".");
    if (g.cantidadVertices() == 0) return;
    std::cout << ajustar("Tarea", 8) << " " << aDerecha("Faltan", 6) << "  " << ajustar("Depende de", 28) << " La esperan (adyacencia)\n";
    int mostrar = std::min(g.cantidadVertices(), MAXIMO_LINEAS);
    for (int i = 0; i < mostrar; i++) {
        const NodoGrafo* nodo = g.vertices()[i];
        std::cout << ajustar(nodo->codigo, 8) << " " << aDerecha(std::to_string(nodo->pendientes), 6) << "  "
                  << ajustar(recortarA(listaCodigos(nodo->dependencias), 28), 28) << " " << listaCodigos(nodo->dependientes)
                  << (nodo->completada ? " (hecha)" : "") << "\n";
    }
    if (g.cantidadVertices() > mostrar) info("... (" + std::to_string(g.cantidadVertices() - mostrar) + " tareas más)");
    std::vector<std::string> orden = planificador.ordenDeEjecucion();
    if (static_cast<int>(orden.size()) > MAXIMO_LINEAS) {
        orden.resize(MAXIMO_LINEAS);
        orden.push_back("...");
    }
    info("Orden de ejecución (Kahn): " + unir(orden, " -> "));
}

// ===================== Colas =====================

void colas(Planificador& planificador) {
    subtitulo("Colas de mensajes");
    auto& tareas = planificador.colaTareas();
    std::vector<std::string> mensajesTareas;
    for (const auto& m : tareas.contenido()) mensajesTareas.push_back(m.codigo + "(intento " + std::to_string(m.intento) + ")");
    std::cout << "Cola '" << tareas.nombre() << "'. En cola ahora: " << tareas.tamano() << ", publicados: " << tareas.publicados()
              << ", consumidos: " << tareas.consumidos() << ".\n";
    std::cout << "  frente -> " << (mensajesTareas.empty() ? "(vacía)" : unir(mensajesTareas, " -> ")) << "\n";

    auto& resultados = planificador.colaResultados();
    std::cout << "Cola '" << resultados.nombre() << "'. En cola ahora: " << resultados.tamano() << ", publicados: "
              << resultados.publicados() << ", consumidos: " << resultados.consumidos() << ".\n";
    std::cout << "  frente -> " << (resultados.tamano() == 0 ? "(vacía)" : "...") << "\n";

    std::vector<std::string> listas;
    for (const Tarea* t : planificador.tareasListasPorUrgencia()) listas.push_back(t->codigo + " p" + std::to_string(t->prioridad));
    std::cout << "Esperando despacho en el rojinegro (de más a menos urgente): " << (listas.empty() ? "ninguna" : unir(listas, ", ")) << "\n";
    info("Las colas se llenan y vacían durante 'Ejecutar planificador'; ahí el planificador publica una tarea por cada trabajador libre.");
}

void resumen(const ResumenEjecucion& r) {
    std::cout << "\n";
    subtitulo("Resultado de la ejecución");
    std::vector<std::string> completadas = r.ordenCompletadas;
    if (static_cast<int>(completadas.size()) > MAXIMO_LINEAS) {
        completadas.resize(MAXIMO_LINEAS);
        completadas.push_back("...");
    }
    std::cout << "Completadas (" << r.ordenCompletadas.size() << "), en orden: "
              << (completadas.empty() ? "ninguna" : unir(completadas, " -> ")) << "\n";
    std::cout << "Intentos fallidos: " << r.intentosFallidos << ", de los cuales se reintentaron " << r.reintentos << ".\n";
    std::cout << "Fallidas sin más reintentos: " << (r.fallidas.empty() ? "ninguna" : unir(r.fallidas, ", ")) << "\n";
    std::cout << "Bloqueadas porque algo de lo que dependen falló: "
              << (r.quedaronBloqueadas.empty() ? "ninguna" : recortarA(unir(r.quedaronBloqueadas, ", "), 300)) << "\n";
    std::cout << "Duración: " << r.duracionMs << " ms\n";
}

void todas(Planificador& planificador) {
    arbolBPlus(planificador);
    std::cout << "\n";
    arbolRojoNegro(planificador);
    std::cout << "\n";
    hash(planificador);
    std::cout << "\n";
    grafo(planificador);
}

}  // namespace vista

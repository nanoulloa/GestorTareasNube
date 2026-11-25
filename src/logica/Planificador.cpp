#include "Planificador.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>

// ===================== Simulador de fallos =====================

SimuladorFallos::SimuladorFallos(double probabilidad, unsigned semilla)
    : probabilidad_(std::min(1.0, std::max(0.0, probabilidad))), semilla_(semilla) {}

bool SimuladorFallos::debeFallar(const std::string& codigo, int intento) const {
    if (probabilidad_ <= 0.0) return false;
    if (probabilidad_ >= 1.0) return true;
    uint32_t semillaIntento = hashFnv1a(codigo) * 31u + static_cast<uint32_t>(intento) * 7919u + semilla_;
    std::mt19937 generador(semillaIntento);
    double azar = static_cast<double>(generador()) / 4294967296.0;  // número entre 0 y 1
    return azar < probabilidad_;
}

// ===================== Trabajador =====================

namespace {

// Trabajador (worker): toma mensajes de la cola de tareas, simula la ejecución y publica el resultado.
// No toca las tareas ni las estructuras, solo las colas, por eso no hay condiciones de carrera.
void trabajar(std::string nombre, IColaMensajes<MensajeTarea>* colaTareas,
              IColaMensajes<MensajeResultado>* colaResultados, const SimuladorFallos* simulador) {
    MensajeTarea mensaje;
    while (colaTareas->consumir(mensaje)) {
        colaResultados->publicar({mensaje.codigo, TipoResultado::Iniciada, nombre, mensaje.intento});
        if (mensaje.duracionMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(mensaje.duracionMs));
        bool falla = simulador->debeFallar(mensaje.codigo, mensaje.intento);
        colaResultados->publicar({mensaje.codigo, falla ? TipoResultado::Fallida : TipoResultado::Completada, nombre, mensaje.intento});
    }
}

std::string unir(const std::vector<std::string>& textos, const std::string& separador) {
    std::string resultado;
    for (size_t i = 0; i < textos.size(); i++) {
        if (i > 0) resultado += separador;
        resultado += textos[i];
    }
    return resultado;
}

}  // namespace

// ===================== Planificador =====================

Planificador::Planificador()
    : Planificador(std::make_unique<ColaNube<MensajeTarea>>("tareas.listas"),
                   std::make_unique<ColaNube<MensajeResultado>>("tareas.resultados"),
                   std::make_unique<RepositorioMemoria>()) {}

Planificador::Planificador(std::unique_ptr<IColaMensajes<MensajeTarea>> colaTareas,
                           std::unique_ptr<IColaMensajes<MensajeResultado>> colaResultados,
                           std::unique_ptr<IRepositorio> repositorio)
    : colaTareas_(std::move(colaTareas)), colaResultados_(std::move(colaResultados)), repositorio_(std::move(repositorio)) {}

void Planificador::registrar(const std::string& texto) const {
    if (bitacora_) bitacora_(texto);
}

// ---------- Proyectos ----------

Resultado Planificador::crearProyecto(const std::string& codigo, const std::string& nombre) {
    std::string codigoLimpio = mayusculas(recortar(codigo));
    std::string nombreLimpio = recortar(nombre);
    if (!codigoProyectoValido(codigoLimpio)) {
        return Resultado::error("El código del proyecto debe tener de 2 a 8 letras o números, sin espacios.");
    }
    if (nombreLimpio.empty() || nombreLimpio.size() > LARGO_MAXIMO_NOMBRE) {
        return Resultado::error("El nombre debe tener entre 1 y 40 caracteres.");
    }
    if (buscarProyecto(codigoLimpio) != nullptr) {
        return Resultado::error("Ya existe un proyecto con el código " + codigoLimpio + ".");
    }
    proyectos_.push_back({codigoLimpio, nombreLimpio});
    repositorio_->guardarProyecto(proyectos_.back());
    registrar("[Proyecto] Se creó " + codigoLimpio + " (" + nombreLimpio + ").");
    return Resultado::ok("Proyecto " + codigoLimpio + " (" + nombreLimpio + ") creado.");
}

const Proyecto* Planificador::buscarProyecto(const std::string& codigo) const {
    std::string buscado = mayusculas(recortar(codigo));
    for (const Proyecto& p : proyectos_) {
        if (p.codigo == buscado) return &p;
    }
    return nullptr;
}

// ---------- Tareas ----------

Resultado Planificador::crearTarea(const std::string& codigoProyecto, const std::string& titulo, int prioridad, int duracionMs) {
    const Proyecto* proyecto = buscarProyecto(codigoProyecto);
    if (proyecto == nullptr) return Resultado::error("No existe el proyecto '" + codigoProyecto + "'.");
    std::string tituloLimpio = recortar(titulo);
    if (tituloLimpio.empty() || tituloLimpio.size() > LARGO_MAXIMO_TITULO) {
        return Resultado::error("El título debe tener entre 1 y 60 caracteres.");
    }
    if (prioridad < PRIORIDAD_MINIMA || prioridad > PRIORIDAD_MAXIMA) return Resultado::error("La prioridad va de 1 a 10.");
    if (duracionMs < 0 || duracionMs > DURACION_MAXIMA_MS) return Resultado::error("La duración va de 0 a 5000 ms.");

    auto nueva = std::make_unique<Tarea>();
    Tarea* tarea = nueva.get();
    tarea->numero = siguienteNumero_++;
    tarea->codigo = formatearCodigo(tarea->numero);
    tarea->proyecto = proyecto->codigo;
    tarea->titulo = tituloLimpio;
    tarea->prioridad = prioridad;
    tarea->duracionMs = duracionMs;
    tarea->secuencia = siguienteSecuencia_++;
    almacen_.push_back(std::move(nueva));

    indiceCodigos_.insertar(tarea->codigo, tarea);
    arbolTareas_.insertar(tarea->llave(), tarea);
    grafo_.agregarTarea(tarea->codigo);

    int entrada = indiceCodigos_.indiceDe(tarea->codigo);
    registrar("[Crear] " + tarea->codigo + " '" + tarea->titulo + "': hash -> entrada " + indiceCodigos_.indiceEnBinario(entrada) +
              " (cubeta C" + std::to_string(indiceCodigos_.cubetaEn(entrada)->id) + "); B+ -> llave " + tarea->llave().texto() +
              ", altura " + std::to_string(arbolTareas_.altura()) + ".");

    // Una tarea nueva no depende de nadie, así que ya puede ejecutarse.
    pasarALista(tarea);
    return Resultado::ok("Tarea " + tarea->codigo + " creada en " + proyecto->codigo + ".", tarea);
}

// Búsqueda O(1) promedio con el hash extensible.
Tarea* Planificador::buscarTarea(const std::string& codigo) const {
    int numero = 0;
    if (!leerNumeroTarea(codigo, numero)) return nullptr;
    Tarea* tarea = nullptr;
    return indiceCodigos_.buscar(formatearCodigo(numero), tarea) ? tarea : nullptr;
}

Resultado Planificador::agregarDependencia(const std::string& codigoTarea, const std::string& codigoDependeDe) {
    Tarea* tarea = buscarTarea(codigoTarea);
    if (tarea == nullptr) return Resultado::error("No existe la tarea '" + codigoTarea + "'.");
    Tarea* dependeDe = buscarTarea(codigoDependeDe);
    if (dependeDe == nullptr) return Resultado::error("No existe la tarea '" + codigoDependeDe + "'.");
    if (tarea->estado != EstadoTarea::Bloqueada && tarea->estado != EstadoTarea::Lista) {
        return Resultado::error(tarea->codigo + " está " + nombreEstado(tarea->estado) + "; solo se le agregan dependencias antes de ejecutarse.");
    }

    switch (grafo_.agregarDependencia(tarea->codigo, dependeDe->codigo)) {
        case ResultadoDependencia::MismaTarea:
            return Resultado::error("Una tarea no puede depender de sí misma.");
        case ResultadoDependencia::Repetida:
            return Resultado::error(tarea->codigo + " ya depende de " + dependeDe->codigo + ".");
        case ResultadoDependencia::FormaCiclo:
            registrar("[Grafo] El DFS encontró un camino de " + tarea->codigo + " a " + dependeDe->codigo +
                      ": la dependencia cerraría un ciclo.");
            return Resultado::error(dependeDe->codigo + " ya depende (directa o indirectamente) de " + tarea->codigo +
                                    "; esa dependencia formaría un ciclo.");
        case ResultadoDependencia::TareaNoExiste:
            return Resultado::error("Alguna de las tareas no está en el grafo.");
        case ResultadoDependencia::Agregada:
            break;
    }

    int pendientes = grafo_.pendientes(tarea->codigo);
    registrar("[Grafo] Arista " + dependeDe->codigo + " -> " + tarea->codigo + ". Pendientes de " + tarea->codigo + ": " +
              std::to_string(pendientes) + ".");
    if (tarea->estado == EstadoTarea::Lista && pendientes > 0) {
        tareasListas_.eliminar(tarea->llavePrioridad());
        tarea->estado = EstadoTarea::Bloqueada;
        repositorio_->guardar(*tarea);
        registrar("[Rojinegro] Sale " + tarea->codigo + ": ahora está Bloqueada.");
    }
    return Resultado::ok(tarea->codigo + " ahora depende de " + dependeDe->codigo + ".", tarea);
}

// Quita la tarea de todas las estructuras; las que dependían de ella ya no la esperan.
Resultado Planificador::cancelarTarea(const std::string& codigo) {
    Tarea* tarea = buscarTarea(codigo);
    if (tarea == nullptr) return Resultado::error("No existe la tarea '" + codigo + "'.");
    if (tarea->estado == EstadoTarea::EnCola || tarea->estado == EstadoTarea::EnEjecucion) {
        return Resultado::error(tarea->codigo + " ya la tiene un trabajador; no se puede cancelar.");
    }
    if (tarea->estado == EstadoTarea::Completada) {
        return Resultado::error(tarea->codigo + " ya se completó; no hay nada que cancelar.");
    }

    std::string codigoTarea = tarea->codigo;
    if (tarea->estado == EstadoTarea::Lista) tareasListas_.eliminar(tarea->llavePrioridad());
    arbolTareas_.eliminar(tarea->llave());
    indiceCodigos_.eliminar(codigoTarea);
    std::vector<std::string> liberadas;
    grafo_.eliminarTarea(codigoTarea, liberadas);
    repositorio_->eliminar(codigoTarea);
    registrar("[Cancelar] " + codigoTarea + " sale del rojinegro, del B+ (altura " + std::to_string(arbolTareas_.altura()) +
              "), del hash y del grafo.");
    almacen_.erase(std::remove_if(almacen_.begin(), almacen_.end(),
                                  [tarea](const std::unique_ptr<Tarea>& t) { return t.get() == tarea; }),
                   almacen_.end());

    liberarTareas(liberadas);
    std::string extra = liberadas.empty() ? "" : " Quedaron listas: " + unir(liberadas, ", ") + ".";
    return Resultado::ok("Tarea " + codigoTarea + " cancelada." + extra);
}

Resultado Planificador::cambiarPrioridad(const std::string& codigo, int nuevaPrioridad) {
    Tarea* tarea = buscarTarea(codigo);
    if (tarea == nullptr) return Resultado::error("No existe la tarea '" + codigo + "'.");
    if (nuevaPrioridad < PRIORIDAD_MINIMA || nuevaPrioridad > PRIORIDAD_MAXIMA) return Resultado::error("La prioridad va de 1 a 10.");
    if (tarea->estado != EstadoTarea::Bloqueada && tarea->estado != EstadoTarea::Lista) {
        return Resultado::error(tarea->codigo + " está " + nombreEstado(tarea->estado) + "; la prioridad solo se cambia antes de ejecutarse.");
    }
    int anterior = tarea->prioridad;
    if (tarea->estado == EstadoTarea::Lista) {
        // La prioridad es parte de la llave: se saca con la llave vieja y se mete con la nueva.
        tareasListas_.eliminar(tarea->llavePrioridad());
        tarea->prioridad = nuevaPrioridad;
        tareasListas_.insertar(tarea->llavePrioridad(), tarea);
        registrar("[Rojinegro] " + tarea->codigo + " se reubica: p" + std::to_string(anterior) + " -> p" + std::to_string(nuevaPrioridad) + ".");
    } else {
        tarea->prioridad = nuevaPrioridad;
    }
    repositorio_->guardar(*tarea);
    return Resultado::ok(tarea->codigo + ": prioridad " + std::to_string(anterior) + " -> " + std::to_string(nuevaPrioridad) + ".", tarea);
}

// ---------- Consultas ----------

std::vector<Tarea*> Planificador::listarRango(const LlaveTarea& desde, const LlaveTarea& hasta, int* hojasVisitadas) const {
    std::vector<Tarea*> tareas;
    for (const auto& par : arbolTareas_.recorrerRango(desde, hasta, hojasVisitadas)) tareas.push_back(par.second);
    return tareas;
}

// Todas las tareas en orden de proyecto, recorriendo las hojas enlazadas del B+.
std::vector<Tarea*> Planificador::todasLasTareas() const {
    std::vector<Tarea*> tareas;
    for (const auto& par : arbolTareas_.recorrerTodo()) tareas.push_back(par.second);
    return tareas;
}

std::vector<Tarea*> Planificador::tareasListasPorUrgencia() const {
    auto enOrden = tareasListas_.recorrerEnOrden();
    std::vector<Tarea*> resultado;
    for (auto it = enOrden.rbegin(); it != enOrden.rend(); ++it) resultado.push_back(it->second);
    return resultado;
}

// ---------- Ejecución ----------

// Arranca los trabajadores y reparte tareas hasta que no quede nada por correr. En cada
// vuelta llena los trabajadores libres con lo más urgente y espera un resultado.
ResumenEjecucion Planificador::ejecutar(const OpcionesEjecucion& opciones) {
    ResumenEjecucion resumen;
    int trabajadores = std::max(1, std::min(opciones.trabajadores, 16));
    SimuladorFallos simulador(opciones.probabilidadFallo, opciones.semilla);
    auto inicio = std::chrono::steady_clock::now();

    colaTareas_->abrir();
    std::vector<std::thread> hilos;
    for (int i = 0; i < trabajadores; i++) {
        hilos.emplace_back(trabajar, "W" + std::to_string(i + 1), colaTareas_.get(), colaResultados_.get(), &simulador);
    }
    registrar("[Planificador] Arranca. Trabajadores: " + std::to_string(trabajadores) + ", tareas listas: " +
              std::to_string(tareasListas_.cantidad()) + ", probabilidad de fallo: " +
              std::to_string(static_cast<int>(std::lround(opciones.probabilidadFallo * 100))) + "%, reintentos máximos: " +
              std::to_string(opciones.maximoReintentos) + ".");

    while (true) {
        // Solo se despacha si hay un trabajador libre: así el rojinegro decide en cada momento
        // y una tarea urgente recién liberada no queda detrás de otras.
        while (static_cast<int>(enVuelo_.size()) < trabajadores) {
            LlavePrioridad llave;
            Tarea* siguiente = nullptr;
            if (!tareasListas_.extraerMaximo(llave, siguiente)) break;
            despachar(siguiente, resumen);
        }
        if (enVuelo_.empty()) break;  // no hay nada corriendo ni nada listo

        MensajeResultado mensaje;
        if (!colaResultados_->consumir(mensaje)) break;
        procesarResultado(mensaje, resumen, opciones);
    }

    colaTareas_->cerrar();  // los trabajadores salen de su ciclo
    for (std::thread& hilo : hilos) hilo.join();

    for (Tarea* tarea : todasLasTareas()) {
        if (tarea->estado == EstadoTarea::Bloqueada) resumen.quedaronBloqueadas.push_back(tarea->codigo);
    }
    resumen.duracionMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - inicio).count();
    registrar("[Planificador] Fin. Completadas: " + std::to_string(resumen.ordenCompletadas.size()) + ", fallidas: " +
              std::to_string(resumen.fallidas.size()) + ", reintentos: " + std::to_string(resumen.reintentos) + ", tiempo: " +
              std::to_string(resumen.duracionMs) + " ms.");
    return resumen;
}

void Planificador::despachar(Tarea* tarea, ResumenEjecucion& resumen) {
    tarea->estado = EstadoTarea::EnCola;
    tarea->intentos++;
    enVuelo_.push_back(tarea);
    colaTareas_->publicar({tarea->codigo, tarea->duracionMs, tarea->intentos});
    resumen.ordenDespacho.push_back(tarea->codigo);
    repositorio_->guardar(*tarea);
    registrar("[Rojinegro] Sale " + tarea->codigo + " (p" + std::to_string(tarea->prioridad) +
              ", la más urgente) -> [Cola tareas] publicada, en cola: " + std::to_string(colaTareas_->tamano()) + ".");
}

// Llega un resultado: el hash encuentra la tarea, se marca, el grafo libera a sus
// dependientes y esos entran al rojinegro.
void Planificador::procesarResultado(const MensajeResultado& mensaje, ResumenEjecucion& resumen, const OpcionesEjecucion& opciones) {
    Tarea* tarea = buscarTarea(mensaje.codigo);
    if (tarea == nullptr) return;

    switch (mensaje.tipo) {
        case TipoResultado::Iniciada:
            tarea->estado = EstadoTarea::EnEjecucion;
            repositorio_->guardar(*tarea);
            registrar("[" + mensaje.trabajador + "] Toma " + tarea->codigo + " de la cola (intento " + std::to_string(mensaje.intento) + ").");
            break;

        case TipoResultado::Completada:
            enVuelo_.erase(std::remove(enVuelo_.begin(), enVuelo_.end(), tarea), enVuelo_.end());
            tarea->estado = EstadoTarea::Completada;
            resumen.ordenCompletadas.push_back(tarea->codigo);
            repositorio_->guardar(*tarea);
            registrar("[Cola resultados] " + tarea->codigo + " completada por " + mensaje.trabajador + ".");
            liberarTareas(grafo_.completarTarea(tarea->codigo));
            break;

        case TipoResultado::Fallida:
            enVuelo_.erase(std::remove(enVuelo_.begin(), enVuelo_.end(), tarea), enVuelo_.end());
            resumen.intentosFallidos++;
            if (tarea->intentos <= opciones.maximoReintentos) {
                resumen.reintentos++;
                registrar("[Cola resultados] " + tarea->codigo + " falló en " + mensaje.trabajador + " (intento " +
                          std::to_string(tarea->intentos) + "); se reintenta.");
                pasarALista(tarea);
            } else {
                tarea->estado = EstadoTarea::Fallida;
                resumen.fallidas.push_back(tarea->codigo);
                repositorio_->guardar(*tarea);
                registrar("[Cola resultados] " + tarea->codigo + " falló " + std::to_string(tarea->intentos) +
                          " veces y queda Fallida; lo que depende de ella no podrá correr.");
            }
            break;
    }
}

void Planificador::liberarTareas(const std::vector<std::string>& codigos) {
    for (const std::string& codigo : codigos) {
        Tarea* tarea = buscarTarea(codigo);
        if (tarea != nullptr && tarea->estado == EstadoTarea::Bloqueada) {
            registrar("[Grafo] " + codigo + " ya no tiene dependencias pendientes.");
            pasarALista(tarea);
        }
    }
}

void Planificador::pasarALista(Tarea* tarea) {
    tarea->estado = EstadoTarea::Lista;
    tareasListas_.insertar(tarea->llavePrioridad(), tarea);
    repositorio_->guardar(*tarea);
    registrar("[Rojinegro] Entra " + tarea->codigo + " (p" + std::to_string(tarea->prioridad) + "). Tareas listas: " +
              std::to_string(tareasListas_.cantidad()) + ".");
}

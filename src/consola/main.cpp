#include <iostream>
#include <memory>
#include <random>
#include <string>

#include "../logica/EscenarioDemostracion.h"
#include "../logica/Planificador.h"
#include "Vista.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

// Se lanza cuando ya no hay más entrada (consola cerrada o entrada desde archivo).
struct EntradaTerminada {};

// ===================== Lectura validada =====================

std::string leerLinea(const std::string& mensaje) {
    std::cout << mensaje << std::flush;
    std::string linea;
    if (!std::getline(std::cin, linea)) throw EntradaTerminada{};
    return recortar(linea);
}

// Texto obligatorio: si queda vacío o es muy largo se vuelve a preguntar.
std::string leerTexto(const std::string& mensaje, size_t largoMaximo) {
    while (true) {
        std::string texto = leerLinea(mensaje);
        if (texto.empty()) {
            vista::error("No puede quedar vacío.");
        } else if (texto.size() > largoMaximo) {
            vista::error("Máximo " + std::to_string(largoMaximo) + " caracteres.");
        } else {
            return texto;
        }
    }
}

int leerEntero(const std::string& mensaje, int minimo, int maximo, int porDefecto = INT_MIN) {
    while (true) {
        std::string texto = leerLinea(mensaje);
        if (texto.empty() && porDefecto != INT_MIN) return porDefecto;
        bool soloDigitos = !texto.empty() && texto.size() <= 9;
        for (size_t i = 0; i < texto.size() && soloDigitos; i++) {
            if (!std::isdigit(static_cast<unsigned char>(texto[i])) && !(i == 0 && texto[i] == '-')) soloDigitos = false;
        }
        if (soloDigitos && texto != "-") {
            int valor = std::stoi(texto);
            if (valor >= minimo && valor <= maximo) return valor;
        }
        vista::error("Escriba un número entero entre " + std::to_string(minimo) + " y " + std::to_string(maximo) + ".");
    }
}

bool confirmar(const std::string& mensaje) {
    while (true) {
        std::string texto = leerLinea(mensaje + " (s/n): ");
        for (char& c : texto) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (texto == "s" || texto == "si" || texto == "sí") return true;
        if (texto == "n" || texto == "no") return false;
        vista::error("Responda s o n.");
    }
}

// Lee T-0005, t-5 o 5. Enter vacío devuelve "" para volver al menú.
std::string leerCodigoTarea(const std::string& mensaje) {
    while (true) {
        std::string texto = leerLinea(mensaje);
        if (texto.empty()) return "";
        int numero = 0;
        if (leerNumeroTarea(texto, numero)) return formatearCodigo(numero);
        vista::error("Formato inválido. Use por ejemplo T-0005 o solo 5.");
    }
}

// Convierte "APP" o "APP:T-0003" en una llave del B+. Vacío es el extremo del rango.
bool leerLimite(const std::string& texto, bool esInicio, LlaveTarea& llave) {
    llave = esInicio ? LlaveTarea::minima() : LlaveTarea::maxima();
    if (texto.empty()) return true;
    std::string entrada = mayusculas(texto);
    size_t dosPuntos = entrada.find(':');
    std::string proyecto = recortar(entrada.substr(0, dosPuntos));
    if (!codigoProyectoValido(proyecto)) return false;
    if (dosPuntos == std::string::npos) {
        llave = esInicio ? LlaveTarea::inicioDe(proyecto) : LlaveTarea::finDe(proyecto);
        return true;
    }
    int numero = 0;
    if (!leerNumeroTarea(entrada.substr(dosPuntos + 1), numero)) return false;
    llave = {proyecto, numero};
    return true;
}

// ===================== Demostración =====================

class Demostracion {
public:
    explicit Demostracion(Planificador& planificador) : p_(planificador) {}

    void ejecutar() {
        vista::titulo("Demostración");
        vista::info("Se cargan 3 proyectos con 15 tareas, se definen dependencias y se ejecutan con trabajadores.");
        vista::info("Las líneas con sangría son la bitácora del planificador: dicen qué estructura se tocó.");
        pausar_ = confirmar("¿Hacer una pausa entre pasos?");

        vista::titulo("Paso 1: Crear proyectos y tareas");
        vista::info("Cada tarea entra al hash (por código), al B+ (por proyecto:tarea) y al grafo.");
        vista::info("Como todavía no tiene dependencias, también entra al rojinegro como Lista.");
        escenario::cargarProyectosYTareas(p_);
        std::cout << "\n";
        vista::arbolBPlus(p_);
        std::cout << "\n";
        vista::hash(p_);
        pausa();

        vista::titulo("Paso 2: Definir dependencias");
        vista::info("Una arista A -> B significa que B espera a A. A cada tarea se le cuenta cuántas le faltan.");
        escenario::cargarDependencias(p_);
        std::cout << "\n";
        vista::subtitulo("Dependencias que el sistema debe rechazar");
        for (const auto& d : escenario::dependenciasRechazadas()) {
            std::string tarea = formatearCodigo(d.tarea);
            std::string dependeDe = formatearCodigo(d.dependeDe);
            vista::info("Intento: " + tarea + " depende de " + dependeDe + " (" + d.motivo + ")");
            Resultado r = p_.agregarDependencia(tarea, dependeDe);
            if (r.exito) {
                vista::error("Se aceptó y no debía: " + r.mensaje);
            } else {
                vista::exito("Rechazada, como se esperaba: " + r.mensaje);
            }
        }
        std::cout << "\n";
        vista::grafo(p_);
        std::cout << "\n";
        vista::arbolRojoNegro(p_);
        pausa();

        vista::titulo("Paso 3: Cambios antes de ejecutar");
        vista::info("La documentación se vuelve urgente: T-0003 pasa de prioridad 2 a 9.");
        vista::info("En el rojinegro la prioridad es parte de la llave, así que se elimina y se vuelve a insertar.");
        vista::mostrar(p_.cambiarPrioridad("T-0003", 9));
        std::cout << "\n";
        vista::info("Se agrega una tarea que al final no hace falta y se cancela.");
        Resultado extra = p_.crearTarea("APP", "Traducir la documentación", 1, 200);
        if (extra.tarea != nullptr) {
            std::string codigo = extra.tarea->codigo;
            vista::mostrar(p_.agregarDependencia(codigo, "T-0003"));
            vista::mostrar(p_.cancelarTarea(codigo));
        }
        std::cout << "\n";
        vista::arbolRojoNegro(p_);
        pausa();

        OpcionesEjecucion opciones = escenario::opciones();
        vista::titulo("Paso 4: Ejecutar con trabajadores");
        vista::info(std::to_string(opciones.trabajadores) + " trabajadores, 25% de probabilidad de fallo y hasta " +
                    std::to_string(opciones.maximoReintentos) + " reintentos por tarea.");
        vista::info("El planificador solo publica una tarea cuando hay un trabajador libre, y siempre saca la más urgente del rojinegro.");
        std::cout << "\n";
        ResumenEjecucion resumen = p_.ejecutar(opciones);
        pausa();

        vista::titulo("Paso 5: Resultado");
        vista::resumen(resumen);
        bool ordenCorrecto = respetaDependencias(resumen);
        vista::info(std::string("¿Cada tarea terminó después de todo lo que necesitaba? ") + (ordenCorrecto ? "Sí" : "No"));
        std::vector<std::string> primeras;
        for (size_t i = 0; i < resumen.ordenDespacho.size() && i < 3; i++) {
            Tarea* t = p_.buscarTarea(resumen.ordenDespacho[i]);
            primeras.push_back(resumen.ordenDespacho[i] + " (p" + (t ? std::to_string(t->prioridad) : "?") + ")");
        }
        vista::info("Primeras en salir del rojinegro: " + vista::unir(primeras, ", ") + ": las más urgentes de las que estaban listas.");
        std::vector<std::string> reintentadas;
        for (Tarea* t : p_.todasLasTareas()) {
            if (t->intentos > 1) reintentadas.push_back(t->codigo + " (" + std::to_string(t->intentos) + " intentos, " + nombreEstado(t->estado) + ")");
        }
        vista::info("Tareas que fallaron y se reintentaron: " + (reintentadas.empty() ? std::string("ninguna") : vista::unir(reintentadas, ", ")));
        std::cout << "\n";
        vista::subtitulo("Estado final de todas las tareas (recorrido por las hojas del B+)");
        vista::tablaTareas(p_.todasLasTareas());
        std::cout << "\n";
        vista::colas(p_);
        std::cout << "\n";
        vista::arbolRojoNegro(p_);
        std::cout << "\n";
        int completadasGuardadas = 0;
        for (const Tarea& t : p_.repositorio().cargarTodo()) {
            if (t.estado == EstadoTarea::Completada) completadasGuardadas++;
        }
        vista::info("Base de datos (" + p_.repositorio().descripcion() + "): proyectos guardados: " +
                    std::to_string(p_.repositorio().cantidadProyectos()) + ", tareas guardadas: " +
                    std::to_string(p_.repositorio().cantidadTareas()) + ", completadas según la base: " + std::to_string(completadasGuardadas) + ".");
        vista::info("Los datos de la demostración quedan cargados: puede revisarlos con las demás opciones del menú.");
    }

private:
    Planificador& p_;
    bool pausar_ = false;

    void pausa() {
        if (pausar_) leerLinea("Presione Enter para continuar...");
    }

    bool respetaDependencias(const ResumenEjecucion& resumen) {
        auto posicion = [&resumen](const std::string& codigo) {
            for (size_t i = 0; i < resumen.ordenCompletadas.size(); i++) {
                if (resumen.ordenCompletadas[i] == codigo) return static_cast<int>(i);
            }
            return -1;
        };
        for (const NodoGrafo* nodo : p_.grafo().vertices()) {
            int pos = posicion(nodo->codigo);
            if (pos < 0) continue;
            for (const NodoGrafo* dep : nodo->dependencias) {
                int posDep = posicion(dep->codigo);
                if (posDep < 0 || posDep > pos) return false;
            }
        }
        return true;
    }
};

// ===================== Menú =====================

class Menu {
public:
    Menu() { conectarBitacora(); }

    void ejecutar() {
        vista::titulo("Gestor de Tareas en la Nube");
        vista::info("Simulador: la nube son colas y trabajadores; los datos se guardan en " + planificador_->repositorio().descripcion() + ".");
        while (true) {
            mostrarMenu();
            std::string opcion = leerLinea("Opción: ");
            if (opcion == "0") {
                vista::info("Hasta luego.");
                return;
            }
            try {
                ejecutarOpcion(opcion);
            } catch (const EntradaTerminada&) {
                throw;
            } catch (const std::exception& e) {
                // Red de seguridad: un error inesperado no debe cerrar el programa.
                vista::error(std::string("Algo salió mal: ") + e.what());
            }
        }
    }

private:
    std::unique_ptr<Planificador> planificador_ = std::make_unique<Planificador>();
    bool mostrarEventos_ = true;

    void conectarBitacora() {
        planificador_->alRegistrar([this](const std::string& texto) {
            if (mostrarEventos_) std::cout << "  " << texto << "\n";
        });
    }

    void mostrarMenu() {
        int listas = 0, bloqueadas = 0, completadas = 0, fallidas = 0;
        for (const Tarea* t : planificador_->todasLasTareas()) {
            if (t->estado == EstadoTarea::Lista) listas++;
            if (t->estado == EstadoTarea::Bloqueada) bloqueadas++;
            if (t->estado == EstadoTarea::Completada) completadas++;
            if (t->estado == EstadoTarea::Fallida) fallidas++;
        }
        std::cout << "\n------------------- MENÚ PRINCIPAL -------------------\n";
        std::cout << "Proyectos: " << planificador_->proyectos().size() << " | Tareas: " << planificador_->cantidadTareas()
                  << " | Listas: " << listas << " | Bloqueadas: " << bloqueadas << " | Completadas: " << completadas
                  << " | Fallidas: " << fallidas << "\n";
        std::cout << " 1. Crear proyecto\n 2. Crear tarea\n 3. Agregar dependencia\n 4. Cancelar tarea\n 5. Cambiar prioridad\n"
                     " 6. Buscar tarea\n 7. Listar por rango\n 8. Ver estado de las colas\n 9. Ejecutar planificador\n"
                     "10. Ver estructuras\n11. Demostración\n 0. Salir\n";
    }

    void ejecutarOpcion(const std::string& opcion) {
        if (opcion == "1") crearProyecto();
        else if (opcion == "2") crearTarea();
        else if (opcion == "3") agregarDependencia();
        else if (opcion == "4") cancelarTarea();
        else if (opcion == "5") cambiarPrioridad();
        else if (opcion == "6") buscarTarea();
        else if (opcion == "7") listarPorRango();
        else if (opcion == "8") vista::colas(*planificador_);
        else if (opcion == "9") ejecutarPlanificador();
        else if (opcion == "10") verEstructuras();
        else if (opcion == "11") demostracion();
        else vista::error("Opción inválida. Escriba un número del 0 al 11.");
    }

    Tarea* pedirTareaExistente(const std::string& mensaje) {
        std::string codigo = leerCodigoTarea(mensaje);
        if (codigo.empty()) return nullptr;
        Tarea* tarea = planificador_->buscarTarea(codigo);
        if (tarea == nullptr) vista::error("No existe la tarea " + codigo + ".");
        return tarea;
    }

    void crearProyecto() {
        vista::titulo("Crear proyecto");
        std::string codigo = mayusculas(leerLinea("Código, de 2 a 8 letras o números (Enter = volver): "));
        if (codigo.empty()) return;
        if (!codigoProyectoValido(codigo)) {
            vista::error("El código debe tener de 2 a 8 letras o números, sin espacios ni símbolos.");
            return;
        }
        if (planificador_->buscarProyecto(codigo) != nullptr) {
            vista::error("Ya existe el proyecto " + codigo + ".");
            return;
        }
        std::string nombre = leerTexto("Nombre: ", LARGO_MAXIMO_NOMBRE);
        vista::mostrar(planificador_->crearProyecto(codigo, nombre));
    }

    void crearTarea() {
        vista::titulo("Crear tarea");
        if (planificador_->proyectos().empty()) {
            vista::error("Primero cree un proyecto (opción 1).");
            return;
        }
        std::vector<std::string> nombres;
        for (const Proyecto& p : planificador_->proyectos()) nombres.push_back(p.codigo + " (" + p.nombre + ")");
        vista::info("Proyectos: " + vista::unir(nombres, ", "));
        std::string proyecto = leerLinea("Código del proyecto (Enter = volver): ");
        if (proyecto.empty()) return;
        if (planificador_->buscarProyecto(proyecto) == nullptr) {
            vista::error("No existe el proyecto '" + proyecto + "'.");
            return;
        }
        std::string titulo = leerTexto("Título: ", LARGO_MAXIMO_TITULO);
        int prioridad = leerEntero("Prioridad (1 = baja ... 10 = muy urgente): ", PRIORIDAD_MINIMA, PRIORIDAD_MAXIMA);
        int duracion = leerEntero("Duración simulada en ms (0 a 5000, Enter = 300): ", 0, DURACION_MAXIMA_MS, 300);
        vista::mostrar(planificador_->crearTarea(proyecto, titulo, prioridad, duracion));
    }

    void agregarDependencia() {
        vista::titulo("Agregar dependencia");
        if (planificador_->cantidadTareas() < 2) {
            vista::error("Se necesitan al menos dos tareas.");
            return;
        }
        std::string tarea = leerCodigoTarea("Tarea que debe esperar (Enter = volver): ");
        if (tarea.empty()) return;
        std::string dependeDe = leerCodigoTarea("¿De qué tarea depende " + tarea + "? (Enter = volver): ");
        if (dependeDe.empty()) return;
        vista::mostrar(planificador_->agregarDependencia(tarea, dependeDe));
    }

    void cancelarTarea() {
        vista::titulo("Cancelar tarea");
        Tarea* tarea = pedirTareaExistente("Código de la tarea a cancelar (Enter = volver): ");
        if (tarea == nullptr) return;
        vista::info(tarea->codigo + " " + tarea->titulo + " (p" + std::to_string(tarea->prioridad) + ", " + nombreEstado(tarea->estado) + ")");
        if (!confirmar("¿Seguro que desea cancelar " + tarea->codigo + "?")) {
            vista::info("No se canceló nada.");
            return;
        }
        std::string codigo = tarea->codigo;
        vista::mostrar(planificador_->cancelarTarea(codigo));
    }

    void cambiarPrioridad() {
        vista::titulo("Cambiar prioridad");
        Tarea* tarea = pedirTareaExistente("Código de la tarea (Enter = volver): ");
        if (tarea == nullptr) return;
        vista::info(tarea->codigo + " tiene prioridad " + std::to_string(tarea->prioridad) + " y está " + nombreEstado(tarea->estado) + ".");
        if (tarea->estado != EstadoTarea::Bloqueada && tarea->estado != EstadoTarea::Lista) {
            vista::error("La prioridad solo se cambia antes de ejecutarse (Bloqueada o Lista).");
            return;
        }
        int nueva = leerEntero("Nueva prioridad (1 a 10): ", PRIORIDAD_MINIMA, PRIORIDAD_MAXIMA);
        vista::mostrar(planificador_->cambiarPrioridad(tarea->codigo, nueva));
    }

    void buscarTarea() {
        vista::titulo("Buscar tarea");
        Tarea* tarea = pedirTareaExistente("Código de la tarea (Enter = volver): ");
        if (tarea != nullptr) vista::detalleTarea(*planificador_, tarea);
    }

    void listarPorRango() {
        vista::titulo("Listar por rango (árbol B+)");
        vista::info("Escriba PROYECTO o PROYECTO:TAREA. Ejemplos: APP, APP:T-0003, WEB:5.");
        vista::info("Para ver un solo proyecto escriba el mismo código en Desde y Hasta.");
        LlaveTarea desde, hasta;
        if (!leerLimite(leerLinea("Desde (Enter = desde el inicio): "), true, desde)) {
            vista::error("Formato inválido para 'Desde'.");
            return;
        }
        if (!leerLimite(leerLinea("Hasta (Enter = hasta el final): "), false, hasta)) {
            vista::error("Formato inválido para 'Hasta'.");
            return;
        }
        if (hasta < desde) {
            vista::error("El inicio del rango queda después del final.");
            return;
        }
        int hojas = 0;
        vista::tablaTareas(planificador_->listarRango(desde, hasta, &hojas));
        vista::info("Se bajó una sola vez por el árbol. Hojas recorridas siguiendo los enlaces: " + std::to_string(hojas) + ".");
    }

    void ejecutarPlanificador() {
        vista::titulo("Ejecutar planificador");
        if (planificador_->tareasListas().vacio()) {
            vista::info("No hay tareas listas para ejecutar. Cree tareas o revise que no estén todas bloqueadas.");
            return;
        }
        OpcionesEjecucion opciones;
        opciones.trabajadores = leerEntero("Número de trabajadores (1 a 8, Enter = 3): ", 1, 8, 3);
        opciones.probabilidadFallo = leerEntero("Probabilidad de fallo en % (0 a 100, Enter = 20): ", 0, 100, 20) / 100.0;
        opciones.maximoReintentos = leerEntero("Reintentos por tarea (0 a 5, Enter = 2): ", 0, 5, 2);
        bool detalle = confirmar("¿Mostrar cada paso de la ejecución?");
        opciones.semilla = std::random_device{}();
        mostrarEventos_ = detalle;
        ResumenEjecucion resumen = planificador_->ejecutar(opciones);
        mostrarEventos_ = true;
        vista::resumen(resumen);
    }

    void verEstructuras() {
        vista::titulo("Ver estructuras");
        std::cout << " 1. Árbol B+\n 2. Árbol rojinegro\n 3. Directorio del hash extensible\n 4. Grafo de dependencias\n 5. Todas\n 0. Volver\n";
        switch (leerEntero("Opción: ", 0, 5)) {
            case 1: vista::arbolBPlus(*planificador_); break;
            case 2: vista::arbolRojoNegro(*planificador_); break;
            case 3: vista::hash(*planificador_); break;
            case 4: vista::grafo(*planificador_); break;
            case 5: vista::todas(*planificador_); break;
            default: break;
        }
    }

    void demostracion() {
        if (!planificador_->proyectos().empty() && !confirmar("La demostración reemplaza los datos actuales. ¿Continuar?")) return;
        planificador_ = std::make_unique<Planificador>();
        conectarBitacora();
        Demostracion(*planificador_).ejecutar();
    }
};

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try {
        Menu().ejecutar();
    } catch (const EntradaTerminada&) {
        std::cout << "\nSe cerró la entrada. Fin del programa.\n";
    }
    return 0;
}

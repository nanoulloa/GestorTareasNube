#pragma once

#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <string>

// Estados por los que pasa una tarea.
enum class EstadoTarea { Bloqueada, Lista, EnCola, EnEjecucion, Completada, Fallida };

inline const char* nombreEstado(EstadoTarea estado) {
    switch (estado) {
        case EstadoTarea::Bloqueada: return "Bloqueada";
        case EstadoTarea::Lista: return "Lista";
        case EstadoTarea::EnCola: return "EnCola";
        case EstadoTarea::EnEjecucion: return "EnEjecucion";
        case EstadoTarea::Completada: return "Completada";
        case EstadoTarea::Fallida: return "Fallida";
    }
    return "?";
}

constexpr int PRIORIDAD_MINIMA = 1;
constexpr int PRIORIDAD_MAXIMA = 10;
constexpr int DURACION_MAXIMA_MS = 5000;
constexpr size_t LARGO_MAXIMO_TITULO = 60;
constexpr size_t LARGO_MAXIMO_NOMBRE = 40;

inline std::string recortar(const std::string& texto) {
    size_t inicio = texto.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) return "";
    size_t fin = texto.find_last_not_of(" \t\r\n");
    return texto.substr(inicio, fin - inicio + 1);
}

inline std::string mayusculas(std::string texto) {
    for (char& c : texto) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return texto;
}

inline std::string formatearCodigo(int numero) {
    char texto[24];
    std::snprintf(texto, sizeof texto, "T-%04d", numero);
    return texto;
}

// Acepta "T-0042", "t-42" o solo "42".
inline bool leerNumeroTarea(const std::string& entrada, int& numero) {
    numero = 0;
    std::string texto = mayusculas(recortar(entrada));
    if (texto.size() >= 2 && texto[0] == 'T' && texto[1] == '-') texto = texto.substr(2);
    if (texto.empty() || texto.size() > 9) return false;
    for (char c : texto) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    numero = std::stoi(texto);
    return numero > 0;
}

// Un código de proyecto tiene de 2 a 8 letras o números, por ejemplo APP o WEB2.
inline bool codigoProyectoValido(const std::string& codigo) {
    if (codigo.size() < 2 || codigo.size() > 8) return false;
    for (char c : codigo) {
        if (!std::isalnum(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// Llave del árbol B+: primero el proyecto y luego el número, así las tareas
// de un mismo proyecto quedan juntas en las hojas.
struct LlaveTarea {
    std::string proyecto;
    int numero = 0;

    bool operator<(const LlaveTarea& otra) const {
        if (proyecto != otra.proyecto) return proyecto < otra.proyecto;
        return numero < otra.numero;
    }

    std::string texto() const { return proyecto + ":" + formatearCodigo(numero); }

    static LlaveTarea minima() { return {"", 0}; }
    static LlaveTarea maxima() { return {"~", INT_MAX}; }  // '~' va después de cualquier letra o número
    static LlaveTarea inicioDe(const std::string& p) { return {p, 0}; }
    static LlaveTarea finDe(const std::string& p) { return {p, INT_MAX}; }
};

// Llave del rojinegro: es "mayor" la más urgente, así queda en el extremo derecho.
// Con la misma prioridad gana la más vieja (secuencia menor).
struct LlavePrioridad {
    int prioridad = 0;
    long long secuencia = 0;

    bool operator<(const LlavePrioridad& otra) const {
        if (prioridad != otra.prioridad) return prioridad < otra.prioridad;
        return secuencia > otra.secuencia;
    }
};

struct Proyecto {
    std::string codigo;
    std::string nombre;
};

struct Tarea {
    int numero = 0;
    std::string codigo;
    std::string proyecto;
    std::string titulo;
    int prioridad = 1;
    int duracionMs = 0;
    long long secuencia = 0;  // orden de creación, desempata prioridades
    EstadoTarea estado = EstadoTarea::Bloqueada;
    int intentos = 0;

    LlaveTarea llave() const { return {proyecto, numero}; }
    LlavePrioridad llavePrioridad() const { return {prioridad, secuencia}; }
};

enum class TipoResultado { Iniciada, Completada, Fallida };

// Lo que el planificador publica para que un trabajador lo ejecute.
struct MensajeTarea {
    std::string codigo;
    int duracionMs = 0;
    int intento = 0;
};

// Lo que un trabajador publica en la cola de resultados.
struct MensajeResultado {
    std::string codigo;
    TipoResultado tipo = TipoResultado::Iniciada;
    std::string trabajador;
    int intento = 0;
};

// Respuesta de las operaciones del planificador; evita usar excepciones para errores del usuario.
struct Resultado {
    bool exito = false;
    std::string mensaje;
    Tarea* tarea = nullptr;

    static Resultado ok(const std::string& mensaje, Tarea* tarea = nullptr) { return {true, mensaje, tarea}; }
    static Resultado error(const std::string& mensaje) { return {false, mensaje, nullptr}; }
};

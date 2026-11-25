#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// FNV-1a de 32 bits: por cada carácter se hace XOR y se multiplica por un primo.
// Reparte bien los bits aunque los códigos se parezcan (T-0001, T-0002...).
inline uint32_t hashFnv1a(const std::string& texto) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : texto) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

// Cubeta: pocos pares clave-valor con su profundidad local.
template <typename V>
struct Cubeta {
    int id;
    int profundidadLocal;
    int capacidad;  // capacidad normal; solo se pasa en el caso especial de claves con el mismo hash
    std::vector<std::string> claves;
    std::vector<V> valores;

    int cantidad() const { return static_cast<int>(claves.size()); }
    bool llena() const { return cantidad() >= capacidad; }

    int buscar(const std::string& clave) const {
        for (int i = 0; i < cantidad(); i++) {
            if (claves[i] == clave) return i;
        }
        return -1;
    }

    void agregar(const std::string& clave, const V& valor) {
        claves.push_back(clave);
        valores.push_back(valor);
    }

    void quitarEn(int i) {
        claves.erase(claves.begin() + i);
        valores.erase(valores.begin() + i);
    }
};

// Hash extensible: el directorio tiene 2^profundidadGlobal entradas y cada una apunta a
// una cubeta (varias pueden compartirla). Para ubicar una clave se usan los últimos bits
// del hash. Si una cubeta se llena solo esa se divide, y el directorio se duplica
// únicamente cuando la cubeta ya usaba todos sus bits.
template <typename V>
class HashExtensible {
public:
    using FuncionHash = std::function<uint32_t(const std::string&)>;
    static constexpr int PROFUNDIDAD_MAXIMA = 20;  // para que claves con el mismo hash no crezcan sin fin

    explicit HashExtensible(int capacidadCubeta = 4, FuncionHash funcion = hashFnv1a)
        : capacidad_(capacidadCubeta), funcionHash_(std::move(funcion)) {
        directorio_.push_back(nuevaCubeta(0));
    }

    // Primero se juntan las cubetas distintas y después se liberan, porque varias
    // entradas del directorio pueden apuntar a la misma cubeta.
    ~HashExtensible() {
        std::vector<Cubeta<V>*> distintas;
        for (int i = 0; i < tamanoDirectorio(); i++) {
            if (esEntradaPrincipal(i)) distintas.push_back(directorio_[i]);
        }
        for (Cubeta<V>* cubeta : distintas) delete cubeta;
    }

    HashExtensible(const HashExtensible&) = delete;
    HashExtensible& operator=(const HashExtensible&) = delete;

    int cantidad() const { return cantidad_; }
    int profundidadGlobal() const { return profundidadGlobal_; }
    int capacidadCubeta() const { return capacidad_; }
    int tamanoDirectorio() const { return static_cast<int>(directorio_.size()); }
    int divisiones() const { return divisiones_; }
    int duplicaciones() const { return duplicaciones_; }
    int fusiones() const { return fusiones_; }
    int reducciones() const { return reducciones_; }
    const Cubeta<V>* cubetaEn(int indice) const { return directorio_[indice]; }
    uint32_t calcularHash(const std::string& clave) const { return funcionHash_(clave); }

    // Entrada del directorio que le toca a la clave: los últimos bits del hash.
    int indiceDe(const std::string& clave) const { return static_cast<int>(funcionHash_(clave) & mascara(profundidadGlobal_)); }

    std::string indiceEnBinario(int indice) const {
        if (profundidadGlobal_ == 0) return "unica";
        std::string bits;
        for (int b = profundidadGlobal_ - 1; b >= 0; b--) bits += ((indice >> b) & 1) ? '1' : '0';
        return bits;
    }

    int cantidadCubetas() const {
        int total = 0;
        for (int i = 0; i < tamanoDirectorio(); i++) {
            if (esEntradaPrincipal(i)) total++;
        }
        return total;
    }

    // Una cubeta con profundidad local d aparece en 2^(global - d) entradas; la principal
    // es la de índice menor, que siempre es menor que 2^d. Sirve para no repetir cubetas.
    bool esEntradaPrincipal(int indice) const { return indice < (1 << directorio_[indice]->profundidadLocal); }

    bool buscar(const std::string& clave, V& valor) const {
        const Cubeta<V>* cubeta = directorio_[indiceDe(clave)];
        int pos = cubeta->buscar(clave);
        if (pos < 0) return false;
        valor = cubeta->valores[pos];
        return true;
    }

    bool contiene(const std::string& clave) const { return directorio_[indiceDe(clave)]->buscar(clave) >= 0; }

    bool reemplazar(const std::string& clave, const V& valor) {
        Cubeta<V>* cubeta = directorio_[indiceDe(clave)];
        int pos = cubeta->buscar(clave);
        if (pos < 0) return false;
        cubeta->valores[pos] = valor;
        return true;
    }

    // Devuelve false si la clave ya existía.
    bool insertar(const std::string& clave, const V& valor) {
        if (contiene(clave)) return false;
        uint32_t hash = funcionHash_(clave);
        while (true) {
            int indice = static_cast<int>(hash & mascara(profundidadGlobal_));
            Cubeta<V>* cubeta = directorio_[indice];
            bool dividirNoSirve = cubeta->profundidadLocal >= PROFUNDIDAD_MAXIMA || todasConMismoHash(cubeta, hash);
            if (!cubeta->llena() || dividirNoSirve) {
                cubeta->agregar(clave, valor);
                cantidad_++;
                return true;
            }
            // Llena: se divide y se vuelve a intentar (puede tocar dividir otra vez).
            dividirCubeta(indice);
        }
    }

    // Si la cubeta y su hermana caben en una sola se fusionan, y si ninguna cubeta
    // usa todos los bits el directorio se reduce a la mitad.
    bool eliminar(const std::string& clave) {
        int indice = indiceDe(clave);
        Cubeta<V>* cubeta = directorio_[indice];
        int pos = cubeta->buscar(clave);
        if (pos < 0) return false;
        cubeta->quitarEn(pos);
        cantidad_--;
        fusionarSiSePuede(indice);
        reducirSiSePuede();
        return true;
    }

    std::vector<V> valores() const {
        std::vector<V> resultado;
        for (int i = 0; i < tamanoDirectorio(); i++) {
            if (esEntradaPrincipal(i)) {
                for (const V& v : directorio_[i]->valores) resultado.push_back(v);
            }
        }
        return resultado;
    }

    std::vector<std::string> claves() const {
        std::vector<std::string> resultado;
        for (int i = 0; i < tamanoDirectorio(); i++) {
            if (esEntradaPrincipal(i)) {
                for (const std::string& c : directorio_[i]->claves) resultado.push_back(c);
            }
        }
        return resultado;
    }

    // Revisa el directorio, las profundidades y que cada clave esté en su cubeta.
    bool esValido(std::string& error) const {
        error.clear();
        if (tamanoDirectorio() != (1 << profundidadGlobal_)) {
            error = "el directorio no mide 2^global";
            return false;
        }
        int contadas = 0;
        for (int i = 0; i < tamanoDirectorio(); i++) {
            const Cubeta<V>* cubeta = directorio_[i];
            int local = cubeta->profundidadLocal;
            if (local > profundidadGlobal_) {
                error = "profundidad local mayor que la global";
                return false;
            }
            int principal = i & static_cast<int>(mascara(local));
            for (int k = principal; k < tamanoDirectorio(); k += (1 << local)) {
                if (directorio_[k] != cubeta) {
                    error = "entradas que deberian compartir cubeta no la comparten";
                    return false;
                }
            }
            if (i != principal) continue;
            if (cubeta->cantidad() > capacidad_ && local < PROFUNDIDAD_MAXIMA &&
                !todasConMismoHash(cubeta, funcionHash_(cubeta->claves[0]))) {
                error = "una cubeta paso su capacidad sin necesidad";
                return false;
            }
            for (const std::string& clave : cubeta->claves) {
                if ((funcionHash_(clave) & mascara(local)) != static_cast<uint32_t>(principal)) {
                    error = "una clave esta en una cubeta que no le corresponde";
                    return false;
                }
                contadas++;
            }
        }
        if (contadas != cantidad_) {
            error = "la cantidad no coincide";
            return false;
        }
        return true;
    }

private:
    int capacidad_;
    FuncionHash funcionHash_;
    std::vector<Cubeta<V>*> directorio_;
    int profundidadGlobal_ = 0;
    int cantidad_ = 0;
    int siguienteId_ = 1;
    int divisiones_ = 0;
    int duplicaciones_ = 0;
    int fusiones_ = 0;
    int reducciones_ = 0;

    static uint32_t mascara(int bits) { return bits == 0 ? 0u : ((1u << bits) - 1u); }

    Cubeta<V>* nuevaCubeta(int profundidadLocal) { return new Cubeta<V>{siguienteId_++, profundidadLocal, capacidad_, {}, {}}; }

    bool todasConMismoHash(const Cubeta<V>* cubeta, uint32_t hash) const {
        for (const std::string& clave : cubeta->claves) {
            if (funcionHash_(clave) != hash) return false;
        }
        return true;
    }

    void dividirCubeta(int indice) {
        Cubeta<V>* vieja = directorio_[indice];
        // Si la cubeta ya usa todos los bits del directorio, hace falta un bit más.
        if (vieja->profundidadLocal == profundidadGlobal_) duplicarDirectorio();

        uint32_t bitNuevo = 1u << vieja->profundidadLocal;
        Cubeta<V>* nueva = nuevaCubeta(vieja->profundidadLocal + 1);
        vieja->profundidadLocal++;

        // Las claves con el bit nuevo en 1 se pasan a la cubeta nueva.
        int i = 0;
        while (i < vieja->cantidad()) {
            if (funcionHash_(vieja->claves[i]) & bitNuevo) {
                nueva->agregar(vieja->claves[i], vieja->valores[i]);
                vieja->quitarEn(i);
            } else {
                i++;
            }
        }
        // Las entradas que tienen ese bit en 1 ahora apuntan a la nueva.
        for (size_t j = 0; j < directorio_.size(); j++) {
            if (directorio_[j] == vieja && (j & bitNuevo)) directorio_[j] = nueva;
        }
        divisiones_++;
    }

    // La mitad nueva es copia de la vieja; las cubetas no se tocan.
    void duplicarDirectorio() {
        size_t tamano = directorio_.size();
        for (size_t i = 0; i < tamano; i++) directorio_.push_back(directorio_[i]);
        profundidadGlobal_++;
        duplicaciones_++;
    }

    void fusionarSiSePuede(int indice) {
        while (true) {
            Cubeta<V>* cubeta = directorio_[indice];
            int profundidad = cubeta->profundidadLocal;
            if (profundidad == 0) return;
            // La hermana difiere solo en el último bit que usa la cubeta.
            int bit = 1 << (profundidad - 1);
            Cubeta<V>* hermana = directorio_[indice ^ bit];
            if (hermana->profundidadLocal != profundidad || cubeta->cantidad() + hermana->cantidad() > capacidad_) return;

            Cubeta<V>* queda = (indice & bit) == 0 ? cubeta : hermana;
            Cubeta<V>* sale = queda == cubeta ? hermana : cubeta;
            for (int i = 0; i < sale->cantidad(); i++) queda->agregar(sale->claves[i], sale->valores[i]);
            queda->profundidadLocal--;
            for (auto& entrada : directorio_) {
                if (entrada == sale) entrada = queda;
            }
            delete sale;
            fusiones_++;
        }
    }

    void reducirSiSePuede() {
        while (profundidadGlobal_ > 0) {
            for (const Cubeta<V>* cubeta : directorio_) {
                if (cubeta->profundidadLocal == profundidadGlobal_) return;
            }
            // Ninguna cubeta usa el último bit: las dos mitades son iguales.
            directorio_.resize(directorio_.size() / 2);
            profundidadGlobal_--;
            reducciones_++;
        }
    }
};

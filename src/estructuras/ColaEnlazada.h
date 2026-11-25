#pragma once

#include <vector>

// Cola FIFO con nodos enlazados. Guarda el frente (por donde se saca) y el final
// (por donde se mete), así las dos operaciones son O(1).
// No es segura para varios hilos; de eso se encarga ColaNube.
template <typename T>
class ColaEnlazada {
public:
    ColaEnlazada() = default;
    ColaEnlazada(const ColaEnlazada&) = delete;
    ColaEnlazada& operator=(const ColaEnlazada&) = delete;

    ~ColaEnlazada() {
        while (frente_ != nullptr) {
            Nodo* siguiente = frente_->siguiente;
            delete frente_;
            frente_ = siguiente;
        }
    }

    void encolar(const T& valor) {
        Nodo* nuevo = new Nodo{valor, nullptr};
        if (final_ == nullptr) {
            frente_ = nuevo;
        } else {
            final_->siguiente = nuevo;
        }
        final_ = nuevo;
        cantidad_++;
    }

    bool desencolar(T& valor) {
        if (frente_ == nullptr) return false;
        Nodo* viejo = frente_;
        valor = viejo->valor;
        frente_ = viejo->siguiente;
        if (frente_ == nullptr) final_ = nullptr;  // quedó vacía
        delete viejo;
        cantidad_--;
        return true;
    }

    bool verFrente(T& valor) const {
        if (frente_ == nullptr) return false;
        valor = frente_->valor;
        return true;
    }

    int tamano() const { return cantidad_; }
    bool vacia() const { return cantidad_ == 0; }

    // Copia del contenido del frente al final, sin sacar nada.
    std::vector<T> aVector() const {
        std::vector<T> lista;
        for (Nodo* nodo = frente_; nodo != nullptr; nodo = nodo->siguiente) lista.push_back(nodo->valor);
        return lista;
    }

private:
    struct Nodo {
        T valor;
        Nodo* siguiente;
    };

    Nodo* frente_ = nullptr;
    Nodo* final_ = nullptr;
    int cantidad_ = 0;
};

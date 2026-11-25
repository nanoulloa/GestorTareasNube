#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include "../estructuras/ColaEnlazada.h"

// Interfaz de la cola de mensajes. El resto del sistema solo conoce esto, así se
// podría cambiar por un servicio de colas real sin tocar el planificador.
template <typename T>
class IColaMensajes {
public:
    virtual ~IColaMensajes() = default;
    virtual void publicar(const T& mensaje) = 0;
    virtual bool consumir(T& mensaje) = 0;          // espera hasta que haya algo; false si se cerró
    virtual bool intentarConsumir(T& mensaje) = 0;  // no espera
    virtual void cerrar() = 0;
    virtual void abrir() = 0;
    virtual int tamano() const = 0;
    virtual long publicados() const = 0;
    virtual long consumidos() const = 0;
    virtual std::vector<T> contenido() const = 0;
    virtual std::string nombre() const = 0;
};

// Simula un servicio de colas en la nube. Por dentro es la ColaEnlazada hecha a mano,
// protegida con un mutex, y una variable de condición para que los consumidores
// esperen sin gastar CPU.
template <typename T>
class ColaNube : public IColaMensajes<T> {
public:
    explicit ColaNube(std::string nombre) : nombre_(std::move(nombre)) {}

    void publicar(const T& mensaje) override {
        {
            std::lock_guard<std::mutex> candado(mutex_);
            cola_.encolar(mensaje);
            publicados_++;
        }
        hayMensajes_.notify_one();
    }

    bool consumir(T& mensaje) override {
        std::unique_lock<std::mutex> candado(mutex_);
        hayMensajes_.wait(candado, [this] { return !cola_.vacia() || cerrada_; });
        if (cola_.vacia()) return false;  // se cerró y no queda nada
        cola_.desencolar(mensaje);
        consumidos_++;
        return true;
    }

    bool intentarConsumir(T& mensaje) override {
        std::lock_guard<std::mutex> candado(mutex_);
        if (!cola_.desencolar(mensaje)) return false;
        consumidos_++;
        return true;
    }

    // Despierta a todos los que esperan para que terminen.
    void cerrar() override {
        {
            std::lock_guard<std::mutex> candado(mutex_);
            cerrada_ = true;
        }
        hayMensajes_.notify_all();
    }

    void abrir() override {
        std::lock_guard<std::mutex> candado(mutex_);
        cerrada_ = false;
    }

    int tamano() const override {
        std::lock_guard<std::mutex> candado(mutex_);
        return cola_.tamano();
    }

    long publicados() const override {
        std::lock_guard<std::mutex> candado(mutex_);
        return publicados_;
    }

    long consumidos() const override {
        std::lock_guard<std::mutex> candado(mutex_);
        return consumidos_;
    }

    std::vector<T> contenido() const override {
        std::lock_guard<std::mutex> candado(mutex_);
        return cola_.aVector();
    }

    std::string nombre() const override { return nombre_; }

private:
    std::string nombre_;
    ColaEnlazada<T> cola_;
    mutable std::mutex mutex_;
    std::condition_variable hayMensajes_;
    bool cerrada_ = false;
    long publicados_ = 0;
    long consumidos_ = 0;
};

#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include "../estructuras/HashExtensible.h"
#include "../estructuras/Modelo.h"

// Donde se guarda el estado del sistema. El planificador solo conoce esta interfaz.
class IRepositorio {
public:
    virtual ~IRepositorio() = default;
    virtual void guardarProyecto(const Proyecto& proyecto) = 0;
    virtual void guardar(const Tarea& tarea) = 0;  // inserta o reemplaza
    virtual bool eliminar(const std::string& codigo) = 0;
    virtual std::vector<Tarea> cargarTodo() = 0;
    virtual int cantidadProyectos() = 0;
    virtual int cantidadTareas() = 0;
    virtual std::string descripcion() const = 0;
};

// Guarda copias de las tareas en memoria (en otro hash extensible).
class RepositorioMemoria : public IRepositorio {
public:
    void guardarProyecto(const Proyecto& proyecto) override {
        std::lock_guard<std::mutex> candado(mutex_);
        for (Proyecto& p : proyectos_) {
            if (p.codigo == proyecto.codigo) {
                p = proyecto;
                return;
            }
        }
        proyectos_.push_back(proyecto);
    }

    void guardar(const Tarea& tarea) override {
        std::lock_guard<std::mutex> candado(mutex_);
        if (!tareas_.reemplazar(tarea.codigo, tarea)) tareas_.insertar(tarea.codigo, tarea);
    }

    bool eliminar(const std::string& codigo) override {
        std::lock_guard<std::mutex> candado(mutex_);
        return tareas_.eliminar(codigo);
    }

    std::vector<Tarea> cargarTodo() override {
        std::lock_guard<std::mutex> candado(mutex_);
        std::vector<Tarea> todas = tareas_.valores();
        std::sort(todas.begin(), todas.end(), [](const Tarea& a, const Tarea& b) { return a.numero < b.numero; });
        return todas;
    }

    int cantidadProyectos() override {
        std::lock_guard<std::mutex> candado(mutex_);
        return static_cast<int>(proyectos_.size());
    }

    int cantidadTareas() override {
        std::lock_guard<std::mutex> candado(mutex_);
        return tareas_.cantidad();
    }

    std::string descripcion() const override { return "memoria local"; }

private:
    HashExtensible<Tarea> tareas_;
    std::vector<Proyecto> proyectos_;
    std::mutex mutex_;
};

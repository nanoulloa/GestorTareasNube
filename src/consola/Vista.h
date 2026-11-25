#pragma once

#include <string>
#include <vector>

#include "../logica/Planificador.h"

// Impresión de cada estructura de forma que se pueda explicar en la exposición.
namespace vista {

void titulo(const std::string& texto);
void subtitulo(const std::string& texto);
void exito(const std::string& texto);
void error(const std::string& texto);
void info(const std::string& texto);
void mostrar(const Resultado& resultado);

std::string unir(const std::vector<std::string>& textos, const std::string& separador);
std::string ajustar(const std::string& texto, int ancho);
std::string recortarA(const std::string& texto, int largo);

void tablaTareas(const std::vector<Tarea*>& tareas);
void detalleTarea(Planificador& planificador, Tarea* tarea);
void arbolBPlus(Planificador& planificador);
void arbolRojoNegro(Planificador& planificador);
void hash(Planificador& planificador);
void grafo(Planificador& planificador);
void colas(Planificador& planificador);
void resumen(const ResumenEjecucion& resumen);
void todas(Planificador& planificador);

}  // namespace vista

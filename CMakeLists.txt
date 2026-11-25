cmake_minimum_required(VERSION 3.16)
project(GestorTareasNube LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # /utf-8 para que las tildes se vean bien en la consola de Windows
    add_compile_options(/W4 /utf-8 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

find_package(Threads REQUIRED)

# Estructuras y lógica: las usan tanto la consola como las pruebas
add_library(gestor_core STATIC
    src/estructuras/GrafoDependencias.cpp
    src/logica/Planificador.cpp
)
target_include_directories(gestor_core PUBLIC src)
target_link_libraries(gestor_core PUBLIC Threads::Threads)

add_executable(GestorTareas
    src/consola/main.cpp
    src/consola/Vista.cpp
)
target_link_libraries(GestorTareas PRIVATE gestor_core)

add_executable(Pruebas tests/Pruebas.cpp)
target_link_libraries(Pruebas PRIVATE gestor_core)

enable_testing()
add_test(NAME pruebas COMMAND Pruebas)

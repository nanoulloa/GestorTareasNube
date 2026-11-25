#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

enum class ColorNodo { Rojo, Negro };

// Nodo del rojinegro. Las hojas vacías apuntan a un centinela negro en vez de nullptr,
// como en el libro de Cormen; eso simplifica los casos de eliminación.
template <typename K, typename V>
struct NodoRN {
    K llave;
    V valor;
    ColorNodo color;
    NodoRN* izquierdo;
    NodoRN* derecho;
    NodoRN* padre;
};

// Árbol rojinegro genérico. Reglas: cada nodo es rojo o negro, la raíz es negra,
// un rojo no tiene hijos rojos y todos los caminos a las hojas tienen los mismos negros.
// Con eso la altura no pasa de 2 log2(n + 1) y todo es O(log n).
template <typename K, typename V>
class ArbolRojoNegro {
public:
    using Nodo = NodoRN<K, V>;

    ArbolRojoNegro() {
        nulo_ = new Nodo{K{}, V{}, ColorNodo::Negro, nullptr, nullptr, nullptr};
        nulo_->izquierdo = nulo_;
        nulo_->derecho = nulo_;
        nulo_->padre = nulo_;
        raiz_ = nulo_;
    }

    ~ArbolRojoNegro() {
        liberar(raiz_);
        delete nulo_;
    }

    ArbolRojoNegro(const ArbolRojoNegro&) = delete;
    ArbolRojoNegro& operator=(const ArbolRojoNegro&) = delete;

    int cantidad() const { return cantidad_; }
    bool vacio() const { return raiz_ == nulo_; }
    int rotaciones() const { return rotaciones_; }
    int altura() const { return calcularAltura(raiz_); }

    bool buscar(const K& llave, V& valor) const {
        Nodo* nodo = buscarNodo(llave);
        if (nodo == nulo_) return false;
        valor = nodo->valor;
        return true;
    }

    bool contiene(const K& llave) const { return buscarNodo(llave) != nulo_; }

    // El extremo derecho es el mayor (la tarea más urgente).
    bool verMaximo(K& llave, V& valor) const {
        if (raiz_ == nulo_) return false;
        Nodo* maximo = maximoDesde(raiz_);
        llave = maximo->llave;
        valor = maximo->valor;
        return true;
    }

    bool extraerMaximo(K& llave, V& valor) {
        if (!verMaximo(llave, valor)) return false;
        eliminarNodo(maximoDesde(raiz_));
        return true;
    }

    // ---------- Inserción ----------

    // Inserta como en un árbol binario de búsqueda y luego arregla los colores.
    bool insertar(const K& llave, const V& valor) {
        Nodo* padre = nulo_;
        Nodo* actual = raiz_;
        while (actual != nulo_) {
            padre = actual;
            if (llave < actual->llave) {
                actual = actual->izquierdo;
            } else if (actual->llave < llave) {
                actual = actual->derecho;
            } else {
                return false;  // repetida
            }
        }
        Nodo* nuevo = new Nodo{llave, valor, ColorNodo::Rojo, nulo_, nulo_, padre};
        if (padre == nulo_) {
            raiz_ = nuevo;
        } else if (llave < padre->llave) {
            padre->izquierdo = nuevo;
        } else {
            padre->derecho = nuevo;
        }
        cantidad_++;
        arreglarInsercion(nuevo);
        return true;
    }

    // ---------- Eliminación ----------

    bool eliminar(const K& llave) {
        Nodo* nodo = buscarNodo(llave);
        if (nodo == nulo_) return false;
        eliminarNodo(nodo);
        return true;
    }

    // ---------- Recorridos ----------

    std::vector<std::pair<K, V>> recorrerEnOrden() const {
        std::vector<std::pair<K, V>> resultado;
        enOrden(raiz_, resultado);
        return resultado;
    }

    // Visita de derecha a izquierda con la profundidad: sirve para dibujar el árbol acostado.
    void recorrerParaDibujar(const std::function<void(const Nodo*, int)>& visitar) const { dibujar(raiz_, 0, visitar); }

    // Revisa las reglas, el orden de las llaves y los punteros al padre.
    bool esValido(std::string& error) const {
        error.clear();
        if (raiz_->color != ColorNodo::Negro) {
            error = "la raiz no es negra";
            return false;
        }
        int contados = 0;
        if (alturaNegra(raiz_, nullptr, nullptr, contados, error) < 0) return false;
        if (contados != cantidad_) {
            error = "la cantidad no coincide";
            return false;
        }
        return true;
    }

private:
    Nodo* nulo_;
    Nodo* raiz_;
    int cantidad_ = 0;
    int rotaciones_ = 0;

    // El nodo nuevo es rojo: el único problema posible es que su padre también lo sea.
    void arreglarInsercion(Nodo* nodo) {
        while (nodo->padre->color == ColorNodo::Rojo) {
            Nodo* abuelo = nodo->padre->padre;
            if (nodo->padre == abuelo->izquierdo) {
                Nodo* tio = abuelo->derecho;
                if (tio->color == ColorNodo::Rojo) {
                    // Caso 1: tío rojo. Se recolorea y el problema sube al abuelo.
                    nodo->padre->color = ColorNodo::Negro;
                    tio->color = ColorNodo::Negro;
                    abuelo->color = ColorNodo::Rojo;
                    nodo = abuelo;
                } else {
                    if (nodo == nodo->padre->derecho) {
                        // Caso 2: zigzag. Una rotación lo convierte en el caso 3.
                        nodo = nodo->padre;
                        rotarIzquierda(nodo);
                    }
                    // Caso 3: en línea recta. Recolorear y rotar el abuelo.
                    nodo->padre->color = ColorNodo::Negro;
                    abuelo->color = ColorNodo::Rojo;
                    rotarDerecha(abuelo);
                }
            } else {
                // Lo mismo en espejo.
                Nodo* tio = abuelo->izquierdo;
                if (tio->color == ColorNodo::Rojo) {
                    nodo->padre->color = ColorNodo::Negro;
                    tio->color = ColorNodo::Negro;
                    abuelo->color = ColorNodo::Rojo;
                    nodo = abuelo;
                } else {
                    if (nodo == nodo->padre->izquierdo) {
                        nodo = nodo->padre;
                        rotarDerecha(nodo);
                    }
                    nodo->padre->color = ColorNodo::Negro;
                    abuelo->color = ColorNodo::Rojo;
                    rotarIzquierda(abuelo);
                }
            }
        }
        raiz_->color = ColorNodo::Negro;
    }

    void eliminarNodo(Nodo* z) {
        Nodo* y = z;  // el nodo que realmente sale de su lugar
        ColorNodo colorOriginal = y->color;
        Nodo* x;      // el que ocupa el lugar de y

        if (z->izquierdo == nulo_) {
            x = z->derecho;
            trasplantar(z, z->derecho);
        } else if (z->derecho == nulo_) {
            x = z->izquierdo;
            trasplantar(z, z->izquierdo);
        } else {
            // Con dos hijos se usa el sucesor: el menor del subárbol derecho.
            y = minimoDesde(z->derecho);
            colorOriginal = y->color;
            x = y->derecho;
            if (y->padre == z) {
                x->padre = y;
            } else {
                trasplantar(y, y->derecho);
                y->derecho = z->derecho;
                y->derecho->padre = y;
            }
            trasplantar(z, y);
            y->izquierdo = z->izquierdo;
            y->izquierdo->padre = y;
            y->color = z->color;
        }
        delete z;
        cantidad_--;
        // Si salió un negro, a un camino le falta un negro.
        if (colorOriginal == ColorNodo::Negro) arreglarEliminacion(x);
    }

    // x carga un "negro extra"; se resuelve según el color del hermano w y sus hijos.
    void arreglarEliminacion(Nodo* x) {
        while (x != raiz_ && x->color == ColorNodo::Negro) {
            if (x == x->padre->izquierdo) {
                Nodo* w = x->padre->derecho;
                if (w->color == ColorNodo::Rojo) {
                    // Caso 1: hermano rojo. Se rota para que quede negro.
                    w->color = ColorNodo::Negro;
                    x->padre->color = ColorNodo::Rojo;
                    rotarIzquierda(x->padre);
                    w = x->padre->derecho;
                }
                if (w->izquierdo->color == ColorNodo::Negro && w->derecho->color == ColorNodo::Negro) {
                    // Caso 2: hermano negro con hijos negros. El negro extra sube.
                    w->color = ColorNodo::Rojo;
                    x = x->padre;
                } else {
                    if (w->derecho->color == ColorNodo::Negro) {
                        // Caso 3: el hijo rojo del hermano está del lado de adentro.
                        w->izquierdo->color = ColorNodo::Negro;
                        w->color = ColorNodo::Rojo;
                        rotarDerecha(w);
                        w = x->padre->derecho;
                    }
                    // Caso 4: el hijo rojo está del lado de afuera. Se termina.
                    w->color = x->padre->color;
                    x->padre->color = ColorNodo::Negro;
                    w->derecho->color = ColorNodo::Negro;
                    rotarIzquierda(x->padre);
                    x = raiz_;
                }
            } else {
                Nodo* w = x->padre->izquierdo;
                if (w->color == ColorNodo::Rojo) {
                    w->color = ColorNodo::Negro;
                    x->padre->color = ColorNodo::Rojo;
                    rotarDerecha(x->padre);
                    w = x->padre->izquierdo;
                }
                if (w->derecho->color == ColorNodo::Negro && w->izquierdo->color == ColorNodo::Negro) {
                    w->color = ColorNodo::Rojo;
                    x = x->padre;
                } else {
                    if (w->izquierdo->color == ColorNodo::Negro) {
                        w->derecho->color = ColorNodo::Negro;
                        w->color = ColorNodo::Rojo;
                        rotarIzquierda(w);
                        w = x->padre->izquierdo;
                    }
                    w->color = x->padre->color;
                    x->padre->color = ColorNodo::Negro;
                    w->izquierdo->color = ColorNodo::Negro;
                    rotarDerecha(x->padre);
                    x = raiz_;
                }
            }
        }
        x->color = ColorNodo::Negro;
    }

    // Pone al subárbol v en el lugar del subárbol u.
    void trasplantar(Nodo* u, Nodo* v) {
        if (u->padre == nulo_) {
            raiz_ = v;
        } else if (u == u->padre->izquierdo) {
            u->padre->izquierdo = v;
        } else {
            u->padre->derecho = v;
        }
        v->padre = u->padre;
    }

    // Rotación a la izquierda: el hijo derecho y sube al lugar de x, x queda como
    // hijo izquierdo de y, y el subárbol izquierdo de y pasa a ser el derecho de x.
    void rotarIzquierda(Nodo* x) {
        Nodo* y = x->derecho;
        x->derecho = y->izquierdo;
        if (y->izquierdo != nulo_) y->izquierdo->padre = x;
        y->padre = x->padre;
        if (x->padre == nulo_) {
            raiz_ = y;
        } else if (x == x->padre->izquierdo) {
            x->padre->izquierdo = y;
        } else {
            x->padre->derecho = y;
        }
        y->izquierdo = x;
        x->padre = y;
        rotaciones_++;
    }

    void rotarDerecha(Nodo* y) {
        Nodo* x = y->izquierdo;
        y->izquierdo = x->derecho;
        if (x->derecho != nulo_) x->derecho->padre = y;
        x->padre = y->padre;
        if (y->padre == nulo_) {
            raiz_ = x;
        } else if (y == y->padre->derecho) {
            y->padre->derecho = x;
        } else {
            y->padre->izquierdo = x;
        }
        x->derecho = y;
        y->padre = x;
        rotaciones_++;
    }

    Nodo* buscarNodo(const K& llave) const {
        Nodo* actual = raiz_;
        while (actual != nulo_) {
            if (llave < actual->llave) {
                actual = actual->izquierdo;
            } else if (actual->llave < llave) {
                actual = actual->derecho;
            } else {
                return actual;
            }
        }
        return nulo_;
    }

    Nodo* minimoDesde(Nodo* nodo) const {
        while (nodo->izquierdo != nulo_) nodo = nodo->izquierdo;
        return nodo;
    }

    Nodo* maximoDesde(Nodo* nodo) const {
        while (nodo->derecho != nulo_) nodo = nodo->derecho;
        return nodo;
    }

    void enOrden(Nodo* nodo, std::vector<std::pair<K, V>>& resultado) const {
        if (nodo == nulo_) return;
        enOrden(nodo->izquierdo, resultado);
        resultado.push_back({nodo->llave, nodo->valor});
        enOrden(nodo->derecho, resultado);
    }

    void dibujar(Nodo* nodo, int profundidad, const std::function<void(const Nodo*, int)>& visitar) const {
        if (nodo == nulo_) return;
        dibujar(nodo->derecho, profundidad + 1, visitar);
        visitar(nodo, profundidad);
        dibujar(nodo->izquierdo, profundidad + 1, visitar);
    }

    int calcularAltura(Nodo* nodo) const {
        if (nodo == nulo_) return 0;
        return 1 + std::max(calcularAltura(nodo->izquierdo), calcularAltura(nodo->derecho));
    }

    // Altura negra del subárbol, o -1 si encuentra un error.
    int alturaNegra(Nodo* nodo, const K* minimo, const K* maximo, int& contados, std::string& error) const {
        if (nodo == nulo_) return 1;
        contados++;
        if ((minimo != nullptr && !(*minimo < nodo->llave)) || (maximo != nullptr && !(nodo->llave < *maximo))) {
            error = "una llave rompe el orden";
            return -1;
        }
        if (nodo->color == ColorNodo::Rojo && (nodo->izquierdo->color == ColorNodo::Rojo || nodo->derecho->color == ColorNodo::Rojo)) {
            error = "un nodo rojo tiene un hijo rojo";
            return -1;
        }
        if ((nodo->izquierdo != nulo_ && nodo->izquierdo->padre != nodo) || (nodo->derecho != nulo_ && nodo->derecho->padre != nodo)) {
            error = "un hijo no apunta a su padre";
            return -1;
        }
        int izquierda = alturaNegra(nodo->izquierdo, minimo, &nodo->llave, contados, error);
        if (izquierda < 0) return -1;
        int derecha = alturaNegra(nodo->derecho, &nodo->llave, maximo, contados, error);
        if (derecha < 0) return -1;
        if (izquierda != derecha) {
            error = "los caminos tienen distinta cantidad de negros";
            return -1;
        }
        return izquierda + (nodo->color == ColorNodo::Negro ? 1 : 0);
    }

    void liberar(Nodo* nodo) {
        if (nodo == nulo_) return;
        liberar(nodo->izquierdo);
        liberar(nodo->derecho);
        delete nodo;
    }
};

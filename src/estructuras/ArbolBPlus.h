#pragma once

#include <string>
#include <utility>
#include <vector>

// Nodo del árbol B+. Las hojas guardan llaves y valores y están enlazadas con
// "siguiente"; los internos solo guardan separadores e hijos. Los arreglos
// tienen un espacio de más para aguantar el desborde antes de dividir.
template <typename K, typename V>
struct NodoBPlus {
    bool esHoja;
    int cantidad = 0;  // cuántas llaves tiene
    K* claves;
    V* valores;          // solo en hojas
    NodoBPlus** hijos;   // solo en internos, siempre cantidad + 1
    NodoBPlus* siguiente = nullptr;

    NodoBPlus(int orden, bool hoja)
        : esHoja(hoja),
          claves(new K[orden]),
          valores(hoja ? new V[orden] : nullptr),
          hijos(hoja ? nullptr : new NodoBPlus*[orden + 1]()) {}

    ~NodoBPlus() {
        delete[] claves;
        delete[] valores;
        delete[] hijos;
    }

    NodoBPlus(const NodoBPlus&) = delete;
    NodoBPlus& operator=(const NodoBPlus&) = delete;

    void insertarEnHoja(int pos, const K& llave, const V& valor) {
        for (int i = cantidad; i > pos; i--) {
            claves[i] = claves[i - 1];
            valores[i] = valores[i - 1];
        }
        claves[pos] = llave;
        valores[pos] = valor;
        cantidad++;
    }

    void quitarDeHoja(int pos) {
        for (int i = pos; i < cantidad - 1; i++) {
            claves[i] = claves[i + 1];
            valores[i] = valores[i + 1];
        }
        cantidad--;
    }

    // Mete el separador en la posición i y el hijo derecho en i + 1.
    void insertarSeparador(int i, const K& separador, NodoBPlus* derecho) {
        for (int j = cantidad; j > i; j--) claves[j] = claves[j - 1];
        for (int j = cantidad + 1; j > i + 1; j--) hijos[j] = hijos[j - 1];
        claves[i] = separador;
        hijos[i + 1] = derecho;
        cantidad++;
    }

    // Quita la llave i y el hijo que está a su derecha.
    void quitarSeparador(int i) {
        for (int j = i; j < cantidad - 1; j++) claves[j] = claves[j + 1];
        for (int j = i + 1; j < cantidad; j++) hijos[j] = hijos[j + 1];
        hijos[cantidad] = nullptr;
        cantidad--;
    }

    void insertarAlInicio(const K& llave, NodoBPlus* hijo) {
        for (int j = cantidad; j > 0; j--) claves[j] = claves[j - 1];
        for (int j = cantidad + 1; j > 0; j--) hijos[j] = hijos[j - 1];
        claves[0] = llave;
        hijos[0] = hijo;
        cantidad++;
    }

    void agregarAlFinal(const K& llave, NodoBPlus* hijo) {
        claves[cantidad] = llave;
        hijos[cantidad + 1] = hijo;
        cantidad++;
    }

    std::pair<K, NodoBPlus*> quitarPrimero() {
        K llave = claves[0];
        NodoBPlus* hijo = hijos[0];
        for (int j = 0; j < cantidad - 1; j++) claves[j] = claves[j + 1];
        for (int j = 0; j < cantidad; j++) hijos[j] = hijos[j + 1];
        hijos[cantidad] = nullptr;
        cantidad--;
        return {llave, hijo};
    }

    std::pair<K, NodoBPlus*> quitarUltimo() {
        K llave = claves[cantidad - 1];
        NodoBPlus* hijo = hijos[cantidad];
        hijos[cantidad] = nullptr;
        cantidad--;
        return {llave, hijo};
    }
};

// Árbol B+ genérico. Todos los datos viven en las hojas y las hojas están enlazadas,
// por eso recorrer un rango es bajar una vez y luego avanzar hoja por hoja.
// Con orden 4: máximo 3 llaves por nodo, las hojas mínimo 2 y los internos mínimo 1.
template <typename K, typename V>
class ArbolBPlus {
public:
    using Nodo = NodoBPlus<K, V>;

    explicit ArbolBPlus(int orden = 4)
        : orden_(orden), maxClaves_(orden - 1), minHoja_(orden / 2), minInterno_((orden + 1) / 2 - 1) {}

    ~ArbolBPlus() { liberar(raiz_); }

    ArbolBPlus(const ArbolBPlus&) = delete;
    ArbolBPlus& operator=(const ArbolBPlus&) = delete;

    int orden() const { return orden_; }
    int cantidad() const { return cantidad_; }
    bool vacio() const { return raiz_ == nullptr; }
    int divisiones() const { return divisiones_; }
    int redistribuciones() const { return redistribuciones_; }
    int fusiones() const { return fusiones_; }

    int altura() const {
        int niveles = 0;
        for (const Nodo* n = raiz_; n != nullptr; n = n->esHoja ? nullptr : n->hijos[0]) niveles++;
        return niveles;
    }

    // ---------- Búsqueda ----------

    bool buscar(const K& llave, V& valor) const {
        const Nodo* hoja = buscarHoja(llave);
        if (hoja == nullptr) return false;
        int i = posicionEnHoja(hoja, llave);
        if (i < hoja->cantidad && igual(hoja->claves[i], llave)) {
            valor = hoja->valores[i];
            return true;
        }
        return false;
    }

    bool contiene(const K& llave) const {
        V valor{};
        return buscar(llave, valor);
    }

    // ---------- Inserción ----------

    // Devuelve false si la llave ya existía.
    bool insertar(const K& llave, const V& valor) {
        if (raiz_ == nullptr) {
            raiz_ = new Nodo(orden_, true);
            raiz_->insertarEnHoja(0, llave, valor);
            cantidad_ = 1;
            return true;
        }

        K separador{};
        Nodo* nuevoDerecho = nullptr;
        if (!insertarRecursivo(raiz_, llave, valor, separador, nuevoDerecho)) return false;

        // Si la raíz se dividió, el árbol crece un nivel hacia arriba.
        if (nuevoDerecho != nullptr) {
            Nodo* nuevaRaiz = new Nodo(orden_, false);
            nuevaRaiz->hijos[0] = raiz_;
            nuevaRaiz->insertarSeparador(0, separador, nuevoDerecho);
            raiz_ = nuevaRaiz;
        }
        cantidad_++;
        return true;
    }

    // ---------- Eliminación ----------

    // Devuelve false si la llave no existía.
    bool eliminar(const K& llave) {
        if (raiz_ == nullptr || !eliminarRecursivo(raiz_, llave)) return false;
        cantidad_--;

        // Si la raíz quedó sin llaves, el árbol baja un nivel.
        if (raiz_->cantidad == 0) {
            Nodo* vieja = raiz_;
            raiz_ = raiz_->esHoja ? nullptr : raiz_->hijos[0];
            delete vieja;
        }
        return true;
    }

    // ---------- Recorridos ----------

    // Pares con llave entre desde y hasta (incluidas). Baja una vez y sigue las hojas enlazadas.
    std::vector<std::pair<K, V>> recorrerRango(const K& desde, const K& hasta, int* hojasVisitadas = nullptr) const {
        std::vector<std::pair<K, V>> resultado;
        int hojas = 0;
        if (!(hasta < desde)) {
            const Nodo* hoja = buscarHoja(desde);
            int i = hoja == nullptr ? 0 : posicionEnHoja(hoja, desde);
            bool seguir = true;
            while (hoja != nullptr && seguir) {
                hojas++;
                for (; i < hoja->cantidad; i++) {
                    if (hasta < hoja->claves[i]) {
                        seguir = false;
                        break;
                    }
                    resultado.push_back({hoja->claves[i], hoja->valores[i]});
                }
                hoja = hoja->siguiente;
                i = 0;
            }
        }
        if (hojasVisitadas != nullptr) *hojasVisitadas = hojas;
        return resultado;
    }

    std::vector<std::pair<K, V>> recorrerTodo() const {
        std::vector<std::pair<K, V>> resultado;
        const Nodo* hoja = raiz_;
        while (hoja != nullptr && !hoja->esHoja) hoja = hoja->hijos[0];
        for (; hoja != nullptr; hoja = hoja->siguiente) {
            for (int i = 0; i < hoja->cantidad; i++) resultado.push_back({hoja->claves[i], hoja->valores[i]});
        }
        return resultado;
    }

    // Nodos nivel por nivel, de la raíz a las hojas. Se usa para imprimir.
    std::vector<std::vector<const Nodo*>> obtenerNiveles() const {
        std::vector<std::vector<const Nodo*>> niveles;
        if (raiz_ == nullptr) return niveles;
        std::vector<const Nodo*> actual{raiz_};
        while (!actual.empty()) {
            niveles.push_back(actual);
            std::vector<const Nodo*> siguiente;
            for (const Nodo* nodo : actual) {
                if (!nodo->esHoja) {
                    for (int i = 0; i <= nodo->cantidad; i++) siguiente.push_back(nodo->hijos[i]);
                }
            }
            actual = siguiente;
        }
        return niveles;
    }

    // Revisa las reglas del B+: orden, tamaños, hojas al mismo nivel y lista de hojas completa.
    bool esValido(std::string& error) const {
        error.clear();
        if (raiz_ == nullptr) {
            if (cantidad_ != 0) error = "arbol vacio con cantidad distinta de cero";
            return error.empty();
        }
        std::vector<const Nodo*> hojas;
        int profundidadHojas = -1;
        int contadas = 0;
        if (!validarNodo(raiz_, nullptr, nullptr, 0, profundidadHojas, hojas, contadas, error)) return false;
        if (contadas != cantidad_) {
            error = "la cantidad no coincide";
            return false;
        }
        for (size_t i = 0; i < hojas.size(); i++) {
            const Nodo* esperada = i + 1 < hojas.size() ? hojas[i + 1] : nullptr;
            if (hojas[i]->siguiente != esperada) {
                error = "una hoja no apunta a la que le sigue";
                return false;
            }
        }
        return true;
    }

private:
    int orden_;
    int maxClaves_;
    int minHoja_;
    int minInterno_;
    Nodo* raiz_ = nullptr;
    int cantidad_ = 0;
    int divisiones_ = 0;
    int redistribuciones_ = 0;
    int fusiones_ = 0;

    static bool igual(const K& a, const K& b) { return !(a < b) && !(b < a); }

    int minimo(const Nodo* nodo) const { return nodo->esHoja ? minHoja_ : minInterno_; }

    // Primera posición de la hoja con llave mayor o igual a la buscada.
    static int posicionEnHoja(const Nodo* hoja, const K& llave) {
        int i = 0;
        while (i < hoja->cantidad && hoja->claves[i] < llave) i++;
        return i;
    }

    // Hijo por el que se baja. Si la llave es igual al separador se va a la derecha,
    // porque el separador es la primera llave del hijo derecho.
    static int indiceHijo(const Nodo* nodo, const K& llave) {
        int i = 0;
        while (i < nodo->cantidad && !(llave < nodo->claves[i])) i++;
        return i;
    }

    const Nodo* buscarHoja(const K& llave) const {
        const Nodo* nodo = raiz_;
        while (nodo != nullptr && !nodo->esHoja) nodo = nodo->hijos[indiceHijo(nodo, llave)];
        return nodo;
    }

    // Baja hasta la hoja e inserta. Si un nodo se desborda se divide y le devuelve
    // al padre la llave separadora y el nuevo nodo derecho.
    bool insertarRecursivo(Nodo* nodo, const K& llave, const V& valor, K& separador, Nodo*& nuevoDerecho) {
        nuevoDerecho = nullptr;
        if (nodo->esHoja) {
            int pos = posicionEnHoja(nodo, llave);
            if (pos < nodo->cantidad && igual(nodo->claves[pos], llave)) return false;  // repetida
            nodo->insertarEnHoja(pos, llave, valor);
            if (nodo->cantidad > maxClaves_) {
                nuevoDerecho = dividirHoja(nodo);
                separador = nuevoDerecho->claves[0];
            }
            return true;
        }

        int i = indiceHijo(nodo, llave);
        K separadorHijo{};
        Nodo* derechoHijo = nullptr;
        if (!insertarRecursivo(nodo->hijos[i], llave, valor, separadorHijo, derechoHijo)) return false;
        if (derechoHijo != nullptr) {
            nodo->insertarSeparador(i, separadorHijo, derechoHijo);
            if (nodo->cantidad > maxClaves_) nuevoDerecho = dividirInterno(nodo, separador);
        }
        return true;
    }

    // Parte una hoja llena. La primera llave de la hoja nueva se COPIA hacia arriba.
    Nodo* dividirHoja(Nodo* hoja) {
        Nodo* nueva = new Nodo(orden_, true);
        int mitad = hoja->cantidad / 2;
        for (int j = mitad; j < hoja->cantidad; j++) nueva->insertarEnHoja(nueva->cantidad, hoja->claves[j], hoja->valores[j]);
        hoja->cantidad = mitad;
        nueva->siguiente = hoja->siguiente;  // se mantiene la lista de hojas
        hoja->siguiente = nueva;
        divisiones_++;
        return nueva;
    }

    // Parte un interno lleno. La llave del medio SUBE y no se queda en ninguno de los dos.
    Nodo* dividirInterno(Nodo* nodo, K& llaveQueSube) {
        Nodo* nuevo = new Nodo(orden_, false);
        int medio = nodo->cantidad / 2;
        llaveQueSube = nodo->claves[medio];
        nuevo->hijos[0] = nodo->hijos[medio + 1];
        for (int j = medio + 1; j < nodo->cantidad; j++) nuevo->agregarAlFinal(nodo->claves[j], nodo->hijos[j + 1]);
        while (nodo->cantidad > medio) nodo->quitarUltimo();
        divisiones_++;
        return nuevo;
    }

    bool eliminarRecursivo(Nodo* nodo, const K& llave) {
        if (nodo->esHoja) {
            int pos = posicionEnHoja(nodo, llave);
            if (pos >= nodo->cantidad || !igual(nodo->claves[pos], llave)) return false;
            nodo->quitarDeHoja(pos);
            return true;
        }
        int i = indiceHijo(nodo, llave);
        if (!eliminarRecursivo(nodo->hijos[i], llave)) return false;
        // El padre revisa si el hijo quedó con menos llaves de las permitidas.
        if (nodo->hijos[i]->cantidad < minimo(nodo->hijos[i])) rebalancear(nodo, i);
        return true;
    }

    // Primero intenta pedir prestado a un hermano; si ninguno puede, fusiona.
    void rebalancear(Nodo* padre, int i) {
        if (i > 0 && padre->hijos[i - 1]->cantidad > minimo(padre->hijos[i - 1])) {
            prestarDeIzquierda(padre, i);
        } else if (i < padre->cantidad && padre->hijos[i + 1]->cantidad > minimo(padre->hijos[i + 1])) {
            prestarDeDerecha(padre, i);
        } else if (i > 0) {
            fusionar(padre, i - 1);
        } else {
            fusionar(padre, i);
        }
    }

    void prestarDeIzquierda(Nodo* padre, int i) {
        Nodo* hijo = padre->hijos[i];
        Nodo* izquierdo = padre->hijos[i - 1];
        if (hijo->esHoja) {
            int ultima = izquierdo->cantidad - 1;
            hijo->insertarEnHoja(0, izquierdo->claves[ultima], izquierdo->valores[ultima]);
            izquierdo->quitarDeHoja(ultima);
            padre->claves[i - 1] = hijo->claves[0];
        } else {
            // En internos la llave baja del padre y la del hermano sube al padre.
            auto par = izquierdo->quitarUltimo();
            hijo->insertarAlInicio(padre->claves[i - 1], par.second);
            padre->claves[i - 1] = par.first;
        }
        redistribuciones_++;
    }

    void prestarDeDerecha(Nodo* padre, int i) {
        Nodo* hijo = padre->hijos[i];
        Nodo* derecho = padre->hijos[i + 1];
        if (hijo->esHoja) {
            hijo->insertarEnHoja(hijo->cantidad, derecho->claves[0], derecho->valores[0]);
            derecho->quitarDeHoja(0);
            padre->claves[i] = derecho->claves[0];
        } else {
            auto par = derecho->quitarPrimero();
            hijo->agregarAlFinal(padre->claves[i], par.second);
            padre->claves[i] = par.first;
        }
        redistribuciones_++;
    }

    // Une al hijo k con el k + 1 y quita del padre la llave que los separaba.
    void fusionar(Nodo* padre, int k) {
        Nodo* izquierdo = padre->hijos[k];
        Nodo* derecho = padre->hijos[k + 1];
        if (izquierdo->esHoja) {
            for (int j = 0; j < derecho->cantidad; j++) izquierdo->insertarEnHoja(izquierdo->cantidad, derecho->claves[j], derecho->valores[j]);
            izquierdo->siguiente = derecho->siguiente;
        } else {
            // En internos el separador del padre baja y queda en medio.
            izquierdo->agregarAlFinal(padre->claves[k], derecho->hijos[0]);
            for (int j = 0; j < derecho->cantidad; j++) izquierdo->agregarAlFinal(derecho->claves[j], derecho->hijos[j + 1]);
        }
        padre->quitarSeparador(k);
        delete derecho;
        fusiones_++;
    }

    bool validarNodo(const Nodo* nodo, const K* minimoPermitido, const K* maximoPermitido, int profundidad,
                     int& profundidadHojas, std::vector<const Nodo*>& hojas, int& contadas, std::string& error) const {
        bool esRaiz = nodo == raiz_;
        if (nodo->cantidad > maxClaves_ || (!esRaiz && nodo->cantidad < minimo(nodo)) || nodo->cantidad == 0) {
            error = "nodo con cantidad de llaves fuera de los limites";
            return false;
        }
        for (int i = 0; i < nodo->cantidad; i++) {
            if (i > 0 && !(nodo->claves[i - 1] < nodo->claves[i])) {
                error = "llaves desordenadas dentro de un nodo";
                return false;
            }
            if ((minimoPermitido != nullptr && nodo->claves[i] < *minimoPermitido) ||
                (maximoPermitido != nullptr && !(nodo->claves[i] < *maximoPermitido))) {
                error = "llave fuera del rango que le da su padre";
                return false;
            }
        }
        if (nodo->esHoja) {
            if (profundidadHojas == -1) profundidadHojas = profundidad;
            if (profundidadHojas != profundidad) {
                error = "hay hojas en niveles distintos";
                return false;
            }
            hojas.push_back(nodo);
            contadas += nodo->cantidad;
            return true;
        }
        for (int i = 0; i <= nodo->cantidad; i++) {
            const K* min = i > 0 ? &nodo->claves[i - 1] : minimoPermitido;
            const K* max = i < nodo->cantidad ? &nodo->claves[i] : maximoPermitido;
            if (!validarNodo(nodo->hijos[i], min, max, profundidad + 1, profundidadHojas, hojas, contadas, error)) return false;
        }
        return true;
    }

    void liberar(Nodo* nodo) {
        if (nodo == nullptr) return;
        if (!nodo->esHoja) {
            for (int i = 0; i <= nodo->cantidad; i++) liberar(nodo->hijos[i]);
        }
        delete nodo;
    }
};

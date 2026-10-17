#ifndef SISTEMA_ARCHI_FAT_H
#define SISTEMA_ARCHI_FAT_H

#include <vector>
#include <iostream>
#include <string>
#include <algorithm>

#include "archivoFAT.h"
#include "cache.h"

//CLASE ADMINISTRADORA (DISCO DURO + TABLA FAT + DIRECTORIO)

//Sistema de Archivos, aqui se encuentran todas la funciones del Sistema de Archivos

class SistemaArchivosFAT
{
public:
    //Disco Duro (Matriz de bloques de caracteres/bytes)
    std::vector<std::vector<char>> discoFisico;

    //Memoria Ram: Tabla FAT (-2 = Libre, -1 = EOF / Fin de Archivo)
    std::vector<int> tablaFAT;

    //Memoria Ram: Directorio de archivos 
    std::vector<archivoFAT> directorio;

    //Caches: Vector/Cache de lineas cache
    cache L1;
    cache L2;
    cache L3;

    //Contadores de acceso a las caches (por nivel)
    size_t hitL1 = 0;   // aciertos en L1
    size_t missL1 = 0;  // fallos en L1
    size_t hitL2 = 0;   // aciertos en L2
    size_t missL2 = 0;  // fallos en L2
    size_t hitL3 = 0;   // aciertos en L3
    size_t missL3 = 0;  // fallos en L3

    size_t tamanoBloque; // Tamano fijo de cada bloque (ej: 16 bytes)


    //Constructor: Inicializa el disco, la tabla FAT y los Caches
    SistemaArchivosFAT(size_t totalBloques = 100, size_t bytesPorBloque = 16)
    : L1(40, bytesPorBloque), L2(60, bytesPorBloque), L3(80, bytesPorBloque) //vector de 40 bloques, vector de 60 bloques, vector de 80 bloques
    {
        tamanoBloque = bytesPorBloque;  //16 bytes en cada bloque/ 16 casillas
        discoFisico.resize(totalBloques, std::vector<char>(bytesPorBloque, '\0'));  //100 bloques/casilla del disco duro, los vectores que estan en cada casilla tendra 16 casillas inicializadas en '\0'
        tablaFAT.resize(totalBloques, -2); //vector de 100 bloques, -2 representa bloque libre
    }

    //Busca la primera casilla libre en la Tabla FAT
    int buscarBloqueLibre()
    {
        for(size_t i = 0; i < tablaFAT.size(); i++)
        {
            if(tablaFAT[i] == -2)
            {
                return i;
            }
        }
        return -1; //Disco lleno
    }

    //Verifica si ya existe un archivo con ese nombre en el directorio
    bool nombreExiste(const std::string& n)
    {
        for(size_t i = 0; i < directorio.size(); i++)
        {
            if(directorio[i].nombreFAT() == n)
            {
                return true;
            }
        }
        return false;
    }

    //Busca los bloques disponibles para guardar los metadatos del archivo y su contenido real 
    bool guardarArchivo(std::string nombre, const std::string& texto, bool visible)
    {
        if(texto.empty()) //Si no hay texto retorna false
        {
            return false;
        }

        if(nombre.empty()) //nombre por defecto si el usuario no escribe nada
        {
            nombre = "Sin titulo";
        }

        //Si ya existe un archivo con ese nombre, le agregamos un sufijo (2), (3), ...
        if(nombreExiste(nombre))
        {
            int sufijo = 2;
            std::string nuevoNombre;
            do
            {
                nuevoNombre = nombre + " (" + std::to_string(sufijo) + ")";
                sufijo++;
            }while (nombreExiste(nuevoNombre)); //buscamos el primer sufijo libre
            nombre = nuevoNombre;
        }

        size_t bytesTotal = texto.length(); //le asignamos el tamano del texto
        size_t bloquesNecesarios = (bytesTotal + tamanoBloque - 1) / tamanoBloque; //obtenemos la cantidad de bloques que vamos a necesitar en base a la formula

        //Verificar espacio disponible ANTES de modificar nada (en la RAM/FAT)

        size_t bloquesLibres = 0;   
        for(int estado : tablaFAT)
        {
            if(estado == -2)   //contamos la cantidad de bloques libres
            {
                bloquesLibres++;
            }
        }

        if(bloquesLibres < bloquesNecesarios) //se cumple si hay mas bloques necesarios que bloques libres (osea, no hay suficiente espacio)
        {
            std::cout << "Error: No hay espacio suficiente en el disco.\n";
            return false;
        }

        int primerBloqueAsignado = -1;
        int bloqueAnterior = -1;
        size_t posTexto = 0;

        //ciclo que se encargara de guardar el archivo en sus bloques correspondientes
        for(size_t b = 0; b < bloquesNecesarios; b++) 
        {
            int bloqueLibre = buscarBloqueLibre();

            tablaFAT[bloqueLibre] = -1; //Marcado temporalmente como EOF

            if(primerBloqueAsignado == -1) //revisa si ya tenemos el primer bloque, si se cumple es porque no lo tenemos y lo vamos a asginar
            {
                primerBloqueAsignado = bloqueLibre;
            }

            if(bloqueAnterior != -1) //revisa si ya tenemos un bloque anterior, guardamos la direccion del bloque libre actual en v[bloqueAnterior]
            {
                tablaFAT[bloqueAnterior] = bloqueLibre;
            }


            size_t numLineasCache = L1.tam; //guardamos el numero de lineas de la cache
            int idx = bloqueLibre % numLineasCache; //Posición en la Caché L1
            int tag = bloqueLibre / numLineasCache; //Etiqueta

            //Conteo de aciertos/fallos en L1, hay un hit si la línea ya tenía ese mismo bloque
            if(L1.caches[idx].valido && L1.caches[idx].tag == tag)
            {
                hitL1++; //acierto
            } 
            else
            {
                missL1++; //fallo
            } 

            //En caso de que la Cache L1 tenga una casilla ocupada
            if(L1.caches[idx].valido) 
            {
                //Si la casilla ya tenía datos de otro bloque (tag distinto):
                if(L1.caches[idx].tag != tag) 
                {


                    
                    std::cout << "[EVICTION] La línea L1[" << idx << "] está ocupada por el bloque con Tag "  << L1.caches[idx].tag << ". Desalojando a Caché L2...\n";

                    //Reconstruimos el ID del bloque viejo que sale de L1
                    int bloqueViejoID = L1.caches[idx].tag * numLineasCache + idx;

                    size_t numLineasCacheL2 = L2.tam; //guardamos el numero de lineas de la cache
                    int idxL2 = bloqueViejoID % numLineasCacheL2; // Posición en la Caché L2
                    int tagL2 = bloqueViejoID / numLineasCacheL2; // Etiqueta

                    //Conteo de aciertos/fallos en L2: el bloque bajó a L2
                    if(L2.caches[idxL2].valido && L2.caches[idxL2].tag == tagL2)
                    {
                        hitL2++;
                    }
                    else
                    {
                        missL2++;
                    } 



                    if(L2.caches[idxL2].valido)
                    {
                        if(L2.caches[idxL2].tag != tagL2)
                        {



                            std::cout << "[EVICTION] La línea L2[" << idxL2 << "] está ocupada por el bloque con Tag "  << L2.caches[idxL2].tag << ". Desalojando a Caché L3...\n";

                            // Reconstruimos el ID del bloque viejo que sale de L2
                            int bloqueViejoID_L2 = L2.caches[idxL2].tag * numLineasCacheL2 + idxL2;

                            size_t numLineasCacheL3 = L3.tam;
                            int idxL3 = bloqueViejoID_L2 % numLineasCacheL3; // Posición en la Caché L1
                            int tagL3 = bloqueViejoID_L2 / numLineasCacheL3;

                            // Conteo de aciertos/fallos en L3: el bloque bajó a L3
                            if(L3.caches[idxL3].valido && L3.caches[idxL3].tag == tagL3)
                            {
                                hitL3++;
                            }
                            else
                            {
                                missL3++;
                            }



                            if(L3.caches[idxL3].valido)
                            {
                               if(L3.caches[idxL3].tag != tagL3)
                                {
                                    std::cout << "[EVICTION] La línea L3[" << idxL3 << "] está ocupada. Desalojando a RAM...\n";

                                    //Reconstruimos el ID del bloque viejo que sale de L3
                                    int bloqueViejoID_L3 = L3.caches[idxL3].tag * numLineasCacheL3 + idxL3;

                                    //Escritura en la RAM
                                    discoFisico[bloqueViejoID_L3] = L3.caches[idxL3].datos;
                                }

                            }

                            //Sobreescritura en L3
                            L3.caches[idxL3].valido = true;
                            L3.caches[idxL3].tag = tagL3;
                            L3.caches[idxL3].datos = L2.caches[idxL2].datos; // Los datos que venían bajando de L2
                        }

                    }

                    //Sobreescritura en L2
                    L2.caches[idxL2].valido = true;
                    L2.caches[idxL2].tag = tagL2;
                    L2.caches[idxL2].datos = L1.caches[idx].datos;
                    
                }
            }
            
            //Sobreescritura en L1
            L1.caches[idx].valido = true;
            L1.caches[idx].tag = tag;
            L1.guardar_datos(idx, bytesTotal, posTexto, texto);

            //Mantenemos la coherencia guardando también en la RAM
            discoFisico[bloqueLibre] = L1.caches[idx].datos;

            bloqueAnterior = bloqueLibre; // Al final asignamos a Bloque Anterior, el Bloque Libre
        }

        //Registrar metadatos en el directorio (RAM)
        archivoFAT nuevoArchivo(nombre);
        nuevoArchivo.tamanobytes = bytesTotal;
        nuevoArchivo.primerbloque = primerBloqueAsignado;

        directorio.push_back(nuevoArchivo);

        if(visible)
        {
            std::cout << "Archivo '" << nombre << "' guardado exitosamente.\n";
        }

        return true;
    }

    //Leer Archivo (se muestra el recorriedo de las caches y ram)
    std::string leerArchivo(const std::string& nombre)
    {
        int bloqueActual = -1;

        //Busca el archivo en el directorio para obtener su primer bloque
        for(auto& arch : directorio) 
        {
            if(arch.nombreFAT() == nombre) 
            {
                bloqueActual = arch.primerbloque;
                break;
            }
        }

        if(bloqueActual == -1) 
        {
            return "";
        }

        std::string textoReconstruido = "";

        //Recorre la lista enlazada de la FAT; por cada bloque recorre la jerarquia de memoria
        while(bloqueActual != -1) {

            size_t numLineasCache = L1.tam;
            int idx = bloqueActual % numLineasCache; // Posición en la Caché L1
            int tag = bloqueActual / numLineasCache; // Etiqueta en L1

            const std::vector<char>* datos; // de dónde se toman los bytes finales

            //Busqueda en L1
            if(L1.caches[idx].valido && L1.caches[idx].tag == tag)
            {
                hitL1++; //HIT en L1
                datos = &L1.caches[idx].datos;
            }
            else
            {
                missL1++; //MISS en L1 -> bajamos a L2

                size_t numLineasCacheL2 = L2.tam;
                int idxL2 = bloqueActual % numLineasCacheL2; // Posición en la Caché L2
                int tagL2 = bloqueActual / numLineasCacheL2; // Etiqueta en L2

                //Busqueda en L2
                if(L2.caches[idxL2].valido && L2.caches[idxL2].tag == tagL2)
                {
                    hitL2++; //HIT en L2
                    datos = &L2.caches[idxL2].datos;
                }
                else
                {
                    missL2++; //MISS en L2 -> bajamos a L3

                    size_t numLineasCacheL3 = L3.tam;
                    int idxL3 = bloqueActual % numLineasCacheL3; // Posición en la Caché L3
                    int tagL3 = bloqueActual / numLineasCacheL3; // Etiqueta en L3

                    //Busqueda en L3
                    if(L3.caches[idxL3].valido && L3.caches[idxL3].tag == tagL3)
                    {
                        hitL3++; //HIT en L3
                        datos = &L3.caches[idxL3].datos;
                    }
                    else
                    {
                        missL3++; //MISS en toda la jerarquia -> se lee de la RAM

                        datos = &discoFisico[bloqueActual];

                        std::cout << "[READ MISS] El bloque " << bloqueActual << " no estaba en cache. Cargando desde RAM a L3/L2/L1...\n";

                        //Llenar L3 con los datos de la RAM
                        L3.caches[idxL3].valido = true;
                        L3.caches[idxL3].tag = tagL3;
                        L3.caches[idxL3].datos = discoFisico[bloqueActual];

                        //Llenar L2 con lo que subio de L3
                        L2.caches[idxL2].valido = true;
                        L2.caches[idxL2].tag = tagL2;
                        L2.caches[idxL2].datos = L3.caches[idxL3].datos;

                        //Llenar L1 con lo que subio de L2
                        L1.caches[idx].valido = true;
                        L1.caches[idx].tag = tag;
                        L1.caches[idx].datos = L2.caches[idxL2].datos;

                        datos = &L1.caches[idx].datos;
                    }
                }
            }

            for(char c : *datos) 
            {
                if(c != '\0')
                {
                    textoReconstruido += c;
                } 
            }
            bloqueActual = tablaFAT[bloqueActual]; //Salto al siguiente bloque
        }

        return textoReconstruido;
    }

    //Borrado de Archivo
    bool borrarArchivo(const std::string& nombre, bool visible)
    {
        for(size_t i = 0; i < directorio.size(); i++) 
        {
            if (directorio[i].nombreFAT() == nombre)
            {
                int bloqueActual = directorio[i].primerbloque;

                //Recorre la cadena en la FAT marcando los bloques como libres (-2)
                while (bloqueActual != -1)
                {
                    int siguiente = tablaFAT[bloqueActual];
                    tablaFAT[bloqueActual] = -2; // Liberar posición
                    
                    //Limpia los bytes en el disco físico
                    std::fill(discoFisico[bloqueActual].begin(), discoFisico[bloqueActual].end(), '\0');

                    bloqueActual = siguiente;
                }

                //Elimina la entrada del directorio
                directorio.erase(directorio.begin() + i);

                if (visible)
                {
                    std::cout << "Archivo '" << nombre << "' eliminado del disco.\n";
                }

                return true;
            }
        }
        std::cout << "Error: No se pudo eliminar.\n";
        return false;
    }

    //Editar Archivo
    bool editarArchivo(const std::string& nombre, const std::string& nuevoTexto)
    {
        if (nuevoTexto.empty())
        {
            return false;  //no se edita con texto vacio (evita borrar sin consecuencia)
        }

        if (!borrarArchivo(nombre,false))
        {
            return false;  //libera bloques y quita del directorio
        }

        return guardarArchivo(nombre, nuevoTexto,false);  // guarda la versión nueva
    }

    //Mostrar Tabla FAT/mapa del Disco
    void mostrarFAT()
    {
        std::cout << "\n=== TABLA FAT ===\n";
        for (size_t i = 0; i < tablaFAT.size(); i++)
        {
            std::cout << "Bloque " << i << " -> " << tablaFAT[i] << "\n";
        }
    }

    //Buscar archivos en el directorio
    bool detallesArchivo(const std::string& nombre)
    {
        for (size_t i = 0; i < directorio.size(); i++)
        {
            if (directorio[i].nombreFAT() == nombre)
            {
                directorio[i].detallesFAT();
                return true;
            }
        }
        return false;
    }

    //Mostrar estados de la cache
    void mostrarCaches()
    {
        std::cout << "\n=== CACHE L1 (" << L1.tam << " lineas) ===\n";
        for (size_t i = 0; i < L1.tam; i++)
        {
            if(L1.caches[i].valido)
            {
                int bloqueID = L1.caches[i].tag * L1.tam + i;
                std::cout << "Linea " << i << ": valido=true tag=" << L1.caches[i].tag << " bloque=" << bloqueID << " datos=[";
                for(char c : L1.caches[i].datos)
                {
                    if (c == '\0')
                    {
                        std::cout << "";
                    }
                    else
                    {
                        std::cout << c;
                    }
                }
                std::cout << "]\n";
            }
            else
            {
                std::cout << "Linea " << i << ": valido=false\n";
            }
        }

        std::cout << "\n=== CACHE L2 (" << L2.tam << " lineas) ===\n";

        for (size_t i = 0; i < L2.tam; i++)
        {
            if (L2.caches[i].valido)
            {
                int bloqueID = L2.caches[i].tag * L2.tam + i;
                std::cout << "Linea " << i << ": valido=true tag=" << L2.caches[i].tag << " bloque=" << bloqueID << " datos=[";
                for (char c : L2.caches[i].datos)
                {
                    if (c == '\0')
                    {
                        std::cout << "";
                    }
                    else
                    {
                        std::cout << c;
                    }
                }
                std::cout << "]\n";
            }
            else
            {
                std::cout << "Linea " << i << ": valido=false\n";
            }
        }

        std::cout << "\n=== CACHE L3 (" << L3.tam << " lineas) ===\n";

        for (size_t i = 0; i < L3.tam; i++)
        {
            if (L3.caches[i].valido)
            {
                int bloqueID = L3.caches[i].tag * L3.tam + i;
                std::cout << "Linea " << i << ": valido=true tag=" << L3.caches[i].tag << " bloque=" << bloqueID << " datos=[";

                for (char c : L3.caches[i].datos)
                {
                    if (c == '\0')
                    {
                        std::cout << "";
                    }
                    else
                    {
                        std::cout << c;
                    }
                }
                std::cout << "]\n";
            }
            else
            {
                std::cout << "Linea " << i << ": valido=false\n";
            }
        }
        std::cout << "\n";
    }

    //Renombrar Archivos
    bool renombrarArchivo(const std::string& nombre, const std::string& nuevoNombre)
    {
        if(nuevoNombre.empty())
        {
            return false;  //no se renombra a nombre vacio
        }

        for(size_t i = 0; i < directorio.size(); i++)
        {
            if(directorio[i].nombreFAT() == nombre)
            {
                if(nombreExiste(nuevoNombre))  // evita duplicados
                {
                    std::cout << "Error: ya existe un archivo con el nombre '" << nuevoNombre << "'.\n";
                    return false;
                }
                directorio[i].edit_nombreFAT(nuevoNombre);
                std::cout << "Archivo '" << nombre << "' renombrado a '" << nuevoNombre << "'.\n";
                return true;
            }
        }
        std::cout << "Error: archivo '" << nombre << "' no encontrado.\n";
        return false;
    }

    //Estadisticas del Disco
    void estadisticasDisco()
    {
        size_t libres = 0;
        size_t ocupados = 0;

        for(int estado : tablaFAT)
        {
            if (estado == -2)
            {
                libres++;
            }
            else
            {
                ocupados++;
            }
        }

        std::cout << "\n=== ESTADISTICAS DEL DISCO ===\n";
        std::cout << "Bloques totales:   " << tablaFAT.size() << "\n";
        std::cout << "Bloques libres:    " << libres << "\n";
        std::cout << "Bloques ocupados:  " << ocupados << "\n";
        std::cout << "Archivos en el directorio: " << directorio.size() << "\n";
    }

    //Estadisticas de Acceso a la Cache
    void mostrarNivel(const std::string& nombre, size_t hits, size_t misses)
    {
        size_t total = hits + misses;
        double tasaHit = (total > 0) ? (100.0 * hits) / total : 0.0;
        double tasaMiss = (total > 0) ? (100.0 * misses) / total : 0.0;

        std::cout << "--- " << nombre << " ---\n";
        std::cout << "Aciertos (hits):  " << hits << "\n";
        std::cout << "Fallos (misses):  " << misses << "\n";
        std::cout << "Total accesos:    " << total << "\n";
        std::cout << "Tasa de aciertos: " << tasaHit << "%\n";
        std::cout << "Tasa de fallos:   " << tasaMiss << "%\n";
    }

    void estadisticasCache()
    {
        std::cout << "\n=== ESTADISTICAS DE CACHES ===\n";
        mostrarNivel("L1", hitL1, missL1);
        std::cout << "\n";
        mostrarNivel("L2", hitL2, missL2);
        std::cout << "\n";
        mostrarNivel("L3", hitL3, missL3);
    }

    void listarDirectorio()
    {
        if(directorio.empty())
        {
            std::cout << "El directorio esta vacio.\n";
            return;
        }

        for(auto& arch : directorio)
        {
            std::cout << arch.nombreFAT() <<"\n---\n";
        }
    }

};


#endif

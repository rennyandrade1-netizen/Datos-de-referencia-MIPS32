#ifndef SISTEMA_ARC_EXT2_H
#define SISTEMA_ARC_EXT2_H

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>

#include "archivoEXT2.h"

class SistemaArchivosExt2
{
    public:

        //Disco fisico (matriz de bloques de bytes)
        std::vector<std::vector<char>> discoFisico;

        size_t tamanoBloque; //Tamano fijo de cada bloque

        //Superbloque: la copia en memoria de la configuracion del disco.
        //Todavia no existe hasta que se llama a formatear() o montar()
        SuperbloqueExt2 superbloque;

        //Memoria: los tres vectores de la jerarquia

        //L1: copia en memoria de la tabla de inodos (los metadatos): se necesita en cada operacion
        std::vector<InodoExt2> tablaInodos;

        //L2: copia en memoria de los bloques de datos de los archivos
        std::vector<std::vector<char>> cacheDatos;

        //L3: copia en memoria de las entradas del directorio abierto
        std::vector<DirectorioExt2> directorio;

        //Inodo del directorio que esta cargado en el vector L3
        int directorioActual;

        //Bloques que pertenecen al directorio abierto
        std::vector<int> bloquesDirectorioAbierto;

        //Contadores de acierto y fallo por nivel
        size_t hitInodos = 0;       //aciertos en inodos
        size_t missInodos = 0;      //fallos en inodos
        size_t hitDatos = 0;        //aciertos en datos
        size_t missDatos = 0;       //fallos en datos
        size_t hitDirectorios = 0;  //aciertos en directorios
        size_t missDirectorios = 0; //fallos en directorios

        //Constructor: crea el disco fisico en blanco.
        SistemaArchivosExt2(size_t totalBloques = 100, size_t bytesPorBloque = 1024)
        {
            //Si la geometria no sirve, la ajustamos a una minima valida en vez de dejar el disco en un estado que revienta al formatear

            if(!SuperbloqueExt2::geometriaValida(totalBloques, bytesPorBloque))
            {
                std::cout << "Error: geometria invalida (" << totalBloques << " bloques de " << bytesPorBloque << " bytes).\n";
                std::cout << "Se necesitan al menos 12 bloques y un tamano de bloque\n" << " multiplo de " << TAMANO_INODO << " bytes. Se ajusta la geometria.\n";

                if(bytesPorBloque < TAMANO_INODO)
                {
                    bytesPorBloque = TAMANO_INODO;
                }

                if(bytesPorBloque % TAMANO_INODO != 0)
                {
                    bytesPorBloque = ((bytesPorBloque / TAMANO_INODO) + 1) * TAMANO_INODO;
                }
                
                if(totalBloques < 12)
                { 
                    totalBloques = 12;
                }
            }

            tamanoBloque = bytesPorBloque;
            directorioActual = -1;
            discoFisico.resize(totalBloques, std::vector<char>(bytesPorBloque, '\0'));

            //Un vector vacio significa "este bloque no esta en la cache"
            cacheDatos.resize(totalBloques);
        }

        //Vacia las dos caches en memoria. Se usa al formatear, para no dejar datos de un disco anterior dando vueltas
        void limpiarCache()
        {
            tablaInodos.clear();
            directorio.clear();
            directorioActual = -1;
            bloquesDirectorioAbierto.clear();
            cacheDatos.assign(discoFisico.size(), std::vector<char>());
        }

        //Formatea el disco: construye el superbloque, inicializa los bitmaps y reserva los bloques y los inodos que usa el propio sistema
        void formatear(bool silencio)
        {
            limpiarCache();
            superbloque = SuperbloqueExt2(discoFisico.size(), tamanoBloque);

            //Si el disco es muy chico o el tamano de bloque no es multiplo de 128, los indices del superbloque se salen del vector
            if(discoFisico.size() < 12 || tamanoBloque % TAMANO_INODO != 0)
            {
                std::cout << "Error: el disco no puede formatearse. Se necesitan al menos\n" << "12 bloques y un tamano de bloque multiplo de " << TAMANO_INODO << " bytes.\n";
                return;
            }

            //Aun con 12 bloques, la raiz necesita uno mas para sus entradas
            if(discoFisico.size() <= superbloque.inicioDatos)
            {
                std::cout << "Error: el disco es demasiado chico. El sistema usa los bloques\n" << "0 al " << superbloque.inicioDatos << " y hace falta al menos\n" << "un bloque mas para la raiz.\n";
                return;
            }

            superbloque.guardarEnDisco(discoFisico);

            //Los bitmaps arrancan en cero, que significa "todo libre"
            for(unsigned int b = superbloque.inicioBitmapBloques; b < superbloque.inicioDatos; b++)
            {
                std::fill(discoFisico[b].begin(), discoFisico[b].end(), '\0');
            }

            //Marcamos como ocupados los bloques del propio sistema: el superbloque,
            //los bitmaps y la tabla de inodos. Si no, buscarBloqueLibre() devolveria
            //el bloque 0 y el primer archivo pisaria el superbloque
            for(unsigned int b = 0; b < superbloque.inicioDatos; b++)
            {
                marcarBloque(b, true);
            }

            //Los primeros inodos son del sistema: el 1 es de bloques danados, el 2 la raiz
            for(int i = 0; i < INODOS_RESERVADOS; i++)
            {
                marcarInodo(i, true);
            }

            crearRaiz(); //el directorio raiz es el inodo 2

            if(!silencio)
            {

                std::cout << "Disco formateado: " << superbloque.totalBloques << " bloques de " << superbloque.bytesPorBloque << " bytes.\n";
                std::cout << "Bloques del sistema: 0 al " << superbloque.inicioDatos - 1 << "\n";
            }


            abrirDirectorio(2); // dejamos la raiz abierta y vacia
        }

        //Crea el directorio raiz. En Ext2 la raiz es siempre el inodo 2: el inodo 0 no existe y el 1 esta reservado para los bloques danados
        void crearRaiz()
        {
            int bloqueRaiz = buscarBloqueLibre();

            InodoExt2 raiz(2, true); //inodo 2, y es un directorio
            raiz.tamanobytes = 0;    //todavia no tiene ninguna entrada

            if(bloqueRaiz != -1)
            {
                //La raiz necesita al menos un bloque para guardar sus entradas, aun que este vacio
                raiz.bloquesDirectos[0] = bloqueRaiz;
                marcarBloque(bloqueRaiz, true);

                //El bloque tiene que quedar en cero.
                std::fill(discoFisico[bloqueRaiz].begin(), discoFisico[bloqueRaiz].end(), '\0');
                cacheDatos[bloqueRaiz].clear();
            }
            else
            {
                std::cout << "Error: no hay bloques libres para la raiz.\n";
            }

            escribirInodo(raiz);
            marcarInodo(2, true);
        }

        //Operaciones sobre el bitmap de bloques
        //El bitmap vive en el disco, dentro del bloque que indica el superbloque.
        //El bit N del bitmap corresponde al bloque N: 0 = libre, 1 = ocupado

        static int byteDelBit(int bit)
        {
            return bit / 8;
        }

        static int mascaraDelBit(int bit)
        {
            return 1 << (bit % 8);
        }

        //Verifica si un bloque esta libre
        bool estaBloqueLibre(int bloque) const
        {
            if(bloque < 0 || (unsigned int) bloque >= superbloque.totalBloques)
            { 
                return false;
            }

            int byte = discoFisico[superbloque.inicioBitmapBloques][byteDelBit(bloque)];

            return (byte & mascaraDelBit(bloque)) == 0;
        }

        //Marca un bloque como ocupado o como libre
        void marcarBloque(int bloque, bool ocupado)
        {
            if(bloque < 0 || (unsigned int) bloque >= superbloque.totalBloques)
            { 
                return;
            }

            char& byte = discoFisico[superbloque.inicioBitmapBloques][byteDelBit(bloque)];

            if(ocupado)
            {
                byte |= mascaraDelBit(bloque);
            }
            else
            {         
                byte &= ~mascaraDelBit(bloque);
            }
        }

        //Busca el primer bloque libre del disco.
        //Empieza en el bloque 0
        int buscarBloqueLibre()
        {
            for(int b = 0; b < (int) superbloque.totalBloques; b++)
            {
                if (estaBloqueLibre(b))
                {
                    return b;
                }
            }

            return -1; // disco lleno
        }

        //Cuantos bloques de tamanoBloque necesita un texto de cierta cantidad de bytes.
        int cuantosBloquesNecesita(size_t bytes) const
        {
            return (bytes + tamanoBloque - 1) / tamanoBloque;
        }

        //Busca el primer inodo libre. 
        int buscarInodoLibre()
        {
            for(int i = 0; i < (int) superbloque.totalInodos; i++)
            {
                if(estaInodoLibre(i))
                {
                    return i;
                }
            }

            return -1; // no quedan inodos
        }

        //Operaciones sobre el bitmap de inodo

        //Verifica si un inodo esta libre
        bool estaInodoLibre(int numero) const
        {
            if(numero < 0 || (unsigned int) numero >= superbloque.totalInodos)
            {
                return false;
            }

            int byte = discoFisico[superbloque.inicioBitmapInodos][byteDelBit(numero)];

            return (byte & mascaraDelBit(numero)) == 0;
        }

        //Marca un inodo como ocupado o como libre
        void marcarInodo(int numero, bool ocupado)
        {
            if(numero < 0 || (unsigned int) numero >= superbloque.totalInodos)
            { 
                return;
            }

            char& byte = discoFisico[superbloque.inicioBitmapInodos][byteDelBit(numero)];

            if(ocupado)
            { 
                byte |= mascaraDelBit(numero);
            }
            else
            {         
                byte &= ~mascaraDelBit(numero);
            }
        }

        //Tabla de inodos
        //Los inodos se guardan en el disco, de a TAMANO_INODO bytes cada uno.
        //El inodo N esta en el bloque:
        //inicioTablaInodos + (N * 128) / bytesPorBloque
        //y en la posicion (N * 128) % bytesPorBloque de ese bloque

        //A que bloque del disco pertenece el inodo numero N
        int bloqueDelInodo(int numero) const
        {
            return superbloque.inicioTablaInodos + (numero * TAMANO_INODO) / (int) superbloque.bytesPorBloque;
        }

        //Posicion del inodo N dentro de su bloque
        int posicionDelInodo(int numero) const
        {
            return (numero * TAMANO_INODO) % (int) superbloque.bytesPorBloque;
        }

        //Escribe un inodo en la tabla de inodos del disco.
        
        void escribirInodo(const InodoExt2& inodo)
        {
            if(inodo.idInodo < 0 || (unsigned int) inodo.idInodo >= superbloque.totalInodos)
            {
                return;
            }

            int bloque = bloqueDelInodo(inodo.idInodo);
            int pos = posicionDelInodo(inodo.idInodo);

            if(bloque >= (int) superbloque.inicioDatos)
            { 
                return; // se salio de la tabla
            }

            //El inodo tiene que caber entero en el bloque. Si el tamano de bloque no es
            //multiplo de 128, el ultimo inodo se parte y pisa el siguiente bloque
            if(pos + TAMANO_INODO > (int) discoFisico[bloque].size()) 
            {
                return;
            }

            char buffer[TAMANO_INODO];

            snprintf(buffer, sizeof(buffer), "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                     inodo.idInodo, (int) inodo.tamanobytes, inodo.esDirectorio ? 1 : 0,
                     inodo.bloquesDirectos[0],  inodo.bloquesDirectos[1],
                     inodo.bloquesDirectos[2],  inodo.bloquesDirectos[3],
                     inodo.bloquesDirectos[4],  inodo.bloquesDirectos[5],
                     inodo.bloquesDirectos[6],  inodo.bloquesDirectos[7],
                     inodo.bloquesDirectos[8],  inodo.bloquesDirectos[9],
                     inodo.bloquesDirectos[10], inodo.bloquesDirectos[11],
                     inodo.bloqueIndirectoSimple);

            std::fill(discoFisico[bloque].begin() + pos, discoFisico[bloque].begin() + pos + TAMANO_INODO, '\0');

            for(int i = 0; buffer[i] != '\0'; i++)
            {
                discoFisico[bloque][pos + i] = buffer[i];
            }

            guardarEnCacheInodo(inodo);
        }

        //Cache L1 de inodos
        //tablaInodos es la copia en memoria. Antes de ir al disco se busca aqui

        //Busca el inodo en la cache. Si lo encuentra, copia y devuelve true
        bool buscarEnCacheInodo(int numero, InodoExt2& inodo) const
        {
            for(size_t i = 0; i < tablaInodos.size(); i++)
            {
                if(tablaInodos[i].idInodo == numero)
                {
                    inodo = tablaInodos[i];
                    return true;
                }
            }

            return false;
        }

        //Deja el inodo en la cache, o lo actualiza si ya estaba
        void guardarEnCacheInodo(const InodoExt2& inodo)
        {
            for(size_t i = 0; i < tablaInodos.size(); i++)
            {
                if(tablaInodos[i].idInodo == inodo.idInodo)
                {
                    tablaInodos[i] = inodo;
                    return;
                }
            }

            tablaInodos.push_back(inodo);
        }

        //Leer un inodo pasando por la cache: primero busca en memoria y solo si no lo encuentra va al disco
        bool obtenerInodo(int numero, InodoExt2& inodo)
        {
            if(buscarEnCacheInodo(numero, inodo))
            {
                hitInodos++;
                return true;
            }

            missInodos++;

            if(!leerInodo(numero, inodo)) 
            {
                return false;
            }

            guardarEnCacheInodo(inodo); //queda cargado para la proxima vez
            return true;
        }

        //Cache L2 de bloques de datos
        //Un bloque esta en la cache si su vector NO esta vacio

        bool estaBloqueEnCache(int bloque) const
        {
            if(bloque < 0 || (unsigned int) bloque >= cacheDatos.size())
            {
                return false;
            }

            return !cacheDatos[bloque].empty();
        }

        //Escribe un bloque de datos: lo graba en el disco y lo deja en la cache
        void escribirBloque(int bloque, const std::vector<char>& datos)
        {
            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return;
            }

            discoFisico[bloque] = datos;
            cacheDatos[bloque] = datos;
        }

        //Lee un bloque de datos: primero busca en la cache y, si no esta, va al disco
        std::vector<char> leerBloque(int bloque)
        {
            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return std::vector<char>();
            }

            if(estaBloqueEnCache(bloque)) //acierto
            {
                hitDatos++;
                return cacheDatos[bloque];
            }

            missDatos++; //fallo: hay que ir al disco
            cacheDatos[bloque] = discoFisico[bloque];
            return cacheDatos[bloque];
        }

        //Libera un bloque: lo deja libre en el bitmap y lo borra del disco y de la cache
        void liberarBloque(int bloque)
        {
            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return;
            }

            std::fill(discoFisico[bloque].begin(), discoFisico[bloque].end(), '\0');
            cacheDatos[bloque].clear(); // si no, la cache devolveria el contenido viejo
            marcarBloque(bloque, false);
        }

        //Operaciones para leer y borrar archivos

        //Devuelve el numero de inodo de un archivo del directorio abierto o -1 si no existe. Solo mira el nombre, no el tipo
        int inodoDeNombre(const std::string& nombre) const
        {
            for(size_t i = 0; i < directorio.size(); i++)
            {
                if(directorio[i].nombreExt2() == nombre)
                {
                    return directorio[i].numeroInodo;
                }
            }
            return -1;
        }

        //Junta en una lista todos los bloques de un archivo: primero los 12
        //directos y despues los que salen del bloque indirecto
        std::vector<int> bloquesDeArchivo(const InodoExt2& inodo) const
        {
            std::vector<int> bloques;

            for(int i = 0; i < 12; i++)
            {
                if(inodo.bloquesDirectos[i] != -1)
                {
                    bloques.push_back(inodo.bloquesDirectos[i]);
                }
            }

            if(inodo.bloqueIndirectoSimple != -1)
            {
                std::vector<int> punteros = leerBloqueIndirecto(inodo.bloqueIndirectoSimple);

                for(size_t i = 0; i < punteros.size(); i++)
                {
                    if(punteros[i] != -1)
                    {
                        bloques.push_back(punteros[i]);
                    }
                }
            }

            return bloques;
        }

        //Libera todos los bloques de un archivo, y tambien su bloque indirecto.
        //Deja el inodo vaciado, listo para reutilizarlo
        void liberarBloquesDelInodo(InodoExt2& inodo)
        {
            std::vector<int> bloques = bloquesDeArchivo(inodo);

            for(size_t i = 0; i < bloques.size(); i++)
            {
                liberarBloque(bloques[i]);
            }

            if(inodo.bloqueIndirectoSimple != -1)
            {
                liberarBloque(inodo.bloqueIndirectoSimple);
            }

            for(int i = 0; i < 12; i++)
            {
                inodo.bloquesDirectos[i] = -1;
                inodo.bloqueIndirectoSimple = -1;
                inodo.tamanobytes = 0;
            }
        }

        //Vuelve a leer del disco todas las entradas del directorio abierto.
        //Se usa despues de borrar una entrada, para que la cache L3 no quede
        //mostrando algo que ya no esta
        void recargarDirectorioAbierto()
        {
            directorio.clear();

            for(size_t i = 0; i < bloquesDirectorioAbierto.size(); i++)
            {
                std::vector<DirectorioExt2> entradas = leerEntradas(bloquesDirectorioAbierto[i]);

                for(size_t j = 0; j < entradas.size(); j++)
                {
                    directorio.push_back(entradas[j]);
                }
            }
        }

        //Saca una entrada de un bloque de directorio, reescribiendolo entero
        //sin esa linea. Las entradas son lineas de texto, asi que reachescribir
        //el bloque es facil
        bool eliminarEntradaDeBloque(int bloque, const std::string& nombre)
        {
            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return false;
            }

            //Juntamos el texto del bloque
            std::string texto;
            bool termine = false;

            for(size_t i = 0; i < discoFisico[bloque].size() && !termine; i++)
            {
                if (discoFisico[bloque][i] == '\0')
                {
                    termine = true;
                }
                else
                {
                    texto += discoFisico[bloque][i];
                }
            }

            //Recorremos linea por linea y vamos armando el bloque sin la que buscamos
            std::string limpio;
            std::string linea;
            bool removida = false;

            for(size_t i = 0; i <= texto.size(); i++)
            {
                if(i != texto.size() && texto[i] != '\n')
                { 
                    linea += texto[i]; continue;
                }

                //Fin de linea: la linea es "<numero> <nombre>"
                int numero = 0, leidos = 0;

                if(sscanf(linea.c_str(), "%d%n", &numero, &leidos) == 1)
                {
                    while(leidos < (int) linea.size() && linea[leidos] == ' ')
                    {
                        leidos++;
                    }

                    std::string nombreDeLinea = linea.substr(leidos);

                    if(nombreDeLinea == nombre)
                    {
                        removida = true; // la salteamos
                    }
                    else
                    {

                        limpio += linea;
                        limpio += "\n";
                    }
                }

                linea.clear();
            }

            if(!removida)
            {
                return false;
            }

            //Reescribimos el bloque con lo que quedo, y relleno con '\0'
            std::fill(discoFisico[bloque].begin(), discoFisico[bloque].end(), '\0');

            for(size_t i = 0; i < limpio.size() && i < discoFisico[bloque].size(); i++)
            {
                discoFisico[bloque][i] = limpio[i];
            }

            return true;
        }

        //Saca una entrada del directorio abierto: busca en cual de sus bloques esta, lo reescribe y recarga la cache. False si no la encuentra
        bool eliminarEntrada(const std::string& nombre)
        {
            for(size_t i = 0; i < bloquesDirectorioAbierto.size(); i++)
            {
                if(!eliminarEntradaDeBloque(bloquesDirectorioAbierto[i], nombre))
                {
                    continue;
                }

                recargarDirectorioAbierto(); // la cache L3 se entera del cambio
                return true;
            }

            return false;
        }

        //Bloque indirecto
        //Cuando un archivo necesita mas de 12 bloques, los que sobran no caben en
        //los punteros directos del inodo. Se guarda un bloque que contiene los
        //numeros de esos bloques, y el inodo apunta a ese bloque.

        //Cuantos punteros caben en un bloque indirecto. Cada puntero ocupa 4 bytes
        int capacidadBloqueIndirecto() const
        {
            return tamanoBloque / 4;
        }

        //Escribe un bloque indirecto completo. Los punteros de mas van en -1
        void escribirBloqueIndirecto(int bloque, const std::vector<int>& punteros)
        {
            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return;
            }

            std::string texto;
            char numero[16];

            for(size_t i = 0; i < punteros.size(); i++)
            {
                snprintf(numero, sizeof(numero), "%d ", punteros[i]);
                texto += numero;
            }

            std::fill(discoFisico[bloque].begin(), discoFisico[bloque].end(), '\0');

            for(size_t i = 0; i < texto.size() && i < discoFisico[bloque].size(); i++)
            {
                discoFisico[bloque][i] = texto[i];
            }
        }

        //Lee el contenido de un bloque indirecto: la lista de numeros de bloque
        std::vector<int> leerBloqueIndirecto(int bloque) const
        {
            std::vector<int> punteros;

            if (bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return punteros;
            }

            std::string texto;

            for(size_t i = 0; i < discoFisico[bloque].size(); i++)
            {
                if(discoFisico[bloque][i] == '\0')
                {
                    break;
                }

                texto += discoFisico[bloque][i];
            }

            int numero = 0, leidos = 0;
            const char* p = texto.c_str();

            while (*p != '\0')
            {
                if(sscanf(p, "%d%n", &numero, &leidos) != 1)
                {
                    break;
                }

                punteros.push_back(numero);
                p += leidos;

                while(*p == ' ')
                {
                    p++; // saltamos los espacios
                }
            }

            return punteros;
        }

        //Lee un inodo de la tabla de inodos del disco
        bool leerInodo(int numero, InodoExt2& inodo)
        {
            if(numero < 0 || (unsigned int) numero >= superbloque.totalInodos)
            {
                return false;
            }

            int bloque = bloqueDelInodo(numero);
            int pos = posicionDelInodo(numero);

            if(bloque >= (int) superbloque.inicioDatos)
            {
                return false; //se salio de la tabla
            }

            //Juntamos los 128 bytes del inodo, parando en el primer '\0'
            std::string texto;

            for(int i = 0; i < TAMANO_INODO; i++)
            {
                if (discoFisico[bloque][pos + i] == '\0')
                {
                    break;
                }

                texto += discoFisico[bloque][pos + i];
            }

            if(texto.empty())
            {
                return false; //el inodo nunca se escribio
            }

            int id = 0, tam = 0, tipo = 0, indirecto = 0;
            int directos[12];
            int leidos = sscanf(texto.c_str(),
                                "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                                &id, &tam, &tipo, &directos[0], &directos[1], &directos[2],
                                &directos[3], &directos[4], &directos[5], &directos[6],
                                &directos[7], &directos[8], &directos[9], &directos[10],
                                &directos[11], &indirecto);

            if(leidos != 16)
            {
                return false; //inodo corrupto
            }

            inodo.idInodo = id;
            inodo.tamanobytes = tam;
            inodo.esDirectorio = (tipo == 1);
            inodo.bloqueIndirectoSimple = indirecto;

            for(int i = 0; i < 12; i++)
            {
                inodo.bloquesDirectos[i] = directos[i];
            }

            return true;
        }

        //Entradas de directorio dentro de un bloque
        //Devuelve false si la entrada ya no cabe en el bloque
        bool escribirEntrada(int bloque, const DirectorioExt2& entrada, int* bytesEscritos = nullptr)
        {
            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return false;
            }

            char linea[512];
            int largo = snprintf(linea, sizeof(linea), "%d %s\n",entrada.numeroInodo, entrada.nombre.c_str());

            //Vemos cuanto texto hay ya escrito en el bloque
            size_t ocupado = 0;

            while(ocupado < discoFisico[bloque].size() && discoFisico[bloque][ocupado] != '\0')
            {
                ocupado++;
            }

            if(ocupado + largo + 1 > discoFisico[bloque].size())
            {
                return false; // no cabe
            }

            for(int i = 0; i < largo; i++)
            {
                discoFisico[bloque][ocupado + i] = linea[i];
            }

            //Si el bloque es del directorio abierto, la entrada tambien se
            //agrega a la cache, para que el disco y la memoria no se contradigan
            if(esBloqueDelDirectorioAbierto(bloque))
            {
                directorio.push_back(entrada);
            }

            if(bytesEscritos != nullptr)
            {
                *bytesEscritos = largo; // el inodo lo necesita
            }

            return true;
        }

        //Verifica si un bloque pertenece al directorio que esta abierto
        bool esBloqueDelDirectorioAbierto(int bloque) const
        {
            for(size_t i = 0; i < bloquesDirectorioAbierto.size(); i++)
            {
                if (bloquesDirectorioAbierto[i] == bloque)
                {
                    return true;
                }
            }

            return false;
        }

        //Lee todas las entradas que hay guardadas en un bloque de directorio
        std::vector<DirectorioExt2> leerEntradas(int bloque) const
        {
            std::vector<DirectorioExt2> entradas;

            if(bloque < 0 || (unsigned int) bloque >= discoFisico.size())
            {
                return entradas;
            }

            //Nos quedamos con el texto del bloque, hasta el primer '\0'
            std::string texto;

            for(size_t i = 0; i < discoFisico[bloque].size(); i++)
            {
                if(discoFisico[bloque][i] == '\0')
                {
                    break;
                }

                texto += discoFisico[bloque][i];
            }

            //Recorremos linea por linea
            size_t pos = 0;

            while(pos < texto.size())
            {
                size_t fin = texto.find('\n', pos);

                if(fin == std::string::npos)
                {
                    fin = texto.size();
                }

                std::string linea = texto.substr(pos, fin - pos);
                pos = fin + 1;

                if(linea.empty())
                {
                    continue;
                }

                DirectorioExt2 entrada;
                char nombre[256];
                int leidos = sscanf(linea.c_str(), "%d %255[^\n]", &entrada.numeroInodo, nombre);

                if (leidos == 2) // las dos cosas se pudieron leer
                {
                    entrada.nombre = nombre;
                    entradas.push_back(entrada);
                }
            }

            return entradas;
        }

        //Abre un directorio: lee su inodo, recorre sus bloques y llena el
        //vector directorio con todas las entradas que encuentra.
        bool abrirDirectorio(int numeroInodo)
        {
            if(numeroInodo == directorioActual) // ya esta en memoria, no vamos al disco
            {
                hitDirectorios++;
                return true;
            }

            InodoExt2 inodo;

            if(!obtenerInodo(numeroInodo, inodo))
            {
                std::cout << "Error: no existe el inodo " << numeroInodo << ".\n";
                return false;
            }

            if(!inodo.es_directorioExt2())
            {
                std::cout << "Error: el inodo " << numeroInodo << " es un archivo, no un directorio.\n";
                return false;
            }

            missDirectorios++;
            directorio.clear();
            bloquesDirectorioAbierto.clear();

            //Recorremos los bloques directos del directorio y juntamos sus entradas
            for(int i = 0; i < 12; i++)
            {
                if(inodo.bloquesDirectos[i] == -1)
                {
                    break; // los punteros libres van al final
                }

                bloquesDirectorioAbierto.push_back(inodo.bloquesDirectos[i]);

                std::vector<DirectorioExt2> entradas = leerEntradas(inodo.bloquesDirectos[i]);

                directorio.insert(directorio.end(), entradas.begin(), entradas.end());
            }

            directorioActual = numeroInodo;
            return true;
        }

        //Verifica si ya existe un archivo con ese nombre en el directorio abierto
        bool nombreExiste(const std::string& n) const
        {
            for(size_t i = 0; i < directorio.size(); i++)
            {
                if(directorio[i].nombreExt2() == n)
                {
                    return true;
                }
            }
            return false;
        }

        //Le suma bytes al tamanobytes del directorio abierto y lo guarda.
        //Sin esto el inodo de un directorio quedaria siempre en 0
        void sumarTamanoAlDirectorio(int bytes)
        {
            InodoExt2 inodo;

            if(!obtenerInodo(directorioActual, inodo))
            {
                return;
            }

            inodo.tamanobytes += bytes;
            escribirInodo(inodo);
        }

        //Agrega una entrada al directorio abierto. Si ningun bloque tiene lugar,
        //busca un bloque nuevo y lo cuelga del inodo del directorio
        bool agregarEntradaAlDirectorio(const DirectorioExt2& entrada)
        {
            int escritos = 0;

            for(size_t i = 0; i < bloquesDirectorioAbierto.size(); i++)
            {
                if(escribirEntrada(bloquesDirectorioAbierto[i], entrada, &escritos))
                {
                    sumarTamanoAlDirectorio(escritos);
                    return true;
                }
            }

            //Ningun bloque del directorio tiene lugar: hay que ampliarlo
            int nuevo = buscarBloqueLibre();

            if(nuevo == -1)
            {
                std::cout << "Error: disco lleno, no hay bloque para el directorio.\n";
                return false;
            }

            std::fill(discoFisico[nuevo].begin(), discoFisico[nuevo].end(), '\0');
            marcarBloque(nuevo, true);
            bloquesDirectorioAbierto.push_back(nuevo);

            InodoExt2 inodo;

            if(!obtenerInodo(directorioActual, inodo)) return false;

            //Buscamos el primer puntero libre del inodo del directorio
            for(int i = 0; i < 12; i++)
            {
                if(inodo.bloquesDirectos[i] == -1)
                {
                    inodo.bloquesDirectos[i] = nuevo;
                    escribirInodo(inodo); // el inodo del directorio tambien cambia

                    if(escribirEntrada(nuevo, entrada, &escritos))
                    {
                        sumarTamanoAlDirectorio(escritos);
                        return true;
                    }
                }
            }

            std::cout << "Error: el directorio no puede crecer mas.\n";
            return false;
        }

        //Crea un directorio nuevo dentro del directorio abierto.
        //Es el mismo camino que guardarArchivo(), pero en vez de escribir texto
        //en los bloques, le asigna un bloque vacio donde despues van las entradas
        bool crearDirectorio(const std::string& nombre)
        {
            if(nombre.empty())
            {
                std::cout << "Error: el nombre del directorio esta vacio.\n";
                return false;
            }

            if(!estaFormateado())
            {
                std::cout << "Error: el disco no esta formateado.\n";
                return false;
            }

            if(directorioActual == -1)
            {
                std::cout << "Error: no hay ningun directorio abierto.\n";
                return false;
            }

            if(nombreExiste(nombre))
            {
                std::cout << "Error: ya existe algo llamado '" << nombre << "'.\n";
                return false;
            }

            //buscar un inodo libre
            int numeroInodo = buscarInodoLibre();

            if(numeroInodo == -1)
            {
                std::cout << "Error: no quedan inodos libres.\n";
                return false;
            }

            //buscar un bloque para las entradas
            int bloqueNuevo = buscarBloqueLibre();

            if(bloqueNuevo == -1)
            {
                std::cout << "Error: disco lleno, no hay bloque para el directorio.\n";
                return false;
            }

            marcarBloque(bloqueNuevo, true);
            std::fill(discoFisico[bloqueNuevo].begin(), discoFisico[bloqueNuevo].end(), '\0');

            //escribir el inodo, que dice que es un directorio
            InodoExt2 inodo(numeroInodo, true);
            inodo.bloquesDirectos[0] = bloqueNuevo;
            inodo.tamanobytes = 0; //todavia no tiene ninguna entrada

            escribirInodo(inodo);
            marcarInodo(numeroInodo, true);

            //anotar el directorio dentro del padre
            DirectorioExt2 entrada;
            entrada.nombre = nombre;
            entrada.numeroInodo = numeroInodo;

            if(!agregarEntradaAlDirectorio(entrada))
            {
                //Deshacemos, ni el inodo ni el bloque quedan usados
                marcarInodo(numeroInodo, false);
                marcarBloque(bloqueNuevo, false);
                std::cout << "Error: no se pudo crear la entrada del directorio.\n";
                return false;
            }

            std::cout << "Directorio '" << nombre << "' creado. Inodo " << numeroInodo << ", bloque " << bloqueNuevo << ".\n";
            return true;
        }

        //Guarda un archivo completo en el sistema de archivos.
        
        bool guardarArchivo(const std::string& nombre, const std::string& texto,bool visible ,int inodoAReutilizar = -1){

            if(texto.empty())
            {
                std::cout << "Error: el contenido esta vacio.\n";
                return false;
            }

            if(!estaFormateado())
            {
                std::cout << "Error: el disco no esta formateado.\n";
                return false;
            }

            if(directorioActual == -1)
            {
                std::cout << "Error: no hay ningun directorio abierto.\n";
                return false;
            }

            
            //Si el nombre existe es un error. Salvo que estemos sobrescribiendo un archivo que ya existe
            if(nombreExiste(nombre) && inodoAReutilizar == -1)
            {
                std::cout << "Error: ya existe un archivo llamado '" << nombre << "'.\n";
                return false;
            }

            int numeroInodo = inodoAReutilizar;

            //buscar un inodo libre (o usar el que nos pasaron) 
            if(numeroInodo == -1)
            {
                numeroInodo = buscarInodoLibre();
            }
            else
            {
                //Vamos a sobrescribir: sus bloques viejos hay que liberarlos
                //primero, o se pierden y el archivo queda corrupto
                InodoExt2 viejo;

                if(!leerInodo(numeroInodo, viejo))
                {
                    return false;
                }

                liberarBloquesDelInodo(viejo);
                escribirInodo(viejo);
            }

            if(numeroInodo == -1)
            {
                std::cout << "Error: no quedan inodos libres.\n";
                return false;
            }

            //buscar los bloques libres y apuntarlos ---
            int bloquesNecesarios = cuantosBloquesNecesita(texto.size());
            std::vector<int> bloques; //numeros de bloque, en orden

            for(int i = 0; i < bloquesNecesarios; i++)
            {
                int libre = buscarBloqueLibre();

                if(libre == -1) //disco lleno a medio guardar: hay que deshacer
                {
                    for(size_t j = 0; j < bloques.size(); j++)
                    {
                        marcarBloque(bloques[j], false);
                    }

                    std::cout << "Error: no hay espacio suficiente en el disco.\n";
                    return false;
                }

                marcarBloque(libre, true);
                bloques.push_back(libre);
            }

            InodoExt2 inodo(numeroInodo, false); // es un archivo, no un directorio

            //Los primeros 12 bloques van en los punteros directos
            for(int i = 0; i < 12 && i < (int) bloques.size(); i++)
            {
                inodo.bloquesDirectos[i] = bloques[i];
            }

            //Si sobran bloques, sus numeros van dentro de un bloque indirecto
            int bloqueIndirecto = -1;

            if((int) bloques.size() > 12)
            {
                bloqueIndirecto = buscarBloqueLibre();

                if(bloqueIndirecto == -1)
                {
                    for(size_t j = 0; j < bloques.size(); j++) 
                    {
                        marcarBloque(bloques[j], false);
                    }

                    std::cout << "Error: no hay espacio para el bloque indirecto.\n";
                    return false;
                }

                marcarBloque(bloqueIndirecto, true);

                std::vector<int> punteros(bloques.begin() + 12, bloques.end());
                punteros.resize(capacidadBloqueIndirecto(), -1); // los sobrantes en -1

                escribirBloqueIndirecto(bloqueIndirecto, punteros);
                inodo.bloqueIndirectoSimple = bloqueIndirecto;
            }

            inodo.tamanobytes = texto.size();

            //Paso 3: escribir el texto en los bloques
            //El texto se corta en trozos de tamanoBloque. El ultimo bloque se
            //rellena con '\0', y al leer se corta en tamanobytes
            size_t pos = 0;

            for(size_t i = 0; i < bloques.size(); i++)
            {
                std::vector<char> bloque(tamanoBloque, '\0');

                for(size_t j = 0; j < tamanoBloque && pos < texto.size(); j++)
                {
                    bloque[j] = texto[pos];
                    pos++;
                }

                escribirBloque(bloques[i], bloque);
            }

            //escribir el inodo en el disco
            escribirInodo(inodo);
            marcarInodo(numeroInodo, true);

            //agregar la entrada al directorio, Si estamos sobrescribiendo, la entrada ya existe
            //y no hay que agregarla otra vez, o quedaria el nombre repetido
            if(inodoAReutilizar != -1)
            {
                std::cout << "Archivo '" << nombre << "' sobrescrito. Inodo " << numeroInodo << ", " << bloquesNecesarios << " bloque(s).\n";
                return true;
            }

            DirectorioExt2 entrada;
            entrada.nombre = nombre;
            entrada.numeroInodo = numeroInodo;

            if(!agregarEntradaAlDirectorio(entrada))
            {
                //Si no se pudo anotar, deshacemos todo lo hecho
                marcarInodo(numeroInodo, false);
                marcarBloque(bloqueIndirecto, false);

                for(size_t i = 0; i < bloques.size(); i++)
                {
                    marcarBloque(bloques[i], false);
                }

                std::cout << "Error: no se pudo crear la entrada del archivo.\n";
                return false;
            }

            if(visible)
            {

                std::cout << "Archivo '" << nombre << "' guardado. Inodo " << numeroInodo << ", " << bloquesNecesarios << " bloque(s).\n";
            }


            return true;
        }

        //Lee un archivo del directorio abierto y devuelve su contenido.
        //Es el camino inverso de guardarArchivo()
        std::string leerArchivo(const std::string& nombre)
        {
            if(!estaFormateado())
            {
                std::cout << "Error: el disco no esta formateado.\n";
                return "";
            }

            if(directorioActual == -1)
            {
                std::cout << "Error: no hay ningun directorio abierto.\n";
                return "";
            }

            //Buscar la entrada en el directorio abierto
            int numeroInodo = inodoDeNombre(nombre);

            if(numeroInodo == -1)
            {
                std::cout << "Error: no existe '" << nombre << "' en este directorio.\n";
                return "";
            }

            //Leer el inodo (pasa por la cache L1)
            InodoExt2 inodo;

            if(!obtenerInodo(numeroInodo, inodo))
            {
                std::cout << "Error: el inodo " << numeroInodo << " esta danado.\n";
                return "";
            }

            if(inodo.es_directorioExt2())
            {
                std::cout << "Error: '" << nombre << "' es un directorio, no un archivo.\n";
                return "";
            }

            //Recorrer los bloques y juntar los bytes
            //El texto se corta en tamanobytes, asi que el relleno con '\0' del
            //ultimo bloque no entra en el resultado
            std::vector<int> bloques = bloquesDeArchivo(inodo);
            std::string texto;
            texto.reserve(inodo.tamanobytes);

            for(size_t i = 0; i < bloques.size() && texto.size() < inodo.tamanobytes; i++)
            {
                std::vector<char> bloque = leerBloque(bloques[i]); // pasa por la cache L2

                for(size_t j = 0; j < bloque.size() && texto.size() < inodo.tamanobytes; j++)
                {
                    if(bloque[j] == '\0')
                    {
                        break; //relleno
                    }

                    texto += bloque[j];
                }
            }
            return texto;
        }

        //Borra un archivo del directorio abierto: libera sus bloques, su inodo
        //y saca la entrada del directorio. Los bloques del directorio que se
        //quedaron vacios se dejan asignados, como hace ext2 de verdad
        bool borrarArchivo(const std::string& nombre, bool visible)
        {
            if(!estaFormateado())
            {
                std::cout << "Error: el disco no esta formateado.\n";
                return false;
            }

            int numeroInodo = inodoDeNombre(nombre);

            if(numeroInodo == -1)
            {
                std::cout << "Error: no existe '" << nombre << "' en este directorio.\n";
                return false;
            }

            InodoExt2 inodo;

            if(!obtenerInodo(numeroInodo, inodo))
            {
                std::cout << "Error: el inodo " << numeroInodo << " esta danado.\n";
                return false;
            }

            if(inodo.es_directorioExt2())
            {
                std::cout << "Error: '" << nombre << "' es un directorio, no se puede borrar.\n";
                return false;
            }

            //Liberar los bloques de datos y el bloque indirecto
            liberarBloquesDelInodo(inodo);

            //Dejar el inodo libre y vaciado
            escribirInodo(inodo);
            marcarInodo(numeroInodo, false);

            //Sacar la entrada del directorio
            if(!eliminarEntrada(nombre))
            {
                std::cout << "Error: no se pudo quitar la entrada del directorio.\n";
                return false;
            }

            if(visible) 
            {
                std::cout << "Archivo '" << nombre << "' eliminado del disco.\n";
            }

            return true;
        }

        //Libera todo lo que hay adentro de un directorio
        void vaciarDirectorio(int numeroInodo)
        {
            InodoExt2 inodo;

            //leemos directo del disco, sin pasar por la cache, para no ensuciarla
            if(!leerInodo(numeroInodo, inodo))
            {
                return;
            }

            //Primero el contenido
            for (int i = 0; i < 12; i++)
            {
                if (inodo.bloquesDirectos[i] == -1)
                {
                    continue;
                }

                std::vector<DirectorioExt2> entradas = leerEntradas(inodo.bloquesDirectos[i]);

                for (size_t j = 0; j < entradas.size(); j++)
                {
                    InodoExt2 hijo;

                    if(!leerInodo(entradas[j].numeroInodo, hijo))
                    {
                        continue;
                    }

                    if(hijo.es_directorioExt2())
                    {
                        vaciarDirectorio(entradas[j].numeroInodo);
                    }

                    liberarBloquesDelInodo(hijo);
                    escribirInodo(hijo);
                    marcarInodo(entradas[j].numeroInodo, false);
                }
            }

            //Despues los bloques del propio directorio
            liberarBloquesDelInodo(inodo);
            escribirInodo(inodo);
            marcarInodo(numeroInodo, false);
        }

        //Borra un directorio del directorio abierto.
        bool borrarDirectorio(const std::string& nombre, bool recursivo = false)
        {
            if(!estaFormateado())
            {
                std::cout << "Error: el disco no esta formateado.\n";
                return false;
            }

            int numeroInodo = inodoDeNombre(nombre);

            if(numeroInodo == -1)
            {
                std::cout << "Error: no existe '" << nombre << "' en este directorio.\n";
                return false;
            }

            InodoExt2 inodo;

            if(!obtenerInodo(numeroInodo, inodo))
            {
                std::cout << "Error: el inodo " << numeroInodo << " esta danado.\n";
                return false;
            }

            if(!inodo.es_directorioExt2())
            {
                std::cout << "Error: '" << nombre << "' es un archivo, use borrarArchivo.\n";
                return false;
            }

            if(numeroInodo == 2)
            {
                std::cout << "Error: la raiz no se puede borrar.\n";
                return false;
            }

            if(numeroInodo == directorioActual)
            {
                std::cout << "Error: no se puede borrar el directorio abierto.\n";
                return false;
            }

            //Verificar si esta vacio
            bool vacio = true;
            std::vector<int> bloques = bloquesDeArchivo(inodo);

            for(size_t i = 0; i < bloques.size(); i++)
            {
                if (!leerEntradas(bloques[i]).empty())
                {
                    vacio = false;
                }
            }

            if(!vacio && !recursivo)
            {
                std::cout << "Error: '" << nombre << "' no esta vacio. " << "Use borrarDirectorio(nombre, true) para borrarlo con todo adentro.\n";
                return false;
            }

            //Borrar todo lo que tiene adentro, en cascada
            vaciarDirectorio(numeroInodo);

            //Sacar la entrada del padre
            if(!eliminarEntrada(nombre))
            {
                std::cout << "Error: no se pudo quitar la entrada del directorio.\n";
                return false;
            }

            std::cout << "Directorio '" << nombre << "' eliminado" << (vacio ? "." : " con todo su contenido.") << "\n";
            return true;
        }

        //Agrega texto al final de un archivo existente.
        bool agregarTexto(const std::string& nombre, const std::string& texto)
        {
            if(texto.empty())
            {
                std::cout << "Error: el texto a agregar esta vacio.\n";
                return false;
            }

            int numeroInodo = inodoDeNombre(nombre);

            if(numeroInodo == -1)
            {
                std::cout << "Error: no existe '" << nombre << "' en este directorio.\n";
                return false;
            }

            InodoExt2 inodo;

            if(!obtenerInodo(numeroInodo, inodo))
            {
                std::cout << "Error: el inodo " << numeroInodo << " esta danado.\n";
                return false;
            }

            if(inodo.es_directorioExt2())
            {
                std::cout << "Error: '" << nombre << "' es un directorio, no un archivo.\n";
                return false;
            }

            std::string contenidoActual = leerArchivo(nombre);

            return guardarArchivo(nombre, contenidoActual + texto, true, numeroInodo);
        }

        //Muestra cuantas veces se acerto y se fallo en cada nivel de cache.
        //Un fallo de cache no es un error: es ir a buscarlo al disco
        void estadisticasCache()
        {
            std::cout << "\n========== Estadisticas de cache ==========\n";

            size_t totalAcc = 0, totalFalla = 0;
            const char* nombres[3] = { "L1 inodos      ", "L2 datos       ", "L3 directorios " };
            size_t aciertos[3] = { hitInodos, hitDatos, hitDirectorios };
            size_t fallas[3] = { missInodos, missDatos, missDirectorios };

            for(int i = 0; i < 3; i++)
            {
                std::cout << nombres[i] << ": " << aciertos[i] << " aciertos, " << fallas[i] << " fallos";

                size_t total = aciertos[i] + fallas[i];

                if (total > 0)
                {
                    double porcentaje = (100.0 * aciertos[i]) / total;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.1f", porcentaje);
                    std::cout << "   (" << buf << "% de acierto)";
                }

                std::cout << "\n";
                totalAcc += aciertos[i];
                totalFalla += fallas[i];
            }

            std::cout << "-----------------------------------------\n";
            std::cout << "TOTAL: " << totalAcc << " aciertos, " << totalFalla << " fallos\n";
            std::cout << "=========================================\n";
        }

        //Muestra que hay cargado en cada cache ahora mismo
        void mostrarCaches()
        {
            std::cout << "\n========== Contenido de las caches ==========\n";

            std::cout << "\n--- L1: tablaInodos (" << tablaInodos.size() << " inodos) ---\n";

            if(tablaInodos.empty())
            {
                std::cout << "  (vacia)\n";
            }

            for(size_t i = 0; i < tablaInodos.size(); i++)
            {
                std::cout << "  inodo " << tablaInodos[i].idInodo << (tablaInodos[i].es_directorioExt2() ? "  [directorio]" : "  [archivo]  ") << "  " << tablaInodos[i].tamanoExt2() << " bytes  bloques: ";

                for(int j = 0; j < 12; j++)
                {
                    if(tablaInodos[i].bloquesDirectos[j] != -1)
                    {
                        std::cout << tablaInodos[i].bloquesDirectos[j] << " ";
                    }
                }

                if(tablaInodos[i].bloqueIndirectoSimple != -1)
                {
                    std::cout << "indirecto: " << tablaInodos[i].bloqueIndirectoSimple;
                }

                std::cout << "\n";
            }

            std::cout << "\n--- L2: cacheDatos ---\n";
            int enCache = 0;

            for(size_t i = 0; i < cacheDatos.size(); i++)
            {
                if(estaBloqueEnCache((int) i))
                {
                    enCache++;
                }
            }

            std::cout << "  bloques cargados: " << enCache << " de " << cacheDatos.size() << "\n";

            if(enCache == 0)
            {
                std::cout << "  (vacia)\n";
            }

            for(size_t i = 0; i < cacheDatos.size(); i++)
            {
                if(!estaBloqueEnCache((int) i))
                {
                    continue;
                }

                //Mostramos los primeros bytes de cada bloque cacheado
                std::string vista;
                bool termine = false;

                for(size_t j = 0; j < cacheDatos[i].size() && !termine && j < 30; j++)
                {
                    if (cacheDatos[i][j] == '\0')
                    {
                        termine = true;
                    }
                    else
                    {
                        vista += cacheDatos[i][j];
                    }
                }

                std::cout << "  bloque " << i << ": \"" << vista << "\"\n";
            }

            std::cout << "\n--- L3: directorio abierto ---\n";
            std::cout << "  directorio actual: " << (directorioActual == -1 ? "ninguno" : std::to_string(directorioActual)) << "\n";
            std::cout << "  entradas: " << directorio.size() << "\n";
            std::cout << "  bloques que lo forman: ";

            for(size_t i = 0; i < bloquesDirectorioAbierto.size(); i++)
            {
                std::cout << bloquesDirectorioAbierto[i] << " ";
            }

            std::cout << "\n";

            if(directorio.empty())
            {
                std::cout << "  (vacio)\n";
            }

            for(size_t i = 0; i < directorio.size(); i++)
            {
                std::cout << "  \"" << directorio[i].nombreExt2() << "\" -> inodo " << directorio[i].numeroInodo << "\n";
            }

            std::cout << "\n===========================================\n";
        }

        //Monta el disco: lee el superbloque del bloque 0 y lo carga en memoria
        bool montar()
        {
            if(!superbloque.leerDeDisco(discoFisico))
            {
                std::cout << "Error: el disco no esta formateado o esta danado.\n";
                return false;
            }

            //El disco tiene que tener la misma cantidad de bloques que dice el
            //superbloque. Si no, lo ajustamos, porque el resto de las funciones
            //se guian por el superbloque
            if(discoFisico.size() != (size_t) superbloque.totalBloques)
            {
                discoFisico.resize(superbloque.totalBloques, std::vector<char>(superbloque.bytesPorBloque, '\0'));
            }

            tamanoBloque = superbloque.bytesPorBloque;

            //Las caches de memoria no valen nada todavia: el superbloque recien
            //leido puede no coincidir con lo que habia antes
            limpiarCache();

            //Sin esto el listado sale vacio despues de montar
            if(!abrirDirectorio(2))
            {
                std::cout << "Error: no se pudo abrir el directorio raiz.\n";
                return false;
            }

            std::cout << "Disco montado: " << superbloque.totalBloques << " bloques de " << superbloque.bytesPorBloque << " bytes.\n";
            std::cout << "Raiz abierta: " << directorio.size() << " entrada(s) en " << bloquesDirectorioAbierto.size() << " bloque(s).\n";
            return true;
        }

        //Verifica si el disco tiene un superbloque valido en el bloque 0
        bool estaFormateado() const
        {
            return superbloque.esExt2(discoFisico);
        }

        //EDITAR (contrato): borra y vuelve a guardar, mismo camino que FAT
        bool editarArchivo(const std::string& nombre, const std::string& nuevoTexto)
        {
            if(nuevoTexto.empty())
            {
                return false;
            }
            if(!borrarArchivo(nombre,false))
            {
                return false;
            }

            return guardarArchivo(nombre, nuevoTexto,false);
        }

        //RENOMBRAR (contrato): quita la entrada y pone otra con el mismo inodo
        bool renombrarArchivo(const std::string& nombre, const std::string& nuevoNombre)
        {
            if(nuevoNombre.empty())
            {
                return false;
            }

            int numeroInodo = inodoDeNombre(nombre);

            if(numeroInodo == -1)
            {
                std::cout << "Error: archivo no encontrado.\n"; return false; 
            }

            if(nombreExiste(nuevoNombre)) 
            {
                std::cout << "Error: ya existe '" << nuevoNombre << "'.\n"; return false;
            }

            if(!eliminarEntrada(nombre))
            {
                return false;
            }

            DirectorioExt2 entrada;
            entrada.nombre = nuevoNombre;
            entrada.numeroInodo = numeroInodo;
            std::cout << "Archivo '" << nombre << "' renombrado a '" << nuevoNombre << "'.\n";
            return agregarEntradaAlDirectorio(entrada);
        }

        //DETALLES (contrato): nombre + ficha tecnica del inodo
        bool detallesArchivo(const std::string& nombre)
        {
            int numeroInodo = inodoDeNombre(nombre);

            if(numeroInodo == -1)
            {
                return false;
            }

            InodoExt2 inodo;

            if(!obtenerInodo(numeroInodo, inodo))
            {
                return false;
            }

            inodo.detallesExt2();
            return true;
        }

        //LISTAR (contrato): las entradas del directorio abierto
        void listarDirectorio()
        {
            if(directorio.empty())
            {
                std::cout << "El directorio esta vacio.\n";

                return;
            }

            for(auto& e : directorio)
            {
                std::cout << e.nombreExt2()<< "\n---\n";
            }
        }

        //ESTADISTICAS DISCO (contrato): bitmaps de bloques y de inodos
        void estadisticasDisco()
        {
            size_t libresB = 0, ocupadosB = 0, libresI = 0, ocupadosI = 0;
            for(unsigned int b = 0; b < superbloque.totalBloques; b++)
            {
                if(estaBloqueLibre((int)b))
                {
                    libresB++;
                }else
                {
                    ocupadosB++;
                }
            }

            for(unsigned int i = 0; i < superbloque.totalInodos; i++)
            {
                if (estaInodoLibre((int)i))
                {
                    libresI++;
                }else
                {
                    ocupadosI++;
                }
            }
            std::cout << "\n=== ESTADISTICAS DEL DISCO ===\n";
            std::cout << "Bloques: " << ocupadosB << " ocupados / " << libresB << " libres\n";
            std::cout << "Inodos:  " << ocupadosI << " ocupados / " << libresI << " libres\n";
            std::cout << "Entradas en el directorio abierto: " << directorio.size() << "\n";
        }


};



#endif

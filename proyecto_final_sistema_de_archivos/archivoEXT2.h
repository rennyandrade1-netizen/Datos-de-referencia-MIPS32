#ifndef ARCHIVO_EXT2_H
#define ARCHIVO_EXT2_H

#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>


//Directorio: solo guarda nombre del archivo y el numero del inodo

struct DirectorioExt2 
{
    std::string nombre;
    int numeroInodo;

    //Retorna el nombre del archivo
    std::string nombreExt2() const
    {
        return nombre;
    }

    //Edita el nombre del archivo
    void edit_nombreExt2(const std::string& t)
    {
        nombre = t;
    }

};

//El Inodo contiene toda la información técnica del archivo (menos el nombre)
class InodoExt2 
{
public:
    int idInodo;
    size_t tamanobytes;
    bool esDirectorio;  //true = el contenido son entradas de directorio
                        //false = el contenido es texto del archivo
    int bloquesDirectos[12];   //Guarda direcciones de vectores (bytes si esDirectorio == false\ lineas de entrada a directorios si esDirectorio == true)
    int bloqueIndirectoSimple; //Direccion a Espacio extra (directo al disco) por si los bloques directos no son suficientes, guarda direcciones a vectores

    //Inicializa InodoExt2
    InodoExt2(int id = -1, bool directorio = false) 
    {
        idInodo = id;
        tamanobytes = 0;
        esDirectorio = directorio;
        for (int i = 0; i < 12; i++)
        {
            bloquesDirectos[i] = -1;
        }
        bloqueIndirectoSimple = -1;
    }

    //Verifica si el inodo es un directorio o un archivo
    bool es_directorioExt2() const
    {
        return esDirectorio;
    }

    //Verifica si el archivo esta vacio
    bool es_vacioExt2() const
    {
        return tamanobytes == 0;
    }

    //Verifica si el inodo todavia no esta asignado a ningun archivo
    bool estaLibreExt2() const
    {
        return idInodo == -1;
    }

    //Retorna el tamano del archivo
    size_t tamanoExt2() const
    {
        return tamanobytes;
    }

    //Muestra los datos tecnicos del inodo (No olvidar que el Nombre esta en el Directorio, no en el Inodo)
    void detallesExt2() const
    {
        int usados = 0;

        for (int i = 0; i < 12; i++)
        {
            if (bloquesDirectos[i] != -1)
            {
                usados++;
            }
        }

        std::cout << "Inodo: " << idInodo << "\n";
        std::cout << "Tipo: " << (es_directorioExt2() ? "directorio" : "archivo") << "\n";
        std::cout << "Tamano: " << tamanoExt2() << " bytes\n";
        std::cout << "Bloques directos: " << usados << " de 12\n";
        std::cout << "Bloque indirecto simple: " << bloqueIndirectoSimple << "\n";
        std::cout << "Punteros: [";

        for (int i = 0; i < 12; i++)
        {
            std::cout << bloquesDirectos[i];

            if (i < 11) std::cout << ", ";
        }

        std::cout << "]\n";
    }


};

//Constantes del superbloque
const int TAMANO_INODO = 128;              //en Ext2 un inodo siempre mide 128 bytes
const unsigned short MAGIC_EXT2 = 0xEF53;   //numero magico: dice que esto es un Ext2
const int INODOS_RESERVADOS = 10;          //inodos 0 al 9 son del sistema:
                                          //el 0 no existe, el 1 es de bloques danados, el 2 es la raiz
const int TOTAL_INODOS = 64;               //cuantos inodos tiene el disco en total

//SuperbloqueExt2

//Es el bloque 0 del disco. Guarda la configuracion del sistema de archivos

class SuperbloqueExt2
{
    public:

        //Geometria del disco
        unsigned int totalBloques;       //cuantos bloques tiene el disco
        unsigned int bytesPorBloque;     //de cuantos bytes es cada bloque

        //Cantidades
        unsigned int inodosPorBloque;    //cuantos inodos entran en un bloque
        unsigned int totalInodos;        //cuantos inodos caben en todo el disco
        unsigned int bloquesTablaInodos; //cuantos bloques ocupa la tabla de inodos

        //Ubicacion de las estructuras
        unsigned int inicioBitmapBloques; //donde empieza el bitmap de bloques
        unsigned int inicioBitmapInodos;  //donde empieza el bitmap de inodos
        unsigned int inicioTablaInodos;   //donde empieza la tabla de inodos
        unsigned int inicioDatos;         //primer bloque libre para archivos

        //Identificacion
        unsigned short numeroMagico;     //0xEF53: dice que esto es un Ext2
        unsigned int estado;             //0 = limpio, 1 = sin desmontar


        //Inicializar Superbloque
        SuperbloqueExt2()
        {
            totalBloques = 0;
            bytesPorBloque = 0;
            inodosPorBloque = 0;
            totalInodos = 0;
            bloquesTablaInodos = 0;
            inicioBitmapBloques = 0;
            inicioBitmapInodos = 0;
            inicioTablaInodos = 0;
            inicioDatos = 0;
            numeroMagico = 0;
            estado = 0;
        }

        //Comprueba que la geometria pedira sea utilizable
  
        static bool geometriaValida(size_t bloques, size_t bytesBloque)
        {
            if (bytesBloque < TAMANO_INODO)
            {
                return false;
            }

            if (bytesBloque % TAMANO_INODO != 0)
            {
                return false;
            }

            if (bloques < 12)
            {
                return false;
            }

            return true;
        }

        //Constructor: calcula la geometria y la ubicacion de cada estructura
        SuperbloqueExt2(size_t bloques, size_t bytesBloque)
        {
            totalBloques = bloques;
            bytesPorBloque = bytesBloque;

            inodosPorBloque = bytesBloque / TAMANO_INODO; // 128 bytes por inodo
            totalInodos = TOTAL_INODOS;                      // inodos del disco
            bloquesTablaInodos = (totalInodos + inodosPorBloque - 1) / inodosPorBloque;

            // El bloque 0 es el superbloque, despues van los bitmaps y la tabla

            unsigned int cursor = 1;

            inicioBitmapBloques = cursor;
            cursor += (totalBloques + 7) / 8 / bytesPorBloque + 1;
            inicioBitmapInodos = cursor;
            cursor += (totalInodos + 7) / 8 / bytesPorBloque + 1;
            inicioTablaInodos = cursor;
            inicioDatos = inicioTablaInodos + bloquesTablaInodos;

            numeroMagico = MAGIC_EXT2;
            estado = 0; // limpio
        }

        //Convierte un bloque del disco en texto, hasta el primer '\0'
        static std::string bloqueATexto(const std::vector<char>& bloque)
        {
            std::string texto;

            for (size_t i = 0; i < bloque.size(); i++)
            {
                if (bloque[i] == '\0')
                {
                    break;
                }

                texto += bloque[i];
            }

            return texto;
        }

        //Escribe el superbloque en el bloque 0 del disco (al formatear).
        //El numero magico se escribe primero, asi se puede identificar rapido
        void guardarEnDisco(std::vector<std::vector<char>>& disco)
        {
            if (disco.empty())
            {
                return;
            }

            char buffer[256];

            snprintf(buffer, sizeof(buffer), "%u %u %u %u %u %u %u %u %u %u %u",
                     numeroMagico, totalBloques, bytesPorBloque,
                     inodosPorBloque, totalInodos, bloquesTablaInodos,
                     inicioBitmapBloques, inicioBitmapInodos, inicioTablaInodos,
                     inicioDatos, estado);

            for (size_t i = 0; i < disco[0].size(); i++)
            {
                disco[0][i] = '\0';
            }
        

            for (size_t i = 0; i + 1 < disco[0].size() && buffer[i] != '\0'; i++)
            {
                disco[0][i] = buffer[i];
            }
        }

        //Lee el superbloque del bloque 0 del disco (al montar)
        bool leerDeDisco(const std::vector<std::vector<char>>& disco)
        {
            if (disco.empty())
            {
                return false;
            } 

            unsigned short magic = 0;
            unsigned int leidos = 0;
            std::string texto = bloqueATexto(disco[0]);

            //Contamos cuantos campos hay, separados por espacio. Tiene que ser exactamente 11
            
            int campos = 0;
            bool dentroDeNumero = false;

            for (size_t i = 0; i < texto.size(); i++)
            {
                if (texto[i] == ' ')
                {
                    dentroDeNumero = false;
                }else 
                {
                    if (!dentroDeNumero)
                    {
                        dentroDeNumero = true;
                        campos++;
                    }
                }
            }

            if (campos != 11)
            {
                numeroMagico = 0;
                return false;
            }

            leidos = sscanf(texto.c_str(), "%hu %u %u %u %u %u %u %u %u %u %u",
                            &magic, &totalBloques, &bytesPorBloque,
                            &inodosPorBloque, &totalInodos, &bloquesTablaInodos,
                            &inicioBitmapBloques, &inicioBitmapInodos, &inicioTablaInodos,
                            &inicioDatos, &estado);

            if (leidos != 11) // no se pudo interpretar todo como numeros
            {
                numeroMagico = 0;
                return false;
            }

            //Si el numero magico no es el de ext2, el superbloque no es nuestro
            if (magic != MAGIC_EXT2)
            {
                numeroMagico = 0;
                return false;
            }

            numeroMagico = magic;
            return true;
        }

        //Verifica si el bloque 0 contiene un superbloque valido
        bool esExt2(const std::vector<std::vector<char>>& disco) const
        {
            if (disco.empty()) return false;

            unsigned short magic = 0;
            std::string texto = bloqueATexto(disco[0]);

            if (sscanf(texto.c_str(), "%hu", &magic) != 1) return false;

            return magic == MAGIC_EXT2;
        }

};


#endif

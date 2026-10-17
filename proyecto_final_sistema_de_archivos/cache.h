#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <string>
#include <algorithm>


//linea cache

class lineascache
{
    public:

        bool valido;             // Bit de validez (true/false)
        int tag;                 // Etiqueta de identificacion del bloque
        std::vector<char> datos; // Copia de los datos reales del bloque


        lineascache()           //inicia linea cache
        {
            valido = false;
            tag = 0;
        }


};


//Caches

class cache
{
    public:

        std::vector<lineascache> caches;
        size_t bytes;
        size_t tam;


        cache(size_t n, size_t tamBloque)     // inicializamos nuestra cache, es un vector de tamano "n"
        {
            size_t i;
            tam = n;
            caches.resize(tam);       
            bytes = tamBloque;


            for (i = 0; i < tam; i++) //creamos cada bloque y nos aseguramos de inicializar el vector datos con su tamanoo "16" bytes
            {
                caches[i] = lineascache();
                caches[i].datos.resize(bytes, '\0');
            }
        }
    

        void guardar_datos(size_t direccion, size_t bytesTotal, size_t& pos_texto, const std::string& text)
        {
            size_t i = 0;
            std::fill(caches[direccion].datos.begin(), caches[direccion].datos.end(), '\0'); //limpia las casillas primero
            while(i < bytes && pos_texto < bytesTotal)  //se guardara el texto hasta que se llene el bloque o termine el texto
            {                                           //pos_texto va por referencia para mantener el valor y seguir llenando en otro bloque
                caches[direccion].datos[i] = text[pos_texto];

                i++;
                pos_texto++;
            }

        }
    };
    
    #endif

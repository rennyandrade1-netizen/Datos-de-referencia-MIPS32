#ifndef ARCHI_FAT_H
#define ARCHI_FAT_H

#include <iostream>
#include <string>

//MetadatosFAT
class archivoFAT
{   
    public:

        std::string nombre;     //Nombre
        size_t tamanobytes;     //Tamano
        int primerbloque;       //Direccion del Primer Bloque


        archivoFAT(std::string nom = "Sin titulo")  //Constructor para crear un archivo
        {
            nombre = nom;
            tamanobytes = 0;
            primerbloque = -1;
        }


        bool es_vacioFAT()  //Verifica si el archivo esta vacio
        {
            return tamanobytes == 0;
        }


        std::string nombreFAT() //Retorna el nombre del Archivo
        {
            return nombre;
        }


        void edit_nombreFAT (const std::string& t) //Edita el Nombre del Archivo
        {
            nombre = t;
        }


        size_t tamanoFAT() //Retorna el tamano del Archivo
        {
            return tamanobytes;
        }


        void detallesFAT()  // Muestra el nombre del archivo, el tamano del archivo por conveniencia del Proyecto, la direccion del Primer Bloque
        {

            std::cout << "Tamano: " << tamanoFAT() << " bytes\n";
            std::cout << "Primer bloque: " << primerbloque << "\n";
        }

};

#endif

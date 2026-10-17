#include "Sistema_Arc_Ext2.h"
#include "Sistema_Arc_Fat.h"

#include <iostream>
#include <string>


//---prototipo del menu

void mostrarMenu();
void mostrarMenuHerraFAT();
void mostrarMenuHerraEXT2();
void mostrarMenuPrincipal();
int  pedirOpcion();
std::string pedirLinea(std::string aviso);
void herramientas(SistemaArchivosFAT &sistema);
void herramientas(SistemaArchivosExt2 &sistema);

//--- plantillas de las opciones
template <class T>
void opcionGuardar(T &sistema){

        std::string nombre = pedirLinea("Nombre del archivo: ");
        std::string texto = pedirLinea("contenido a guardar: ");
        sistema.guardarArchivo(nombre, texto, true);

}

template <class T>
void opcionLeer(T &sistema){

    std::string nombre = pedirLinea("Nombre del archivo: ");

    if (!sistema.nombreExiste(nombre)) {

        std::cout << "Error: Archivo '"<< nombre  << "' no encontrado.\n";

    }else {

        std::string contenido = sistema.leerArchivo(nombre);
        if (contenido != "") {

             std::cout << "--- contenido ---\n" << contenido << "\n-----------------\n";

        }

    }

}

template <class T>
void opcionEditar(T &sistema){

    std::string nombre = pedirLinea("Nombre del archivo: ");

    if (!sistema.nombreExiste(nombre)) {

        std::cout << "Error: Archivo '"<< nombre  << "' no encontrado.\n";

    }else{

        std::string texto = pedirLinea("Nuevo contenido: ");

        if (texto.empty()) {

            std::cout << "Edicion cancelada: nuevo contenido vacio.\n";

        }else {

            sistema.editarArchivo(nombre, texto);

        }

    }






}

template <class T>
void opcionBorrar(T &sistema){

    std::string nombre = pedirLinea("Nombre del archivo: ");

    if (!sistema.nombreExiste(nombre)) {

        std::cout << "Error: Archivo '"<< nombre  << "' no encontrado.\n";

    }else {

         sistema.borrarArchivo(nombre,true);

    }


}

template <class T>
void opcionListar(T &sistema){

    sistema.listarDirectorio();


}

template <class T>
void opcionDatalles(T &sistema){

    if(!sistema.detallesArchivo(pedirLinea("Nombre del archivo: "))){

        std::cout<<"Error: archivo no encontrado\n";

    }

}

template <class T>
void opcionRenombrar(T &sistema){

    std::string nombre = pedirLinea("Nombre actual: ");

    if (!sistema.nombreExiste(nombre)) {

        std::cout << "Error: Archivo '"<< nombre  << "' no encontrado.\n";

    }else {

        std::string nombre2 = pedirLinea("Nuevo nombre: ");
        sistema.renombrarArchivo(nombre,nombre2);

    }

}



// menu plantillas

template <class T>
void MenuSistema(T &sistema);

int main(){

    int modo = -1;

    SistemaArchivosFAT  fat(100,16);
    SistemaArchivosExt2 etx2(100,256);
    etx2.formatear(true);

    do {

        mostrarMenuPrincipal();
        modo = pedirOpcion();

        switch (modo) {

            case 1:
                MenuSistema(fat);
                break;

            case 2:

                MenuSistema(etx2);

                break;

            case 0:
                std::cout << "Saliendo del programa...\n\n";
                break;

            default:
                std::cout<< "Opcion no valida.\n";

        }

    }while (modo != 0);



    return 0;
}

void mostrarMenu(){

    std::cout<< "\n========== MENU ==========\n";
    std::cout<< "1. Crear un archivo\n";
    std::cout<< "2. Ver todos los archivos\n";
    std::cout<< "3. Leer un archivo\n";
    std::cout<< "4. Editar un archivo\n";
    std::cout<< "5. Renombrar un archivo\n";
    std::cout<< "6. Borrar un archivo\n";
    std::cout<< "7. Detalles un archivo\n";
    std::cout<< "8. Estadisticas del disco\n";
    std::cout<< "9. Herramientas del sistema\n";
    std::cout<< "0. Salir del sistema\n";
    std::cout<< "==========================\n";


}

void mostrarMenuHerraFAT(){
    
    std::cout<< "\n===== HERRAMIENTAS DEL FAT =====\n";
    std::cout<< "1. Ver tabla\n";
    std::cout<< "2. Ver caches\n";
    std::cout<< "3. Estadisticas de la caches\n";
    std::cout<< "0. Volver\n";
    std::cout<< "================================\n";

}

void mostrarMenuHerraEXT2(){

    std::cout << "\n======= HERRAMIENTAS DEL EXT2 =======\n";
    std::cout << "1. Formatear disco (borra todo)\n";
    std::cout << "2. Montar disco\n";
    std::cout << "3. Crear directorio\n";
    std::cout << "4. Borrar directorio\n";
    std::cout << "5. Abrir directorio (navegar)\n";
    std::cout << "6. Agregar texto a un archivo (append)\n";
    std::cout << "7. Ver caches (inodos/datos/directorio)\n";
    std::cout << "8. Estadisticas de caches\n";
    std::cout << "0. Volver\n";
    std::cout << "=====================================\n";

}


void herramientas(SistemaArchivosFAT &sistema){

    int op = -1;

    do {

        mostrarMenuHerraFAT();
        op = pedirOpcion();

        switch (op) {

            case 1:
                sistema.mostrarFAT();
                break;
            case 2:
                sistema.mostrarCaches();
                break;
            case 3:
                sistema.estadisticasCache();
                break;
            case 0:
                break;
            default:
                std::cout<<"Opcion no valida, intente de nuevo.\n";

        }

    }while (op != 0);

}

void herramientas(SistemaArchivosExt2 &sistema){

    int op = -1;

    do {

        mostrarMenuHerraEXT2();
        op = pedirOpcion();

        switch (op) {

            case 1:
                sistema.formatear(false);
            break;

            case 2:
                sistema.montar();
            break;

            case 3:
                sistema.crearDirectorio(pedirLinea("Nombre del directorio: "));
            break;

            case 4: {
                std::string nombre = pedirLinea("Nombre del directorio: ");
                std::string rec = pedirLinea("Borrar tambien su contenido? (s/n): ");
                sistema.borrarDirectorio(nombre, rec == "s");
                break;
            }

            case 5: {
                std::string nombre = pedirLinea("Nombre del directorio (vacio = raiz): ");
                if (nombre.empty()) { sistema.abrirDirectorio(2); break; }
                int inodo = sistema.inodoDeNombre(nombre);
                if (inodo == -1) std::cout << "Error: no existe '" << nombre << "'.\n";
                else sistema.abrirDirectorio(inodo);
                break;
            }

            case 6:
                sistema.agregarTexto(pedirLinea("Nombre del archivo: "),pedirLinea("Texto a agregar: "));
                break;

            case 7:
                sistema.mostrarCaches();
                break;

            case 8:
                sistema.estadisticasCache();
                break;

            case 0:
                std::cout<<"Saliendo de las herramientas.\n\n";
                break;

            default:
                std::cout<<"Opcion no valida, intente de nuevo.\n";

        }

    }while (op != 0);

}

void mostrarMenuPrincipal(){

    std::cout<< "\n===== MODO DE LA SIMULACION =====\n";
    std::cout<< "1. Modo Fat\n";
    std::cout<< "2. Modo Ext2\n";
    std::cout<< "0. salir\n";
    std::cout<< "=================================\n";

}

int  pedirOpcion(){

    int opci;

    std::cout<< "Eliege una opcion: ";

    if(!(std::cin >> opci)){

        std::cin.clear();
        std::cin.ignore(10000,'\n');
        opci=-1;

    }

    std::cin.ignore(10000,'\n');
    return opci;
}


std::string pedirLinea(std::string aviso){

    std::string texto;

    std::cout << aviso;
    std::getline(std::cin,texto);


    return texto;
}


//-----

template <class T>
void MenuSistema(T &sistema){

    int opci=-1;

    do {

        mostrarMenu();
        opci=pedirOpcion();

        switch (opci) {

            case 1:

                opcionGuardar(sistema);

                break;

            case 2:

                opcionListar(sistema);

                break;

            case 3:

               opcionLeer(sistema);

                break;

            case 4:

                opcionEditar(sistema);

                break;

            case 5:
                
                opcionRenombrar(sistema);

                break;

            case 6:

                opcionBorrar(sistema);

                break;

            case 7:

                opcionDatalles(sistema);

                break;

            case 8:

                sistema.estadisticasDisco();

                break;

            case 9:

                herramientas(sistema);

                break;


            case 0:
                std::cout<< "Saliendo de la simulacion...\n\n";
                break;

            default:
                std::cout << "Opcion no valida, intente de nuevo.\n";

        }

    }while (opci != 0);

}



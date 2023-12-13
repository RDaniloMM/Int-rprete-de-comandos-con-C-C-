#include <iostream>
#include <queue>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>

#define ROJO     "\x1b[31m"
#define VERDE   "\x1b[32m"
#define AMARILLO  "\x1b[33m"
#define AZUL    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"
#define MAX_COMMAND_LENGTH 8192
#define MAX_ARGUMENTS 32
using namespace std;

//(com (-opcs)* (args)* (opr in/outfile?)*)*
//Ejemplo:
//mkdir Prueba && cd Prueba && touch proy.txt in.txt && ls -al > proy.txt && ./prueba <in.txt >out.txt
/*
Lista de operadores de control:
    Operadores de redireccion:
        Solo se acepta uno de ellos a la vez, se evalúa el caso.
        >
        <
        |
        >>
    Operadores de encadenamiento:
        ;
        &&
        ||
*/

class Comando{
private:
    //char *outfile;
    //char *inputfile;
    char dir[PATH_MAX];
    char hostName[HOST_NAME_MAX];
    int redireccionamiento = 0;
    passwd *user;
    queue<char **> coms; //cola de comandos a ejecutarse
    queue<char **> direccion;
    char str[MAX_COMMAND_LENGTH]; //comando de entrada
    //int background;

public:
    Comando();
    void prompt();
    void divide();
    void execute();
    int casoredireccion(char *);
    int casoencadenamiento(char *);
    void ver_comandos();
};

Comando::Comando(){
    gethostname(hostName, HOST_NAME_MAX);
    uid_t uid = geteuid();
    user = getpwuid(uid);
}

void Comando::prompt(){
    getcwd(dir, PATH_MAX); //obtiene el nombre del directorio actual
    cout << VERDE << user->pw_name << "@" << hostName << RESET << ":" << AZUL << dir << RESET << "[ESIS]$ ";
    fflush(stdout);
    cin.getline(str, MAX_COMMAND_LENGTH);
    if(!strcmp(str, "salir")) exit(0);
}
//comando 
void Comando::divide(){
    // se divide la linea en argumentos, args[0] es el comando simple
    int n = 0, casoencad, m = 0;
    bool hayencad = 0;
    char **comsimple = new char *[MAX_ARGUMENTS];
    char **direc = new char *[MAX_ARGUMENTS];

    char *token = strtok(str, " ");
    while(token != NULL && n < MAX_COMMAND_LENGTH){
        if((casoencad = casoredireccion(token)) == 2){ // Analiza si el Token es igual a "<" (value 2) para el redireccionamiento
           // Objetivo del if es dividir el comando en 2 partes "programa" < "archivo"
            comsimple[n] = NULL; // asigna valor Null al ultimo valor de la PRIMERA PARTE
            token = strtok(NULL, " ");
            direc[m++] = token; // asigna el nombre de archivo (token) en direc;
            redireccionamiento = 2; // condicion para saber si se encontro el redireccionamiento
        }else if(!(casoencad = casoencadenamiento(token))){ //si no es un opr de encad, se sigue guardando como comando simple
            comsimple[n++] = token;
            hayencad = 0;
        }
        else{//caso contrario, se acaba el comando simple y se guarda en la cola de comandos
            hayencad = 1;
            comsimple[n] = NULL;
            coms.push(comsimple);
            n = 0;
            comsimple = new char *[MAX_ARGUMENTS];
        }
        token = strtok(NULL, " ");
    }
    if(!hayencad){
        comsimple[n] = NULL;
        coms.push(comsimple);
        if(redireccionamiento == 2){
            direc[m] = NULL; // asigno al final de la segunda parte NULL para fin de cadena
            direccion.push(direc); // el nombre de fichero se plasma en el atributo direccion
        }

    }
}

void Comando::execute(){
    while(!coms.empty()){
        if(strcmp(coms.front()[0], "cd") == 0){
            if(chdir(coms.front()[1]) == -1){
                cout << "Error al cambiar de directorio\n";
            }
        }
        else{
            pid_t child_pid = fork();

             //Duplica el proceso actual y retorna un valor segun sea el proceso
            if(child_pid < 0){ //Error
                cout << "Error al crear un proceso hijo" << endl;
                exit(1);
            }
            else if(child_pid == 0){ //Proceso hijo
                if (redireccionamiento == 2){ // Si se encontro el redireccionamiento
                    int fd = open(direccion.front()[0], O_RDONLY); // Abro el archivo (segunda parte) guardado en direccion con permiso para solo leer 
                    if (fd < 0) {
                        cerr << "Error al abrir el archivo de entrada" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDIN_FILENO); // lee el archivo en lugar del teclado y lo reserva
                    close(fd); // cierra el archivo
                }
                execvp(coms.front()[0], coms.front()); //Ejecuta el comando con sus argumentos
                cout << "Error al ejecutar el comando\n"; //cuando el comando se ejecuta correctamente no se muestra esta línea
                exit(1);
            }
            else{// Proceso padre
                wait(NULL); //Espera a que el proceso hijo termine
            }
        }
        coms.pop();
    }
}


void Comando::ver_comandos(){
    queue<char **>q = coms;
    while(!q.empty()){
        int i = 0;
        while(q.front()[i] != NULL){
            cout << q.front()[i++] << " ";
        }
        cout << endl;
        q.pop();
    }
    cout << endl;
}

int Comando::casoredireccion(char *token){//verifica si un token es un operador de redireccion
    if(!strcmp(token, ">")) return 1;
    else if(!strcmp(token, "<")) return 2;
    else if(!strcmp(token, ">>")) return 3;
    else if(!strcmp(token, "|")) return 4;
    else return 0;
}

int Comando::casoencadenamiento(char *token){ //no deberia necesitar de un espacio...
    if(!strcmp(token, "&&")) return 1;
    else if(!strcmp(token, "||")) return 2;
    else if(!strcmp(token, ";")) return 3;
    else return 0;
}

int main(){
    Comando comando;
    while(true){
        comando.prompt();
        comando.divide();
        //comando.ver_comandos();
        comando.execute();
    }
    return 0;
}

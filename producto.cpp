#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>

#define ROJO     "\x1b[31m"
#define VERDE   "\x1b[32m"
#define AMARILLO  "\x1b[33m"
#define AZUL    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"
#define MAX_COMMAND_LENGTH 256
#define MAX_ARGUMENTS 32
using namespace std;

//(com (-opcs)* (args)* (opr in/outfile?)*)*
//Ejemplo:
//cd Proyectos/ac && touch proy.txt in.txt && ls -al > proy.txt && ./prog <in.txt >out.txt
/*
Lista de operadores de control:
    Operadores de redireccion:
        >
        <
        >>
        |
    Operadores de encadenamiento:
        ;
        &&
        ||
        & (tal vez)
*/

class Comando{
private:
    int numArgs = 0;
    //char *outfile;
    //char *inputfile;

    char dir[PATH_MAX];
    char hostName[HOST_NAME_MAX];

    uid_t uid;
    passwd *user;

    char command[MAX_COMMAND_LENGTH];
    char *args[MAX_ARGUMENTS];
    //char *errfile;
    //int background;

public:
    Comando();
    void prompt();
    void divide();
    void execute();
    friend int casoredireccion(char *);
    friend void ver_comandos(Comando &);
    //void print();
    //void clr();
};

Comando::Comando(){
    gethostname(hostName, HOST_NAME_MAX);
    uid = geteuid();
    user = getpwuid(uid);
}

void Comando::prompt(){
    getcwd(dir, PATH_MAX); //obtiene el nombre del directorio actual
    cout << VERDE << user->pw_name << "@" << hostName << RESET << ":" << AZUL << dir << RESET << "[ESIS]$ ";
    fflush(stdout);
    cin.getline(command, MAX_COMMAND_LENGTH);
    if(!strcmp(command, "salir")) return;
}

void Comando::divide(){
    // se divide la linea en argumentos, args[0] es el comando simple
    char *token = strtok(command, " ");
    while(token != NULL && numArgs < MAX_ARGUMENTS){
        //int casoredir = casoredireccion(token);
        args[numArgs] = token;
        token = strtok(NULL, " ");
        numArgs++;
        //cout << casoredir << endl;
    }
    args[numArgs] = NULL; //el ultimo debe ser un NULL
}

void Comando::execute(){
    if(numArgs > 0){ //debe existir por lo menos el comando
        if(strcmp(args[0], "cd") == 0){
            if(numArgs == 2){ //el primero es "cd" y el segundo la ruta.
                if(chdir(args[1]) == -1){ //chdir retorna -1 cuando hay un error al cambiar de directorio
                    cout << "Error al cambiar de directorio\n";
                }
            }
            else{//hay más de 2 argumentos
                //cout << "Numero de args: " << numArgs << endl;
                cout << "Uso incorrecto: cd directorio\n";
            }
        }

        else{
            pid_t child_pid = fork(); //Duplica el proceso actual y retorna un valor segun sea el proceso

            if(child_pid < 0){ //Error
                cout << "Error al crear un proceso hijo" << endl;
                exit(1);
            }
            else if(child_pid == 0){ //Proceso hijo
                execvp(args[0], args); //Ejecuta el comando con sus argumentos
                cout << "Error al ejecutar el comando\n"; //cuando el comando se ejecuta correctamente no se muestra esta línea
                exit(1);
            }
            else{// Proceso padre
                wait(NULL);  // Espera a que el proceso hijo termine
            }
        }
        numArgs = 0;
    }
}


void ver_comandos(Comando &c){
    int i = 0;
    while(c.args[i] != NULL){
        cout << c.args[i] << endl;
        ++i;
    }
}

int casoredireccion(char *token){//verifica si un token es un operador de redireccion
    if(!strcmp(token, ">")) return 1;
    else if(!strcmp(token, "<")) return 2;
    else if(!strcmp(token, ">>")) return 3;
    else if(!strcmp(token, "|")) return 4;
    else return 0;
}

int main(){
    Comando comando;
    while(true){
        comando.prompt();
        comando.divide();
        comando.execute();
    }
    return 0;
}

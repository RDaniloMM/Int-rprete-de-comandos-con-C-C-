#include <iostream>
#include <queue>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>  // Necesario para la redirección de salida

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
    passwd *user;
    queue<char **> coms; //cola de comandos a ejecutarse
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
    int n = 0, casoencad;
    bool hayencad = 0;
    char **comsimple = new char *[MAX_ARGUMENTS];

    char *token = strtok(str, " ");
    while(token != NULL && n < MAX_COMMAND_LENGTH){
        if(!(casoencad = casoencadenamiento(token))){ //si no es un opr de encad, se sigue guardando como comando simple
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
            if(child_pid < 0){
                cout << "Error al crear un proceso hijo" << endl;
                exit(1);
            }
            else if(child_pid == 0){
                int out_redirect = -1;  // Descriptor de archivo para la redirección de salida

                // Buscar operador de redirección de salida ">"
                for(int i = 0; coms.front()[i] != NULL; ++i){
                    if(casoredireccion(coms.front()[i]) == 1){  // 1 representa ">"
                        out_redirect = i + 1;  // El siguiente token después de ">" es el nombre del archivo de salida
                        break;
                    }
                }

                if(out_redirect != -1){
                    // Redirigir la salida a un archivo
                    int fd_out = open(coms.front()[out_redirect], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if(fd_out == -1){
                        perror("Error al abrir el archivo de salida");
                        exit(1);
                    }
                    dup2(fd_out, STDOUT_FILENO);  // Redirigir STDOUT al archivo
                    close(fd_out);  // Cerrar el descriptor de archivo duplicado
                }

                execvp(coms.front()[0], coms.front());
                cout << "Error al ejecutar el comando\n";
                exit(1);
            }
            else{
                wait(NULL);
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
    else if(!strcmp(token, "|")) return 3;
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

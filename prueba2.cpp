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

class Comando{
private:
    char dir[PATH_MAX];
    char hostName[HOST_NAME_MAX];
    passwd *user;
    queue<pair<char **, int>> coms; //cola de comandos a ejecutarse
    queue<char *> direccion; //cola de archivos o argumentos
    char str[MAX_COMMAND_LENGTH]; //comando de entrada
    vector<int> mod; //vector de redirecciones
public:
    Comando();
    void prompt();
    void divide();
    void execute();
    int casoredireccion(char *);
    int casoencadenamiento(char *);
    void processRed();
};

Comando::Comando(){
    gethostname(hostName, HOST_NAME_MAX);
    uid_t uid = geteuid();
    user = getpwuid(uid);
}

void Comando::prompt(){
    getcwd(dir, PATH_MAX); // obtiene la direccion actual
    string homeDir(user->pw_dir); // directorio home del usuario
    string dirStr(dir);

    //encuentra homeDir dentro de dir (directorio completo actual) y reemplaza /home/usuario por ~
    size_t pos = dirStr.find(homeDir);
    if(pos != string::npos){
        dirStr.replace(pos, homeDir.length(), "~");
    }
    cout << VERDE << user->pw_name << "@" << hostName << RESET << ":" << AZUL << dirStr << RESET << "[ESIS]$ ";
    fflush(stdout);
    cin.getline(str, MAX_COMMAND_LENGTH);
    if(!strcmp(str, "salir")) exit(0);
}

void Comando::divide(){
    // se divide la linea en argumentos, args[0] es el comando simple
    int n = 0, casoencad, casoredir = 0;
    bool haydirec = 0;

    char **comsimple = new char *[MAX_ARGUMENTS]; //Se encarga de reunir los comandos token por token
    char *token = strtok(str, " "); //Asigna a cada token los valores de ingreso del str separados por " " espacio
    while(token != NULL && n < MAX_COMMAND_LENGTH){
        casoredir = 0; // Regresa a 0 para volver a empezar denuevo a analizar el redireccionamiento
        if(!(casoencad = casoencadenamiento(token))){ //si no es un opr de encad, se sigue guardando como comando simple
            casoredir = casoredireccion(token); //extrae el valor 0 si no hay redireccion , 1 si encuentra > , 2 si encuntra <
            if(casoredir){ // caso encuentre redireccion
                haydirec = 1; //cambia la variable a que si encontro redireccion
                token = strtok(NULL, " "); //pasa al siguiente token obviando ">,<" para que token almacene el nombre del archivo
                comsimple[n] = NULL; //pone limite al conjunto de tokens antes de ">,<" almacenados en comsimple
                coms.push({comsimple,casoredir}); // se inserta todo el comando antes de ">,<" y que tipo de redireccion es (casoredir);
                direccion.push(token);// almacena el nombre del archivo que va despues de ">,<" gracias a que se uso la linea 80
                mod.push_back(casoredir);// De acuerdo a la posicion del comando esto almacenara si es una redireccion o un comando simple
            }
            else{
                comsimple[n++] = token; // guarda cada token dentro de comsimple
            }
        }
        else{//caso contrario, se acaba el comando simple y se guarda en la cola de comandos
            comsimple[n] = NULL; // pone limite al conjunto de tokens almacenados
            if(!haydirec){ // consulta si anterior a ello se encontro un direccionamiento porque si encontro se volveria a repetir la linea 94
                coms.push({comsimple,casoredir});// caso que no encontro redireccionamiento significa que la linea 82 no fue ejecutada, por ende necesita ejecutarse para guarda el comando simple
                mod.push_back(0);
            }
            haydirec = 0;
            n = 0;
            comsimple = new char *[MAX_ARGUMENTS];
        }

        token = strtok(NULL, " ");
    }
    comsimple[n] = NULL;
    if(!haydirec){
        coms.push({comsimple,casoredir});
        mod.push_back(0);
    }
}

void Comando::execute(){
    while(!coms.empty()){
        if(strcmp(coms.front().first[0], "cd") == 0){
            if(chdir(coms.front().first[1]) == -1){
                cout << "Error al cambiar de directorio\n";
            }
        }
        else{
            pid_t child_pid = fork(); //Duplica el proceso actual y retorna un valor segun sea el proceso
            if(child_pid < 0){ //Error
                cout << "Error al crear un proceso hijo" << endl;
                exit(1);
            }
            else if(child_pid == 0){ //Proceso hijo
                processRed();
                execvp(coms.front().first[0], coms.front().first); //Ejecuta el comando con sus argumentos
                cout << "Error al ejecutar el comando\n"; //cuando el comando se ejecuta correctamente no se muestra esta lÃ­nea
                exit(1);
            }
            else{// Proceso padre
                wait(NULL); //Espera a que el proceso hijo termine
            }
            if(!direccion.empty() && (mod[0] > 0)){
                direccion.pop();
            }

        }
        mod.erase(mod.begin());
        coms.pop();
    }
}

void Comando::processRed(){ // Analiza si se ejecuto alguna redireccion para el proceso de manejado de archivo
    if(coms.front().second == 2){ // Si se encontro el redireccionamiento "<"
        int fd = open(direccion.front(), O_RDONLY); // Abro el archivo (segunda parte) guardado en direccion con permiso para solo leer
        if(fd < 0){
            cerr << "Error al abrir el archivo de entrada" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO); // lee el archivo en lugar del teclado y lo reserva
        close(fd); // cierra el archivo
    }
    else if(coms.front().second == 1){
        int fd = open(direccion.front(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }else if(coms.front().second == 3){
        int fd = open(direccion.front(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);       
    }
}

int Comando::casoredireccion(char *token){//verifica si un token es un operador de redireccion

    if(!strcmp(token, ">")) return 1;
    else if(!strcmp(token, "<")) return 2;
    else if(!strcmp(token, ">>" )) return 3;
    else return 0;
}

int Comando::casoencadenamiento(char *token){
    if(!strcmp(token, ";")) return 1;
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

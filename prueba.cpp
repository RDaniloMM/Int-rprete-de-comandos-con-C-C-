#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <vector>
#include <queue>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
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
#define READ_END    0    
#define WRITE_END   1    
using namespace std;

// Simular la función getch en Linux
char obtenerTecla(){
    int buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if(tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if(tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if(read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if(tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return (buf);
}

class Comando{
private:
    char dir[PATH_MAX];
    char hostName[HOST_NAME_MAX];
    passwd *user;
    queue<pair<char **, int>> coms; //cola de comandos a ejecutarse
    queue<char *> direccion; //cola de archivos o argumentos
    char str[MAX_COMMAND_LENGTH]; //comando de entrada
    vector<string> historial_comandos; // para ver el historial de comandos
public:
    Comando();
    void ingresar_comando(Comando comando);
    void prompt();
    void divide();
    void execute();
    int casoredireccion(char *);
    int casoencadenamiento(char *);
    void processRed();
    void Pipe();
};

Comando::Comando(){
    gethostname(hostName, HOST_NAME_MAX);
    uid_t uid = geteuid();
    user = getpwuid(uid);
}

void Comando::ingresar_comando(Comando comando){

    fflush(stdout);
    int index = 0;
    int tamanio_historialc = historial_comandos.size();
    comando.prompt();

    while((str[index] = obtenerTecla()) != '\n'){     // Leer caracter a caracter ingresado
        if(str[index] == '\e'){
            char siguiente = obtenerTecla();
            char tercero = obtenerTecla();

            if(siguiente == '[' && tercero == 'A'){       // Si se presiona la flecha arriba se obtiene el anterior comando ingresado
                cout << "\033[2K\r" << flush; // Borra la línea completa y retrocede al principio
                comando.prompt();
                index = 0;

                // Casos que se dan cuando retrocedemos en el historial de comandos
                if((tamanio_historialc - 1) != -1){
                    cout << historial_comandos[--tamanio_historialc];
                    strcpy(str, historial_comandos[tamanio_historialc].c_str());
                    index = historial_comandos[tamanio_historialc].size();
                }
                else if(!historial_comandos.empty() && (tamanio_historialc - 1) == -1){ // se encuentra en el indice 0 del vector historial_comandos
                    cout << historial_comandos[tamanio_historialc];
                    strcpy(str, historial_comandos[tamanio_historialc].c_str());
                    index = historial_comandos[tamanio_historialc].size();
                }
            }
            else if(siguiente == '[' && tercero == 'B'){  // Si se presiona la flecha abajo se obtiene el posterior comando ingresado
                cout << "\033[2K\r" << flush; // Borra la línea completa y retrocede al principio
                comando.prompt();
                index = 0;

                // Casos que se dan cuando avanzamos en el historial de comandos
                if(!historial_comandos.empty() && (tamanio_historialc + 1) != historial_comandos.size()){
                    cout << historial_comandos[++tamanio_historialc];
                    strcpy(str, historial_comandos[tamanio_historialc].c_str());
                    index = historial_comandos[tamanio_historialc].size();
                }
                else if(!historial_comandos.empty() && (tamanio_historialc + 1) == historial_comandos.size()){ // se encuentra en el indice 0 del vector historial_comandos
                    strcpy(str, "");
                    index = 0;
                }
            }
        }
        else if(str[index] == 127){       // Simula la tecla Backspace para borrar el ultimo caracter
            if(index != 0){
                cout << "\b \b";
                index--;
            }
        }
        else{
            cout << str[index];
            index++;
        }
    }

    if(index > 0){                         // para no tomar en cuenta las cadenas vacias
        str[index] = '\0';
        historial_comandos.push_back(str);
    }

    cout << endl;
    if(!strcmp(str, "salir")) exit(0);
}

void Comando::prompt(){
    getcwd(dir, PATH_MAX); // obtiene la direccion actual
    string homeDir(user->pw_dir); // directorio home del usuario
    string dirStr(dir);

    // encuentra homeDir dentro de dir (directorio completo actual) y reemplaza /home/usuario por ~
    size_t pos = dirStr.find(homeDir);
    if(pos != string::npos){
        dirStr.replace(pos, homeDir.length(), "~");
    }
    cout << VERDE << user->pw_name << "@" << hostName << RESET << ":" << AZUL << dirStr << RESET << "[ESIS]$ ";

}

void Comando::divide(){
    // se divide la linea en argumentos, args[0] es el comando simple
    int n = 0, casoencad, casoredir = 0;
    bool haydirec = 0;

    char **comsimple = new char *[MAX_ARGUMENTS];
    char *token = strtok(str, " ");
    while(token != NULL && n < MAX_COMMAND_LENGTH){
        casoredir = 0; // Regresa a 0 para volver a empezar denuevo a analizar el redireccionamiento

        if(!(casoencad = casoencadenamiento(token))){
            casoredir = casoredireccion(token);
            if((casoredir == 1) || (casoredir == 2) || (casoredir == 3)){
                haydirec = 1;
                token = strtok(NULL, " ");
                comsimple[n] = NULL;
                coms.push({comsimple,casoredir});
                direccion.push(token);
            }
            else if(casoredir == 4){
                comsimple[n] = NULL;
                coms.push({comsimple,casoredir});
                haydirec = 0;
                n = 0;
                comsimple = new char *[MAX_ARGUMENTS];
                direccion.push(NULL);
            }
            else{
                comsimple[n++] = token; // guarda cada token dentro de comsimple
            }
        }

        else{
            comsimple[n] = NULL;
            if(!haydirec){
                coms.push({comsimple,casoredir});
            }
            haydirec = 0;
            n = 0;
            comsimple = new char *[MAX_ARGUMENTS];
            direccion.push(NULL);
        }

        token = strtok(NULL, " ");
    }

    comsimple[n] = NULL;
    if(!haydirec){
        coms.push({comsimple,casoredir});
        direccion.push(NULL);
    }
}

void Comando::execute(){
    while(!coms.empty()){
        if(strcmp(coms.front().first[0], "cd") == 0){
            if(chdir(coms.front().first[1]) == -1){
                cout << "Error al cambiar de directorio\n";
            }
        }
        else if(coms.front().second == 4){
            Pipe();
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
                cout << "Error al ejecutar el comando : " << coms.front().first[0] << "\n"; //cuando el comando se ejecuta correctamente no se muestra esta lÃ­nea
                exit(1);
            }
            else{// Proceso padre
                wait(NULL); //Espera a que el proceso hijo termine
            }
        }
        direccion.pop();
        coms.pop();
    }
}

void Comando::processRed(){ // Analiza si se ejecuto alguna redireccion para el proceso de manejado de archivo
    if(coms.front().second == 1){
        int fd = open(direccion.front(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    else if(coms.front().second == 2){ // Si se encontro el redireccionamiento "<"
        int fd = open(direccion.front(), O_RDONLY); // Abro el archivo (segunda parte) guardado en direccion con permiso para solo leer
        if(fd < 0){
            cerr << "Error al abrir el archivo de entrada" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO); // lee el archivo en lugar del teclado y lo reserva
        close(fd); // cierra el archivo
    }

    else if(coms.front().second == 3){
        int fd = open(direccion.front(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

}

void Comando::Pipe(){
    int fd[2];
    pipe(fd);
    pid_t child_pid1 = fork();
    if(child_pid1 < 0){ //Error
        cout << "Error al crear un proceso hijo" << endl;
        exit(1);
    }
    else if(child_pid1 == 0){ //Proceso hijo
        close(fd[READ_END]);
        dup2(fd[WRITE_END], STDOUT_FILENO);
        close(fd[WRITE_END]);
        execvp(coms.front().first[0], coms.front().first); //Ejecuta el comando con sus argumentos
        cout << "Error al ejecutar el comando\n"; //cuando el comando se ejecuta correctamente no se muestra esta lÃ­nea
        exit(1);
    }
    else{// Proceso padre
        close(fd[WRITE_END]);
        pid_t child_pid2 = fork();
        if(child_pid2 < 0){ //Error
            cout << "Error al crear un proceso hijo" << endl;
            exit(1);
        }
        else if(child_pid2 == 0){ //Proceso hijo
            close(fd[WRITE_END]);
            dup2(fd[READ_END], STDIN_FILENO);
            close(fd[READ_END]);
            direccion.pop();
            coms.pop();
            execvp(coms.front().first[0], coms.front().first); //Ejecuta el comando con sus argumentos
            cout << "Error al ejecutar el comando\n"; //cuando el comando se ejecuta correctamente no se muestra esta lÃ­nea
            exit(1);
        }
        else{
            close(fd[READ_END]);
            wait(NULL);
            wait(NULL);
            direccion.pop();
            coms.pop();
        }
    }
}


int Comando::casoredireccion(char *token){//verifica si un token es un operador de redireccion

    if(!strcmp(token, ">")) return 1;
    else if(!strcmp(token, "<")) return 2;
    else if(!strcmp(token, ">>")) return 3;
    else if(!strcmp(token, "|")) return 4;
    else return 0;
}

int Comando::casoencadenamiento(char *token){
    if(!strcmp(token, ";")) return 1;
    else return 0;
}

int main(){
    Comando comando;
    while(true){
        comando.ingresar_comando(comando);
        comando.divide();
        comando.execute();
    }
    return 0;
}

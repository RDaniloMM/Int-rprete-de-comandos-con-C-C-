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

char obtenerTecla() {
    int buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return (buf);
}

class Comando{
private:
    char dir[PATH_MAX];
    char hostName[HOST_NAME_MAX];
    passwd *user;
    queue<pair<char **, int>> coms; // cola de comandos a ejecutarse
    queue<char *> direccion; // cola de archivos o argumentos
    char str[MAX_COMMAND_LENGTH]; // comando de entrada
    vector<int> mod; // vector de redirecciones
    vector<string> historial_comandos; // para ver el historial de comandos
public:
    Comando();
    void ingresar_comando(Comando comando);
    void prompt();
    void divide();
    void execute();
    int casoredireccion(char *);
    int casoencadenamiento(char *);
    int processRed();
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

    while ((str[index] = obtenerTecla()) != '\n') {
        if (str[index] == '\e'){
            char siguiente = obtenerTecla();
            char tercero = obtenerTecla();
            if (siguiente == '[' && tercero == 'A') {       // Si se presiona la flecha arriba se obtiene el anterior comando ingresado
                cout << "\033[2K\r" << flush; // Borra la línea completa y retrocede al principio
                comando.prompt();
                index = 0;
                if ((tamanio_historialc-1) != -1){
                    cout<<historial_comandos[--tamanio_historialc];
                }
                else{
                    cout<<historial_comandos[tamanio_historialc];
                }
                strcpy(str, historial_comandos[tamanio_historialc].c_str());
                index = historial_comandos[tamanio_historialc].size();
            }
            else if (siguiente == '[' && tercero == 'B') {  // Si se presiona la flecha abajo se obtiene el posterior comando ingresado
                cout << "\033[2K\r" << flush; // Borra la línea completa y retrocede al principio
                comando.prompt();
                index = 0;
                if ((tamanio_historialc+1) != historial_comandos.size()){
                    cout<<historial_comandos[++tamanio_historialc];
                    strcpy(str, historial_comandos[tamanio_historialc].c_str());
                    index = historial_comandos[tamanio_historialc].size();
                }
                else{
                    strcpy(str, "");
                    index = 0;
                }
            }
        }
        else if (str[index] == 127) {
            // borra el ultimo caracter 
            if (index != 0){
                cout << "\b \b";
                index--;
            }
        }
        else{
            cout<<str[index];
            index++;
        }
    }
    str[index] = '\0';
    // si se introduce un espacio en blanco no hacer push_back
    historial_comandos.push_back(str);
    cout<<endl;
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

    char **comsimple = new char *[MAX_ARGUMENTS]; // Se encarga de reunir los comandos token por token
    char *token = strtok(str, " "); // Asigna a cada token los valores de ingreso del str separados por " " espacio
    while(token != NULL && n < MAX_COMMAND_LENGTH){
        casoredir = 0; // Regresa a 0 para volver a empezar denuevo a analizar el redireccionamiento

        if(!(casoencad = casoencadenamiento(token))){ // si no es un opr de encad, se sigue guardando como comando simple
            casoredir = casoredireccion(token); // extrae el valor 0 si no hay redireccion , 1 si encuentra > , 2 si encuntra <
            if((casoredir == 1) || (casoredir == 2) || (casoredir == 3)){ // caso encuentre redireccion
                haydirec = 1; // cambia la variable a que si encontro redireccion
                token = strtok(NULL, " "); // pasa al siguiente token obviando ">,<" para que token almacene el nombre del archivo
                comsimple[n] = NULL; // pone limite al conjunto de tokens antes de ">,<" almacenados en comsimple
                coms.push({comsimple, casoredir}); // se inserta todo el comando antes de ">,<" y que tipo de redireccion es (casoredir);
                direccion.push(token); // almacena el nombre del archivo que va despues de ">,<" gracias a que se uso la linea 80
                mod.push_back(casoredir); // De acuerdo a la posicion del comando esto almacenara si es una redireccion o un comando simple
            }
            else if(casoredir == 4){
                comsimple[n] = NULL;
                coms.push({comsimple, casoredir});
                mod.push_back(casoredir);
                haydirec = 0;
                n = 0;
                comsimple = new char *[MAX_ARGUMENTS];
            }
            else{
                comsimple[n++] = token; // guarda cada token dentro de comsimple
            }
        }

        else{// caso contrario, se acaba el comando simple y se guarda en la cola de comandos
            comsimple[n] = NULL; // pone limite al conjunto de tokens almacenados
            if(!haydirec){ // consulta si anterior a ello se encontro un direccionamiento porque si encontro se volveria a repetir la linea 94
                coms.push({comsimple, casoredir}); // caso que no encontro redireccionamiento significa que la linea 82 no fue ejecutada, por ende necesita ejecutarse para guarda el comando simple
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
        coms.push({comsimple, casoredir});
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
            pid_t child_pid = fork(); // Duplica el proceso actual y retorna un valor segun sea el proceso
            if(child_pid < 0){ // Error
                cout << "Error al crear un proceso hijo" << endl;
                exit(1);
            }
            else if(child_pid == 0){ // Proceso hijo
                if(processRed() != 0){
                    execvp(coms.front().first[0], coms.front().first); // Ejecuta el comando con sus argumentos
                    cout << "Error al ejecutar el comando\n"; // cuando el comando se ejecuta correctamente no se muestra esta línea
                    exit(1);
                }
            }
            else{// Proceso padre
                if(mod[0] != 5){
                    waitpid(child_pid, NULL, 0);
                }
            }
            if(!direccion.empty() && (mod[0] > 0 && mod[0] != 4)){
                direccion.pop();
            }
        }

        if(mod[0] == 4){
            mod.erase(mod.begin());
            coms.pop();
        }
        mod.erase(mod.begin());
        coms.pop();
    }
}

int Comando::processRed(){ // Analiza si se ejecutó alguna redirección para el proceso de manejado de archivo
    if(coms.front().second == 1){
        int fd = open(direccion.front(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd < 0){
            cerr << "Error al abrir el archivo de salida" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        return 1;
    }

    else if(coms.front().second == 2){ // Si se encontro el redireccionamiento "<"
        int fd = open(direccion.front(), O_RDONLY); // Abro el archivo (segunda parte) guardado en direccion con permiso para solo leer
        if(fd < 0){
            cerr << "Error al abrir el archivo de entrada" << endl;
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO); // lee el archivo en lugar del teclado y lo reserva
        close(fd); // cierra el archivo
        return 1;
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
        return 1;
    }
    else if(coms.front().second == 4){
        int status;
        int fd[2];
        pipe(fd);
        pid_t child_pid1 = fork();
        if(child_pid1 < 0){ // Error
            cout << "Error al crear un proceso hijo" << endl;
            exit(1);
        }
        else if(child_pid1 == 0){ // Proceso hijo
            close(fd[READ_END]);
            dup2(fd[WRITE_END], STDOUT_FILENO);
            close(fd[WRITE_END]);
            execvp(coms.front().first[0], coms.front().first); // Ejecuta el comando con sus argumentos
            cout << "Error al ejecutar el comando\n"; // cuando el comando se ejecuta correctamente no se muestra esta línea
            exit(1);
        }
        else{// Proceso padre
            close(fd[WRITE_END]);
            pid_t child_pid2 = fork();
            if(child_pid2 < 0){ // Error
                cout << "Error al crear un proceso hijo" << endl;
                exit(1);
            }
            else if(child_pid2 == 0){ // Proceso hijo
                //    close(fd[WRITE_END]);
                dup2(fd[READ_END], STDIN_FILENO);
                close(fd[READ_END]);
                coms.pop();
                execvp(coms.front().first[0], coms.front().first); // Ejecuta el comando con sus argumentos
                cout << "Error al ejecutar el comando\n"; // cuando el comando se ejecuta correctamente no se muestra esta línea
                exit(1);
            }
            else{
                close(fd[READ_END]);
                close(fd[WRITE_END]);
            }
        }
        return 0;
    }
    else if(coms.front().second == 5){ // Operador "&"
        mod.push_back(5);
        coms.front().second = 0; // Modificar el tipo de redirección para evitar ejecutar en el proceso padre
    }

    return 1;
}

int Comando::casoredireccion(char *token){// verifica si un token es un operador de redirección

    if(!strcmp(token, ">")) return 1;
    else if(!strcmp(token, "<")) return 2;
    else if(!strcmp(token, ">>")) return 3;
    else if(!strcmp(token, "|")) return 4;
    else if(!strcmp(token, "&")) return 5; // Nuevo operador "&" para trabajo en segundo plano
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
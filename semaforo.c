/**
 * Autores - Rodrigo Quiroz Reyes y Esteban Manrique de Lara Sirvent
 * Fecha - 01/10/2020
 * Actividad 6: IPC
 *
*/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#define TCP_PORT 8000

void gestor_Usr(int);
int leer_int(int);
void sig_handler(int);

volatile sig_atomic_t color=0;
volatile sig_atomic_t senal=0;
volatile sig_atomic_t cliente;
volatile sig_atomic_t pid_next;
volatile sig_atomic_t alarmBloq = 0;
volatile sig_atomic_t veces = 0;

int main(int argc, const char * argv[])
{
    signal(SIGALRM, sig_handler);
    FILE *stream; //Usado para la comunicacion mediante socket entre semaforo y central

    struct sockaddr_in direccion;
    char buffer[1000]; //Aqui se escriben datos a ser comunicados mediante el socket
    ssize_t leidos, escritos;
    
    if (argc != 2) //En caso de que no se introduzca una direccion IP valida
    {
        printf("Use: %s IP_Servidor \n", argv[0]);
        exit(-1);
    }
    
    //Creacion del socket
    cliente = socket(PF_INET, SOCK_STREAM, 0);
    
    //Se establece conexión con servidor/central
    inet_aton(argv[1], &direccion.sin_addr);
    direccion.sin_port = htons(TCP_PORT);
    direccion.sin_family = AF_INET;
    escritos = connect(cliente, (struct sockaddr *) &direccion, sizeof(direccion));
    
    if (escritos == 0) 
    {
        signal(SIGTSTP, SIG_IGN); //Permite ignorar el uso de CTRL + Z
        //signal(SIGINT, SIG_IGN); //Permite ignorar el uso de CTRL + C

        printf("Conectado a %s:%d \n", inet_ntoa(direccion.sin_addr), ntohs(direccion.sin_port));
        sleep(1);
        printf("PID de este semaforo: %d\n", getpid());
        sprintf(buffer, "%d", getpid());
        write(cliente, &buffer, sizeof(buffer)); //PID de este semaforo es puesto en buffer para ser enviado a central

        pid_next= leer_int(cliente); //Mediante comunicacion con sockets, semaforo recibe PID de semaforo con quien se tiene que comunicar a contiunuacion
        printf("PID de Siguiente semaforo: %d\n", pid_next);

        int inicio=leer_int(cliente); //Se obtiende posicion dentro del sistema de semaforos con el que se va a tratar
        printf("%d\n", inicio);

        //Se establece el uso de handler para SIGUSR1
       
        signal(SIGUSR1, gestor_Usr);
        sleep(1);

        //Semaforo pasa de estar en ALTO a SIGA
        if(inicio == 0)
        {
            color = 1;
        }
        sigset_t conjunto;
        
        //Se introducen en conjunto de signals aquellas a ser manejadas por los semaforos
        sigaddset(&conjunto, SIGUSR1); 
        sigaddset(&conjunto, SIGALRM);

        if(color==1) //En caso de que semaforo este en SIGA
        {
            color=0; //Semaforo pasa a ROJO
            kill(pid_next,SIGUSR1); //Se manda senal a siguiente semaforo
            sleep(1);
        }
        while(1)
        {
            //sigprocmask(SIG_BLOCK, &conjunto, NULL);
            senal = leer_int(cliente); //Se checa constantemente si no se ha recibido senal de central de hacer algo especial
            if(senal==0) //Comportamiento normal -> ALTO,SIGA,ALTO...
            {
                sigprocmask(SIG_UNBLOCK, &conjunto, NULL);
                if(alarmBloq == 1 && color==1)
                {
                    alarmBloq = 0;
                    color = 0;
                    sleep(1);
                    gestor_Usr(0);
                }
            }
            else if(senal==2 || senal==1)
            {                             
                sigprocmask(SIG_BLOCK, &conjunto, NULL); //Bloqueo de senales a peticion de Central
                alarm(0);
                alarmBloq = 1;
                if(senal==2)
                {
                    printf("SEMAFORO INTERMITENTE \n");
                    sleep(2);
                }
                else if(senal==1)
                {
                    printf("SEMAFORO EN ALTO \n");
                    sleep(2);
                }
            }
            veces = 0;
        }
    }
    //Cerrar sockets
    close(cliente);
    return 0;
}

/**
 * Funcion encargada de leer el PID del semaforo al cual se le tiene que enviar SIGUSR1 despues de haber estado 30 segundos en siga O recibir
 * notificacion de central acerca de la necesidad de un comportamiento especial
 * 
 * @param cliente, utilizada para la lectura del buffer del socket entre el semaforo y la central
 * 
 * return x, el cual es el PID del semaforo a recibir la siguiente SIGUSR1
 **/
int leer_int(int cliente)
{
    char buffer[1000];
    int x;
    read(cliente, &buffer, sizeof(buffer));
    x=atoi(buffer);
    return x;
}

/**
 * Funcion encargada de mandar senal a siguiente semaforo cuando termine su ciclo de 30 segundos. Cambia color de semaforo a ROJO
 * y hace kill() para comunicacion con siguiente semaforo
 * 
 * @param sig, signal recibida por handler
 * 
 **/ 
void sig_handler(int sig)
{
    write(STDOUT_FILENO, "Cambio mi estado a ALTO.\n",25);
    color=0;
    kill(pid_next, SIGUSR1);
}

/**
 * Funcion encargada de escribir en buffer estado en el que se encuentra (SIGA) el semaforo para que central este enterada de dicho Estado
 * Tambien es responsable de ejecutar el tiempo de espera (alarm()) y de poner el semaforo en SIGA
 * 
 * @param sig, signal recibida por gestor
 * 
 **/
void gestor_Usr(int sig)
{
    if(color==0 && veces==0)
    {
        sleep(1);
        char buffer[1000];
        sprintf(buffer, "%d", 1);
        write(cliente, &buffer, sizeof(buffer));
        write(STDOUT_FILENO, "Cambio mi estado a SIGA.\n",25);
        //sleep(1);
        color = 1;
        veces = 1;
        alarm(10);
    }
}
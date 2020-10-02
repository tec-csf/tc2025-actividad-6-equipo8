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

#define TCP_PORT 8000
#define NO_SEMAFORO 4

typedef int (*opcion_t) (int*, int*); 

void enviador_int_aux(int,int);
void enviador_int(int*,int*);
int imprimirEstados(int*, int*);
int imprimirEstados1(int*, int*);
int imprimirEstados2(int*, int*);
int imprimirEstados3(int*, int*);

void gestor_CtrlC(int);
void gestor_CtrlZ(int);
void gestor_CtrlZ2(int);
int leer_int(int);
void gestor(void (* handler_t)(int));
void sincronizar();

int controlSignals = 0;
int counter = 1;
int *cliente_actual;
int *cliente;

int main(int argc, const char * argv[])
{     
    
    opcion_t * imprimir = (opcion_t *) malloc( NO_SEMAFORO * sizeof(opcion_t));
    *imprimir=imprimirEstados;
    *(imprimir+1)=imprimirEstados1;
    *(imprimir+2)=imprimirEstados2;
    *(imprimir+3)=imprimirEstados3;

    struct sockaddr_in direccion;
    char buffer[1000]; //Aqui se escriben datos a ser comunicados mediante el socket
    int* pids;
    pids = (int*)malloc(sizeof(int)*NO_SEMAFORO); //Arreglo para guardar PID's de semaforos involucrados en el sistema
    int servidor;
    cliente = (int*)malloc(sizeof(int)*NO_SEMAFORO); //Arreglo para guardar los clientes correspondientes a los semaforos del sistema
    int* estadosActuales;
    estadosActuales = (int*)malloc(sizeof(int)*NO_SEMAFORO); //Arreglo donde se guardan los estados actuales de todos los semaforos
    int *intermitente = (int*)malloc(sizeof(int)*NO_SEMAFORO); //Arreglo usado para poner a todos los semaforos en Intermitenca
    int *altoTotal = (int*)malloc(sizeof(int)*NO_SEMAFORO); //Arreglo usado para poner a todos los semaforos en Alto total
    int *restaurar=(int*)malloc(sizeof(int)*NO_SEMAFORO); //Arreglo usado para regresar semaforos a estado Normal
    ssize_t leidos, escritos;
    int continuar = NO_SEMAFORO;
    pid_t pid;
    
    //En caso de que no se introduzca una direccion IP valida
    if (argc != 2) 
    {
        printf("Use: %s IP_Servidor. Favor de introducir direccion Ip para uso de socket -> ./central 127.0.0.1 \n", argv[0]);
        exit(-1);
    }

    servidor = socket(PF_INET, SOCK_STREAM, 0); //Creacion del socket

    //Enlace con socket
    inet_aton(argv[1], &direccion.sin_addr); 
    direccion.sin_port = htons(TCP_PORT);
    direccion.sin_family = AF_INET;
    
    bind(servidor, (struct sockaddr *) &direccion, sizeof(direccion)); //Enlace entre IP y puerto

    listen(servidor, NO_SEMAFORO); //Limita numero de clientes

    int tamano = sizeof(direccion);

    //Aceptar conexiones
    int i = 0;
    int* auxClientes = cliente;
    int* auxPid = pids;
    int *turno =  (int*)malloc(sizeof(int)*NO_SEMAFORO);
    int*auxTurno=turno;
    int *auxActuales = estadosActuales;
    int*auxInt=intermitente;
    int* auxParoTotal = altoTotal;
    int*auxRes=restaurar;

    while(i<continuar) //Limite de conexiones previamente definido. Se llenan arreglos usados para mantener turnos, estados e implementacion de 
    //Intermitencia y Alto Total
    {
        *(auxClientes + i)= accept(servidor, (struct sockaddr *) &direccion, &tamano);
        printf("Aceptando conexiones en %s:%d \n", inet_ntoa(direccion.sin_addr), ntohs(direccion.sin_port));
        *(pids + i) = leer_int(*(auxClientes + i));
        printf("PID: %d\n", *(pids + i) );
        *auxTurno=i;
        auxTurno++;
        *auxActuales = 0;
        auxActuales++;
        *auxInt=2;
        auxInt++;
        *auxParoTotal = 1;
        auxParoTotal++;
        *auxRes=0;
        auxRes++;
        i++;
    }

    enviador_int(pids,cliente); //Se envia PIDS de destino a cada semaforo del sistema
    enviador_int(turno,cliente); //Se envia turnos dentro del sistema a cada semaforo
    
    sleep(1);
    int control=0;
    enviador_int(restaurar,cliente);
    sincronizar();
    while(1)
    {
         
        if(controlSignals==0)
        {
            if(counter == 0)
            {
                printf("Empiezo desde %d\n",control);
                enviador_int(restaurar,cliente);
                control=(*(imprimir+control))(estadosActuales, cliente_actual);
                counter++;
                
            }
            else
            {
                enviador_int(restaurar,cliente);
                control=(*(imprimir+control))(estadosActuales, cliente);
            }
             //Se imprimen los estados de los semaforos cada vez que se registra un cambio en el sistema
            printf("DEBIG 0\n");
        }
        else if(controlSignals==1) //Semaforos en Intermitencia
        {
            enviador_int(intermitente,cliente);
            printf("Semáforos en intermtentes\n");
            sleep(2);
        }
        else //Semaforos en Rojo Total
        {
            enviador_int(altoTotal, cliente);
            printf("Semáforos en stop\n");
            sleep(2);
        } 
    }
    
    auxClientes = cliente; 

    //Cierre de clientes y servidor   
    for (int i = 0; i < 4; i++)
    {
        close(*(auxClientes + i));
    }
    close(servidor);

    //Liberacion de memoria
    free(cliente);
    free(pids);
    free(estadosActuales);
    free(altoTotal);
    free(restaurar);
    free(intermitente);
    free(imprimir);
    free(turno);

    return 0;
}

/**
 * Funcion encargada de generar una vuelta de "sincronizacion" con los semaforos del sistema antes de empezar operaciones.
 * Evita problemas de sincronizacion (cambios de estado en mas de 1 semaforo a la vez) en vueltas posteriores
 * 
 **/ 
void sincronizar()
{
    int*auxClientes=cliente;
    printf("Sincronizando semaforos\n");
    for(int i=1;auxClientes<cliente+NO_SEMAFORO; ++auxClientes,i++) //Se checan todos los clientes para ver si se envio notificacion de cambio
    {
        int estado = leer_int(*auxClientes);
        printf("Semaforo %d: Online\n", i);
    }
    printf("Todos los Semáforos en linea\n");
}

/**
 * Funcion encargada de enviar PID de destino de SIGUSR1 despues de haber estado en 30 segundos. Esto permite que semaforos conozcan con 
 * quien comunciarse.
 * 
 * @param arr_pid, arreglo de pids de semforos del sistema
 * @param arr_aclientes, arreglo de clientes de todos los semaforos del sistema
 * 
 **/
void enviador_int(int*arr_pid,int*arr_clientes)
{
    int *auxClientes=arr_clientes;
    int *auxPid;
    for(auxPid=arr_pid+1;auxClientes<arr_clientes+NO_SEMAFORO;++auxPid,++auxClientes)
    {   
        if(auxPid==arr_pid+NO_SEMAFORO)
        {
            enviador_int_aux(*arr_pid,*auxClientes);
        }
        else
        {
            enviador_int_aux(*auxPid,*auxClientes);
        }
        sleep(1);
    }
}

/**
 * Funcion auxiliar a enviador_int(). Como tal, esta es la funcion que escibe en buffer de socket el PID para comunciacion de USRSIG1
 * 
 * @param arr_pid, pid de semforo del sistema a escribir en buffer
 * @param arr_aclientes, cliente de semaforo del sistema que va a ser utilizado
 * 
 **/
void enviador_int_aux(int arr_pid,int arr_clientes)
{
    char buffer[1000];
    sprintf(buffer, "%d", arr_pid);
    write(arr_clientes, &buffer, sizeof(buffer));
}

/**
 * Funcion encargada de estar constantemente checando si los semaforos han enviado una notificacion de algun cambio en sus estado.
 * Imprime estados de semaforos en caso de recibir alguna notificacion de cambio. Se utiliza en caso de funcionamiento normal del sistema
 * de semaforos
 * 
 * @param actuales, arreglo de estados actuales de los semaforos
 * @param arr_aclientes, arreglo de clientes de todos los semaforos del sistema
 * 
 **/
int imprimirEstados(int* actuales, int*arr_clientes)
{
    int* auxClientes = arr_clientes;
    int* auxActuales = actuales;
    char buffer[1000];
    int banderaCambio = 0;
    struct sigaction gestor;
    gestor.sa_handler = gestor_CtrlZ;
    signal(SIGTSTP, gestor.sa_handler);
    struct sigaction gestor2;
    gestor2.sa_handler = gestor_CtrlC;
    signal(SIGINT, gestor2.sa_handler);
    int i=4;
    
    for(int j=0; auxClientes<arr_clientes+NO_SEMAFORO; ++auxClientes,j++) //Se checan todos los clientes para ver si se envio notificacion de cambio
    {
        if(controlSignals == 0)
        { 
            cliente_actual=auxClientes;
            int estado = leer_int(*auxClientes); //AQUI SE QUEDA CENTRAL
            
            int contador=0;
            for(int i=0;i<NO_SEMAFORO;i++,++auxActuales)
            {
                
                if(auxClientes==(arr_clientes+i)) *(auxActuales) = estado;
                else  *(auxActuales) = 0;
            }
            for (int i = 0; i < NO_SEMAFORO; i++)
            {
                if(*(actuales+i)==0) contador++; //Checa si estado no es 0 (ALTO)
            }
            auxActuales = actuales;

                if(contador!=4) //En caso de que se tenga cambios en el sistema, se hace la impresion
                {          
                    for(int i = 0; i<NO_SEMAFORO; i++)
                    {
                        printf("Estado semaforo %d: %d\n",(i + 1) , *(auxActuales + i));
                    
                    }
                    printf("\n");
                }
                fflush(NULL);
            sleep(9);
            
            if(controlSignals == 0)
            {
                
                i--;
            }

        }
    }

    if(i==4) i=0;
    return i;
}

/**
 * Funcion encargada de estar constantemente checando si los semaforos han enviado una notificacion de algun cambio en sus estado.
 * Imprime estados de semaforos en caso de recibir alguna notificacion de cambio. Se utiliza cuando la activacion de una senal en la central
 * se llegase a activar en el 4to semaforo del sistema
 * 
 * @param actuales, arreglo de estados actuales de los semaforos
 * @param arr_aclientes, arreglo de clientes de todos los semaforos del sistema
 * 
 **/
int imprimirEstados1(int* actuales, int* arr_clientes)
{
    int* auxActuales = actuales;
    char buffer[1000];
    int banderaCambio = 0;
    struct sigaction gestor;
    gestor.sa_handler = gestor_CtrlZ;
    signal(SIGTSTP, gestor.sa_handler);
    struct sigaction gestor2;
    gestor2.sa_handler = gestor_CtrlC;
    signal(SIGINT, gestor2.sa_handler);
    
    int estado = leer_int(*arr_clientes);

    int contador=0;
    for(int i=0;i<NO_SEMAFORO;i++,++auxActuales)
    {    
        if(arr_clientes==(cliente+i)) *(auxActuales) = estado;
        else  *(auxActuales) = 0;
    }
    for (int i = 0; i < NO_SEMAFORO; i++)
    {
        if(*(actuales+i)==0) contador++; //Checa si estado no es 0 (ALTO)
    }
            
    auxActuales = actuales;

    if(contador!=NO_SEMAFORO) //En caso de que se tenga cambios en el sistema, se hace la impresion
    {          
       for(int i = 0; i<NO_SEMAFORO; i++)
        {
            printf("Estado semaforo %d: %d\n",(i + 1) , *(auxActuales + i));
        }
            printf("\n");
    }    
    else
    {
        printf("Semaforos puestos en Intermitente desde la CENTRAL\n");
    }
    return 0;
}

/**
 * Funcion encargada de estar constantemente checando si los semaforos han enviado una notificacion de algun cambio en sus estado.
 * Imprime estados de semaforos en caso de recibir alguna notificacion de cambio. Se utiliza cuando la activacion de una senal en la central
 * se llegase a activar en el 3ro semaforo del sistema
 * 
 * @param actuales, arreglo de estados actuales de los semaforos
 * @param arr_aclientes, arreglo de clientes de todos los semaforos del sistema
 * 
 **/
int imprimirEstados2(int* actuales, int* arr_clientes)
{
    int* auxActuales = actuales;
    char buffer[1000];
    int banderaCambio = 0;
    struct sigaction gestor;
    gestor.sa_handler = gestor_CtrlZ;
    signal(SIGTSTP, gestor.sa_handler);
    struct sigaction gestor2;
    gestor2.sa_handler = gestor_CtrlC;
    signal(SIGINT, gestor2.sa_handler);
    int i=2;
    
    for(int j=0; arr_clientes<cliente+NO_SEMAFORO; ++arr_clientes,j++) //Se checan todos los clientes para ver si se envio notificacion de cambio
    {
        if(controlSignals==0)
        { 
            cliente_actual=arr_clientes;
            int estado = leer_int(*arr_clientes);
        
            int contador=0;
            for(int i=0;i<NO_SEMAFORO;i++,++auxActuales)
            { 
                if(arr_clientes==(cliente+i)) *(auxActuales) = estado;
                else  *(auxActuales) = 0;
            }
            for (int i = 0; i < NO_SEMAFORO; i++)
            {
                if(*(actuales+i)==0) contador++; //Checa si estado no es 0 (ALTO)
            }
                
            auxActuales = actuales;

                if(contador!=4) //En caso de que se tenga cambios en el sistema, se hace la impresion
                {          
                    for(int i = 0; i<NO_SEMAFORO; i++)
                    {
                        printf("Estado semaforo %d: %d\n",(i + 1) , *(auxActuales + i));
                    }
                    printf("\n");
                }
                fflush(NULL);
            sleep(9);
            if(controlSignals == 0)
            {
                
                i--;
            }
        }
    }
                

    return i;
}

/**
 * Funcion encargada de estar constantemente checando si los semaforos han enviado una notificacion de algun cambio en sus estado.
 * Imprime estados de semaforos en caso de recibir alguna notificacion de cambio. Se utiliza cuando la activacion de una senal en la central
 * se llegase a activar en el 2do semaforo del sistema
 * 
 * @param actuales, arreglo de estados actuales de los semaforos
 * @param arr_aclientes, arreglo de clientes de todos los semaforos del sistema
 * 
 **/
int imprimirEstados3(int* actuales, int* arr_clientes)
{
    int* auxActuales = actuales;
    char buffer[1000];
    int banderaCambio = 0;
    struct sigaction gestor;
    gestor.sa_handler = gestor_CtrlZ;
    signal(SIGTSTP, gestor.sa_handler);
    struct sigaction gestor2;
    gestor2.sa_handler = gestor_CtrlC;
    signal(SIGINT, gestor2.sa_handler);
    int i=3;
    for(; arr_clientes<cliente+NO_SEMAFORO; ++arr_clientes) //Se checan todos los clientes para ver si se envio notificacion de cambio
    {
        if(controlSignals==0)
        {
            cliente_actual=arr_clientes;
            int estado = leer_int(*arr_clientes);
            printf("Estados: %d\n",estado);
            int contador=0;
            for(int i=0;i<NO_SEMAFORO;i++,++auxActuales)
            {
                if(arr_clientes==(cliente+i)) *(auxActuales) = estado;
                else  *(auxActuales) = 0;
            }
            for (int i = 0; i < NO_SEMAFORO; i++)
            {
                if(*(actuales+i)==0) contador++; //Checa si estado no es 0 (ALTO)
            }
                
            auxActuales = actuales;

                if(contador!=4) //En caso de que se tenga cambios en el sistema, se hace la impresion
                {          
                    for(int i = 0; i<NO_SEMAFORO; i++)
                    {
                        printf("Estado semaforo %d: %d\n",(i + 1) , *(auxActuales + i));
                    }
                    printf("\n");
                }
                fflush(NULL);
            sleep(9);
            if(controlSignals == 0)
            {
                i--;
            }
        }
    }
                
    return i;
}

/**
 * Funcion encargada de implementar interrupcion de CTRL + C o regresar semaforos a normailidad. Lo anterior se realiza
 * mediante el contorl de la bandera controlSignals (2 indica que semaforos estan en ALTO TOTAL)
 * 
 * @param sig, senal recibida por handler
 * 
 **/
void gestor_CtrlC(int sig)
{
    if(controlSignals == 0)
    {
        write(STDOUT_FILENO, "Inicio de interrup C. Semaforos a Alto Total\n", 47);
        controlSignals = 2;
    }
    else
    {
        controlSignals = 0;
        write(STDOUT_FILENO, "Final de interrup C. Semaforos vuelven a estado normal\n", 57);
        counter = 0;
    }
    return;
}

/**
 * Funcion encargada de implementar interrupcion de CTRL + Z o regresar semaforos a normailidad. Lo anterior se realiza
 * mediante el contorl de la bandera controlSignals (1 indica que semaforos estan en INTERMITENCIA)
 * 
 * @param sig, senal recibida por handler
 * 
 **/
void gestor_CtrlZ(int sig)
{
    if(controlSignals == 0)
    {
        write(STDOUT_FILENO, "Inicio de interrup Z. Semaforos a Intermitentes\n", 50);
        controlSignals = 1;
    }
    else
    {
        controlSignals = 0;
        write(STDOUT_FILENO, "Final de interrup Z. Semaforos vuelven a estado normal\n", 57);
        counter = 0;
    }
    return;
}

/**
 * Funcion encargada de leer el PID de semaforo comunicando se central, para luego ser guardado y usado en metodo enviador_int()
 * 
 * @param cliente, utilizada para la lectura del buffer del socket entre el semaforo y la central
 * 
 * return x, el cual es el PID del semaforo comunicandose con central
 **/
int leer_int(int cliente)
{
    char buffer[1000];
    int x;
    read(cliente, &buffer, sizeof(buffer));
    x=atoi(buffer);
    return x;
}
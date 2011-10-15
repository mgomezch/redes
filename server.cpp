#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <unordered_map>
#include <sstream>

#include "util.hpp"

#define MAX(a, b) ((a)>(b))?(a):(b)

void exit_usage(char *name, int exitcode) {
        if (exitcode != EX_OK) fprintf(stderr, "\n");
        fprintf(
                        (exitcode == EX_OK) ? stdout : stderr,
                        "Uso: %s -p <puerto> [-4|-6] [-v]\n"
                        "                    [-n <conns>] [-m <conns>]\n"
                        "                    {-s <tiempo> | -x} {-r <reqs> | -y}\n"
                        "     %s -h\n"
                        "\n"
                        "Opciones:\n"
                        "  -a <archivo>: Nombre de archivo de autentificación.\n"
                        "  -p <puerto>: puerto para aceptar peticiones del cliente.\n"
                        "  -n <conns>: número máximo de conexiones en espera por socket.\n"
                        "              Por defecto: %d.\n"
                        "  -m <conns>: número máximo de conexiones activas.\n"
                        "              Por defecto: %d.\n"
                        "  -s <tiempo>: tiempo máximo (en segundos) de inactividad de cada conexión.\n"
                        "               Por defecto: %d\n"
                        "  -x: No usar timeout.\n"
                        "      Esta opción es peligrosa; permite ataques de DoS al servidor.\n"
                        "  -r <reqs>: número máximo de pedidos por conexión.\n"
                        "             Por defecto: %d\n"
                        "  -y: No limitar número de pedidos por conexión.\n"
                        "      Esta opción es peligrosa; permite ataques de DoS al servidor.\n"
                        "  -4: utilizar el protocolo IPv4.\n"
                        "  -6: utilizar el protocolo IPv6.\n"
                        "  -v: mostrar información adicional en la salida estándar.\n"
                        "  -h: mostrar este mensaje.\n",
                name, name, DEFAULT_BACKLOG, DEFAULT_MAXCHILDREN, DEFAULT_TIMEOUT, DEFAULT_MAXREQS);
        exit(exitcode);
}

int main(int argc, char **argv) {
        std::unordered_map<std::string, std::string> auth;
        int i,                                  // Contador
            j,                                  // Contador
            socks,                              // Tamaño del arreglo sockfds
            addrc,                              // Número de sockets retornado por getaddrinfo()
            conn,                               // file descriptor de conexión
            nfds,                               // Número de sockets con actividad para select()
            childcount,                         // Contador de hijos (uno por conexión TCP activa)
            reqcount,                           // Contador de pedidos en una conexión TCP activa.
            pid,                                // Retorno de fork()
            exitcode,                           // Código de salida de hijos
            talking,                            // Booleano para control de flujo de hijos
            opt,                                // Retorno de getopt()
            verbose = 0,                        // Opción para imprimir mensajes explícitos
            ipv = IPV_UNSET,                    // Opción para protocolo de red (versión de IP)
            backlog = DEFAULT_BACKLOG,          // Opción para máximo número de conexiones en espera
            maxchildren = DEFAULT_MAXCHILDREN,  // Opción para máximo número de hijos
            timeout = DEFAULT_TIMEOUT,          // Opción para tiempo máximo de inactividad de una conexión
            maxreqs = DEFAULT_MAXREQS,          // Opción para máximo número de pedidos por conexión
            calidad = 1;                        // Calidad del servidor
        int *sockfds;                           // Arreglo de file descriptors para atender clientes
        long int la,                            // Retorno de strtol() en lectura de argumentos
                 lb;                            // Retorno de strtol() en lectura de argumentos
        char c,                                 // Caracter leído del cliente
             *auth_file = NULL,
             *service = NULL,                   // Argumento para puerto en que aceptar conexiones
             *narg = NULL,                      // Argumento para máximo número de conexiones en espera
             *marg = NULL,                      // Argumento para máximo número de hijos
             *sarg = NULL,                      // Argumento para tiempo máximo de inactividad de una conexión
             *rarg = NULL,                      // Opción para máximo número de pedidos por conexión
             *str;                              // Respuesta al cliente
        struct timeval t;                       // Tiempo
        struct tm *tstr;                        // Tiempo descompuesto
        struct addrinfo hints,                  // Opciones para getaddrinfo()
                        *results,               // Retorno de getaddrinfo()
                        *rp;                    // Iterador para retorno de getaddrinfo()
        struct protoent *proto;                 // Protocolo de transporte
        FILE *file;                             // Archivo para revisar rango de opción -n
        fd_set fds;                             // Conjunto de file descriptors para select()

        while ((opt = getopt(argc, argv, "a:p:n:m:s:r:c:xytu46vh")) != -1) switch (opt) {
                case 'a':
                        if (auth_file != NULL) free(auth_file);
                        if (asprintf(&auth_file, "%s", optarg) == -1) {
                                fprintf(stderr, "Error copiando string para manejo de argumentos.\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 'p':
                        if (service != NULL) free(service);
                        if (asprintf(&service, "%s", optarg) == -1) {
                                fprintf(stderr, "Error copiando string para manejo de argumentos.\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 'n':
                        if (narg != NULL) free(narg);
                        if (asprintf(&narg, "%s", optarg) == -1) {
                                fprintf(stderr, "Error copiando string para manejo de argumentos.\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 'm':
                        if (marg != NULL) free(marg);
                        if (asprintf(&marg, "%s", optarg) == -1) {
                                fprintf(stderr, "Error copiando string para manejo de argumentos.\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 's':
                        if (sarg != NULL) free(sarg);
                        if (asprintf(&sarg, "%s", optarg) == -1) {
                                fprintf(stderr, "Error copiando string para manejo de argumentos.\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 'x':
                        if (sarg != NULL) free(sarg);
                        timeout = 0;
                        break;
                case 'r':
                        if (rarg != NULL) free(rarg);
                        if (asprintf(&rarg, "%s", optarg) == -1) {
                                fprintf(stderr, "Error copiando string para manejo de argumentos.\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 'c':
                        if ((calidad = atoi(optarg)) < 1 || calidad > 10){
                                fprintf(stderr, "La calidad debe estar entre 1 y 10\n");
                                exit(EX_OSERR);
                        }
                        break;
                case 'y':
                        if (rarg != NULL) free(rarg);
                        maxreqs = 0;
                        break;
                case '4':
                        if (ipv == IPV_UNSET) ipv = IPV_IPV4;
                        else {
                                fprintf(stderr, "%s: Las opciones -4 y -6 son mutuamente excluyentes.\n", argv[0]);
                                exit_usage(argv[0], EX_USAGE);
                        }
                        break;
                case '6':
                        if (ipv == IPV_UNSET) ipv = IPV_IPV6;
                        else {
                                fprintf(stderr, "%s: Las opciones -4 y -6 son mutuamente excluyentes.\n", argv[0]);
                                exit_usage(argv[0], EX_USAGE);
                        }
                        break;
                case 'v':
                        verbose = 1;
                        break;
                case 'h':
                        exit_usage(argv[0], EX_OK);
                        break;
                default:
                        exit_usage(argv[0], EX_USAGE);
        }
        if (auth_file == NULL) {
                fprintf(stderr, "Es obligatorio especificar un archivo de autenticación.\n");
                exit_usage(argv[0], EX_USAGE);
        } else {
                if ((file = fopen(auth_file, "r")) == NULL) {
                        if (verbose) perror("fopen");
                        fprintf(stderr, "No se pudo abrir el archivo de autenticación.\n");
                        exit(EX_DATAERR);
                } else {
                        char *username, *password;
                        do {
                                i = fscanf(file, "%m[^!\n]!%m[^!\n]", &username, &password);
                                if (i == 2) {
                                        auth[std::string(username)] = std::string(password);
                                        free(username);
                                        free(password);
                                }
                        } while (i == 2);
                        fclose(file);
                }
        }
        if (service == NULL) {
                fprintf(stderr, "Es obligatorio especificar un puerto.\n");
                exit_usage(argv[0], EX_USAGE);
        }
        if (narg != NULL) {
                errno = 0;
                la = strtol(narg, NULL, 0);
                if (errno != 0) {
                        if (verbose) perror("strtol");
                        fprintf(stderr, "El argumento de la opción -n tiene un error de formato o es demasiado grande.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                free(narg);
                backlog = la;
                lb = INT_MAX-1;
                if ((file = fopen("/proc/sys/net/core/somaxconn", "r")) == NULL) {
                        if (verbose) perror("fopen");
                        if (verbose) fprintf(stderr, "No se pudo verificar el valor máximo del sistema para el número de conexiones en espera en un socket.  Es posible que el número máximo de conexiones en espera en un socket sea menor que el pedido.\n");
                } else {
                        errno = 0;
                        i = fscanf(file, "%ld", &lb);
                        if ((i == 0)||(i == EOF)) {
                                if (errno != 0) if (verbose) perror("fscanf");
                                if (verbose) fprintf(stderr, "No se pudo verificar el valor máximo del sistema para el número de conexiones en espera en un socket.  Es posible que el número máximo de conexiones en espera en un socket sea menor que el pedido.\n");
                                lb = INT_MAX-1;
                        }
                        fclose(file);
                }
                if (la < 1 || la > lb) {
                        fprintf(stderr, "El argumento de la opción -n debe ser un entero mayor que cero y menor o igual que %ld.\n", lb);
                        exit_usage(argv[0], EX_USAGE);
                }
        }
        if (marg != NULL) {
                errno = 0;
                la = strtol(marg, NULL, 0);
                if (errno != 0) {
                        if (verbose) perror("strtol");
                        fprintf(stderr, "El argumento de la opción -m tiene un error de formato o es demasiado grande.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                free(marg);
                if (la<1) {
                        fprintf(stderr, "El argumento de la opción -m debe ser un entero mayor que cero.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                maxchildren = la;
        }
        if (sarg != NULL) {
                errno = 0;
                la = strtol(sarg, NULL, 0);
                if (errno != 0) {
                        if (verbose) perror("strtol");
                        fprintf(stderr, "El argumento de la opción -s tiene un error de formato o es demasiado grande.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                free(sarg);
                if (la<0) {
                        fprintf(stderr, "El argumento de la opción -s debe ser un entero no negativo.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                timeout = la;
        }
        if (rarg != NULL) {
                errno = 0;
                la = strtol(rarg, NULL, 0);
                if (errno != 0) {
                        if (verbose) perror("strtol");
                        fprintf(stderr, "El argumento de la opción -r tiene un error de formato o es demasiado grande.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                free(rarg);
                if (la<0) {
                        fprintf(stderr, "El argumento de la opción -r debe ser un entero no negativo.\n");
                        exit_usage(argv[0], EX_USAGE);
                }
                maxreqs = la;
        }

        if (fclose(stdin) == EOF) if (verbose) perror("Error cerrando entrada estandar con fclose");

        memset(&hints, 0, sizeof(struct addrinfo));
        switch (ipv) {
                case IPV_IPV4:
                        hints.ai_family = AF_INET;
                        break;
                case IPV_IPV6:
                        hints.ai_family = AF_INET6;
                        break;
                case IPV_UNSET:
                default:
                        hints.ai_family = AF_UNSPEC;
                        break;
        }
        hints.ai_flags = AI_PASSIVE;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        hints.ai_socktype = SOCK_STREAM;
        proto = getprotobyname("TCP");
        hints.ai_protocol = proto->p_proto;

        if ((i = getaddrinfo(NULL, service, &hints, &results)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(i));
                exit(EX_OSERR);
        }

        for (addrc = 0, rp = results; rp != NULL; rp = rp->ai_next, ++addrc);
        if (addrc == 0) {
                fprintf(stderr, "No se encontró ninguna manera de crear el servicio.\n");
                exit(EX_UNAVAILABLE);
        }
        if ((sockfds = static_cast<int *>(calloc(addrc, sizeof(int)))) == NULL) {
                perror("calloc");
                exit(EX_OSERR);
        }

        for (i = 0, rp = results, socks = 0; rp != NULL; ++i, rp = rp->ai_next) {
                if ((sockfds[i] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
                        if (verbose) perror("socket");
                        sockfds[i] = NO_SOCK;
                        continue;
                }
                if (ipv == IPV_UNSET && rp->ai_family == PF_INET6) {
#if defined(IPV6_V6ONLY)
                        j = 1;
                        if (setsockopt(sockfds[i], IPPROTO_IPV6, IPV6_V6ONLY, &j, sizeof(j)) == -1) {
                                if (verbose) perror("setsockopt");
                                close(sockfds[i]);
                                sockfds[i] = NO_SOCK;
                                continue;
                        }
#else
                        fprintf(stderr, "Imposible usar la opción IPV6_V6ONLY para sockets IPv6; no se utilizará el socket.\n");
                        close(sockfds[i]);
                        sockfds[i] = NO_SOCK;
                        continue;
#endif
                }
                if (bind(sockfds[i], rp->ai_addr, rp->ai_addrlen) == -1) {
                        if (verbose) perror("bind");
                        close(sockfds[i]);
                        sockfds[i] = NO_SOCK;
                        continue;
                }
                if (listen(sockfds[i], backlog) == -1) {
                        if (verbose) perror("listen");
                        close(sockfds[i]);
                        sockfds[i] = NO_SOCK;
                        continue;
                }
                ++socks;
                if (verbose) {
                        fprintf(stderr, "Recibiendo conexiones en:\nai_family = %d\nai_socktype = %d\nai_protocol = %d\n\n", rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                        proto = getprotobynumber(rp->ai_protocol);
                        if (proto == NULL) fprintf(stderr, "protocolo desconocido!\n");
                        else fprintf(stderr, "protocolo: %s\n", proto->p_name);
                }
        }
        freeaddrinfo(results);

        if (socks <= 0) {
                fprintf(stderr, "No se encontró ninguna manera de crear el servicio.\n");
                exit(EX_UNAVAILABLE);
        }

        for (i = 0, j = 0; i < socks; ++i) if (sockfds[i] == NO_SOCK) {
                if (j == 0) j = i+1;
                for (; j<addrc; ++j) if (sockfds[j] != NO_SOCK) break;
                sockfds[i] = sockfds[j];
                ++j;
        }
        if ((sockfds = static_cast<int *>(realloc(sockfds, socks*sizeof(int)))) == NULL) {
                perror("realloc");
                exit(EX_OSERR);
        }

        for (i = 0; i < socks; ++i) if (fcntl(sockfds[i], F_SETFL, O_NONBLOCK) == -1) {
                perror("fcntl");
                exit(EX_OSERR);
        }

        childcount = 0;
        for (;;) { // main loop
                for (;;) {
                        pid = waitpid(-1, NULL, (childcount > maxchildren) ? 0 : WNOHANG);
                        if (pid == -1) if (verbose) perror("waitpid");
                        if (pid == 0 || pid == -1) break;
                        else {
                                --childcount;
                                if (verbose) fprintf(stderr, "Proceso hijo terminó; ahora hay %d hijos.\n", childcount);
                        }
                }
                FD_ZERO(&fds);
                nfds = -1;
                for (i = 0; i < socks; ++i) {
                        nfds = MAX(nfds, sockfds[i]);
                        FD_SET(sockfds[i], &fds);
                }
                if (nfds < 0) {
                        fprintf(stderr, "Error calculando valor máximo de sockets: se obtuvo %d.\n", nfds);
                        exit(EX_SOFTWARE);
                }
                t.tv_sec = 1;
                t.tv_usec = 0;
                if ((nfds = select(nfds + 1, &fds, NULL, NULL, &t)) == -1) {
                        if (errno == EINTR) {
                                if (verbose) perror("select");
                                continue;
                        } else {
                                perror("select");
                                exit(EX_OSERR);
                        }
                }

                for (i = 0, j = 0; i < socks && j < nfds; ++i) if (FD_ISSET(sockfds[i], &fds)) {
                        if ((conn = accept(sockfds[i], NULL, NULL)) == -1) {
                                if (verbose) perror("Conexión perdida; error haciendo accept");
                        } else {
                                pid = fork();
                                if (pid == 0) {
                                        for (i = 0; i < socks; ++i) if (close(sockfds[i]) == -1) if (verbose) perror("close");
                                        free(sockfds);
                                        talking = 1;
                                        if (fcntl(conn, F_SETFL, O_NONBLOCK) == -1) {
                                                if (verbose) perror("fcntl");
                                                exitcode = EX_OSERR;
                                                talking = 0;
                                        }
                                        reqcount = 0;
                                        while (talking && (maxreqs == 0 || reqcount < maxreqs)) {
                                                t.tv_sec  = timeout;
                                                t.tv_usec = 0;
                                                FD_ZERO(&fds);
                                                FD_SET(conn, &fds);
                                                switch (select(conn+1, &fds, NULL, NULL, (timeout == 0) ? NULL : (&t))) {
                                                        case -1:
                                                                if (verbose) perror("select");
                                                                exitcode = EX_TEMPFAIL;
                                                                talking = 0;
                                                                break;
                                                        case 0:
                                                                if (verbose) fprintf(stderr, "Conexión terminada por timeout.\n");
                                                                exitcode = EX_OK;
                                                                talking = 0;
                                                                break;
                                                        case 1:
                                                                opt = recv(conn, &c, 1, 0);
                                                                if (opt == -1) {
                                                                        if (verbose) perror("Error leyendo comando de cliente: recv");
                                                                        if (errno == EAGAIN) break;
                                                                        exitcode = EX_OSERR;
                                                                        talking = 0;
                                                                }
                                                                else if (opt == 0) {
                                                                        if (verbose) fprintf(stderr, "Conexión terminada por el cliente.\n");
                                                                        exitcode = EX_OK;
                                                                        talking = 0;
                                                                }
                                                                else if (opt == 1) {
                                                                        if (c == 'r') {
// ---------------------------------------- EJEMPLO DE CÓDIGO EN RESPUESTA A COMANDO 'r' ---------------------------------
                                                                                if (gettimeofday(&t, NULL) == -1) {
                                                                                        if (verbose) perror("gettimeofday");
                                                                                        exitcode = EX_SOFTWARE;
                                                                                        talking = 0;
                                                                                        break;
                                                                                }
                                                                                if ((tstr = gmtime(&(t.tv_sec))) == NULL) {
                                                                                        if (verbose) fprintf(stderr, "Error determinando la hora.\n");
                                                                                        exitcode = EX_SOFTWARE;
                                                                                        talking = 0;
                                                                                        break;
                                                                                }
                                                                                if (asprintf(&str, "G:CE:%d:%d:%d:%d:%d:%d.%lu %d\n", 1900 + (tstr->tm_year), tstr->tm_mon, tstr->tm_mday, tstr->tm_hour, tstr->tm_min, tstr->tm_sec, static_cast<long unsigned int>(t.tv_usec), calidad) == -1) {
                                                                                        if (verbose) fprintf(stderr, "Error creando string para comunicación.\n");
                                                                                        exitcode = EX_OSERR;
                                                                                        talking = 0;
                                                                                        break;
                                                                                }
                                                                                write(conn, str, strlen(str));
                                                                                free(str);
                                                                                ++reqcount;
// --------------------------------- FIN DE EJEMPLO DE CÓDIGO EN RESPUESTA A COMANDO 'r' ---------------------------------
                                                                        } else if (c == '!') {
// ---------------------------------------- EJEMPLO DE CÓDIGO EN RESPUESTA A COMANDO '!' ---------------------------------
                                                                                if (verbose) printf("Leyendo comando del cliente.\n");
                                                                                int es = 0;
                                                                                long int fsn;
                                                                                std::stringstream username;
                                                                                std::stringstream password;
                                                                                std::stringstream lenguaje;
                                                                                std::stringstream caso    ;
                                                                                std::stringstream fs      ;
                                                                                std::stringstream archivo ;
                                                                                while (es < 9) {
                                                                                        if (verbose) printf("Leyendo dato %d del cliente.\n", es);
                                                                                        opt = recv(conn, &c, 1, 0);
                                                                                        if (opt == -1) {
                                                                                                if (verbose) perror("Error leyendo datos de cliente: recv");
                                                                                                if (errno == EAGAIN) break;
                                                                                                exitcode = EX_OSERR;
                                                                                                talking = 0;
                                                                                                break;
                                                                                        } else if (opt == 0) {
                                                                                                if (verbose) fprintf(stderr, "Conexión terminada por el cliente.\n");
                                                                                                exitcode = EX_OK;
                                                                                                talking = 0;
                                                                                                break;
                                                                                        } else if (opt == 1) {
                                                                                                if (c == '!') ++es;
                                                                                                else {
                                                                                                        if (verbose) printf("Leyendo caracter %c del cliente.\n", c);
                                                                                                        if      (es == 0) username << c;
                                                                                                        else if (es == 2) password << c;
                                                                                                        else if (es == 4) lenguaje << c;
                                                                                                        else if (es == 6) caso     << c;
                                                                                                        else if (es == 8) fs       << c;
                                                                                                }
                                                                                        }
                                                                                }
                                                                                if (!talking) break;
                                                                                if (verbose) printf("Fin de lectura de datos del cliente.\n");

                                                                                if (sscanf(fs.str().c_str(), "%ld", &fsn) != 1) {
                                                                                        fprintf(stderr, "Error leyendo tamaño del archivo a leer del cliente (%s).\n", fs.str().c_str());
                                                                                        exitcode = EX_DATAERR;
                                                                                        talking = 0;
                                                                                        break;
                                                                                }

                                                                                if (verbose) printf("Leyendo %ld caracteres del archivo del cliente.\n", fsn);
                                                                                for (int i = 0; i < fsn; ++i) {
                                                                                        opt = recv(conn, &c, 1, 0);
                                                                                        if (opt == -1) {
                                                                                                if (verbose) perror("Error leyendo programa del cliente: recv");
                                                                                                if (errno == EAGAIN) break;
                                                                                                exitcode = EX_OSERR;
                                                                                                talking = 0;
                                                                                                break;
                                                                                        } else if (opt == 0) {
                                                                                                if (verbose) fprintf(stderr, "Conexión terminada por el cliente.\n");
                                                                                                exitcode = EX_OK;
                                                                                                talking = 0;
                                                                                                break;
                                                                                        } else if (opt == 1) {
                                                                                                if (verbose) printf("Leyendo caracter '%c' del archivo del cliente.\n", c);
                                                                                                archivo << c;
                                                                                        }
                                                                                }
                                                                                if (!talking) break;
                                                                                if (verbose) printf("Fin de lectura del archivo del cliente.\n");
// --------------------------------- FIN DE EJEMPLO DE CÓDIGO EN RESPUESTA A COMANDO '!' ---------------------------------
                                                                        }
                                                                } else {
                                                                        fprintf(stderr, "Error imposible.\n");
                                                                        exitcode = EX_OSERR;
                                                                        talking = 0;
                                                                }
                                                                break;
                                                        default:
                                                                fprintf(stderr, "Error imposible.\n");
                                                                exitcode = EX_SOFTWARE;
                                                                talking = 0;
                                                                break;
                                                } // switch (select(conn))
                                        } // while (talking)
                                        if (shutdown(conn, SHUT_RDWR) == -1) if (verbose) perror("shutdown");
                                        if (close(conn) == -1) if (verbose) perror("close");
                                        exit(exitcode);
                                } // if (child)
                                else if (pid == -1) {
                                        if (verbose) perror("Conexión perdida; error haciendo fork");
                                } else {
                                        ++childcount;
                                        if (verbose) fprintf(stderr, "Conexión recibida; ahora hay %d hijos.\n", childcount);
                                }
                                if (close(conn) == -1) if (verbose) perror("close");
                        } // if accept() success
                        ++nfds;
                } /* foreach (FD_ISSET()) */
        } /* main loop */

        if (verbose) fprintf(stderr, "Error imposible.\n");
        exit(EX_SOFTWARE);
}


#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.hpp"

#define DEBUG_CLIENT

#ifdef DEBUG_CLIENT
#       define DEBUG_PRINT_CLIENT(SYM) printf("DEBUG_PRINT_CLIENT: " #SYM "\n", SYM);
#endif

void exit_usage(char *name, int exitcode) {
        if (exitcode != EX_OK) fprintf(stderr, "\n");
        fprintf((exitcode == EX_OK) ? stdout : stderr,
                        "Uso: %s -c <caso> -u <usuario> -w <password> -l <lenguaje> -f <archivo> -s <servidor> -p <puerto> [-4|-6]\n"
                        "\n"
                        "Opciones:\n"
// -------------------------------- EJEMPLO DE OPCIONES -------------------------------------
                        "  -c <caso>      : Nombre del caso de prueba a correr.\n"
                        "  -u <usuario>   : Nombre de usuario.\n"
                        "  -w <password>  : Clave del usuario.\n"
                        "  -l [c|cpp|java]: Lenguaje del archivo de código\n"
// ------------------------- FIN DE EJEMPLO DE OPCIONES -------------------------------------
// -------------------------------- EJEMPLO DE ARCHIVO  -------------------------------------
                        "  -f <archivo>   : Código fuente a enviar\n"
// ------------------------- FIN DE EJEMPLO DE ARCHIVO  -------------------------------------
                        "  -s <servidor>  : Nombre del servidor\n"
                        "  -p <puerto>    : puerto donde escucha el servidor\n"
                        "  -4: utilizar el protocolo IPv4\n"
                        "  -6: utilizar el protocolo IPv6\n", name);
        exit(exitcode);
}

int main(int argc, char **argv) {
        char *buffer = NULL;
        struct sockaddr_in saddr;
        struct hostent *hp;
// -------------------------------- EJEMPLO DE OPCIONES -------------------------------------
        char *caso     = NULL;
        char *usuario  = NULL;
        char *password = NULL;
        char *lenguaje = NULL;
// ------------------------- FIN DE EJEMPLO DE OPCIONES -------------------------------------
// -------------------------------- EJEMPLO DE ARCHIVO  -------------------------------------
        char *archivo  = NULL;
        long int fs;
        FILE *f;
// ------------------------- FIN DE EJEMPLO DE ARCHIVO  -------------------------------------
        char *server   = NULL;
        char *service  = NULL;
        int ipv = IPV_UNSET;
        int s;
        unsigned short port;
        FILE *sockfile;
        int opt;
        int acc;
        int ret;

        while ((opt = getopt(argc, argv, "c:u:w:l:f:s:p:46h")) != -1) switch (opt) {
// -------------------------------- EJEMPLO DE OPCIONES -------------------------------------
                case 'c':
                        if (caso != NULL) free(caso);
                        if (asprintf(&caso, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
                case 'u':
                        if (usuario != NULL) free(usuario);
                        if (asprintf(&usuario, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
                case 'w':
                        if (password != NULL) free(password);
                        if (asprintf(&password, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
                case 'l':
                        if (lenguaje != NULL) free(lenguaje);
                        if (asprintf(&lenguaje, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
// ------------------------- FIN DE EJEMPLO DE OPCIONES -------------------------------------
// -------------------------------- EJEMPLO DE ARCHIVO  -------------------------------------
                case 'f':
                        if (archivo != NULL) free(archivo);
                        if (asprintf(&archivo, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
// ------------------------- FIN DE EJEMPLO DE ARCHIVO  -------------------------------------
                case 's':
                        if (server != NULL) free(server);
                        if (asprintf(&server, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
                case 'p':
                        if (service != NULL) free(service);
                        if (asprintf(&service, "%s", optarg) == -1) {
                                fprintf(stderr, "%s: Error copiando string para manejo de argumentos.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
#if 0
                        if (((*n) = atoi(optarg)) <= 0){
                                fprintf(stderr, "%s: Especifica correctamene la cantidad de preguntas a realizar.\n", argv[0]);
                                exit(EX_OSERR);
                        }
                        break;
#endif
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
                case 'h':
                        exit_usage(argv[0], EX_OK);
                default:
                        exit_usage(argv[0], EX_USAGE);
        }

// -------------------------------- EJEMPLO DE OPCIONES -------------------------------------
        if (caso == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un caso.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }
        if (usuario == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un usuario.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }
        if (password == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un password.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }
        if (lenguaje == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un lenguaje.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }
// ------------------------- FIN DE EJEMPLO DE OPCIONES -------------------------------------
// -------------------------------- EJEMPLO DE ARCHIVO  -------------------------------------
        if (archivo == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un archivo de código.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }
// ------------------------- FIN DE EJEMPLO DE ARCHIVO  -------------------------------------
        if (server == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un servidor.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }
        if (service == NULL) {
                fprintf(stderr, "%s: Es obligatorio especificar un puerto.\n", argv[0]);
                exit_usage(argv[0], EX_USAGE);
        }

// -------------------------------- EJEMPLO DE ARCHIVO  -------------------------------------
        if ((f = fopen(archivo, "r")) == NULL) {
                int e = errno;
                printf("No se puede leer el archivo %s ", archivo);
                switch (e) {
                        case EACCES:
                                printf("(no tiene los permisos necesarios)");
                                break;
                        case ENOENT:
                                printf("(no existe)");
                                break;
                }
                printf("\n");
                exit(EX_NOPERM);
        }
        fseek(f, 0L, SEEK_END);
        fs = ftell(f);
        rewind(f);
        char *input = new char[fs];
        if (static_cast<long int>(fread(input, sizeof(char), fs, f)) < fs) {
                fprintf(stderr, "Error leyendo archivo de código. %ld\n", fs);
                exit(EX_IOERR);
        }
        fclose(f);
// ------------------------- FIN DE EJEMPLO DE ARCHIVO  -------------------------------------

        errno = 0;
        if (sscanf(service, "%hu", &port) != 1) {
                if (errno != 0) {
                        perror("sscanf");
                        exit(EX_OSERR);
                } else {
                        fprintf(stderr, "Error en formato del puerto.\n");
                        exit(EX_USAGE);
                }
        }

        if ((hp = gethostbyname(server)) == NULL) {
                perror("gethostbyname");
                exit(EX_UNAVAILABLE);
        }

        memset(&saddr, 0, sizeof(saddr));
        memcpy((char*) &saddr.sin_addr, hp->h_addr, hp->h_length);
        saddr.sin_family = hp->h_addrtype;
        saddr.sin_port = htons(port);

        s = socket(hp->h_addrtype, SOCK_STREAM, 0);
        if (s < 0) {
                perror("socket");
                exit(EX_IOERR);
        }
        if (connect(s, (struct sockaddr *)(&saddr), sizeof(saddr)) < 0) {
                perror("connect");
                exit(EX_IOERR);
        }

        if ((buffer = static_cast<char*>(malloc(sizeof(char) * (MAX_SERVER_RESPONSE + 1)))) == NULL){
                fprintf(stderr, "No se pudo crear el buffer para la respuesta del servidor.\n");
                exit(EX_SOFTWARE);
        }

        memset(buffer, 0, MAX_SERVER_RESPONSE + 1);

        if ((sockfile = fdopen(s, "r+")) == NULL) {
                perror("fdopen");
                exit(EX_IOERR);
        }

// -------------------------------- EJEMPLO DE OPCIONES -------------------------------------
        fprintf(sockfile, "!%d!%s", strlen(usuario ), usuario ); // 0, 1
        fprintf(sockfile, "!%d!%s", strlen(password), password); // 2, 3
        fprintf(sockfile, "!%d!%s", strlen(lenguaje), lenguaje); // 4, 5
        fprintf(sockfile, "!%d!%s", strlen(caso    ), caso    ); // 6, 7
// ------------------------- FIN DE EJEMPLO DE OPCIONES -------------------------------------
// -------------------------------- EJEMPLO DE ARCHIVO  -------------------------------------
        fprintf(sockfile, "!%ld!" , fs                        ); // 8
        fflush(sockfile);

        acc = 0;
        while (acc < fs) {
                ret = write(s, input, fs);
                if (ret == -1) {
                        if (errno == EAGAIN || errno == EINTR) continue;
                        exit(EX_IOERR);
                }
                acc += ret;
        }
// ------------------------- FIN DE EJEMPLO DE ARCHIVO  -------------------------------------

        shutdown(s, SHUT_RDWR);
        close(s);

        exit(EX_OK);
}

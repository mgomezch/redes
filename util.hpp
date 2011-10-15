#ifndef UTIL_HPP
#define UTIL_HPP

#define STRINGIFY_VALUE(S) STRINGIFY(S)
#define STRINGIFY(S) #S

#define TYPE_UNSET (-1)         // Protocolo de capa de transporte indefinido
#define TYPE_TCP 1
#define TYPE_UDP 2

#define IPV_UNSET (-1)          // Protocolo de capa de red indefinido
#define IPV_IPV4 1
#define IPV_IPV6 2

#define NO_SOCK (-1)            // No es un socket
#define MAX_SERVER_RESPONSE 256 // Tamaño máximo de la respuesta del servidor

#define SEQSTR_SIGCHR '~'       // Caracer especial, comienzo de secuencia
#define SEQSTR_LENGTH 10        // Tamaño de cadena que contiene secuencia

#define MAX_HOST_NAME         1024  // Longitud maxima para el nombre de un host
#define MAX_CHILDREN_RESPONSE 1024  // Tamaño máximo de un proceso hijo

#define DEFAULT_BACKLOG 5
#define DEFAULT_MAXCHILDREN 10
#define DEFAULT_TIMEOUT 10
#define DEFAULT_MAXREQS 0 // 20

#endif

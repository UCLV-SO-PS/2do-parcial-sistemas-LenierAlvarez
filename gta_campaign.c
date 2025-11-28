// gta_campaign.c
// Compilar: gcc -Wall -Wextra -o gta_campaign gta_campaign.c
// Uso: ./gta_campaign <n>    (n debe ser un solo número entero impar > 0)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= (size_t)w;
        p += w;
    }
    return count;
}

ssize_t read_all(int fd, void *buf, size_t count) {
    char *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return count - left; // EOF
        left -= (size_t)r;
        p += r;
    }
    return count;
}

int is_number(const char *s) {
    if (!s || *s == '\0') return 0;
    char *p = (char*)s;
    if (*p == '+' || *p == '-') p++;
    while (*p) {
        if (*p < '0' || *p > '9') return 0;
        p++;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <n>\n<n> debe ser un número entero impar > 0\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (!is_number(argv[1])) {
        fprintf(stderr, "Error: argumento no es un número válido.\n");
        return EXIT_FAILURE;
    }
    int n = atoi(argv[1]);
    if (n <= 0 || n % 2 == 0) {
        fprintf(stderr, "Error: n debe ser impar y mayor que 0.\n");
        return EXIT_FAILURE;
    }

    // Generar vector aleatorio de gastos (valores entre 5000 y 100000)
    int *gastos = malloc(sizeof(int) * n);
    if (!gastos) { perror("malloc"); return EXIT_FAILURE; }
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (int i = 0; i < n; ++i) {
        gastos[i] = 5000 + (rand() % 95001); // 5000..100000
    }

    // Crear pipes: p2c1, c1p, p2c2, c2p
    int p2c1[2], c1p[2], p2c2[2], c2p[2];
    if (pipe(p2c1) == -1 || pipe(c1p) == -1 || pipe(p2c2) == -1 || pipe(c2p) == -1) {
        perror("pipe");
        free(gastos);
        return EXIT_FAILURE;
    }

    pid_t pid1 = fork();
    if (pid1 < 0) { perror("fork"); free(gastos); return EXIT_FAILURE; }

    if (pid1 == 0) {
        // ------------------- HIJO 1 -------------------
        // cierra extremos no usados
        close(p2c1[1]); // no escribe en p2c1
        close(c1p[0]);  // no lee en c1p

        // también cerrar pipes ajenos
        close(p2c2[0]); close(p2c2[1]);
        close(c2p[0]); close(c2p[1]);

        // Leer n
        int rn;
        if (read_all(p2c1[0], &rn, sizeof(int)) != sizeof(int)) {
            perror("child1 read n");
            _exit(EXIT_FAILURE);
        }

        int *arr = malloc(sizeof(int) * rn);
        if (!arr) { perror("malloc child1"); _exit(EXIT_FAILURE); }

        if (read_all(p2c1[0], arr, sizeof(int) * rn) != (ssize_t)(sizeof(int) * rn)) {
            perror("child1 read arr");
            free(arr); _exit(EXIT_FAILURE);
        }
        close(p2c1[0]);

        // calcular suma acumulada
        int *cum = malloc(sizeof(int) * rn);
        if (!cum) { perror("malloc cum"); free(arr); _exit(EXIT_FAILURE); }
        int s = 0;
        for (int i = 0; i < rn; ++i) {
            s += arr[i];
            cum[i] = s;
        }

        // enviar resultados al padre
        if (write_all(c1p[1], &rn, sizeof(int)) != sizeof(int)) { perror("child1 write n"); }
        if (write_all(c1p[1], cum, sizeof(int) * rn) != (ssize_t)(sizeof(int) * rn)) { perror("child1 write cum"); }
        close(c1p[1]);

        free(arr); free(cum);
        _exit(EXIT_SUCCESS);
    }

    // Padre sigue y crea el hijo 2
    pid_t pid2 = fork();
    if (pid2 < 0) { perror("fork2"); free(gastos); return EXIT_FAILURE; }

    if (pid2 == 0) {
        // ------------------- HIJO 2 -------------------
        // cierra extremos no usados
        close(p2c2[1]); // no escribe en p2c2
        close(c2p[0]);  // no lee en c2p

        // cerrar pipes ajenos
        close(p2c1[0]); close(p2c1[1]);
        close(c1p[0]); close(c1p[1]);

        // Leer n
        int rn;
        if (read_all(p2c2[0], &rn, sizeof(int)) != sizeof(int)) {
            perror("child2 read n");
            _exit(EXIT_FAILURE);
        }

        int *arr = malloc(sizeof(int) * rn);
        if (!arr) { perror("malloc child2"); _exit(EXIT_FAILURE); }

        if (read_all(p2c2[0], arr, sizeof(int) * rn) != (ssize_t)(sizeof(int) * rn)) {
            perror("child2 read arr");
            free(arr); _exit(EXIT_FAILURE);
        }
        close(p2c2[0]);

        // calcular promedio mensual (simple promedio de todos los meses)
        long long total = 0;
        for (int i = 0; i < rn; ++i) total += arr[i];
        double promedio = (double)total / (double)rn;

        // enviar resultado al padre (primero tamaño 1, luego double)
        int one = 1;
        if (write_all(c2p[1], &one, sizeof(int)) != sizeof(int)) { perror("child2 write size"); }
        if (write_all(c2p[1], &promedio, sizeof(double)) != sizeof(double)) { perror("child2 write avg"); }
        close(c2p[1]);

        free(arr);
        _exit(EXIT_SUCCESS);
    }

    // ------------------- PADRE -------------------
    // cerrar extremos no usados en padres (lectura o escritura según corresponda)
    close(p2c1[0]); close(c1p[1]);
    close(p2c2[0]); close(c2p[1]);

    // Enviar n y vector a hijo1
    if (write_all(p2c1[1], &n, sizeof(int)) != sizeof(int)) { perror("parent write n to child1"); }
    if (write_all(p2c1[1], gastos, sizeof(int) * n) != (ssize_t)(sizeof(int) * n)) { perror("parent write arr to child1"); }
    close(p2c1[1]);

    // Enviar n y vector a hijo2
    if (write_all(p2c2[1], &n, sizeof(int)) != sizeof(int)) { perror("parent write n to child2"); }
    if (write_all(p2c2[1], gastos, sizeof(int) * n) != (ssize_t)(sizeof(int) * n)) { perror("parent write arr to child2"); }
    close(p2c2[1]);

    // Leer respuesta de hijo1 (suma acumulada)
    int rn1;
    if (read_all(c1p[0], &rn1, sizeof(int)) != sizeof(int)) { perror("parent read rn1"); }
    int *cumres = malloc(sizeof(int) * rn1);
    if (!cumres) { perror("malloc parent cumres"); free(gastos); return EXIT_FAILURE; }
    if (read_all(c1p[0], cumres, sizeof(int) * rn1) != (ssize_t)(sizeof(int) * rn1)) { perror("parent read cumres"); }
    close(c1p[0]);

    // Leer respuesta de hijo2 (promedio)
    int rn2;
    if (read_all(c2p[0], &rn2, sizeof(int)) != sizeof(int)) { perror("parent read rn2"); }
    double avg;
    if (read_all(c2p[0], &avg, sizeof(double)) != sizeof(double)) { perror("parent read avg"); }
    close(c2p[0]);

    // Imprimir resultados
    printf("Vector original de gastos (n=%d):\n[", n);
    for (int i = 0; i < n; ++i) {
        printf("%d", gastos[i]);
        if (i != n-1) printf(", ");
    }
    printf("]\n\n");

    printf("Suma acumulada (Hijo 1):\n[");
    for (int i = 0; i < rn1; ++i) {
        printf("%d", cumres[i]);
        if (i != rn1-1) printf(", ");
    }
    printf("]\n\n");

    printf("Promedio mensual (Hijo 2): %.2f\n", avg);

    // esperar hijos
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    free(gastos);
    free(cumres);
    return EXIT_SUCCESS;
}

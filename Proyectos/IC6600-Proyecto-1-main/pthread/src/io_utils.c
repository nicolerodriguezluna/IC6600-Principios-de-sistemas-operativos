/* ===============================================================================================================
 * io_utils.c — Proporciona utilidades de lectura/escritura de archivos y manejo de rutas/listados.
 * =============================================================================================================== */

#include "../include/io_utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* inicializa el vector dinámico de rutas en cero. */
void sv_init(strvec_t *v) { memset(v, 0, sizeof(*v)); }

/* agrega una copia de un path al vector y redimensiona si es necesario. */
void sv_push(strvec_t *v, const char *s)
{
    if (v->len == v->cap)
    {
        v->cap = v->cap ? v->cap * 2 : 32;
        v->paths = realloc(v->paths, v->cap * sizeof(char *));
        if (!v->paths)
            DIE("sin memoria");
    }
    v->paths[v->len++] = strdup(s);
}

/* libera todos los paths almacenados y el contenedor. */
void sv_free(strvec_t *v)
{
    for (size_t i = 0; i < v->len; i++)
        free(v->paths[i]);
    free(v->paths);
}

/* lee el archivo completoa memoria, agrega '\0', y devuelve tamaño leído. */
int read_file_text(const char *path, char **out_buf, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END))
    {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf)
    {
        fclose(f);
        return -1;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out_buf = buf;
    if (out_len)
        *out_len = rd;
    return 0;
}

/* escribe exactamente 'len' bytes al archivo destino. */
int write_file_text(const char *path, const char *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    size_t wr = fwrite(buf, 1, len, f);
    fclose(f);
    return wr == len ? 0 : -1;
}

/* lista archivos con el sufijo dado en 'dir' y los agrega a 'out'. */
int list_files_with_suffix(const char *dir, const char *suffix, strvec_t *out)
{
    DIR *d = opendir(dir);
    if (!d)
        return -1;
    struct dirent *ent;
    while ((ent = readdir(d)))
    {
        char path[PATH_MAX];
        join_path(dir, ent->d_name, path);

        /* Ignora "." y ".." */
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }

        struct stat st;
        if (stat(path, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            continue;
        }

        if (!has_suffix(ent->d_name, suffix))
            continue;
        sv_push(out, path);
    }
    closedir(d);
    return 0;
}

/* copia 'in' a 'out' y cambia/extiende la extensión por 'new_ext'. */
void replace_extension(const char *in, const char *new_ext, char out[PATH_MAX])
{
    strncpy(out, in, PATH_MAX - 1);
    out[PATH_MAX - 1] = '\0';
    char *dot = strrchr(out, '.');
    if (!dot)
    {
        strncat(out, new_ext, PATH_MAX - strlen(out) - 1);
    }
    else
    {
        *dot = '\0';
        strncat(out, new_ext, PATH_MAX - strlen(out) - 1);
    }
}

/* join_path: compone una ruta 'dir/name' */
void join_path(const char *dir, const char *name, char out[PATH_MAX])
{
    size_t ld = strlen(dir);
    snprintf(out, PATH_MAX, "%s%s%s", dir, (ld && dir[ld - 1] == '/') ? "" : "/", name);
}

- c common functions
    ```c
    #include <sys/types.h>
    #include <stdio.h>
    #include <ctype.h>
    #include <string.h>
    #include <stdbool.h>
    #include <stdlib.h>
    #include <assert.h>

    void replace_special(char *buffer, size_t buffer_size);
    int escape_slashes(char *buffer, size_t buffer_size);
    size_t strstripnewline(char *buffer);
    int strunescape(char *buf, size_t buf_len);
    int escape_string(char *buffer, size_t buffer_size);
    bool string_has_suffix(char const *s, char const *suffix);
    int strjoin(char *buffer, size_t buffer_size, char **fields, size_t fields_num, const char *sep);
    int strsplit(char *string, char **fields, size_t size);

    int main(void)
    {
        char str1[] = "123-456!&-789";
        printf("str1:%s\n", str1);
        replace_special(str1, strlen(str1));
        printf("str1 after replace_special:%s\n", str1);
    }

    int strsplit(char *string, char **fields, size_t size)
    {
        size_t i = 0;
        char *ptr = string;
        char *saveptr = NULL;
        while ((fields[i] = strtok_r(ptr, " \t\r\n", &saveptr)) != NULL) {
            ptr = NULL;
            i++;

            if (i >= size)
                break;
        }

        return (int)i;
    }

    int strjoin(char *buffer, size_t buffer_size, char **fields, size_t fields_num, const char *sep)
    {
        size_t avail = 0;
        char *ptr = buffer;
        size_t sep_len = 0;

        size_t buffer_req = 0;

        if (((fields_num != 0) && (fields == NULL)) ||
                ((buffer_size != 0) && (buffer == NULL)))
            return -1;

        if (buffer != NULL)
            buffer[0] = 0;

        if (buffer_size != 0)
            avail = buffer_size - 1;

        if (sep != NULL)
            sep_len = strlen(sep);

        for (size_t i = 0; i < fields_num; i++) {
            size_t field_len = strlen(fields[i]);

            if (i != 0)
                buffer_req += sep_len;
            buffer_req += field_len;

            if (buffer_size == 0)
                continue;

            if ((i != 0) && (sep_len > 0)) {
                if (sep_len >= avail) {
                    /* prevent subsequent iterations from writing to the
                    * buffer. */
                    avail = 0;
                    continue;
                }

                memcpy(ptr, sep, sep_len);

                ptr += sep_len;
                avail -= sep_len;
            }

            if (field_len > avail)
                field_len = avail;

            memcpy(ptr, fields[i], field_len);
            ptr += field_len;

            avail -= field_len;
            if (ptr != NULL)
                *ptr = 0;
        }

        return (int)buffer_req;
    }

    bool string_has_suffix(char const *s, char const *suffix)
    {
        if (s == NULL || suffix == NULL) {
            return false;
        }

        size_t s_len = strlen(s);
        size_t suffix_len = strlen(suffix);
        if (s_len < suffix_len) {
            return false;
        }

        s += (s_len - suffix_len);
        return strcmp(s, suffix) == 0;
    }

    int escape_string(char *buffer, size_t buffer_size)
    {
        char *temp;
        size_t j;

        /* Check if we need to escape at all first */
        temp = strpbrk(buffer, " \t\"\\");
        if (temp == NULL)
            return 0;

        if (buffer_size < 3)
            return -1;

        temp = calloc(1, buffer_size);
        if (temp == NULL)
            return -1;

        temp[0] = '"';
        j = 1;

        for (size_t i = 0; i < buffer_size; i++) {
            if (buffer[i] == 0) {
                break;
            } else if ((buffer[i] == '"') || (buffer[i] == '\\')) {
                if (j > (buffer_size - 4))
                    break;
                temp[j] = '\\';
                temp[j + 1] = buffer[i];
                j += 2;
            } else {
                if (j > (buffer_size - 3))
                    break;
                temp[j] = buffer[i];
                j++;
            }
        }

        assert((j + 1) < buffer_size);
        temp[j] = '"';
        temp[j + 1] = 0;

        strncpy(buffer, temp, buffer_size);
        free(temp);
        return 0;
    }

    int strunescape(char *buf, size_t buf_len)
    {
        for (size_t i = 0; (i < buf_len) && (buf[i] != '\0'); ++i) {
            if (buf[i] != '\\')
                continue;

            if (((i + 1) >= buf_len) || (buf[i + 1] == 0)) {
                printf("string unescape: backslash found at end of string.\n");
                /* Ensure null-byte at the end of the buffer. */
                buf[i] = 0;
                return -1;
            }

            switch (buf[i + 1]) {
            case 't':
                buf[i] = '\t';
                break;
            case 'n':
                buf[i] = '\n';
                break;
            case 'r':
                buf[i] = '\r';
                break;
            default:
                buf[i] = buf[i + 1];
                break;
            }

            /* Move everything after the position one position to the left.
            * Add a null-byte as last character in the buffer. */
            memmove(buf + i + 1, buf + i + 2, buf_len - i - 2);
            buf[buf_len - 1] = '\0';
        }
        return 0;
    }

    size_t strstripnewline(char *buffer)
    {
        size_t buffer_len = strlen(buffer);

        while (buffer_len > 0) {
            if ((buffer[buffer_len - 1] != '\n') && (buffer[buffer_len - 1] != '\r'))
                break;
            buffer_len--;
            buffer[buffer_len] = 0;
        }

        return buffer_len;
    }

    int escape_slashes(char *buffer, size_t buffer_size)
    {
        size_t buffer_len;

        buffer_len = strlen(buffer);

        if (buffer_len <= 1) {
            if (strcmp("/", buffer) == 0) {
                if (buffer_size < 5)
                    return -1;
                strncpy(buffer, "root", buffer_size);
            }
            return 0;
        }

        /* Move one to the left */
        if (buffer[0] == '/') {
            memmove(buffer, buffer + 1, buffer_len);
            buffer_len--;
        }

        for (size_t i = 0; i < buffer_len; i++) {
            if (buffer[i] == '/')
                buffer[i] = '_';
        }

        return 0;
    }

    void replace_special(char *buffer, size_t buffer_size)
    {
        for (size_t i = 0; i < buffer_size; i++) {
            if (buffer[i] == 0)
                return;
            if ((!isalnum((int)buffer[i])) && (buffer[i] != '-'))
                buffer[i] = '_';
        }
    }
    ```
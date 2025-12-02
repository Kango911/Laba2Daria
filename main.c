#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <limits.h>

// ========== КОНСТАНТЫ И СТРУКТУРЫ ==========
#define BYTE_SIZE 8
#define ASCII_SIZE 256
#define MAX_TREE_HT 100
#define BUFFER_SIZE 4096

// Структура узла дерева Хаффмана
typedef struct Node {
    unsigned char symbol;   // Символ (только для листьев)
    unsigned int freq;      // Частота символа
    struct Node *left;      // Левый потомок (0 бит)
    struct Node *right;     // Правый потомок (1 бит)
} Node;

// Структура для хранения кода символа
typedef struct Code {
    unsigned char symbol;
    char bits[MAX_TREE_HT]; // Строковое представление кода
    int length;            // Длина кода в битах
} Code;

// Структура минимальной кучи
typedef struct MinHeap {
    int size;              // Текущий размер кучи
    int capacity;          // Максимальная емкость
    Node** array;          // Массив указателей на узлы
} MinHeap;

// ========== ПРОТОТИПЫ ФУНКЦИЙ ==========
Node* createNode(unsigned char symbol, unsigned int freq);
MinHeap* createMinHeap(int capacity);
void swapNodes(Node** a, Node** b);
void heapify(MinHeap* heap, int idx);
Node* extractMin(MinHeap* heap);
void insertMinHeap(MinHeap* heap, Node* node);
void buildMinHeap(MinHeap* heap);
Node* buildHuffmanTree(unsigned int frequencies[]);
void generateCodesRecursive(Node* root, char* code, int depth, Code codes[]);
void generateCodes(Node* root, Code codes[]);
void freeHuffmanTree(Node* root);
void countFrequencies(FILE* file, unsigned int frequencies[]);
void writeEncodedFile(FILE* input, FILE* output, Code codes[], long* bit_count);
void decodeFile(FILE* input, FILE* output, Node* root, long bit_count);
int compareFiles(FILE* file1, FILE* file2);
void printStatistics(const char* filename, unsigned int frequencies[],
                     Code codes[], long original_size, long compressed_size);

// ========== РЕАЛИЗАЦИЯ ФУНКЦИЙ ==========

// Создание нового узла
Node* createNode(unsigned char symbol, unsigned int freq) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (node == NULL) {
        fprintf(stderr, "Ошибка выделения памяти для узла\n");
        exit(EXIT_FAILURE);
    }

    node->symbol = symbol;
    node->freq = freq;
    node->left = node->right = NULL;
    return node;
}

// Создание минимальной кучи
MinHeap* createMinHeap(int capacity) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    if (heap == NULL) {
        fprintf(stderr, "Ошибка выделения памяти для кучи\n");
        exit(EXIT_FAILURE);
    }

    heap->size = 0;
    heap->capacity = capacity;
    heap->array = (Node**)malloc(capacity * sizeof(Node*));

    if (heap->array == NULL) {
        fprintf(stderr, "Ошибка выделения памяти для массива кучи\n");
        free(heap);
        exit(EXIT_FAILURE);
    }

    return heap;
}

// Обмен местами двух узлов
void swapNodes(Node** a, Node** b) {
    Node* temp = *a;
    *a = *b;
    *b = temp;
}

// Восстановление свойства кучи (просеивание вниз)
void heapify(MinHeap* heap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < heap->size && heap->array[left]->freq < heap->array[smallest]->freq) {
        smallest = left;
    }

    if (right < heap->size && heap->array[right]->freq < heap->array[smallest]->freq) {
        smallest = right;
    }

    if (smallest != idx) {
        swapNodes(&heap->array[smallest], &heap->array[idx]);
        heapify(heap, smallest);
    }
}

// Извлечение узла с минимальной частотой
Node* extractMin(MinHeap* heap) {
    if (heap->size <= 0) {
        return NULL;
    }

    Node* minNode = heap->array[0];
    heap->array[0] = heap->array[heap->size - 1];
    heap->size--;
    heapify(heap, 0);

    return minNode;
}

// Вставка нового узла в кучу
void insertMinHeap(MinHeap* heap, Node* node) {
    heap->size++;
    int i = heap->size - 1;

    while (i > 0 && node->freq < heap->array[(i - 1) / 2]->freq) {
        heap->array[i] = heap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }

    heap->array[i] = node;
}

// Построение минимальной кучи
void buildMinHeap(MinHeap* heap) {
    int n = heap->size - 1;
    for (int i = (n - 1) / 2; i >= 0; i--) {
        heapify(heap, i);
    }
}

// Построение дерева Хаффмана
Node* buildHuffmanTree(unsigned int frequencies[]) {
    // Считаем количество уникальных символов
    int unique_count = 0;
    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            unique_count++;
        }
    }

    // Создаем минимальную кучу
    MinHeap* heap = createMinHeap(unique_count);

    // Создаем листья для каждого символа с ненулевой частотой
    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            heap->array[heap->size++] = createNode((unsigned char)i, frequencies[i]);
        }
    }

    // Строим минимальную кучу
    buildMinHeap(heap);

    // Основной цикл построения дерева Хаффмана
    while (heap->size > 1) {
        // Извлекаем два узла с минимальными частотами
        Node* left = extractMin(heap);
        Node* right = extractMin(heap);

        // Создаем новый внутренний узел
        Node* parent = createNode(0, left->freq + right->freq);
        parent->left = left;
        parent->right = right;

        // Вставляем новый узел обратно в кучу
        insertMinHeap(heap, parent);
    }

    // Последний оставшийся узел - корень дерева
    Node* root = extractMin(heap);

    // Освобождаем память кучи (но не узлов!)
    free(heap->array);
    free(heap);

    return root;
}

// Рекурсивная генерация кодов
void generateCodesRecursive(Node* root, char* code, int depth, Code codes[]) {
    if (root == NULL) {
        return;
    }

    // Если это лист, сохраняем код
    if (root->left == NULL && root->right == NULL) {
        code[depth] = '\0';
        codes[root->symbol].symbol = root->symbol;
        strcpy(codes[root->symbol].bits, code);
        codes[root->symbol].length = depth;
        return;
    }

    // Рекурсивно обходим левое поддерево (добавляем '0')
    code[depth] = '0';
    generateCodesRecursive(root->left, code, depth + 1, codes);

    // Рекурсивно обходим правое поддерево (добавляем '1')
    code[depth] = '1';
    generateCodesRecursive(root->right, code, depth + 1, codes);
}

// Генерация кодов для всех символов
void generateCodes(Node* root, Code codes[]) {
    char code[MAX_TREE_HT];
    for (int i = 0; i < ASCII_SIZE; i++) {
        codes[i].length = 0;
        codes[i].bits[0] = '\0';
    }
    generateCodesRecursive(root, code, 0, codes);
}

// Освобождение памяти дерева
void freeHuffmanTree(Node* root) {
    if (root == NULL) {
        return;
    }

    freeHuffmanTree(root->left);
    freeHuffmanTree(root->right);
    free(root);
}

// Подсчет частот символов в файле
void countFrequencies(FILE* file, unsigned int frequencies[]) {
    // Инициализируем массив нулями
    for (int i = 0; i < ASCII_SIZE; i++) {
        frequencies[i] = 0;
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;

    rewind(file);  // Перемещаемся в начало файла

    // Читаем файл блоками для эффективности
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            frequencies[buffer[i]]++;
        }
    }
}

// Побитовая запись закодированных данных
void writeEncodedFile(FILE* input, FILE* output, Code codes[], long* bit_count) {
    unsigned char buffer = 0;  // Буфер для накопления битов
    int bit_pos = 0;           // Позиция текущего бита в буфере (0-7)
    *bit_count = 0;            // Общее количество записанных битов

    unsigned char read_buffer[BUFFER_SIZE];
    size_t bytes_read;

    rewind(input);  // Перемещаемся в начало входного файла

    // Читаем файл блоками
    while ((bytes_read = fread(read_buffer, 1, BUFFER_SIZE, input)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char ch = read_buffer[i];
            Code* code = &codes[ch];

            // Записываем каждый бит кода
            for (int j = 0; j < code->length; j++) {
                if (code->bits[j] == '1') {
                    buffer |= (1 << (7 - bit_pos));
                }

                bit_pos++;
                (*bit_count)++;

                // Если буфер заполнен, записываем его в файл
                if (bit_pos == 8) {
                    fputc(buffer, output);
                    buffer = 0;
                    bit_pos = 0;
                }
            }
        }
    }

    // Если остались незаписанные биты, дополняем буфер нулями и записываем
    if (bit_pos > 0) {
        fputc(buffer, output);
    }
}

// Побитовое чтение и декодирование
void decodeFile(FILE* input, FILE* output, Node* root, long bit_count) {
    Node* current = root;
    unsigned char byte;
    long bits_processed = 0;

    rewind(input);  // Перемещаемся в начало закодированного файла

    // Читаем файл побайтово
    while (bits_processed < bit_count && fread(&byte, 1, 1, input) == 1) {
        // Обрабатываем каждый бит в байте
        for (int i = 7; i >= 0 && bits_processed < bit_count; i--) {
            // Получаем текущий бит
            int bit = (byte >> i) & 1;

            // Переходим по дереву
            if (bit == 0) {
                current = current->left;
            } else {
                current = current->right;
            }

            // Если достигли листа, записываем символ
            if (current->left == NULL && current->right == NULL) {
                fputc(current->symbol, output);
                current = root;  // Возвращаемся к корню
            }

            bits_processed++;
        }
    }
}

// Сравнение двух файлов
int compareFiles(FILE* file1, FILE* file2) {
    unsigned char buffer1[BUFFER_SIZE];
    unsigned char buffer2[BUFFER_SIZE];
    size_t bytes_read1, bytes_read2;

    rewind(file1);
    rewind(file2);

    do {
        bytes_read1 = fread(buffer1, 1, BUFFER_SIZE, file1);
        bytes_read2 = fread(buffer2, 1, BUFFER_SIZE, file2);

        if (bytes_read1 != bytes_read2) {
            return 0;
        }

        if (memcmp(buffer1, buffer2, bytes_read1) != 0) {
            return 0;
        }
    } while (bytes_read1 > 0);

    return 1;
}

// Вывод статистики
void printStatistics(const char* filename, unsigned int frequencies[],
                     Code codes[], long original_size, long compressed_size) {
    printf("\n=== СТАТИСТИКА СЖАТИЯ ===\n");
    printf("Исходный файл: %s\n", filename);
    printf("Размер исходного файла: %ld байт\n", original_size);
    printf("Размер сжатого файла: %ld байт\n", compressed_size);

    if (original_size > 0) {
        double ratio = (double)compressed_size / original_size * 100;
        printf("Коэффициент сжатия: %.2f%%\n", ratio);

        if (compressed_size < original_size) {
            double saved = 100 - ratio;
            printf("✓ Сжатие успешно: экономия %.2f%% (%.2f байт)\n",
                   saved, original_size - compressed_size);
        } else if (compressed_size == original_size) {
            printf("⚠ Сжатие не произошло (размеры равны)\n");
        } else {
            printf("✗ Сжатие неэффективно (файл увеличился на %.2f%%)\n",
                   ratio - 100);
        }
    }

    printf("\n=== ТАБЛИЦА ЧАСТОТ И КОДОВ ===\n");
    printf("%-10s %-10s %-20s %s\n", "Символ", "Частота", "Код", "Длина");
    printf("------------------------------------------------\n");

    int total_symbols = 0;
    int max_freq = 0;
    unsigned char max_freq_symbol = 0;

    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            total_symbols++;

            // Находим самый частый символ
            if (frequencies[i] > max_freq) {
                max_freq = frequencies[i];
                max_freq_symbol = i;
            }

            // Форматированный вывод символа (только для первых 20 символов)
            if (total_symbols <= 20) {
                char symbol_str[10];
                if (i == '\n') {
                    strcpy(symbol_str, "'\\n'");
                } else if (i == '\t') {
                    strcpy(symbol_str, "'\\t'");
                } else if (i == ' ') {
                    strcpy(symbol_str, "' '");
                } else if (i < 32 || i > 126) {
                    sprintf(symbol_str, "0x%02X", i);
                } else {
                    sprintf(symbol_str, "'%c'", (char)i);
                }

                printf("%-10s %-10u %-20s %d\n",
                       symbol_str,
                       frequencies[i],
                       codes[i].bits,
                       codes[i].length);
            }
        }
    }

    if (total_symbols > 20) {
        printf("... и еще %d символов\n", total_symbols - 20);
    }

    printf("\nВсего уникальных символов: %d\n", total_symbols);

    // Информация о самом частом символе
    if (max_freq > 0) {
        char max_symbol_str[10];
        if (max_freq_symbol == '\n') {
            strcpy(max_symbol_str, "'\\n'");
        } else if (max_freq_symbol == '\t') {
            strcpy(max_symbol_str, "'\\t'");
        } else if (max_freq_symbol == ' ') {
            strcpy(max_symbol_str, "' '");
        } else if (max_freq_symbol < 32 || max_freq_symbol > 126) {
            sprintf(max_symbol_str, "0x%02X", max_freq_symbol);
        } else {
            sprintf(max_symbol_str, "'%c'", max_freq_symbol);
        }

        printf("Самый частый символ: %s (встречается %d раз, %.1f%%)\n",
               max_symbol_str, max_freq,
               (double)max_freq / original_size * 100);
    }

    // Вычисляем среднюю длину кода
    unsigned long long total_freq = 0;
    unsigned long long weighted_length = 0;

    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            total_freq += frequencies[i];
            weighted_length += frequencies[i] * codes[i].length;
        }
    }

    if (total_freq > 0) {
        double avg_length = (double)weighted_length / total_freq;
        printf("Средняя длина кода: %.2f бит\n", avg_length);
        printf("Эффективность по сравнению с ASCII (8 бит): %.1f%%\n",
               (1 - avg_length / 8) * 100);
    }
}

// ========== ОСНОВНАЯ ПРОГРАММА ==========
int main(int argc, char* argv[]) {
    // Настройка локали для корректного отображения кириллицы
    setlocale(LC_ALL, "ru_RU.UTF-8");

    printf("==============================================\n");
    printf("Лабораторная работа: Алгоритм Хаффмана\n");
    printf("Вариант на 1-7 баллов\n");
    printf("==============================================\n");

    // Имена файлов по умолчанию
    const char* input_filename = "input.txt";
    const char* encoded_filename = "encoded.bin";
    const char* decoded_filename = "decoded.txt";

    // Если переданы аргументы командной строки
    if (argc > 1) {
        input_filename = argv[1];
        if (argc > 2) {
            encoded_filename = argv[2];
            if (argc > 3) {
                decoded_filename = argv[3];
            }
        }
    }

    printf("Используемые файлы:\n");
    printf("  Входной файл: %s\n", input_filename);
    printf("  Сжатый файл:  %s\n", encoded_filename);
    printf("  Выходной файл: %s\n", decoded_filename);
    printf("\n");

    // Проверяем существование входного файла
    FILE* input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "Ошибка: не удалось открыть файл '%s'\n", input_filename);
        fprintf(stderr, "Создайте файл %s с текстом для сжатия или укажите другой файл\n",
                input_filename);
        fprintf(stderr, "Использование: %s [input.txt] [encoded.bin] [decoded.txt]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    // Замеряем время выполнения
    clock_t start_time = clock();

    // Шаг 1: Подсчет частот символов
    printf("[1/6] Подсчет частот символов...\n");
    unsigned int frequencies[ASCII_SIZE];
    countFrequencies(input_file, frequencies);

    // Получаем размер исходного файла
    fseek(input_file, 0, SEEK_END);
    long original_size = ftell(input_file);
    rewind(input_file);

    printf("   Размер исходного файла: %ld байт\n", original_size);

    // Проверяем, не пустой ли файл
    if (original_size == 0) {
        fprintf(stderr, "Ошибка: файл '%s' пустой\n", input_filename);
        fclose(input_file);
        return EXIT_FAILURE;
    }

    // Шаг 2: Построение дерева Хаффмана
    printf("[2/6] Построение дерева Хаффмана...\n");
    Node* root = buildHuffmanTree(frequencies);
    printf("   Дерево построено успешно\n");

    // Шаг 3: Генерация кодов
    printf("[3/6] Генерация кодов символов...\n");
    Code codes[ASCII_SIZE];
    generateCodes(root, codes);
    printf("   Коды сгенерированы успешно\n");

    // Шаг 4: Кодирование файла
    printf("[4/6] Кодирование исходного файла...\n");
    FILE* encoded_file = fopen(encoded_filename, "wb");
    if (encoded_file == NULL) {
        fprintf(stderr, "Ошибка: не удалось создать файл '%s'\n", encoded_filename);
        fclose(input_file);
        freeHuffmanTree(root);
        return EXIT_FAILURE;
    }

    long bit_count = 0;
    writeEncodedFile(input_file, encoded_file, codes, &bit_count);
    fclose(encoded_file);

    // Получаем размер сжатого файла
    encoded_file = fopen(encoded_filename, "rb");
    fseek(encoded_file, 0, SEEK_END);
    long compressed_size = ftell(encoded_file);
    fclose(encoded_file);

    printf("   Закодированные данные сохранены в '%s'\n", encoded_filename);
    printf("   Использовано бит: %ld (%.2f байт)\n", bit_count, (double)bit_count / 8);

    // Шаг 5: Декодирование файла
    printf("[5/6] Декодирование сжатого файла...\n");
    encoded_file = fopen(encoded_filename, "rb");
    FILE* decoded_file = fopen(decoded_filename, "wb");

    if (encoded_file == NULL || decoded_file == NULL) {
        fprintf(stderr, "Ошибка при открытии файлов для декодирования\n");
        if (encoded_file) fclose(encoded_file);
        if (decoded_file) fclose(decoded_file);
        fclose(input_file);
        freeHuffmanTree(root);
        return EXIT_FAILURE;
    }

    decodeFile(encoded_file, decoded_file, root, bit_count);

    fclose(encoded_file);
    fclose(decoded_file);

    printf("   Декодированные данные сохранены в '%s'\n", decoded_filename);

    // Шаг 6: Проверка корректности
    printf("[6/6] Проверка корректности восстановления...\n");
    decoded_file = fopen(decoded_filename, "rb");

    if (compareFiles(input_file, decoded_file)) {
        printf("   ✓ Восстановление успешно! Файлы идентичны.\n");
    } else {
        printf("   ✗ Ошибка! Восстановленный файл не совпадает с исходным.\n");
    }

    fclose(decoded_file);
    fclose(input_file);

    // Вывод статистики
    printStatistics(input_filename, frequencies, codes, original_size, compressed_size);

    // Замер времени
    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nВремя выполнения: %.3f секунд\n", elapsed_time);

    // Освобождение памяти
    freeHuffmanTree(root);

    printf("\n==============================================\n");
    printf("Программа завершена успешно!\n");
    printf("Для проверки можно сравнить файлы:\n");
    printf("  fc /B %s %s  (Windows)\n", input_filename, decoded_filename);
    printf("  diff %s %s     (Linux/Mac)\n", input_filename, decoded_filename);
    printf("==============================================\n");

    return EXIT_SUCCESS;
}
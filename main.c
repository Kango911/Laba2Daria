/*
 * Лабораторная работа №2: Алгоритм Хаффмана
 * Сжатие данных без потерь с использованием префиксного кодирования
 *
 * Автор: [Ваше имя]
 * Дата: [Дата]
 * Курс: Алгоритмы и структуры данных
 *
 * Этот код реализует алгоритм Хаффмана для сжатия и восстановления файлов
 * Алгоритм Хаффмана - это жадный алгоритм оптимального префиксного кодирования
 * Время работы: O(n log n), где n - количество уникальных символов
 */

// Подключаем необходимые библиотеки
#include <stdio.h>      // Для работы с файлами и вводом/выводом
#include <stdlib.h>     // Для динамического выделения памяти, exit()
#include <string.h>     // Для работы со строками (strcpy, memcmp)
#include <locale.h>     // Для установки локали (поддержка кириллицы)
#include <time.h>       // Для замера времени выполнения (clock())
#include <windows.h>    // Windows-specific: SetConsoleOutputCP, SetConsoleCP
#include <direct.h>     // Для создания директорий (_mkdir)

// ========== КОНСТАНТЫ И СТРУКТУРЫ ==========

// Макросы для задания констант программы
#define BYTE_SIZE 8               // Количество бит в одном байте
#define ASCII_SIZE 256            // Количество возможных ASCII символов (0-255)
#define MAX_TREE_HT 100           // Максимальная высота дерева Хаффмана (ограничение для кодов)
#define BUFFER_SIZE 4096          // Размер буфера для чтения/записи файлов (4KB)

/*
 * Структура Node - узел бинарного дерева Хаффмана
 * Используется для построения дерева кодирования
 */
typedef struct Node {
    unsigned char symbol;   // Символ (хранится только в листьях дерева)
    unsigned int freq;      // Частота появления символа (вес узла)
    struct Node *left;      // Указатель на левого потомка (соответствует биту 0)
    struct Node *right;     // Указатель на правого потомка (соответствует биту 1)
} Node;

/*
 * Структура Code - для хранения сгенерированного кода Хаффмана для символа
 * Используется для быстрого доступа к кодам при кодировании
 */
typedef struct Code {
    unsigned char symbol;   // Символ, которому соответствует код
    char bits[MAX_TREE_HT]; // Строковое представление двоичного кода (например, "101")
    int length;             // Длина кода в битах
} Code;

/*
 * Структура MinHeap - минимальная куча (min-heap)
 * Используется для эффективного извлечения узлов с минимальной частотой
 * при построении дерева Хаффмана
 */
typedef struct MinHeap {
    int size;               // Текущее количество элементов в куче
    int capacity;           // Максимальная емкость кучи (максимальное количество элементов)
    Node** array;           // Массив указателей на узлы дерева Хаффмана
} MinHeap;

// ========== ПРОТОТИПЫ ФУНКЦИЙ ==========

// Функции для работы с деревом Хаффмана и кучей
Node* createNode(unsigned char symbol, unsigned int freq);                // Создание нового узла
MinHeap* createMinHeap(int capacity);                                     // Создание минимальной кучи
void swapNodes(Node** a, Node** b);                                       // Обмен двух указателей на узлы
void heapify(MinHeap* heap, int idx);                                     // Восстановление свойства кучи
Node* extractMin(MinHeap* heap);                                          // Извлечение минимального элемента
void insertMinHeap(MinHeap* heap, Node* node);                            // Вставка узла в кучу
void buildMinHeap(MinHeap* heap);                                         // Построение кучи из массива
Node* buildHuffmanTree(unsigned int frequencies[]);                       // Построение дерева Хаффмана
void generateCodesRecursive(Node* root, char* code, int depth, Code codes[]); // Рекурсивная генерация кодов
void generateCodes(Node* root, Code codes[]);                             // Обертка для генерации кодов
void freeHuffmanTree(Node* root);                                         // Освобождение памяти дерева

// Функции для работы с файлами и сжатия
void countFrequencies(FILE* file, unsigned int frequencies[]);            // Подсчет частот символов
void writeEncodedFile(FILE* input, FILE* output, Code codes[], long* bit_count); // Кодирование файла
void decodeFile(FILE* input, FILE* output, Node* root, long bit_count);   // Декодирование файла
int compareFiles(FILE* file1, FILE* file2);                               // Сравнение двух файлов
void printStatistics(const char* filename, unsigned int frequencies[],    // Вывод статистики
                     Code codes[], long original_size, long compressed_size);

// Основные функции программы
int huffman_compress_decompress(const char* input_filename,               // Полный цикл сжатия-восстановления
                               const char* encoded_filename,
                               const char* decoded_filename);
void createTestFiles();                                                   // Создание тестовых файлов
void showMenu();                                                          // Отображение меню выбора

// ========== РЕАЛИЗАЦИЯ ФУНКЦИЙ ==========

/**
 * Функция createNode - создает новый узел дерева Хаффмана
 * @param symbol - символ (для листьев) или 0 (для внутренних узлов)
 * @param freq - частота символа (вес узла)
 * @return указатель на созданный узел
 *
 * Выделяет память под новый узел, инициализирует его поля
 * и возвращает указатель на него. Если выделение памяти
 * не удалось, программа завершается с ошибкой.
 */
Node* createNode(unsigned char symbol, unsigned int freq) {
    Node* node = (Node*)malloc(sizeof(Node));  // Выделяем память для узла
    if (node == NULL) {                        // Проверяем успешность выделения памяти
        fprintf(stderr, "Ошибка выделения памяти для узла\n");  // Выводим сообщение об ошибке
        exit(EXIT_FAILURE);                    // Завершаем программу с кодом ошибки
    }

    node->symbol = symbol;                     // Устанавливаем символ
    node->freq = freq;                         // Устанавливаем частоту
    node->left = node->right = NULL;           // Инициализируем указатели на потомков как NULL
    return node;                               // Возвращаем указатель на созданный узел
}

/**
 * Функция createMinHeap - создает минимальную кучу заданной емкости
 * @param capacity - максимальное количество элементов в куче
 * @return указатель на созданную кучу
 *
 * Выделяет память для структуры кучи и массива указателей на узлы.
 * Используется для построения дерева Хаффмана.
 */
MinHeap* createMinHeap(int capacity) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));  // Выделяем память для структуры кучи
    if (heap == NULL) {                                 // Проверяем успешность выделения памяти
        fprintf(stderr, "Ошибка выделения памяти для кучи\n");
        exit(EXIT_FAILURE);
    }

    heap->size = 0;                                     // Инициализируем размер кучи как 0
    heap->capacity = capacity;                          // Устанавливаем максимальную емкость
    heap->array = (Node**)malloc(capacity * sizeof(Node*));  // Выделяем память для массива указателей

    if (heap->array == NULL) {                          // Проверяем успешность выделения памяти для массива
        fprintf(stderr, "Ошибка выделения памяти для массива кучи\n");
        free(heap);                                     // Освобождаем память, выделенную для структуры кучи
        exit(EXIT_FAILURE);                             // Завершаем программу
    }

    return heap;                                        // Возвращаем указатель на созданную кучу
}

/**
 * Функция swapNodes - меняет местами два указателя на узлы
 * @param a - первый указатель
 * @param b - второй указатель
 *
 * Используется в функции heapify для восстановления
 * свойства минимальной кучи.
 */
void swapNodes(Node** a, Node** b) {
    Node* temp = *a;    // Сохраняем значение первого указателя во временной переменной
    *a = *b;            // Присваиваем первому указателю значение второго
    *b = temp;          // Присваиваем второму указателю сохраненное значение
}

/**
 * Функция heapify - восстанавливает свойство минимальной кучи
 * для поддерева с корнем в позиции idx
 * @param heap - указатель на минимальную кучу
 * @param idx - индекс корня поддерева
 *
 * Сложность: O(log n)
 * Этот алгоритм также называют "просеиванием вниз" (sift-down).
 * Он гарантирует, что узел с индексом idx будет меньше своих потомков.
 */
void heapify(MinHeap* heap, int idx) {
    int smallest = idx;            // Предполагаем, что текущий узел является наименьшим
    int left = 2 * idx + 1;        // Вычисляем индекс левого потомка
    int right = 2 * idx + 2;       // Вычисляем индекс правого потомка

    // Если левый потомок существует и его частота меньше частоты текущего наименьшего
    if (left < heap->size && heap->array[left]->freq < heap->array[smallest]->freq) {
        smallest = left;           // Обновляем индекс наименьшего элемента
    }

    // Если правый потомок существует и его частота меньше частоты текущего наименьшего
    if (right < heap->size && heap->array[right]->freq < heap->array[smallest]->freq) {
        smallest = right;          // Обновляем индекс наименьшего элемента
    }

    // Если наименьший элемент не является текущим узлом
    if (smallest != idx) {
        swapNodes(&heap->array[smallest], &heap->array[idx]);  // Меняем узлы местами
        heapify(heap, smallest);                               // Рекурсивно вызываем heapify для поддерева
    }
}

/**
 * Функция extractMin - извлекает узел с минимальной частотой из кучи
 * @param heap - указатель на минимальную кучу
 * @return указатель на узел с минимальной частотой
 *
 * Извлекает корневой элемент кучи (минимальный), заменяет его последним
 * элементом, уменьшает размер кучи и восстанавливает свойство кучи.
 * Сложность: O(log n)
 */
Node* extractMin(MinHeap* heap) {
    if (heap->size <= 0) {                         // Если куча пуста
        return NULL;                               // Возвращаем NULL
    }

    Node* minNode = heap->array[0];                // Минимальный элемент всегда в корне (индекс 0)
    heap->array[0] = heap->array[heap->size - 1];  // Перемещаем последний элемент в корень
    heap->size--;                                  // Уменьшаем размер кучи на 1
    heapify(heap, 0);                              // Восстанавливаем свойство кучи начиная с корня

    return minNode;                                // Возвращаем минимальный узел
}

/**
 * Функция insertMinHeap - вставляет новый узел в минимальную кучу
 * @param heap - указатель на минимальную кучу
 * @param node - указатель на узел для вставки
 *
 * Добавляет новый узел в конец кучи, затем "поднимает" его на
 * правильную позицию для сохранения свойства минимальной кучи.
 * Сложность: O(log n)
 */
void insertMinHeap(MinHeap* heap, Node* node) {
    heap->size++;                                   // Увеличиваем размер кучи
    int i = heap->size - 1;                         // Индекс для нового элемента (последняя позиция)

    // "Просеиваем вверх" (sift-up): поднимаем элемент, пока он меньше родителя
    while (i > 0 && node->freq < heap->array[(i - 1) / 2]->freq) {
        heap->array[i] = heap->array[(i - 1) / 2];  // Перемещаем родителя вниз
        i = (i - 1) / 2;                            // Переходим к родительской позиции
    }

    heap->array[i] = node;                          // Устанавливаем новый узел на найденную позицию
}

/**
 * Функция buildMinHeap - строит минимальную кучу из неупорядоченного массива
 * @param heap - указатель на минимальную кучу
 *
 * Применяет heapify ко всем внутренним узлам, начиная с последнего
 * внутреннего узла и двигаясь к корню.
 * Сложность: O(n)
 */
void buildMinHeap(MinHeap* heap) {
    int n = heap->size - 1;                         // Индекс последнего элемента
    // Начинаем с последнего внутреннего узла и идем к корню
    for (int i = (n - 1) / 2; i >= 0; i--) {
        heapify(heap, i);                           // Восстанавливаем свойство кучи для поддерева
    }
}

/**
 * Функция buildHuffmanTree - строит дерево Хаффмана на основе частот символов
 * @param frequencies - массив частот символов (индекс - код символа, значение - частота)
 * @return указатель на корень дерева Хаффмана
 *
 * Алгоритм построения дерева Хаффмана:
 * 1. Создать лист для каждого символа с ненулевой частотой
 * 2. Поместить все листья в минимальную кучу
 * 3. Пока в куче больше одного узла:
 *    a. Извлечь два узла с минимальными частотами
 *    b. Создать новый внутренний узел с частотой, равной сумме частот дочерних узлов
 *    c. Сделать извлеченные узлы левым и правым потомками нового узла
 *    d. Добавить новый узел в кучу
 * 4. Вернуть последний оставшийся узел (корень дерева)
 *
 * Сложность: O(n log n), где n - количество уникальных символов
 */
Node* buildHuffmanTree(unsigned int frequencies[]) {
    // Подсчитываем количество уникальных символов (символов с ненулевой частотой)
    int unique_count = 0;
    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            unique_count++;
        }
    }

    // Создаем минимальную кучу с емкостью, равной количеству уникальных символов
    MinHeap* heap = createMinHeap(unique_count);

    // Создаем листья для каждого символа с ненулевой частотой и добавляем их в кучу
    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            heap->array[heap->size++] = createNode((unsigned char)i, frequencies[i]);
        }
    }

    buildMinHeap(heap);                              // Строим минимальную кучу из массива листьев

    // Основной цикл построения дерева Хаффмана
    while (heap->size > 1) {                         // Пока в куче не останется один узел
        Node* left = extractMin(heap);               // Извлекаем узел с минимальной частотой (левый)
        Node* right = extractMin(heap);              // Извлекаем следующий узел с минимальной частотой (правый)

        // Создаем новый внутренний узел с символом 0 (не используется во внутренних узлах)
        // Частота нового узла равна сумме частот левого и правого потомков
        Node* parent = createNode(0, left->freq + right->freq);
        parent->left = left;                         // Делаем левый узел левым потомком
        parent->right = right;                       // Делаем правый узел правым потомком

        insertMinHeap(heap, parent);                 // Добавляем новый узел обратно в кучу
    }

    Node* root = extractMin(heap);                   // Последний оставшийся узел - корень дерева

    // Освобождаем память, выделенную для кучи (но не для узлов дерева!)
    free(heap->array);
    free(heap);

    return root;                                     // Возвращаем указатель на корень дерева Хаффмана
}

/**
 * Функция generateCodesRecursive - рекурсивно генерирует коды Хаффмана для символов
 * @param root - текущий узел дерева
 * @param code - массив символов, представляющий текущий код (заполняется по мере обхода)
 * @param depth - текущая глубина в дереве (длина текущего кода)
 * @param codes - массив структур Code для сохранения сгенерированных кодов
 *
 * Обходит дерево Хаффмана в глубину (DFS) и генерирует двоичные коды:
 * - При переходе в левого потомка добавляется '0'
 * - При переходе в правого потомка добавляется '1'
 * - При достижении листа сохраняется сгенерированный код
 */
void generateCodesRecursive(Node* root, char* code, int depth, Code codes[]) {
    if (root == NULL) {                              // Базовый случай рекурсии: достигнут NULL
        return;
    }

    // Если текущий узел - лист (не имеет потомков)
    if (root->left == NULL && root->right == NULL) {
        code[depth] = '\0';                          // Добавляем завершающий нуль-символ для строки
        codes[root->symbol].symbol = root->symbol;   // Сохраняем символ
        strcpy(codes[root->symbol].bits, code);      // Копируем сгенерированный код в структуру
        codes[root->symbol].length = depth;          // Сохраняем длину кода
        return;
    }

    // Рекурсивно обходим левое поддерево
    code[depth] = '0';                               // Добавляем '0' при переходе влево
    generateCodesRecursive(root->left, code, depth + 1, codes);

    // Рекурсивно обходим правое поддерево
    code[depth] = '1';                               // Добавляем '1' при переходе вправо
    generateCodesRecursive(root->right, code, depth + 1, codes);
}

/**
 * Функция generateCodes - обертка для рекурсивной генерации кодов Хаффмана
 * @param root - корень дерева Хаффмана
 * @param codes - массив структур Code для сохранения кодов
 *
 * Инициализирует массив кодов и запускает рекурсивную генерацию.
 */
void generateCodes(Node* root, Code codes[]) {
    char code[MAX_TREE_HT];                          // Временный массив для построения кода
    // Инициализируем все коды нулевой длиной и пустой строкой
    for (int i = 0; i < ASCII_SIZE; i++) {
        codes[i].length = 0;
        codes[i].bits[0] = '\0';
    }
    generateCodesRecursive(root, code, 0, codes);    // Начинаем рекурсивную генерацию с корня
}

/**
 * Функция freeHuffmanTree - рекурсивно освобождает память, занятую деревом Хаффмана
 * @param root - корень дерева (или поддерева)
 *
 * Обходит дерево в глубину (post-order traversal) и освобождает память каждого узла.
 */
void freeHuffmanTree(Node* root) {
    if (root == NULL) {                              // Базовый случай: достигнут NULL
        return;
    }

    freeHuffmanTree(root->left);                     // Рекурсивно освобождаем левое поддерево
    freeHuffmanTree(root->right);                    // Рекурсивно освобождаем правое поддерево
    free(root);                                      // Освобождаем память текущего узла
}

/**
 * Функция countFrequencies - подсчитывает частоту появления каждого символа в файле
 * @param file - указатель на открытый файл
 * @param frequencies - массив для сохранения частот (индекс = код символа)
 *
 * Считывает файл блоками для эффективности и подсчитывает,
 * сколько раз встречается каждый символ (0-255).
 */
void countFrequencies(FILE* file, unsigned int frequencies[]) {
    // Инициализируем массив частот нулями
    for (int i = 0; i < ASCII_SIZE; i++) {
        frequencies[i] = 0;
    }

    unsigned char buffer[BUFFER_SIZE];               // Буфер для чтения файла
    size_t bytes_read;                               // Количество прочитанных байт

    rewind(file);                                    // Перемещаем указатель файла в начало

    // Читаем файл блоками по BUFFER_SIZE байт
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        // Обрабатываем каждый прочитанный байт
        for (size_t i = 0; i < bytes_read; i++) {
            frequencies[buffer[i]]++;                // Увеличиваем счетчик для соответствующего символа
        }
    }
}

/**
 * Функция writeEncodedFile - кодирует исходный файл и записывает результат в бинарный файл
 * @param input - входной файл (исходные данные)
 * @param output - выходной файл (закодированные данные)
 * @param codes - массив кодов Хаффмана для каждого символа
 * @param bit_count - указатель на переменную для подсчета общего количества записанных битов
 *
 * Алгоритм кодирования:
 * 1. Для каждого символа из входного файла берем его код Хаффмана
 * 2. Записываем каждый бит кода в битовый буфер
 * 3. Когда буфер заполняется (8 бит), записываем его как один байт в выходной файл
 * 4. В конце дописываем неполный байт, если остались биты
 */
void writeEncodedFile(FILE* input, FILE* output, Code codes[], long* bit_count) {
    unsigned char buffer = 0;                        // Байтовый буфер для накопления битов
    int bit_pos = 0;                                 // Позиция текущего бита в буфере (0-7)
    *bit_count = 0;                                  // Инициализируем счетчик битов

    unsigned char read_buffer[BUFFER_SIZE];          // Буфер для чтения исходного файла
    size_t bytes_read;                               // Количество прочитанных байт

    rewind(input);                                   // Перемещаем указатель входного файла в начало

    // Читаем исходный файл блоками
    while ((bytes_read = fread(read_buffer, 1, BUFFER_SIZE, input)) > 0) {
        // Обрабатываем каждый прочитанный символ
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char ch = read_buffer[i];       // Текущий символ
            Code* code = &codes[ch];                 // Получаем код Хаффмана для этого символа

            // Обрабатываем каждый бит кода
            for (int j = 0; j < code->length; j++) {
                if (code->bits[j] == '1') {          // Если текущий бит равен '1'
                    buffer |= (1 << (7 - bit_pos));  // Устанавливаем соответствующий бит в буфере
                }

                bit_pos++;                           // Переходим к следующей позиции в буфере
                (*bit_count)++;                      // Увеличиваем общий счетчик битов

                // Если буфер заполнен (8 бит)
                if (bit_pos == 8) {
                    fputc(buffer, output);           // Записываем байт в выходной файл
                    buffer = 0;                      // Сбрасываем буфер
                    bit_pos = 0;                     // Сбрасываем позицию бита
                }
            }
        }
    }

    // Если после обработки всех символов остались незаписанные биты
    if (bit_pos > 0) {
        fputc(buffer, output);                       // Записываем последний неполный байт
    }
}

/**
 * Функция decodeFile - декодирует бинарный файл с использованием дерева Хаффмана
 * @param input - закодированный бинарный файл
 * @param output - выходной файл для декодированных данных
 * @param root - корень дерева Хаффмана
 * @param bit_count - общее количество значимых битов в закодированном файле
 *
 * Алгоритм декодирования:
 * 1. Начинаем с корня дерева
 * 2. Для каждого бита в битовом потоке:
 *    - Если бит равен 0, переходим к левому потомку
 *    - Если бит равен 1, переходим к правому потомку
 * 3. При достижении листа записываем соответствующий символ в выходной файл
 * 4. Возвращаемся к корню и повторяем для следующего символа
 */
void decodeFile(FILE* input, FILE* output, Node* root, long bit_count) {
    Node* current = root;                            // Текущий узел в дереве (начинаем с корня)
    unsigned char byte;                              // Текущий прочитанный байт
    long bits_processed = 0;                         // Счетчик обработанных битов

    rewind(input);                                   // Перемещаем указатель файла в начало

    // Читаем файл побайтово, пока не обработаем все значимые биты
    while (bits_processed < bit_count && fread(&byte, 1, 1, input) == 1) {
        // Обрабатываем каждый бит в байте (старший бит обрабатывается первым)
        for (int i = 7; i >= 0 && bits_processed < bit_count; i--) {
            int bit = (byte >> i) & 1;               // Извлекаем i-й бит из байта

            // Переходим по дереву в зависимости от значения бита
            if (bit == 0) {
                current = current->left;             // Бит 0 -> идем влево
            } else {
                current = current->right;            // Бит 1 -> идем вправо
            }

            // Если достигли листа (узла без потомков)
            if (current->left == NULL && current->right == NULL) {
                fputc(current->symbol, output);      // Записываем символ в выходной файл
                current = root;                      // Возвращаемся к корню для декодирования следующего символа
            }

            bits_processed++;                        // Увеличиваем счетчик обработанных битов
        }
    }
}

/**
 * Функция compareFiles - сравнивает два файла на идентичность
 * @param file1 - первый файл
 * @param file2 - второй файл
 * @return 1 если файлы идентичны, 0 если различаются
 *
 * Считывает оба файла блоками и сравнивает их содержимое.
 * Возвращает 1 только если файлы полностью идентичны.
 */
int compareFiles(FILE* file1, FILE* file2) {
    unsigned char buffer1[BUFFER_SIZE];              // Буфер для первого файла
    unsigned char buffer2[BUFFER_SIZE];              // Буфер для второго файла
    size_t bytes_read1, bytes_read2;                 // Количество прочитанных байт из каждого файла

    rewind(file1);                                   // Перемещаем указатель первого файла в начало
    rewind(file2);                                   // Перемещаем указатель второго файла в начало

    do {
        // Читаем блоки из обоих файлов
        bytes_read1 = fread(buffer1, 1, BUFFER_SIZE, file1);
        bytes_read2 = fread(buffer2, 1, BUFFER_SIZE, file2);

        // Если размеры прочитанных блоков разные, файлы не идентичны
        if (bytes_read1 != bytes_read2) {
            return 0;
        }

        // Сравниваем содержимое блоков
        if (memcmp(buffer1, buffer2, bytes_read1) != 0) {
            return 0;
        }
    } while (bytes_read1 > 0);                       // Пока не достигнут конец файла

    return 1;                                        // Все проверки пройдены, файлы идентичны
}

/**
 * Функция printStatistics - выводит статистику сжатия в консоль
 * @param filename - имя исходного файла
 * @param frequencies - массив частот символов
 * @param codes - массив кодов Хаффмана
 * @param original_size - размер исходного файла в байтах
 * @param compressed_size - размер сжатого файла в байтах
 *
 * Выводит:
 * - Исходный и конечный размеры
 * - Коэффициент сжатия
 * - Таблицу частот и кодов для символов
 * - Самый частый символ
 * - Среднюю длину кода
 * - Эффективность сжатия по сравнению с ASCII
 */
void printStatistics(const char* filename, unsigned int frequencies[],
                     Code codes[], long original_size, long compressed_size) {
    printf("\n=== СТАТИСТИКА СЖАТИЯ ===\n");
    printf("Исходный файл: %s\n", filename);
    printf("Размер исходного файла: %ld байт\n", original_size);
    printf("Размер сжатого файла: %ld байт\n", compressed_size);

    if (original_size > 0) {
        double ratio = (double)compressed_size / original_size * 100;  // Коэффициент сжатия в процентах
        printf("Коэффициент сжатия: %.2f%%\n", ratio);

        // Анализ эффективности сжатия
        if (compressed_size < original_size) {
            double saved = 100 - ratio;                                // Процент сэкономленного места
            printf("  Сжатие успешно: экономия %.2f%% (%.2f байт)\n",
                   saved, original_size - compressed_size);
        } else if (compressed_size == original_size) {
            printf("  Сжатие не произошло (размеры равны)\n");
        } else {
            printf("  Сжатие неэффективно (файл увеличился на %.2f%%)\n",
                   ratio - 100);
        }
    }

    printf("\n=== ТАБЛИЦА ЧАСТОТ И КОДОВ ===\n");
    printf("%-10s %-10s %-20s %s\n", "Символ", "Частота", "Код", "Длина");
    printf("------------------------------------------------\n");

    int total_symbols = 0;                            // Общее количество уникальных символов
    int max_freq = 0;                                 // Максимальная частота
    unsigned char max_freq_symbol = 0;                // Символ с максимальной частотой

    // Выводим информацию о символах
    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {                     // Если символ встречается в файле
            total_symbols++;

            // Обновляем информацию о самом частом символе
            if (frequencies[i] > max_freq) {
                max_freq = frequencies[i];
                max_freq_symbol = i;
            }

            // Выводим информацию о первых 20 символах (чтобы не перегружать вывод)
            if (total_symbols <= 20) {
                char symbol_str[10];                  // Строковое представление символа
                // Форматируем вывод в зависимости от типа символа
                if (i == '\n') {
                    strcpy(symbol_str, "'\\n'");      // Символ новой строки
                } else if (i == '\t') {
                    strcpy(symbol_str, "'\\t'");      // Символ табуляции
                } else if (i == ' ') {
                    strcpy(symbol_str, "' '");        // Пробел
                } else if (i < 32 || i > 126) {       // Непечатаемый символ
                    sprintf(symbol_str, "0x%02X", i); // Выводим в шестнадцатеричном формате
                } else {                              // Печатаемый символ
                    sprintf(symbol_str, "'%c'", (char)i);
                }

                printf("%-10s %-10u %-20s %d\n",      // Вывод строки таблицы
                       symbol_str,
                       frequencies[i],
                       codes[i].bits,
                       codes[i].length);
            }
        }
    }

    // Если символов больше 20, выводим информацию об этом
    if (total_symbols > 20) {
        printf("... и еще %d символов\n", total_symbols - 20);
    }

    printf("\nВсего уникальных символов: %d\n", total_symbols);

    // Выводим информацию о самом частом символе
    if (max_freq > 0) {
        char max_symbol_str[10];                      // Строковое представление самого частого символа
        // Форматируем вывод как и ранее
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
               (double)max_freq / original_size * 100);  // Процентное содержание символа
    }

    // Вычисляем среднюю длину кода
    unsigned long long total_freq = 0;                // Сумма всех частот
    unsigned long long weighted_length = 0;           // Сумма произведений частот на длины кодов

    for (int i = 0; i < ASCII_SIZE; i++) {
        if (frequencies[i] > 0) {
            total_freq += frequencies[i];
            weighted_length += frequencies[i] * codes[i].length;
        }
    }

    if (total_freq > 0) {
        double avg_length = (double)weighted_length / total_freq;  // Средняя длина кода в битах
        printf("Средняя длина кода: %.2f бит\n", avg_length);
        // Эффективность по сравнению с кодированием ASCII (8 бит на символ)
        printf("Эффективность по сравнению с ASCII (8 бит): %.1f%%\n",
               (1 - avg_length / 8) * 100);
    }
}

/**
 * Функция huffman_compress_decompress - выполняет полный цикл сжатия и восстановления файла
 * @param input_filename - путь к исходному файлу
 * @param encoded_filename - путь для сохранения сжатого файла
 * @param decoded_filename - путь для сохранения восстановленного файла
 * @return EXIT_SUCCESS при успехе, EXIT_FAILURE при ошибке
 *
 * Выполняет все 6 шагов алгоритма Хаффмана:
 * 1. Подсчет частот символов
 * 2. Построение дерева Хаффмана
 * 3. Генерация кодов
 * 4. Кодирование файла
 * 5. Декодирование файла
 * 6. Проверка корректности
 */
int huffman_compress_decompress(const char* input_filename,
                               const char* encoded_filename,
                               const char* decoded_filename) {

    printf("\n==============================================\n");
    printf("Обработка файла: %s\n", input_filename);
    printf("==============================================\n");

    // Шаг 0: Открываем исходный файл
    FILE* input_file = fopen(input_filename, "rb");   // Открываем в бинарном режиме для чтения
    if (input_file == NULL) {                         // Проверяем успешность открытия
        fprintf(stderr, "Ошибка: не удалось открыть файл '%s'\n", input_filename);
        return EXIT_FAILURE;                          // Возвращаем код ошибки
    }

    clock_t start_time = clock();                     // Запоминаем время начала выполнения

    // Шаг 1: Подсчет частот символов
    printf("[1/6] Подсчет частот символов...\n");
    unsigned int frequencies[ASCII_SIZE];
    countFrequencies(input_file, frequencies);

    // Определяем размер исходного файла
    fseek(input_file, 0, SEEK_END);                   // Перемещаем указатель в конец файла
    long original_size = ftell(input_file);           // Получаем текущую позицию (это и есть размер)
    rewind(input_file);                               // Возвращаем указатель в начало файла

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
    FILE* encoded_file = fopen(encoded_filename, "wb");  // Открываем файл для записи в бинарном режиме
    if (encoded_file == NULL) {
        fprintf(stderr, "Ошибка: не удалось создать файл '%s'\n", encoded_filename);
        fclose(input_file);
        freeHuffmanTree(root);
        return EXIT_FAILURE;
    }

    long bit_count = 0;                               // Переменная для хранения количества битов
    writeEncodedFile(input_file, encoded_file, codes, &bit_count);
    fclose(encoded_file);

    // Определяем размер сжатого файла
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

    // Шаг 6: Проверка корректности восстановления
    printf("[6/6] Проверка корректности восстановления...\n");
    decoded_file = fopen(decoded_filename, "rb");

    if (compareFiles(input_file, decoded_file)) {
        printf("   Восстановление успешно! Файлы идентичны.\n");
    } else {
        printf("   Ошибка! Восстановленный файл не совпадает с исходным.\n");
    }

    fclose(decoded_file);
    fclose(input_file);

    // Вывод статистики сжатия
    printStatistics(input_filename, frequencies, codes, original_size, compressed_size);

    // Замер времени выполнения
    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("\nВремя выполнения: %.3f секунд\n", elapsed_time);

    // Освобождение памяти, выделенной для дерева Хаффмана
    freeHuffmanTree(root);

    printf("\n==============================================\n");
    printf("Обработка завершена успешно!\n");
    printf("==============================================\n");

    return EXIT_SUCCESS;
}

/**
 * Функция createTestFiles - создает тестовые файлы для проверки алгоритма
 *
 * Создает 5 тестовых файлов в папке test/:
 * 1. test1.txt - простой текст на английском
 * 2. test2.txt - текст с повторениями символов (для демонстрации эффективности сжатия)
 * 3. test3.txt - текст со специальными символами
 * 4. test4.txt - пустой файл (для тестирования обработки ошибок)
 * 5. test5.txt - большой файл (1000 строк) для тестирования производительности
 */
void createTestFiles() {
    printf("\nСоздание тестовых файлов...\n");

    _mkdir("test");                                   // Создаем папку test, если ее нет

    // Тест 1: Простой английский текст
    FILE* f1 = fopen("test/test1.txt", "w");          // Открываем файл для записи
    if (f1 == NULL) {
        printf("Ошибка создания файла test1.txt\n");
        return;
    }
    fprintf(f1, "Hello, World!\n");
    fprintf(f1, "This is a simple test file.\n");
    fprintf(f1, "1234567890\n");
    fprintf(f1, "!@#$%%^&*()\n");
    fclose(f1);                                       // Закрываем файл
    printf("  Создан: test/test1.txt\n");

    // Тест 2: Текст с повторениями символов
    FILE* f2 = fopen("test/test2.txt", "w");
    if (f2 == NULL) {
        printf("Ошибка создания файла test2.txt\n");
        return;
    }
    // Много повторений символа 'a' (самый частый символ)
    for (int i = 0; i < 40; i++) fprintf(f2, "a");
    fprintf(f2, "\n");
    // Среднее количество повторений символа 'b'
    for (int i = 0; i < 20; i++) fprintf(f2, "b");
    fprintf(f2, "\n");
    // Меньше повторений для других символов
    for (int i = 0; i < 15; i++) fprintf(f2, "c");
    fprintf(f2, "\n");
    for (int i = 0; i < 10; i++) fprintf(f2, "d");
    fprintf(f2, "\n");
    for (int i = 0; i < 5; i++) fprintf(f2, "e");
    fclose(f2);
    printf("  Создан: test/test2.txt\n");

    // Тест 3: Смешанный текст со специальными символами
    FILE* f3 = fopen("test/test3.txt", "w");
    if (f3 == NULL) {
        printf("Ошибка создания файла test3.txt\n");
        return;
    }
    fprintf(f3, "xK7!pL@2#mN$4%%qR^6&sT*8(uV)0_wY+1=aX-3[cZ]5{eB}7|dC9\\fA;'gS:\"hD,<iF>.jG/?kH\n");
    fprintf(f3, "The quick brown fox jumps over the lazy dog 1234567890\n");
    fprintf(f3, "Test file with special characters\n");
    fclose(f3);
    printf("  Создан: test/test3.txt\n");

    // Тест 4: Пустой файл
    FILE* f4 = fopen("test/test4.txt", "w");
    if (f4 == NULL) {
        printf("Ошибка создания файла test4.txt\n");
        return;
    }
    fclose(f4);
    printf("  Создан: test/test4.txt (пустой)\n");

    // Тест 5: Большой файл
    FILE* f5 = fopen("test/test5.txt", "w");
    if (f5 == NULL) {
        printf("Ошибка создания файла test5.txt\n");
        return;
    }
    // Генерируем 1000 строк текста
    for (int i = 0; i < 1000; i++) {
        fprintf(f5, "Line number %d: Huffman algorithm is an optimal prefix coding algorithm.\n", i+1);
    }
    fclose(f5);
    printf("  Создан: test/test5.txt (большой файл)\n");

    printf("Все тестовые файлы созданы в папке test/\n");
}

/**
 * Функция showMenu - отображает интерактивное меню для выбора тестового файла
 *
 * Предоставляет пользователю возможность:
 * 1. Выбрать один из тестовых файлов
 * 2. Запустить все тесты последовательно
 * 3. Создать/обновить тестовые файлы
 * 4. Выйти из программы
 *
 * Использует рекурсивный вызов для возврата в меню после выполнения теста.
 */
void showMenu() {
    int choice;                                       // Переменная для хранения выбора пользователя

    system("cls");                                    // Очищаем консоль (Windows)
    printf("==============================================\n");
    printf("     ЛАБОРАТОРНАЯ РАБОТА: АЛГОРИТМ ХАФФМАНА\n");
    printf("==============================================\n");
    printf("\nДоступные тестовые файлы:\n");
    printf("1. test1.txt - Простой текст (английский)\n");
    printf("2. test2.txt - Текст с повторениями символов\n");
    printf("3. test3.txt - Смешанный текст (спецсимволы)\n");
    printf("4. test4.txt - Пустой файл\n");
    printf("5. test5.txt - Большой файл\n");
    printf("6. Запустить ВСЕ тесты\n");
    printf("7. Создать/обновить тестовые файлы\n");
    printf("8. Выйти из программы\n");
    printf("\nВыберите вариант (1-8): ");

    // Читаем выбор пользователя
    if (scanf("%d", &choice) != 1) {                  // Проверяем корректность ввода
        printf("Ошибка ввода!\n");
        while (getchar() != '\n');                    // Очищаем буфер ввода
        printf("\nНажмите Enter для продолжения...");
        getchar();
        showMenu();                                   // Рекурсивный вызов меню
        return;
    }

    _mkdir("results");                                // Создаем папку для результатов, если ее нет

    // Обрабатываем выбор пользователя
    switch(choice) {
        case 1:  // Тест 1
            huffman_compress_decompress("test/test1.txt",
                                       "results/test1_encoded.bin",
                                       "results/test1_decoded.txt");
            break;
        case 2:  // Тест 2
            huffman_compress_decompress("test/test2.txt",
                                       "results/test2_encoded.bin",
                                       "results/test2_decoded.txt");
            break;
        case 3:  // Тест 3
            huffman_compress_decompress("test/test3.txt",
                                       "results/test3_encoded.bin",
                                       "results/test3_decoded.txt");
            break;
        case 4:  // Тест 4 (пустой файл)
            huffman_compress_decompress("test/test4.txt",
                                       "results/test4_encoded.bin",
                                       "results/test4_decoded.txt");
            break;
        case 5:  // Тест 5 (большой файл)
            huffman_compress_decompress("test/test5.txt",
                                       "results/test5_encoded.bin",
                                       "results/test5_decoded.txt");
            break;
        case 6:  // Запуск всех тестов
            printf("\nЗапуск всех тестов...\n");
            for (int i = 1; i <= 5; i++) {
                char input[50], encoded[50], decoded[50];
                // Формируем имена файлов для каждого теста
                sprintf(input, "test/test%d.txt", i);
                sprintf(encoded, "results/test%d_encoded.bin", i);
                sprintf(decoded, "results/test%d_decoded.txt", i);

                printf("\n\n=== ТЕСТ %d ===\n", i);
                huffman_compress_decompress(input, encoded, decoded);

                // Пауза между тестами (кроме последнего)
                if (i < 5) {
                    printf("\nНажмите Enter для продолжения...");
                    while (getchar() != '\n');        // Очищаем буфер ввода
                    getchar();                        // Ожидаем нажатия Enter
                }
            }
            break;
        case 7:  // Создание тестовых файлов
            createTestFiles();
            break;
        case 8:  // Выход из программы
            printf("\nВыход из программы.\n");
            exit(0);                                  // Завершаем программу
            break;
        default: // Неверный выбор
            printf("\nНеверный выбор! Пожалуйста, выберите от 1 до 8.\n");
            break;
    }

    // Ожидаем нажатия Enter для возврата в меню
    printf("\nНажмите Enter для возврата в меню...");
    while (getchar() != '\n');                        // Очищаем буфер ввода
    getchar();                                        // Ожидаем нажатия Enter
    showMenu();                                       // Рекурсивный вызов меню
}

/**
 * Основная функция программы - точка входа
 * @param argc - количество аргументов командной строки
 * @param argv - массив аргументов командной строки
 * @return EXIT_SUCCESS при успешном выполнении, EXIT_FAILURE при ошибке
 *
 * Поддерживает два режима работы:
 * 1. С аргументами командной строки: программа.exe входной_файл сжатый_файл декодированный_файл
 * 2. Без аргументов: интерактивный режим с меню
 */
int main(int argc, char* argv[]) {
    // Настройка кодировки консоли Windows для корректного отображения кириллицы
    SetConsoleOutputCP(CP_UTF8);                      // Устанавливаем кодовую страницу вывода в UTF-8
    SetConsoleCP(CP_UTF8);                           // Устанавливаем кодовую страницу ввода в UTF-8

    setlocale(LC_ALL, "ru_RU.UTF-8");                // Устанавливаем локаль для работы с кириллицей

    // Проверяем аргументы командной строки
    if (argc == 4) {
        // Режим 1: Работа с конкретными файлами, указанными в командной строке
        // Формат: программа.exe входной_файл сжатый_файл декодированный_файл
        return huffman_compress_decompress(argv[1], argv[2], argv[3]);
    }
    else if (argc == 1) {
        // Режим 2: Интерактивный режим с меню выбора
        showMenu();
    }
    else {
        // Неправильное количество аргументов
        printf("Использование программы:\n");
        printf("  1. Без аргументов: %s  (запуск с меню)\n", argv[0]);
        printf("  2. С аргументами: %s входной_файл сжатый_файл декодированный_файл\n", argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
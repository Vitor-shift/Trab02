#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h> 

#define SEED    0x12345678
#define MAX_CEP_ENTRIES 100000 // Aumentado para garantir espaço suficiente

typedef enum { SIMPLE_HASH, DOUBLE_HASH } HashType;

typedef struct {
     uintptr_t * table;
     int size;
     int max;
     uintptr_t deleted;
     char * (*get_key)(void *);
     HashType type;
     float load_factor_threshold;
}thash;

typedef struct{
    char cep[9];
    char cidade[256];
    char estado[4];
}tcep_data;

tcep_data *all_ceps[MAX_CEP_ENTRIES];
int num_ceps = 0;

uint32_t hashf(const char* str, uint32_t h){
    for (; *str; ++str) {
        h ^= *str;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

uint32_t hashf2(const char* str, uint32_t h){
    uint32_t hash = 2166136261U;
    for (; *str; ++str) {
        hash = (hash * 31) + (*str);
    }
    return hash % (h - 1) + 1;
}


char *get_cep_key(void *data){
    return ((tcep_data *)data)->cep;
}

// Funções da tabela hash
void init_hash(thash *h, int max, HashType type, float load_factor_threshold){
    h->max = max;
    h->size = 0;
    h->deleted = (uintptr_t)-1; // Valor para marcar como deletado
    h->table = (uintptr_t *)calloc(h->max, sizeof(uintptr_t));
    assert(h->table != NULL);
    h->get_key = get_cep_key;
    h->type = type;
    h->load_factor_threshold = load_factor_threshold;
}

void rehash(thash *h){
    uintptr_t *old_table = h->table;
    int old_max = h->max;

    h->max *= 2; // Dobra o tamanho
    h->size = 0; // Resetar size
    h->table = (uintptr_t *)calloc(h->max, sizeof(uintptr_t));
    assert(h->table != NULL);

    for (int i = 0; i < old_max; i++){
        if (old_table[i] != 0 && old_table[i] != h->deleted){
            // Reinserir elemento na nova tabela
            int pos;
            tcep_data *data = (tcep_data *)old_table[i];
            const char *key = h->get_key(data);

            uint32_t h1 = hashf(key, SEED) % h->max;
            uint32_t h2 = 0;
            if (h->type == DOUBLE_HASH) {
                h2 = hashf2(key, h->max);
            }

            for (int j = 0; j < h->max; j++){
                if (h->type == SIMPLE_HASH){
                    pos = (h1 + j) % h->max;
                } else { // DOUBLE_HASH
                    pos = (h1 + j * h2) % h->max;
                }

                if (h->table[pos] == 0){
                    h->table[pos] = (uintptr_t)data;
                    h->size++;
                    break;
                }
            }
        }
    }
    free(old_table);
}

int insert_hash(thash *h, void *data){
    const char *key = h->get_key(data);
    uint32_t h1 = hashf(key, SEED) % h->max;
    uint32_t h2 = 0;

    if (h->type == DOUBLE_HASH) {
        h2 = hashf2(key, h->max);
    }

    if ((float)(h->size + 1) / h->max > h->load_factor_threshold) {
        rehash(h);
    }

    int pos;
    for (int j = 0; j < h->max; j++){
        if (h->type == SIMPLE_HASH){
            pos = (h1 + j) % h->max;
        } else { // DOUBLE_HASH
            pos = (h1 + j * h2) % h->max;
        }

        if (h->table[pos] == 0 || h->table[pos] == h->deleted){
            h->table[pos] = (uintptr_t)data;
            h->size++;
            return 0; // Sucesso na inserção
        }
    }
    // Se o loop terminar sem encontrar um slot vazio, a inserção falhou.
    fprintf(stderr, "Nao foi possivel inserir chave %s. Tabela cheia ou loop infinito de probing.\n", key);
    return 1; // Falha na inserção
}

void *search_hash(thash *h, const char *key){
    uint32_t h1 = hashf(key, SEED) % h->max;
    uint32_t h2 = 0;

    if (h->type == DOUBLE_HASH) {
        h2 = hashf2(key, h->max);
    }

    int pos;
    for (int j = 0; j < h->max; j++){
        if (h->type == SIMPLE_HASH){
            pos = (h1 + j) % h->max;
        } else { // DOUBLE_HASH
            pos = (h1 + j * h2) % h->max;
        }

        if (h->table[pos] == 0){ // Posição vazia, elemento não está na tabela
            return NULL;
        }
        if (h->table[pos] != h->deleted){
            tcep_data *current_data = (tcep_data *)h->table[pos];
            if (strcmp(h->get_key(current_data), key) == 0){
                return current_data;
            }
        }
    }
    return NULL;
}

void free_hash(thash *h){
    free(h->table);
    h->table = NULL;
    h->size = 0;
    h->max = 0;
}

// Carregar CEPs do CSV
void load_ceps_from_csv(const char *filename){
    FILE *file = fopen(filename, "r");
    if (!file){
        perror("Erro ao abrir o arquivo CSV");
        exit(EXIT_FAILURE);
    }

    char line[1024];
    while (fgets(line, sizeof(line), file) && num_ceps < MAX_CEP_ENTRIES){
        tcep_data *new_cep = (tcep_data *)malloc(sizeof(tcep_data));
        assert(new_cep != NULL);

        // Parse da linha: cep,cidade,estado
        char *token = strtok(line, ",");
        if (token) strcpy(new_cep->cep, token); else break;

        token = strtok(NULL, ",");
        if (token) strcpy(new_cep->cidade, token); else break;

        token = strtok(NULL, "\n"); // Pega até o final da linha
        if (token) strcpy(new_cep->estado, token); else break;

        all_ceps[num_ceps++] = new_cep;
    }
    fclose(file);
}

// Funções auxiliares para os testes de inserção e busca
void insere_ceps(thash *h, int num_to_insert){
    int inserted_count = 0;
    printf("Tentando inserir %d CEPs na tabela (max_size: %d, current_size: %d, type: %s).\n",
           num_to_insert, h->max, h->size, (h->type == SIMPLE_HASH ? "Simple" : "Double"));
    for (int i = 0; i < num_to_insert; i++){
        // Garante que não tentaremos inserir mais CEPs do que temos disponíveis
        if (i >= num_ceps) {
            fprintf(stderr, "Tentando inserir mais CEPs do que o total carregado. Parando.\n");
            break;
        }
        if (insert_hash(h, all_ceps[i]) == 0) { // Verifica o retorno
            inserted_count++;
        } else {
            // Se a inserção falhar, imprime um aviso e continua para o próximo
            fprintf(stderr, "Falha ao inserir CEP %s. Total inserido ate agora: %d.\n", all_ceps[i]->cep, inserted_count);
        }
    }
    printf("Concluido a tentativa de inserir %d CEPs. %d inseridos com sucesso. Tamanho final da hash: %d.\n",
           num_to_insert, inserted_count, h->size);
}

void busca_ceps(thash *h, int num_to_search){
    for (int i = 0; i < num_to_search; i++){
        // Busca um CEP existente
        search_hash(h, all_ceps[i]->cep);
    }
}


// --- Funções para o Comparativo 1: Tempo de busca por taxa de ocupação ---
void busca_por_taxa(thash *h_ptr, int initial_max_size, HashType type, float target_load_factor) {
    thash h;
    init_hash(&h, initial_max_size, type, 0.99); // Threshold alto para não rehashar durante a inserção inicial

    int num_to_insert = (int)(initial_max_size * target_load_factor);
    if (num_to_insert > num_ceps) num_to_insert = num_ceps; // Limita ao número de CEPs disponíveis
    if (num_to_insert == 0 && target_load_factor > 0) num_to_insert = 1; // Garante ao menos 1 inserção para 0% ou muito pequeno

    insere_ceps(&h, num_to_insert);

    busca_ceps(&h, num_to_insert); // Busca os mesmos CEPs que foram inseridos

    free_hash(&h);
}

// Funções de busca com diferentes taxas de ocupação para Hash Simples
void busca10_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.10); }
void busca20_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.20); }
void busca30_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.30); }
void busca40_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.40); }
void busca50_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.50); }
void busca60_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.60); }
void busca70_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.70); }
void busca80_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.80); }
void busca90_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.90); }
void busca99_simple_hash() { thash h; busca_por_taxa(&h, 6100, SIMPLE_HASH, 0.99); }

// Funções de busca com diferentes taxas de ocupação para Double Hash
void busca10_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.10); }
void busca20_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.20); }
void busca30_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.30); }
void busca40_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.40); }
void busca50_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.50); }
void busca60_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.60); }
void busca70_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.70); }
void busca80_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.80); }
void busca90_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.90); }
void busca99_double_hash() { thash h; busca_por_taxa(&h, 6100, DOUBLE_HASH, 0.99); }

// --- Funções para o Comparativo 2: Tempo de inserção com diferentes tamanhos iniciais ---
void insere_todos(thash *h_ptr, int initial_max_size, HashType type) {
    thash h;
    init_hash(&h, initial_max_size, type, 0.70); // Fator de carga padrão para rehash (pode ajustar)
    insere_ceps(&h, num_ceps); // Insere todos os CEPs carregados
    free_hash(&h);
}

void insere_todos_6100_simple_hash() { insere_todos(NULL, 6100, SIMPLE_HASH); }
void insere_todos_6100_double_hash() { insere_todos(NULL, 6100, DOUBLE_HASH); }
void insere_todos_1000_simple_hash() { insere_todos(NULL, 1000, SIMPLE_HASH); }
void insere_todos_1000_double_hash() { insere_todos(NULL, 1000, DOUBLE_HASH); }


int main(int argc, char* argv[]){
    printf("Iniciando o programa de teste de hash...\n");

    load_ceps_from_csv("ceps_data.csv");

    printf("-> %d CEPs foram carregados do arquivo 'ceps_data.csv'.\n", num_ceps);

    printf("\n--- Testes de Busca (Comparativo 1) ---\n");
    printf("Executando testes de busca para Hash Simples (6100 buckets, varias taxas de ocupacao)...\n");
    busca10_simple_hash();
    busca20_simple_hash();
    busca30_simple_hash();
    busca40_simple_hash();
    busca50_simple_hash();
    busca60_simple_hash();
    busca70_simple_hash();
    busca80_simple_hash();
    busca90_simple_hash();
    busca99_simple_hash();

    printf("Executando testes de busca para Double Hash (6100 buckets, varias taxas de ocupacao)...\n");
    busca10_double_hash();
    busca20_double_hash();
    busca30_double_hash();
    busca40_double_hash();
    busca50_double_hash();
    busca60_double_hash();
    busca70_double_hash();
    busca80_double_hash();
    busca90_double_hash();
    busca99_double_hash();

    printf("\n--- Testes de Insercao (Comparativo 2) ---\n");
    printf("Executando testes de insercao para Hash Simples (6100 buckets iniciais, todos os CEPs)...\n");
    insere_todos_6100_simple_hash();

    printf("Executando testes de insercao para Double Hash (6100 buckets iniciais, todos os CEPs)...\n");
    insere_todos_6100_double_hash();

    printf("Executando testes de insercao para Hash Simples (1000 buckets iniciais, todos os CEPs)...\n");
    insere_todos_1000_simple_hash();

    printf("Executando testes de insercao para Double Hash (1000 buckets iniciais, todos os CEPs)...\n");
    insere_todos_1000_double_hash();


    // Libera a memória dos CEPs carregados
    for (int i = 0; i < num_ceps; i++) {
        free(all_ceps[i]);
    }

    printf("\nPrograma finalizado.\n");

    return 0;
}
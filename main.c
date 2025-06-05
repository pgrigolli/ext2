#define _POSIX_C_SOURCE 200809L // Para expor S_IFREG e outras macros POSIX

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h> // For uint32_t, uint16_t
#include <string.h> // Para strcmp, strrchr, etc.
#include <sys/stat.h> // Para S_ISDIR, S_ISREG, etc. (usado na impressão do i_mode)
#include <time.h>     // Para ctime() na formatação de datas
#include <stddef.h> // Para offsetof

// Estrutura do Superbloco Ext2. Contém informações globais sobre o sistema de arquivos.
// Todos os valores são armazenados em little-endian no disco.
struct ext2_super_block {
    uint32_t s_inodes_count;      // Contagem total de inodes
    uint32_t s_blocks_count;      // Contagem total de blocos
    uint32_t s_r_blocks_count;    // Contagem de blocos reservados (para o superusuário)
    uint32_t s_free_blocks_count; // Contagem de blocos livres
    uint32_t s_free_inodes_count; // Contagem de inodes livres
    uint32_t s_first_data_block;  // Primeiro bloco de dados (0 ou 1, dependendo do tamanho do bloco)
    uint32_t s_log_block_size;    // Tamanho do bloco (log2(tamanho_do_bloco) - 10). Ex: 0 para 1KB.
    uint32_t s_log_frag_size;     // Tamanho do fragmento (log2(tamanho_do_fragmento) - 10)
    uint32_t s_blocks_per_group;  // Blocos por grupo
    uint32_t s_frags_per_group;   // Fragmentos por grupo
    uint32_t s_inodes_per_group;  // Inodes por grupo
    uint32_t s_mtime;             // Tempo da última montagem
    uint32_t s_wtime;             // Tempo da última escrita
    uint16_t s_mnt_count;         // Contagem de montagens desde a última checagem
    uint16_t s_max_mnt_count;     // Contagem máxima de montagens antes de forçar checagem
    uint16_t s_magic;             // Número mágico (0xEF53 para Ext2)
    uint16_t s_state;             // Estado do sistema de arquivos
    uint16_t s_errors;            // Comportamento ao detectar erros
    uint16_t s_minor_rev_level;   // Nível de revisão menor
    uint32_t s_lastcheck;         // Tempo da última checagem (fsck)
    uint32_t s_checkinterval;     // Intervalo máximo entre checagens
    uint32_t s_creator_os;        // SO que criou o sistema de arquivos
    uint32_t s_rev_level;         // Nível de revisão (0 = antiga, 1 = dinâmica)
    uint16_t s_def_resuid;        // UID padrão para blocos reservados
    uint16_t s_def_resgid;        // GID padrão para blocos reservados
    // Campos estendidos do Superbloco (se s_rev_level >= 1)
    uint32_t s_first_ino;         // Primeiro inode não reservado
    uint16_t s_inode_size;        // Tamanho da estrutura do inode
    uint16_t s_block_group_nr;    // Número do grupo de blocos desta cópia do superbloco
    uint32_t s_feature_compat;    // Conjunto de funcionalidades compatíveis
    uint32_t s_feature_incompat;  // Conjunto de funcionalidades incompatíveis
    uint32_t s_feature_ro_compat; // Conjunto de funcionalidades somente leitura
    uint8_t  s_uuid[16];          // UUID (identificador único universal) do volume
    char     s_volume_name[16];   // Nome do volume
    char     s_last_mounted[64];  // Diretório onde foi montado pela última vez
    uint32_t s_algo_bitmap;       // Bitmap de algoritmos de compressão (uso varia)
};

// Estrutura do Descritor de Grupo de Blocos (Block Group Descriptor).
// Descreve as características de cada grupo de blocos.
struct ext2_group_desc {
    uint32_t bg_block_bitmap;        // Bloco do bitmap de blocos (indica blocos livres/usados)
    uint32_t bg_inode_bitmap;        // Bloco do bitmap de inodes (indica inodes livres/usados)
    uint32_t bg_inode_table;         // Bloco do início da tabela de inodes
    uint16_t bg_free_blocks_count;   // Contagem de blocos livres neste grupo
    uint16_t bg_free_inodes_count;   // Contagem de inodes livres neste grupo
    uint16_t bg_used_dirs_count;     // Contagem de diretórios neste grupo
    uint16_t bg_pad;                 // Alinhamento (não usado, para preenchimento)
    uint32_t bg_reserved[3];         // Reservado
};

// Estrutura do Inode (índice de nó). Contém metadados sobre um arquivo ou diretório.
struct ext2_inode {
    uint16_t i_mode;        // Tipo de arquivo e permissões
    uint16_t i_uid;         // ID do usuário (UID) proprietário
    uint32_t i_size;        // Tamanho do arquivo em bytes
    uint32_t i_atime;       // Tempo do último acesso (access time)
    uint32_t i_ctime;       // Tempo de criação do inode (creation time)
    uint32_t i_mtime;       // Tempo da última modificação do arquivo (modification time)
    uint32_t i_dtime;       // Tempo de deleção (deletion time)
    uint16_t i_gid;         // ID do grupo (GID) proprietário
    uint16_t i_links_count; // Contagem de links (hard links) para este inode
    uint32_t i_blocks;      // Número de blocos de 512 bytes alocados (não blocos do FS!)
    uint32_t i_flags;       // Flags do inode (ex: imutável, somente append)
    uint32_t i_osd1;        // Dependente do OS 1 (geralmente para Linux, salva geração de arquivo ou ACL)
    uint32_t i_block[15];   // Ponteiros para os blocos de dados:
                            // 12 diretos, 1 indireto, 1 duplo indireto, 1 triplo indireto
    uint32_t i_generation;  // Número de geração do arquivo (para NFS)
    uint32_t i_file_acl;    // ACL do arquivo (obsoleto ou usado para ACLs estendidas)
    uint32_t i_dir_acl;     // ACL do diretório (ou 32 bits superiores do tamanho do arquivo se i_mode for arquivo regular)
    uint32_t i_faddr;       // Endereço do fragmento (obsoleto)
    uint8_t  i_osd2[12];    // Dependente do OS 2
};

// Estrutura de uma Entrada de Diretório. Linka um nome a um inode.
// Inclui o tipo do arquivo para evitar leitura do inode para `ls -F`.
struct ext2_dir_entry_2 {
    uint32_t inode;         // Número do Inode (0 se a entrada não for usada ou for vazia)
    uint16_t rec_len;       // Comprimento total desta entrada (incluindo todos os campos e o nome)
    uint8_t  name_len;      // Comprimento do nome em bytes
    uint8_t  file_type;     // Tipo do arquivo (ver EXT2_FT_* abaixo)
    char name[255 + 1];     // Nome do arquivo (255 é EXT2_NAME_LEN, +1 para terminador null)
};

// Constantes para 'file_type' em 'ext2_dir_entry_2' (baseadas em include/linux/ext2_fs.h)
#define EXT2_FT_UNKNOWN  0 // Tipo desconhecido
#define EXT2_FT_REG_FILE 1 // Arquivo regular
#define EXT2_FT_DIR      2 // Diretório
#define EXT2_FT_CHRDEV   3 // Dispositivo de caractere
#define EXT2_FT_BLKDEV   4 // Dispositivo de bloco
#define EXT2_FT_FIFO     5 // FIFO (pipe nomeado)
#define EXT2_FT_SOCK     6 // Socket
#define EXT2_FT_SYMLINK  7 // Link simbólico
#define EXT2_FT_MAX      8 // Usado para validação de tipo

#define EXT2_NAME_LEN 255 // Comprimento máximo do nome de arquivo em uma entrada de diretório

#define EXT2_ROOT_INO 2 // O Inode do diretório raiz é sempre 2

#define SUPERBLOCK_OFFSET 1024 // O superbloco começa em 1024 bytes do início da imagem
#define BLOCK_SIZE_FIXED 1024 // Tamanho do bloco fixo em 1024 bytes para este projeto

// Constantes para 's_rev_level' e tamanho do inode (para clareza e compatibilidade)
#define EXT2_GOOD_OLD_REV 0      // Nível de revisão original (inode_size = 128)
#define EXT2_DYNAMIC_REV  1      // Nível de revisão dinâmico (inode_size variável, >128)
#define EXT2_GOOD_OLD_INODE_SIZE 128 // Tamanho padrão do inode para EXT2_GOOD_OLD_REV

#define EXT2_N_BLOCKS 15  // Número total de ponteiros de bloco em um inode (12 diretos + 3 indiretos)

// Função para ler o superbloco de uma imagem de disco Ext2.
// Abre o arquivo da imagem, posiciona no offset do superbloco e lê os dados.
// Retorna o file descriptor (fd) em caso de sucesso, -1 em caso de erro.
int read_superblock(const char *device_path, struct ext2_super_block *sb) {
    int fd = open(device_path, O_RDWR); // Abre para leitura e escrita
    if (fd < 0) {
        perror("Erro ao abrir a imagem do disco");
        return -1;
    }

    if (lseek(fd, SUPERBLOCK_OFFSET, SEEK_SET) < 0) { // Posiciona no offset do superbloco
        perror("Erro ao posicionar para o superbloco");
        close(fd);
        return -1;
    }

    if (read(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) { // Lê o superbloco
        perror("Erro ao ler o superbloco");
        close(fd);
        return -1;
    }
    return fd; // Sucesso, retorna o file descriptor
}

// Função para escrever o superbloco de volta para a imagem de disco.
// Posiciona no offset do superbloco e escreve os dados do superbloco fornecido.
// Retorna 0 em sucesso, -1 em erro.
int write_superblock(int fd, const struct ext2_super_block *sb) {
    if (lseek(fd, SUPERBLOCK_OFFSET, SEEK_SET) < 0) { // Posiciona para escrita do superbloco
        perror("Erro ao posicionar para escrita do superbloco");
        return -1;
    }

    if (write(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) { // Escreve o superbloco
        perror("Erro ao escrever o superbloco");
        return -1;
    }
    return 0; // Sucesso
}

// Função para ler a Tabela de Descritores de Grupo de Blocos (BGDT).
// Aloca memória para a tabela e a lê do disco.
// Retorna um ponteiro para a BGDT alocada dinamicamente, ou NULL em erro.
// O chamador é responsável por liberar a memória com free().
struct ext2_group_desc* read_block_group_descriptor_table(
    int fd,
    const struct ext2_super_block *sb,
    unsigned int *num_block_groups_out
) {
    // Calcula o número total de grupos de blocos
    unsigned int num_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    if (num_block_groups_out) {
        *num_block_groups_out = num_block_groups;
    }

    size_t bgdt_size = num_block_groups * sizeof(struct ext2_group_desc);
    struct ext2_group_desc *bgdt = (struct ext2_group_desc *)malloc(bgdt_size); // Aloca memória para a BGDT
    if (!bgdt) {
        perror("Erro ao alocar memória para a BGDT");
        return NULL;
    }

    // A BGDT começa no bloco seguinte ao superbloco.
    // Com BLOCK_SIZE_FIXED = 1024, o superbloco está no offset 1024 (Bloco 1).
    // Então, a BGDT começa no Bloco 2 (offset 2048).
    off_t bgdt_offset = SUPERBLOCK_OFFSET + BLOCK_SIZE_FIXED; 

    printf("Calculando BGDT: %u grupos, offset: %ld, tamanho total: %zu bytes\n", num_block_groups, (long)bgdt_offset, bgdt_size);

    if (lseek(fd, bgdt_offset, SEEK_SET) < 0) { // Posiciona para leitura da BGDT
        perror("Erro ao posicionar para a BGDT");
        free(bgdt);
        return NULL;
    }

    if (read(fd, bgdt, bgdt_size) != bgdt_size) { // Lê a BGDT
        perror("Erro ao ler a BGDT");
        free(bgdt);
        return NULL;
    }

    printf("BGDT lida com sucesso!\n");
    return bgdt;
}

// Função para ler um inode específico.
// Calcula o offset do inode no disco com base em seu número e no grupo de blocos a que pertence.
// Retorna 0 em sucesso, -1 em erro. O inode lido é armazenado em 'inode_out'.
int read_inode(
    int fd, 
    const struct ext2_super_block *sb, 
    const struct ext2_group_desc *bgdt, 
    uint32_t inode_num, 
    struct ext2_inode *inode_out
) {
    if (inode_num == 0) {
        fprintf(stderr, "Erro: Número de inode inválido (0).\n");
        return -1;
    }

    // Calcula a qual grupo de blocos o inode pertence (inodes são numerados a partir de 1).
    uint32_t block_group_index = (inode_num - 1) / sb->s_inodes_per_group;
    
    // Obtém o descritor do grupo de blocos correspondente.
    const struct ext2_group_desc *group_descriptor = &bgdt[block_group_index];

    // Calcula o tamanho do inode na estrutura de disco.
    // Para revisão antiga (0), o tamanho é fixo em 128 bytes.
    // Para revisão dinâmica (>=1), o tamanho é especificado no superbloco.
    uint16_t inode_size = EXT2_GOOD_OLD_INODE_SIZE;
    if (sb->s_rev_level >= EXT2_DYNAMIC_REV) {
        if (sb->s_inode_size > 0 ) {
             inode_size = sb->s_inode_size;
        }
    }

    // Calcula o índice do inode dentro da tabela de inodes do seu grupo.
    uint32_t index_in_group = (inode_num - 1) % sb->s_inodes_per_group;

    // Calcula o offset final do inode no disco.
    // bg_inode_table é o número do bloco onde a tabela de inodes começa.
    off_t inode_table_start_offset = (off_t)group_descriptor->bg_inode_table * BLOCK_SIZE_FIXED;
    off_t inode_offset_in_table = index_in_group * inode_size;
    off_t final_inode_offset = inode_table_start_offset + inode_offset_in_table;

    if (lseek(fd, final_inode_offset, SEEK_SET) < 0) { // Posiciona para leitura do inode
        char err_msg[200];
        snprintf(err_msg, sizeof(err_msg), "Erro ao posicionar para o inode %u (offset %ld)", inode_num, (long)final_inode_offset);
        perror(err_msg);
        return -1;
    }

    if (read(fd, inode_out, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) { // Lê o inode
        perror("Erro ao ler o inode");
        return -1;
    }
    return 0; // Sucesso
}

// Função auxiliar para ler um bloco de dados do disco.
// Lê o conteúdo do bloco especificado em 'block_num' para 'buffer'.
// Retorna 0 em sucesso, -1 em erro. 'buffer' deve ter pelo menos BLOCK_SIZE_FIXED bytes.
int read_data_block(int fd, uint32_t block_num, char *buffer) {
    if (block_num == 0) { // Bloco 0 é especial (pode ser boot block ou usado para sparse files)
        memset(buffer, 0, BLOCK_SIZE_FIXED); // Preenche com zeros para representar um bloco não alocado
        return 0; 
    }

    off_t offset = (off_t)block_num * BLOCK_SIZE_FIXED; // Calcula o offset do bloco
    if (lseek(fd, offset, SEEK_SET) < 0) { // Posiciona para leitura do bloco
        char err_msg[100];
        snprintf(err_msg, sizeof(err_msg), "Erro ao posicionar para o bloco de dados %u (offset %ld)", block_num, (long)offset);
        perror(err_msg);
        return -1;
    }

    if (read(fd, buffer, BLOCK_SIZE_FIXED) != BLOCK_SIZE_FIXED) { // Lê o bloco
        perror("Erro ao ler o bloco de dados");
        return -1;
    }
    return 0; // Sucesso
}

// Função auxiliar para escrever um bloco de dados no disco.
// Escreve o conteúdo de 'buffer' para o bloco especificado por 'block_num'.
// Retorna 0 em sucesso, -1 em erro.
int write_data_block(int fd, uint32_t block_num, const char *buffer) {
    if (block_num == 0) {
        fprintf(stderr, "Erro write_data_block: Tentativa de escrever no bloco de dados 0.\n");
        return -1; 
    }
    off_t offset = (off_t)block_num * BLOCK_SIZE_FIXED; // Calcula o offset do bloco
    if (lseek(fd, offset, SEEK_SET) < 0) { // Posiciona para escrita do bloco
        char err_msg[100];
        snprintf(err_msg, sizeof(err_msg), "Erro write_data_block: posicionar para bloco %u", block_num);
        perror(err_msg);
        return -1;
    }
    if (write(fd, buffer, BLOCK_SIZE_FIXED) != BLOCK_SIZE_FIXED) { // Escreve o bloco
        perror("Erro write_data_block: ao escrever bloco de dados");
        return -1;
    }
    return 0; 
}

// Função para escrever uma entrada (inode) na tabela de inodes.
// Escreve o conteúdo de 'inode_to_write' para o slot do inode 'inode_num' no disco.
// Retorna 0 em sucesso, -1 em erro.
int write_inode_table_entry(int fd, const struct ext2_super_block *sb,
                            const struct ext2_group_desc *bgdt, 
                            uint32_t inode_num, const struct ext2_inode *inode_to_write) {
    if (inode_num == 0) {
        fprintf(stderr, "Erro write_inode_table_entry: Tentativa de escrever no inode 0.\n");
        return -1;
    }
    // Calcula o índice do grupo de blocos e o tamanho do inode no disco.
    uint32_t block_group_index = (inode_num - 1) / sb->s_inodes_per_group;
    const struct ext2_group_desc *group_descriptor = &bgdt[block_group_index];
    uint16_t inode_size_on_disk = (sb->s_rev_level == EXT2_GOOD_OLD_REV) ? 
                                  EXT2_GOOD_OLD_INODE_SIZE : sb->s_inode_size;
    if (inode_size_on_disk < EXT2_GOOD_OLD_INODE_SIZE) inode_size_on_disk = EXT2_GOOD_OLD_INODE_SIZE;

    // Calcula o offset final do inode no disco.
    uint32_t index_in_group = (inode_num - 1) % sb->s_inodes_per_group;
    off_t inode_table_start_block_offset = (off_t)group_descriptor->bg_inode_table * BLOCK_SIZE_FIXED;
    off_t inode_offset_in_table = index_in_group * inode_size_on_disk;
    off_t final_inode_offset = inode_table_start_block_offset + inode_offset_in_table;

    if (lseek(fd, final_inode_offset, SEEK_SET) < 0) { // Posiciona para escrita do inode
        perror("Erro write_inode_table_entry: lseek");
        return -1;
    }
    if (write(fd, inode_to_write, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) { // Escreve o inode
        perror("Erro write_inode_table_entry: write");
        return -1;
    }
    return 0; 
}

// Função para escrever um descritor de grupo específico na BGDT.
// Retorna 0 em sucesso, -1 em erro.
int write_group_descriptor(int fd, const struct ext2_super_block *sb, 
                           uint32_t group_index_to_write, 
                           const struct ext2_group_desc *group_desc_to_write) {
    // Calcula o offset da BGDT, que começa após o superbloco.
    off_t bgdt_base_offset = SUPERBLOCK_OFFSET + BLOCK_SIZE_FIXED;
    // Calcula o offset do descritor de grupo específico.
    off_t specific_group_desc_offset = bgdt_base_offset + (group_index_to_write * sizeof(struct ext2_group_desc));

    // Valida o índice do grupo.
    unsigned int num_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    if (group_index_to_write >= num_block_groups) {
        fprintf(stderr, "Erro write_group_descriptor: Índice de grupo %u é inválido (total de grupos %u).\n", 
                group_index_to_write, num_block_groups);
        return -1;
    }

    if (lseek(fd, specific_group_desc_offset, SEEK_SET) < 0) { // Posiciona para escrita
        perror("Erro write_group_descriptor: lseek");
        return -1;
    }

    if (write(fd, group_desc_to_write, sizeof(struct ext2_group_desc)) != sizeof(struct ext2_group_desc)) { // Escreve o descritor
        perror("Erro write_group_descriptor: write");
        return -1;
    }
    return 0; // Sucesso
}

// Implementa o comando 'info', que exibe informações detalhadas do superbloco Ext2.
void comando_info(struct ext2_super_block *sb) {
    printf("--- Informações do Superbloco ---\n");
    printf("Magic number: 0x%X (Esperado: 0xEF53)\n", sb->s_magic);
    if (sb->s_magic != 0xEF53) {
        printf("ERRO: Magic number não corresponde ao Ext2!\n");
    }
    printf("Total de inodes: %u\n", sb->s_inodes_count);
    printf("Total de blocos: %u\n", sb->s_blocks_count);
    printf("Blocos reservados: %u\n", sb->s_r_blocks_count);
    printf("Blocos livres: %u\n", sb->s_free_blocks_count);
    printf("Inodes livres: %u\n", sb->s_free_inodes_count);
    printf("Primeiro bloco de dados: %u\n", sb->s_first_data_block);
    
    unsigned int block_size = 1024; // Simplificação do projeto: tamanho do bloco fixo em 1024 bytes
    printf("Tamanho do bloco: %u bytes (definido como 1024 pela simplificação)\n", block_size);
    printf("Blocos por grupo: %u\n", sb->s_blocks_per_group);
    printf("Inodes por grupo: %u\n", sb->s_inodes_per_group);
    printf("Último montagem (mount time): %u\n", sb->s_mtime);
    printf("Última escrita (write time): %u\n", sb->s_wtime);
    printf("Contagem de montagens: %u\n", sb->s_mnt_count);
    printf("Contagem máxima de montagens: %u\n", sb->s_max_mnt_count);
    printf("Estado do sistema de arquivos: %u\n", sb->s_state);
    printf("Tratamento de erro: %u\n", sb->s_errors);
    printf("Nível de revisão menor: %u\n", sb->s_minor_rev_level);
    printf("Última checagem (last check): %u\n", sb->s_lastcheck);
    printf("Intervalo de checagem: %u\n", sb->s_checkinterval);
    printf("SO criador: %u (0=Linux, 1=Hurd, 2=Masix, 3=FreeBSD, 4=Lites)\n", sb->s_creator_os);
    printf("Nível de revisão: %u\n", sb->s_rev_level);
    if (sb->s_rev_level >= 1) { // Campos válidos apenas se rev_level >= EXT2_DYNAMIC_REV
        printf("Primeiro inode não reservado: %u\n", sb->s_first_ino);
        printf("Tamanho da estrutura do inode: %u bytes\n", sb->s_inode_size);
    }
    printf("Nome do volume: %.16s\n", sb->s_volume_name);
    printf("Último local de montagem: %.64s\n", sb->s_last_mounted);
    
    // Calcula o número de grupos de blocos com base nas contagens de blocos e inodes.
    unsigned int num_block_groups_blocks = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    unsigned int num_block_groups_inodes = (sb->s_inodes_count + sb->s_inodes_per_group - 1) / sb->s_inodes_per_group;
    printf("Número de grupos de blocos (baseado em blocos): %u\n", num_block_groups_blocks);
    printf("Número de grupos de blocos (baseado em inodes): %u\n", num_block_groups_inodes);
    printf("-----------------------------------\n");
}

// Função auxiliar para procurar uma entrada em um diretório e retornar seu inode.
// Lê o bloco de dados do diretório e itera pelas entradas de diretório.
// Retorna o número do inode se encontrado, 0 caso contrário.
static uint32_t dir_lookup(int fd, const struct ext2_super_block *sb,
                           const struct ext2_group_desc *bgdt,
                           uint32_t dir_inode_num, const char *name_to_find, 
                           uint8_t *found_file_type) {
    struct ext2_inode dir_inode;
    if (read_inode(fd, sb, bgdt, dir_inode_num, &dir_inode) != 0) {
        return 0; 
    }

    if (!S_ISDIR(dir_inode.i_mode)) { // Verifica se é um diretório
        return 0; 
    }

    if (dir_inode.i_block[0] == 0) { // Diretório sem blocos alocados
        return 0;
    }

    char data_block_buffer[BLOCK_SIZE_FIXED];
    // Simplificação: diretórios usam apenas o primeiro bloco de dados (i_block[0])
    if (read_data_block(fd, dir_inode.i_block[0], data_block_buffer) != 0) {
        return 0;
    }

    unsigned int offset = 0;
    while (offset < dir_inode.i_size) { // Itera pelas entradas do diretório
        if (offset >= BLOCK_SIZE_FIXED) { // Impede leitura além do bloco
            break; 
        }

        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(data_block_buffer + offset);

        if (entry->rec_len == 0) { // Prevenção contra corrupção
            break;
        }
        
        // Verifica se a entrada está em uso (inode != 0) e se o nome corresponde.
        if (entry->inode != 0 && entry->name_len == strlen(name_to_find)) {
            if (strncmp(entry->name, name_to_find, entry->name_len) == 0) {
                if (found_file_type != NULL) {
                    *found_file_type = entry->file_type; // Retorna o tipo do arquivo encontrado
                }
                return entry->inode; // Entrada encontrada, retorna o número do inode
            }
        }
        offset += entry->rec_len; // Move para a próxima entrada
    }
    return 0; // Entrada não encontrada
}

// Função para resolver um caminho de arquivo/diretório para um número de inode.
// Percorre o caminho, componente por componente, usando dir_lookup.
// Retorna o número do inode se o caminho for resolvido, 0 caso contrário.
uint32_t path_to_inode_number(int fd, const struct ext2_super_block *sb,
                               const struct ext2_group_desc *bgdt,
                               uint32_t base_inode_num, const char *path_str,
                               uint8_t *resolved_final_type) {
    char mutable_path[1024]; // Buffer mutável para strtok
    strncpy(mutable_path, path_str, sizeof(mutable_path) - 1);
    mutable_path[sizeof(mutable_path) - 1] = '\0';

    uint32_t current_inode;

    if (resolved_final_type) *resolved_final_type = EXT2_FT_UNKNOWN; // Inicializa o tipo final

    if (strlen(mutable_path) == 0) { // Caminho vazio
        current_inode = base_inode_num;
        if (resolved_final_type) { // Obtém o tipo do inode base
            struct ext2_inode temp_inode_info;
            if (read_inode(fd, sb, bgdt, current_inode, &temp_inode_info) == 0) {
                if (S_ISDIR(temp_inode_info.i_mode)) *resolved_final_type = EXT2_FT_DIR;
                else if (S_ISREG(temp_inode_info.i_mode)) *resolved_final_type = EXT2_FT_REG_FILE;
            }
        }
        return current_inode;
    }

    char *p_path_for_strtok;
    if (mutable_path[0] == '/') { // Caminho absoluto, começa da raiz
        current_inode = EXT2_ROOT_INO;
        p_path_for_strtok = mutable_path + 1; // Pula a barra inicial
        if (*p_path_for_strtok == '\0') { // Se o caminho é apenas "/", retorna o inode raiz
            if (resolved_final_type) *resolved_final_type = EXT2_FT_DIR;
            return EXT2_ROOT_INO;
        }
    } else { // Caminho relativo, começa do diretório base
        current_inode = base_inode_num;
        p_path_for_strtok = mutable_path;
    }

    char *token = strtok(p_path_for_strtok, "/"); // Quebra o caminho em componentes
    uint8_t last_token_type = EXT2_FT_UNKNOWN;

    while (token != NULL) {
        if (strlen(token) == 0) { // Ignora tokens vazios (ex: //)
            token = strtok(NULL, "/");
            continue;
        }
        
        uint32_t next_inode_num;
        uint8_t current_token_file_type = EXT2_FT_UNKNOWN;

        if (strcmp(token, ".") == 0) { // Trata "." (diretório atual)
            next_inode_num = current_inode;
            struct ext2_inode temp_inode_info;
            if (read_inode(fd, sb, bgdt, current_inode, &temp_inode_info) != 0 || !S_ISDIR(temp_inode_info.i_mode)) {
                return 0;
            }
            current_token_file_type = EXT2_FT_DIR;
        } else if (strcmp(token, "..") == 0) { // Trata ".." (diretório pai)
            if (current_inode == EXT2_ROOT_INO) {
                next_inode_num = EXT2_ROOT_INO; // ".." na raiz é a própria raiz
                current_token_file_type = EXT2_FT_DIR;
            } else {
                next_inode_num = dir_lookup(fd, sb, bgdt, current_inode, "..", &current_token_file_type);
                if (next_inode_num == 0) return 0;
            }
        } else { // Procura o componente no diretório atual
            next_inode_num = dir_lookup(fd, sb, bgdt, current_inode, token, &current_token_file_type);
            if (next_inode_num == 0) {
                return 0; // Componente não encontrado
            }
        }
        
        current_inode = next_inode_num;
        last_token_type = current_token_file_type;

        char *peek_next_token = strtok(NULL, "/");
        token = peek_next_token;

        if (token != NULL) { // Se houver mais componentes, o componente atual deve ser um diretório
            if (last_token_type != EXT2_FT_DIR) {
                 struct ext2_inode temp_inode_check_dir;
                 if(read_inode(fd,sb,bgdt,current_inode, &temp_inode_check_dir)!=0 || !S_ISDIR(temp_inode_check_dir.i_mode)){
                    return 0; // Componente intermediário não é diretório
                 }
            }
        }
    }

    if (resolved_final_type) { // Define o tipo do último componente resolvido
        *resolved_final_type = last_token_type;
        if (*resolved_final_type == EXT2_FT_UNKNOWN && current_inode != 0) { // Se o tipo não foi determinado pela entrada de diretório
            struct ext2_inode final_inode_info;
            if (read_inode(fd, sb, bgdt, current_inode, &final_inode_info) == 0) {
                if (S_ISDIR(final_inode_info.i_mode)) *resolved_final_type = EXT2_FT_DIR;
                else if (S_ISREG(final_inode_info.i_mode)) *resolved_final_type = EXT2_FT_REG_FILE;
                else if (S_ISLNK(final_inode_info.i_mode)) *resolved_final_type = EXT2_FT_SYMLINK;
            }
        }
    }
    return current_inode; // Retorna o inode do caminho resolvido
}

// Implementa o comando 'ls', que lista o conteúdo de um diretório ou informações de um arquivo.
void comando_ls(int fd, const struct ext2_super_block *sb, 
                const struct ext2_group_desc *bgdt, 
                uint32_t diretorio_inode_num, const char* path_argumento) {
    
    uint32_t inode_a_listar = diretorio_inode_num; // Inode padrão: diretório atual
    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;

    if (path_argumento != NULL && strlen(path_argumento) > 0) {
        // Se um caminho for fornecido, resolve-o para obter o inode.
        inode_a_listar = path_to_inode_number(fd, sb, bgdt, diretorio_inode_num, path_argumento, &tipo_resolvido);
        if (inode_a_listar == 0) {
            printf("ls: não foi possível acessar '%s': Arquivo ou diretório não encontrado\n", path_argumento);
            return;
        }
        if (tipo_resolvido != EXT2_FT_DIR) { // Se não é um diretório, assume-se que é um arquivo regular.
            struct ext2_inode temp_check;
            if(read_inode(fd,sb,bgdt,inode_a_listar,&temp_check)==0 && S_ISREG(temp_check.i_mode)){
                printf("%s\n", path_argumento); // Lista o próprio arquivo
                return;
            } else if (tipo_resolvido != EXT2_FT_DIR) {
                 printf("ls: não é possível listar '%s': Não é um diretório\n", path_argumento);
                 return;
            }
        }
    }

    struct ext2_inode dir_inode_obj;
    if (read_inode(fd, sb, bgdt, inode_a_listar, &dir_inode_obj) != 0) {
        printf("ls: erro ao ler inode %u\n", inode_a_listar);
        return;
    }

    if (!S_ISDIR(dir_inode_obj.i_mode)) { // Confere se o inode é de um diretório
        printf("ls: inode %u não é um diretório.\n", inode_a_listar);
        return;
    }

    if (dir_inode_obj.i_block[0] == 0) { // Diretório sem blocos alocados (vazio)
        return; 
    }

    char data_block_buffer[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, dir_inode_obj.i_block[0], data_block_buffer) != 0) {
        printf("ls: erro ao ler bloco de dados do diretório (inode %u)\n", inode_a_listar);
        return;
    }

    printf("Conteúdo do diretório (inode %u):\n", inode_a_listar);
    unsigned int offset = 0;
    while (offset < dir_inode_obj.i_size) { // Itera pelas entradas do diretório
        if (offset >= BLOCK_SIZE_FIXED) break; 

        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(data_block_buffer + offset);
        if (entry->rec_len == 0) break; // Prevenção contra corrupção

        if (entry->inode != 0) { // Se a entrada estiver em uso
            char name_buffer[EXT2_NAME_LEN + 1];
            strncpy(name_buffer, entry->name, entry->name_len);
            name_buffer[entry->name_len] = '\0';
            
            // Adiciona uma barra se for um diretório, para facilitar a visualização
            if (entry->file_type == EXT2_FT_DIR) {
                printf("%s/\n", name_buffer);
            } else {
                printf("%s\n", name_buffer);
            }
        }
        offset += entry->rec_len; // Move para a próxima entrada
    }
}

// Função para ler todo o conteúdo de um arquivo, dado seu inode.
// Lida com blocos diretos e indiretos (simples, duplo, triplo - embora os últimos possam ser limitados).
// Retorna um buffer alocado dinamicamente com o conteúdo do arquivo.
// O chamador DEVE liberar este buffer usando free().
// file_size_out: (saída) armazena o tamanho do arquivo lido.
// Retorna NULL em caso de erro ou se não for um arquivo regular.
char* read_file_data(int fd, const struct ext2_super_block *sb,
                     const struct ext2_group_desc *bgdt,
                     const struct ext2_inode *file_inode, uint32_t *file_size_out) {

    if (!S_ISREG(file_inode->i_mode)) { // Verifica se é um arquivo regular
        fprintf(stderr, "read_file_data: Inode não é um arquivo regular.\n");
        return NULL;
    }

    *file_size_out = file_inode->i_size; // Obtém o tamanho do arquivo
    if (*file_size_out == 0) { // Arquivo vazio
        char *empty_buffer = (char*)malloc(1);
        if (empty_buffer) empty_buffer[0] = '\0';
        return empty_buffer; 
    }

    char *file_content_buffer = (char *)malloc(*file_size_out + 1); // Aloca buffer para o conteúdo
    if (!file_content_buffer) {
        perror("read_file_data: Erro ao alocar memória para conteúdo do arquivo");
        return NULL;
    }

    char block_read_buffer[BLOCK_SIZE_FIXED];
    uint32_t bytes_read = 0;
    unsigned int i;

    // 1. Lendo Blocos Diretos (i_block[0] a i_block[11])
    for (i = 0; i < 12 && bytes_read < *file_size_out; ++i) {
        if (file_inode->i_block[i] == 0) { // Bloco não alocado (sparse file)
            uint32_t to_copy = (bytes_read + BLOCK_SIZE_FIXED > *file_size_out) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
            memset(file_content_buffer + bytes_read, 0, to_copy); // Preenche com zeros
            bytes_read += to_copy;
            continue;
        }
        if (read_data_block(fd, file_inode->i_block[i], block_read_buffer) != 0) { // Lê o bloco de dados
            fprintf(stderr, "read_file_data: Erro ao ler bloco direto %u (bloco real %u)\n", i, file_inode->i_block[i]);
            free(file_content_buffer);
            return NULL;
        }
        uint32_t to_copy = (*file_size_out - bytes_read < BLOCK_SIZE_FIXED) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
        memcpy(file_content_buffer + bytes_read, block_read_buffer, to_copy); // Copia para o buffer principal
        bytes_read += to_copy;
    }

    // 2. Lendo Bloco de Indireção Simples (i_block[12])
    if (bytes_read < *file_size_out && file_inode->i_block[12] != 0) {
        char indirect_block_pointers_buffer[BLOCK_SIZE_FIXED];
        if (read_data_block(fd, file_inode->i_block[12], indirect_block_pointers_buffer) != 0) { // Lê o bloco de ponteiros
            fprintf(stderr, "read_file_data: Erro ao ler bloco de indireção simples (bloco %u)\n", file_inode->i_block[12]);
            free(file_content_buffer);
            return NULL;
        }
        uint32_t *indirect_pointers = (uint32_t *)indirect_block_pointers_buffer;
        unsigned int num_pointers_in_block = BLOCK_SIZE_FIXED / sizeof(uint32_t);

        for (i = 0; i < num_pointers_in_block && bytes_read < *file_size_out; ++i) {
            if (indirect_pointers[i] == 0) { // Bloco não alocado
                uint32_t to_copy = (bytes_read + BLOCK_SIZE_FIXED > *file_size_out) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
                memset(file_content_buffer + bytes_read, 0, to_copy);
                bytes_read += to_copy;
                continue;
            }
            if (read_data_block(fd, indirect_pointers[i], block_read_buffer) != 0) {
                fprintf(stderr, "read_file_data: Erro ao ler bloco de dados %u (apontado por indireção simples)\n", indirect_pointers[i]);
                free(file_content_buffer);
                return NULL;
            }
            uint32_t to_copy = (*file_size_out - bytes_read < BLOCK_SIZE_FIXED) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
            memcpy(file_content_buffer + bytes_read, block_read_buffer, to_copy);
            bytes_read += to_copy;
        }
    }

    // 3. Lendo Bloco de Dupla Indireção (i_block[13])
    if (bytes_read < *file_size_out && file_inode->i_block[13] != 0) {
        char double_indirect_block_pointers_buffer[BLOCK_SIZE_FIXED];
        if (read_data_block(fd, file_inode->i_block[13], double_indirect_block_pointers_buffer) != 0) { // Lê o bloco de ponteiros de segundo nível
            fprintf(stderr, "read_file_data: Erro ao ler bloco de dupla indireção (bloco %u)\n", file_inode->i_block[13]);
            free(file_content_buffer);
            return NULL;
        }
        uint32_t *double_indirect_pointers = (uint32_t *)double_indirect_block_pointers_buffer;
        unsigned int num_pointers_in_block = BLOCK_SIZE_FIXED / sizeof(uint32_t);

        for (i = 0; i < num_pointers_in_block && bytes_read < *file_size_out; ++i) {
            if (double_indirect_pointers[i] == 0) continue; 

            char indirect_block_pointers_buffer[BLOCK_SIZE_FIXED]; 
            if (read_data_block(fd, double_indirect_pointers[i], indirect_block_pointers_buffer) != 0) { // Lê o bloco de ponteiros de primeiro nível
                fprintf(stderr, "read_file_data: Erro ao ler bloco de indireção simples (nível 2, bloco %u) da dupla indireção\n", double_indirect_pointers[i]);
                free(file_content_buffer);
                return NULL;
            }
            uint32_t *indirect_pointers = (uint32_t *)indirect_block_pointers_buffer;

            unsigned int j;
            for (j = 0; j < num_pointers_in_block && bytes_read < *file_size_out; ++j) {
                 if (indirect_pointers[j] == 0) { // Bloco não alocado
                    uint32_t to_copy = (bytes_read + BLOCK_SIZE_FIXED > *file_size_out) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
                    memset(file_content_buffer + bytes_read, 0, to_copy);
                    bytes_read += to_copy;
                    continue;
                }
                if (read_data_block(fd, indirect_pointers[j], block_read_buffer) != 0) { // Lê o bloco de dados final
                    fprintf(stderr, "read_file_data: Erro ao ler bloco de dados %u (apontado por dupla indireção)\n", indirect_pointers[j]);
                    free(file_content_buffer);
                    return NULL;
                }
                uint32_t to_copy = (*file_size_out - bytes_read < BLOCK_SIZE_FIXED) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
                memcpy(file_content_buffer + bytes_read, block_read_buffer, to_copy);
                bytes_read += to_copy;
            }
        }
    }
    // Bloco de Tripla Indireção (i_block[14]) - Não implementado aqui.

    file_content_buffer[*file_size_out] = '\0'; // Adiciona terminador null para facilitar tratamento como string
    return file_content_buffer;
}

// Implementa o comando 'cat', que exibe o conteúdo de um arquivo.
void comando_cat(int fd, const struct ext2_super_block *sb, 
                 const struct ext2_group_desc *bgdt, 
                 uint32_t diretorio_atual_inode_num, const char* path_arquivo) {

    if (path_arquivo == NULL || strlen(path_arquivo) == 0) {
        printf("cat: Caminho do arquivo não especificado.\n");
        return;
    }

    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;
    // Resolve o caminho para obter o inode do arquivo
    uint32_t arquivo_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, path_arquivo, &tipo_resolvido);

    if (arquivo_inode_num == 0) {
        printf("cat: '%s': Arquivo ou diretório não encontrado\n", path_arquivo);
        return;
    }

    struct ext2_inode arquivo_inode_obj;
    if (read_inode(fd, sb, bgdt, arquivo_inode_num, &arquivo_inode_obj) != 0) { // Lê o inode do arquivo
        printf("cat: Erro ao ler inode %u para o arquivo '%s'\n", arquivo_inode_num, path_arquivo);
        return;
    }

    if (!S_ISREG(arquivo_inode_obj.i_mode)) { // Verifica se é um arquivo regular
        if (S_ISDIR(arquivo_inode_obj.i_mode)) {
             printf("cat: '%s': É um diretório\n", path_arquivo);
        } else {
             printf("cat: '%s': Não é um arquivo regular\n", path_arquivo);
        }
        return;
    }

    uint32_t tamanho_do_arquivo;
    char *conteudo_arquivo = read_file_data(fd, sb, bgdt, &arquivo_inode_obj, &tamanho_do_arquivo); // Lê o conteúdo do arquivo

    if (conteudo_arquivo == NULL) {
        printf("cat: Falha ao ler o conteúdo de '%s'\n", path_arquivo);
        return;
    }

    if (tamanho_do_arquivo > 0) {
        fwrite(conteudo_arquivo, 1, tamanho_do_arquivo, stdout); // Imprime o conteúdo
    }

    free(conteudo_arquivo); // Libera a memória alocada
}

// Implementa o comando 'attr', que exibe os atributos (metadados) de um arquivo ou diretório.
void comando_attr(int fd, const struct ext2_super_block *sb, 
                  const struct ext2_group_desc *bgdt, 
                  uint32_t diretorio_atual_inode_num, const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) {
        printf("attr: Caminho do arquivo ou diretório não especificado.\n");
        return;
    }

    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;
    // Resolve o caminho para obter o inode do alvo
    uint32_t alvo_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, path_alvo, &tipo_resolvido);

    if (alvo_inode_num == 0) {
        printf("attr: '%s': Arquivo ou diretório não encontrado\n", path_alvo);
        return;
    }

    struct ext2_inode alvo_inode_obj;
    if (read_inode(fd, sb, bgdt, alvo_inode_num, &alvo_inode_obj) != 0) { // Lê o inode do alvo
        printf("attr: Erro ao ler inode %u para '%s'\n", alvo_inode_num, path_alvo);
        return;
    }

    printf("Atributos para '%s' (Inode: %u):\n", path_alvo, alvo_inode_num);

    // Exibe o tipo de arquivo
    char tipo_str[50] = "Desconhecido";
    if (S_ISREG(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Arquivo Regular");
    else if (S_ISDIR(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Diretório");
    else if (S_ISLNK(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Link Simbólico");
    else if (S_ISCHR(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Dispositivo de Caractere");
    else if (S_ISBLK(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Dispositivo de Bloco");
    else if (S_ISFIFO(alvo_inode_obj.i_mode)) strcpy(tipo_str, "FIFO/Pipe");
    else if (S_ISSOCK(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Socket");
    printf("  Tipo:          %s (0x%X)\n", tipo_str, alvo_inode_obj.i_mode & 0xF000);

    // Exibe as permissões
    printf("  Modo (perms):  %o (octal)\n", alvo_inode_obj.i_mode & 0xFFF);
    char perms_str[10];
    perms_str[0] = (alvo_inode_obj.i_mode & S_IRUSR) ? 'r' : '-';
    perms_str[1] = (alvo_inode_obj.i_mode & S_IWUSR) ? 'w' : '-';
    perms_str[2] = (alvo_inode_obj.i_mode & S_IXUSR) ? 'x' : '-';
    perms_str[3] = (alvo_inode_obj.i_mode & S_IRGRP) ? 'r' : '-';
    perms_str[4] = (alvo_inode_obj.i_mode & S_IWGRP) ? 'w' : '-';
    perms_str[5] = (alvo_inode_obj.i_mode & S_IXGRP) ? 'x' : '-';
    perms_str[6] = (alvo_inode_obj.i_mode & S_IROTH) ? 'r' : '-';
    perms_str[7] = (alvo_inode_obj.i_mode & S_IWOTH) ? 'w' : '-';
    perms_str[8] = (alvo_inode_obj.i_mode & S_IXOTH) ? 'x' : '-';
    perms_str[9] = '\0';
    printf("  Permissões:    %s (u:%c%c%c g:%c%c%c o:%c%c%c)\n", perms_str,
           (alvo_inode_obj.i_mode & S_IRUSR) ? 'r' : '-', (alvo_inode_obj.i_mode & S_IWUSR) ? 'w' : '-', (alvo_inode_obj.i_mode & S_IXUSR) ? 'x' : '-', 
           (alvo_inode_obj.i_mode & S_IRGRP) ? 'r' : '-', (alvo_inode_obj.i_mode & S_IWGRP) ? 'w' : '-', (alvo_inode_obj.i_mode & S_IXGRP) ? 'x' : '-', 
           (alvo_inode_obj.i_mode & S_IROTH) ? 'r' : '-', (alvo_inode_obj.i_mode & S_IWOTH) ? 'w' : '-', (alvo_inode_obj.i_mode & S_IXOTH) ? 'x' : '-'  
          );
    if (alvo_inode_obj.i_mode & S_ISUID) printf("                 (setuid bit set)\n");
    if (alvo_inode_obj.i_mode & S_ISGID) printf("                 (setgid bit set)\n");

    printf("  UID:           %u\n", alvo_inode_obj.i_uid);
    printf("  GID:           %u\n", alvo_inode_obj.i_gid);
    printf("  Tamanho:       %u bytes\n", alvo_inode_obj.i_size);
    printf("  Links:         %u\n", alvo_inode_obj.i_links_count);
    printf("  Blocos (FS):   %u (calculado: %u)\n", alvo_inode_obj.i_blocks / (BLOCK_SIZE_FIXED/512), alvo_inode_obj.i_blocks ); 
    
    // Exibe os timestamps formatados
    time_t atime_val = alvo_inode_obj.i_atime;
    time_t ctime_val = alvo_inode_obj.i_ctime;
    time_t mtime_val = alvo_inode_obj.i_mtime;
    char buffer_time[30];
    
    strncpy(buffer_time, ctime(&atime_val), sizeof(buffer_time)-1); buffer_time[sizeof(buffer_time)-1] = 0; 
    buffer_time[strcspn(buffer_time, "\n")] = 0;
    printf("  Acesso:        %s\n", buffer_time);

    strncpy(buffer_time, ctime(&ctime_val), sizeof(buffer_time)-1); buffer_time[sizeof(buffer_time)-1] = 0;
    buffer_time[strcspn(buffer_time, "\n")] = 0;
    printf("  Criação Inode: %s\n", buffer_time);

    strncpy(buffer_time, ctime(&mtime_val), sizeof(buffer_time)-1); buffer_time[sizeof(buffer_time)-1] = 0;
    buffer_time[strcspn(buffer_time, "\n")] = 0;
    printf("  Modificação:   %s\n", buffer_time);
    
    if (alvo_inode_obj.i_dtime != 0) {
        time_t dtime_val = alvo_inode_obj.i_dtime;
        strncpy(buffer_time, ctime(&dtime_val), sizeof(buffer_time)-1); buffer_time[sizeof(buffer_time)-1] = 0;
        buffer_time[strcspn(buffer_time, "\n")] = 0;
        printf("  Deleção:       %s\n", buffer_time);
    }

    printf("  Flags Inode:   0x%X\n", alvo_inode_obj.i_flags);

    printf("  Ponteiros de Bloco (i_block):\n");
    for (int k=0; k<15; ++k) { // Exibe os 15 ponteiros de bloco
        printf("    i_block[%2d]: %u (0x%X)\n", k, alvo_inode_obj.i_block[k], alvo_inode_obj.i_block[k]);
    }
}

// Implementa o comando 'pwd' (print working directory), que exibe o caminho completo do diretório atual.
void comando_pwd(const char* diretorio_atual_str) {
    printf("%s\n", diretorio_atual_str);
}

// Função auxiliar para normalizar uma string de caminho (ex: remove barras duplas, trata . e ..).
// Retorna uma string alocada dinamicamente que deve ser liberada pelo chamador.
char* normalizar_path_string(const char* base, const char* append) {
    char temp_path[2048]; 
    
    if (append && append[0] == '/') { // Se 'append' é um caminho absoluto, usa-o diretamente
        strncpy(temp_path, append, sizeof(temp_path) -1 );
    } else { // Caso contrário, constrói o caminho relativo
        strncpy(temp_path, base, sizeof(temp_path) - 1);
        if (append && strlen(append) > 0) {
            if (temp_path[strlen(temp_path) - 1] != '/') {
                strncat(temp_path, "/", sizeof(temp_path) - strlen(temp_path) - 1);
            }
            strncat(temp_path, append, sizeof(temp_path) - strlen(temp_path) - 1);
        }
    }
    temp_path[sizeof(temp_path) -1] = '\0';

    size_t len = strlen(temp_path);
    while (len > 1 && temp_path[len-1] == '/') { // Remove barras extras no final (exceto para a raiz)
        temp_path[len-1] = '\0';
        len--;
    }
    if (strlen(temp_path) == 0 && ( (append && append[0] == '/') || (base && base[0] == '/' && (!append || strlen(append)==0))  ) ) {
        strcpy(temp_path, "/"); // Garante que a raiz seja representada como "/"
    }

    return strdup(temp_path); // Retorna uma cópia alocada dinamicamente
}

// Implementa o comando 'cd' (change directory), que muda o diretório de trabalho atual.
void comando_cd(int fd, const struct ext2_super_block *sb, 
                const struct ext2_group_desc *bgdt, 
                uint32_t *diretorio_atual_inode_num_ptr, 
                char* diretorio_atual_str, 
                size_t diretorio_atual_str_max_len,
                const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) { // 'cd' sem argumentos vai para a raiz
        *diretorio_atual_inode_num_ptr = EXT2_ROOT_INO;
        strncpy(diretorio_atual_str, "/", diretorio_atual_str_max_len -1);
        diretorio_atual_str[diretorio_atual_str_max_len -1] = '\0';
        return;
    }

    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;
    // Resolve o caminho alvo para obter o inode do novo diretório
    uint32_t novo_inode_num = path_to_inode_number(fd, sb, bgdt, *diretorio_atual_inode_num_ptr, path_alvo, &tipo_resolvido);

    if (novo_inode_num == 0) {
        printf("cd: '%s': Arquivo ou diretório não encontrado\n", path_alvo);
        return;
    }

    struct ext2_inode novo_inode_obj;
    if (read_inode(fd, sb, bgdt, novo_inode_num, &novo_inode_obj) != 0) { // Lê o inode do alvo
        printf("cd: Erro ao ler inode %u para '%s'\n", novo_inode_num, path_alvo);
        return;
    }

    if (!S_ISDIR(novo_inode_obj.i_mode)) { // Verifica se o alvo é um diretório
        printf("cd: '%s': Não é um diretório\n", path_alvo);
        return;
    }

    *diretorio_atual_inode_num_ptr = novo_inode_num; // Atualiza o inode do diretório atual

    // Atualiza a string do diretório atual para o prompt
    char* path_normalizado;
    if (path_alvo[0] == '/') { // Caminho absoluto
        path_normalizado = normalizar_path_string(path_alvo, NULL);
    } else { // Caminho relativo
        path_normalizado = normalizar_path_string(diretorio_atual_str, path_alvo);
    }
    
    if (*diretorio_atual_inode_num_ptr == EXT2_ROOT_INO) { // Se o novo diretório é a raiz, a string é "/"
        strncpy(diretorio_atual_str, "/", diretorio_atual_str_max_len -1);
    } else {
        strncpy(diretorio_atual_str, path_normalizado, diretorio_atual_str_max_len - 1);
    }
    diretorio_atual_str[diretorio_atual_str_max_len - 1] = '\0';
    free(path_normalizado); // Libera a memória alocada pela normalizar_path_string
}

// Função auxiliar: Retorna 1 se o bit estiver setado (1), 0 se estiver limpo (0) no bitmap.
int is_bit_set(const unsigned char *bitmap_buffer, int bit_num) {
    return (bitmap_buffer[bit_num / 8] & (1 << (bit_num % 8))) != 0;
}

// Função auxiliar: Seta o bit (para 1) no bitmap.
void set_bit(unsigned char *bitmap_buffer, int bit_num) {
    bitmap_buffer[bit_num / 8] |= (1 << (bit_num % 8));
}

// Função auxiliar: Limpa o bit (para 0) no bitmap.
void clear_bit(unsigned char *bitmap_buffer, int bit_num) {
    bitmap_buffer[bit_num / 8] &= ~(1 << (bit_num % 8));
}

// Função para alocar um inode livre.
// Percorre os grupos de blocos para encontrar um inode livre no bitmap de inodes.
// Atualiza o superbloco, descritor de grupo e o bitmap de inodes no disco.
// Retorna o número do inode alocado em sucesso, 0 em falha (sem inodes livres).
uint32_t allocate_inode(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt) {
    unsigned int num_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    unsigned char inode_bitmap_buffer[BLOCK_SIZE_FIXED];

    for (unsigned int group_idx = 0; group_idx < num_block_groups; ++group_idx) { // Itera pelos grupos de blocos
        if (bgdt[group_idx].bg_free_inodes_count > 0) { // Se o grupo tem inodes livres
            // Lê o bitmap de inodes do grupo
            if (read_data_block(fd, bgdt[group_idx].bg_inode_bitmap, (char*)inode_bitmap_buffer) != 0) {
                fprintf(stderr, "allocate_inode: Erro ao ler bitmap de inodes do grupo %u (bloco %u)\n", 
                        group_idx, bgdt[group_idx].bg_inode_bitmap);
                continue; 
            }

            // Encontra o primeiro bit 0 (inode livre) no bitmap
            for (unsigned int bit_in_group = 0; bit_in_group < sb->s_inodes_per_group; ++bit_in_group) {
                if (!is_bit_set(inode_bitmap_buffer, bit_in_group)) { // Se o bit está limpo (inode livre)
                    set_bit(inode_bitmap_buffer, bit_in_group); // Seta o bit (marca como usado)

                    // Escreve o bitmap de inodes atualizado de volta para o disco
                    if (write_data_block(fd, bgdt[group_idx].bg_inode_bitmap, (char*)inode_bitmap_buffer) != 0) {
                        fprintf(stderr, "allocate_inode: Erro ao escrever bitmap de inodes atualizado para grupo %u\n", group_idx);
                        return 0; 
                    }

                    // Atualiza as contagens de inodes livres no superbloco e no descritor de grupo
                    sb->s_free_inodes_count--;
                    bgdt[group_idx].bg_free_inodes_count--;

                    // Escreve o superbloco e o descritor de grupo atualizados
                    if (write_superblock(fd, sb) != 0) {
                        fprintf(stderr, "allocate_inode: Erro ao escrever superbloco após alocação\n");
                        return 0;
                    }
                    if (write_group_descriptor(fd, sb, group_idx, &bgdt[group_idx]) != 0) {
                        fprintf(stderr, "allocate_inode: Erro ao escrever descritor de grupo %u após alocação\n", group_idx);
                        return 0;
                    }

                    // Calcula o número global do inode (inodes são 1-indexados)
                    uint32_t allocated_inode_num = (group_idx * sb->s_inodes_per_group) + bit_in_group + 1;
                    return allocated_inode_num;
                }
            }
            fprintf(stderr, "Alerta allocate_inode: Grupo %u indicou inodes livres (%u), mas bitmap estava cheio ou erro.\n", 
                    group_idx, bgdt[group_idx].bg_free_inodes_count);
            bgdt[group_idx].bg_free_inodes_count = 0; 
        }
    }

    fprintf(stderr, "allocate_inode: Não há inodes livres em nenhum grupo de blocos.\n");
    return 0; // Nenhum inode livre encontrado
}

// Implementa o comando 'touch', que cria um novo arquivo vazio ou atualiza o timestamp de um existente.
void comando_touch(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
                   uint32_t diretorio_atual_inode_num, char* diretorio_atual_str, 
                   const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) {
        printf("touch: Nome do arquivo não especificado.\n");
        return;
    }

    char nome_arquivo[EXT2_NAME_LEN + 1];
    char caminho_pai_str[1024]; 
    uint32_t inode_pai_num;

    // 1. Analisa o caminho para separar o nome do arquivo do caminho do diretório pai.
    const char *ultimo_slash = strrchr(path_alvo, '/');
    if (ultimo_slash != NULL) { 
        size_t len_caminho_pai = ultimo_slash - path_alvo;
        if (len_caminho_pai == 0) { strcpy(caminho_pai_str, "/"); } // Caso de caminho absoluto, ex: "/arquivo.txt"
        else { strncpy(caminho_pai_str, path_alvo, len_caminho_pai); caminho_pai_str[len_caminho_pai] = '\0'; }
        strncpy(nome_arquivo, ultimo_slash + 1, EXT2_NAME_LEN); // Nome do arquivo é o que vem depois da última barra
        nome_arquivo[EXT2_NAME_LEN] = '\0';
    } else { // Se não há barra, o pai é o diretório atual
        strcpy(caminho_pai_str, "."); 
        strncpy(nome_arquivo, path_alvo, EXT2_NAME_LEN);
        nome_arquivo[EXT2_NAME_LEN] = '\0';
    }
    
    if (strlen(nome_arquivo) == 0) {
        printf("touch: Nome do arquivo inválido (vazio).\n");
        return;
    }
    if (strchr(nome_arquivo, '/') != NULL) {
        printf("touch: Nome do arquivo não pode conter '/'.\n");
        return;
    }

    // 2. Obtém o inode do diretório pai.
    uint8_t tipo_pai;
    inode_pai_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, caminho_pai_str, &tipo_pai);
    if (inode_pai_num == 0) {
        printf("touch: Diretório pai '%s' não encontrado.\n", caminho_pai_str);
        return;
    }
    struct ext2_inode inode_pai_obj;
    if (read_inode(fd, sb, bgdt, inode_pai_num, &inode_pai_obj) != 0 || !S_ISDIR(inode_pai_obj.i_mode)) {
        printf("touch: Caminho pai '%s' não é um diretório.\n", caminho_pai_str);
        return;
    }

    // 3. Verifica se o nome do arquivo já existe no diretório pai.
    if (dir_lookup(fd, sb, bgdt, inode_pai_num, nome_arquivo, NULL) != 0) {
        printf("touch: '%s' já existe.\n", path_alvo);
        return;
    }

    // 4. Aloca um novo inode para o arquivo.
    uint32_t novo_inode_arquivo_num = allocate_inode(fd, sb, bgdt);
    if (novo_inode_arquivo_num == 0) {
        printf("touch: Falha ao alocar novo inode. Disco cheio?\n");
        return;
    }

    // 5. Inicializa e escreve o novo inode do arquivo.
    struct ext2_inode novo_inode_arquivo_obj;
    memset(&novo_inode_arquivo_obj, 0, sizeof(struct ext2_inode)); // Zera a estrutura do inode
    novo_inode_arquivo_obj.i_mode = S_IFREG | 0644; // Define como arquivo regular e permissões rw-r--r--
    novo_inode_arquivo_obj.i_uid = 0; // UID: root (simplificação)
    novo_inode_arquivo_obj.i_gid = 0; // GID: root (simplificação)
    novo_inode_arquivo_obj.i_size = 0; // Arquivo vazio
    novo_inode_arquivo_obj.i_links_count = 1; // Um hard link (do diretório pai)
    novo_inode_arquivo_obj.i_atime = novo_inode_arquivo_obj.i_mtime = novo_inode_arquivo_obj.i_ctime = time(NULL); // Define timestamps
    novo_inode_arquivo_obj.i_dtime = 0;
    novo_inode_arquivo_obj.i_blocks = 0; // Nenhum bloco de dados alocado

    if (write_inode_table_entry(fd, sb, bgdt, novo_inode_arquivo_num, &novo_inode_arquivo_obj) != 0) { // Escreve o novo inode
        printf("touch: Falha ao escrever o novo inode do arquivo no disco.\n");
        return;
    }

    // 6. Adiciona a nova entrada no diretório pai.
    char dir_data_block[BLOCK_SIZE_FIXED];
    if (inode_pai_obj.i_block[0] == 0) { 
        printf("touch: Erro crítico - diretório pai (inode %u) não tem bloco de dados alocado.\n", inode_pai_num);
        return;
    }

    if (read_data_block(fd, inode_pai_obj.i_block[0], dir_data_block) != 0) { // Lê o bloco de dados do diretório pai
        printf("touch: Falha ao ler bloco de dados do diretório pai.\n");
        return;
    }

    unsigned int offset = 0;
    struct ext2_dir_entry_2 *entry = NULL;
    int entrada_adicionada = 0;

    uint16_t nome_len_real_novo_arq = strlen(nome_arquivo);
    // Calcula o tamanho real necessário para a nova entrada de diretório, alinhado a 4 bytes.
    uint16_t rec_len_necessario_nova_entrada = (offsetof(struct ext2_dir_entry_2, name) + nome_len_real_novo_arq + 3) & ~3;

    while (offset < inode_pai_obj.i_size) { // Percorre as entradas existentes no diretório
        entry = (struct ext2_dir_entry_2 *)(dir_data_block + offset);
        if (entry->rec_len == 0) break; // Prevenção contra corrupção

        uint16_t rec_len_real_atual = (entry->inode == 0) ? 0 : 
                                     (offsetof(struct ext2_dir_entry_2, name) + entry->name_len + 3) & ~3;
        
        // Tenta reutilizar o espaço de uma entrada deletada (inode 0) se houver espaço suficiente.
        if (entry->inode == 0 && entry->rec_len >= rec_len_necessario_nova_entrada) {
            entry->inode = novo_inode_arquivo_num;
            entry->name_len = nome_len_real_novo_arq;
            entry->file_type = EXT2_FT_REG_FILE;
            strncpy(entry->name, nome_arquivo, nome_len_real_novo_arq);
            entrada_adicionada = 1;
            break;
        }

        // Se há espaço livre após a entrada atual e antes da próxima (ou fim do bloco),
        // pode encurtar a entrada atual e adicionar a nova.
        if (offset + entry->rec_len >= inode_pai_obj.i_size || 
            (entry->rec_len > rec_len_real_atual && (entry->rec_len - rec_len_real_atual) >= rec_len_necessario_nova_entrada )) {
            
            uint16_t espaco_disponivel_apos_real_atual = entry->rec_len - rec_len_real_atual;
            if (rec_len_real_atual == 0 && entry->inode == 0){ 
                 espaco_disponivel_apos_real_atual = entry->rec_len;
            }

            if (espaco_disponivel_apos_real_atual >= rec_len_necessario_nova_entrada) {
                if(entry->inode != 0) { 
                    entry->rec_len = rec_len_real_atual; // Encurta a entrada atual para seu tamanho real
                }
                struct ext2_dir_entry_2 *nova_entrada_dir = (struct ext2_dir_entry_2 *)(dir_data_block + offset + entry->rec_len);
                nova_entrada_dir->inode = novo_inode_arquivo_num;
                nova_entrada_dir->name_len = nome_len_real_novo_arq;
                nova_entrada_dir->file_type = EXT2_FT_REG_FILE;
                strncpy(nova_entrada_dir->name, nome_arquivo, nome_len_real_novo_arq);
                nova_entrada_dir->rec_len = espaco_disponivel_apos_real_atual; // A nova entrada ocupa o restante do espaço
                
                entrada_adicionada = 1;
                break;
            }
        }
        offset += entry->rec_len;
    }

    // Se a entrada não foi adicionada nos espaços existentes, tenta adicionar no final do bloco.
    if (!entrada_adicionada) {
        if (inode_pai_obj.i_size < BLOCK_SIZE_FIXED && 
            (BLOCK_SIZE_FIXED - inode_pai_obj.i_size) >= rec_len_necessario_nova_entrada) {
            
            if (entry && entry->inode != 0 && offset + entry->rec_len == inode_pai_obj.i_size) {
                 uint16_t rec_len_real_ultima_entrada = (offsetof(struct ext2_dir_entry_2, name) + entry->name_len + 3) & ~3;
                 entry->rec_len = rec_len_real_ultima_entrada;
                 offset = (char*)entry - dir_data_block + entry->rec_len;
            } else { 
                 offset = inode_pai_obj.i_size;
            }

            struct ext2_dir_entry_2 *nova_entrada_dir = (struct ext2_dir_entry_2 *)(dir_data_block + offset);
            nova_entrada_dir->inode = novo_inode_arquivo_num;
            nova_entrada_dir->name_len = nome_len_real_novo_arq;
            nova_entrada_dir->file_type = EXT2_FT_REG_FILE;
            strncpy(nova_entrada_dir->name, nome_arquivo, nome_len_real_novo_arq);
            nova_entrada_dir->rec_len = BLOCK_SIZE_FIXED - offset; // Ocupa o resto do bloco
            
            inode_pai_obj.i_size = offset + nova_entrada_dir->rec_len; // Atualiza o tamanho do diretório pai
            if (inode_pai_obj.i_size > BLOCK_SIZE_FIXED) inode_pai_obj.i_size = BLOCK_SIZE_FIXED; 

            entrada_adicionada = 1;
        } else {
            printf("touch: Falha ao adicionar entrada no diretório pai '%s'. Sem espaço no bloco de dados do diretório (ou lógica de adição falhou).\n", caminho_pai_str);
            return;
        }
    }

    // Escreve o bloco de dados do diretório pai modificado de volta ao disco.
    if (write_data_block(fd, inode_pai_obj.i_block[0], dir_data_block) != 0) {
        printf("touch: Falha ao escrever bloco de dados atualizado do diretório pai.\n");
        return;
    }

    // Atualiza o inode do diretório pai (timestamps) e escreve de volta.
    inode_pai_obj.i_mtime = inode_pai_obj.i_ctime = time(NULL);
    if (write_inode_table_entry(fd, sb, bgdt, inode_pai_num, &inode_pai_obj) != 0) {
        printf("touch: Falha ao atualizar inode do diretório pai.\n");
        return;
    }

    printf("touch: Arquivo '%s' criado com sucesso (inode %u).\n", path_alvo, novo_inode_arquivo_num);
}

// Função para alocar um bloco de dados livre.
// Percorre os grupos de blocos para encontrar um bloco livre no bitmap de blocos.
// Atualiza o superbloco, descritor de grupo e o bitmap de blocos no disco.
// Retorna o número do bloco alocado em sucesso, 0 em falha (sem blocos livres).
uint32_t allocate_data_block(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt) {
    unsigned int num_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    unsigned char block_bitmap_buffer[BLOCK_SIZE_FIXED];

    for (unsigned int group_idx = 0; group_idx < num_block_groups; ++group_idx) { // Itera pelos grupos de blocos
        if (bgdt[group_idx].bg_free_blocks_count > 0) { // Se o grupo tem blocos livres
            // Lê o bitmap de blocos do grupo
            if (read_data_block(fd, bgdt[group_idx].bg_block_bitmap, (char*)block_bitmap_buffer) != 0) {
                fprintf(stderr, "allocate_data_block: Erro ao ler bitmap de blocos do grupo %u (bloco %u)\n", 
                        group_idx, bgdt[group_idx].bg_block_bitmap);
                continue; 
            }

            // Encontra o primeiro bit 0 (bloco livre) no bitmap
            for (unsigned int bit_in_group = 0; bit_in_group < sb->s_blocks_per_group; ++bit_in_group) {
                if (!is_bit_set(block_bitmap_buffer, bit_in_group)) { // Se o bit está limpo (bloco livre)
                    set_bit(block_bitmap_buffer, bit_in_group); // Seta o bit (marca como usado)

                    // Escreve o bitmap de blocos atualizado de volta para o disco
                    if (write_data_block(fd, bgdt[group_idx].bg_block_bitmap, (char*)block_bitmap_buffer) != 0) {
                        fprintf(stderr, "allocate_data_block: Erro ao escrever bitmap de blocos atualizado para grupo %u\n", group_idx);
                        return 0; 
                    }

                    // Atualiza as contagens de blocos livres no superbloco e no descritor de grupo
                    sb->s_free_blocks_count--;
                    bgdt[group_idx].bg_free_blocks_count--;

                    // Escreve o superbloco e o descritor de grupo atualizados
                    if (write_superblock(fd, sb) != 0) {
                        fprintf(stderr, "allocate_data_block: Erro ao escrever superbloco\n");
                        return 0;
                    }
                    if (write_group_descriptor(fd, sb, group_idx, &bgdt[group_idx]) != 0) {
                        fprintf(stderr, "allocate_data_block: Erro ao escrever descritor de grupo %u\n", group_idx);
                        return 0;
                    }

                    // Calcula o número global do bloco
                    uint32_t allocated_block_num = (group_idx * sb->s_blocks_per_group) + sb->s_first_data_block + bit_in_group;
                    return allocated_block_num;
                }
            }
            fprintf(stderr, "Alerta allocate_data_block: Grupo %u indicou blocos livres (%u), mas bitmap estava cheio.\n", 
                    group_idx, bgdt[group_idx].bg_free_blocks_count);
            bgdt[group_idx].bg_free_blocks_count = 0; 
        }
    }

    fprintf(stderr, "allocate_data_block: Não há blocos de dados livres em nenhum grupo.\n");
    return 0; 
}

// Implementa o comando 'mkdir', que cria um novo diretório.
void comando_mkdir(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
                   uint32_t diretorio_atual_inode_num, char* diretorio_atual_str, 
                   const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) {
        printf("mkdir: Nome do diretório não especificado.\n");
        return;
    }

    char nome_novo_dir[EXT2_NAME_LEN + 1];
    char caminho_pai_str[1024];
    uint32_t inode_pai_num;

    // 1. Analisa o caminho alvo (similar ao 'touch').
    const char *ultimo_slash = strrchr(path_alvo, '/');
    if (ultimo_slash != NULL) {
        size_t len_caminho_pai = ultimo_slash - path_alvo;
        if (len_caminho_pai == 0) { strcpy(caminho_pai_str, "/"); }
        else { strncpy(caminho_pai_str, path_alvo, len_caminho_pai); caminho_pai_str[len_caminho_pai] = '\0'; }
        strncpy(nome_novo_dir, ultimo_slash + 1, EXT2_NAME_LEN); nome_novo_dir[EXT2_NAME_LEN] = '\0';
    } else {
        strcpy(caminho_pai_str, ".");
        strncpy(nome_novo_dir, path_alvo, EXT2_NAME_LEN); nome_novo_dir[EXT2_NAME_LEN] = '\0';
    }
    // Validações do nome do novo diretório
    if (strlen(nome_novo_dir) == 0 || strcmp(nome_novo_dir, ".") == 0 || strcmp(nome_novo_dir, "..") == 0) {
        printf("mkdir: Nome de diretório inválido: '%s'\n", nome_novo_dir);
        return;
    }
    if (strchr(nome_novo_dir, '/') != NULL) {
        printf("mkdir: Nome do diretório não pode conter '/'.\n");
        return;
    }

    // 2. Obtém o inode do diretório pai.
    uint8_t tipo_pai;
    inode_pai_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, caminho_pai_str, &tipo_pai);
    if (inode_pai_num == 0) {
        printf("mkdir: Diretório pai '%s' não encontrado.\n", caminho_pai_str);
        return;
    }
    struct ext2_inode inode_pai_obj;
    if (read_inode(fd, sb, bgdt, inode_pai_num, &inode_pai_obj) != 0 || !S_ISDIR(inode_pai_obj.i_mode)) {
        printf("mkdir: Caminho pai '%s' não é um diretório.\n", caminho_pai_str);
        return;
    }

    // 3. Verifica se o nome já existe no diretório pai.
    if (dir_lookup(fd, sb, bgdt, inode_pai_num, nome_novo_dir, NULL) != 0) {
        printf("mkdir: '%s' já existe.\n", path_alvo);
        return;
    }

    // 4. Aloca um novo inode para o diretório.
    uint32_t novo_dir_inode_num = allocate_inode(fd, sb, bgdt);
    if (novo_dir_inode_num == 0) {
        printf("mkdir: Falha ao alocar inode para novo diretório. Disco cheio?\n");
        return;
    }

    // 5. Aloca um bloco de dados para o novo diretório (para armazenar . e ..).
    uint32_t novo_dir_data_block_num = allocate_data_block(fd, sb, bgdt);
    if (novo_dir_data_block_num == 0) {
        printf("mkdir: Falha ao alocar bloco de dados para novo diretório. Disco cheio?\n");
        return;
    }

    // 6. Inicializa o inode do novo diretório.
    struct ext2_inode novo_dir_inode_obj;
    memset(&novo_dir_inode_obj, 0, sizeof(struct ext2_inode)); // Zera a estrutura
    novo_dir_inode_obj.i_mode = S_IFDIR | 0755; // Define como diretório e permissões rwxr-xr-x
    novo_dir_inode_obj.i_uid = 0; 
    novo_dir_inode_obj.i_gid = 0; 
    novo_dir_inode_obj.i_size = BLOCK_SIZE_FIXED; // Tamanho de um bloco para . e ..
    novo_dir_inode_obj.i_links_count = 2; // Para '.' e para a entrada no diretório pai
    novo_dir_inode_obj.i_atime = novo_dir_inode_obj.i_mtime = novo_dir_inode_obj.i_ctime = time(NULL);
    novo_dir_inode_obj.i_blocks = BLOCK_SIZE_FIXED / 512; // Número de blocos de 512 bytes
    novo_dir_inode_obj.i_block[0] = novo_dir_data_block_num; // Aponta para o primeiro bloco de dados

    // 7. Escreve o inode do novo diretório no disco.
    if (write_inode_table_entry(fd, sb, bgdt, novo_dir_inode_num, &novo_dir_inode_obj) != 0) {
        printf("mkdir: Falha ao escrever inode do novo diretório.\n");
        return;
    }

    // 8. Prepara e escreve o bloco de dados do novo diretório (com entradas '.' e '..').
    char novo_dir_data_block_buffer[BLOCK_SIZE_FIXED];
    memset(novo_dir_data_block_buffer, 0, BLOCK_SIZE_FIXED);
    
    // Configura a entrada "." (aponta para o próprio diretório)
    struct ext2_dir_entry_2 *dot_entry = (struct ext2_dir_entry_2 *)novo_dir_data_block_buffer;
    dot_entry->inode = novo_dir_inode_num;
    dot_entry->name_len = 1;
    dot_entry->file_type = EXT2_FT_DIR;
    strcpy(dot_entry->name, ".");
    dot_entry->rec_len = (offsetof(struct ext2_dir_entry_2, name) + dot_entry->name_len + 3) & ~3; // Calcula rec_len alinhado
    if (dot_entry->rec_len < 12) dot_entry->rec_len = 12; 

    // Configura a entrada ".." (aponta para o diretório pai)
    struct ext2_dir_entry_2 *dotdot_entry = (struct ext2_dir_entry_2 *)(novo_dir_data_block_buffer + dot_entry->rec_len);
    dotdot_entry->inode = inode_pai_num;
    dotdot_entry->name_len = 2;
    dotdot_entry->file_type = EXT2_FT_DIR;
    strcpy(dotdot_entry->name, "..");
    dotdot_entry->rec_len = BLOCK_SIZE_FIXED - dot_entry->rec_len; // Ocupa o restante do bloco

    if (write_data_block(fd, novo_dir_data_block_num, novo_dir_data_block_buffer) != 0) { // Escreve o bloco de dados
        printf("mkdir: Falha ao escrever bloco de dados do novo diretório.\n");
        return;
    }

    // 9. Adiciona a entrada para o novo diretório no diretório pai (lógica similar ao 'touch').
    char pai_dir_data_block[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, inode_pai_obj.i_block[0], pai_dir_data_block) != 0) {
         printf("mkdir: Falha ao ler bloco de dados do diretório pai para adicionar nova entrada.\n"); return;
    }
    unsigned int offset_pai = 0;
    struct ext2_dir_entry_2 *entry_pai = NULL;
    int entrada_adicionada_pai = 0;
    uint16_t nome_len_novo_dir = strlen(nome_novo_dir);
    uint16_t rec_len_necessario_novo_dir_no_pai = (offsetof(struct ext2_dir_entry_2, name) + nome_len_novo_dir + 3) & ~3;

    while (offset_pai < inode_pai_obj.i_size) {
        entry_pai = (struct ext2_dir_entry_2 *)(pai_dir_data_block + offset_pai);
        if (entry_pai->rec_len == 0) break;
        uint16_t rec_len_real_atual_pai = (entry_pai->inode == 0) ? 0 : 
                                          (offsetof(struct ext2_dir_entry_2, name) + entry_pai->name_len + 3) & ~3;
        if (entry_pai->inode == 0 && entry_pai->rec_len >= rec_len_necessario_novo_dir_no_pai) { 
            entry_pai->inode = novo_dir_inode_num; entry_pai->name_len = nome_len_novo_dir; 
            entry_pai->file_type = EXT2_FT_DIR; strncpy(entry_pai->name, nome_novo_dir, nome_len_novo_dir);
            entrada_adicionada_pai = 1; break;
        }
        if (offset_pai + entry_pai->rec_len >= inode_pai_obj.i_size || 
            (entry_pai->rec_len > rec_len_real_atual_pai && (entry_pai->rec_len - rec_len_real_atual_pai) >= rec_len_necessario_novo_dir_no_pai)) {
            uint16_t espaco_disp = entry_pai->rec_len - rec_len_real_atual_pai;
            if (rec_len_real_atual_pai == 0 && entry_pai->inode == 0) espaco_disp = entry_pai->rec_len;
            if (espaco_disp >= rec_len_necessario_novo_dir_no_pai) {
                if(entry_pai->inode != 0) entry_pai->rec_len = rec_len_real_atual_pai;
                struct ext2_dir_entry_2 *nova_e = (struct ext2_dir_entry_2 *)(pai_dir_data_block + offset_pai + entry_pai->rec_len);
                nova_e->inode = novo_dir_inode_num; nova_e->name_len = nome_len_novo_dir;
                nova_e->file_type = EXT2_FT_DIR; strncpy(nova_e->name, nome_novo_dir, nome_len_novo_dir);
                nova_e->rec_len = espaco_disp;
                entrada_adicionada_pai = 1; break;
            }
        }
        offset_pai += entry_pai->rec_len;
    }
    if (!entrada_adicionada_pai) { // Tenta adicionar no final do bloco se não houver espaço intermediário
        if (inode_pai_obj.i_size < BLOCK_SIZE_FIXED && (BLOCK_SIZE_FIXED - inode_pai_obj.i_size) >= rec_len_necessario_novo_dir_no_pai) {
            if (entry_pai && entry_pai->inode != 0 && offset_pai + entry_pai->rec_len == inode_pai_obj.i_size) {
                 uint16_t real_len_ult = (offsetof(struct ext2_dir_entry_2, name) + entry_pai->name_len + 3) & ~3;
                 entry_pai->rec_len = real_len_ult; offset_pai = (char*)entry_pai - pai_dir_data_block + entry_pai->rec_len;
            } else { offset_pai = inode_pai_obj.i_size; }
            struct ext2_dir_entry_2 *nova_e = (struct ext2_dir_entry_2 *)(pai_dir_data_block + offset_pai);
            nova_e->inode = novo_dir_inode_num; nova_e->name_len = nome_len_novo_dir;
            nova_e->file_type = EXT2_FT_DIR; strncpy(nova_e->name, nome_novo_dir, nome_len_novo_dir);
            nova_e->rec_len = BLOCK_SIZE_FIXED - offset_pai;
            inode_pai_obj.i_size = offset_pai + nova_e->rec_len;
            if (inode_pai_obj.i_size > BLOCK_SIZE_FIXED) inode_pai_obj.i_size = BLOCK_SIZE_FIXED;
            entrada_adicionada_pai = 1;
        } else {
            printf("mkdir: Falha ao adicionar entrada no diretório pai '%s'. Sem espaço.\n", caminho_pai_str);
            return;
        }
    }
    if (write_data_block(fd, inode_pai_obj.i_block[0], pai_dir_data_block) != 0) { // Escreve o bloco de dados do diretório pai
        printf("mkdir: Falha ao escrever bloco de dados atualizado do dir pai.\n"); return;
    }

    // 10. Atualiza o inode do diretório pai e escreve de volta.
    inode_pai_obj.i_links_count++; // Incrementa o link count do pai (por causa do '..' do novo diretório)
    inode_pai_obj.i_mtime = inode_pai_obj.i_ctime = time(NULL);
    if (write_inode_table_entry(fd, sb, bgdt, inode_pai_num, &inode_pai_obj) != 0) {
        printf("mkdir: Falha ao atualizar inode do diretório pai.\n"); return;
    }

    // 11. Atualiza o contador de diretórios usados no descritor de grupo do NOVO diretório.
    uint32_t grupo_idx_novo_dir = (novo_dir_inode_num - 1) / sb->s_inodes_per_group;
    bgdt[grupo_idx_novo_dir].bg_used_dirs_count++;
    if (write_group_descriptor(fd, sb, grupo_idx_novo_dir, &bgdt[grupo_idx_novo_dir]) != 0) {
        printf("mkdir: Falha ao atualizar contador de diretórios no descritor de grupo %u.\n", grupo_idx_novo_dir);
    }

    printf("mkdir: Diretório '%s' criado com sucesso (inode %u, data block %u).\n", 
           path_alvo, novo_dir_inode_num, novo_dir_data_block_num);
}

// Função para desalocar um inode.
// Limpa o bit correspondente no bitmap de inodes e atualiza as contagens de inodes livres.
void deallocate_inode(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt, uint32_t inode_num) {
    if (inode_num == 0 || inode_num == EXT2_ROOT_INO) { // Não permite desalocar inode 0 ou o inode raiz
        fprintf(stderr, "deallocate_inode: Tentativa de desalocar inode inválido ou raiz (%u).\n", inode_num);
        return;
    }
    // Calcula o grupo de blocos e o bit dentro do bitmap correspondente ao inode.
    unsigned int group_idx = (inode_num - 1) / sb->s_inodes_per_group;
    unsigned int bit_in_group = (inode_num - 1) % sb->s_inodes_per_group;
    unsigned char inode_bitmap_buffer[BLOCK_SIZE_FIXED];

    // Valida o índice do grupo.
    if (group_idx >= ((sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group)) {
        fprintf(stderr, "deallocate_inode: Índice de grupo inválido %u para inode %u.\n", group_idx, inode_num);
        return;
    }

    // Lê o bitmap de inodes do grupo.
    if (read_data_block(fd, bgdt[group_idx].bg_inode_bitmap, (char*)inode_bitmap_buffer) != 0) {
        fprintf(stderr, "deallocate_inode: Erro ao ler bitmap de inodes do grupo %u.\n", group_idx);
        return; 
    }

    if (!is_bit_set(inode_bitmap_buffer, bit_in_group)) { // Verifica se o bit já está limpo
        fprintf(stderr, "deallocate_inode: Inode %u (bit %u no grupo %u) já está livre.\n", inode_num, bit_in_group, group_idx);
    } else {
        clear_bit(inode_bitmap_buffer, bit_in_group); // Limpa o bit (marca como livre)
        if (write_data_block(fd, bgdt[group_idx].bg_inode_bitmap, (char*)inode_bitmap_buffer) != 0) { // Escreve o bitmap atualizado
            fprintf(stderr, "deallocate_inode: Erro ao escrever bitmap de inodes atualizado para grupo %u.\n", group_idx);
            return; 
        }
        sb->s_free_inodes_count++; // Incrementa a contagem de inodes livres no superbloco
        bgdt[group_idx].bg_free_inodes_count++; // Incrementa a contagem de inodes livres no grupo
        
        if (write_superblock(fd, sb) != 0) { // Escreve o superbloco atualizado
            fprintf(stderr, "deallocate_inode: Erro ao escrever superbloco.\n");
        }
        if (write_group_descriptor(fd, sb, group_idx, &bgdt[group_idx]) != 0) { // Escreve o descritor de grupo atualizado
            fprintf(stderr, "deallocate_inode: Erro ao escrever descritor de grupo %u.\n", group_idx);
        }
    }
}

// Função para desalocar um bloco de dados.
// Limpa o bit correspondente no bitmap de blocos e atualiza as contagens de blocos livres.
void deallocate_data_block(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt, uint32_t block_num) {
    if (block_num == 0) { // Bloco 0 não é gerenciado por bitmaps de dados (pode ser boot block)
        fprintf(stderr, "deallocate_data_block: Tentativa de desalocar bloco de dados 0.\n");
        return;
    }
    // Calcula o grupo de blocos e o bit dentro do bitmap correspondente ao bloco.
    unsigned int group_idx = (block_num - sb->s_first_data_block) / sb->s_blocks_per_group;
    unsigned int bit_in_group = (block_num - sb->s_first_data_block) % sb->s_blocks_per_group;
    unsigned char block_bitmap_buffer[BLOCK_SIZE_FIXED];

    // Valida o índice do grupo.
    if (group_idx >= ((sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group)) {
        fprintf(stderr, "deallocate_data_block: Índice de grupo inválido %u para bloco %u.\n", group_idx, block_num);
        return;
    }
    
    // Lê o bitmap de blocos do grupo.
    if (read_data_block(fd, bgdt[group_idx].bg_block_bitmap, (char*)block_bitmap_buffer) != 0) {
        fprintf(stderr, "deallocate_data_block: Erro ao ler bitmap de blocos do grupo %u.\n", group_idx);
        return;
    }

    if (!is_bit_set(block_bitmap_buffer, bit_in_group)) { // Verifica se o bit já está limpo
        fprintf(stderr, "deallocate_data_block: Bloco %u (bit %u no grupo %u) já está livre.\n", block_num, bit_in_group, group_idx);
    } else {
        clear_bit(block_bitmap_buffer, bit_in_group); // Limpa o bit (marca como livre)
        if (write_data_block(fd, bgdt[group_idx].bg_block_bitmap, (char*)block_bitmap_buffer) != 0) { // Escreve o bitmap atualizado
            fprintf(stderr, "deallocate_data_block: Erro ao escrever bitmap de blocos atualizado para grupo %u.\n", group_idx);
            return;
        }
        sb->s_free_blocks_count++; // Incrementa a contagem de blocos livres no superbloco
        bgdt[group_idx].bg_free_blocks_count++; // Incrementa a contagem de blocos livres no grupo

        if (write_superblock(fd, sb) != 0) { // Escreve o superbloco atualizado
            fprintf(stderr, "deallocate_data_block: Erro ao escrever superbloco.\n");
        }
        if (write_group_descriptor(fd, sb, group_idx, &bgdt[group_idx]) != 0) { // Escreve o descritor de grupo atualizado
            fprintf(stderr, "deallocate_data_block: Erro ao escrever descritor de grupo %u.\n", group_idx);
        }
    }
}

// Implementa o comando 'rm' (remove arquivo), que deleta um arquivo regular.
void comando_rm(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
                uint32_t diretorio_atual_inode_num, const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) {
        printf("rm: Operando faltando\n");
        return;
    }

    char nome_arquivo[EXT2_NAME_LEN + 1];
    char caminho_pai_str[1024];
    uint32_t inode_pai_num;

    // 1. Analisa o caminho alvo para separar o nome do arquivo do caminho do diretório pai.
    const char *ultimo_slash = strrchr(path_alvo, '/');
    if (ultimo_slash != NULL) {
        size_t len_caminho_pai = ultimo_slash - path_alvo;
        if (len_caminho_pai == 0) { strcpy(caminho_pai_str, "/"); }
        else { strncpy(caminho_pai_str, path_alvo, len_caminho_pai); caminho_pai_str[len_caminho_pai] = '\0'; }
        strncpy(nome_arquivo, ultimo_slash + 1, EXT2_NAME_LEN); nome_arquivo[EXT2_NAME_LEN] = '\0';
    } else {
        strcpy(caminho_pai_str, "."); 
        strncpy(nome_arquivo, path_alvo, EXT2_NAME_LEN); nome_arquivo[EXT2_NAME_LEN] = '\0';
    }
    // Validações do nome do arquivo.
    if (strlen(nome_arquivo) == 0 || strcmp(nome_arquivo, ".") == 0 || strcmp(nome_arquivo, "..") == 0) {
        printf("rm: não é possível remover '%s': Nome de arquivo inválido\n", nome_arquivo);
        return;
    }

    // 2. Resolve o inode do diretório pai.
    uint8_t tipo_pai;
    inode_pai_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, caminho_pai_str, &tipo_pai);
    if (inode_pai_num == 0) {
        printf("rm: não foi possível remover '%s': Diretório pai '%s' não encontrado\n", path_alvo, caminho_pai_str);
        return;
    }
    struct ext2_inode inode_pai_obj;
    if (read_inode(fd, sb, bgdt, inode_pai_num, &inode_pai_obj) != 0 || !S_ISDIR(inode_pai_obj.i_mode)) {
        printf("rm: não foi possível remover '%s': Caminho pai '%s' não é um diretório\n", path_alvo, caminho_pai_str);
        return;
    }

    // 3. Resolve o inode do arquivo a ser removido.
    uint32_t arquivo_inode_num = dir_lookup(fd, sb, bgdt, inode_pai_num, nome_arquivo, NULL);
    if (arquivo_inode_num == 0) {
        printf("rm: não foi possível remover '%s': Arquivo ou diretório não encontrado\n", path_alvo);
        return;
    }

    // 4. Lê o inode do arquivo.
    struct ext2_inode arquivo_inode_obj;
    if (read_inode(fd, sb, bgdt, arquivo_inode_num, &arquivo_inode_obj) != 0) {
        printf("rm: erro ao ler inode %u para '%s'\n", arquivo_inode_num, path_alvo);
        return;
    }

    // 5. Verifica se é um arquivo regular (o 'rm' padrão não remove diretórios sem flag -r).
    if (S_ISDIR(arquivo_inode_obj.i_mode)) {
        printf("rm: não é possível remover '%s': É um diretório\n", path_alvo);
        return;
    }
    if (!S_ISREG(arquivo_inode_obj.i_mode)) {
         printf("rm: não é possível remover '%s': Não é um arquivo regular\n", path_alvo);
        return;
    }

    // 6. Remove a entrada do diretório pai.
    char dir_data_block[BLOCK_SIZE_FIXED];
    int entrada_removida_do_diretorio = 0;
    if (inode_pai_obj.i_block[0] == 0) { return; } 
    if (read_data_block(fd, inode_pai_obj.i_block[0], dir_data_block) != 0) { return; }

    unsigned int offset = 0;
    struct ext2_dir_entry_2 *current_entry = NULL;
    struct ext2_dir_entry_2 *prev_entry = NULL; 

    while (offset < inode_pai_obj.i_size) {
        current_entry = (struct ext2_dir_entry_2 *)(dir_data_block + offset);
        if (current_entry->rec_len == 0) break;

        if (current_entry->inode == arquivo_inode_num && current_entry->name_len == strlen(nome_arquivo) &&
            strncmp(current_entry->name, nome_arquivo, current_entry->name_len) == 0) {
            
            current_entry->inode = 0; // Marca a entrada como não utilizada (inode 0)
            entrada_removida_do_diretorio = 1;
            break; 
        }
        prev_entry = current_entry;
        offset += current_entry->rec_len;
    }

    if (!entrada_removida_do_diretorio) {
        printf("rm: inconsistência - arquivo encontrado por dir_lookup mas não na iteração do bloco do diretório.\n");
        return; 
    }

    if (write_data_block(fd, inode_pai_obj.i_block[0], dir_data_block) != 0) { // Escreve o bloco de dados do diretório pai
        printf("rm: erro ao escrever bloco de dados do diretório pai modificado.\n");
    }
    inode_pai_obj.i_mtime = inode_pai_obj.i_ctime = time(NULL); // Atualiza timestamps do diretório pai
    if (write_inode_table_entry(fd, sb, bgdt, inode_pai_num, &inode_pai_obj) != 0) { // Escreve o inode do diretório pai
        printf("rm: erro ao atualizar inode do diretório pai.\n");
    }

    // 7. Decrementa o link count do inode do arquivo.
    arquivo_inode_obj.i_links_count--; 

    // 8. Se o link count for 0, libera os blocos de dados do arquivo e o próprio inode.
    if (arquivo_inode_obj.i_links_count == 0) {
        // Libera blocos diretos
        for (int i = 0; i < 12; ++i) {
            if (arquivo_inode_obj.i_block[i] != 0) {
                deallocate_data_block(fd, sb, bgdt, arquivo_inode_obj.i_block[i]);
                arquivo_inode_obj.i_block[i] = 0; // Zera o ponteiro após desalocar
            }
        }
        // Libera bloco de indireção simples
        if (arquivo_inode_obj.i_block[12] != 0) {
            char indirect_block_buffer[BLOCK_SIZE_FIXED];
            if (read_data_block(fd, arquivo_inode_obj.i_block[12], indirect_block_buffer) == 0) {
                uint32_t *pointers = (uint32_t *)indirect_block_buffer;
                int num_pointers = BLOCK_SIZE_FIXED / sizeof(uint32_t);
                for (int i = 0; i < num_pointers; ++i) {
                    if (pointers[i] != 0) deallocate_data_block(fd, sb, bgdt, pointers[i]);
                }
            }
            deallocate_data_block(fd, sb, bgdt, arquivo_inode_obj.i_block[12]); // Desaloca o bloco de ponteiros
            arquivo_inode_obj.i_block[12] = 0;
        }
        // Libera bloco de dupla indireção
        if (arquivo_inode_obj.i_block[13] != 0) {
            char double_indirect_buffer[BLOCK_SIZE_FIXED];
            if (read_data_block(fd, arquivo_inode_obj.i_block[13], double_indirect_buffer) == 0) {
                uint32_t *lvl1_pointers = (uint32_t *)double_indirect_buffer;
                int num_lvl1_pointers = BLOCK_SIZE_FIXED / sizeof(uint32_t);
                for (int i = 0; i < num_lvl1_pointers; ++i) {
                    if (lvl1_pointers[i] != 0) {
                        char indirect_block_buffer[BLOCK_SIZE_FIXED];
                        if (read_data_block(fd, lvl1_pointers[i], indirect_block_buffer) == 0) {
                            uint32_t *lvl2_pointers = (uint32_t *)indirect_block_buffer;
                            int num_lvl2_pointers = BLOCK_SIZE_FIXED / sizeof(uint32_t);
                            for (int j = 0; j < num_lvl2_pointers; ++j) {
                                if (lvl2_pointers[j] != 0) deallocate_data_block(fd, sb, bgdt, lvl2_pointers[j]);
                            }
                        }
                        deallocate_data_block(fd, sb, bgdt, lvl1_pointers[i]);
                    }
                }
            }
            deallocate_data_block(fd, sb, bgdt, arquivo_inode_obj.i_block[13]);
            arquivo_inode_obj.i_block[13] = 0;
        }
        // A tripla indireção (i_block[14]) não é tratada aqui.

        arquivo_inode_obj.i_blocks = 0; // Zera a contagem de blocos
        arquivo_inode_obj.i_size = 0; // Define o tamanho do arquivo como 0
        arquivo_inode_obj.i_dtime = time(NULL); // Define o tempo de deleção

        // 9. Libera o inode do arquivo.
        deallocate_inode(fd, sb, bgdt, arquivo_inode_num);
        printf("rm: '%s' removido\n", path_alvo);
    } else {
        printf("rm: '%s' (links restantes: %u) - apenas entrada de diretório removida\n", path_alvo, arquivo_inode_obj.i_links_count);
    }
}

// Implementa o comando 'rmdir', que remove um diretório vazio.
void comando_rmdir(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
                  uint32_t diretorio_atual_inode_num, const char* path_alvo) {
    if (path_alvo == NULL || path_alvo[0] == '\0') {
        fprintf(stderr, "rmdir: caminho não especificado\n");
        return;
    }

    // Não permite remover "." ou "..".
    if (strcmp(path_alvo, ".") == 0 || strcmp(path_alvo, "..") == 0) {
        fprintf(stderr, "rmdir: não é possível remover '.' ou '..'\n");
        return;
    }

    // Obtém o inode do diretório a ser removido.
    uint8_t tipo_alvo;
    uint32_t dir_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                 path_alvo, &tipo_alvo);
    
    if (dir_inode_num == 0) {
        fprintf(stderr, "rmdir: diretório não encontrado: %s\n", path_alvo);
        return;
    }

    if (tipo_alvo != EXT2_FT_DIR) { // Verifica se é realmente um diretório
        fprintf(stderr, "rmdir: '%s' não é um diretório\n", path_alvo);
        return;
    }

    // Lê o inode do diretório a ser removido.
    struct ext2_inode dir_inode;
    if (read_inode(fd, sb, bgdt, dir_inode_num, &dir_inode) != 0) {
        fprintf(stderr, "rmdir: erro ao ler inode do diretório\n");
        return;
    }

    // Verifica se o diretório está vazio (contém apenas "." e "..").
    char dir_data[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, dir_inode.i_block[0], dir_data) != 0) {
        fprintf(stderr, "rmdir: erro ao ler bloco de dados do diretório\n");
        return;
    }

    size_t offset = 0;
    int entry_count = 0;
    while (offset < BLOCK_SIZE_FIXED) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(dir_data + offset);
        if (entry->inode != 0) {
            entry_count++;
            if (entry_count > 2) { // Se mais de 2 entradas (., ..), não está vazio
                fprintf(stderr, "rmdir: diretório não está vazio\n");
                return;
            }
        }
        offset += entry->rec_len;
        if (offset >= BLOCK_SIZE_FIXED || entry->rec_len == 0) break;
    }

    // Obtém o inode do diretório pai e o nome do diretório a ser removido.
    char *last_slash = strrchr(path_alvo, '/');
    const char *dir_name;
    char parent_path[1024];
    uint32_t parent_inode_num;

    if (last_slash == NULL) { // Caminho sem '/', o pai é o diretório atual
        parent_inode_num = diretorio_atual_inode_num;
        dir_name = path_alvo;
    } else {
        if (last_slash == path_alvo) { // Caso especial: "/nome"
            parent_inode_num = EXT2_ROOT_INO;
            dir_name = last_slash + 1;
        } else {
            strncpy(parent_path, path_alvo, last_slash - path_alvo);
            parent_path[last_slash - path_alvo] = '\0';
            uint8_t parent_type;
            parent_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num,
                                                  parent_path, &parent_type);
            dir_name = last_slash + 1;
        }
    }

    // Lê o inode do diretório pai.
    struct ext2_inode parent_inode;
    if (read_inode(fd, sb, bgdt, parent_inode_num, &parent_inode) != 0) {
        fprintf(stderr, "rmdir: erro ao ler inode do diretório pai\n");
        return;
    }

    // Lê o bloco de dados do diretório pai.
    char parent_data[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, parent_inode.i_block[0], parent_data) != 0) {
        fprintf(stderr, "rmdir: erro ao ler bloco de dados do diretório pai\n");
        return;
    }

    // Procura e remove a entrada do diretório no diretório pai.
    offset = 0;
    struct ext2_dir_entry_2 *prev_entry = NULL;
    while (offset < BLOCK_SIZE_FIXED) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(parent_data + offset);
        if (entry->inode != 0 && entry->name_len == strlen(dir_name) &&
            strncmp(entry->name, dir_name, entry->name_len) == 0) {
            
            // Se for a última entrada no bloco, apenas ajustar o rec_len da entrada anterior para recuperar o espaço.
            if (offset + entry->rec_len >= BLOCK_SIZE_FIXED) {
                if (prev_entry) {
                    prev_entry->rec_len += entry->rec_len;
                }
            } else { // Caso contrário, move as entradas seguintes para preencher o vazio.
                size_t remaining = BLOCK_SIZE_FIXED - (offset + entry->rec_len);
                memmove(parent_data + offset,
                       parent_data + offset + entry->rec_len,
                       remaining);
                
                // Ajusta o rec_len da "nova" última entrada para cobrir o espaço liberado.
                struct ext2_dir_entry_2 *last = (struct ext2_dir_entry_2 *)(parent_data + BLOCK_SIZE_FIXED - entry->rec_len);
                last->rec_len += entry->rec_len;
            }

            // Atualiza o bloco de dados do diretório pai no disco.
            if (write_data_block(fd, parent_inode.i_block[0], parent_data) != 0) {
                fprintf(stderr, "rmdir: erro ao escrever bloco de dados do diretório pai\n");
                return;
            }

            // Atualiza os timestamps do diretório pai.
            parent_inode.i_mtime = time(NULL);
            parent_inode.i_ctime = time(NULL);
            if (write_inode_table_entry(fd, sb, bgdt, parent_inode_num, &parent_inode) != 0) {
                fprintf(stderr, "rmdir: erro ao atualizar inode do diretório pai\n");
                return;
            }

            // Desaloca o bloco de dados do diretório que foi removido.
            deallocate_data_block(fd, sb, bgdt, dir_inode.i_block[0]);

            // Desaloca o inode do diretório que foi removido.
            deallocate_inode(fd, sb, bgdt, dir_inode_num);

            // Decrementa o contador de diretórios usados no grupo de blocos.
            uint32_t group_idx = (dir_inode_num - 1) / sb->s_inodes_per_group;
            bgdt[group_idx].bg_used_dirs_count--;
            if (write_group_descriptor(fd, sb, group_idx, &bgdt[group_idx]) != 0) {
                fprintf(stderr, "rmdir: erro ao atualizar descritor do grupo\n");
            }
            printf("rmdir: diretório removido com sucesso: %s\n", path_alvo);
            return;
        }
        prev_entry = entry;
        offset += entry->rec_len;
        if (offset >= BLOCK_SIZE_FIXED || entry->rec_len == 0) break;
    }

    fprintf(stderr, "rmdir: erro interno - entrada do diretório não encontrada\n");
}

// Implementa o comando 'rename', que renomeia um arquivo ou diretório.
// Atualmente, suporta apenas renomear dentro do mesmo diretório.
void comando_rename(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
                   uint32_t diretorio_atual_inode_num, const char* path_origem, const char* path_destino) {
    if (path_origem == NULL || path_origem[0] == '\0' || 
        path_destino == NULL || path_destino[0] == '\0') {
        fprintf(stderr, "rename: origem e destino devem ser especificados\n");
        return;
    }

    // Não permite renomear "." ou "..".
    if (strcmp(path_origem, ".") == 0 || strcmp(path_origem, "..") == 0 ||
        strcmp(path_destino, ".") == 0 || strcmp(path_destino, "..") == 0) {
        fprintf(stderr, "rename: não é possível renomear '.' ou '..'\n");
        return;
    }

    // Obtém o inode do arquivo/diretório de origem.
    uint8_t tipo_origem;
    uint32_t origem_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                    path_origem, &tipo_origem);
    
    if (origem_inode_num == 0) {
        fprintf(stderr, "rename: arquivo/diretório de origem não encontrado: %s\n", path_origem);
        return;
    }

    // Verifica se o destino já existe.
    uint8_t tipo_destino;
    uint32_t destino_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                     path_destino, &tipo_destino);
    
    if (destino_inode_num != 0) {
        fprintf(stderr, "rename: destino já existe: %s\n", path_destino);
        return;
    }

    // Obtém o diretório pai e o nome base do arquivo de origem.
    char *origem_last_slash = strrchr(path_origem, '/');
    const char *origem_name;
    char origem_parent_path[1024];
    uint32_t origem_parent_inode_num;

    if (origem_last_slash == NULL) {
        origem_parent_inode_num = diretorio_atual_inode_num;
        origem_name = path_origem;
    } else {
        if (origem_last_slash == path_origem) {
            origem_parent_inode_num = EXT2_ROOT_INO;
            origem_name = origem_last_slash + 1;
        } else {
            strncpy(origem_parent_path, path_origem, origem_last_slash - path_origem);
            origem_parent_path[origem_last_slash - path_origem] = '\0';
            uint8_t parent_type;
            origem_parent_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num,
                                                         origem_parent_path, &parent_type);
            origem_name = origem_last_slash + 1;
        }
    }

    // Obtém o diretório pai e o nome base do arquivo de destino.
    char *destino_last_slash = strrchr(path_destino, '/');
    const char *destino_name;
    char destino_parent_path[1024];
    uint32_t destino_parent_inode_num;

    if (destino_last_slash == NULL) {
        destino_parent_inode_num = diretorio_atual_inode_num;
        destino_name = path_destino;
    } else {
        if (destino_last_slash == path_destino) {
            destino_parent_inode_num = EXT2_ROOT_INO;
            destino_name = destino_last_slash + 1;
        } else {
            strncpy(destino_parent_path, path_destino, destino_last_slash - path_destino);
            destino_parent_path[destino_last_slash - path_destino] = '\0';
            uint8_t parent_type;
            destino_parent_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num,
                                                          destino_parent_path, &parent_type);
            destino_name = destino_last_slash + 1;
        }
    }

    // Verifica se o diretório pai do destino existe.
    if (destino_parent_inode_num == 0) {
        fprintf(stderr, "rename: diretório pai do destino não existe\n");
        return;
    }

    // Se origem e destino estão no mesmo diretório, a operação é mais simples (apenas renomear a entrada).
    if (origem_parent_inode_num == destino_parent_inode_num) {
        // Lê o inode do diretório pai.
        struct ext2_inode parent_inode;
        if (read_inode(fd, sb, bgdt, origem_parent_inode_num, &parent_inode) != 0) {
            fprintf(stderr, "rename: erro ao ler inode do diretório pai\n");
            return;
        }

        // Lê o bloco de dados do diretório.
        char dir_data[BLOCK_SIZE_FIXED];
        if (read_data_block(fd, parent_inode.i_block[0], dir_data) != 0) {
            fprintf(stderr, "rename: erro ao ler bloco de dados do diretório\n");
            return;
        }

        // Procura a entrada a ser renomeada.
        size_t offset = 0;
        while (offset < BLOCK_SIZE_FIXED) {
            struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(dir_data + offset);
            if (entry->inode != 0 && 
                entry->name_len == strlen(origem_name) &&
                strncmp(entry->name, origem_name, entry->name_len) == 0) {
                
                // Atualiza o comprimento e o nome na entrada de diretório.
                entry->name_len = strlen(destino_name);
                strncpy(entry->name, destino_name, entry->name_len);

                // Escreve o bloco de dados atualizado de volta ao disco.
                if (write_data_block(fd, parent_inode.i_block[0], dir_data) != 0) {
                    fprintf(stderr, "rename: erro ao escrever bloco de dados do diretório\n");
                    return;
                }

                // Atualiza os timestamps do diretório pai.
                parent_inode.i_mtime = time(NULL);
                parent_inode.i_ctime = time(NULL);
                if (write_inode_table_entry(fd, sb, bgdt, origem_parent_inode_num, &parent_inode) != 0) {
                    fprintf(stderr, "rename: erro ao atualizar inode do diretório pai\n");
                }
                printf("rename: arquivo renomeado com sucesso: %s -> %s\n", path_origem, path_destino);
                return;
            }
            offset += entry->rec_len;
            if (offset >= BLOCK_SIZE_FIXED || entry->rec_len == 0) break;
        }

        fprintf(stderr, "rename: erro interno - entrada não encontrada\n");
        return;
    }

    // Se origem e destino estão em diretórios diferentes, a funcionalidade de mover não está implementada.
    fprintf(stderr, "rename: não é possível mover entre diretórios diferentes ainda\n");
    return;
}

// Implementa o comando 'mv' (move/rename), que move ou renomeia arquivos/diretórios.
// Atualmente, suporta renomear dentro do mesmo diretório e mover para um diretório existente.
void comando_mv(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
                uint32_t diretorio_atual_inode_num, const char* path_origem, const char* path_destino) {
    if (path_origem == NULL || path_origem[0] == '\0' || 
        path_destino == NULL || path_destino[0] == '\0') {
        fprintf(stderr, "mv: origem e destino devem ser especificados\n");
        return;
    }

    // Não permite mover "." ou "..".
    if (strcmp(path_origem, ".") == 0 || strcmp(path_origem, "..") == 0 ||
        strcmp(path_destino, ".") == 0 || strcmp(path_destino, "..") == 0) {
        fprintf(stderr, "mv: não é possível mover '.' ou '..'\n");
        return;
    }

    // Obtém o inode do arquivo/diretório de origem.
    uint8_t tipo_origem;
    uint32_t origem_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                    path_origem, &tipo_origem);
    
    if (origem_inode_num == 0) {
        fprintf(stderr, "mv: arquivo/diretório de origem não encontrado: %s\n", path_origem);
        return;
    }

    // Verifica se o destino existe e, se sim, se é um diretório.
    uint8_t tipo_destino;
    uint32_t destino_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                     path_destino, &tipo_destino);
    
    // Obtém o nome base do arquivo de origem.
    const char *origem_name = strrchr(path_origem, '/');
    if (origem_name == NULL) {
        origem_name = path_origem;
    } else {
        origem_name++; // Pula a barra
    }

    // Determina o caminho de destino efetivo (se mover para dentro de um diretório).
    char novo_path_destino[1024];
    const char *destino_efetivo;
    
    if (destino_inode_num != 0) {
        if (tipo_destino == EXT2_FT_DIR) {
            // Destino é um diretório, move para dentro dele.
            snprintf(novo_path_destino, sizeof(novo_path_destino), "%s/%s", 
                    path_destino, origem_name);
            destino_efetivo = novo_path_destino;

            // Verifica se já existe um arquivo com o mesmo nome no diretório de destino.
            uint8_t tipo_temp;
            uint32_t temp_inode = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                     destino_efetivo, &tipo_temp);
            if (temp_inode != 0) {
                fprintf(stderr, "mv: já existe um arquivo '%s' no diretório de destino\n", origem_name);
                return;
            }
        } else {
            fprintf(stderr, "mv: destino já existe e não é um diretório: %s\n", path_destino);
            return;
        }
    } else { // Destino não existe, é um novo nome no diretório pai do destino.
        destino_efetivo = path_destino;
    }

    // Obtém o diretório pai e o nome base do arquivo de origem.
    char *origem_last_slash = strrchr(path_origem, '/');
    char origem_parent_path[1024];
    uint32_t origem_parent_inode_num;

    if (origem_last_slash == NULL) {
        origem_parent_inode_num = diretorio_atual_inode_num;
    } else {
        if (origem_last_slash == path_origem) {
            origem_parent_inode_num = EXT2_ROOT_INO;
        } else {
            strncpy(origem_parent_path, path_origem, origem_last_slash - path_origem);
            origem_parent_path[origem_last_slash - path_origem] = '\0';
            uint8_t parent_type;
            origem_parent_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num,
                                                         origem_parent_path, &parent_type);
        }
    }

    // Obtém o diretório pai e o nome base do arquivo de destino.
    char *destino_last_slash = strrchr(destino_efetivo, '/');
    const char *destino_name;
    char destino_parent_path[1024];
    uint32_t destino_parent_inode_num;

    if (destino_last_slash == NULL) {
        destino_parent_inode_num = diretorio_atual_inode_num;
        destino_name = destino_efetivo;
    } else {
        if (destino_last_slash == destino_efetivo) {
            destino_parent_inode_num = EXT2_ROOT_INO;
            destino_name = destino_last_slash + 1;
        } else {
            strncpy(destino_parent_path, destino_efetivo, destino_last_slash - destino_efetivo);
            destino_parent_path[destino_last_slash - destino_efetivo] = '\0';
            uint8_t parent_type;
            destino_parent_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num,
                                                          destino_parent_path, &parent_type);
            destino_name = destino_last_slash + 1;
        }
    }

    // Verifica se o diretório pai do destino existe.
    if (destino_parent_inode_num == 0) {
        fprintf(stderr, "mv: diretório pai do destino não existe\n");
        return;
    }

    // Primeiro, lê os inodes dos diretórios pai.
    struct ext2_inode origem_parent_inode;
    if (read_inode(fd, sb, bgdt, origem_parent_inode_num, &origem_parent_inode) != 0) {
        fprintf(stderr, "mv: erro ao ler inode do diretório pai de origem\n");
        return;
    }

    struct ext2_inode destino_parent_inode;
    if (read_inode(fd, sb, bgdt, destino_parent_inode_num, &destino_parent_inode) != 0) {
        fprintf(stderr, "mv: erro ao ler inode do diretório pai de destino\n");
        return;
    }

    // Lê os blocos de dados dos diretórios.
    char origem_dir_data[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, origem_parent_inode.i_block[0], origem_dir_data) != 0) {
        fprintf(stderr, "mv: erro ao ler bloco de dados do diretório de origem\n");
        return;
    }

    char destino_dir_data[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, destino_parent_inode.i_block[0], destino_dir_data) != 0) {
        fprintf(stderr, "mv: erro ao ler bloco de dados do diretório de destino\n");
        return;
    }

    // Encontra a entrada a ser movida/renomeada no diretório de origem.
    size_t origem_offset = 0;
    struct ext2_dir_entry_2 *origem_prev_entry = NULL;
    struct ext2_dir_entry_2 *origem_entry = NULL;
    uint16_t origem_entry_len = 0;

    while (origem_offset < BLOCK_SIZE_FIXED) {
        origem_entry = (struct ext2_dir_entry_2 *)(origem_dir_data + origem_offset);
        if (origem_entry->inode != 0 && 
            origem_entry->name_len == strlen(origem_name) &&
            strncmp(origem_entry->name, origem_name, origem_entry->name_len) == 0) {
            origem_entry_len = origem_entry->rec_len;
            break;
        }
        origem_prev_entry = origem_entry;
        origem_offset += origem_entry->rec_len;
        if (origem_offset >= BLOCK_SIZE_FIXED || origem_entry->rec_len == 0) break;
    }

    if (origem_offset >= BLOCK_SIZE_FIXED || origem_entry == NULL) {
        fprintf(stderr, "mv: erro interno - entrada de origem não encontrada\n");
        return;
    }

    // Calcula o espaço necessário para a nova entrada no diretório de destino.
    uint16_t rec_len_necessario = (offsetof(struct ext2_dir_entry_2, name) + strlen(destino_name) + 3) & ~3;

    // Procura espaço no diretório de destino para a nova entrada.
    size_t destino_offset = 0;
    struct ext2_dir_entry_2 *destino_prev_entry = NULL;

    while (destino_offset < BLOCK_SIZE_FIXED) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(destino_dir_data + destino_offset);
        
        uint16_t real_len = (offsetof(struct ext2_dir_entry_2, name) + entry->name_len + 3) & ~3;
        uint16_t espaco_extra = entry->rec_len - real_len;

        if (espaco_extra >= rec_len_necessario) { // Encontrou espaço suficiente.
            // Ajusta o rec_len da entrada atual no destino.
            entry->rec_len = real_len;
            
            // Cria a nova entrada no destino.
            struct ext2_dir_entry_2 *nova_entrada = (struct ext2_dir_entry_2 *)(destino_dir_data + destino_offset + real_len);
            nova_entrada->inode = origem_entry->inode;
            nova_entrada->rec_len = espaco_extra;
            nova_entrada->name_len = strlen(destino_name);
            nova_entrada->file_type = origem_entry->file_type;
            strncpy(nova_entrada->name, destino_name, strlen(destino_name));

            // Remove a entrada antiga do diretório de origem.
            if (origem_offset + origem_entry_len >= BLOCK_SIZE_FIXED) {
                if (origem_prev_entry) {
                    origem_prev_entry->rec_len += origem_entry_len;
                }
            } else {
                size_t remaining = BLOCK_SIZE_FIXED - (origem_offset + origem_entry_len);
                memmove(origem_dir_data + origem_offset,
                       origem_dir_data + origem_offset + origem_entry_len,
                       remaining);
                
                struct ext2_dir_entry_2 *last = (struct ext2_dir_entry_2 *)(origem_dir_data + BLOCK_SIZE_FIXED - origem_entry_len);
                last->rec_len += origem_entry_len;
            }

            // Escreve as alterações nos blocos de dados de ambos os diretórios.
            if (write_data_block(fd, destino_parent_inode.i_block[0], destino_dir_data) != 0) {
                fprintf(stderr, "mv: erro ao escrever bloco de dados do diretório de destino\n");
                return;
            }

            if (write_data_block(fd, origem_parent_inode.i_block[0], origem_dir_data) != 0) {
                fprintf(stderr, "mv: erro ao escrever bloco de dados do diretório de origem\n");
                return;
            }

            // Se o que foi movido é um diretório, atualiza sua entrada ".." para apontar para o novo pai.
            if (tipo_origem == EXT2_FT_DIR) {
                struct ext2_inode dir_inode;
                if (read_inode(fd, sb, bgdt, origem_entry->inode, &dir_inode) != 0) {
                    fprintf(stderr, "mv: erro ao ler inode do diretório movido\n");
                    return;
                }

                char dir_content[BLOCK_SIZE_FIXED];
                if (read_data_block(fd, dir_inode.i_block[0], dir_content) != 0) {
                    fprintf(stderr, "mv: erro ao ler conteúdo do diretório movido\n");
                    return;
                }

                struct ext2_dir_entry_2 *dotdot = (struct ext2_dir_entry_2 *)(dir_content + 
                    ((struct ext2_dir_entry_2 *)dir_content)->rec_len);
                if (strncmp(dotdot->name, "..", 2) == 0) { // Encontra a entrada ".."
                    dotdot->inode = destino_parent_inode_num; // Atualiza o inode do pai
                    
                    if (write_data_block(fd, dir_inode.i_block[0], dir_content) != 0) { // Escreve o bloco de dados do diretório movido
                        fprintf(stderr, "mv: erro ao atualizar entrada '..' do diretório\n");
                        return;
                    }
                }

                // Atualiza os contadores de diretório usados nos grupos de blocos, se os grupos de origem e destino forem diferentes.
                uint32_t origem_group = (origem_parent_inode_num - 1) / sb->s_inodes_per_group;
                uint32_t destino_group = (destino_parent_inode_num - 1) / sb->s_inodes_per_group;
                
                if (origem_group != destino_group) {
                    bgdt[origem_group].bg_used_dirs_count--;
                    bgdt[destino_group].bg_used_dirs_count++;
                    
                    if (write_group_descriptor(fd, sb, origem_group, &bgdt[origem_group]) != 0) {
                        fprintf(stderr, "mv: erro ao atualizar descritor do grupo de origem\n");
                    }
                    if (write_group_descriptor(fd, sb, destino_group, &bgdt[destino_group]) != 0) {
                        fprintf(stderr, "mv: erro ao atualizar descritor do grupo de destino\n");
                    }
                }
            }

            // Atualiza os tempos de modificação dos diretórios pai (origem e destino).
            time_t current_time = time(NULL);
            origem_parent_inode.i_mtime = current_time;
            origem_parent_inode.i_ctime = current_time;
            destino_parent_inode.i_mtime = current_time;
            destino_parent_inode.i_ctime = current_time;

            if (write_inode_table_entry(fd, sb, bgdt, origem_parent_inode_num, &origem_parent_inode) != 0) {
                fprintf(stderr, "mv: erro ao atualizar inode do diretório pai de origem\n");
            }
            if (write_inode_table_entry(fd, sb, bgdt, destino_parent_inode_num, &destino_parent_inode) != 0) {
                fprintf(stderr, "mv: erro ao atualizar inode do diretório pai de destino\n");
            }
            printf("mv: arquivo movido com sucesso: %s -> %s\n", path_origem, destino_efetivo);
            return;
        }

        destino_prev_entry = entry;
        destino_offset += entry->rec_len;
        if (destino_offset >= BLOCK_SIZE_FIXED || entry->rec_len == 0) break;
    }

    fprintf(stderr, "mv: não há espaço suficiente no diretório de destino\n");
}

// Implementa o comando 'cp' (copy), que copia um arquivo.
// Atualmente, suporta apenas copiar arquivos regulares (não diretórios).
void comando_cp(int fd, struct ext2_super_block *sb, struct ext2_group_desc *bgdt,
              uint32_t diretorio_atual_inode_num, const char* path_origem, const char* path_destino) {
    // Verifica se o arquivo de origem existe.
    uint8_t tipo_origem;
    uint32_t origem_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                   path_origem, &tipo_origem);
    
    if (origem_inode_num == 0) {
        fprintf(stderr, "cp: arquivo de origem não encontrado: %s\n", path_origem);
        return;
    }

    // Não permite copiar diretórios (funcionalidade não implementada).
    if (tipo_origem == EXT2_FT_DIR) {
        fprintf(stderr, "cp: não é possível copiar diretórios (ainda não implementado)\n");
        return;
    }

    // Lê o inode do arquivo de origem.
    struct ext2_inode origem_inode;
    if (read_inode(fd, sb, bgdt, origem_inode_num, &origem_inode) != 0) {
        fprintf(stderr, "cp: erro ao ler inode do arquivo de origem\n");
        return;
    }

    // Obtém o nome base do arquivo de origem.
    const char *origem_name = strrchr(path_origem, '/');
    if (origem_name == NULL) {
        origem_name = path_origem;
    } else {
        origem_name++;
    }

    // Verifica se o destino existe e, se sim, se é um diretório.
    uint8_t tipo_destino;
    uint32_t destino_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, 
                                                    path_destino, &tipo_destino);

    char caminho_final[1024];
    const char *nome_final;

    // Se o destino existe e é um diretório, o arquivo será copiado para dentro dele com o mesmo nome.
    if (destino_inode_num != 0 && tipo_destino == EXT2_FT_DIR) {
        snprintf(caminho_final, sizeof(caminho_final), "%s/%s", path_destino, origem_name);
        nome_final = origem_name;
    } else { // Caso contrário, o destino é o novo nome do arquivo.
        strncpy(caminho_final, path_destino, sizeof(caminho_final) - 1);
        nome_final = strrchr(path_destino, '/');
        if (nome_final == NULL) {
            nome_final = path_destino;
        } else {
            nome_final++;
        }
    }

    // Verifica se já existe um arquivo com o mesmo nome no destino.
    uint8_t tipo_temp;
    if (path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, caminho_final, &tipo_temp) != 0) {
        fprintf(stderr, "cp: arquivo de destino já existe: %s\n", caminho_final);
        return;
    }

    // Obtém o diretório pai do destino.
    char destino_parent_path[1024];
    uint32_t destino_parent_inode_num;

    if (strrchr(caminho_final, '/') != NULL) {
        strncpy(destino_parent_path, caminho_final, strrchr(caminho_final, '/') - caminho_final);
        destino_parent_path[strrchr(caminho_final, '/') - caminho_final] = '\0';
        
        uint8_t parent_type;
        destino_parent_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num,
                                                     destino_parent_path, &parent_type);
    } else {
        destino_parent_inode_num = diretorio_atual_inode_num;
    }

    // Aloca um novo inode para o arquivo de destino.
    uint32_t novo_inode_num = allocate_inode(fd, sb, bgdt);
    if (novo_inode_num == 0) {
        fprintf(stderr, "cp: não foi possível alocar novo inode\n");
        return;
    }

    // Cria um novo inode copiando o original e atualizando os timestamps.
    struct ext2_inode novo_inode;
    memcpy(&novo_inode, &origem_inode, sizeof(struct ext2_inode));
    
    time_t current_time = time(NULL);
    novo_inode.i_ctime = current_time;
    novo_inode.i_atime = current_time;
    novo_inode.i_mtime = current_time;
    novo_inode.i_links_count = 1; // Um link para o novo arquivo.

    // Aloca novos blocos de dados e copia o conteúdo do arquivo de origem para o de destino.
    for (int i = 0; i < EXT2_N_BLOCKS && i < novo_inode.i_blocks / (BLOCK_SIZE_FIXED/512); i++) {
        if (origem_inode.i_block[i] == 0) continue; // Pula blocos não alocados.

        uint32_t novo_bloco = allocate_data_block(fd, sb, bgdt); // Aloca um novo bloco.
        if (novo_bloco == 0) {
            fprintf(stderr, "cp: erro ao alocar bloco de dados\n");
            // Em caso de erro, limpa os blocos já alocados para evitar vazamento.
            for (int j = 0; j < i; j++) {
                if (novo_inode.i_block[j] != 0) {
                    deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[j]);
                }
            }
            deallocate_inode(fd, sb, bgdt, novo_inode_num);
            return;
        }

        char buffer[BLOCK_SIZE_FIXED];
        if (read_data_block(fd, origem_inode.i_block[i], buffer) != 0) { // Lê o bloco do arquivo de origem.
            fprintf(stderr, "cp: erro ao ler bloco de dados do arquivo de origem\n");
            for (int j = 0; j <= i; j++) {
                if (novo_inode.i_block[j] != 0) {
                    deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[j]);
                }
            }
            deallocate_inode(fd, sb, bgdt, novo_inode_num);
            return;
        }

        if (write_data_block(fd, novo_bloco, buffer) != 0) { // Escreve o bloco no arquivo de destino.
            fprintf(stderr, "cp: erro ao escrever bloco de dados do arquivo de destino\n");
            for (int j = 0; j <= i; j++) {
                if (novo_inode.i_block[j] != 0) {
                    deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[j]);
                }
            }
            deallocate_inode(fd, sb, bgdt, novo_inode_num);
            return;
        }

        novo_inode.i_block[i] = novo_bloco; // Atualiza o ponteiro do novo inode.
    }

    // Escreve o novo inode no disco.
    if (write_inode_table_entry(fd, sb, bgdt, novo_inode_num, &novo_inode) != 0) {
        fprintf(stderr, "cp: erro ao escrever novo inode\n");
        for (int i = 0; i < EXT2_N_BLOCKS; i++) {
            if (novo_inode.i_block[i] != 0) {
                deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[i]);
            }
        }
        deallocate_inode(fd, sb, bgdt, novo_inode_num);
        return;
    }

    // Lê o inode do diretório pai do destino.
    struct ext2_inode destino_parent_inode;
    if (read_inode(fd, sb, bgdt, destino_parent_inode_num, &destino_parent_inode) != 0) {
        fprintf(stderr, "cp: erro ao ler inode do diretório pai de destino\n");
        for (int i = 0; i < EXT2_N_BLOCKS; i++) {
            if (novo_inode.i_block[i] != 0) {
                deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[i]);
            }
        }
        deallocate_inode(fd, sb, bgdt, novo_inode_num);
        return;
    }

    // Lê o bloco de dados do diretório pai do destino.
    char dir_data[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, destino_parent_inode.i_block[0], dir_data) != 0) {
        fprintf(stderr, "cp: erro ao ler bloco de dados do diretório pai\n");
        for (int i = 0; i < EXT2_N_BLOCKS; i++) {
            if (novo_inode.i_block[i] != 0) {
                deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[i]);
            }
        }
        deallocate_inode(fd, sb, bgdt, novo_inode_num);
        return;
    }

    // Procura espaço no diretório pai do destino para adicionar a nova entrada.
    size_t offset = 0;
    uint16_t rec_len_necessario = (offsetof(struct ext2_dir_entry_2, name) + strlen(nome_final) + 3) & ~3;

    while (offset < BLOCK_SIZE_FIXED) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(dir_data + offset);
        
        uint16_t real_len = (offsetof(struct ext2_dir_entry_2, name) + entry->name_len + 3) & ~3;
        uint16_t espaco_extra = entry->rec_len - real_len;

        if (espaco_extra >= rec_len_necessario) { // Encontrou espaço suficiente.
            entry->rec_len = real_len; // Encurta a entrada atual.
            
            // Cria a nova entrada de diretório.
            struct ext2_dir_entry_2 *nova_entrada = (struct ext2_dir_entry_2 *)(dir_data + offset + real_len);
            nova_entrada->inode = novo_inode_num;
            nova_entrada->rec_len = espaco_extra;
            nova_entrada->name_len = strlen(nome_final);
            nova_entrada->file_type = tipo_origem;
            strncpy(nova_entrada->name, nome_final, strlen(nome_final));

            // Escreve o bloco de dados do diretório pai atualizado.
            if (write_data_block(fd, destino_parent_inode.i_block[0], dir_data) != 0) {
                fprintf(stderr, "cp: erro ao escrever bloco de dados do diretório pai\n");
                for (int i = 0; i < EXT2_N_BLOCKS; i++) {
                    if (novo_inode.i_block[i] != 0) {
                        deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[i]);
                    }
                }
                deallocate_inode(fd, sb, bgdt, novo_inode_num);
                return;
            }

            // Atualiza os timestamps do diretório pai e escreve o inode de volta.
            destino_parent_inode.i_mtime = current_time;
            destino_parent_inode.i_ctime = current_time;
            
            if (write_inode_table_entry(fd, sb, bgdt, destino_parent_inode_num, &destino_parent_inode) != 0) {
                fprintf(stderr, "cp: erro ao atualizar inode do diretório pai\n");
                return;
            }
            printf("cp: arquivo copiado com sucesso: %s -> %s\n", path_origem, caminho_final);
            return;
        }

        offset += entry->rec_len;
        if (offset >= BLOCK_SIZE_FIXED || entry->rec_len == 0) break;
    }

    fprintf(stderr, "cp: não há espaço suficiente no diretório de destino\n");
    // Em caso de falha em adicionar a entrada no diretório, limpa os recursos alocados.
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        if (novo_inode.i_block[i] != 0) {
            deallocate_data_block(fd, sb, bgdt, novo_inode.i_block[i]);
        }
    }
    deallocate_inode(fd, sb, bgdt, novo_inode_num);
}

// Função principal do programa.
int main(int argc, char *argv[]) {
    // Verifica se o caminho da imagem de disco foi fornecido.
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <imagem_ext2>\n", argv[0]);
        return 1;
    }

    const char *disk_image_path = argv[1];
    struct ext2_super_block sb;
    struct ext2_group_desc *bgdt = NULL; 
    unsigned int num_block_groups = 0;
    int fd = -1; 

    printf("Tentando ler o superbloco de: %s\n", disk_image_path);
    fd = read_superblock(disk_image_path, &sb); // Tenta ler o superbloco.

    if (fd < 0) {
        return 1;
    }
    printf("Superbloco lido com sucesso!\n\n");

    // Verifica o magic number para confirmar que é um Ext2.
    if (sb.s_magic != 0xEF53) {
        fprintf(stderr, "Erro: A imagem fornecida não parece ser um sistema de arquivos Ext2 (magic number incorreto).\n");
        close(fd);
        return 1;
    }

    // Lê a Tabela de Descritores de Grupo de Blocos (BGDT).
    bgdt = read_block_group_descriptor_table(fd, &sb, &num_block_groups);
    if (!bgdt) {
        fprintf(stderr, "Falha ao ler a Tabela de Descritores de Grupo de Blocos.\n");
        close(fd);
        return 1;
    }

    char comando[100];
    char prompt[200];
    
    // Extrai o nome da imagem para usar no prompt.
    const char *image_name_for_prompt = strrchr(disk_image_path, '/');
    if (image_name_for_prompt == NULL) {
        image_name_for_prompt = disk_image_path;
    } else {
        image_name_for_prompt++; 
    }

    // Inicializa o diretório atual como a raiz ("/").
    char diretorio_atual[1024] = "/"; 

    // Define o inode do diretório atual do shell como o inode raiz.
    uint32_t diretorio_atual_inode = EXT2_ROOT_INO;

    // Loop principal do shell.
    while(1) {
        // Monta o prompt.
        snprintf(prompt, sizeof(prompt), "ext2shell:[%s:%s] $ ", image_name_for_prompt, diretorio_atual);
        printf("%s", prompt);
        
        // Lê a linha de comando.
        if (fgets(comando, sizeof(comando), stdin) == NULL) {
            printf("\nSaindo.\n"); 
            break;
        }
        
        // Extrai o primeiro token (o comando).
        char *primeiro_token = strtok(comando, " \t\n"); 
        if (primeiro_token == NULL) { 
            continue;
        }

        // Processa os comandos.
        if (strcmp(primeiro_token, "info") == 0) {
            comando_info(&sb);
        } else if (strcmp(primeiro_token, "ls") == 0) {
            char *arg_path = strtok(NULL, " \t\n"); 
            comando_ls(fd, &sb, bgdt, diretorio_atual_inode, arg_path);
        } else if (strcmp(primeiro_token, "cat") == 0) {
            char *arg_path_cat = strtok(NULL, " \t\n");
            comando_cat(fd, &sb, bgdt, diretorio_atual_inode, arg_path_cat);
        } else if (strcmp(primeiro_token, "attr") == 0) {
            char *arg_path_attr = strtok(NULL, " \t\n");
            comando_attr(fd, &sb, bgdt, diretorio_atual_inode, arg_path_attr);
        } else if (strcmp(primeiro_token, "pwd") == 0) {
            comando_pwd(diretorio_atual);
        } else if (strcmp(primeiro_token, "cd") == 0) {
            char *arg_path_cd = strtok(NULL, " \t\n");
            comando_cd(fd, &sb, bgdt, &diretorio_atual_inode, diretorio_atual, sizeof(diretorio_atual), arg_path_cd);
        } else if (strcmp(primeiro_token, "touch") == 0) {
            char *arg_path_touch = strtok(NULL, " \t\n");
            comando_touch(fd, &sb, bgdt, diretorio_atual_inode, diretorio_atual, arg_path_touch);
        } else if (strcmp(primeiro_token, "mkdir") == 0) {
            char *arg_path_mkdir = strtok(NULL, " \t\n");
            comando_mkdir(fd, &sb, bgdt, diretorio_atual_inode, diretorio_atual, arg_path_mkdir);
        } else if (strcmp(primeiro_token, "rm") == 0) {
            char *arg_path_rm = strtok(NULL, " \t\n");
            comando_rm(fd, &sb, bgdt, diretorio_atual_inode, arg_path_rm);
        } else if (strcmp(primeiro_token, "rmdir") == 0) {
            char *arg_path_rmdir = strtok(NULL, " \t\n");
            comando_rmdir(fd, &sb, bgdt, diretorio_atual_inode, arg_path_rmdir);
        } else if (strcmp(primeiro_token, "rename") == 0) {
            char *arg_path_origem = strtok(NULL, " \t\n");
            char *arg_path_destino = strtok(NULL, " \t\n");
            comando_rename(fd, &sb, bgdt, diretorio_atual_inode, arg_path_origem, arg_path_destino);
        } else if (strcmp(primeiro_token, "mv") == 0) {
            char *arg_path_origem = strtok(NULL, " \t\n");
            char *arg_path_destino = strtok(NULL, " \t\n");
            comando_mv(fd, &sb, bgdt, diretorio_atual_inode, arg_path_origem, arg_path_destino);
        } else if (strcmp(primeiro_token, "cp") == 0) {
            char *arg_path_origem = strtok(NULL, " \t\n");
            char *arg_path_destino = strtok(NULL, " \t\n");
            if (arg_path_origem == NULL || arg_path_destino == NULL) {
                fprintf(stderr, "Uso: cp <origem> <destino>\n");
                continue;
            }
            comando_cp(fd, &sb, bgdt, diretorio_atual_inode, arg_path_origem, arg_path_destino);
        } else if (strcmp(primeiro_token, "quit") == 0 || strcmp(primeiro_token, "exit") == 0) {
            printf("Saindo.\n");
            break;
        } else {
            printf("Comando desconhecido: '%s'\n", primeiro_token);
        }
    }

    // Exemplo de leitura e impressão do inode raiz após o shell (para verificação).
    if (fd >=0 && bgdt != NULL) { 
        struct ext2_inode root_inode_data;
        printf("\nTentando ler o inode do diretório raiz (inode %u)...\n", EXT2_ROOT_INO);
        if (read_inode(fd, &sb, bgdt, EXT2_ROOT_INO, &root_inode_data) == 0) {
            printf("Inode Raiz (2) lido com sucesso!\n");
            printf("  i_mode: 0x%X (Tipo: %s, Perms: %o)\n", 
                   root_inode_data.i_mode, 
                   (S_ISDIR(root_inode_data.i_mode)) ? "Diretório" :
                   (S_ISREG(root_inode_data.i_mode)) ? "Arquivo Regular" :
                   (S_ISLNK(root_inode_data.i_mode)) ? "Link Simbólico" : "Outro",
                   root_inode_data.i_mode & 0xFFF); 
            printf("  i_size: %u bytes\n", root_inode_data.i_size);
            printf("  i_links_count: %u\n", root_inode_data.i_links_count);
            printf("  i_blocks (512B units): %u\n", root_inode_data.i_blocks);
        } else {
            fprintf(stderr, "Falha ao ler o inode do diretório raiz.\n");
        }
    }

    // Libera a memória alocada e fecha o file descriptor.
    if (bgdt) {
        free(bgdt); 
    }
    if (fd >= 0) {
        close(fd); 
    }

    return 0;
}
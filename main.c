#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h> // For uint32_t, uint16_t
#include <string.h> // Para strcmp, strrchr, etc.
#include <sys/stat.h> // Para S_ISDIR, S_ISREG, etc. (usado na impressão do i_mode)
#include <time.h>     // Para ctime() na formatação de datas

// Base Superblock Fields from Ext2 documentation
// All values are little-endian on disk.
struct ext2_super_block {
    uint32_t s_inodes_count;      // Total inode count
    uint32_t s_blocks_count;      // Total block count
    uint32_t s_r_blocks_count;    // Reserved block count
    uint32_t s_free_blocks_count; // Free block count
    uint32_t s_free_inodes_count; // Free inode count
    uint32_t s_first_data_block;  // First Data Block
    uint32_t s_log_block_size;    // Block size (log2 of block size minus 10)
    uint32_t s_log_frag_size;     // Fragment size (log2 of fragment size minus 10)
    uint32_t s_blocks_per_group;  // Blocks per group
    uint32_t s_frags_per_group;   // Fragments per group
    uint32_t s_inodes_per_group;  // Inodes per group
    uint32_t s_mtime;             // Mount time
    uint32_t s_wtime;             // Write time
    uint16_t s_mnt_count;         // Mount count
    uint16_t s_max_mnt_count;     // Maximal mount count
    uint16_t s_magic;             // Magic signature (0xEF53)
    uint16_t s_state;             // File system state
    uint16_t s_errors;            // Behaviour when detecting errors
    uint16_t s_minor_rev_level;   // Minor revision level
    uint32_t s_lastcheck;         // Time of last check
    uint32_t s_checkinterval;     // Max. time between checks
    uint32_t s_creator_os;        // Creator OS
    uint32_t s_rev_level;         // Revision level
    uint16_t s_def_resuid;        // Default uid for reserved blocks
    uint16_t s_def_resgid;        // Default gid for reserved blocks
    // Extended Superblock Fields (if s_rev_level >= 1)
    uint32_t s_first_ino;         // First non-reserved inode
    uint16_t s_inode_size;        // Size of inode structure
    uint16_t s_block_group_nr;    // Block group # of this superblock
    uint32_t s_feature_compat;    // Compatible feature set
    uint32_t s_feature_incompat;  // Incompatible feature set
    uint32_t s_feature_ro_compat; // Readonly-compatible feature set
    uint8_t  s_uuid[16];          // 128-bit uuid for volume
    char     s_volume_name[16];   // Volume name
    char     s_last_mounted[64];  // Directory where last mounted
    uint32_t s_algo_bitmap;       // For compression (usage varies)
    // Skipping other fields for brevity for now
};

// Estrutura do Descritor de Grupo de Blocos (Block Group Descriptor)
struct ext2_group_desc {
    uint32_t bg_block_bitmap;        // Bloco do bitmap de blocos
    uint32_t bg_inode_bitmap;        // Bloco do bitmap de inodes
    uint32_t bg_inode_table;         // Bloco do início da tabela de inodes
    uint16_t bg_free_blocks_count;   // Contagem de blocos livres no grupo
    uint16_t bg_free_inodes_count;   // Contagem de inodes livres no grupo
    uint16_t bg_used_dirs_count;     // Contagem de diretórios no grupo
    uint16_t bg_pad;                 // Alinhamento (não usado, para preenchimento)
    uint32_t bg_reserved[3];         // Reservado
};

// Estrutura do Inode (conforme OSDev Wiki e especificações Ext2)
struct ext2_inode {
    uint16_t i_mode;        // Tipo de arquivo e permissões
    uint16_t i_uid;         // ID do usuário (UID)
    uint32_t i_size;        // Tamanho do arquivo em bytes
    uint32_t i_atime;       // Tempo do último acesso (access time)
    uint32_t i_ctime;       // Tempo de criação (creation time)
    uint32_t i_mtime;       // Tempo da última modificação (modification time)
    uint32_t i_dtime;       // Tempo de deleção (deletion time)
    uint16_t i_gid;         // ID do grupo (GID)
    uint16_t i_links_count; // Contagem de links (hard links)
    uint32_t i_blocks;      // Número de blocos de 512 bytes (não blocos do FS!) alocados
    uint32_t i_flags;       // Flags do inode
    uint32_t i_osd1;        // OS dependent 1
    uint32_t i_block[15];   // Ponteiros para os blocos de dados (12 diretos, 1 indireto, 1 duplo indireto, 1 triplo indireto)
    uint32_t i_generation;  // Número de geração do arquivo (para NFS)
    uint32_t i_file_acl;    // ACL do arquivo (obsoleto ou usado para ACLs estendidas)
    uint32_t i_dir_acl;     // ACL do diretório (ou os 32 bits superiores do tamanho do arquivo se i_mode é regular file)
    uint32_t i_faddr;       // Endereço do fragmento (obsoleto)
    uint8_t  i_osd2[12];    // OS dependent 2
};

// Estrutura de uma Entrada de Diretório (com file_type)
// Veja https://wiki.osdev.org/Ext2#Directory_Entry
struct ext2_dir_entry_2 {
    uint32_t inode;         // Número do Inode (0 se a entrada não for usada)
    uint16_t rec_len;       // Comprimento total desta entrada (incluindo todos os campos e o nome)
    uint8_t  name_len;      // Comprimento do nome em bytes
    uint8_t  file_type;     // Tipo do arquivo (veja EXT2_FT_* abaixo)
    char name[255 + 1];     // Nome do arquivo (255 é EXT2_NAME_LEN, +1 para terminador null)
                            // O tamanho real em disco é name_len. Este buffer é para conveniência.
};

// Constantes para file_type em ext2_dir_entry_2 (do include/linux/ext2_fs.h)
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7
#define EXT2_FT_MAX      8

#define EXT2_NAME_LEN 255

#define EXT2_ROOT_INO 2 // O Inode do diretório raiz é sempre 2

#define SUPERBLOCK_OFFSET 1024
#define BLOCK_SIZE_FIXED 1024 // Conforme simplificação do projeto

// Constantes para s_rev_level e tamanho do inode (para clareza)
#define EXT2_GOOD_OLD_REV 0
#define EXT2_DYNAMIC_REV  1
#define EXT2_GOOD_OLD_INODE_SIZE 128

// Função para ler o superblock
// Retorna o file descriptor (fd) em sucesso, -1 em erro.
int read_superblock(const char *device_path, struct ext2_super_block *sb) {
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Erro ao abrir a imagem do disco");
        return -1;
    }

    if (lseek(fd, SUPERBLOCK_OFFSET, SEEK_SET) < 0) {
        perror("Erro ao posicionar para o superbloco");
        close(fd);
        return -1;
    }

    if (read(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) {
        perror("Erro ao ler o superbloco");
        close(fd);
        return -1;
    }

    // Não fechar o fd aqui, ele será usado por outras funções
    return fd; // Sucesso, retorna o file descriptor
}

// Função para ler a Tabela de Descritores de Grupo de Blocos (BGDT)
struct ext2_group_desc* read_block_group_descriptor_table(
    int fd,
    const struct ext2_super_block *sb,
    unsigned int *num_block_groups_out
) {
    unsigned int num_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    // Poderia haver uma verificação com base em s_inodes_count também, mas geralmente são consistentes.
    if (num_block_groups_out) {
        *num_block_groups_out = num_block_groups;
    }

    size_t bgdt_size = num_block_groups * sizeof(struct ext2_group_desc);
    struct ext2_group_desc *bgdt = (struct ext2_group_desc *)malloc(bgdt_size);
    if (!bgdt) {
        perror("Erro ao alocar memória para a BGDT");
        return NULL;
    }

    // A BGDT começa no bloco seguinte ao superbloco.
    // Superbloco está no bloco 1 (offset 1024, assumindo bloco 0 como boot block ou não usado para FS).
    // Se s_first_data_block > 0, o superbloco está no bloco s_first_data_block.
    // Com blocos de 1024 bytes, e superbloco no offset 1024 (Bloco 1), BGDT está no Bloco 2 (offset 2048).
    // A especificação OSDev diz "The Superblock is always located at byte 1024 from the beginning of the volume"
    // E "the group descriptor table occupies some integral number of blocks following the superblock."
    // No Ext2, se o tamanho do bloco for 1KB, o Superbloco está no bloco 1. Se for >1KB, está no bloco 0.
    // Como temos BLOCK_SIZE_FIXED = 1024, o Superbloco (1024 bytes) ocupa o Bloco 1 (offsets 1024 a 2047).
    // Portanto, a BGDT começa no Bloco 2 (offset 2048).
    off_t bgdt_offset = (sb->s_first_data_block + 1) * BLOCK_SIZE_FIXED;
    // Se s_log_block_size indica blocos > 1K, s_first_data_block é 0. Se blocos = 1K, s_first_data_block é 1.
    // Se block_size == 1024, s_log_block_size == 0. s_first_data_block deve ser 1.
    // Então bgdt_offset = (1+1) * 1024 = 2048.
    // Vamos usar uma abordagem mais simples baseada na documentação de que a BGDT segue o superbloco.
    // Se o superbloco está em 1024 e ocupa 1 bloco (já que o struct é <1024 e o bloco é 1024),
    // a BGDT começa em 1024 + BLOCK_SIZE_FIXED.
    bgdt_offset = SUPERBLOCK_OFFSET + BLOCK_SIZE_FIXED; 

    printf("Calculando BGDT: %u grupos, offset: %ld, tamanho total: %zu bytes\n", num_block_groups, (long)bgdt_offset, bgdt_size);

    if (lseek(fd, bgdt_offset, SEEK_SET) < 0) {
        perror("Erro ao posicionar para a BGDT");
        free(bgdt);
        return NULL;
    }

    if (read(fd, bgdt, bgdt_size) != bgdt_size) {
        perror("Erro ao ler a BGDT");
        free(bgdt);
        return NULL;
    }

    printf("BGDT lida com sucesso!\n");
    return bgdt;
}

// Função para ler um inode específico
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

    // Calcular a qual grupo de blocos o inode pertence.
    // Inodes são numerados a partir de 1.
    uint32_t block_group_index = (inode_num - 1) / sb->s_inodes_per_group;
    
    // Obter o descritor do grupo de blocos correspondente.
    // Adicionar verificação para garantir que block_group_index é válido (não excede o número de grupos)
    // unsigned int num_total_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    // if (block_group_index >= num_total_block_groups) { // Esta verificação pode ser feita antes se num_total_block_groups for passado
    //    fprintf(stderr, "Erro: Índice do grupo de blocos (%u) para o inode %u excede o total de grupos.\n", block_group_index, inode_num);
    //    return -1;
    // }
    const struct ext2_group_desc *group_descriptor = &bgdt[block_group_index];

    // Calcular o tamanho do inode.
    // Para rev_level 0 (EXT2_GOOD_OLD_REV), o tamanho do inode é 128.
    // Para rev_level >= 1 (EXT2_DYNAMIC_REV), s_inode_size no superbloco é usado.
    uint16_t inode_size = EXT2_GOOD_OLD_INODE_SIZE; // Define 128 como padrão
    if (sb->s_rev_level >= EXT2_DYNAMIC_REV) { // EXT2_DYNAMIC_REV é tipicamente 1
        if (sb->s_inode_size > 0 ) { // Garante que s_inode_size é válido
             inode_size = sb->s_inode_size;
        } // else mantém 128 se s_inode_size for 0, o que seria um erro no superbloco mas tratamos com fallback
    }
    // Para este projeto, dado que nossa struct ext2_inode tem 128 bytes e não há menção
    // a inodes de tamanho variável na simplificação, vamos ler sizeof(struct ext2_inode).
    // Se o inode_size do sistema de arquivos for maior, só leremos a parte inicial.
    // A lógica de cálculo de offset ainda usa o inode_size do superbloco para pular corretamente.

    // Calcular o índice do inode dentro da tabela de inodes do grupo.
    uint32_t index_in_group = (inode_num - 1) % sb->s_inodes_per_group;

    // Calcular o offset do inode no disco.
    // group_descriptor->bg_inode_table é o número do bloco onde a tabela de inodes começa.
    off_t inode_table_start_offset = (off_t)group_descriptor->bg_inode_table * BLOCK_SIZE_FIXED;
    off_t inode_offset_in_table = index_in_group * inode_size;
    off_t final_inode_offset = inode_table_start_offset + inode_offset_in_table;

    // printf("Debug read_inode(%u): Grupo %u, Tabela Inodes Bloco %u, Índice no Grupo %u, Tamanho Inode %u, Offset Final %ld\n", 
    //        inode_num, block_group_index, group_descriptor->bg_inode_table, index_in_group, inode_size, (long)final_inode_offset);

    if (lseek(fd, final_inode_offset, SEEK_SET) < 0) {
        char err_msg[200];
        snprintf(err_msg, sizeof(err_msg), "Erro ao posicionar para o inode %u (offset %ld)", inode_num, (long)final_inode_offset);
        perror(err_msg);
        return -1;
    }

    if (read(fd, inode_out, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        // Para este projeto, assumimos que sizeof(struct ext2_inode) é o tamanho real do inode no disco (128B).
        // Se sb->s_inode_size for maior que 128 e nossa struct for 128, read() lerá 128 bytes.
        // Isso deve estar OK para a maioria dos campos padrão.
        perror("Erro ao ler o inode");
        return -1;
    }

    return 0; // Sucesso
}

// Função auxiliar para ler um bloco de dados do disco
// Retorna 0 em sucesso, -1 em erro. O conteúdo é colocado em 'buffer'.
// O 'buffer' deve ter pelo menos BLOCK_SIZE_FIXED bytes.
int read_data_block(int fd, uint32_t block_num, char *buffer) {
    if (block_num == 0) {
        // Bloco 0 pode ser usado para "sparse files" (arquivos com buracos)
        // Para simplificar, tratamos como um bloco de zeros ou um erro se não for esperado.
        // fprintf(stderr, "Aviso: Tentativa de ler o bloco de dados 0.\n");
        // Preenche o buffer com zeros para representar um bloco não alocado (sparse).
        memset(buffer, 0, BLOCK_SIZE_FIXED);
        return 0; 
    }

    off_t offset = (off_t)block_num * BLOCK_SIZE_FIXED;
    // printf("Debug read_data_block: Lendo bloco %u do offset %ld\n", block_num, (long)offset);

    if (lseek(fd, offset, SEEK_SET) < 0) {
        char err_msg[100];
        snprintf(err_msg, sizeof(err_msg), "Erro ao posicionar para o bloco de dados %u (offset %ld)", block_num, (long)offset);
        perror(err_msg);
        return -1;
    }

    if (read(fd, buffer, BLOCK_SIZE_FIXED) != BLOCK_SIZE_FIXED) {
        perror("Erro ao ler o bloco de dados");
        return -1;
    }
    return 0; // Sucesso
}

// Função para o comando 'info'
void comando_info(struct ext2_super_block *sb) {
    printf("--- Informações do Superbloco ---\n");
    printf("Magic number: 0x%X (Esperado: 0xEF53)\n", sb->s_magic);
    if (sb->s_magic != 0xEF53) {
        printf("ERRO: Magic number não corresponde ao Ext2!\n");
        // Considerar não prosseguir se não for um sistema de arquivos Ext2 válido
    }
    printf("Total de inodes: %u\n", sb->s_inodes_count);
    printf("Total de blocos: %u\n", sb->s_blocks_count);
    printf("Blocos reservados: %u\n", sb->s_r_blocks_count);
    printf("Blocos livres: %u\n", sb->s_free_blocks_count);
    printf("Inodes livres: %u\n", sb->s_free_inodes_count);
    printf("Primeiro bloco de dados: %u\n", sb->s_first_data_block);
    
    // Conforme a simplificação do projeto, tamanho do bloco é fixo em 1024 bytes
    // s_log_block_size = log2(block_size / 1024). Se block_size = 1024, s_log_block_size = 0.
    // unsigned int block_size = 1024 << sb->s_log_block_size;
    unsigned int block_size = 1024; // Simplificação do projeto
    printf("Tamanho do bloco: %u bytes (definido como 1024 pela simplificação)\n", block_size);
    // printf("Tamanho do fragmento (log2): %u\n", sb->s_log_frag_size);
    printf("Blocos por grupo: %u\n", sb->s_blocks_per_group);
    // printf("Fragmentos por grupo: %u\n", sb->s_frags_per_group);
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
        // Outros campos estendidos podem ser adicionados aqui
    }
    printf("Nome do volume: %.16s\n", sb->s_volume_name);
    printf("Último local de montagem: %.64s\n", sb->s_last_mounted);
    
    // Calcular número de grupos de blocos
    unsigned int num_block_groups_blocks = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    unsigned int num_block_groups_inodes = (sb->s_inodes_count + sb->s_inodes_per_group - 1) / sb->s_inodes_per_group;
    printf("Número de grupos de blocos (baseado em blocos): %u\n", num_block_groups_blocks);
    printf("Número de grupos de blocos (baseado em inodes): %u\n", num_block_groups_inodes);
    // Idealmente, esses dois números devem ser iguais.
    printf("-----------------------------------\n");
}

// Função auxiliar para procurar uma entrada em um diretório e retornar seu inode.
// Retorna o número do inode se encontrado, 0 caso contrário.
// dir_inode_num: inode do diretório onde procurar.
// name_to_find: nome da entrada a ser procurada.
// found_file_type: (saída opcional) tipo do arquivo encontrado, se não for NULL.
static uint32_t dir_lookup(int fd, const struct ext2_super_block *sb,
                           const struct ext2_group_desc *bgdt,
                           uint32_t dir_inode_num, const char *name_to_find, 
                           uint8_t *found_file_type) {
    struct ext2_inode dir_inode;
    if (read_inode(fd, sb, bgdt, dir_inode_num, &dir_inode) != 0) {
        // fprintf(stderr, "dir_lookup: Erro ao ler inode %u\n", dir_inode_num);
        return 0; 
    }

    if (!S_ISDIR(dir_inode.i_mode)) {
        // fprintf(stderr, "dir_lookup: Inode %u não é um diretório.\n", dir_inode_num);
        return 0; 
    }

    if (dir_inode.i_block[0] == 0 && dir_inode.i_size > 0) {
        //fprintf(stderr, "dir_lookup: Diretório inode %u tem tamanho > 0 mas i_block[0] é 0.\n", dir_inode_num);
        // Isso pode indicar um diretório vazio que ainda tem i_size não zero de forma estranha, ou corrupção.
        // Um diretório verdadeiramente vazio (apenas com . e .. talvez) teria i_block[0] > 0.
        return 0; // Não há blocos de dados para ler
    }
    if (dir_inode.i_block[0] == 0) { // Diretório vazio ou sem blocos alocados
        return 0;
    }

    char data_block_buffer[BLOCK_SIZE_FIXED];
    // Simplificação do projeto: diretórios usam apenas 1 bloco de dados (i_block[0])
    if (read_data_block(fd, dir_inode.i_block[0], data_block_buffer) != 0) {
        // fprintf(stderr, "dir_lookup: Erro ao ler bloco de dados %u para o diretório inode %u\n", dir_inode.i_block[0], dir_inode_num);
        return 0;
    }

    unsigned int offset = 0;
    while (offset < dir_inode.i_size) { // i_size é o tamanho total dos dados do diretório
        // Garantir que não ultrapassamos o buffer do bloco que lemos
        if (offset >= BLOCK_SIZE_FIXED) {
            // fprintf(stderr, "dir_lookup: Offset (%u) excedeu BLOCK_SIZE_FIXED (%d) para dir inode %u. i_size: %u\n", offset, BLOCK_SIZE_FIXED, dir_inode_num, dir_inode.i_size);
            break; // Dados do diretório parecem exceder um bloco, contra a simplificação.
        }

        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(data_block_buffer + offset);

        if (entry->rec_len == 0) { // Prevenção contra loop infinito / corrupção
            // fprintf(stderr, "dir_lookup: rec_len é 0 no diretório inode %u, offset %u. Corrompido.\n", dir_inode_num, offset);
            break;
        }
        
        // Checar se a entrada está em uso (inode != 0) e se o nome bate
        if (entry->inode != 0 && entry->name_len == strlen(name_to_find)) {
            if (strncmp(entry->name, name_to_find, entry->name_len) == 0) {
                if (found_file_type != NULL) {
                    *found_file_type = entry->file_type;
                }
                return entry->inode; // Entrada encontrada!
            }
        }
        offset += entry->rec_len;
    }
    return 0; // Entrada não encontrada
}

// Função para resolver um caminho para um número de inode.
// Retorna o número do inode se o caminho for resolvido, 0 caso contrário.
// base_inode_num: inode do diretório base para caminhos relativos.
// path_str: string do caminho a ser resolvido.
// resolved_final_type: (saída opcional) tipo do arquivo/diretório final do caminho.
uint32_t path_to_inode_number(int fd, const struct ext2_super_block *sb,
                               const struct ext2_group_desc *bgdt,
                               uint32_t base_inode_num, const char *path_str,
                               uint8_t *resolved_final_type) {
    char mutable_path[1024]; // Para strtok
    strncpy(mutable_path, path_str, sizeof(mutable_path) - 1);
    mutable_path[sizeof(mutable_path) - 1] = '\0';

    uint32_t current_inode;
    char *path_tokenizer_state; // Para strtok_r se fosse multithread, mas strtok normal ok aqui

    if (resolved_final_type) *resolved_final_type = EXT2_FT_UNKNOWN; // Padrão

    if (strlen(mutable_path) == 0) { // Caminho vazio
        // Se relativo, é o base_inode_num. Se absoluto (não deveria ser vazio), é erro.
        // Para agora, vamos assumir que um path vazio relativo ao base_inode_num refere-se a ele mesmo.
        current_inode = base_inode_num;
        // Precisamos do tipo do base_inode_num se for o resultado final
        if (resolved_final_type) {
            struct ext2_inode temp_inode_info;
            if (read_inode(fd, sb, bgdt, current_inode, &temp_inode_info) == 0) {
                if (S_ISDIR(temp_inode_info.i_mode)) *resolved_final_type = EXT2_FT_DIR;
                else if (S_ISREG(temp_inode_info.i_mode)) *resolved_final_type = EXT2_FT_REG_FILE;
                // ... outros tipos ...
            }
        }
        return current_inode;
    }

    char *p_path_for_strtok;
    if (mutable_path[0] == '/') {
        current_inode = EXT2_ROOT_INO;
        p_path_for_strtok = mutable_path + 1;
        if (*p_path_for_strtok == '\0') { // Caminho é exatamente "/"
            if (resolved_final_type) *resolved_final_type = EXT2_FT_DIR;
            return EXT2_ROOT_INO;
        }
    } else {
        current_inode = base_inode_num;
        p_path_for_strtok = mutable_path;
    }

    char *token = strtok(p_path_for_strtok, "/");
    uint8_t last_token_type = EXT2_FT_UNKNOWN;

    while (token != NULL) {
        if (strlen(token) == 0) { // Token vazio (ex: //, ou /path/ -> token final vazio)
            token = strtok(NULL, "/"); // Pula para o próximo
            continue;
        }
        
        uint32_t next_inode_num;
        uint8_t current_token_file_type = EXT2_FT_UNKNOWN;

        if (strcmp(token, ".") == 0) {
            next_inode_num = current_inode; // "." refere-se ao diretório atual
            // Precisamos confirmar que current_inode é um diretório e obter seu tipo
            struct ext2_inode temp_inode_info;
            if (read_inode(fd, sb, bgdt, current_inode, &temp_inode_info) != 0 || !S_ISDIR(temp_inode_info.i_mode)) {
                return 0; // Erro ou não é diretório
            }
            current_token_file_type = EXT2_FT_DIR;
        } else if (strcmp(token, "..") == 0) {
            if (current_inode == EXT2_ROOT_INO) {
                next_inode_num = EXT2_ROOT_INO; // ".." na raiz é a raiz
                current_token_file_type = EXT2_FT_DIR;
            } else {
                // dir_lookup para ".." no current_inode
                next_inode_num = dir_lookup(fd, sb, bgdt, current_inode, "..", &current_token_file_type);
                if (next_inode_num == 0) return 0; // ".." não encontrado ou erro
            }
        } else {
            next_inode_num = dir_lookup(fd, sb, bgdt, current_inode, token, &current_token_file_type);
            if (next_inode_num == 0) {
                // fprintf(stderr, "path_to_inode: '%s' não encontrado em inode %u\n", token, current_inode);
                return 0; // Componente não encontrado
            }
        }
        
        current_inode = next_inode_num;
        last_token_type = current_token_file_type;

        char *peek_next_token = strtok(NULL, "/");
        token = peek_next_token;

        if (token != NULL) { // Se houver mais componentes no caminho
            // O current_inode (que era o next_inode_num) deve ser um diretório para continuar
            // Usar o file_type obtido do dir_entry é uma otimização.
            // Uma checagem mais robusta seria ler o inode e verificar S_ISDIR(inode.i_mode)
            if (last_token_type != EXT2_FT_DIR) {
                 struct ext2_inode temp_inode_check_dir;
                 if(read_inode(fd,sb,bgdt,current_inode, &temp_inode_check_dir)!=0 || !S_ISDIR(temp_inode_check_dir.i_mode)){
                    // fprintf(stderr, "path_to_inode: Componente intermediário não é diretório (inode %u, type %u)\n", current_inode, last_token_type);
                    return 0; // Componente intermediário não é diretório
                 }
            }
        }
    }

    if (resolved_final_type) {
        *resolved_final_type = last_token_type;
        // Para maior robustez do tipo final, especialmente se last_token_type for UNKNOWN
        // ou se a feature de filetype não estiver habilitada, ler o i_mode.
        if (*resolved_final_type == EXT2_FT_UNKNOWN && current_inode != 0) {
            struct ext2_inode final_inode_info;
            if (read_inode(fd, sb, bgdt, current_inode, &final_inode_info) == 0) {
                if (S_ISDIR(final_inode_info.i_mode)) *resolved_final_type = EXT2_FT_DIR;
                else if (S_ISREG(final_inode_info.i_mode)) *resolved_final_type = EXT2_FT_REG_FILE;
                else if (S_ISLNK(final_inode_info.i_mode)) *resolved_final_type = EXT2_FT_SYMLINK;
                // Adicionar outros se necessário
            }
        }
    }
    return current_inode;
}

// Função para o comando 'ls'
void comando_ls(int fd, const struct ext2_super_block *sb, 
                const struct ext2_group_desc *bgdt, 
                uint32_t diretorio_inode_num, const char* path_argumento) {
    
    uint32_t inode_a_listar = diretorio_inode_num; // Padrão: diretório atual do shell
    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;

    if (path_argumento != NULL && strlen(path_argumento) > 0) {
        // Se um caminho foi fornecido, tenta resolvê-lo
        inode_a_listar = path_to_inode_number(fd, sb, bgdt, diretorio_inode_num, path_argumento, &tipo_resolvido);
        if (inode_a_listar == 0) {
            printf("ls: não foi possível acessar '%s': Arquivo ou diretório não encontrado\n", path_argumento);
            return;
        }
        if (tipo_resolvido != EXT2_FT_DIR) {
             // Se o tipo não foi resolvido para diretório por path_to_inode_number, 
             // ou se a função não conseguiu determinar (deixou UNKNOWN), 
             // precisamos verificar o i_mode do inode.
            struct ext2_inode temp_check;
            if(read_inode(fd,sb,bgdt,inode_a_listar,&temp_check)==0 && S_ISREG(temp_check.i_mode)){
                // Se for um arquivo regular, apenas imprime o nome do arquivo.
                // O comportamento de `ls` em um arquivo é listar o próprio arquivo.
                // Para simplificar, podemos apenas imprimir o path_argumento.
                printf("%s\n", path_argumento);
                return;
            } else if (tipo_resolvido != EXT2_FT_DIR) { // Se ainda não é diretório
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

    if (!S_ISDIR(dir_inode_obj.i_mode)) {
        // Isto pode acontecer se ls for chamado no inode de um arquivo diretamente (sem path_argumento)
        // ou se path_to_inode_number resolveu para algo que não é diretório mas a flag tipo_resolvido falhou.
        printf("ls: inode %u não é um diretório.\n", inode_a_listar);
        // Se path_argumento foi dado, o nome já foi impresso acima. Senão, poderíamos tentar imprimir um nome para inode_a_listar?
        // Por agora, apenas erro.
        return;
    }

    if (dir_inode_obj.i_block[0] == 0) { // Diretório vazio ou sem blocos
        // printf("(Diretório vazio)\n");
        return; // Nenhum conteúdo para listar
    }

    char data_block_buffer[BLOCK_SIZE_FIXED];
    if (read_data_block(fd, dir_inode_obj.i_block[0], data_block_buffer) != 0) {
        printf("ls: erro ao ler bloco de dados do diretório (inode %u)\n", inode_a_listar);
        return;
    }

    printf("Conteúdo do diretório (inode %u):\n", inode_a_listar);
    unsigned int offset = 0;
    while (offset < dir_inode_obj.i_size) {
        if (offset >= BLOCK_SIZE_FIXED) break; 

        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(data_block_buffer + offset);
        if (entry->rec_len == 0) break;

        if (entry->inode != 0) { // Entrada em uso
            char name_buffer[EXT2_NAME_LEN + 1];
            strncpy(name_buffer, entry->name, entry->name_len);
            name_buffer[entry->name_len] = '\0';
            
            // Adicionar uma barra se for diretório (usando file_type da entrada)
            if (entry->file_type == EXT2_FT_DIR) {
                printf("%s/\n", name_buffer);
            } else {
                printf("%s\n", name_buffer);
            }
        }
        offset += entry->rec_len;
    }
}

// Função para ler todo o conteúdo de um arquivo, dado seu inode.
// Retorna um buffer alocado dinamicamente com o conteúdo do arquivo.
// O chamador DEVE liberar este buffer usando free().
// file_size_out: (saída) armazena o tamanho do arquivo lido.
// Retorna NULL em caso de erro ou se não for um arquivo regular.
char* read_file_data(int fd, const struct ext2_super_block *sb,
                     const struct ext2_group_desc *bgdt,
                     const struct ext2_inode *file_inode, uint32_t *file_size_out) {

    if (!S_ISREG(file_inode->i_mode)) {
        fprintf(stderr, "read_file_data: Inode não é um arquivo regular.\n");
        return NULL;
    }

    *file_size_out = file_inode->i_size;
    if (*file_size_out == 0) { // Arquivo vazio
        char *empty_buffer = (char*)malloc(1);
        if (empty_buffer) empty_buffer[0] = '\0';
        return empty_buffer; // Retorna buffer vazio (mas alocado)
    }

    char *file_content_buffer = (char *)malloc(*file_size_out + 1); // +1 para null terminator opcional
    if (!file_content_buffer) {
        perror("read_file_data: Erro ao alocar memória para conteúdo do arquivo");
        return NULL;
    }

    char block_read_buffer[BLOCK_SIZE_FIXED];
    uint32_t bytes_read = 0;
    unsigned int i;

    // 1. Blocos Diretos (i_block[0] a i_block[11])
    for (i = 0; i < 12 && bytes_read < *file_size_out; ++i) {
        if (file_inode->i_block[i] == 0) { // Bloco não alocado (sparse file) ou fim prematuro
             // Se for sparse, precisamos preencher com zeros até o próximo bloco ou fim do arquivo.
             // Para simplificar, assumimos que se o bloco é 0 aqui e ainda há bytes a ler,
             // o arquivo termina ou é um buraco. Para o cat, buracos são como zeros.
            uint32_t to_copy = (bytes_read + BLOCK_SIZE_FIXED > *file_size_out) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
            memset(file_content_buffer + bytes_read, 0, to_copy);
            bytes_read += to_copy;
            continue;
        }
        if (read_data_block(fd, file_inode->i_block[i], block_read_buffer) != 0) {
            fprintf(stderr, "read_file_data: Erro ao ler bloco direto %u (bloco real %u)\n", i, file_inode->i_block[i]);
            free(file_content_buffer);
            return NULL;
        }
        uint32_t to_copy = (*file_size_out - bytes_read < BLOCK_SIZE_FIXED) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
        memcpy(file_content_buffer + bytes_read, block_read_buffer, to_copy);
        bytes_read += to_copy;
    }

    // 2. Bloco de Indireção Simples (i_block[12])
    if (bytes_read < *file_size_out && file_inode->i_block[12] != 0) {
        char indirect_block_pointers_buffer[BLOCK_SIZE_FIXED];
        if (read_data_block(fd, file_inode->i_block[12], indirect_block_pointers_buffer) != 0) {
            fprintf(stderr, "read_file_data: Erro ao ler bloco de indireção simples (bloco %u)\n", file_inode->i_block[12]);
            free(file_content_buffer);
            return NULL;
        }
        uint32_t *indirect_pointers = (uint32_t *)indirect_block_pointers_buffer;
        unsigned int num_pointers_in_block = BLOCK_SIZE_FIXED / sizeof(uint32_t);

        for (i = 0; i < num_pointers_in_block && bytes_read < *file_size_out; ++i) {
            if (indirect_pointers[i] == 0) { // Bloco não alocado (sparse) ou fim
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

    // 3. Bloco de Dupla Indireção (i_block[13])
    if (bytes_read < *file_size_out && file_inode->i_block[13] != 0) {
        char double_indirect_block_pointers_buffer[BLOCK_SIZE_FIXED];
        if (read_data_block(fd, file_inode->i_block[13], double_indirect_block_pointers_buffer) != 0) {
            fprintf(stderr, "read_file_data: Erro ao ler bloco de dupla indireção (bloco %u)\n", file_inode->i_block[13]);
            free(file_content_buffer);
            return NULL;
        }
        uint32_t *double_indirect_pointers = (uint32_t *)double_indirect_block_pointers_buffer;
        unsigned int num_pointers_in_block = BLOCK_SIZE_FIXED / sizeof(uint32_t);

        for (i = 0; i < num_pointers_in_block && bytes_read < *file_size_out; ++i) {
            if (double_indirect_pointers[i] == 0) continue; // Nível de indireção não usado

            char indirect_block_pointers_buffer[BLOCK_SIZE_FIXED]; // Reutiliza nome, mas é um novo buffer
            if (read_data_block(fd, double_indirect_pointers[i], indirect_block_pointers_buffer) != 0) {
                fprintf(stderr, "read_file_data: Erro ao ler bloco de indireção simples (nível 2, bloco %u) da dupla indireção\n", double_indirect_pointers[i]);
                free(file_content_buffer);
                return NULL;
            }
            uint32_t *indirect_pointers = (uint32_t *)indirect_block_pointers_buffer;
            // num_pointers_in_block é o mesmo

            unsigned int j;
            for (j = 0; j < num_pointers_in_block && bytes_read < *file_size_out; ++j) {
                 if (indirect_pointers[j] == 0) { // Bloco não alocado (sparse) ou fim
                    uint32_t to_copy = (bytes_read + BLOCK_SIZE_FIXED > *file_size_out) ? (*file_size_out - bytes_read) : BLOCK_SIZE_FIXED;
                    memset(file_content_buffer + bytes_read, 0, to_copy);
                    bytes_read += to_copy;
                    continue;
                }
                if (read_data_block(fd, indirect_pointers[j], block_read_buffer) != 0) {
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

    // 4. Bloco de Tripla Indireção (i_block[14]) - Não necessário pela simplificação (<= 64MiB)
    // Se fosse necessário, a lógica seria similar, adicionando mais um nível de loop.

    file_content_buffer[*file_size_out] = '\0'; // Adiciona terminador null para facilitar impressão como string
    return file_content_buffer;
}

// Função para o comando 'cat'
void comando_cat(int fd, const struct ext2_super_block *sb, 
                 const struct ext2_group_desc *bgdt, 
                 uint32_t diretorio_atual_inode_num, const char* path_arquivo) {

    if (path_arquivo == NULL || strlen(path_arquivo) == 0) {
        printf("cat: Caminho do arquivo não especificado.\n");
        return;
    }

    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;
    uint32_t arquivo_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, path_arquivo, &tipo_resolvido);

    if (arquivo_inode_num == 0) {
        printf("cat: '%s': Arquivo ou diretório não encontrado\n", path_arquivo);
        return;
    }

    struct ext2_inode arquivo_inode_obj;
    if (read_inode(fd, sb, bgdt, arquivo_inode_num, &arquivo_inode_obj) != 0) {
        printf("cat: Erro ao ler inode %u para o arquivo '%s'\n", arquivo_inode_num, path_arquivo);
        return;
    }

    if (!S_ISREG(arquivo_inode_obj.i_mode)) {
        // Poderia verificar tipo_resolvido também, mas i_mode é mais definitivo.
        if (S_ISDIR(arquivo_inode_obj.i_mode)) {
             printf("cat: '%s': É um diretório\n", path_arquivo);
        } else {
             printf("cat: '%s': Não é um arquivo regular\n", path_arquivo);
        }
        return;
    }

    uint32_t tamanho_do_arquivo;
    char *conteudo_arquivo = read_file_data(fd, sb, bgdt, &arquivo_inode_obj, &tamanho_do_arquivo);

    if (conteudo_arquivo == NULL) {
        // read_file_data já deve ter impresso uma mensagem de erro mais específica
        printf("cat: Falha ao ler o conteúdo de '%s'\n", path_arquivo);
        return;
    }

    if (tamanho_do_arquivo > 0) {
        // Imprime o conteúdo. fwrite é mais seguro para dados binários,
        // mas para 'cat' em modo texto, assumimos que o conteúdo é imprimível.
        // Se o arquivo contiver null bytes no meio, printf("%s") pararia.
        // fwrite lida com isso corretamente.
        fwrite(conteudo_arquivo, 1, tamanho_do_arquivo, stdout);
    }
    // Adicionar uma nova linha no final se o arquivo não terminar com uma, comportamento comum do cat.
    // if (tamanho_do_arquivo > 0 && conteudo_arquivo[tamanho_do_arquivo -1] != '\n') {
    //     printf("\n");
    // }

    free(conteudo_arquivo);
}

// Função para o comando 'attr'
void comando_attr(int fd, const struct ext2_super_block *sb, 
                  const struct ext2_group_desc *bgdt, 
                  uint32_t diretorio_atual_inode_num, const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) {
        printf("attr: Caminho do arquivo ou diretório não especificado.\n");
        return;
    }

    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;
    uint32_t alvo_inode_num = path_to_inode_number(fd, sb, bgdt, diretorio_atual_inode_num, path_alvo, &tipo_resolvido);

    if (alvo_inode_num == 0) {
        printf("attr: '%s': Arquivo ou diretório não encontrado\n", path_alvo);
        return;
    }

    struct ext2_inode alvo_inode_obj;
    if (read_inode(fd, sb, bgdt, alvo_inode_num, &alvo_inode_obj) != 0) {
        printf("attr: Erro ao ler inode %u para '%s'\n", alvo_inode_num, path_alvo);
        return;
    }

    printf("Atributos para '%s' (Inode: %u):\n", path_alvo, alvo_inode_num);

    // Tipo de Arquivo
    char tipo_str[50] = "Desconhecido";
    if (S_ISREG(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Arquivo Regular");
    else if (S_ISDIR(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Diretório");
    else if (S_ISLNK(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Link Simbólico");
    else if (S_ISCHR(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Dispositivo de Caractere");
    else if (S_ISBLK(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Dispositivo de Bloco");
    else if (S_ISFIFO(alvo_inode_obj.i_mode)) strcpy(tipo_str, "FIFO/Pipe");
    else if (S_ISSOCK(alvo_inode_obj.i_mode)) strcpy(tipo_str, "Socket");
    printf("  Tipo:          %s (0x%X)\n", tipo_str, alvo_inode_obj.i_mode & 0xF000);

    // Permissões
    printf("  Modo (perms):  %o (octal)\n", alvo_inode_obj.i_mode & 0xFFF);
    // Detalhar permissões (rwx)
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
           (alvo_inode_obj.i_mode & S_IRUSR) ? 'r' : '-', (alvo_inode_obj.i_mode & S_IWUSR) ? 'w' : '-', (alvo_inode_obj.i_mode & S_IXUSR) ? 'x' : '-', /* User */
           (alvo_inode_obj.i_mode & S_IRGRP) ? 'r' : '-', (alvo_inode_obj.i_mode & S_IWGRP) ? 'w' : '-', (alvo_inode_obj.i_mode & S_IXGRP) ? 'x' : '-', /* Group */
           (alvo_inode_obj.i_mode & S_IROTH) ? 'r' : '-', (alvo_inode_obj.i_mode & S_IWOTH) ? 'w' : '-', (alvo_inode_obj.i_mode & S_IXOTH) ? 'x' : '-'  /* Other */
          );
    if (alvo_inode_obj.i_mode & S_ISUID) printf("                 (setuid bit set)\n");
    if (alvo_inode_obj.i_mode & S_ISGID) printf("                 (setgid bit set)\n");
    // if (alvo_inode_obj.i_mode & S_ISVTX) printf("                 (sticky bit set)\n"); // S_ISVTX pode não estar em todos os <sys/stat.h>

    printf("  UID:           %u\n", alvo_inode_obj.i_uid);
    printf("  GID:           %u\n", alvo_inode_obj.i_gid);
    printf("  Tamanho:       %u bytes\n", alvo_inode_obj.i_size);
    printf("  Links:         %u\n", alvo_inode_obj.i_links_count);
    printf("  Blocos (FS):   %u (calculado: %u)\n", alvo_inode_obj.i_blocks / (BLOCK_SIZE_FIXED/512), alvo_inode_obj.i_blocks ); // i_blocks é em unidades de 512B
    
    // Timestamps
    // time_t é geralmente um long. Precisamos converter.
    time_t atime_val = alvo_inode_obj.i_atime;
    time_t ctime_val = alvo_inode_obj.i_ctime;
    time_t mtime_val = alvo_inode_obj.i_mtime;
    // ctime() adiciona uma nova linha no final, então removemos se existir.
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
    // Poderíamos decodificar os flags aqui (EXT2_IMMUTABLE_FL, EXT2_APPEND_FL, etc.)

    printf("  Ponteiros de Bloco (i_block):\n");
    for (int k=0; k<15; ++k) {
        printf("    i_block[%2d]: %u (0x%X)\n", k, alvo_inode_obj.i_block[k], alvo_inode_obj.i_block[k]);
    }
}

// Função para o comando 'pwd' (print working directory)
void comando_pwd(const char* diretorio_atual_str) {
    printf("%s\n", diretorio_atual_str);
}

// Função para normalizar um caminho (remove barras duplicadas, trata . e .. simples)
// Retorna uma string alocada que deve ser liberada pelo chamador.
// Esta é uma implementação simplificada.
char* normalizar_path_string(const char* base, const char* append) {
    char temp_path[2048]; // Buffer grande o suficiente
    
    if (append && append[0] == '/') { // Se o que estamos anexando é um caminho absoluto
        strncpy(temp_path, append, sizeof(temp_path) -1 );
    } else { // Caminho relativo ou base apenas
        strncpy(temp_path, base, sizeof(temp_path) - 1);
        if (append && strlen(append) > 0) {
            if (temp_path[strlen(temp_path) - 1] != '/') {
                strncat(temp_path, "/", sizeof(temp_path) - strlen(temp_path) - 1);
            }
            strncat(temp_path, append, sizeof(temp_path) - strlen(temp_path) - 1);
        }
    }
    temp_path[sizeof(temp_path) -1] = '\0';

    // Lógica de normalização mais robusta seria necessária para tratar todos os casos de . e ..
    // Por exemplo, "/foo/bar/../baz" -> "/foo/baz"
    // A implementação atual é mais uma "canonização" simples (ex: remove barras extras no final).
    // Se o caminho terminar com /, remove, a menos que seja apenas "/"
    size_t len = strlen(temp_path);
    while (len > 1 && temp_path[len-1] == '/') {
        temp_path[len-1] = '\0';
        len--;
    }
    if (strlen(temp_path) == 0 && ( (append && append[0] == '/') || (base && base[0] == '/' && (!append || strlen(append)==0))  ) ) {
        // Se o resultado for vazio mas deveria ser a raiz (ex: cd /)
        strcpy(temp_path, "/");
    }

    return strdup(temp_path);
}

// Função para o comando 'cd' (change directory)
void comando_cd(int fd, const struct ext2_super_block *sb, 
                const struct ext2_group_desc *bgdt, 
                uint32_t *diretorio_atual_inode_num_ptr, // Ponteiro para atualizar
                char* diretorio_atual_str, // Buffer para atualizar (ex: de main)
                size_t diretorio_atual_str_max_len,
                const char* path_alvo) {

    if (path_alvo == NULL || strlen(path_alvo) == 0) {
        // `cd` sem argumentos geralmente vai para o HOME, mas não temos esse conceito.
        // Poderia ir para a raiz ou não fazer nada. Vamos para a raiz.
        *diretorio_atual_inode_num_ptr = EXT2_ROOT_INO;
        strncpy(diretorio_atual_str, "/", diretorio_atual_str_max_len -1);
        diretorio_atual_str[diretorio_atual_str_max_len -1] = '\0';
        return;
    }

    uint8_t tipo_resolvido = EXT2_FT_UNKNOWN;
    uint32_t novo_inode_num = path_to_inode_number(fd, sb, bgdt, *diretorio_atual_inode_num_ptr, path_alvo, &tipo_resolvido);

    if (novo_inode_num == 0) {
        printf("cd: '%s': Arquivo ou diretório não encontrado\n", path_alvo);
        return;
    }

    // Checagem mais robusta do tipo, lendo o inode
    struct ext2_inode novo_inode_obj;
    if (read_inode(fd, sb, bgdt, novo_inode_num, &novo_inode_obj) != 0) {
        printf("cd: Erro ao ler inode %u para '%s'\n", novo_inode_num, path_alvo);
        return;
    }

    if (!S_ISDIR(novo_inode_obj.i_mode)) {
        printf("cd: '%s': Não é um diretório\n", path_alvo);
        return;
    }

    // Se chegou aqui, é um diretório válido. Atualiza o inode atual.
    *diretorio_atual_inode_num_ptr = novo_inode_num;

    // Atualizar a string do diretório atual (diretorio_atual_str)
    char* path_normalizado;
    if (path_alvo[0] == '/') { // Caminho absoluto
        path_normalizado = normalizar_path_string(path_alvo, NULL);
    } else { // Caminho relativo
        path_normalizado = normalizar_path_string(diretorio_atual_str, path_alvo);
    }
    
    // A `path_to_inode_number` já resolve . e .., então o `novo_inode_num` está correto.
    // A string `path_normalizado` é uma tentativa de manter o prompt visualmente correto.
    // Uma forma mais robusta seria reconstruir o caminho a partir da raiz até `novo_inode_num`,
    // mas isso é complexo. `normalizar_path_string` tenta limpar um pouco.
    
    // Se o novo inode é a raiz, garantir que a string é "/"
    if (*diretorio_atual_inode_num_ptr == EXT2_ROOT_INO) {
        strncpy(diretorio_atual_str, "/", diretorio_atual_str_max_len -1);
    } else {
        strncpy(diretorio_atual_str, path_normalizado, diretorio_atual_str_max_len - 1);
    }
    diretorio_atual_str[diretorio_atual_str_max_len - 1] = '\0';
    free(path_normalizado);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <imagem_ext2>\n", argv[0]);
        return 1;
    }

    const char *disk_image_path = argv[1];
    struct ext2_super_block sb;
    struct ext2_group_desc *bgdt = NULL; // Ponteiro para a tabela de descritores de grupo
    unsigned int num_block_groups = 0;
    int fd = -1; // File descriptor da imagem

    printf("Tentando ler o superbloco de: %s\n", disk_image_path);
    fd = read_superblock(disk_image_path, &sb);

    if (fd < 0) {
        // read_superblock já imprimiu o erro
        return 1;
    }
    printf("Superbloco lido com sucesso!\n\n");

    if (sb.s_magic != 0xEF53) {
        fprintf(stderr, "Erro: A imagem fornecida não parece ser um sistema de arquivos Ext2 (magic number incorreto).\n");
        close(fd);
        return 1;
    }

    // Ler a Tabela de Descritores de Grupo de Blocos
    bgdt = read_block_group_descriptor_table(fd, &sb, &num_block_groups);
    if (!bgdt) {
        fprintf(stderr, "Falha ao ler a Tabela de Descritores de Grupo de Blocos.\n");
        close(fd);
        return 1;
    }
    // Se chegou aqui, bgdt foi lido e a memória alocada.
    // num_block_groups também foi populado.
    // Por enquanto, apenas imprimimos que foi lido.
    // No futuro, o comando 'info' poderia mostrar alguns detalhes da BGDT.

    char comando[100];
    char prompt[200];
    // Exemplo de prompt: ext2shell:[myext2image/] $
    // Extrair o nome da imagem para o prompt
    const char *image_name_for_prompt = strrchr(disk_image_path, '/');
    if (image_name_for_prompt == NULL) {
        image_name_for_prompt = disk_image_path;
    } else {
        image_name_for_prompt++; // Pular a barra '/'
    }

    // Diretório atual inicializado como raiz "/"
    char diretorio_atual[1024] = "/"; 

    // Definir o inode do diretório atual do shell
    uint32_t diretorio_atual_inode = EXT2_ROOT_INO;
    // A string diretorio_atual já está sendo usada para o prompt.
    // Precisamos mantê-la sincronizada com diretorio_atual_inode (faremos com o cd).

    while(1) {
        // Monta o prompt: ext2shell:[nome_imagem:diretorio_atual_string] $
        snprintf(prompt, sizeof(prompt), "ext2shell:[%s:%s] $ ", image_name_for_prompt, diretorio_atual);
        printf("%s", prompt);
        
        if (fgets(comando, sizeof(comando), stdin) == NULL) {
            printf("\nSaindo.\n"); // EOF (Ctrl+D)
            break;
        }
        
        char *primeiro_token = strtok(comando, " \t\n"); // Pega o comando principal
        if (primeiro_token == NULL) { // Linha vazia
            continue;
        }

        if (strcmp(primeiro_token, "info") == 0) {
            comando_info(&sb);
        } else if (strcmp(primeiro_token, "ls") == 0) {
            char *arg_path = strtok(NULL, " \t\n"); // Pega o argumento opcional para ls
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
        } else if (strcmp(primeiro_token, "quit") == 0 || strcmp(primeiro_token, "exit") == 0) {
            printf("Saindo.\n");
            break;
        } else {
            printf("Comando desconhecido: '%s'\n", primeiro_token);
        }
    }

    // Teste: Ler e imprimir informações do inode raiz (inode 2) após BGDT
    if (fd >=0 && bgdt != NULL) { // Garante que fd e bgdt são válidos
        struct ext2_inode root_inode_data;
        printf("\nTentando ler o inode do diretório raiz (inode %u)...\n", EXT2_ROOT_INO);
        if (read_inode(fd, &sb, bgdt, EXT2_ROOT_INO, &root_inode_data) == 0) {
            printf("Inode Raiz (2) lido com sucesso!\n");
            printf("  i_mode: 0x%X (Tipo: %s, Perms: %o)\n", 
                   root_inode_data.i_mode, 
                   (S_ISDIR(root_inode_data.i_mode)) ? "Diretório" :
                   (S_ISREG(root_inode_data.i_mode)) ? "Arquivo Regular" :
                   (S_ISLNK(root_inode_data.i_mode)) ? "Link Simbólico" : "Outro",
                   root_inode_data.i_mode & 0xFFF); // Primeiros 12 bits são permissões
            printf("  i_size: %u bytes\n", root_inode_data.i_size);
            printf("  i_links_count: %u\n", root_inode_data.i_links_count);
            printf("  i_blocks (512B units): %u\n", root_inode_data.i_blocks);
            // Poderia adicionar mais campos do inode aqui
        } else {
            fprintf(stderr, "Falha ao ler o inode do diretório raiz.\n");
        }
    }

    // Limpeza antes de sair
    if (bgdt) {
        free(bgdt); // Libera a memória alocada para a BGDT
    }
    if (fd >= 0) {
        close(fd); // Fecha o arquivo da imagem
    }

    return 0;
} 
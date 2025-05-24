#include <iostream>
#include <fstream>
#include <cstdint>
#include <iomanip>

#define OFFSET 1024
#define BLOCKSIZE 1024
#define EXT2_SUPER_MAGIC 0xEF53

#pragma pack(push, 1)
struct Superblock{
    uint32_t s_inodes_count; // Quantidade atual de INODES (em uso e livres) no FS.
    uint32_t s_blocks_count; // Quantidade atual de BLOCKS no FS
    uint32_t s_r_blocks_count; // Quantidade de BLOCKS reservado para root
    uint32_t s_free_blocks_count; // Quantidade total de BLOCKS livres (incluindo os root)
    uint32_t s_free_inodes_count; // Quantidade total de INODES livres 
    uint32_t s_first_data_block; // Em qual BLOCK o superbloco se encontra
    uint32_t s_log_block_size; // Usado para permitir tamanho diferentes de blocos. Nesse caso, sempre será 0, vide especificação
    uint32_t s_log_frag_size; // Tamanho do fragmento
    uint32_t s_blocks_per_group; // Quantidade total de BLOCKS por grupo
    uint32_t s_frags_per_group; // Quantidade máxima de fragmentos por grupo
    uint32_t s_inodes_per_group; // Quantidade total de INODES por grupo
    uint32_t s_mtime; // Ultima vez que o sistema foi montado
    uint32_t s_wtime; // Ultima vez que o sistema foi acessado para escrita 
    uint16_t s_mnt_count; // Quantidade de vezes que o sistema foi montado desde a ultima vez que foi verificado
    uint16_t s_max_mnt_count; // Quantidade maxima de vezes que o sistema pode ser montado antes de ser totalmente verificado
    uint16_t s_magic; // Numero magico que define o sistema de arquivos como Ext2
    

} typedef superblock;


#pragma pack(pop)



int main(){

}
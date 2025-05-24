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

struct BlockGroupDescriptor{
    uint32_t bg_block_bitmap; // Id do bloco do primeiro bloco do bloco de bitmaps
    uint32_t bg_inode_bitmap; // Id do bloco do primeiro bloco do INODE de bitmap
    uint32_t bg_inode_table; // Id do bloco do primeiro bloco da tabela de INODE
    uint16_t bg_free_blocks_count; // Total de blocos livres
    uint16_t bg_free_inodes_count; // Total de INODES livres
    uint16_t bg_used_dirs_count; // Total de INODES alocadas para diretorios 
    uint16_t bg_pad; // Padding
    uint32_t bg_reserved[3]; // Reservado para o futuro
}typedef bgd;

struct Inode {
    uint16_t i_mode; // Tipo e permissopes
    uint16_t i_uid; // Id do Dono
    uint32_t i_size; // Tamanho em Bytes
    uint32_t i_atime; // Tempo do acesso
    uint32_t i_ctime; // Tempo de criação
    uint32_t i_mtime; // Tempo de modificação
    uint32_t i_dtime; // tempo de deleção
    uint16_t i_gid; // id do grupo
    uint16_t i_links_count; // Quantos links esse inode possui
    uint32_t i_blocks; // Quantidade de blocos alocados para esse inode
    uint32_t i_block[15]; // Ponteiros para os blocos alocados
} typedef inode;

#pragma pack(pop)



int main(){

}
# EXT2
Implementação do sistema de arquivos EXT2 para a matéria de Sistemas Operacionais - BCC5002 - Ministrada pelo Professor Dr. Rodrigo Campiolo.

## Objetivo Central
O objetivo do projeto consistiu na implementação de um shell e estruturas de dados para manipulação de uma imagem de um sistema de arquivos EXT2 em C.

## Implementação
Dentro do arquivo `main.c` está a implementação do shell, junto de suas funções auxiliares e estruturas de dados. Além disso, forma implementados os comandos:

### 📖 Comandos de Leitura
- `info`: Exibe informações do superbloco (blocos, inodes, espaço livre, tamanho de blocos/inodes etc).
- `ls [<path>]`: Lista arquivos e diretórios no caminho especificado.
- `cat <file>`: Exibe o conteúdo de um arquivo.
- `attr <file | dir>`: Mostra atributos do arquivo/diretório (permissões, UID, GID, timestamps etc).
- `cd <path>`: Altera o diretório de trabalho.
- `pwd`: Exibe o caminho absoluto do diretório atual.

### ✏️ Comandos de Escrita e Modificação
- `touch <file>`: Cria um novo arquivo vazio ou atualiza timestamps.
- `mkdir <dir>`: Cria um novo diretório.
- `rm <file>`: Remove um arquivo regular.
- `rmdir <dir>`: Remove um diretório vazio.
- `rename <file> <newfilename>`: Renomeia um arquivo ou diretório.
- `mv <source> <target>`: Move ou renomeia arquivos (dentro do mesmo diretório ou para um diretório existente).
- `cp <source> <host-path>`: Copia um arquivo da imagem para o sistema de arquivos do host.

### 💻 Métodos Auxiliares e Estruturas de Dados

#### Structs
- `ext2_super_block`: Super bloco do EXT2, estrutura que contém inúmeras informações sobre o sistema de arquivos e seu estado atual
- `ext2_group_desc`: Descritor de um grupo de bloco
- `ext2_inode`: Inode do EXT2, representa um arquivo ou diretório, contendo metadados de permissões, dono, etc...
- `ext2_dir_entry_2`: Entrada de diretório do EXT2. Cada diretório contém várias estruturas dessas, com cada uma apontando para um arquivo ou diretório.

#### Métodos Auxiliares para lidar com leitura e escrita
- `read_superblock()`, `write_superblock()`
- `read_inode()`, `write_inode_table_entry()`
- `read_data_block()`, `write_data_block()`
- `allocate_inode()`, `deallocate_inode()`
- `allocate_data_block()`, `deallocate_data_block()`


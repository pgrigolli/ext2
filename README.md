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


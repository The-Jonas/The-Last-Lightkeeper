import os

def extrair_codigo(pastas_alvo, arquivo_headers, arquivo_sources, ignorar_arquivos):
    extensoes_header = ('.h', '.hpp')
    extensoes_source = ('.c', '.cpp')
    
    with open(arquivo_headers, 'w', encoding='utf-8') as f_headers, \
         open(arquivo_sources, 'w', encoding='utf-8') as f_sources:
        
        for pasta in pastas_alvo:
            for raiz, diretorios, arquivos in os.walk(pasta):
                for arquivo in arquivos:
                    if arquivo in ignorar_arquivos:
                        continue
                    
                    caminho_completo = os.path.join(raiz, arquivo)
                    caminho_relativo = os.path.relpath(caminho_completo).replace('\\', '/')
                    
                    cabecalho = f"{caminho_relativo}\n{'-' * 40}\n"
                    
                    try:
                        with open(caminho_completo, 'r', encoding='utf-8') as entrada:
                            conteudo = entrada.read()
                    except Exception as e:
                        conteudo = f"// Erro ao ler este arquivo: {e}"
                        
                    texto_final = f"{cabecalho}{conteudo}\n\n\n"
                    
                    # Verifica a extensão e joga no txt correspondente
                    if arquivo.endswith(extensoes_header):
                        f_headers.write(texto_final)
                    elif arquivo.endswith(extensoes_source):
                        f_sources.write(texto_final)
    
    print(f"Pronto! Arquivos gerados com sucesso:")
    print(f" -> {arquivo_headers}")
    print(f" -> {arquivo_sources}")

pastas = ['./include', './src']

arquivos_para_ignorar = [
    'pl_mpeg.h', 
]

extrair_codigo(
    pastas_alvo=pastas, 
    arquivo_headers='codigo_headers.txt', 
    arquivo_sources='codigo_sources.txt', 
    ignorar_arquivos=arquivos_para_ignorar
)
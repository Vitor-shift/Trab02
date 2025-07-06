import openpyxl
import csv

def process_ceps(input_file, output_file):
    rows_data = []
    is_xlsx = input_file.lower().endswith('.xlsx')

    if is_xlsx:
        try:
            workbook = openpyxl.load_workbook(input_file)
            sheet = workbook.active
            # Leitura a partir da segunda linha (ignorando o cabeçalho)
            for row in sheet.iter_rows(min_row=2):
                # Para XLSX, as células são objetos com o atributo .value
                rows_data.append([cell.value for cell in row])
        except Exception as e:
            print(f"Erro ao ler arquivo XLSX '{input_file}': {e}")
            return
    elif input_file.lower().endswith('.csv'):
        try:
            with open(input_file, 'r', encoding='utf-8') as f:
                reader = csv.reader(f)
                next(reader, None) # Pula o cabeçalho
                for row in reader:
                    # Para CSV, os elementos da linha já são strings
                    rows_data.append(row)
        except Exception as e:
            print(f"Erro ao ler arquivo CSV '{input_file}': {e}")
            return
    else:
        print(f"Erro: Formato de arquivo nao suportado para '{input_file}'. Use .xlsx ou .csv.")
        return

    if not rows_data:
        print(f"Nenhum dado lido de '{input_file}'. Verifique o arquivo e o cabeçalho.")
        return

    with open(output_file, 'w', encoding='utf-8') as f:
        for row in rows_data:
            # Assumindo a ordem original: Estado, Cidade, Faixa_de_CEP
            if len(row) >= 3:
                # Os valores já foram extraídos como str ou None na etapa anterior
                estado_original = str(row[0]).strip() if row[0] is not None else ''
                cidade_original = str(row[1]).strip() if row[1] is not None else ''
                faixa_cep_original = str(row[2]).strip() if row[2] is not None else ''

                # Extrai o primeiro CEP da faixa (e.g., "77880-000" de "77880-000 a 77884-999")
                # Garante que faixa_cep_original não esteja vazia antes de split
                primeiro_cep_str = faixa_cep_original.split(' ')[0] if faixa_cep_original else ''
                # Remove o hífen para obter um CEP de 8 dígitos
                cep_formatado = primeiro_cep_str.replace('-', '')

                # Escreve no formato: CEP,Cidade,Estado
                # Verifica se o CEP formatado não está vazio antes de escrever
                if cep_formatado:
                    f.write(f'{cep_formatado},{cidade_original},{estado_original}\n')
                else:
                    print(f"DEBUG: Linha ignorada - CEP nao pode ser formatado: {row}")
            else:
                print(f"DEBUG: Linha ignorada por nao ter colunas suficientes: {row}")


if __name__ == '__main__':
    # Use o nome do arquivo original
    # nome é "Lista_de_CEPs.xlsx"
    input_source_file = 'Lista_de_CEPs.xlsx'

    process_ceps(input_source_file, 'ceps_data.csv')
    print(f"Arquivo 'ceps_data.csv' gerado com sucesso a partir de '{input_source_file}'.")
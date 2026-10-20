# Verificacao local — 2026-10-20

Framework fixado: `5a2eddeecc646e9325bef53f3c711e119423f99c`.

ROM fornecida: SHA256 `3d9850e4e08aaf4c0915109e2021e9590d4b854b14122169021896f69af1d145`.
Payload SHA256: `a6b81fec11c24a33fd763db5c28005e760a1614e70c1bb5ccde0bd4242431000`.
O arquivo fornecido se chama USA; identidade regional independente ainda nao catalogada.

- Build Clang 22.1.8 / LLVM-MinGW x64 concluida com SDL2 do framework.
- Antes do perfil: 1200 frames, 11 ciclos nativos de 35.734.220.
- Depois do perfil de boot: 35.733.822 de 35.734.220 ciclos nativos.
- Hashes por frame identicos entre codigo recompilado e `--interp-only` em 1200 frames de boot.
- Rota `tests/start-route.txt`: 1800 frames; chegou ao primeiro mapa; 53.256.198 de 53.602.519 ciclos nativos (99,4%). Todos os hashes por frame coincidiram com `--interp-only`.
- CTest: teste de menu em renderer SDL de software e seis casos do importador aprovados.
- `runtime_menu_smoke.py`: host SDL real com video/audio dummy, sem janela visivel. Abertura de menu, navegacao e screenshot aprovados. Dois estados separados por 200 ms mantiveram o mesmo frame enquanto pausado; apos fechar, o frame avancou. Processo encerrou com codigo 0.
- Capturas inspecionadas: `build/title.png`, `build/gameplay.png`, `build/game/menu-preview.png`, `build/runtime-menu.png`.

Na primeira etapa ainda nao havia comparacao com um oraculo independente. Os resultados posteriores aparecem abaixo. Permanecem sem verificacao completa: todas as fases, chefes, final, todos os canais de audio por escuta, controles fisicos, SRAM apos reiniciar e outras regioes/revisoes. O fallback permanece habilitado para caminhos nao perfilados.

## Diferencas ainda pendentes em relacao ao Mushroom

O README e `docs/INTERFACE.md` de `Adoregabriel2005/mushroomrecompiled` foram inspecionados em checkout separado. Ainda faltam neste port: aplicar/descartar; remapeamento de gamepad e teste de botoes pela interface; selecao de dispositivo de audio, frequencia e buffer; slots multiplos; scanlines e brilho. O launcher com memoria de ROM e os tres modos de aspecto foram acrescentados nas etapas seguintes.

Esta e uma primeira integracao funcional, nao a conclusao desses requisitos.

## Menu Rose Recompiled — revisao de abas

- Nome removido dos textos do menu e identidade da janela alterada para Rose Recompiled. Executavel atual: `build/game/RoseRecompiled.exe`.
- Cinco abas em ciclo: Inicio, Som, Video, Controles e Sistema. Trocar de aba no cabecalho nao modifica configuracoes. Navegacao em opcoes continua ajustando valores sem trocar de pagina.
- CTest aprovado apos a alteracao: navega as cinco abas sem mutacoes, ajusta som e video, remapeia uma tecla, impede execucao de salvar via seta ou repeticao, retorna ao cabecalho e fecha.
- Host SDL real com dummy video/audio: navegou todas as abas, capturou cada uma e manteve o frame pausado em todas. Ao fechar, o contador voltou a avancar. Saida 0.
- Capturas de Som, Video e Controles inspecionadas visualmente: `build/rose-menu-som.png`, `build/rose-menu-video.png`, `build/rose-menu-controles.png`.
- ROM local fornecida para o teste conferida: mesmo SHA256 do ZIP inicialmente importado.

## Recompilacao do gameplay

`build-local.ps1` executa o pipeline completo, com perfis privados separados pelo SHA256 da ROM e pela revisao do nesrecomp. A rota atual entra na primeira fase. A introducao precisa terminar antes dos comandos de andar e entrar na porta; a rota antiga parava no mapa.

O codegen gerou 16.496 instrucoes, 30 arquivos C de bancos de ROM e um arquivo C de codigo em RAM (cinco instrucoes descobertas na imagem capturada). O perfil registrou 12.076 entradas de ROM e oito variantes de instrucoes em RAM. Quatro instrucoes dos caminhos descobertos atravessam um banco ou o limite $FFFF e permanecem no interpretador; isso nao significa que apenas quatro instrucoes do jogo inteiro ainda precisem ser descobertas.

No alinhamento 0, a rota de 6000 frames executou 178.655.070 de 178.680.624 ciclos em codigo nativo: 99,9857%. Nenhuma instrucao de RAM precisou ser interpretada nessa rota. O programa continua usando o fallback para trechos ainda nao perfilados.

As capturas nos frames 3000, 3600, 4800 e 5999 foram inspecionadas: gameplay ativo, voo, inimigos, deslocamento horizontal e alteracao de pontuacao/vida. Nao foi comprovada conclusao da fase nem aquisicao de habilidade de copia.

Save state: estado salvo depois do frame 2999, carregado em novo processo e comparado ate o frame 3599. Os 600 frames de continuacao coincidiram integralmente, incluindo o hash interno de hardware. Resultado em `build/state-verification/result.json`.

Audio gerado: PCM mono de 48.000 Hz, 4.792.044 amostras (99,83425 s), pico de 10.036 em 16 bits e 4.094.973 amostras nao nulas. Isso confirma geracao de audio sem silencio total nem saturacao nessa captura; nao substitui verificacao por escuta.

Identidade: uma copia de teste com um byte de PRG alterado foi recusada com saida 2 e mensagem de hash diferente. A ROM original nao foi modificada.

O status final e as comparacoes por alinhamento ficam em `build/verification/result.json`; somente `passed` indica verificacao encerrada. O teste compara todos os hashes internos contra o interpretador do mesmo framework. Contra o TriCNES independente, compara trace observavel por ciclo, memoria/imagem, registradores e contagem de ciclos; o hash da representacao interna e excluido porque os dois nucleos tem estruturas diferentes.

Resultado final: os 6000 frames coincidiram nos alinhamentos 0, 1, 2 e 3, tanto contra o interpretador quanto contra o TriCNES. Cobertura nativa entre 99,9856985% e 99,9857035%. CTest aprovado (menu e 11 casos Python). Os testes Python foram ajustados para criar temporarios dentro do workspace, pois a pasta temporaria do sandbox recusava escrita; as comparacoes do jogo ja concluidas foram rechecadas sem reexecutar o binario inalterado. `tools/check_results.py` registrou o resultado final com SHA256 do executavel, dos seeds e das capturas.

Executavel validado: SHA256 `712a23af76a65c215e99818f2b16e4ff53102090da2569641901296adf0979ed`.

## Pacote portatil 0.2.0-beta.1

ZIP: `build/packages/RoseRecompiled-0.2.0-beta.1-Windows-x64.zip` (2.293.088 bytes).
SHA256: `e98772380a9e5ca9a938c2c976d787e03c99af9e8858b66faed5bf029fb63b66`.

O executavel grafico RoseRecompiled.exe inicia runtime/RoseRuntime.exe sem console, valida o SHA256 do payload da ROM e usa um seletor de arquivo Win32 quando nao ha ROM lembrada. O runtime empacotado e byte a byte identico ao validado no gameplay. O ZIP contem SDL2, licencas e instrucoes, sem ROM, saves, logs ou caminhos pessoais preconfigurados.

`tests/verify_beta.py` extraiu o ZIP em pasta separada e aprovou: validacao da ROM correta, rejeicao de arquivo invalido, execucao de 2400 frames pelo iniciador com PATH vazio, gravacao de log, abertura sem argumentos usando ROM lembrada e pausa do menu. Os testes SDL foram invisiveis; o seletor de arquivo Win32 nao foi operado manualmente. Resultado em `build/beta-package-test/result.json`.

Dependencias PE inspecionadas com llvm-objdump: o iniciador depende apenas de componentes do Windows/UCRT; o runtime depende desses componentes e da SDL2.dll incluida. Nao ha dependencia de DLL do compilador fora do pacote. Nao foi testado em outro computador fisico.

## Video 16:9 experimental

O runtime oferece Original 4:3, 16:9 esticado e 16:9 amplo. O amplo reconstruiu a primeira sala horizontal como uma faixa unica de 426x240, sem sobrepor a borda recortada do quadro 4:3. O mapa e os sprites usam os bancos graficos capturados durante a linha de gameplay; o HUD preserva a proporcao original e estende suas bordas laterais.

Na rota de 3000 frames, o modo original coincidiu com o prefixo do trace previamente validado contra TriCNES. No modo amplo, os traces recompilado e `--interp-only` coincidiram integralmente; cinco objetos comuns foram criados fora da antiga janela e seis objetos estavam presentes nas laterais na captura final. A assinatura dos enderecos da ROM e todas as chamadas isoladas foram aprovadas. A comparacao de alinhamento do mapa encontrou `dx=0` e `dy=0`.

Esta verificacao cobre a rota inicial e a ROM USA fornecida. Chefes, salas verticais, transicoes especiais e outras regioes ainda nao foram certificados para o modo amplo.

Na beta `0.3.0-beta.4`, o preenchimento lateral do HUD passou a ignorar as colunas 0–7 mascaradas pelo NES, removendo a faixa azul inferior. Inimigos com o desenhador padrão deixam de usar a cópia OAM no modo amplo e são recolocados pelas coordenadas mundiais quatro pixels acima, mantendo os pés visíveis sobre o terreno sem deslocar o personagem controlado ou os demais sprites.

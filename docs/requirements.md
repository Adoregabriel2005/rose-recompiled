# Rose Recompiled

## Referencias do autor

- Base tecnica: https://github.com/mstan/nesrecomp
- Referencia de recursos: https://github.com/Adoregabriel2005/mushroomrecompiled
- Menu: imagem MiSTer fornecida pelo autor; painel vinho, moldura clara, fonte monoespacada, linha selecionada clara, navegacao por teclado e controle.

## Escopo e criterios de aceite

Identidade do menu e da janela: Rose Recompiled. Abas horizontais: Inicio, Som, Video, Controles, Sistema. Cabecalho selecionavel com setas esquerda/direita para trocar de aba; baixo entra nas opcoes. Referencias MiSTer atualizadas em 19/10/2026: moldura rosa clara, painel vinho escuro, titulo de secao na faixa lateral vertical. O software carregado mantem sua propria tela de titulo dentro do gameplay.

Recompilacao nativa usando nesrecomp, ROM fornecida localmente pelo jogador. Cada regiao, traducao oficial e revisao precisa de identidade por hash, geracao de codigo e validacao individual; aceitar o arquivo nao equivale a suporte jogavel. Nao inferir regiao pelo nome do ZIP. Preservar idioma e temporizacao da revisao original como padrao. Perfis PAL/NTSC precisam ser conferidos no runtime.

Menu pretendido: jogo, video (janela/tela cheia, escala inteira, aspecto, filtros, paleta, overscan, sincronizacao), audio (volume, mute, canais), controles (remapeamento, gamepad, deadzone), saves (SRAM, slots, backups), sistema (regiao e timing automaticos, desempenho, restaurar padroes), acessibilidade e idioma da interface. Cada opcao deve controlar um recurso real; recursos ainda nao conectados devem aparecer como indisponiveis.

O menu pausa a simulacao e continua processando entrada. Configuracoes sao persistentes e versionadas. Saves separados por identidade da ROM evitam mistura de revisoes. Testar teclado, gamepad, resolucoes e restauracao de configuracoes.

Arquivo de macetes em pasta discreta, sem link no menu nem indicacao na ajuda do jogador. Conteudo inicialmente limitado a dicas; enderecos de RAM, patches e codigos so depois de validacao para cada revisao.

## Etapas

1. Importar e inspecionar ROM USA fornecida; compilar framework e codigo gerado.
2. Validar inicializacao, titulo, entrada, fase, audio, IRQ MMC3 e SRAM com runtime.
3. Integrar menu MiSTer ao host SDL, com configuracoes efetivamente conectadas.
4. Catalogar e testar outras ROMs regionais/revisoes fornecidas localmente.
5. Empacotar executavel sem ROM e sem codigo gerado do jogo no repositorio.

Nao declarar o port completo ou todas as regioes suportadas antes dessas verificacoes.

# PLANO — Brazil Defense

**Documento de design. Fixo. Não é briefing de tarefa.**
O terminal consulta este arquivo para entender o que o jogo É e por
quê. Tarefas da vez vão no briefing.md, separado. Quando uma decisão
de design mudar, este arquivo é atualizado.

Versão: 2026-09-14

---

## 1. Conceito

Tower defense de paródia política com arquétipos ficcionais (não
retratos de pessoas reais). O jogador defende uma urna eletrônica no
fim de um percurso; a horda ("caravana") caminha das bordas até a urna.
Cenário: uma capital fictícia inspirada na Esplanada de Brasília.

Estilo: cômico, sátira leve. Nenhum personagem é pessoa real. Alvos e
defensores são tipos (o Coronel, o Populista, o Togado), nunca nomes
ou rostos identificáveis. Decisão de projeto por dois motivos: evitar
problema legal e de loja, e envelhecer melhor no mercado global.

Plataforma: PC (Steam) primeiro. Mobile e console são futuro incerto.
Idiomas: inglês (padrão) e português. Estrutura pronta para acrescentar
mais.

---

## 2. O loop de uma partida

1. Jogador posiciona a URNA numa zona restrita do tabuleiro.
2. Fase de MONTAGEM: posiciona defesas gastando votos, com tempo limite.
3. ONDAS saem das bordas e caminham até a urna.
4. Entre ondas, cronômetro decrescente; jogador reforça a defesa.
5. A partida escala até o jogador vencer (sobreviver a X ondas) ou
   perder (o candidato vermelho alcançar a urna).

---

## 3. Economia — votos (modelo "cruel")

Uma moeda só, que é placar E dinheiro ao mesmo tempo.

- **Voto AZUL**: ganho a cada inimigo morto, valendo o HP dele
  (`HealthPerVote`, K=1: creep de 10 HP = 10 votos). É o placar do
  jogador.
- **Voto VERMELHO**: ganho pelo adversário a cada inimigo que alcança
  a urna, também pelo HP — um vazamento na onda 84 vale centenas, a
  folga do começo não decide nada.
- **Voto NULO**: dano desperdiçado (overkill, tiros perdidos) na
  mesma taxa. Só informativo, nunca pontua.
- Escala inflada de propósito: com votos por HP a partida chega a
  centenas de milhares de votos e os custos de peça/upgrade (dezenas)
  viraram irrelevantes. **Não recalibrar agora** — custos e escala de
  votos são recalibrados juntos quando o conteúdo real entrar.
- Gastar votos azuis (construir, evoluir, mover, vender com prejuízo)
  DERRUBA o placar azul. Fortalecer a defesa enfraquece a posição
  eleitoral. Essa tensão é a decisão central do jogo.
- Antes de qualquer gasto, a interface mostra o placar RESULTANTE. Se
  o gasto for inverter a liderança, avisa em vermelho. O jogador pode
  fazer, mas fazendo sabendo — nunca perde sem entender.

Custos e reembolsos:
- Construir: gasta BuildCost.
- Vender: devolve 100% antes da onda 1, 50% depois.
- Mover: taxa crescente (2% por onda, teto 25%).
- Divisória: janela de remoção livre até a onda 3.

---

## 4. Os candidatos (chefes) e o retorno

- Um candidato sai a cada `CandidateInterval` (5) ondas: 20 numa
  partida de 100. Sai de uma boca sorteada, lento, alvo prioritário de
  todo defensor no alcance. HP = HP do creep da onda × 40 — o da onda
  100 é muito mais forte que o da 5. Só um agendado por vez; se a
  onda dele encontra outro andando, ele vem com a próxima onda livre.
- Matar um candidato não dá voto. Matar um candidato AGENDADO sobe o
  teto de orçamento (+1 torre, +1 personagem): espaço, não peça — a
  peça continua custando votos azuis. **Qualquer candidato chegando
  na urna = derrota imediata, em qualquer onda.** Creep normal
  chegando só soma vermelho.
- **Retorno (a punição da virada):** quando o vermelho ultrapassa o
  azul, todos os candidatos já mortos na partida voltam, cada um com
  a vida original de quando caiu, distribuídos em
  `ReturnParadeSeconds` (30 s). A onda vira só o desfile deles:
  nenhum creep normal sai até todos caírem. Matar todos **iguala o
  placar por baixo** (azul desce até o vermelho) — nada de brinde,
  só para de sangrar; isso fecha o exploit de provocar a virada para
  zerar a dívida dos gastos. Os que voltam não dão recompensa nenhuma.
  Um deles na urna = derrota. Nova virada mais tarde = novo retorno.
- Enquanto há candidato no board (agendado ou de retorno) a partida
  não se decide: as ondas seguem até ele cair ou chegar.
- Debug de blocagem: cada cubo ganha uma cor por ordem de surgimento
  (`TintColor` no material). Sai com os modelos reais.

---

## 5. Tabuleiro

- Grid lógico 48×22 (célula de 700 cm), invisível, sob terreno aberto.
- Seis BOCAS de spawn nas bordas (dois frontais, quatro laterais). As
  laterais chegam mais rápido — o jogador precisa alongar o caminho
  delas com cercas.
- A cada onda, sorteia quantas e quais bocas ficam ativas. O total de
  creeps não muda — menos bocas = fluxo mais concentrado.
- BOCAS MÓVEIS: antes de cada onda cada boca tem 75% de chance de
  deslizar 1–5 células pela própria borda (nunca para dentro, nunca
  sobre célula ocupada, nunca colada noutra boca, sempre com rota até
  a urna). Se o jogador construiu na frente da saída, a boca deixa de
  ser saída e vai para a posição aberta mais próxima na borda.
- URNA posicionada pelo jogador em qualquer célula livre e alcançável
  (a zona X 40-47, Y 6-15 virou só orientação para o gerador de
  obstáculos). Define a célula-objetivo. Fica travada quando a onda 1
  começa.

---

## 6. Peças

Duas famílias, orçamentos separados:

- **Torres** (equipamento de solo): ocupam célula, não bloqueiam
  caminho. Ex.: metralhadora .50, canhão, lança-granadas fixo.
- **Personagens** (atiradores humanos): ocupam SLOT de plataforma,
  nunca o chão. Recebem multiplicador de alcance por estar mais alto.

Plataformas (levam só personagens): palanque (4 slots), caminhão
(6 slots), arquibancada (8 slots). Ocupam célula, bloqueiam caminho.

Cercas ("divisórias"): ocupam ARESTA entre células (modelo Clash of
Clans), não a célula. Formam o labirinto. Duas instâncias de mesh por
aresta. Validação impede fechar completamente o caminho.

Cada atirador tem: dano, alcance (em células), cadência, velocidade e
tipo de projétil, tipo de dano, arco de tiro, tamanho de pente e tempo
de recarga (munição infinita, o pente é só ritmo). Tudo em DataAsset.

Futuro: arco de tiro por plataforma (palanque 360°, caminhão 270°,
arquibancada 180°) — só entra com indicador visual claro.

---

## 7. Progressão e upgrade

- Defensores evoluem por níveis (até 10). Cada nível: mais dano,
  possivelmente nova arma/skin/efeito.
- Curvas (placeholder, calibrar com conteúdo real):
  - HP do creep por onda: exponencial (~1.12 a 1.15)
  - Quantidade por onda: linear (+creeps por boca)
  - Custo de upgrade: exponencial (custo sobe mais rápido que renda)
  - Dano por nível: linear
- Custo exponencial contra dano linear faz empilhar a mesma torre
  ficar caro sozinho — diversificar vira a jogada certa sem proibição.
- Cinco estrelas de evolução trazem skin nova por ator.

---

## 8. Vitória, dificuldade e saves

- FIM DA ONDA 100 (`WavesToWin` por dificuldade) = apuração: **azul
  na frente (ou empate) vence** e liberta os presos (Easy 1, Normal 2,
  Hard 3); **vermelho na frente perde**. Se houver candidato andando,
  a apuração espera ele cair ou chegar.
- DERROTA também a qualquer momento se um candidato alcança a urna
  (seção 4).
- Vencer congela o board como a derrota e oferece seguir em endless:
  as ondas continuam escalando até uma derrota; a vitória fica.
- Play → tela de dificuldade (resumo do DA, marca de vencida, linha do
  bônus) → partida. O progresso (dificuldades vencidas) persiste em
  `BDProgress.sav`.
- Dificuldades encadeadas: vencer Easy dá bônus inicial no Normal, e
  assim por diante. Começar direto no Hard, sem bônus, é quase
  impossível — de propósito.
- Diferença entre dificuldades: orçamento de peças, velocidade de
  escalada das ondas, número de obstáculos fixos.
- SAVES como recurso limitado: Easy 3, Normal 2, Hard 1. O jogador
  escolhe QUANDO salvar — salvar cedo garante pouco, salvar tarde é
  arriscado. Um ponto de restauração por jogo, sobrescrito a cada save;
  só entre ondas; o contador salvo já vem descontado. Restaura o estado
  completo (níveis, votos, tabuleiro, bocas, seed) no começo da onda
  salva. Carrega-se do painel de derrota ou do "Continue" no menu.
- Balanceamento: `BD.Balance.Report` compara a horda de cada onda com a
  melhor defesa que o orçamento compra (nível de referência 5, 50% de
  eficiência) e diz o growth que empata na onda de vitória. A vida dos
  creeps segue `HealthScaleGrowth` exponencial (1.035) enquanto a curva
  fica estacionada; recalibrar quando houver conteúdo real.

---

## 9. Ambientação

- Ciclo dia/noite atrelado à onda (não ao tempo real, por causa do
  acelerador): 16 ondas por dia, o alpha 0 é 06:00. A animação do sol
  entre uma onda e outra corre em segundos reais — 2x/4x não a
  aceleram. Fases por hora (ajustáveis nas settings): nascer do sol
  05–08, dia 08–17, pôr do sol 17–19, fim de tarde 19–20:30, noite.
  O HUD mostra "fase · hora" enquanto o sol se move e some 3 s depois.
  Postes acendem ao entardecer.
- Ônibus de caravana marcam as bocas ativas e escondem a borda do mapa.
- Urna com som de votação a cada chegada, ao ar livre (falloff natural
  a partir da urna, absorção do ar, sem oclusão), com intervalo mínimo
  entre bipes para uma leva grande não sobrepor dezenas de toques.
- Velocidade de jogo 1x / 2x / 4x, persistente entre ondas, via time
  dilation global.

---

## 10. Abertura e telas

- Splash com a marca (Gurila Games) → loading → menu principal com
  fundo cinemático (cinecut futuro) → Play com fade → jogo.
- Menu: Play, Continue (quando há save), Options (gráficos / áudio /
  controles / idioma), Quit.
- Configurações persistidas em SaveGame.
- Câmera da partida: começa na visão geral e o jogador a move — WASD
  desliza o ponto olhado dentro do tabuleiro; roda = altura entre 30 m
  e a visão geral; Q/E ou botão do meio giram a vista, livre no chão e
  travada de volta ao subir (a borda do mundo nunca aparece de cima);
  Home reseta. A/D invertíveis em Opções. R gira a peça na mão.
- Paleta de construção no HUD (urna + peças, com o que resta de cada
  uma), teclas 1–9 e U. Painéis com cantos arredondados; nomes de
  peças pela string table.
- HUD estiloso com ícones e animação — construído em camadas:
  funcional primeiro, arte depois, animação por último.

---

## 11. Regras de produção

- Tudo data-driven: valores em DataAsset / DeveloperSettings, nada
  hardcoded.
- Classes base em C++, configuração em Blueprint/DataAsset.
- Todo texto visível é FText localizável.
- Calibração fina de balanceamento só quando houver conteúdo real
  (vários atiradores e tipos de inimigo). Até lá, números são
  placeholder consciente e a forma das curvas é o que importa.
- Meshes: origem na base, escala aplicada, frente em +X, um material
  slot quando possível, LOD e colisão simples.
- A cada commit + push, anotar na seção 12 deste arquivo as alterações
  feitas entre o push anterior e o atual (o que entrou, o que mudou de
  regra, o que ficou pendente). Sem isso o histórico do projeto vive
  só no git e some da leitura.

---

## 12. Histórico de pushes

Uma entrada por push, mais recente em cima: data, commit(s) e o que
mudou desde o push anterior.

- **2026-09-14 (noite, 3) — 0747d14** (desde ede2420):
  - HUD: painel do candidato virou lista — uma linha por candidato
    vivo (nome, HP, barra na cor dele), até 6 linhas e "+N a caminho"
    acima disso. `DisplayName` no ator ("Candidato N" até os 20 terem
    nome próprio).
  - **Regra nova (seção 4):** matar candidato AGENDADO sobe o teto de
    orçamento em +1 torre e +1 personagem (`TowerBudgetPerBoss`,
    `CharacterBudgetPerBoss`); é espaço, não peça — cada peça segue
    custando votos azuis. Candidato do desfile de virada não dá nada.
    Aviso no HUD ao ganhar.
  - Textos `HUD.Candidate.Return/More` e `HUD.Reward.Budget` (Return
    faltava desde o commit anterior).

- **2026-09-14 (noite, 2) — b162e3a** (desde 795cdc8):
  - Placar com ícones (cédulas e urna, fixos do HUD, por fração da
    tela) e feedback de voto: cédula vibra e escala, número dá tick,
    urna pulsa junto do bipe; rajada não reinicia, no máximo um pulso
    pendente.
  - Som da urna 3D a partir do ator, listener preso à câmera (vale
    para todo som de mundo), raios calibrados pelo zoom (claro a 60 m,
    quase mudo a 330 m), reverb de exterior construído em código,
    concurrency máx 4, classe de efeitos.
  - Briefing 15:30: âncora das bocas (nunca > 5 células do ônibus,
    pelo passeio ou pelo bloqueio); votos por HP (`HealthPerVote`, azul
    e vermelho) e nulos por dano desperdiçado; barra de apuração de
    três faixas com a urna em cima e número flutuante subindo da urna
    a cada chegada; som ponderado pelo valor; barras de vida dos creeps
    no canvas (`ABDMatchHUD`), só depois de dano, somem no zoom afastado.
    `BD.Camera.Set`.
  - Briefing 17:10 — **regras novas (seções 3, 4 e 8):** candidatos
    viram chefes agendados a cada 5 ondas com HP do creep da onda × 40;
    candidato na urna = derrota imediata em qualquer onda; fim da onda
    100 decidido pelo placar (azul na frente ou empate vence); virada
    do placar traz de volta todos os candidatos já mortos com a vida
    original, onda vira só o desfile, matar todos iguala o placar por
    baixo. O pause de 30 s pós-morte saiu. Cor de debug por ordem nos
    cubos (`TintColor`). Economia inflada registrada, não corrigida.
    `BD.Candidate.Return`, `BD.Delay`.
  - Pendente: registros de candidatos (enviados/caídos) fora do save da
    partida; `TintColor` depende do material do `DA_Candidate`.

- **2026-09-14 (noite) — 5d0cdd3** (desde fa2c178):
  - Relógio do dia (seção 9): `DawnHour` e faixas de fase nas settings
    do ciclo, `EBDDayPhase`, hora/fase/texto no componente; animação do
    sol em segundos reais (`BlendSpeed` 0.03); 16 ondas por dia; linha
    "fase · hora" no HUD enquanto o sol anda + 3 s.
  - Bocas com saída bloqueada (seção 5): antes do sorteio, boca com
    peça ou cerca à frente da saída vai para a posição aberta mais
    próxima na borda (`IsExitOpen`, `TryShiftSpawnPoint`); aviso no log
    se não houver.
  - Vitória na onda 100 como default de classe (o Hard com defesa aberta
    não perdia na 50).
  - Som da urna (seção 9): `S_UrnaVoto` importado de
    `Content/audio/Urna Eletrônica.wav`; toca em `AddVotesRed` pela
    `UBDObjectiveSubsystem::PlayVoteSound`, no máximo um bipe a cada
    0,4 s reais, atenuação ao ar livre configurada em código (esfera
    20 m + falloff 300 m, natural, LPF com a distância), pela classe de
    efeitos. Verificado: 30 chegadas em leva → 17 bipes espaçados.
  - HUD responsivo (seção 10): curva de DPI declarada no
    `DefaultEngine.ini` (1.0 em 1080p, 1.333 em 1440p, 2.0 em 4K, lado
    menor); paleta virou barra de itens na base (centro), cada item com
    ícone em Scale Box dimensionado por fração da tela
    (`ItemIconHeightFraction`), painéis laterais e barra do candidato
    por fração da largura (`SidePanelWidthFraction`,
    `CandidateBarWidthFraction`); `Icon` (512 px+) no DataAsset da peça,
    slot escondido até a arte chegar. `BD.UI.Shot` para capturar o HUD;
    testado em 1080p, 1440p e 4K com o mesmo layout.
  - Seções 4, 5, 8, 9 e 10 deste documento atualizadas com as regras
    decididas hoje.

- **2026-09-14 (tarde) — 297219b** (desde e9967ba):
  - Primeiro teste real do jogador. Faltava o essencial: **paleta de
    construção** no HUD (`Palette` em `BDPlacementSettings`, painel à
    esquerda, teclas 1–9 e U para a urna). Sem ela não havia como pegar
    peça nenhuma.
  - **Câmera livre** (seção 10 na prática): WASD desliza o ponto olhado
    dentro do tabuleiro; roda = altura entre 30 m e a visão geral; Q/E ou
    botão do meio giram a vista, livre no chão e travada de volta ao subir
    (`MaxYawAtMinHeight`, `YawLimitExponent`) para nunca mostrar a borda do
    mundo; Home reseta; Page Up/Down altura por tecla. Opção "Inverter
    câmera A/D" em Opções › Controles. R / Shift+R giram a peça; o
    `IA_Rotate` na roda deixou de ser ligado.
  - **Regra nova (seção 5):** a urna vai em qualquer célula livre e
    alcançável (`bRestrictToZone=false`); a zona só orienta o gerador de
    obstáculos.
  - **Regra nova (seção 5):** bocas móveis — antes de cada onda cada boca
    tem `MouthWanderChance` (75%) de deslizar até `MouthWanderMaxCells` (5)
    células pela própria borda, sem sair dela, sem célula ocupada, sem
    encostar noutra boca e sempre com rota até a urna. Sorteio por seed +
    onda.
  - **Regra nova (seção 8):** vitória na onda 50 (default de classe;
    os DA sobrescrevem). Montagem inicial 120 s como default.
  - **Balanceamento (seção 7/11):** `BD.Balance.Report` compara a horda
    de cada onda com a melhor defesa que o orçamento compra
    (`ReferenceMaxLevel` 5, `ReferenceEngagementEfficiency` 50%) e diz o
    growth que empata na onda de vitória. A curva
    `CF_HealthScaleByWave` travava em ×26,7 da onda 30 em diante; ficou
    estacionada no `DefaultGame.ini` e a exponencial entrou com
    `HealthScaleGrowth = 1.035` (empate na onda 50 para rota de 43
    células; labirinto mais longo vence).
  - Dia/noite: 10 ondas por dia (era 20) e uma linha de log por onda com a
    posição no dia. Linha do alvo das torres ligada por padrão
    (`BD.Tower.ShowTarget`). `BD.Path.Debug` desligado por padrão — a
    esfera com linha era lida como marcador de jogo.
  - Painéis com cantos arredondados (`BoxCornerRadius`). Nomes de peças
    pela string table (`Piece.<asset>`), inglês e português.
  - Logo do splash reimportado de `logo/T_Logo_Golias.png` (2048²).
  - **Pendente no editor:** `FirstWaveDelay` e `WavesToWin` nos três
    `DA_Difficulty_*` (sobrescrevem os defaults); conferir as curvas
    `CF_Sun*` se a noite persistir além de 50% do dia (log mostra);
    `ReferenceEngagementEfficiency` quando houver sensação de jogo.

- **2026-09-14 — 81b91cd** (desde dac5d17):
  - Janela fora do editor (pendência do dia 13): `ApplyGraphics` só passa
    pela resolução com tamanho explícito, tela cheia ou troca de modo
    (sair da tela cheia sem tamanho dá 3/4 do desktop); o resto vai por
    `ApplyNonResolutionSettings`. Não visto na tela ainda.
  - Vitória (seção 8): `WavesToWin` e `PrisonersFreed` no DA de
    dificuldade; `DeclareVictory` ao limpar a onda-alvo, board congelado
    como na derrota; `ContinueEndless` reabre a montagem e as ondas seguem
    escalando até uma derrota. Painel central de fim no HUD (vitória:
    presos + "Continuar (endless)" + menu; derrota: placar + "Carregar
    save" + menu). `BD.Match.Endless`.
  - **Regra nova (seções 4 e 8):** a vitória é só por ondas, mas fica
    retida enquanto o candidato está no board — as ondas seguem saindo;
    ele morre → vitória (na hora entre ondas, ou ao limpar a onda em
    curso); chega na urna → derrota. Fecha o caso de "vencer" com o
    candidato andando e o vermelho na frente.
  - Saves limitados (seção 8): `UBDMatchSave`, um slot por jogo
    (`BDMatch`), sobrescrito a cada save; `SaveBudget` no DA; só entre
    ondas; o contador salvo já vem descontado. Restauração em-place:
    despawn, remove peças e urna, regenera obstáculos pela seed (a urna
    sai antes porque o gerador protege a zona inteira sem urna), recoloca
    tudo pelo fluxo normal de posicionamento, devolve votos/onda/
    orçamentos/níveis. Candidato não é serializado: os votos reemitidos
    o trazem de volta se o placar seguir invertido. Botão Save no HUD,
    "Continue" no menu quando há save. `BD.Save.Write/Load/Status`,
    `BD.Place.AtSlot`.
  - Bônus encadeado (seção 8): `UBDProgressSave` (slot `BDProgress`)
    registra as dificuldades vencidas; `ChainBonus` no DA (votos azuis
    iniciais + peças + saves) entra quando a dificuldade um degrau abaixo
    foi vencida. Defaults placeholder: +100 votos, +4 divisórias, +1
    plataforma, +1 torre, +2 personagens, +1 save. `BD.Progress.Status/
    SetWon/Reset`.
  - Seleção de dificuldade no Play (seção 10): tela `DifficultySelect`
    com Fácil/Normal/Difícil, resumo do DA, marca de vencida e a linha do
    bônus; Voltar. A escolha vai pelo `UBDUISubsystem` e o match manager
    a toma no `BeginPlay`; PIE mantém o default. `BD.UI.Play [dif]`.
  - `-BDSkipFrontEnd` para as checagens headless (o redirecionamento ao
    menu do dia 13 as tinha quebrado).
  - **Pendente no editor:** os três `DA_Difficulty_*` herdam os defaults
    de classe para `WavesToWin`, `PrisonersFreed`, `SaveBudget` e
    `ChainBonus` — os valores do PLANO (presos 1/2/3, saves 3/2/1) ainda
    não foram postos nos assets. Ver na tela: painel de fim, botão Save,
    tela de dificuldade, Continue. Seguem do dia 13: urna no cursor com a
    câmera fixa e o enquadramento.
  - Fora do commit: três PlayerStarts do mapa Esplanada alterados/criados
    na sessão do editor de 14:43 (não confirmados como intencionais).

- **2026-09-13 — e484367 / 8d2f5d5** (desde f1f3c9b):
  - Linha `Wave N cleared. Votes: blue B, red R.` no log a cada onda.
  - Candidato vermelho completo (seção 4): `ABDCandidate`,
    `UBDCandidateSubsystem`, `DA_Candidate` (cubo 3×3×4 vermelho),
    gatilho por mudança de voto, alvo prioritário, derrota ao chegar,
    pausa de 30 s com vermelho congelado ao morrer, volta na onda
    seguinte. `BD.Candidate.Spawn/Status`, `LogBDCandidate`.
  - Venda (seção 3): qualquer peça menos a urna, em qualquer fase;
    100% antes da onda 1 e divisória até a onda 3, 50% depois;
    passageiros voltam ao orçamento. `BD.Place.Sell/SellAtEdge`.
  - Camada de interface (seção 10), crua, UMG por código: mapa
    `MainMenu` → splash com logo "Golias Indie Games" → loading → menu
    (Play/Options/Quit) → fade → jogo. Opções (gráficos/áudio/idioma)
    persistidas em `BDSettings.sav`; inglês e português por string
    table (`Content/BD/Text/*.csv`), troca em runtime. HUD com placar,
    onda, velocidade, bocas, painel do defensor (upgrade/venda com placar
    resultante), painel da peça na mão, candidato. Menu de pausa (botão
    Menu / Esc). Paleta do logo (navy, ciano, off-white).
  - Câmera fixa da partida derivada do grid (`UBDCameraSettings`);
    sem pawn voador. AutoSetup desligado por padrão. Urna vai pra mão
    ao entrar sem urna. Standalone fora do editor redireciona pro menu.
  - **Onde paramos:** fora do editor a janela abre do tamanho do
    desktop (o padrão do `GameUserSettings` é a resolução do monitor);
    a correção em `ApplyGraphics` (só aplicar resolução quando
    explícita ou em tela cheia, via `ApplyResolutionSettings`) ficou
    escrita mas não aplicada. Falta o teste visual da urna no cursor
    com a câmera nova (o log já mostra o hover chegando ao meio do
    tabuleiro) e ajustar o enquadramento da câmera se preciso.

- **2026-09-12 — f1f3c9b / accb09a** (último push antes desta regra):
  movimento orgânico da horda, bocas ativas por onda, variação de rota
  por creep, scoreboard de debug. Pendente na época: candidato e venda.

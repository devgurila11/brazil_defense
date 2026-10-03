# PLANO — Brazil Defense

**Documento de design. Fixo. Não é briefing de tarefa.**
O terminal consulta este arquivo para entender o que o jogo É e por
quê. Tarefas da vez vão no briefing.md, separado. Quando uma decisão
de design mudar, este arquivo é atualizado.

Versão: 2026-09-15

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
- **Questão aberta (2026-09-23): duração da partida.** Jogada natural,
  tudo no nível 1, chegou à onda 115; da 83 em diante não aparece
  decisão nova e a recompensa fica irrelevante. Talvez 100 ondas seja
  muito para o conteúdo atual — mas NÃO mexer no número de ondas
  agora: quem dá sentido às ondas altas é o elenco (variedade de
  inimigos, torres e políticos). Decidir quando o elenco existir.
  O placar, por outro lado, está confirmado: 62/38 na onda 115, morto
  cedo e disputado tarde, como previsto — não reformar o vermelho.

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
- **Anotado para quando mexer em iluminação (não agir antes):** a tela
  mostra o warning do Lumen "Cached lighting in Lumen and real-time sky
  capture lighting is going to be clipped... adjust r.EyeAdaptation...
  Exposure -8.5, safe range [-8.0, 12.0]" (visto em 2026-09-24). É a
  exposição da cena, a -8,5, fora da faixa segura. Não tem a ver com o HUD.

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

- **2026-10-03 (bugs, áudio, estrelas, HUD limpo, coice) — COMMIT** (desde 07fb616):
  - Briefing 2026-10-03 10:30.
  - **Áudios renomeados** (script em lote): 10 músicas →
    `MUS_Battle_01..10` (ordem de geração); efeitos em 5 grupos de 4:
    `AMB_City_Day_01..04`, `AMB_Birds_Day_01..04`, `AMB_Night_01..04`,
    `S_Bus_Engine_01..04`, `S_Bus_Horn_01..04` (vieram 4 de cada
    ambiência, não 1). WAV originais em `SourceAudio/Effects` e
    `SourceAudio/Musics` (este fora do git: 352 MB que repetem a fonte
    guardada nos `.uasset`), mapa em
    `SourceAudio/Musics_Effects_rename_map.txt`. Músicas em streaming
    (Load on Demand) e na `SC_Music`; efeitos na `SC_Effects`.
  - **Bug: mais de um palácio** — não existia trava nenhuma. Recusa
    nova `PalaceAlreadyBuilt` ("Já existe um palácio") na mão e no
    tabuleiro, contada pelos atores vivos: vender libera de novo.
  - **Bugs: Agente afundado** — mesma causa: ele ficava no Z do plano
    do grid e o asfalto está 26 cm acima. Agora pisa no chão traçado
    (como os militantes) e o Z acompanha entre células. Dormindo, a
    animação deita de lado e o braço ainda entrava ~12 cm:
    `SleepLift` 14 cm (DA do palácio, categoria Agent) ergue o corpo só
    no sono, suavizado. Medido com render: andando, osso mais baixo
    +2 a +7 cm; dormindo, +1,2 a +30 cm em 32 s (nada abaixo do chão).
    `BD.Agent.Feet`.
  - **Personagens de plataforma (diagnóstico):** o giro estava certo
    (5–15% do tempo). Com militante no alcance passavam 39–51%
    "reconhecendo" (o `AcquisitionDelay` recomeçava a cada troca de
    alvo, inclusive depois de cada abate) e 16–53% segurando fogo por
    "todos condenados" (tiros no ar já matam), parados. Agora o
    reconhecimento só vale saindo do ócio (`ReacquireGrace` 1 s, Towers)
    e, com todos condenados, a mira segue o mais adiantado sem atirar.
    Reconhecendo caiu para 6–33%. Vale para torres também.
    `BD.Tower.Status` mostra a divisão do tempo.
  - **Coice:** a cada tiro o personagem recua 12% do maior lado da
    malha e volta (0,14 s); torres de chão só com `bRecoilOnGround`. No
    Agente não: a animação de tiro da pistola já mostra o disparo.
  - **Estrelas em todas as construções:** torres de chão pelo nível e
    plataformas pelo nível do bloco (0 até todas as vagas terem
    atirador), mesma linha do palácio, 70% do tamanho, mesmo fade.
    Estrela cheia = nível (torre recém-construída mostra 1).
  - **H = modo limpo:** esconde HUD, estrelas, barras, linhas de debug
    (alcances, grid, rotas) e mensagens na tela; H de novo traz tudo.
    `BD.HUD.Clean [0|1]`.
  - **Música** (`UBDSoundscapeSubsystem`, Project Settings > Brazil
    Defense - Audio): faixa aleatória sem repetir ao começar a onda
    (fade-in 2,5 s), a próxima entra em crossfade 5 s antes do fim (as
    faixas terminam em fade próprio), fade-out 4 s fora da onda. Toca
    durante a pausa.
  - **Ambiência:** cidade + pássaros de dia, noite (coruja/grilos) e
    menos cidade à noite, pela hora do ciclo; cada camada encadeia as 4
    variações curtas com crossfade; cai para 50% sob a música.
  - **Ônibus:** cues `SCue_Bus_Engine` e `SCue_Bus_Horn` (aleatório sem
    repetir, ±4% de tom). Motor nas bocas ativas quando a onda é
    sorteada, e a saída espera pelo menos 1,5 s (`EngineLeadSeconds`);
    buzina no primeiro militante de cada boca. 3D preso ao ônibus.
    Envio de reverb ligado, mas o mapa não tem reverb ativo: ligar um
    afetaria também os sons do jumento, então ficou para decidir.
    `BD.Audio.Status`, `BD.Audio.Music`.
  - Regressão 62/62 (novos: palácio único e revenda, Agente no chão,
    coice, motor por boca ativa, música na onda).

- **2026-10-02 (assets do usuário) — 152fecf** (desde d9926df):
  - Os assets que o usuário trabalhou no editor ao longo do dia:
    modelo do Palácio (`Meshs/Building/Palacio`: `SM_Palacio` e
    texturas, `MI_Palacio`) e o `DA_PalaceData` alterado no editor; STF
    reorganizado na pasta `Meshs/Building/STF` (`SM_STF` e texturas,
    `MI_STF`, as texturas soltas antigas removidas — nada referencia
    mais o caminho antigo); normais das quatro skins do Jumento.

- **2026-10-02 — fechamento do dia** (dfeaa05 → 7cd4ac0, 7 briefings):
  - Feito hoje: Agente (fatia 2 do palácio) com patrulha, tiro pelo
    gesto, perseguição ao candidato, descanso por tempo e barra; regra
    de ouro da economia (construir fixo, evoluir ×3, palácio 3000);
    estrutura de som de tiro (slot vazio); voto por corpo e placar 0 a
    0; virada solta o candidato cedo; mira local das torres; alcance
    on-click; falas 2 por lado; recursos a qualquer momento (com
    proteção contra prender militante); Esc/direito cancelam; noite em
    18% das ondas. Regressão 55/55.
  - **Onde paramos / pendências:**
    - Som do tiro da pistola: o slot `Weapons[0].FireSound` está vazio
      até chegarem 2–3 variações (montar com `BDBuildSoundCue`).
    - Conferir na tela: falas de lados opostos (de fone), ritmo da
      perseguição do Agente e descanso de 45 s, mira das torres.
    - Noite escura: fica para os postes de iluminação.
    - Próximas fatias do palácio: 3 (ministros, usa o chute já pronto)
      e 4 (garrafa).
    - Fora dos commits, do usuário: texturas do STF, normais do
      Jumento, pasta `Meshs/Building/Palacio`, `MI_Palacio` e
      `DA_PalaceData.uasset` modificado no editor.

- **2026-10-02 (fechamento: Esc e mais dia) — 8eb9c98** (desde 034da7e):
  - Briefing 2026-10-02 18:45.
  - **Esc cancela a peça na mão:** o código já cancelava, mas os botões
    da HUD (sobretudo os itens da barra de construção) eram focáveis: ao
    clicar num deles o botão ficava com o teclado e engolia o Esc, que
    nunca chegava ao jogo. Agora nenhum botão da HUD pega foco
    (`ButtonsTakeFocus`; menus continuam focáveis). **Botão direito**
    com peça na mão (ou movendo) também cancela; antes vendia o que
    estava embaixo do cursor. Sem nada na mão segue vendendo.
  - **Mais dia, menos noite:** as ondas caíam em passos iguais pelo
    ciclo, e a noite (20:30–05:00) ocupava 35% dele. Agora as ondas
    atravessam a noite mais depressa: `NightShare` 0,18 (Day Cycle >
    Clock). Com 16 ondas por dia, a noite caiu de 6 para 3 ondas. Curvas
    de luz e horários intocados — a noite escura fica para os postes.
  - Regressão 55/55 (novo: botões da HUD sem foco, direito/Esc com
    peça na mão cancelam sem vender, 18% das ondas à noite).

- **2026-10-02 (recursos a qualquer momento) — 4c6aaa8** (desde e3fe06f):
  - Briefing 2026-10-02 17:15. Construir (qualquer peça), pôr
    personagem em slot, evoluir, mover (com a taxa) e vender passam a
    valer em montagem **e** com onda ativa; só o recurso limita
    (dinheiro, cota, slot). Fora de partida (setup, vitória, derrota)
    continua travado. A peça na mão não é mais descartada quando a onda
    começa. O clique numa peça durante a onda passa pelo mesmo gesto de
    sempre (clique seleciona, arrasto move).
  - **Urna** segue travada depois que a onda 1 começa.
  - **Proteção nova:** com militantes andando, uma peça que bloqueia
    (plataforma, palácio, cerca) é recusada como `WouldBlockPath` se
    cair na célula para onde um militante vai ou se fechar a rota de
    algum militante até a urna — senão ele ficava parado para sempre e
    a onda não acabava. Uma inundação a partir da urna checa todos de
    uma vez (`UBDPathfinder::WouldCutOffCells`).
  - Regressão 53/53: a invariante "nada se constrói com onda ativa"
    virou "constrói com onda ativa se houver recurso, urna travada";
    novo: plataforma na célula de um militante andando é recusada.

- **2026-10-02 (placar 0 a 0, mira das torres) — 9dcd8ef** (desde c28c53a):
  - Briefing 2026-10-02 16:30.
  - **Placar começa 0 a 0** nas três dificuldades: saiu `StartingVotes`
    do `DA_Difficulty` e também os votos do bônus encadeado
    (`ChainBonus.Votes`, que dava +100 azul); o bônus segue com
    dinheiro, separadores, plataformas e saves.
  - **Gatilho de virada:** antes, vermelho passar azul só trazia de volta
    os caídos — sem caídos, não fazia nada. Agora, sem caídos, solta o
    próximo candidato na hora, e ele conta como o da agenda: a onda
    agendada dele não manda outro (log "already walked out"). Com
    caídos, o retorno segue igual. Teste sem defesa: primeiro militante
    na urna (0×1) soltou o candidato da onda 5 na onda 1; derrota na 2.
  - **Mira das torres local:** volta a ser o mais adiantado, mas medido
    no alcance da própria torre — quem tem menos caminho pela frente
    até sair do raio dela (`ABDEnemyBase::GetPathLeftWithin`), não o
    mais perto da urna no mapa. Reavaliado a cada tiro: quem ultrapassa
    dentro do raio leva o próximo. Candidato no raio continua primeiro.
    Saíram `bDynamicAim`/`AimSwitchMargin` das torres. O Agente segue
    no mais próximo dele com trava de 15%. Regra do projeto: torre de
    disparo = mais adiantado no próprio alcance; Agente = mais próximo
    dele; arma contínua (futuro) = grudada.
  - Regressão 52/52 (novo: placar abre 0 a 0).

- **2026-10-02 (Agente, falas, alcance on-click) — de373a7** (desde 3057f6f):
  - Briefing 2026-10-02 15:00.
  - **Voto por corpo** (`bVotesByBody`, padrão ligado): militante morto
    +1 azul, militante na urna +1 vermelho, candidato nada. Nulo também
    em corpos: 1 nulo por "um creep de HP" de dano desperdiçado. A
    regra por HP continua lá desligada. **Depende disso:** os
    `StartingVotes` dos `DA_Difficulty` (Normal começa com 1600 azul,
    que era escala de HP e agora vale ~30 ondas de abates) — decisão
    dele.
  - **Agente:**
    - Persegue o candidato: com um em campo larga a patrulha, corre
      atrás (`ChaseSpeedScale` 1,5) e só para pro candidato, quando ele
      está a 75% do raio (`ChaseCloseIn`; segue atirando enquanto ele
      estiver no raio inteiro — sem isso oscilava na borda e não
      atirava). Militante: para e atira, sem perseguir.
    - Atira até deitar: na volta pra casa para e atira em quem entra no
      raio (teste: 11 tiros e 5 abates no caminho).
    - Descanso por tempo: `RestTimeBase` 45 s − `RestReductionPerKill`
      0,5 s por abate da patrulha, mínimo `RestTimeMin` 10 s; acorda
      sozinho no meio da onda. A barra enche durante o sono (violeta,
      `AgentRestBarColor`). Sai `SleepWaves`.
    - Mira dinâmica: a cada tiro reavalia o mais próximo e só troca se
      o novo estiver 15% mais perto (`AimSwitchMargin`); candidato no
      raio sempre ganha. `EBDAimMode` por arma (Dynamic; Locked fica
      pro laser). As **torres também** passaram a mirar o mais próximo
      com a mesma trava (`bDynamicAim`, `AimSwitchMargin` em Tower
      Settings) — antes miravam o mais adiantado na rota.
    - Tiro amarrado ao gesto: `UBDAnimNotify_Shot` no `Shooting` em
      0,21 s (onde o pulso dá o tranco, achado pelos ossos), e o loop
      toca a `FireRate` × duração (1,75×) = um gesto por tiro.
  - **Falas:** teto 2 (`SpeechMaxConcurrent`), só de lados opostos da
    câmera com 30° de separação (`SpeechMinSeparation`); cheio, a mais
    próxima substitui a mais distante. Pan total por posição. Relincho
    fica no teto antigo de 4; casco e o resto sem mudança.
  - **Alcance on-click:** sem esferas permanentes. Clicar em torre
    mostra o alcance dela; em plataforma, o de quem está nela; no
    palácio ou no Agente, o raio do Agente; no vazio ou Esc, some
    (`UBDInspectionSubsystem`). `BD.Tower.ShowRange`/`ShowTarget` e
    `BD.Agent.ShowRange`: 1 = selecionado, 2 = todos, 0 = nada. Piscadas
    de tiro seguem em `BD.Agent.ShowShots`. Novo `BD.Place.EmptyHand`.
  - Regressão 51/51 (novo: notify no Shooting, descanso, alcance por
    seleção, voto por corpo). Capturas renderizadas conferidas.

- **2026-10-02 (ajustes do Agente) — 4ae5bf0** (desde 91b1832):
  - Briefing 2026-10-02 10:30.
  - **Palácio 3000** (`PalaceCost`, fixo): acima dos 2000 iniciais do
    Normal, o Agente vira conquista de alguns candidatos.
  - Confirmados, sem mudança: taxa de mover sobe com a onda (exceção
    consciente à regra de ouro); bônus por abate −0,15 s/nível e 6
    armas.
  - **Debug do tiro do Agente pisca:** cada disparo desenha um risco
    magenta que some em 0,06 s, no ritmo da cadência
    (`BD.Agent.ShowShots`). Saiu a linha contínua até o alvo. Raio do
    Agente agora verde (`BD.Agent.ShowRange`), para não confundir com
    as torres (ciano/vermelho/amarelo). Arma contínua (laser, rajada),
    quando existir, terá linha contínua.
  - **Som de disparo reutilizável:** `UBDShotSoundSubsystem::PlayShot`
    (3D na posição do atirador, atenuação pela câmera, SC_Effects,
    concorrência com corte do mais distante) e `UBDShotSoundSettings`
    (Project Settings > Brazil Defense - Shot Sound: teto 8, volume,
    raios). Cada arma tem `FireSound` (no `FBDAgentWeapon`; torres e
    atiradores ganham o mesmo campo depois). **Slot vazio**: tiro sem
    som. Para plugar: pôr as 2-3 variações numa pasta, rodar
    `-run=BDBuildSoundCue -Cue=/Game/BD/Audio/SCue_Pistola_Tiro
    -Folder=<pasta>` e apontar `Weapons[0].FireSound` no
    `DA_PalaceData`. `BD.Agent.Status` conta pedidos/tocados/silenciosos.
  - Regressão 47/47 (novo: fundos iniciais não compram o palácio).
    Cenário headless: 18 tiros, 18 pedidos de som, 18 silenciosos.

- **2026-10-02 (regra de ouro) — df908b6** (desde 6a16985):
  - Briefing 2026-10-01 16:30. **Construir tem preço fixo; só a
    evolução sobe (por nível); só a renda sobe com a onda.**
  - Antes, toda peça custava "propinas do candidato da onda" (preço
    subia ~2,9× até a onda 50 e ~8,6× até a 100) e a evolução também
    escalava com a onda. Viraram fixas (preço da onda 1): Palanque 160,
    Caminhão 400, Arquibancada 320, Torre 400, Personagem 160, Palácio
    **1800** (era 6,4 candidatos: 2560 na onda 1, 3469 na 15).
    Separador já era 0.
  - Fórmula: `ReferencePiecePrice` 400 × `ReplacementCostRatio` ×
    base / `ReplacementReferenceCost`; palácio por `PalaceCost` 1800.
    Sai `PalaceCostInCandidates`, `GetPriceScale`,
    `GetUpgradeCostOnWave`, `GetBuildPriceOnWave`.
  - Evolução: nível 2 = base, cada nível seguinte ×`UpgradeCostGrowth`
    = 3,0 (era 1,35 e subia com a onda). Base 40: 40 / 120 / 360 /
    1080, 1600 até o nível 5 (antes 358 na onda 1).
  - Passos da horda −50%: `FootstepVolume` 1,0 → 0,5.
  - **Diagnóstico da onda 11** (partida 2026-10-02 16:11, Normal):
    não passou candidato. O candidato 2 saiu na onda 10 e foi morto na
    11 (log `CANDIDATE 2 killed on wave 11`, `CandidateArrived` 0). O
    vermelho da onda 11 (+324, 305 → 629) são 27 militantes na urna a
    12 votos cada. Game over por candidato na urna funciona: a
    regressão força um até a urna e dá Defeat.
  - Regressão 46/46 (novo: preço fixo de todas as peças, escada de
    evolução 1×/3×/9×/27×; palácio = `PalaceCost`).
  - **Onde paramos:** no Normal o jogador começa com 2000, então o
    palácio a 1800 já dá pra comprar antes da onda 1. Decisão dele.
    A taxa de mover peça ainda sobe com a onda (`MoveTaxStep`);
    não é preço de construir, ficou como está.

- **2026-10-02 (fatia 2) — 9700a2f** (desde dfeaa05):
  - **Palácio, fatia 2 de 4: o Agente ("Mito")** (briefing 2026-10-01
    15:00). `ABDAgent`, um por palácio: nasce em `InitializePalace` numa
    célula ao lado (frente primeiro, livre antes de ocupada) e é
    destruído no `EndPlay` do palácio (venda, limpeza, fim).
  - **Patrulha:** anda 1–3 células numa direção aleatória
    (`Pistol_Walk`), para às vezes 1–3,5 s (`Pistol_Idle`, chance 0,4).
    Passa por torre, plataforma, palácio e creep; só o separador e o
    cenário fixo (`Blocked`) o barram. Sem colisão.
  - **Tiro:** raio de 3 células (esfera azul, `BD.Agent.ShowRange`).
    Creep no raio: para, vira, `Shooting`, tiro instantâneo com
    traçante, um alvo por vez, o mais próximo (`bTargetFurthestAlong`
    troca pelo mais adiantado). Não persegue. Pistola: 10 de dano a
    1,5 tiro/s.
  - **Barra de tempo** azul sobre a cabeça (`ABDMatchHUD`, Interface >
    Agent Bar). 120 s, corre sempre. Zerou: volta pra casa por busca em
    largura respeitando separadores, dorme deitado na célula ao lado do
    palácio (`Sleeping_Idle`) e só acorda quando termina a onda seguinte
    à que esgotou a barra (`SleepWaves` 1). Não defende nesse tempo.
  - **Abate:** +`KillBonusSeconds` na barra, sem teto. Array `Weapons`
    no `DA_PalaceData` com 6 entradas (nível 0 a 5): pistola feita, as
    outras 5 placeholder com os números da pistola e bônus 1,00 / 0,85 /
    0,70 / 0,55 / 0,40 / 0,25 s. O nível do palácio escolhe a arma.
  - **Chute pronto sem alvo:** `Kicking` toca uma vez e devolve o
    estado; `BD.Agent.Kick`. Também `BD.Agent.Status`, `SetTime`,
    `Wake`, e `BD.Match.AddMoney` para testar compras.
  - Tudo exposto no `DA_PalaceData` (categoria Agent), defaults no C++
    (o asset não precisou ser regravado).
  - Regressão 44/44 (novo: agente ao lado com barra cheia, tabela de
    armas, +1 s por abate, chute, barra zerada manda pra casa, venda leva
    o agente). Cenário headless: 10 abates em 3 ondas, dormiu na onda 2
    e acordou ao fim dela; capturas renderizadas conferidas.
  - **Onde paramos:** confirmar a leitura do bônus (−0,15 s por nível)
    e o array de 6 (o briefing diz 5). Ver na tela: escala do Mito
    (1,0 = 4 m em célula de 7 m), passo do andar, tempo de 120 s.
    Fora do commit: troca de texturas do STF (do usuário).

- **2026-10-02 (fecho da fatia 1) — 2ffdfa0** (desde 82033a6):
  - Briefing 2026-10-01 11:00. Preço 6.4 já tinha subido em 2f27fa6.
  - **Estrelas com fade por distância.** Opacidade cai suave (smoothstep)
    com a distância da câmera ao palácio: inteira até
    `PalaceStarFadeStart` 10000 cm, zero a partir de
    `PalaceStarFadeEnd` 18000 cm (Interface > Palace Stars). Visão geral
    (~33400 cm) limpa; contorno, sombra e preenchimento somem juntos.
    Visto em captura nas alturas 5000/11000/28000 e
    7000/12000/16000 (com 9000–15000 sumiam cedo demais; com
    14000–24000 as fileiras de palácios vizinhos ainda encostavam).
  - Regressão 40/40 (novo: curva 1 → 0,5 → 0 e estrelas sumidas na
    câmera com que a partida abre).
  - **Onde paramos:** fatia 1 fechada; falta o usuário ver no jogo.
    Próxima: fatia 2 (o Agente).

- **2026-10-02 — 2f27fa6** (desde a638189):
  - **Palácio do Governo, fatia 1 de 4** (briefing 2026-10-01 10:00):
    só a estrutura e o indicador. Novo `EBDPieceKind::Palace` (último do
    enum): peça `DA_Palace` 2×2, segura as células como plataforma
    (bloqueia o caminho, passa pela checagem de bloqueio), sem teto de
    quantidade, só dinheiro público; vende inteiro de qualquer célula.
    Na paleta depois do atirador; nome nos dois CSVs.
  - **Preço** em propinas de candidato da onda:
    `PalaceCostInCandidates` = 6.4 (Balanceamento > Price). 2560 na onda
    1; 3469 na onda 15 contra 3464 juntados sem gastar nada até o 3º
    candidato no Normal (2000 iniciais + propinas das ondas 5, 10, 15).
  - **`DA_PalaceData`** (`UBDPalaceData`): slot `Mesh` (vazio = cubo do
    motor como bloco provisório, ajustado ao 2×2 e apoiado no chão),
    `MeshScale`, `Level` 0–5 (sem efeito de jogo ainda), `StarsLift`.
    Ator `ABDPalace`: só o cursor o enxerga, unidades não colidem.
  - **5 estrelas** sobre cada palácio, desenhadas no canvas pelo
    `ABDMatchHUD`, sempre de frente para a câmera, tamanho por fração da
    tela (`PalaceStarHeightFraction` em Interface). Cheias até o nível,
    o resto só contorno; hoje todas vazias. `BD.Palace.SetLevel N` e
    `BD.Palace.Status` para testar.
  - `BD.Debug.Later <s> <comando>`: roda um comando N segundos depois,
    para capturar a tela renderizada com o mundo já carregado.
  - Regressão 39/39 (novo: preço e mais caro que tudo, 2×2 posto pelo
    preço, mesh ajustado e estrelas no nível do dado, venda libera as 4).
  - **Onde paramos:** fatia 1 vista em captura (blocos e estrelas cheias
    × vazias); falta o usuário ver no jogo. Fatias 2–4 (Agente,
    ministros, garrafa) não começadas.

- **2026-09-29 (noite) — bb1fd63** (desde 7ef679b):
  - **Zoom de perto.** `CloseUpHeight` 1500 cm em Camera > Movement: a
    roda desce abaixo do `MinHeight` 3000 (≈4 cliques a mais, jumento com
    o dobro do tamanho na tela) para ler estampas e detalhes. Pitch e giro
    ficam congelados no que têm em 3000, então de 3000 para cima tudo
    enquadra como antes. Igual ao `MinHeight` desliga.
  - **Skins da horda.** `UBDEnemyData::SkinMaterials` junta-se ao
    `MeshMaterial` num pool; cada creep sorteia um ao nascer. O pool é
    carregado uma vez no início da onda (`PreloadSkins`), nenhum spawn
    carrega nada; o corpo que cai copia o material do vivo. Mesmos
    números e mesma linha no placar de abates: skin é só aparência. Log
    `DA_...: N skin(s) in the pool` a cada início de onda.
  - `DA_Enemy_Test` com 4 skins: `MI_Jumento_PT` + `MI_Jumento_Gay`,
    `MI_Jumento_Hemp`, `MI_Jumento_Soci` (novas, pai
    `M_Master_Environment`, texturas `T_Jumento_<Skin>_Diffuse/Normal/
    Roughness`), preenchido por script Python headless.
  - Regressão 35/35 (novo: 200 sorteios vestem as 4 skins, deu
    53/46/52/49; o check do corpo animado aceita qualquer skin do pool).
  - **Onde paramos:** falta ver na tela a mistura de skins e o zoom de
    perto; a compressão das 9 texturas novas (Normalmap / Masks) o
    usuário confere no Texture Editor.

- **2026-09-29 (limpeza 2) — 6d0bc20** (desde 5c7a831):
  - Apagados os 4 `.uasset` de `Content/imgs/` (`T_UI_Urn`,
    `T_UI_Vote_Blue/Null/Red`): segunda importação dos mesmos PNGs,
    sem nenhuma referência em mapa, asset, config ou código. O placar usa
    os de `/Game/BD/UI/`. Os PNGs ficam em `Content/imgs/`: são a única
    cópia da arte original no repositório (os de `BD/UI` foram importados
    de `Downloads`).

- **2026-09-29 (limpeza) — 857cbf9** (desde 28007c8):
  - Apagado `Content/Docs/Brazil_Defense.log`, o rascunho antigo do PLANO
    subido em 6e5a7af: o PLANO é este arquivo, e uma cópia velha só
    confundia buscas.

- **2026-09-29 (resto) — 6e5a7af** (desde 908f5ca), a pedido do
  usuário, o que vinha ficando fora dos commits:
  - `Content/imgs/T_UI_Urn`, `T_UI_Vote_Blue`, `T_UI_Vote_Null`,
    `T_UI_Vote_Red` (14/09, ~3,3 MB cada, LFS). O placar carrega os de
    `/Game/BD/UI/`, que já estavam no repositório.
  - `Content/Docs/Brazil_Defense.log` (14/09): apesar do nome, é um
    rascunho antigo do PLANO (20 KB), não log do jogo.
  - `Config/DefaultEditor.ini`: os perfis de preview "Epic Headquarters" e
    "Grey Wireframe" que o editor regravou.
  - Árvore limpa depois deste push.

- **2026-09-29 (sons) — 0561dfa** (desde 2acda79):
  - Os 43 sons do jumento e do militante no estado em que o editor os
    salvou em 28/09 às 13:55, quatro minutos depois de 336a837: 18
    `S_Jumento_Zurro/Casco/Morte/Tombo_NN` e 25 `VO_Militante_01..25`. É o
    que o jogo carrega e o que o usuário ouviu e aprovou; o repositório
    guardava a versão de antes desse save.
  - Continuam fora: `Content/imgs/`, `Content/Docs/Brazil_Defense.log`,
    `Config/DefaultEditor.ini`.

- **2026-09-29 — 14958f5** (desde 498b0fd):
  - **Esfera de alcance.** `BD.Tower.ShowRange` agora vem ligado: esfera
    (16 segmentos) centrada no plano sob cada defensor, com o círculo no
    equador (onde o creep entra no alcance, o combate mede no plano), um
    círculo amarelo do alcance do próximo nível quando ele vai mais longe e
    o rótulo `L2  4.0 cells (next 4.5)`. Cores e segmentos em Towers > Debug.
  - **Alcance cresce por nível, como o dano.** `RangeGrowthPerLevel` 0,10
    (nível 5 = 1,4x) em Balance > Upgrades; nível autorado no `Levels` do
    DataAsset vale o próprio alcance, a fórmula cobre os demais; plataforma
    multiplica por cima. Medido: 3,75 → 4,12 células no chão (1→2), 4,40 →
    4,80 na plataforma (2→3). As rodadas da §13 foram medidas sem isso.
  - Painel do defensor mostra o alcance do próximo nível
    (`HUD.Defender.Upgrade` com `{NextRange}`, pt e en); `EVOLVED` e a
    descrição da evolução trazem alcance antes → depois.
  - Regressão 34/34 (novo: EVOLUCAO, alcance prometido = alcance obtido e
    cresce pela fórmula).
  - Fora do commit: `Content/audio/` (43 .uasset modificados que não são
    desta leva), `Content/imgs/`, `Content/Docs/Brazil_Defense.log`,
    `Config/DefaultEditor.ini`.
  - **Onde paramos:** falta ver na tela a esfera, o rótulo e o painel; o
    0,10 de crescimento é chute inicial para o usuário ajustar.

- **2026-09-28 (noite) — 336a837** (desde 735d0e1):
  - **Casco mudo era bug do recorte.** Os `SFX_Jumento_Casco` de 662d262
    eram silêncio puro (no ffmpeg o `-ss` veio depois do `-i` e o fade-out
    zerou o clipe). Refeitos como `S_Jumento_Casco_01/02` com compressão e
    ganho (pico -2,4 dBFS). Casco agora com volume 1, alcance 8000+22000 cm
    e teto 12 cortando o mais distante. Com render real (`-RenderOffScreen`,
    sem CVar de debug): 4.935 passos em 30 s.
  - **Vozes 60/40.** `SpeechShare` 0,6. As 25 falas foram normalizadas
    (loudnorm -11 LUFS, pico máx -1,1 dBFS): ≈+8 dB em média, sem estourar;
    dobrar por volume passaria de 0 dBFS. `SpeechVolume` 1,0 e
    `CallVolume` 0,7 em Creep Sound para fechar o equilíbrio de ouvido.
  - **Morte em três camadas.** Grito (`SCue_Jumento_Morte`, 8 sons, teto 5
    farthest-first), queda (`ABDCreepCorpse`: corpo só visual, sem
    colisão, creep destruído no instante do abate, uma de 4 quedas
    `A_Jumento_Death_01..04` a 1,5x, afunda 0,4 s, no máximo 40 deitados)
    e baque (`SCue_Jumento_Tombo`, 4 sons, `UBDAnimNotify_BodyFall` no
    quadro do impacto de cada queda: 27, 33, 64, 68; teto 5). KillAll de
    189: 40 caem, 149 afundam na hora, chão limpo em ~3 s; 189 gritos e
    20 baques pedidos, 5 de cada no máximo.
  - **Urna (diagnóstico):** o som só toca em `AddVotesRed` vindo de
    `NotifyEnemyArrived`; nivelamento e load não tocam. 292 chegadas
    medidas, todas a ≤40 cm da urna. Nada mudado; o log Verbose de chegada
    agora traz a distância.
  - **Nomes limpos.** Sons do jumento em `/Game/audio/Jumento/`
    (`S_Jumento_Zurro/Casco/Morte/Tombo_NN`); originais em
    `SourceAudio/Jumento_originais` e `SourceAudio/Urna`. Animações
    `A_Jumento_Run` e `A_Jumento_Death_01..04`; malha, esqueleto e physics
    viraram `SKM_Jumento`, `SKEL_Jumento`, `PHYS_Jumento` (em `Mesh/`) e a
    estática sem uso `SM_Jumento`, renomeados pela ferramenta do editor,
    referências conferidas numa sessão nova. Resta o texto das opções de
    importação FBX das quedas citando o esqueleto antigo (só vale para
    reimportar).
  - `BD.Sound.Stats` conta mortes, baques e corpos; `BD.Sound.LogSteps`.
    Regressão 33/33 (novos: queda sem colisão e creep fora na hora; cada
    queda com baque).
  - Fora do commit: `Content/imgs/`, `Content/Docs/Brazil_Defense.log`,
    `Config/DefaultEditor.ini`.
  - **Onde paramos (fim do dia 28, noite):**
    - Som do jumento aprovado pelo usuário ("ficou demais"): falas,
      zurros, casco, grito, queda e baque. Os números (volumes, alcances,
      tetos em Creep Sound; queda e corpos em Waves > Death) ficam como
      estão até alguém ouvir algo a mudar.
    - Se reimportar as quedas do FBX, escolher `SKEL_Jumento` na janela:
      as opções guardadas ainda citam o esqueleto antigo.
    - Os `S_Jumento_Tombo` agora são o baque; não sobrou som solto sem uso
      na pasta do jumento.
    - Continuam abertas as checagens na tela do dia 25 (pausa, nomes no
      placar, urna, candidatos em grupo, memória numa partida longa), se
      ainda não foram vistas.

- **2026-09-28 — 662d262** (desde 3e27feb):
  - **Som dos militantes (briefing 09-26).** Três Sound Cues em
    `/Game/BD/Audio/`, todos Random sem repetição → Modulator → SC_Effects:
    `SCue_Militante_Falas` (25 falas `VO_Militante_01..25`, ±5% de pitch;
    reutilizável em outro personagem pelo Pitch Multiplier do Cue ou pelo
    `VoicePitch` do data asset), `SCue_Jumento_Efeitos` (4 zurros `ANML*`,
    ±5%) e `SCue_Jumento_Casco` (2 batidas recortadas do `ANMLHors`, ±6%
    de pitch, até -15% de volume).
  - **Falas em lote:** 25 MP3 da ElevenLabs → WAV 16-bit 44,1 kHz mono em
    `Content/audio/Militante/`, importados; os MP3 originais em
    `SourceAudio/Militante_mp3/` (fora do Content, para o editor não
    reimportar).
  - **Vocalização por creep:** `UBDEnemyData` ganhou SpeechSound,
    CallSound, SpeechShare (0,3), VoicePitch, VocalIntervalMin/Max (8/20 s)
    e FootstepSound; `ABDEnemyBase::UpdateVocal` sorteia o próximo
    intervalo. `UBDCreepSoundSubsystem` toca tudo em 3D com o listener na
    câmera; teto global de vozes (`VoiceMaxConcurrent` = 5 no
    `DefaultGame.ini`), cortando a mais distante e, empatado, a mais
    antiga (pedido do usuário). Além do alcance (voz 320 m, casco 160 m)
    o som é mudo e nem é disparado. Ajustes em Project Settings > Brazil
    Defense - Creep Sound (`UBDCreepSoundSettings`).
  - **Casco por Anim Notify:** `UBDAnimNotify_Footstep` ("BD Footstep")
    nos quadros 9 e 23 de 27 do Run_Forward (contatos medidos pelos ossos
    dos pés; o rig é bípede). Concorrência própria de 10, cortando a mais
    antiga.
  - **Ferramentas:** commandlet `-run=BDBuildSoundCue` (monta ou refaz um
    Cue a partir de uma pasta de waves); `BD.Sound.Stats [reset]`,
    `BD.Sound.Mute`, CVar `BD.Enemy.AlwaysTickPose` para testar headless.
    Regressão ganhou o check SOM: 32/32.
  - **Medição** (≈200 jumentos, câmera perto da urna, 60 s, teto 5):
    30% do tempo com 5 vozes, 38% com 4, 21% com 3; picos de 6–7 somam
    menos de 0,5 s (sobreposição de quadro no corte). Com zoom afastado
    só ~15% das vozes ficam ao alcance e nenhum casco.
  - Fora do commit: `Content/imgs/`, `Content/Docs/Brazil_Defense.log`,
    `Config/DefaultEditor.ini`.
  - **Onde paramos (fim do dia 28):**
    - Ouvir no `.exe`: volume das falas e zurros, alcance (perto claro,
      zoom afastado quase mudo), proporção de 30% de fala e se o teto de
      5 vozes soa como multidão. Todos os testes rodaram mudos.
    - Casco: ver se os dois passos por ciclo (quadros 9 e 23) batem com a
      pata na tela e se o volume (0,6) não vira metralhadora com a horda
      perto. O rig é bípede; se quiser quatro batidas, marcar mais dois
      notifies "BD Footstep" no Run_Forward.
    - Decisões minhas ainda não confirmadas: além do alcance o som nem é
      disparado (voz 320 m, casco 160 m); o casco tem teto próprio de 10
      e corta o mais antigo; a fala continua se o jumento morre no meio.
    - Vieram 25 falas, não 24; todas entraram no Cue. Se alguma sobrou,
      é só apagar e refazer o Cue com `-run=BDBuildSoundCue`.
    - Continuam abertas as checagens na tela do dia 25 (pausa, nomes no
      placar, urna, candidatos em grupo, memória numa partida longa), se
      ainda não foram vistas.

- **2026-09-25 (noite) — ab31991** (desde 00a3b8f):
  - **Nome do tipo no placar de abates.** Embaixo do ícone de cada linha
    vai o nome do tipo: `BDLoc::EnemyName` lê `Enemy.<asset>` da tabela
    de texto e, na falta, o `DisplayName` do `UBDEnemyData`.
    `DA_Enemy_Test` = "Militantes" (tabela: Militantes / Militants); a
    linha dos candidatos diz "Candidatos" (`HUD.Kills.Candidates`).
    Atualiza na troca de idioma.
  - **Pausa de jogo.** Botão "II Pausar (P)" antes das velocidades e a
    tecla P. Congela ondas, creeps, animação e timers (`SetGamePaused`),
    com o HUD de pé e a linha "PAUSADO - olhe e planeje, P continua".
    Câmera, teclas, hover, fantasma e colocação/venda continuam valendo
    durante a pausa (bindings `bExecuteWhenPaused`, ações com
    `bTriggerWhenPaused`, `PlacementComponent` com tick em pausa).
    Decisão minha: com o jogo pausado dá para construir, não só olhar.
    O menu (Esc) aberto por cima de uma pausa fecha de volta pausado.
    `BD.UI.GamePause [0|1]` para testes.
  - **Urna respeita o giro do preview.** `PlaceObjective` recebe o yaw
    do preview (antes a urna ficava com a rotação que tinha); o save
    guarda o giro da urna.
  - **Candidatos em grupo no endless.** Numa onda de candidato saem
    `1 + floor(mortos / CandidateGroupStep)` juntos (passo 20, em
    Balance > Candidate e no `DefaultGame.ini`), cada um de uma boca
    diferente; sem bocas bastantes, espalhados o mais possível.
  - **Spam de áudio corrigido.** O `SMix_Settings` e as três
    `SC_*` eram carregados por ponteiro fraco e coletados na troca de
    mapa; o mix seguia citando classes desregistradas e o motor escrevia
    `RecursiveApplyAdjuster failed` a cada frame (7,7 milhões de linhas,
    98% do log de 971 MB da partida da onda 150). Agora o
    `UBDSettingsSubsystem` segura os quatro. Testado: 0 linhas.
  - **Memória no WaveLog.** Colunas `MemUsedMB`, `MemPeakMB`,
    `UObjects`, `Actors` no fim de cada onda, mais a linha
    `Wave N memory:` no log. `BD.Enemy.StaticBodyOnly 1` põe o
    cilindro no lugar do jumento (só para medir). Ver a Rodada 5 no §13.
  - **Diagnósticos (sem mudança):** o ponto vermelho do separador era
    `WouldBlockPath` correto (a aresta 46,10|+X era a última entrada da
    urna em 47,10, na borda leste); votos por HP, não por corpo (azul =
    10 × escala de HP por abate, números da onda 85 batem exatos).
  - Regressão 31/31 (novos: giro da urna, candidatos em grupo). Os dois
    alvos compilados.
  - Continuam fora: `Content/imgs/`, `Content/Docs/Brazil_Defense.log`
    e o `DefaultEditor.ini`.
  - **Onde paramos (fim do dia 25):**
    - Ver na tela, no `.exe`: botão de pausa e tecla P, nomes no placar,
      giro da urna, e se a pausa deve permitir construir ou só olhar
      (hoje permite; decisão ainda não confirmada).
    - Confirmar a memória com renderização: numa partida longa na tela,
      olhar `MemUsedMB` no WaveLog e ver se estabiliza como na Rodada 5.
    - O `BD.Sim.Run` para em 1 hora real (`STALLED`): 100 ondas não
      cabem nesse teto. Aumentar o teto se precisar medir até o fim.
    - Candidatos em grupo no endless: ver na tela a "parede" de 2 ou
      mais juntos, junto com o retorno dos caídos.

- **2026-09-25 (fim de tarde) — e470d84** (desde 89d375e):
  - **Referências do jumento versionadas.** `img_references/` ganhou
    `Jumento_PT`, `Jumento_Gay`, `Jumento_Hemp` e `Jumento_Socialista`
    (as imagens-fonte das skins), para poder regerar os assets. Entram
    como git comum, igual às quatro referências que já estavam lá
    (`.jpeg` não está no LFS). Sem mudança de código.
  - Continuam fora: `Content/imgs/` (cópias soltas),
    `Content/Docs/Brazil_Defense.log` e o `DefaultEditor.ini`.

- **2026-09-25 (tarde) — 9c7af93** (desde 48b50a2):
  - **Placar de abates por tipo, nas laterais do HUD.** Esquerda: um
    ícone por tipo de inimigo com o total de abates; direita: o
    candidato placeholder com quantos dos 20 agendados já caíram
    (`GetFallenCount`, só o número). Cada linha só aparece no primeiro
    abate do tipo e a lista cresce para baixo. Pulso por linha com o
    mesmo `FBDPulse` dos votos, com a trava de rajada. Ícone por fração
    da altura da tela (`KillIconHeightFraction`).
  - A chave é o tipo de gameplay, não a skin: `UBDEnemyData` ganhou
    `KillType` e `KillIcon` (`DA_Enemy_Test` = "Jumento" +
    `T_UI_Jumento`); as skins do jumento devem repetir o mesmo
    `KillType`. A contagem vive em `FBDMatchCombatTotals::KillsByType`
    e zera com a partida.
  - Novo `T_UI_Candidate_Placeholder` (busto genérico, UI/UserInterface2D)
    em `CandidateKillIcon`; `CandidateFaces` já existe nos settings para
    os 20 rostos, ainda não lido.
  - `BD.Test.Regression`: 29/29 PASS. Capturas renderizadas em 1080p,
    1440p e 4K (limitado a 3840x1592 pelo monitor). Os dois alvos
    compilados.
  - Fora do commit: `img_references/`, `Content/imgs/`,
    `Content/Docs/Brazil_Defense.log` e o `DefaultEditor.ini`.

- **2026-09-25 — 3fb5007** (desde 26e5d8b):
  - **O creep virou o jumento animado.** SK `Run_Forward__1_` (Mixamo,
    reimportado com escala 100, In Place, normais importadas) com o
    `MI_Jumento_PT`, e `MeshScale` 2 no `DA_Enemy_Test` (~400 cm, pelo
    cenário). `UBDEnemyData` ganhou `SkeletalMesh`, `MoveAnimation` e
    `MeshYaw` (-90: o Mixamo olha para +Y); `MeshMaterial` vale para o
    skeletal, e as três variantes serão três DataAssets com o mesmo SK.
    O `ABDEnemyBase` toca o loop em single node, sem Anim BP, com
    PlayRate = velocidade / `AnimReferenceSpeed` (554 cm/s na escala 2,
    Project Settings > Waves), sem root motion. O corpo não tem
    colisão; o projétil mira o centro de qualquer um dos dois corpos.
    `CreepBarWorldHeight` foi para 450. `M_Master_Environment` ganhou
    `Used with Skeletal Mesh`.
  - **Performance da horda.** Plugin `AnimationBudgetAllocator` ligado,
    `a.Budget.Enabled=1`, significância pela distância da câmera até
    40.000 cm; fora da tela não calcula pose. Significance Manager e
    skeletal instanciado ficaram de fora.
  - **Os ônibus seguem as bocas.** `UBDBusSubsystem` liga cada
    `StaticMeshActor` com `SM_Bus` à boca cuja âncora está debaixo
    dele, uma vez, e o desliza junto com ela ao longo da borda, sem
    girar, em `BusMoveSeconds` (2 s). A horda e o candidato esperam os
    ônibus estacionarem; o candidato mantém os 5 s de vantagem sobre a
    horda. `MinBusGap` (2 células, carroceria a carroceria) trava
    qualquer par de ônibus, mesma borda ou canto. O vaguear das bocas
    não mudou. Ganchos de som marcados com `// SOUND:`.
  - Consequência para o balanço: onda com ônibus em movimento solta a
    horda 2 s depois; a do candidato, 7 s em vez de 5.
  - `BD.Test.Regression`: 27/27 PASS (jumento, ritmo, ônibus, folga,
    candidato esperando). Estresse de 40 ondas: pior folga entre
    ônibus 2,05 células. Os dois alvos compilados.
  - Ficou de fora do commit, como antes: `Content/imgs/`,
    `Content/Docs/Brazil_Defense.log` e os perfis de preview do
    `DefaultEditor.ini`. O `Mesh/Jumento` estático (Meshy) foi junto
    mas não é usado.

- **2026-09-24 (23h45) — bffb831** (desde 383be2a):
  - **Votos iniciais por dificuldade.** `StartingVotes` (a vantagem de
    largada no placar, não é moeda) valia 3.000 nas três e agora cai
    com a dificuldade: Easy 3.000, Normal 1.500, Hard 0. Gravado nos
    `DA_Difficulty` pelo commandlet `BDSetProperty` e conferido numa
    sessão nova. `ChainBonus.Votes` (+100 por ter vencido a
    dificuldade abaixo) continua separado: no Hard, quem venceu o
    Normal começa com 100, quem entra direto começa em 0. O vermelho
    continua em 0. `StartingFunds` (2.000) não mudou.
  - A regressão mostra o Normal abrindo com 1.600 (1.500 + 100 do
    bônus): 22/22 PASS. Os dois alvos compilados (sem mudança de
    código).

- **2026-09-24 (23h) — 12c40da** (desde d7d5ff4):
  - O briefing das 23:20 pediu de novo a remoção do "blue count". O
    código já estava certo desde 316f027, mas o `Brazil_Defense.exe`
    era de 23/09 22:54: só o alvo do editor tinha sido compilado. A
    última partida no `PostMatch.csv` confirma, rodada com o build
    2026.09.23-2253. O alvo do jogo foi recompilado (exe de 24/09
    23:17), sem mudança de código.
  - §9 ganhou a nota do warning de exposição do Lumen, para quando for
    mexer em iluminação.
  - `BD.Test.Regression`: 22/22 PASS.

- **2026-09-24 — 316f027** (desde f5bb309):
  - **HUD sem placar repetido.** Saiu a linha "blue count" com o ícone
    de urna do meio: repetia o azul da barra do topo e sobrou da época
    em que voto era moeda. A linha do meio agora é só o PUBLIC FUNDS
    (com o ladrão enquanto há propina chegando). A chave
    `HUD.Money.Votes` saiu dos CSVs de texto.
  - **Log por onda.** `Saved/Logs/WaveLog.csv`, uma linha por onda
    vencida ou perdida em andamento: placar e deltas, fundos
    ganhos/gastos/devolvidos/concedidos, `FundsGap`, separadores na mão,
    tabuleiro, nível médio, creeps da onda, candidato (saiu/morreu/
    chegou), dano desperdiçado, pico de vivos, rota mais curta e mais
    longa. Cada linha cobre a montagem antes da onda mais a onda;
    `MatchStart` agrupa as linhas de uma partida e `Mode` separa
    Screen, Headless e Sim. A linha `Wave N cleared. Votes:` do log
    virou o resumo de uma linha da onda, com o mesmo começo.
    `BD.WaveLog.Enabled` (padrão 1).
  - PostMatch e WaveLog passam a dividir a escrita do CSV e a contagem
    do tabuleiro (`BDReportCsv`); o PostMatch continua escrevendo igual.
  - `BD.Test.Regression` ganhou o check RELATORIO (a onda final tem
    linha e o dinheiro dela fecha): 22/22 PASS.
  - Rodada 4 na §13: primeira corrida com WaveLog (seed 101, 30 ondas).
  - **Pendente:** ver o HUD novo na tela; o warning de exposição do
    Lumen (Exposure -8,5, fora da faixa segura) fica para quando for
    mexer em iluminação.

- **2026-09-23 (23h) — 9387753** (desde d50c721):
  - **Build visível.** Rodapé do menu e canto inferior esquerdo do HUD
    mostram "build AAAA.MM.DD-HHMM Configuração", lido da data do
    próprio binário (DLL do módulo no editor, executável no jogo) —
    muda sozinho a cada compilação. Vai também no log de início de
    partida e numa coluna `Build` no fim do `PostMatch.csv`; colunas
    novas no fim agora só completam as linhas antigas (sem arquivar).
  - **`BD.Test.Regression [quit]`** — o termômetro. Numa partida nova
    monta urna, cercas, torre, palanque e onda de chefe, e dá PASS/FAIL
    por linha: rotas de toda boca; cercar a urna recusado; separador
    sem custo; personagem sem vaga recusado; construir/evoluir debita
    fundos e não votos; bloco da plataforma (trava, anda junto, altura
    = um andar por nível, atiradores sobem juntos); candidato sai
    primeiro; construir recusado na onda e evoluir por clique aceito;
    chefe paga a propina, desfile não paga; LedgerGap 0; azul só desce
    pelo nivelamento; candidato na urna = derrota. 21/21 PASS. Rodar
    antes de dar qualquer leva por pronta.
  - **Regra confirmada (briefing 23:00):** nenhuma peça nova entra
    durante a onda — torre e personagem também só se constroem na
    montagem (antes podiam descer com a onda rodando); evoluir segue
    livre na onda. Travado no `BD.Test.Regression`: toda peça da
    paleta recusada pela barra e pelo gesto com a onda ativa, e a
    evolução por clique aceita.
  - **Mais separadores no início:** DividerBudget Easy 40→100,
    Normal 28→80, Hard 18→60 (placeholder; pela rodada 3 da §13, ~100
    cercas alongam a rota em ~40%). Gravado pelo commandlet novo
    `-run=BDSetProperty -Asset=... -Property=... [-Value=...]`, que lê
    e grava um campo de asset sem abrir o editor.
  - **Rotação da peça no clique do meio.** O `IA_Rotate` nunca foi
    remapeado (seguia na roda, `Axis1D`, e sem ligação no código desde
    14/09). Agora o controller trata o botão do meio: clique parado
    com peça na mão gira a peça; arrastar segue girando a câmera
    (`MiddleClickMaxTravel`, 6 px). R / Shift+R continuam.
  - **Pendente:** conferir no PIE a rotação no clique do meio e o
    rótulo de build no menu e no HUD; `IA_Rotate`/`IMC_Gameplay` ficam
    como estão (mapeamento morto) até o usuário decidir limpar.

- **2026-09-23 (22h) — 681ee89** (desde 4e7d646):
  - **Separador com cota própria.** Divisória custa 0 de dinheiro
    público: desenhar o caminho não compete mais com defendê-lo. A
    cota começa em `DividerBudget` e cresce: +`DividersPerWave` (1) a
    cada onda limpa e +`DividersPerCandidate` + `Step` × (n − 1) pelo
    n-ésimo chefe morto (4, 5, 6…). Tudo no DA_Difficulty. Projeção:
    83 na onda 25, 163 na 50, 398 na 100. HUD mostra "N na cota".
  - **Barra do candidato só após o primeiro dano** (flutuante e no
    painel), como a dos creeps; o nome aparece desde a saída.
  - **Evolução livre durante a onda.** O clique no defensor passava
    pelo "levantar para mover", que só vale na montagem; na onda o
    clique agora seleciona e o segundo compra o nível.
  - **Personagem sem vaga não é oferecido.** Barra e teclas 1–6 usam a
    mesma regra (`GetHandRefusal`/`TakeIntoHand`); texto "Nenhuma
    plataforma com vaga livre".
  - Relatório de fim de partida: colunas `DividersGranted` e
    `DividersInHand`; separador conta como crescimento da defesa.
    As 2 partidas reais do dia foram migradas para o cabeçalho novo.
  - Robô: `FenceShare` virou fatia da cota de separadores (0,75),
    `GrowMaze` põe os ganhos entre ondas. Comandos `BD.Place.Take` e
    `BD.Place.Click`. Seção 8 ganhou a questão aberta da duração.
  - **Pendente:** conferir no PIE a barra do candidato e a evolução
    por clique durante a onda.

- **2026-09-23 (noite) — 2cdeaa6** (desde b1b73c0):
  - **Relatório de fim de partida.** Toda partida que lançou ao menos
    uma onda grava uma linha em `Saved/Logs/PostMatch.csv` e um resumo
    `POST-MATCH` no LogBDMatch: vitória, derrota, abandono (sair ou
    carregar um save por cima) e o limite de ondas do `BD.Sim.Run`.
    53 colunas: identificação (modo Screen/Headless/Sim, seed,
    dificuldade, resultado, motivo, onda), placar, economia (capital,
    ganho dos chefes, reembolsos, gasto em construção/evolução/mover,
    sobra, `LedgerGap` que deve ser 0), defesa por tipo e por peça da
    paleta, níveis, combate (creeps, candidatos, dano, desperdício,
    pico) e ritmo (tempo real/de jogo, última onda em que a defesa
    cresceu). Colunas mudaram → o CSV antigo fica guardado com data.
    CVar `BD.PostMatch.Enabled`.
  - Para isso: livro-caixa `FBDMatchLedger` na partida (todo gasto diz
    se foi construção, evolução ou mover; reembolso e devolução de
    cobrança recusada separados) e totais de combate da partida no
    subsistema de ondas.
  - Verificado: sim de 30 ondas deu ganho 3493 = soma das quedas do
    `BD.Economy.Report`, caixa fechando em 0; derrota e abandono
    também geram linha.
  - **Pendente:** ver a linha de uma partida jogada na tela (PIE).

- **2026-09-23 — 06078e4** (desde 97535c5):
  - **AutoSetup estratégico** (de 2026-09-15, rodada 3 da seção 13):
    urna -> labirinto -> defensores. `BuildMaze` cerca três lados da urna
    e serpenteia a rota mais longa; `BuildCorridorTargets` pontua cada
    célula pela rota ao alcance. Cvars `FenceShare`, `MazeStride`,
    `UrnRing`, `FocusUrn`, `ExtraDividers`. 4 de 4 seeds vencem a 100.
  - **Só votos e propina.** Só o labirinto é contado agora: divisória,
    plataforma e urna. Torre e personagem não têm teto — votos e espaço
    decidem. `TowerBudget`/`CharacterBudget` saíram da dificuldade, do
    bônus em cadeia, do match manager e do save; o chefe agendado paga
    só em propina (o aviso "orçamento aumentado" saiu do HUD). Vagas de
    personagem são contadas no tabuleiro na hora (`CountFreeSlots`),
    com recusa nova `NoFreeSlot`. Restaurar um save não cobra votos.
    `ReferenceTowerCount`/`ReferenceCharacterCount` (8/12) ficam só
    para os relatórios. O resumo da dificuldade mostra os votos de
    partida no lugar das torres e personagens.
  - **HUD: dois medidores.** VOTOS com a urna e DINHEIRO PÚBLICO com a
    Casa da Moeda, sempre visíveis; o ladrão e a seta só aparecem entre
    eles enquanto ele carrega propina.
  - **Custo antes de agir.** A barra de construção mostra "restante ·
    custo" ou, quando a peça não pode ser pega, o motivo em vermelho.
    Peça na mão mostra o custo e o placar resultante (vermelho se não
    paga ou se o vermelho passaria na frente). Evolução sem dinheiro
    diz quanto falta.
  - **Reforma da economia (mesmo dia, substitui "votos como moeda"
    acima).** Votos viraram placar puro: nada os gasta
    (`SpendVotesBlue` e o aviso de inversão saíram). Toda peça,
    evolução e movimento se paga em DINHEIRO PÚBLICO. Preço de uma
    peça = fundos que um candidato daquela onda solta ×
    `ReplacementCostRatio` × custo-base / `ReplacementReferenceCost`
    (1,0 e 100): a defesa de referência custa um chefe em toda onda
    (436 na 5, 1161 na 50, 3449 na 100). A evolução sobe com a mesma
    escala (`GetPriceScale`). Venda e mover usam o preço PAGO
    (`PaidCost`), e a torre guarda o que gastou em níveis
    (`EvolutionSpent`); os dois vão no save (-1 em save antigo, com
    fallback). `StartingFunds` (2000) e `ChainBonus.Funds` (400) na
    dificuldade; `StartingVotes` ficou só como vantagem no placar.
    Divisória e plataforma podem ser construídas entre ondas (só a
    urna trava na onda 1). Desbloqueio por peça em Placement settings
    (`Unlocks`: caminhão na 10, arquibancada na 25), com recusa
    `NotUnlocked` e "Libera após a onda N" na barra. O candidato
    agendado sai PRIMEIRO na onda dele: `OnWaveDealt` da onda +
    `HoldWaveSpawns(CandidateLeadSeconds = 5)`. HUD: dinheiro público
    lidera a linha, maior; votos viram "placar azul", pequenos.
    `BD.Economy.Report` novo: tabuleiro cheio + evolução total = 139%
    do que a partida paga, então tem que escolher. AutoSetup gasta só
    uma fatia em plataformas (`PlatformShare`).
  - **Pendente:** conferir o HUD no PIE (só verificado headless);
    `Content/imgs/T_UI_*` são cópias sem commit, à espera de decisão;
    os próximos passos da rodada 3 seguem abertos; `StartingFunds` não
    está nos assets DA_Difficulty (vale o padrão); evolução custa
    0,14 de um chefe por nível — `UpgradeCostBase` é calibração; save
    antigo carrega com 0 de dinheiro público.

- **2026-09-15 — 82f4755** (desde 1319754):
  - **Moeda nova: a propina (seção 3).** Candidato AGENDADO morto solta
    propina = `floor(MaxHealth / HealthPerBribe)`; o contador do ladrão
    a recebe e a Casa da Moeda a converte em dinheiro público, que é a
    ÚNICA moeda da evolução. Votos não são mais tocados por upgrade.
    Candidato do desfile não solta nada, mesma regra do orçamento.
    Zera na vitória e na derrota; não passa de partida para partida.
    `UBDBribeSubsystem` conduz a sequência (mala quica com som a cada
    toque, contador sobe, caixa registradora, transferência), `ABDMoneyBag`
    é a mala. Slots de som e mesh em Project Settings > Brazil Defense -
    Bribe, vazios até a arte chegar — a sequência roda muda sem eles.
  - **Evolução até o nível 5** (`UBDTowerData::MaxLevels` 10 -> 5) e
    remoção devolve 60% do que os níveis custaram (`EvolutionRefundRatio`),
    inclusive quando a peça sai junto com a plataforma.
  - **HUD:** dois contadores novos sob a barra de contagem, com os
    ícones do ladrão e da Casa da Moeda, e a seta acesa enquanto algo
    atravessa. Textos em pt e en.
  - **Plataformas sobem em bloco (seção 6).** Personagens de uma
    plataforma evoluem em passo: todos os slots preenchidos, e só quem
    está no nível mais baixo compra o próximo. A cada nível fechado a
    plataforma ganha um andar — nível 1 é a altura base, nível 5 são
    cinco andares. Puramente visual: alcance e mira são medidos do
    slot (`DistSquared2D`), então subir não muda parâmetro nenhum.
  - **Teto de personagem acompanha as plataformas.** Cada plataforma
    construída leva seus slots para o `CharacterBudget` e os tira ao ser
    vendida. Sem isso a trava de bloco era inalcançável: 54 slots contra
    ~32 personagens congelavam TODOS os personagens no nível 1 para
    sempre (medido na rodada 2).
  - **Construir cobra votos (seção 3).** `BuildCost` debitado ao colocar
    qualquer peça, com recusa `NoVotes` e preview vermelho. Capital
    inicial novo por dificuldade (`StartingVotes`, default 3000; Easy e
    Hard ainda a definir no editor). Antes só a venda lia `BuildCost` —
    colocar e vender era fábrica de votos.
  - **Correção:** o bônus em cadeia fazia `VotesBlue = Bonus.Votes`
    (atribuição) e passou a sobrescrever o capital inicial; virou `+=`.
  - **Calibração (seção 13).** `HealthScaleGrowth` 1.035 -> 1.022,
    `HealthPerBribe` 2.5 -> 1.0, `RedVoteWeight` novo (1.0, neutro).
    Com isso a onda 100 deixou de ser matematicamente impossível: 3 de
    4 seeds vencem. Duas rodadas de medição registradas na seção 13.
  - **Ferramentas de medição:** `BD.Sim.Run` (joga a partida inteira
    sozinha e reporta em linhas `SIM`), `BD.Debug.AutoEvolve`,
    `BD.Bribe.Report/Status/Grant/Flush`, e o AutoSetup passou a
    construir o orçamento que os chefes liberam.
  - **Pendente:** o candidato é o gargalo real (decide 5 de 5 derrotas
    e depende da rota sorteada); o vermelho continua ~0 numa partida
    vencida e não há escala que resolva; construir só morde na abertura.

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

---

## 13. Medições de balanceamento

Resultados de simulação headless. Esta seção guarda o que foi MEDIDO,
com data e como reproduzir; as curvas em si continuam na seção 7 e as
decisões de ajuste são anotadas aqui quando tomadas.

### Rodada 1 — 2026-09-15

**Como reproduzir.** `BD.Sim.Run <seed> 100 <evoluir 0|1> 4 1` numa
corrida headless (`-game -nullrhi -BDSkipFrontEnd`). O driver monta a
defesa AutoSetup da seed, chama as ondas em sequência a 4x e, quando
evoluir=1, gasta a propina entre ondas no upgrade mais barato
disponível (`BD.Debug.AutoEvolve`). Cada corrida escreve linhas `SIM`.
8 corridas: seeds 101, 202, 303, 404 × com e sem evolução.

**Cenário.** Defesa AutoSetup = 23 defensores (9 plataformas, 14
personagens, 9 torres, 10 cercas). Bônus em cadeia do Easy ativo
(+1 torre, +2 personagens, +1 plataforma, +100 votos). Valores da
época: HealthScaleGrowth 1.035, HealthPerBribe 2.5,
CandidateHealthMultiplier 40, UpgradeCostGrowth 1.35.

**1. Progressão da propina.** 1ª evolução na onda 7 (onda 9 numa
seed). Tabuleiro real de 23 defensores ao nível 5 custa 8.234 de
dinheiro público. Propina acumulada cobre 16% na onda 25, 54% na 50,
145% na 75, 251% na 90 — 100% por volta da onda 65-70. Nível médio
realmente atingido: 1.65 (onda 25), 2.22 (40), 2.57 (50); os níveis
caros vêm por último, então 54% do dinheiro não compra metade dos
níveis. Contra o tabuleiro real a propina NÃO está apertada.

**2. Disputa do placar.** Azul × vermelho: 30.196 × 156 na onda 25
(194:1), 228.332 × 6.548 na onda 50 (35:1). O vermelho nunca encosta;
a vantagem do azul é irreversível desde a onda 1. Nenhuma das 8
derrotas foi por contagem — todas foram candidato chegando na urna.
O placar hoje não decide partida.

**3. Chefes.** Mortos antes de um passar: 7 (seed 101, passou o chefe
8 na onda 41, 1.530 hp), 11 (202, chefe 12 na onda 60, 3.045 hp), 8
(303, chefe 9 na onda 46, 1.817 hp), 2 (404, chefe 3 na onda 16, 647
hp). Os primeiros são tranquilos. O chefe que passa nunca é o difícil:
é o primeiro a chegar depois que a defesa já afundou contra a horda.

**4. Economia inflada — confirmado.** Custos: divisória 10, palanque
40, personagem 40, arquibancada 80, caminhão 100, torre 100. Tabuleiro
inteiro ~2.220 votos. Renda azul acumulada: 3.076 na onda 10 (1,4
tabuleiros), 30.196 na 25 (13,6), 113.002 na 40 (51). A partir da onda
~8 o custo de construção em votos é irrelevante.

**5. Onda de quebra.** Com evolução: 41, 60, 46, 16 (média 40,8). Sem
evolução: 36, 36, 26, 16 (média 28,5). A evolução compra +12,3 ondas
em média — mas ZERO na seed 404, que quebra na onda 16 nos dois casos,
porque lá a defesa está em nível 1.30 e a propina por desenho só
começa na onda 7. Quebra precoce é problema de tabuleiro/rota do
chefe, não de economia.

**Achado estrutural.** Razão horda ÷ capacidade de dano:

| onda | nível simulado | nível 5 (23 def.) | nível 5 (60 def., máx. teórico) |
|---|---|---|---|
| 25 | 0,45 | 0,18 | 0,07 |
| 40 | 0,97 | 0,50 | 0,19 |
| 50 | 1,53 | 0,87 | 0,33 |
| 75 | — | 3,10 | 1,19 |
| 100 | — | 9,75 | 3,74 |

A defesa simulada cruza 1,0 na onda 40 e as quebras vieram em 41, 46 e
60 — o modelo prevê onde aconteceram. Uma defesa perfeita de 23
defensores no nível 5 quebra na onda 53. O máximo teórico (60
defensores, todo o orçamento liberado pelos 20 chefes, todos no nível
5) quebra na onda 72 e na onda 100 está 3,7× abaixo do necessário.
Da onda 50 à 100 a horda cresce 11,2×; a defesa, já no teto de nível,
só pode crescer 3× por número de defensores. **A onda 100 é
inalcançável por qualquer defesa que as regras atuais permitam**, e
nenhum ajuste de propina resolve isso — é HealthScaleGrowth composto
contra contagem linear de creeps.

**Limitações desta rodada.** (a) O AutoSetup constrói uma vez, no
início: o +1 torre e +1 personagem que cada chefe libera nunca são
construídos, então na onda 40 o simulador tem 23 defensores onde um
jogador real teria ~37. As ondas de quebra acima são PISO. (b)
`BD.Bribe.Report` usa 60 defensores como denominador da "defesa
completa"; contra o tabuleiro real de 23 ele superestima o custo em
2,6×. Os dois pontos ficam para a rodada 2.

**Ordem de ajuste sugerida pelos números:** HealthScaleGrowth
primeiro (é a raiz do achado estrutural), depois o vermelho por HP
(194:1 não é disputa), depois construir o orçamento liberado no
simulador; a propina é a última, e possivelmente não precisa mexer.

### Rodada 2 — 2026-09-15

**Valores aplicados.** HealthScaleGrowth 1.035 -> 1.022;
HealthPerBribe 2.5 -> 1.0; bBlueVotesByHealth True; HealthPerVote 1.0;
RedVoteWeight 1.0 (novo, neutro); StartingVotes 3000 (novo, no
DA_Difficulty — Easy/Hard ainda a definir no editor);
EvolutionRefundRatio 0.6.

**Mudancas de regra que entraram com ela.** O teto de personagens
acompanha os slots das plataformas (cada plataforma construida leva
seus slots para o teto e os tira ao ser vendida), o que torna a trava
de bloco alcancavel — antes 54 slots contra ~32 personagens congelavam
TODOS os personagens no nivel 1 para sempre. Construir passou a
debitar BuildCost em votos, com recusa NoVotes e preview vermelho. O
bonus em cadeia somava votos por atribuicao (`VotesBlue = Bonus.Votes`)
e sobrescrevia o capital inicial; virou `+=`. O simulador passou a
construir o orcamento que os chefes liberam, e BD.Bribe.Report a medir
contra o tabuleiro real em vez do teto.

**Como reproduzir.** `BD.Sim.Run <seed> 100 <evoluir 0|1> 4 1`,
8 corridas: seeds 101/202/303/404 x com e sem evolucao.

**As 8 corridas em 1.022.**

| seed | com evolucao | nivel | verm. | sem evolucao | verm. |
|---|---|---|---|---|---|
| 101 | vitoria 100 | 4,71 | 0 | vitoria 100 | 13.204 |
| 202 | vitoria 100 | 4,55 | 1.475 | derrota 65 | 900 |
| 303 | vitoria 100 | 4,89 | 0 | derrota 46 | 80 |
| 404 | derrota 26 | 1,54 | 1.917 | derrota 26 | 1.844 |

As 5 derrotas foram candidato na urna (n. 5, 13, 9, 5). Defensores no
fim: 76-88 (rodada 1: 23). Razao horda÷dano na onda 100: 0,72 em
1.020, 0,825 em 1.022.

**Metricas.** Propina: 1a evolucao na onda 7, gasto 26.164, money=0 no
fim, nivel 4,7/5 — no alvo, nao mexer. Placar (seed 101, evoluindo),
azul por onda 10/25/50/75/100: 2.180 / 23.420 / 146.574 / 502.384 /
1.365.368, com vermelho 0 em todas. Chefes: 19 mortos antes da 100 nas
vitorias. Renda x custo: tabuleiro inicial 3.100 votos (9 plataformas,
54 personagens, 3 torres, 6 cercas; sobraram 6 torres e 26 divisorias
sem pagamento), equivalente a 0,7 tabuleiros de renda na onda 10, 7,6
na 25 e 47 na 50. Overkill (nulo/azul): 26,9% evoluindo contra 11,0%
sem evoluir — evoluir piora o desperdicio.

**Tres achados que mudam o quadro.**

1. A evolucao quase nao decide a partida; o TABULEIRO decide. A seed
   101 vence a onda 100 sem evoluir nada (82 defensores no nivel 1) e
   a seed 404 morre na onda 26 com evolucao. A variavel dominante e a
   rota do chefe contra onde os defensores cairam, nao a curva.

2. Quatro seeds e pouco para medir curva. A seed 404 foi de 95 (1.020)
   para 26 (1.022); 0,2% de crescimento nao explica 69 ondas — e
   divergencia caotica (HP diferente muda quem morre quando, que muda
   a mira, que muda tudo). Atribuir quebra a curva pede ~15 seeds por
   configuracao.

3. O vermelho continua zero e RedVoteWeight nao resolve. Tres das
   quatro vitorias terminaram com vermelho literalmente 0: nada chegou
   a urna, e zero vezes qualquer peso e zero. O alvo 55/45 exige
   vazamento continuo, e a defesa e tudo-ou-nada — segura tudo, ou o
   chefe passa e a partida acaba. O unico sinal que cresce com a
   pressao sem exigir falha e o NULO (26,9% do azul); um vermelho
   competitivo teria de ser construido a partir dele, e isso e logica
   nova.

**Pendente.** O gargalo real e o candidato: decide 5 de 5 derrotas e
sua chegada depende da rota sorteada. Atacar isso antes de mexer em
curva, e remedir com mais seeds. Construir so morde na abertura: para
pesar a partida toda o custo teria de escalar com a onda
(Cost x HealthScale(onda)), que e logica nova. O perfil "defesa media"
nao existe no simulador — ha so evoluir tudo que da ou nada.

### Rodada 3 — 2026-09-15 (AutoSetup estrategico)

**O que motivou.** As rodadas 1 e 2 mediram um AutoSetup que despejava
pecas sem estrategia: nao fazia labirinto, nao alongava rota, nao
isolava a urna, nao concentrava no corredor. A estrategia central do
jogo e a do Clash of Clans — o labirinto e a defesa PRIMARIA, as
torres e personagens a secundaria, posicionados ao longo do corredor
que ele cria. O AutoSetup foi reescrito para jogar assim.

**O que mudou no AutoSetup.** A ordem era urna -> plataformas ->
personagens -> torres -> cercas; virou urna -> LABIRINTO ->
defensores (um defensor colocado antes do labirinto fica ao lado de
uma rota que vai mudar de lugar). `BuildMaze` cerca tres dos quatro
lados da urna deixando uma porta, depois percorre a rota MAIS LONGA
colocando uma cerca a cada `MazeStride` celulas e rele as rotas a cada
cerca. `BuildCorridorTargets` substituiu `BuildStretchTargets`: cada
celula livre vale o numero de celulas de rota ao alcance de uma torre,
entao as curvas da serpentina pontuam varias vezes sem caso especial.
`PlaceFences` e `BuildStretchTargets` foram removidos.

**Parametros de estrategia** (cvars, dao o perfil de jogador):
`BD.Debug.AutoSetup.FenceShare` (0.4), `MazeStride` (3), `UrnRing` (3),
`FocusUrn` (0.35), mais `ExtraDividers` e
`ABDMatchManager::AdjustDividerBudget`, ambos so para medicao.

**Estrategico x despejo** (mesmas seeds, HealthScaleGrowth 1.022,
evoluindo):

| seed | despejo | estrategico | defensores | nivel medio |
|---|---|---|---|---|
| 101 | vitoria 100 | vitoria 100 | 82 -> 74 | 4,71 -> 4,96 |
| 202 | vitoria 100 | vitoria 100 | 88 -> 84 | 4,55 -> 4,64 |
| 303 | vitoria 100 | vitoria 100 | 76 -> 74 | 4,89 -> 4,96 |
| 404 | derrota 26 | **vitoria 100** | 85 -> 76 | 4,12 -> 4,89 |

4 de 4 vitorias. O labirinto custou 320 votos (~8 defensores a menos)
e ainda assim melhorou tudo: menos pecas significa propina dividida
entre menos pecas, entao o nivel medio subiu.

**A seed 404 e a prova.** Historico dela: derrota na onda 16 (rodada
1), na 26 (rodada 2, com E sem evolucao), na 95 (com 1.020) — agora
vitoria na 100. O candidato decidia 5 de 5 derrotas; com a rota mais
longa e a defesa no corredor, os 19 chefes morrem antes da urna em
todas as seeds. O gargalo do candidato se resolveu sozinho, como o
briefing previu.

**O labirinto entregou.** 32 cercas, rota media 42,3 -> 44,3-46,3
(+5% a +9%), mais longa 51 -> 63-65 (+24% a +27%), 6 de 6 bocas sempre
com rota (a validacao `WouldBlockPathEdges` nunca deixa selar).

**Orcamento de cercas e o teto real, nao o FenceShare.** Com
DividerBudget 32 as cercas custam 320 de 3.100 (10%), entao FenceShare
0.4 ou 0.9 dao identico — o alvo "40% em cercas" so significa algo com
DividerBudget acima de ~120. Ganho de rota por orcamento (seed 101,
stride 2):

| divisorias | cercas | rota media | ganho | mais longa | personagens |
|---|---|---|---|---|---|
| 32 (atual) | 32 | 47,0 | +11% | 69 | 53 |
| 62 | 62 | 50,7 | +20% | 91 | 47 |
| 102 | 102 | 58,7 | +39% | 127 | 34 |
| 182 | 124 | 61,3 | +45% | 135 | 29 |

Satura em 124 porque ai o FenceShare passa a morder. Cada cerca a mais
e um personagem a menos — labirinto e defesa saem do mesmo capital, e
essa tensao e a decisao estrategica que o jogo tem a oferecer.

**Duas previsoes do briefing que NAO se confirmaram.**

1. O overkill nao caiu: nulo/azul 26,9% no despejo e 26,9% no
   estrategico, identico. Concentrar no corredor nao reduz desperdicio
   porque todo atirador usa a prioridade `First` e converge no creep
   mais adiantado, esteja onde estiver. Reduzir overkill depende de
   variar a PRIORIDADE DE MIRA, nao a posicao.

2. O vermelho piorou: era 0 / 1.475 / 0 / 1.917 e virou 0 nas quatro.
   Defesa melhor = menos vazamento = placar 1.365.428 a 0. A apuracao
   disputada ficou MAIS dificil, nao menos.

**Ressalva de atribuicao.** Labirinto e defesa-no-corredor mudaram
juntos; esta bateria nao separa qual virou a seed 404. Isolar rodando
com `FenceShare 0` e `UrnRing 0` (mantem a mira de corredor, tira o
labirinto), ~35 min.

**Proximos passos em aberto.** (a) O isolamento acima. (b) O vermelho
continua sem solucao e nenhuma escala resolve — o unico sinal que
cresce com a pressao sem exigir que a defesa falhe e o NULO, 26,9% do
azul. (c) Construir so pesa na abertura. (d) Quatro seeds e pouco:
atribuir quebra a curva pede ~15 por configuracao. (e) Decidir se o
DividerBudget sobe, com a tabela acima como base. NAO recalibrar
curvas antes de (a).

### Rodada 4 — 2026-09-24 (primeiro WaveLog)

**O que é.** Primeira corrida com o log por onda: `Saved/Logs/WaveLog.csv`,
uma linha por onda (placar e deltas, fundos ganhos/gastos, tabuleiro, nível
médio, combate da onda, pico de vivos, rota mais curta e mais longa), mais
uma linha curta em `LogBDMatch`. Cada linha cobre a montagem antes da onda
mais a própria onda. `FundsGap` diferente de 0 é dinheiro que não fecha.
Liga e desliga com `BD.WaveLog.Enabled`. Esta rodada serviu para validar o
log, não para calibrar: é uma seed só.

**Como reproduzir.** `BD.Sim.Run 101 30 1 4 1` headless
(`-game -nullrhi -BDSkipFrontEnd`), build 2026.09.24-2257, Normal, economia
pós-reforma (fundos públicos como moeda, votos só placar).

**Validação.** 30 ondas, 30 linhas. O azul inicial (3.100) mais a soma dos
deltas dá o azul final (44.554). Null, creeps (2.790 gerados = 2.790
mortos, 0 na urna), candidatos (6 saíram, 6 morreram), fundos ganhos
(3.493) e gastos (5.138) batem com o PostMatch. A propina de cada chefe
(436, 486, 542, 604, 674, 751) é igual ao `BD.Economy.Report`. `FundsGap`
foi 0 em todas as ondas.

**A curva** (a cada 5 ondas):

| onda | azul | Δ azul | null | Δ null | fundos | defensores | nível médio | pico vivos | rota curta–longa |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 3.160 | +60 | 18 | +18 | 52 | 8 | 1,25 | 6 | 30–58 |
| 5 | 4.000 | +300 | 565 | +172 | 52 | 8 | 1,25 | 31 | 25–66 |
| 10 | 6.700 | +720 | 2.948 | +737 | 39 | 9 | 1,56 | 58 | 30–70 |
| 15 | 11.554 | +1.170 | 8.154 | +1.246 | 39 | 10 | 1,70 | 79 | 30–90 |
| 20 | 19.138 | +1.800 | 16.263 | +1.967 | 175 | 11 | 1,73 | 100 | 29–96 |
| 25 | 29.920 | +2.400 | 27.297 | +2.409 | 761 | 12 | 2,08 | 113 | 33–119 |
| 30 | 44.554 | +3.240 | 39.730 | +2.631 | 755 | 13 | 2,38 | 128 | 28–131 |

O vermelho ficou em 0 nas 30 ondas.

**O que a curva mostra.**

1. **Os fundos zeram depois de cada chefe.** A abertura gasta 2.348 dos
   2.400. Depois disso só entra dinheiro nas ondas de chefe (a cada 5), e
   ele sai inteiro na montagem seguinte. Em 4 de cada 5 montagens o
   jogador não tem nada para comprar (fundos entre 4 e 175).
2. **O null alcançou o azul.** O Δ null ficou perto do Δ azul a partir da
   onda 10, e no fim o null soma 89% do azul (39.730 contra 44.554); 46%
   do dano foi desperdiçado. Na rodada 3 era 26,9%, mas o cenário mudou
   (defesa bem menor, economia reformada). Não dá para comparar direto.
3. **O pico de vivos colado no total da onda.** Na onda 29 foram 174
   gerados e 174 vivos ao mesmo tempo: a onda inteira entra antes do
   primeiro morrer. A defesa só mata quando os creeps já se amontoaram.
4. **O labirinto cresce pela rota longa, não pela curta.** De 78 para 142
   separadores no tabuleiro, a rota mais longa foi de 58 para 131 e a
   mais curta ficou entre 25 e 33. A boca mais próxima continua curta.
5. **A defesa cresce devagar.** De 8 para 13 defensores e nível médio de
   1,25 para 2,38 em 30 ondas, uma compra por chefe.

**Limites.** Uma seed, 30 ondas, e a política de compra é a do driver da
simulação, não a de um jogador. Para achar padrões ("o vermelho sempre
encosta na onda X") falta rodar várias seeds até a onda 100 e cruzar os
WaveLogs pelo `MatchStart`.

### Rodada 5 — 2026-09-25 (memória por onda)

**O que é.** Duas partidas iguais (`BD.Sim.Run 101 100 1 4 1`, Normal,
headless com `-nullrhi -nosound`), uma com o jumento animado e outra com
o cilindro (`BD.Enemy.StaticBodyOnly 1`). A memória é lida no fim de
cada onda, com o tabuleiro vazio. As duas pararam no teto de 1 hora
real do `BD.Sim.Run` (`STALLED`), na onda 95 e na 96, não na 100.

| onda | jumento MB | cilindro MB | atores | UObjects (jumento) |
|---|---|---|---|---|
| 1 | 1.699 | 1.689 | 210 | 54.393 |
| 20 | 1.714 | 1.714 | 292 | 56.785 |
| 40 | 1.741 | 1.732 | 403 | 56.836 |
| 60 | 1.754 | 1.747 | 546 | 58.219 |
| 80 | 1.780 | 1.773 | 720 | 58.101 |
| 90 | 1.789 | 1.797 | 821 | 56.067 |
| 94–95 | 1.794 | 1.791 | 871 | 64.767 |

**Leitura.** Sem vazamento. A memória sobe cerca de 1 MB por onda nas
duas partidas, junto com os atores do tabuleiro (a simulação põe peças).
Os UObjects sobem e descem entre 54 e 65 mil, então os creeps mortos são
liberados. O jumento fica uns 10 MB acima do cilindro, e essa diferença
não cresce (na onda 90 o cilindro chegou a ficar acima).

**Limites.** Sem RHI, a animação não roda (só com o creep na tela) e a
memória de renderização não entra. O que se via na partida real da onda
150 tinha ainda o log de áudio a cada frame, já corrigido. A próxima
partida na tela grava `MemUsedMB` no WaveLog e confirma o resultado com
renderização.

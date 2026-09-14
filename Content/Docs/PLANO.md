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

- **Voto AZUL**: ganho a cada inimigo morto. É o placar do jogador.
- **Voto VERMELHO**: ganho pelo adversário a cada inimigo que alcança
  a urna.
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

## 4. O candidato vermelho (clímax)

- Surge quando o vermelho ULTRAPASSA o azul, e só se ao menos um
  vermelho já foi marcado. Nunca surge contra jogo perfeito.
- Sai de uma boca sorteada. Lento. HP = creep da onda × 40.
- É alvo prioritário de todo defensor no alcance.
- As ondas normais continuam saindo enquanto ele anda.
- **Chega na urna → derrota.**
- **Morre → a apuração PAUSA por 30s, com o vermelho CONGELADO**
  (chegadas nesse período não contam). Não zera nem iguala o placar —
  dá fôlego para reforçar. Se o placar seguir invertido, ele volta.
- Só um por vez.

---

## 5. Tabuleiro

- Grid lógico 48×22 (célula de 700 cm), invisível, sob terreno aberto.
- Seis BOCAS de spawn nas bordas (dois frontais, quatro laterais). As
  laterais chegam mais rápido — o jogador precisa alongar o caminho
  delas com cercas.
- A cada onda, sorteia quantas e quais bocas ficam ativas. O total de
  creeps não muda — menos bocas = fluxo mais concentrado.
- URNA posicionada pelo jogador numa zona restrita (X 40-47, Y 6-15).
  Define a célula-objetivo. Fica travada quando a onda 1 começa.

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

- VITÓRIA: sobreviver a X ondas, libertando presos. Easy solta 1,
  Normal 2, Hard 3.
- Vencer fecha a partida mas o endless continua disponível (leaderboard).
- Dificuldades encadeadas: vencer Easy dá bônus inicial no Normal, e
  assim por diante. Começar direto no Hard, sem bônus, é quase
  impossível — de propósito.
- Diferença entre dificuldades: orçamento de peças, velocidade de
  escalada das ondas, número de obstáculos fixos.
- SAVES como recurso limitado: Easy 3, Normal 2, Hard 1. O jogador
  escolhe QUANDO salvar — salvar cedo garante pouco, salvar tarde é
  arriscado. Restaura o estado completo (níveis, votos, tabuleiro) no
  começo da onda salva.

---

## 9. Ambientação

- Ciclo dia/noite atrelado à onda (não ao tempo real, por causa do
  acelerador). A luz conta o progresso: amanhecer no início, noite nas
  ondas finais. Postes acendem ao entardecer.
- Ônibus de caravana marcam as bocas ativas e escondem a borda do mapa.
- Urna com som de votação a cada chegada.
- Velocidade de jogo 1x / 2x / 4x, persistente entre ondas, via time
  dilation global.

---

## 10. Abertura e telas

- Splash com a marca (Gurila Games) → loading → menu principal com
  fundo cinemático (cinecut futuro) → Play com fade → jogo.
- Menu: Play, Options (gráficos / áudio / idioma), Quit.
- Configurações persistidas em SaveGame.
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

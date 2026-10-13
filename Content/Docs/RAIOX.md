# Raio-X da mecânica — Brazil Defense

**Foto de: 2026-10-13, commit ae47de3**; atualizada no mesmo dia pela leva de bugs do briefing 17:30 (urna, ministros, save/load, apuração) e pelo comício do briefing 21:00.

> O que o jogo FAZ hoje, lido no código. O PLANO.md diz o que o jogo deve
> ser; este arquivo diz o que ele é, e marca onde os dois discordam.
> Fonte da verdade: código C++ → valores efetivos dos DataAssets e do
> DefaultGame.ini (lidos no editor) → PLANO §1–11 por último.
> Caminhos de código relativos a `Source/Brazil_Defense/Private/`.
>
> **Quando atualizar**: ao implementar mecânica nova ou mudar uma regra
> daqui, corrigir a linha e a data acima. Ao resolver uma divergência
> (no código ou no PLANO), tirá-la da tabela de divergências.

## Sumário — as 10 regras que mais definem o jogo hoje

1. **Placar ≠ dinheiro.** Votos só contam (+1 azul por militante morto, +1 vermelho por chegada; `bVotesByBody`); tudo se compra com **dinheiro público**, nada gasta voto.
2. **A única renda é a propina do candidato agendado morto** (= vida dele: ≈436 na onda 5, ≈3449 na 100), mais os 2000 iniciais e reembolsos.
3. **Candidato a cada 5 ondas** (20 até a 100), vida = 40× a do militante da onda, 0,5 cél/s; **chegou à urna = derrota imediata**, sempre.
4. **Virada (vermelho > azul)**: com caídos, todos voltam num desfile de 30 s, horda parada, e ao fim o placar nivela em min(azul, vermelho); sem caídos, o próximo candidato sai na hora.
5. **Vitória** = limpar a onda 100 sem candidato no board e sem retorno, com azul ≥ vermelho (empate vence); depois, endless opcional que só acaba com candidato na urna.
6. **Horda**: um tipo só (Militantes/jumento, 10 de vida), vida ×1,022^(onda−1), W creeps por boca × 6 bocas, bocas deslizam e são sorteadas a cada onda.
7. **Defensores sem teto de quantidade** (limite = dinheiro, célula, slot); níveis 1–6, custo ×3 por nível; mira = mais adiantado no alcance; **candidato não é prioridade de torre/personagem**.
8. **Cotas só de divisória e plataforma**; divisória grátis em dinheiro, cota +1 por onda e +4, +5, +6… por candidato morto.
9. **Palácio (3000) + Mito**: patrulha de 120 s, raio de 3 células, persegue candidato em qualquer ponto do board, chuta ministros, dorme max(10; 45 − 0,5·abates) s.
10. **Ministros**: 1 por candidato morto (11 assentos); lendo, protegem militantes num raio de 2 a 7 células (ninguém mira, dano perdido); só o chute do Mito os manda de volta.

---

## 1. Loop da partida

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Entrada | `BDGameMode` cria o `ABDMatchManager` (o mapa não traz); fora do editor, quem não passou pelo menu volta ao menu | GlobalDefaultGameMode = BDGameMode | BDGameMode.cpp:30-52 | Não está no PLANO |
| Fase Setup | Dura um frame: sorteia seed, gerador de obstáculos (desligado), vai para Building no mesmo BeginPlay. Em Setup não se coloca nem move nada | `bRandomizeSeed` true; `bGenerateObstacles` False | BDMatchManager.cpp:89-128, 231-254 | Não está no PLANO |
| Urna na mão | Sem urna no board, ela vai para a mão; as outras peças são recusadas até ela estar no chão (ObjectiveMissing). Qualquer célula livre e alcançável | `bRestrictToZone` False | BDGameMode.cpp:54-70; BDPlacementComponent.cpp:1320-1350 | Sim — §2.1 "zona restrita"; §5 e código: livre |
| Urna travada | Só se coloca antes da onda 1; nunca é vendida nem movida | IsBuildLocked = onda ≥ 1 | BDMatchManager.h:175; CanPlace/CanRemove | Não (§5) |
| Primeira contagem | Só corre com a **urna no chão**: sem urna fica parada no valor cheio e o HUD mostra "coloque a urna para a contagem começar"; a urna no chão dispara do `FirstWaveDelay` cheio | Easy 90 / Normal 120 / Hard 45 s | Tick / IsAwaitingUrn (BDMatchManager.cpp); HUD.Wave.PlaceUrn | Não (§2.2) |
| Contagem entre ondas | max(10; 45 × 0,94^onda): ≈42 s após a onda 1, piso de 10 s a partir da 25. Em tempo dilatado (2x/4x acelera, pausa para) | BaseWaveDelay 45, Decay 0,94, Min 10 | GetWaveDelay (BDGameBalanceSettings.cpp:140-144) | Não (§2.4) |
| Tick em Building | FreezeTimer (debug) → IsEndDue → ResolveEnd → desconta contagem → StartWave em ≤0 | — | BDMatchManager.cpp:294-329 | Não está no PLANO |
| Chamar a onda antes | Só em Building, só por console/sim (sem botão), **recusado sem urna no chão**; acumula `EarlyCallBonus` (2/s pulado) que nada gasta | EarlyCallBonusPerSecond 2 | CallWaveEarly | Não está no PLANO |
| Início da onda | ++onda → WaveActive → dia anda → deslize das bocas → sorteio das ativas → total = por boca × bocas → espera do ônibus (máx. de 2,0 e 1,5 s) → OnWaveDealt (candidato agendado) | BusMoveSeconds 2, EngineLeadSeconds 1,5 | StartWave; BDWaveSubsystem.cpp:205-298 | Não está no PLANO (bocas: §5) |
| Fim da onda | Limpa quando um inimigo sai da lista e não sobra creep de onda (candidato não conta) nem nada a soltar | — | ForgetEnemy (BDWaveSubsystem.cpp:1328-1350) | Não está no PLANO |
| OnWaveCleared | +divisórias da onda → WaveLog → IsEndDue/ResolveEnd → senão abre Building | DividersPerWave 1 | BDMatchManager.cpp:458-488 | Não está no PLANO |
| Construir durante a onda | Colocar em Building **e** WaveActive (torre, personagem, plataforma, divisória); vender a qualquer momento; mover em Building/WaveActive | — | CanPlace (:1137-1171); CanRemove; CanMove | Sim — §2.2 montagem "entre ondas, gastando votos" |
| Ordem das recusas | Encerrada → não desbloqueada → sem cota → sem slot → palácio já existe → fase → sem dinheiro | — | GetHandRefusal (BDPlacementComponent.cpp:405-452) | Não está no PLANO |
| Mão entre fases | A peça na mão atravessa as ondas; cai em Setup, Victory e Defeat | — | BDPlacementComponent.cpp:97-110 | Não está no PLANO |
| Fim congela | Victory/Defeat põem a dilatação em 0; voltar a Building devolve a velocidade | — | SetPhase (:256-278) | Não (§8) |
| Velocidades | 1x/2x/4x por dilatação global (botões do HUD, console); vai no save; abre em 1x | AllowedGameSpeeds [1,2,4] | SetGameSpeed (:1069-1090) | Não (§9) |
| O que não acelera | Sol, música/ambiência, bipe da urna, ducking (tempo real) | — | BDDayCycleComponent.cpp:203-206 e outros | Não (§9 cita o sol) |
| Pausa (P/botão) | Param: contagem, dia, ondas, candidatos, propina, creeps/torres. Continuam: câmera, colocação/venda/movimento/evolução (`bTickEvenWhenPaused`, `bTriggerWhenPaused`) e a trilha | — | BDUISubsystem.cpp:497-512; BDPlacementComponent.cpp:48, 149-157 | Não está no PLANO |
| Menu (Esc) | Mão vazia e nada selecionado: abre o menu, pausa, entrada UI only; ao fechar só despausa se não estava pausado antes | — | BDUISubsystem.cpp:438-493 | Não está no PLANO |

## 2. Economia

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Contagem por corpo | `bVotesByBody` ignora os campos de "voto por vida" | `bVotesByBody` True (default de classe); `HealthPerVote` 1, `bBlueVotesByHealth` True, `RedVoteWeight` 1 sem efeito | BDGameBalanceSettings.h:212-236; BDWaveSubsystem.cpp:1233/1262/1288 | **Sim** — §3: azul e vermelho pelo HP (K=1) |
| Azul | +1 por militante morto, por quem quer que seja; candidato 0 | — | NotifyEnemyDied (BDWaveSubsystem.cpp:1277-1294) | Sim (§3 HP) |
| Vermelho | +1 por chegada, com bipe; candidato na urna = 0 voto e derrota. `VotesOnArrival` do DA não é lido | — | NotifyEnemyArrived (:1251-1275); AddVotesRed (BDMatchManager.cpp:784-811) | Sim (§3 HP) |
| Congelamento | `IsCountFrozen()` fixo em false: nunca congela (comentários antigos ficaram) | — | BDCandidateSubsystem.h:112 | Não está no PLANO |
| Nulo | Dano desperdiçado (overkill, tiro perdido, dano em militante protegido por ministro) acumula; a cada "um corpo" (10 × escala da onda atual) +1 nulo. Só exibido | ≈10 na onda 1, ≈29 na 50, ≈86 na 100 | ReportWastedDamage (BDWaveSubsystem.cpp:1217-1249) | Sim — §3 "na mesma taxa" do HP |
| Votos iniciais | Não existe StartingVotes; abre 0×0 | — | ApplyStartingBudgets (:199-204) | Não está no PLANO |
| Votos nunca gastos | Construir, evoluir, mover, vender não tocam voto | — | BDMatchManager.h:318; BDPlacementComponent.cpp:1836, 2223 | **Sim** — §2/§3 "gastar votos derruba o azul" |
| Aviso de resultado | HUD mostra o dinheiro que sobra ou falta, não o placar resultante | — | BDHUDWidget.cpp:1580-1625 | Sim (§3) |
| Duas moedas | `BribeHeld` (ladrão) → `PublicMoney` (Casa da Moeda); tudo se paga com o público | — | BDMatchManager.h:321-371 | **Sim** — §3 "uma moeda só" |
| Dinheiro inicial | Atribuído, não somado | StartingFunds 2000 (E/N/H) | ApplyStartingBudgets (:203) | Não está no PLANO |
| Bônus em cadeia | Dificuldade de baixo já vencida: +400 dinheiro, +4 divisórias, +1 plataforma, +1 save; Easy nunca | ChainBonus 400/4/1/1 nos 3 DAs | :206-221 | Não (§8); valores não estão no PLANO |
| Propina | Só candidato **agendado** morto: floor(vida / HealthPerBribe), mín. 1. Os 20 somam ≈29 648 | HealthPerBribe 1,0 (default de classe 2,5) | NotifyCandidateKilled (BDCandidateSubsystem.cpp:554-569) | **Sim** — §4 "+1 torre/+1 personagem de teto" |
| Coleta | Automática, em fila: saco 3 s → contagem 1,2 → registradora 0,35 → transferência 0,9 (≈5,45 s por candidato), 1:1 | Bag/Count/Register/Transfer | BDBribeSubsystem.cpp:117-294 | Não está no PLANO |
| Dinheiro zerado no fim | Vitória e derrota zeram propina e público (`DropMoney`), descartam a fila; endless começa com 0 | — | BDMatchManager.cpp:559, 766, 935-947 | Não está no PLANO |
| Outras entradas | Só reembolso de venda/evolução e console | — | :851-875 | Não está no PLANO |
| Save/load | Grava votos, propina, dinheiro, PaidCost/EvolutionSpent; load monta sem cobrar e repõe saldos | — | :571-733 | Não (§8) |
| Fórmula de preço | round(400 × 1,0 × BaseCost / 100), fixa em qualquer onda | ReferencePiecePrice 400; Ratio 1; RefCost 100 | GetPieceCost (BDGameBalanceSettings.cpp:119-128); GetBuildPrice (BDMatchManager.cpp:1014-1035) | Sim (§3 BuildCost em votos) |
| Preços | Divisória **0** (só cota) · Palanque 160 · Caminhão 400 · Arquibancada 320 · Torre 400 · Personagem (Nicole) 160 · Palácio **3000** fixo · Urna 0 | Cost 10/40/100/80; BuildCost 100/40; PalaceCost 3000 | DAs + GetBuildPrice | Sim (§3) |
| Desbloqueio por onda | Vale com CurrentWave ≥ onda (já durante a própria onda); `UnlockWave` do TowerData não é lido | Caminhão 10; Arquibancada 25; resto desde o início | GetUnlockWave (BDPlacementSettings.cpp:20-37) | Não está no PLANO |
| Níveis | 1 a 6 | MaxLevels 6 (constante) | BDTowerData.h:95 | **Sim** — §7 "até 10" |
| Custo de evolução | Base × 3^(N−2) em dinheiro: 40, 120, 360, 1080, 3240 (total 4840) | UpgradeCostGrowth 3; UpgradeCostBase 40 (default de classe 50) | GetUpgradeCost (:76-80) | Não (§7 exponencial) |
| Efeito do nível | Dano × (1+0,6·(N−1)); alcance × (1+0,1·(N−1)); nível autorado no DA vence | 0,6 / 0,1 | :82-90; BDTowerBase.cpp:200-216 | Não (§7) |
| Evolução em bloco | Na plataforma: todos os slots ocupados e só o nível mais baixo compra | — | BDPlatformComponent.cpp:133-153 | Não está no PLANO |
| Evolução do palácio | Não há compra; nível do DA (0) ou console | Level 0; MaxLevel 5 | BDPalaceData.h:104, 128-134 | Não está no PLANO |
| Venda | round(PaidCost × fração): 100% antes da onda 1 (divisória até a 3), 50% depois; + floor(60% da evolução) | BuildingPhaseRefundRatio 1; SellRefundRatio 0,5; DividerRemovalGraceWave 3; EvolutionRefundRatio 0,6 | GetSellRefundRatio (:46-54); SellPiece (BDPlacementComponent.cpp:2175-2240) | Não (§3); reembolso de evolução não está no PLANO |
| Janela da divisória | Custa 0, então a fração não muda nada; a cota volta sempre | — | RefundRemoval (:1205-1215) | Sim, em parte (§3) |
| Vender plataforma | Personagens caem junto: só 60% da evolução, nada do preço de compra | — | ForgetSlotPiecesOn (:1940-1971) | Não está no PLANO |
| Mover | PaidCost × min(25%; 2% × onda); soltar na origem não cobra; plataforma leva os personagens | MoveTax 0/0,02/0,25 | GetMoveTaxRate; TryPlaceAtHovered (:1797-1831) | Não (§3) |
| Cota de divisórias | Inicial por DA; +1 por onda limpa; +(4 + N−1) pelo N-ésimo agendado morto | E 100 / N 80 / H 60 | ApplyStartingBudgets; RewardCandidateKill (:967-973) | Sim (§4); cota não está no PLANO |
| Cota de plataformas | 1 por plataforma, volta na venda, não cresce | E 10 / N 8 / H 5 | :195, 1108-1124 | Não está no PLANO |

## 3. Horda

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Tipo | Um tipo só por onda (`WaveEnemy`, senão `DebugEnemy`); sem composição. Horda_02–04 não entram | WaveEnemy = DebugEnemy = DA_Horda_01 | ResolveWaveEnemy (BDWaveSettings.cpp:20-24) | Não está no PLANO |
| Vida | MaxHealth × 1,022^(W−1): 10 na onda 1, ≈29 na 50, ≈86 na 100. `CF_HealthScaleByWave` existe mas está desligada. Aplicada no spawn | MaxHealth 10; HealthScaleGrowth **1,022** (default de classe 1,12) | GetHealthScale (BDGameBalanceSettings.cpp:35-44) | **Sim** — §7 ~1,12–1,15; §8 1,035 |
| Dificuldade | Nenhum DA_Difficulty mexe em vida, quantidade ou cadência | — | UBDDifficultyData | **Sim** — §8 "velocidade de escalada" |
| Velocidade | 1,5 cél/s (1050 cm/s), aceleração 3 cél/s² (≈0,5 s), ±10% por creep e "respiração" ±5% | MoveSpeed 1,5; Acceleration 3; SpeedVariance 0,1 | BDEnemyBase.cpp:153-156, 622-627 | Não está no PLANO |
| Corpo | SKM_Jumento ×2, 4 skins sorteadas (MI_Jumento_PT/Gay/Hemp/Soci), anim segue a velocidade; não colide com nada | AnimReferenceSpeed 554 | BDEnemyBase.cpp:84-107, 220-341 | Não está no PLANO |
| Creeps por boca | 1 + 1·(W−1) = W | Base 1, Step 1 | GetCreepsPerSpawnPoint (:71-74) | Não (§7) |
| Total da onda | W × **todas** as bocas do mapa (6 pelo log), inclusive sem rota; não depende de quantas foram sorteadas | — | HandleWaveStarted (BDWaveSubsystem.cpp:219-231) | Não (§5) |
| Bocas ativas | Sorteio com semente (seed, onda) entre as bocas com rota: de 1 a todas | Min 1, Max 0 (= todas) | DrawActiveSpawnPoints (:517-559) | Não (§5) |
| Distribuição | Round-robin, um creep por intervalo global; boca sem rota é pulada; fallback para qualquer boca que funcione; sem nenhuma, descarta | — | SpawnNextOfWave (:583-651) | Não está no PLANO |
| Intervalo | max(0,20; 0,35 × 0,97^W): ≈0,34 s na onda 1, piso a partir da 19 | 0,35 / 0,97 / 0,20 | GetWaveSpawnInterval (:56-59) | Não está no PLANO |
| Deslize das bocas | Antes de cada onda, 75% por boca; até 6 tentativas de −5..+5 na borda; **coleira de 5 da âncora**; não encosta em outra boca; gap de ônibus ≥ 2; precisa de rota | MouthWanderChance 0,75; MaxCells 5; MinBusGap 2 | WanderSpawnPoints/TryShiftSpawnPoint (:313-515) | Parcial — §5 "1–5 células", sem coleira nem gap |
| Boca bloqueada | Procura saída livre, sempre dentro da coleira de 5; senão fica bloqueada | — | :471-492 | Parcial — §5 "posição aberta mais próxima" |
| Ônibus | **Todos** seguem sua boca; só os das ativas têm motor e buzina (buzina no 1º creep). O creep nasce na célula de saída, não no ônibus | BusMoveSeconds 2 | BDBusSubsystem.cpp:55-160 | Parcial — §9 "marcam as bocas ativas" |
| Pathfinding | A* em 4 direções; Free/Tower/Spawn/Goal andáveis; Platform e Blocked bloqueiam; divisórias bloqueiam arestas; vai à Goal de menor custo | StepCost 1000 | BDPathfinder.cpp:28-60 | Não (§6) |
| Variação de rota | Mapa de custo por creep em [1; 1,3] por célula: espalha a horda; rota mais curta da boca como fallback | RouteCostVariance 1,3; PerCreep | DrawRouteCost (:952-978) | Não está no PLANO |
| Reroteamento | Qualquer mudança no grid reroteia na hora todos os vivos (menos os da perna final) | — | RerouteLivingEnemies (:980-1016) | Não está no PLANO |
| Proteção dos creeps em campo | `WouldCutOffCells`: recusa peça que deixe sem rota a célula para onde um creep vai | — | BDPathfinder.cpp ~250-330; WouldFenceInCreeps (BDPlacementComponent.cpp:1114-1135) | Parcial — §6 só "não fechar o caminho" |
| Movimento orgânico | Faixa lateral ±0,4 célula com ondulação, curvas abertas suavizadas, giro cosmético | LateralOffsetMax 0,4; LaneWander 0,6; CornerSmoothRadius 0,35 | BDEnemyBase.cpp:396-549 | Não está no PLANO |
| Chegada | ≤150 cm da urna (perna final até o ator): +1 vermelho, bipe, destruído sem corpo, grito ou sangue | ArrivalDistance 150 | Arrive (BDEnemyBase.cpp:704-723) | Parcial (§9 bipe ok; §3 por HP) |
| Morte | Avisa o atirador → +1 azul e placar de abates → grito → corpo → sangue → destroy | — | BDEnemyBase.cpp:786-838 | Não está no PLANO |
| Corpo | Só com malha esquelética e anim de morte: 1 de 4 quedas a 1,5x, afunda em 0,4 s; máx. 40 deitados | MaxCorpses 40 | BDCreepCorpse.cpp:34-133 | Não está no PLANO |
| Sangue | Sem `BloodTexture` no ini: **nenhuma mancha aparece no jogo** (só a regressão injeta) | BloodTexture vazio | BDBloodDecals.cpp:53-107 | Não está no PLANO |
| Sons do creep | Grito (máx. 5), queda (5), passos (12), chamado (5), falas (2, ver §10); vocaliza a cada 8–20 s | ver CreepSoundSettings | BDCreepSoundSubsystem.cpp | Não está no PLANO |
| Abrigo de ministro | Militante protegido: ninguém mira, dano vira nulo, corpo pisca | ShelterBlinkHz 5 | BDEnemyBase.cpp:571-600, 759-768 | Não está no PLANO |

## 4. Candidato

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Intervalo | Sai na onda múltipla de 5 (no OnWaveDealt) | CandidateInterval 5 | HandleWaveDealt (BDCandidateSubsystem.cpp:217-268) | Não (§4) |
| Um por vez | Com candidato vivo ou retorno em curso, fica pendente e sai na próxima onda livre | bSchedulePending | :233-240 | Não (§4) |
| Saída antecipada | Virada sem caídos → sai na hora o grupo da próxima múltipla de 5, que depois não solta ninguém. Não em Setup, nem com candidato vivo ou no ônibus | EarlyScheduleWave | HandleVotesChanged (:186-215) | Não está no PLANO |
| Ônibus e vantagem | Espera os ônibus estacionarem e sai antes; a horda espera ônibus + 5 s | CandidateLeadSeconds 5 | :151-161, 245-267 | Não está no PLANO |
| Vida | Vida do militante da onda (10) × 1,022^(W−1) × 40, gravada no registro; o MaxHealth do DA (100) só vale sem WaveEnemy | CandidateHealthMultiplier 40 | GetCandidateHealth (:109-112); SpawnCandidate (:402-430) | Não no ×40 (§4); crescimento diverge (§7/§8) |
| Velocidade | `CandidateSpeed` sobrepõe o DA; aceleração do DA 1; mesma variação de faixa e rota da horda | 0,5 cél/s | BDCandidate.cpp:31-36 | Não (§4) |
| Boca | Sorteio com semente (seed, onda, ordinal) entre todas as bocas com rota, não só as ativas; em grupo, bocas diferentes | — | SpawnFromRecord (:324-363) | Não (§4); semente não está no PLANO |
| DA | Corpo, andar, falas e grito: todos usam o DA_Candidato_01 (cubo vermelho 6×6×8). O **comício** vem do DA do seu número (`DA_Candidato_N`, `UBDCandidateData`; acima de 20 volta ao 01), lido na saída e gravado no registro como a vida | CandidateEnemy; Rally do DA | ini:101; SpawnCandidate / SpawnFromRecord (BDCandidateSubsystem.cpp) | Não (§14) |
| Mira do Mito | Prioridade total: persegue em **qualquer ponto do board**, só atira nele enquanto ele vive | Detection 3; Chase 1,5; CloseIn 0,75 | BDAgent.cpp:515-611, 999-1083 | Parcial (§4) |
| Mira de torres e personagens | **Sem prioridade**: candidato = creep comum (mais adiantado no alcance) | TargetPriority não é lido | BDTowerBase.cpp:445-477 (coment. 452-456) | **Sim** — §4 "alvo prioritário de todo defensor" |
| Refúgio | Nunca é coberto por ministro; intocável só no discurso do comício (`IsRallyImmune`) | — | BDEnemyBase.cpp (IsSheltered, IsUntouchable) | Não está no PLANO |
| Votos | 0 azul, 0 vermelho; fora das contagens e do placar por tipo | — | BDWaveSubsystem.cpp:1262-1299 | Não (§4) |
| Morte: ordem | Solta ministro, entra nos caídos, propina/divisórias (ou nivelamento se for retorno); depois som, corpo e sangue | — | BDCandidate.cpp:85-93 | Não está no PLANO |
| Morte: recompensa | Agendado: propina = vida, divisórias 4+(N−1). Retorno: nada. **Todos soltam 1 ministro** | bReleaseOnCandidateKilled true | :534-569 | **Sim** — §4 "+1 torre/+1 personagem"; "quem volta não dá nada" |
| Morte: pausa | Não existe (IsCountFrozen = false) | — | BDCandidateSubsystem.h:111-113 | Não está no PLANO |
| Chegada | Derrota imediata, agendado ou retorno, em qualquer fase e no endless; declarada antes da onda poder limpar | — | BDCandidate.cpp:73-83; :495-514 | Não (§2/§4/§8) |
| A onda não espera | Limpa sem ele; a contagem segue | — | GetLivingWaveCreepCount (:1162-1173) | Não está no PLANO |
| Grupo (endless) | 1 + floor(caídos distintos / 20): na prática só depois de 20 mortos | CandidateGroupStep 20 | SpawnCandidateGroup (:432-455) | Não está no PLANO |
| Nome no HUD | `NSLOCTEXT "Candidate {N}"` fixo: não lê o DA nem `Candidate.NN` | — | :389-392 | Sim — §14 prevê nome pelo DA/tabela |
| HUD | Até 6 linhas no topo "Nome HP x/y" (barra após o 1º dano) + "+N a caminho"; aviso "O CANDIDATO SAIU" 4 s; linha "OS CAÍDOS VOLTAM · faltam N"; barra vermelha no mundo após o 1º dano; placar com ícone único (`CandidateKillIcon`), `CandidateFace` não é lido | MaxCandidateRows 6; CandidateNoticeSeconds 4 | BDHUDWidget.cpp:1241-1258, 1450-1524; BDMatchHUD.cpp:238-283 | Sim — §14 ícone = `CandidateFace` |
| Rebobinar | Building com onda 0 esquece vivos, enviados, caídos e retorno | — | ResetForNewMatch (:270-296) | Não está no PLANO |
| Comício: quando | `RallyFirstDelay` (6 s) depois de sair do ônibus, depois a cada `RallyEvery` de caminhada (20 s no 1º → 10 s no 20º, linear), sorteado ±20% por comício com semente (seed, onda, ordinal). Tempo de jogo: acelera em 2x/4x, para na pausa | DA: RallyFirstDelay 6; RallyEvery 20→10; RallyEveryJitter 0,2 | UBDRallyComponent (Candidate/BDRallyComponent.cpp) | Não (seção Comício) |
| Comício: sequência | Walking → **Tell** (1,5 s andando, exclamação vermelha + buzina do ônibus como som curto; chegar à urna aqui é derrota) → **Speaking** (parado, `RallyDuration` 4 s ou a duração da cue `RallySpeech`, mínimo 2,5 s) → **Spawning** (parado e já vulnerável, 0,6 s enquanto a multidão brota) → Walking | RallyTellSeconds 1,5; RallyDuration 4; RallyMinDuration 2,5; RallyRiseSeconds 0,6; RallyTellSound SCue_Bus_Horn | TickComponent / Enter* | Não (seção Comício) |
| Comício: discurso | Intocável: torres, personagens e Mito não o miram; dano que chega (inclusive projétil em voo) vira nulo como o do abrigo do ministro. O Mito perde o alvo e segue a patrulha; acaba o discurso, volta a persegui-lo. Só a fala dele: as outras falas que soavam são cortadas e nenhuma começa (horda, Mito, Nicole, grito do ministro); efeitos seguem. Sem `RallySpeech`, toca a `SpeechSound` dele (hoje vazia: só o megafone) | — | IsUntouchable; ABDAgent::FindCandidate; UBDCreepSoundSubsystem::TakeFloor | Não (seção Comício) |
| Comício: multidão | `RallyCount` = 2×N militantes (N = ordinal até 20), sorteados por peso em `RallyComposition` (hoje só DA_Horda_01; vazio = creep da onda), vida da horda da onda. Busca em anel a partir da célula dele sobre células andáveis (respeita divisórias), até 20 passos: uma por célula, só Free/Tower/Spawn (nunca plataforma, palácio, Blocked, urna) e com rota até a urna; o que não acha lugar não brota. Brotam do chão (escala Z 0→1 em 0,6 s, parados, já alvejáveis), votam como qualquer militante, a onda espera por eles, mas **não** entram no total W × bocas (contam em `CreepsRaised`) | RallyCount 2N; RallyMaxSteps 20 | RaiseCrowd; UBDWaveSubsystem::SpawnEnemyAtCell | Não (seção Comício) |
| Comício: um por vez | Um comício no board: o segundo candidato devido espera o primeiro voltar a andar para começar o tell | — | UBDCandidateSubsystem::TryClaimRally | Não (seção Comício) |
| Comício: retorno | Quem volta no desfile não discursa (nem forçado) | bRallyOnReturn false | SpawnFromRecord | Não (seção Comício) |
| Comício: visual | Disco vermelho translúcido no chão (1,5 célula de raio, pulso 1 Hz, `M_RallyAura` sobre o cilindro da engine) e megafone vermelho sobre a cabeça no discurso; aviso "COMÍCIO" 2 s na linha do "O CANDIDATO SAIU" ao começar o tell | RallyAuraRadiusCells 1,5; RallyAuraPulseHz 1; RallyNoticeSeconds 2 | ShowAura; ABDMatchHUD::DrawRallyMegaphones; UBDHUDWidget::UpdateCandidate | Não (seção Comício) |

## 5. Virada e retorno

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Gatilho | A cada mudança de votos: `Red > 0 && Red > Blue`, sem retorno em curso. Nulo não dispara; gasto não mexe no azul | — | HandleVotesChanged (BDCandidateSubsystem.cpp:134-138, 186-215) | Não no gatilho; sim no §3 (gasto não derruba o azul) |
| Quem volta | Todos os caídos (`Fallen`), na ordem da 1ª morte; a lista só cresce | — | BeginReturn (:457-491) | Não (§4) |
| Vida | A de saída da 1ª vez, cheia | ex. onda 5 ≈ 436 | SpawnFromRecord (:375) | Não (§4) |
| Ritmo | Um a cada 30/N s, o 1º na hora | ReturnParadeSeconds 30 | :163-181, 479-481 | Não (§4) |
| Boca e velocidade | Boca sorteada (pode repetir), 0,5 cél/s | — | :345-363 | Não está no PLANO |
| O que a onda vira | Cancela o que faltava soltar; os já em campo seguem; toda onda que começa durante o desfile solta 0 creeps (bocas e ônibus seguem o rito); a contagem continua | IsSpawningHeld | BDWaveSubsystem.cpp:233-238, 670, 1175-1202 | Não (§4) |
| Agendado durante o retorno | Fica pendente e sai na próxima onda livre | — | :235-240 | Parcial (§4) |
| Matar retornante | Sem voto, propina ou divisória, mas **solta 1 ministro** | — | :523-578 | Parcial — §4 "nenhuma recompensa" |
| Matar todos | `EqualizeVotesDown`: azul e vermelho = min(azul, vermelho); no caso típico o **vermelho desce até o azul** | — | :576-588; BDMatchManager.cpp:514-527 | **Sim** — §4 "azul desce até o vermelho" |
| Nova virada | Depois do empate, o próximo vermelho dispara outro retorno completo | — | — | Não (§4) |
| Retornante na urna | Derrota | — | :495-514 | Não (§4) |
| Apuração | Espera também o retorno (inclusive os da fila) | — | IsEndDue (:490-499) | Não (§4/§8) |
| Save/load | Enviados, caídos (ordinal, onda, vida), candidato pendente e onda antecipada vão no save e voltam no load. O load não dispara virada nem saída antecipada mesmo com vermelho à frente: só o próximo voto | FBDCandidateSaveState | CaptureState/RestoreState; HandleVotesChanged (IsRestoring) | Não está no PLANO |

## 6. Palácio e Mito

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Preço | 3000 de dinheiro público, fixo; com os 2000 iniciais não dá no começo | PalaceCost 3000; DA_Palace.cost 0 (não usado) | GetBuildPrice (:1028-1033) | Não está no PLANO |
| Um só | Recusa `PalaceAlreadyBuilt`; sem cota | — | BDPlacementComponent.cpp:255, 439, 1360 | Não está no PLANO |
| 2×2 | Células Platform (bloqueiam caminho), validação de caminho, exige urna | footprint 2×2 | :1167-1196 | Não está no PLANO |
| Malha | SM_Palacio a 90% do footprint, Z×0,6; não colide com quem anda | footprint_fill 0,9 | BDPalace.cpp:36-92 | Não está no PLANO |
| Nível e estrelas | 0 a 5, começa em 0; **nada no jogo sobe**, só `BD.Palace.SetLevel`; 5 estrelas no telhado | Level 0; MaxLevel 5 | BDPalace.cpp:106, 139-155 | Não está no PLANO |
| Arma por nível | 6 entradas; só a Pistola é real (10 de dano, 1,5/s); 1–5 placeholders com os mesmos números; troca no tick em que o nível muda | — | BDPalaceData.cpp:37-70; BDAgent.cpp:650-679, 934-937 | Não está no PLANO |
| Venda e mover | Como as outras peças; o Agente é destruído junto | — | BDPalace.cpp:94-104 | Não (§3) |
| Nascimento | 1 Agente por palácio, na célula de dormir, barra cheia | PatrolTime 120 s | BDPalace.cpp:41-53; BDAgent.cpp:152-201 | Não está no PLANO |
| Corpo | SMK_Mito ×2, arma no socket RightHand, sem colisão | agent_mesh_scale 2 | BDAgent.cpp:80-176 | Não está no PLANO |
| Patrulha | Trechos de 1–3 células em linha livre (não cruza divisória nem Blocked; atravessa peças e creeps); 40% de parar 1–3,5 s | walk 0,5 cél/s; giro 540°/s | StartWalk (:305-365) | Não está no PLANO |
| Alcance | 3 células (2100 cm) contra a horda; **não persegue militantes**. Ministros: 3 + 0,25 × ministros de pé | DetectionRadiusCells 3; AgentReachStepCells 0,25 | :120-133 | Não está no PLANO |
| Alvo válido | Vivo, não chegou, não condenado, não intocável (abrigado por ministro ou candidato discursando), dentro do raio | — | :508-513 | Não está no PLANO |
| Quem primeiro | Candidato no alcance > mais próximo | bTargetFurthestAlong False | AcquireTarget (:515-551) | Parcial (§4) |
| Troca de alvo | Antes de cada tiro: só por um 15% mais perto (Dynamic); por candidato sempre | AimSwitchMargin 0,15 | :553-584 | Não está no PLANO |
| Tiro | Hitscan direto (sem projétil, sem reservar dano) | dano 10 | Fire (:726-755) | Não está no PLANO |
| Cadência | Notify `BD Shot` na anim (dispara mesmo desalinhado); sem notify, timer 1/1,5 s com tolerância de 12° | FireRate 1,5; AimTolerance 12° | :644-648, 898-908 | Não está no PLANO |
| Barra de 120 s | Corre sempre que acordado (com ou sem onda); para na volta para casa; ao zerar, volta | PatrolTime 120 | Tick (:958-970) | Não está no PLANO |
| Bônus por abate | +s de patrulha por abate, sem teto; não soma voltando nem dormindo | 1,0/0,85/0,70/0,55/0,40/0,25 s por nível | NotifyKill (:757-776) | Não está no PLANO |
| Volta para casa | Reto ou BFS até a célula de dormir; passa pela horda sem atirar, mas ainda chuta ministro | — | :367-398, 1011-1036 | Não está no PLANO |
| Onde dorme | Vizinha ao palácio: frente → direita → trás → esquerda | — | FindSleepCell (:239-284) | Não está no PLANO |
| Descanso | max(10; 45 − 0,5 × abates); dormindo não defende nem chuta; acorda sozinho com 120 s | 45 / 0,5 / 10 | GetRestTime; :400-436 | Não está no PLANO |
| Perseguição | Qualquer candidato vivo **que não esteja discursando**, a qualquer distância: corre a 0,75 cél/s, para a 75% do raio, ignora militantes e ministros; não persegue voltando para casa. No discurso o candidato some para ele (patrulha); no fim, persegue de novo | ChaseSpeedScale 1,5; ChaseCloseIn 0,75 | FindCandidate (:586-642) | Parcial (seção Comício) |
| Chute | Sem candidato no board, ministro no alcance tem prioridade sobre militante, mesmo com a barra vazia; chuta a ≤1 célula + corpo, alinhado em 12°, pé acerta em 0,45 s | KickReachCells 1; KickImpactSeconds 0,45 | :786-859 | Não está no PLANO |
| Falas | Abate: 9%, 15 s por falante, 4 s globais. Chute: sempre | CelebrationChance 0,09 | :761-767, 854-857 | Não está no PLANO |

## 7. Ministro

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Quando sai | 1 por candidato morto (agendado ou retorno), se houver alguém no STF | bReleaseOnCandidateKilled true | BDMinisterSubsystem.cpp:182-198 | Não está no PLANO |
| Quantos | Fila de assentos 1..11; fila vazia, não sai ninguém | MaxOnBoard 11 | :154-160, 200-211 | Não está no PLANO |
| Assento | Do 1º ao último; quem foge volta ao fim da fila e pode sair de novo | — | BDMinister.cpp:148-161 | Não está no PLANO |
| Saída de campo | Ministro só sai pelo chute do Mito. Vitória e "Continuar (endless)" não mexem neles (posição, estado, assento) nem na fila. Desfazer o tabuleiro (rebobinar para onda 0, load) tira todos e enche o STF de novo, assentos 1..11; partida nova (Play) é mundo novo | — | UBDMinisterSubsystem::ResetForNewMatch, chamado por ABDMatchManager::ResetBoardSystems | Não está no PLANO |
| De onde | STF = ator com SM_STF; porta = célula pisável mais próxima; vários saem lado a lado | StfMesh | :31-178 | Não está no PLANO |
| Aparência | Bloco preto 4×4×8; livro vermelho; cúpula esférica; sem colisão | BlockScale (4,4,8) | BDMinister.cpp:45-146 | Não está no PLANO |
| Entrada | Anda até a porta; aí não protege ninguém, mas já pode ser chutado | — | Tick 475-480 | Não está no PLANO |
| Passeio | Trechos de 1–8 células em direção aleatória (32 sorteios), 0,4 cél/s; 35% de parar 1–3 s | WalkSpeed 0,4; WalkCells 1/8; IdleChance 0,35 | :286-378 | Não está no PLANO |
| Separadores | Passa **por cima** de divisórias e atravessa peças; só Blocked e a borda o param | bOverDividers | Walk (:69-74) | Não está no PLANO |
| Raio do refúgio | 2,0 + 0,5 × (assento − 1): de 2 a 7 células | RefugeRadiusCells 2; Step 0,5 | BDMinisterSettings.h:137 | Não está no PLANO |
| Quem protege | Todo militante (nunca o candidato) no raio 2D, nos estados Walking, Idle e Reading; havendo alguém ele para e lê; cobertura +0,3 s renovada por frame; cúpula enquanto há alguém dentro | CoverHold 0,3 | UpdateRefuge (:200-256) | Não está no PLANO |
| O que bloqueia | Torres, personagens e Mito não miram o protegido; todo dano que chega se perde (vira nulo), inclusive tiro em voo; corpo pisca. Ele segue andando e pontua vermelho se chegar | ShelterBlinkHz 5 | BDTowerBase.cpp:428; BDAgent.cpp:509; BDEnemyBase.cpp:759 | Não está no PLANO |
| Fim da leitura | 5 s depois de o raio esvaziar; alguém entrando zera | ResumeAfterSeconds 5 | :237-264 | Não está no PLANO |
| Chute | Refúgio cai na hora; voo de 2 células (100/50/25% até achar chão), 1500 cm de altura, 1,4 s, 720°; grito = `FleeSound` do DA ou exclamação preta 1,6 s | FlightPushCells 2 etc. | ReceiveKick (:266-441) | Não está no PLANO |
| Queda e fuga | 1 s deitado → corre a 1,0 cél/s (por cima de divisórias) até o STF → destruído, assento ao fim da fila; não pode ser chutado de novo | DownSeconds 1; FleeSpeedScale 2,5 | :450-535 | Não está no PLANO |
| Dano | Nenhum: não é inimigo, não morre | — | BDMinister.h:43-60 | Não está no PLANO |
| DA | Por assento em `Ministers[Seat−1]`; lista vazia, então hoje ninguém tem DA; quando tiver, só `FleeSound` é lido | Ministers [] | :169-171; BDMinister.cpp:433 | Não (§14) |

## 8. Peças de defesa

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Classe | Torre e personagem = `ABDTowerBase` (personagem = `ABDShooter`); torre no chão, personagem só em slot | DA_Tower → DA_Tower_Test; DA_Character → DA_Personagem_01 | ResolveActorClass (:369-403) | Não |
| Teto | **Torre e personagem sem teto**: só dinheiro, célula e slot | — | HasBudgetCeiling (:1099-1106) | **Sim** — §6 orçamentos separados; §4 +1/+1 |
| Torre | 1 célula Tower, **andável**: não bloqueia caminho | — | IsWalkableState (BDGridSubsystem.cpp:238-255) | Não (§6) |
| Personagem | Recusado no chão; vai ao slot mais próximo do cursor | — | :346-367, 1381-1384 | Não |
| Plataformas | Slots vindos do BP; Palanque 1×1, Caminhão e Arquibancada 2×1; células Platform (bloqueiam). Nº de slots não extraído (o §12 confirma 6 no caminhão) | — | BDPlatformComponent | Não verificado (§6 diz 4/6/8) |
| Alcance na plataforma | ×1,25 fixo nas três (nenhum BP sobrescreve); altura não importa | RangeMultiplier 1,25 | BDTowerBase.cpp:378-379 | Parcial (§6) |
| Bloco | Evolui em bloco; deck sobe 120 cm por nível do bloco, tinta dourada (só visual) | FloorHeight 120 | BDPlatformComponent.cpp:97-164 | Não está no PLANO |
| Divisória | Na aresta; recusa borda do grid e aresta tomada; não bloqueia o Mito | InstancesPerEdge 2 | EvaluateEdgePlacement (:1199-1233) | Não (§6) |
| Validação | `WouldBlockPath`/`WouldBlockPathEdges` (nenhuma boca perde a urna) + `WouldCutOffCells` (creeps em campo) | — | BDPathfinder.cpp:134-242+ | Parcial |
| Escolha de alvo | Creep com **menos caminho restante dentro do alcance**; `TargetPriority` ignorado | — | AcquireTarget (:445-478) | Sim (§4) |
| Re-olhar | Antes de cada tiro troca para um creep mais adiantado (margem de 1 cm) | — | Tick (:586-596) | Não está no PLANO |
| Giro | Só yaw, 360°/s; atira com erro ≤ 5° | TurnRate 360; AimTolerance 5 | :487-509 | Não está no PLANO |
| Reconhecimento | 0,3 s para alvo novo, só se estava ociosa há > 1 s | AcquisitionDelay 0,3; ReacquireGrace 1,0 | :543-580 | Não está no PLANO |
| Todos condenados | Vida − dano a caminho ≤ 0 não é alvo; se todos estão assim, segura o fogo e acompanha o mais adiantado | — | IsDoomed; :142-167, 559-571 | Não está no PLANO |
| Alcance | Base × (1+0,1·(N−1)) × plataforma. Torre 4 → 6 células; Nicole 3,75 → 5,625 | — | GetRangeCellsAtLevel (:359-380) | Não está no PLANO |
| Dano | Torre 10 → 40 no nível 6 | — | :201-217 | Não (§7) |
| Cadência | Não muda com o nível | Torre 1,2/s; Nicole 2,0/s | :219-223 | Não está no PLANO |
| Pente | Torre contínuo; Nicole 6 tiros + 1,5 s de recarga; munição infinita | — | :520-528, 622-635 | Não (§6) |
| Projétil | Teleguiado, 12 cél/s, acerta a 30 cm, some em 10 s; dano reservado no alvo; Area implementado, Pierce = Single | — | BDProjectileBase.cpp:46-174 | Parcial (§6 Pierce) |
| Arco de tiro | Não existe: mira 360° | — | — | **Sim** (§6) |
| Coice | Recua 12% em 0,14 s, só em plataforma; no corpo da Nicole talvez não apareça | — | :121-140 | Não está no PLANO |
| Nicole | Gesto por tiro acelerado para a cadência; 6 armas por nível, só a Pistola é real (2–6 usam a escada e a malha/som da Pistola); comemora 9% | — | BDShooter.cpp | Parcial — §7 "skin por estrela" |
| Estrelas | Cheias = nível − 1, em 6 posições; a plataforma mostra a fileira do bloco | — | BDMatchHUD.cpp:323-359 | Parcial (§7) |

## 9. Dificuldade, vitória, saves

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Escolha | Pelo menu; sem escolha (PIE/Continue) vale Normal (membro do ator); `DefaultDifficulty` das settings não é lido | — | BDMatchManager.cpp:93-102 | Não (§8) |
| O que muda | Só divisórias (100/80/60), plataformas (10/8/5) e 1ª contagem (90/120/45). ObstacleCount (30/20/12) sem efeito. Dinheiro, saves, presos e ondas para vencer iguais | — | DA_Difficulty_* | **Sim** — §8 escalada de ondas e obstáculos |
| WavesToWin | 100 nos três | — | :529-532 | Não (§8) |
| IsEndDue | !venceu, onda ≥ 100, sem candidato vivo, **no ônibus**, **pendente** nem retorno ativo | — | IsEndDue (BDMatchManager.cpp) | Parcial (§8 cita só o candidato andando) |
| Quando é avaliado | Na limpeza da onda (depois de divisórias e WaveLog) e em todo tick de Building | — | :308-313, 476-480 | Não (§8) |
| Apuração | Azul ≥ vermelho = vitória (empate vence), senão derrota | — | ResolveEnd (:501-512) | Não (§8) |
| Ordem das condições | (1) candidato na urna → derrota imediata; (2) limpeza: divisórias → WaveLog → IsEndDue → ResolveEnd → senão Building; (3) tick de Building: FreezeTimer → IsEndDue → ResolveEnd → contagem. Último retornante morto na onda ≥100 → nivelamento → empate → vitória | — | vários | Não está no PLANO (ordem) |
| Vitória | `bWon`, grava progresso, **apaga o save**, zera dinheiro, congela o board | — | DeclareVictory | Não está no PLANO (dinheiro zerado, save apagado) |
| Presos | Só no painel e no log | 2/2/2 | :534-537 | **Sim** — §8 1/2/3 |
| Derrota | WaveLog + PostMatch, zera dinheiro; não desfaz vitória anterior | — | :751-769 | Não (§8) |
| Endless | Só após vitória; começa sem dinheiro; só termina com candidato na urna (vermelho à frente não perde) | — | :735-749 | Não (§8) |
| Saves: quando | Em Building com saves restantes, **urna no chão**, **sem candidato andando ou no ônibus e sem desfile** (inclusive antes da onda 1 e no endless). Cada save desconta 1 | 2/2/2 (+1 cadeia) | GetSaveRefusal | **Sim** — §8 3/2/1 (decisão do usuário) |
| Saves: o que grava | Um ponto de restauração por partida, sobrescrito a cada save. Slot do jogador "BDMatch"; **testes e regressão usam "BDMatch_Test"** (headless `-unattended`, `-BDTestSlot` ou `SetTestSlot`). Grava dificuldade, seed, `MatchStart`, onda, votos, dinheiro, velocidade, cotas, peças, **bocas**, **candidatos** (enviados, caídos, pendente), **LevelDownCount** e **tempo restante da contagem** | — | SaveMatch; UBDMatchSave | Não (§8) |
| Load | Na partida, **só depois da derrota** e só o save **da mesma partida** (dificuldade e `MatchStart`); pelo Continue do menu, assume a dificuldade do save. Reabre a montagem da onda salva com o tempo que ela tinha; não gasta save; tira os ministros; não solta candidato. O slot é **apagado na vitória e no Play**; a derrota o mantém. `BD.Save.Load` segue a mesma trava | — | CanLoadSave / LoadMatch / RestoreMatch; UBDUISubsystem::PlayGame | Não (§8) |
| Progresso | Dificuldades vencidas no slot "BDProgress"; **testes usam "BDProgress_Test"** (mesma regra do save), para uma vitória de teste não dar bônus em cadeia ao jogador | — | UBDProgressSave::GetSlotName | Não está no PLANO |
| Abandono | Linha "Abandoned" no PostMatch ao sair ou carregar por cima | — | :157-169 | Não está no PLANO |

## 10. Ambientação que afeta jogo

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| Dia por onda | Posição = onda mod 16, muda no **início** da onda | WavesPerCycle 16 | BDDayCycleComponent.cpp:112-147 | Não (§9) |
| Fatia de noite | A noite (≈35% do céu) ocupa só 18% das ondas: ondas ≡13, 14, 15 (mod 16) | NightShare 0,18 | SkyAlphaForProgress (BDDaySettings.cpp:23-53) | Não está no PLANO |
| Sol | Segundos reais, sempre para a frente; para na pausa; continua no board congelado do fim | BlendSpeed 0,03 | :185-226 | Não (§9) |
| Sem efeito no combate | Nada de dano, alcance ou spawn depende da hora | — | — | Não |
| Postes | Acendem pela elevação do sol (apagados acima de −3°, cheios abaixo de −12°). **Cada poste bloqueia a própria célula** | — | BDStreetLightComponent.cpp:44-66, 176-205 | Parcial — célula bloqueada não está no PLANO |
| Música | Só em WaveActive, tempo real (continua na pausa); ambiência cai para 0,5 sob a música | — | BDSoundscapeSubsystem.cpp:118-170 | Não está no PLANO |
| Teto de falas | 2 simultâneas, lados opostos da câmera, ≥30°; horda mira 0,2 fala/s; fala do jogador toma a vaga de militante. **No discurso de um comício o teto é 1, do candidato**: as outras falas são cortadas e recusadas (inclusive as "sempre ditas", chute e grito do ministro), e volta a 2 no fim | SpeechMaxConcurrent 2 | CanSpeak; TakeFloor / GiveBackFloor / SayOnFloor | Parcial (seção Comício) |
| Ducking | Fala baixa efeitos e ambiência a 0,5 (fade 0,4 s) | — | :327-359 | Não está no PLANO |
| Bipe da urna | 1 por voto vermelho, intervalo mín. 0,4 s, "pesado" a partir de 200, máx. 4 | — | BDObjectiveSubsystem.cpp:219-260 | Não (§9) |

## 11. Elenco (seção 14)

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está | Diverge do PLANO? |
|---|---|---|---|---|
| DAs | 20 candidatos, 11 ministros, 4 personagens, 4 hordas (39) em `Content/Elenco` | CastSettings 20/11/4/4 | — | Não (§14) |
| Em jogo | Horda_01 (onda e multidão dos comícios), Candidato_01 (corpo de todos os candidatos), Candidato_01..20 (comício de cada número), Personagem_01 (único em `Shooters`); nenhum ministro | — | ini; settings; UBDCandidateData::LoadForOrdinal | Não (§14) |
| Enemy, lidos | MaxHealth, MoveSpeed, Acceleration, EnemyClass, Mesh, SkeletalMesh, MeshScale, MeshYaw, MeshMaterial, SkinMaterials, MoveAnimation, DeathAnimations, sons (Speech, Call, Footstep, Death, BodyFall, pitch, intervalo), KillType, KillIcon, DisplayName | — | BDEnemyBase; BDCreepSoundSubsystem | Não está no PLANO |
| Enemy, não lidos | VotesOnArrival (nunca), VotesOnDeath (só no modo desligado), Skeleton, IdleAnimation, **CandidateFace** (só o audit) | — | — | Sim — §14 ícone do candidato = `CandidateFace` |
| Candidate (`UBDCandidateData`) | Os 20 candidatos são `UBDCandidateData` (subclasse de `UBDEnemyData`, convertidos no lugar pelo `BDScaffoldCast`); `Rally` (FBDRallyParams) com os defaults do número. Slot novo no audit: `Sound.Rally` (RallySpeech) — audit 431 vazios (candidatos 219) | — | BDCandidateData.h; BDScaffoldCastCommandlet.cpp (1b) | Não (§14) |
| Minister | Só FleeSound, e só com o DA ligado em `Ministers` (vazio); o resto só no audit | — | BDMinister.cpp:433 | Não (§14) |
| Shooter, lidos | BodyMesh, BodyScale, BodyYaw, HandSocket, Idle, Shoot, CelebrationSound, IdleAfter, ImpactEffect; armas: Mesh, Grip, Muzzle, Damage, FireRate, Flash, FireSound; herdados da torre | — | BDShooter.cpp | Não |
| Shooter, não lidos | TargetPriority, UnlockWave; Name e bPlaceholder das armas só em log | — | — | Não está no PLANO |
| Nomes | `BDLoc`: Enemy.<asset> → Enemy.NN/Shooter.NN → DisplayName. Nome do candidato no HUD é fixo no código | — | BDLocalization.cpp:79-136 | Parcial (§14) |

---

## Settings e DataAssets (campos de gameplay)

### UBDGameBalanceSettings
| Campo | Valor atual | Default (se diferente) | O que faz |
|---|---|---|---|
| DifficultyAssets | Easy/Normal/Hard → DA_Difficulty_* | vazio | Liga dificuldade ao DA |
| DefaultDifficulty | Normal | | **Não lido** |
| BaseWaveDelay / Decay / Min | 45 / 0,94 / 10 | | Contagem entre ondas |
| EarlyCallBonusPerSecond | 2 | | Bônus de chamar antes; nunca gasto |
| DividerRemovalGraceWave | 3 | | Carência da divisória (inócua, custo 0) |
| BuildingPhaseRefundRatio / SellRefundRatio | 1,0 / 0,5 | | Frações de venda |
| CandidateHealthMultiplier | 40 | | Vida do candidato |
| CandidateSpeed | 0,5 | | Velocidade do candidato |
| CandidateInterval | 5 | | Ondas entre candidatos |
| CandidateGroupStep | 20 | | Grupo no endless |
| CandidateLeadSeconds | 5 | | Atraso da horda na onda do candidato |
| ReturnParadeSeconds | 30 | | Desfile |
| bRallies | True | | Liga os comícios (chave geral, para medir sem eles) |
| bRallyOnReturn | False | | Retornante discursa? |
| RallyRiseSeconds | 0,6 | | Brotar da multidão (o candidato espera) |
| RallyMaxSteps | 20 | | Até onde a multidão procura lugar |
| RallyTellSound | SCue_Bus_Horn | vazio | Som curto do tell (placeholder) |
| RallyAuraRadiusCells / RallyAuraPulseHz | 1,5 / 1 | | Disco do discurso |
| RallyAuraMesh / RallyAuraMaterial | Cylinder / M_RallyAura | vazio | Malha e material do disco |
| HealthScaleByWave | None | | Curva desligada |
| HealthScaleGrowth | **1,022** | 1,12 | Crescimento da vida |
| WaveSpawnInterval Base/Decay/Min | 0,35 / 0,97 / 0,20 | | Intervalo entre creeps |
| CreepsPerSpawnPoint Base/Step | 1 / 1 | | Creeps por boca |
| Min/MaxActiveSpawnPoints | 1 / 0 (= todas) | | Bocas ativas |
| bVotesByBody | True | | Votos por corpo |
| HealthPerVote / bBlueVotesByHealth / RedVoteWeight | 1 / True / 1 | | Sem efeito com bVotesByBody |
| MouthWanderChance / MaxCells | 0,75 / 5 | | Deslize das bocas |
| Reference* (MaxLevel 5, Tower 8, Character 12, Engagement 0,5) | | | Só relatórios |
| UpgradeCostGrowth | 3 | | Custo de evolução |
| Damage/RangeGrowthPerLevel | 0,6 / 0,1 | | Escada de níveis |
| HealthPerBribe | **1,0** | 2,5 | Propina |
| EvolutionRefundRatio | 0,6 | | Reembolso de evolução |
| ReferencePiecePrice / ReplacementCostRatio / ReplacementReferenceCost | 400 / 1 / 100 | | Fórmula de preço |
| PalaceCost | 3000 | | Palácio |
| MoveTax Initial/Step/Max | 0 / 0,02 / 0,25 | | Taxa de mover |
| AllowedGameSpeeds | [1,2,4] | | Velocidades |

### UBDDifficultyData
| Campo | Easy | Normal | Hard | Default | O que faz |
|---|---|---|---|---|---|
| Difficulty | Easy | Normal | Hard | Normal | **Não lido** (vale a chave do mapa) |
| DividerBudget | 100 | 80 | 60 | 28 | Cota inicial de divisórias |
| DividersPerWave | 1 | 1 | 1 | | Por onda limpa |
| DividersPerCandidate / Step | 4/1 | 4/1 | 4/1 | | Por candidato morto |
| PlatformBudget | 10 | 8 | 5 | 3 | Cota de plataformas |
| StartingFunds | 2000 | 2000 | 2000 | | Dinheiro inicial |
| ObstacleCount | 30 | 20 | 12 | 20 | Sem efeito (geração desligada) |
| FirstWaveDelay | 90 | 120 | 45 | 120 | 1ª contagem |
| WavesToWin | 100 | 100 | 100 | | Ondas para vencer |
| PrisonersFreed | 2 | 2 | 2 | | Só exibido |
| SaveBudget | 2 | 2 | 2 | | Saves |
| ChainBonus | 400/4/1/1 | 400/4/1/1 | 400/4/1/1 | | Bônus em cadeia |

### UBDPlaceableData
| Campo | Divider | Palanque | Truck | Bleachers | Tower | Character | Palace | Objective |
|---|---|---|---|---|---|---|---|---|
| ActorClass | BP_Divider | BP_Palanque | BP_Truck | BP_Bleachers | — | — | — | BDObjective |
| TowerData / PalaceData | — | — | — | — | DA_Tower_Test | DA_Personagem_01 | DA_PalaceData | — |
| Cost → preço | 10 → **0** | 40 → 160 | 100 → 400 | 80 → 320 | (100) → 400 | (40) → 160 | 3000 | 0 |
| Footprint / OccupiesAs | aresta | 1×1 Platform | 2×1 Platform | 2×1 Platform | 1×1 Tower | slot | 2×2 Platform | 1×1 Goal |

### UBDTowerData / UBDShooterData
| Campo | DA_Tower_Test | DA_Personagem_01 (Nicole) | DA_Character_Test | Default | O que faz |
|---|---|---|---|---|---|
| BuildCost | 100 | 40 | 40 | 0 | Vira preço pela fórmula |
| UnlockWave | 0 | 0 | 0 | | **Não lido** |
| UpgradeCostBase | 40 | 40 | 40 | 50 | Custo do nível 2 |
| CanPlaceOnGround / OnSlot | T / F | F / T | F / T | | Chão ou slot |
| TargetPriority | First | First | First | | **Não lido** |
| TurnRate / AimTolerance / AcquisitionDelay | 360 / 5 / 0,3 | idem | idem | | Mira |
| MagazineSize / ReloadTime | 0 / 0 | 6 / 1,5 | 6 / 1,5 | 0 | Pente |
| DamageType / AreaRadius | Single / 1 | Single / 1 | Single / 1 | | Tipo de dano |
| Levels[0] | 10 dano, 1,2/s, 4 cél, 12 cél/s | 10, 2,0/s, 3, 12 | igual à Nicole | | Nível 1 |
| Weapons (Shooter) | — | Pistola 10/2,0; armas 2–6 0/0 | — | | Arma por nível |

DA_Character_Test não é apontado por nenhuma peça.

### UBDEnemyData
| Campo | DA_Horda_01 | DA_Candidato_01 | Default | O que faz |
|---|---|---|---|---|
| MaxHealth | 10 | 100 (só fallback) | 100 | Vida base |
| MoveSpeed | 1,5 | 0,5 (sobreposto por CandidateSpeed) | 1,0 | Velocidade |
| Acceleration | 3 | 1 | 2 | Aceleração |
| VotesOnDeath | 1 | 0 | 1 | Sem efeito hoje |
| VotesOnArrival | 1 | 0 | 1 | **Não lido** |
| EnemyClass | None | BDCandidate | | Classe do ator |

### UBDCandidateData (DA_Candidato_NN), campo Rally
| Campo | DA_Candidato_01 | DA_Candidato_20 | Default (struct) | O que faz |
|---|---|---|---|---|
| bRallies | True | True | True | Discursa |
| RallyFirstDelay | 6 | 6 | 6 | 1º comício após o ônibus |
| RallyEvery / RallyEveryJitter | 20 / 0,2 | 10 / 0,2 | 20 / 0,2 | Intervalo e sorteio |
| RallyTellSeconds | 1,5 | 1,5 | 1,5 | Aviso |
| RallyDuration / RallyMinDuration | 4 / 2,5 | 4 / 2,5 | 4 / 2,5 | Discurso sem cue / piso |
| RallyCount | 2 | 40 | 0 (= nunca preenchido: usa o default do número) | Multidão |
| RallyComposition | DA_Horda_01 ×1 | DA_Horda_01 ×1 | vazio (= creep da onda) | Tipos e pesos |
| RallySpeech | vazio | vazio | vazio | Fala do comício (sem ela, SpeechSound) |

### UBDPalaceData (DA_PalaceData)
| Campo | Valor | O que faz |
|---|---|---|
| Level | 0 | Nível inicial (só o console muda) |
| WalkSpeed / WalkCells / IdleChance / IdleTime | 0,5 / 1–3 / 0,4 / 1–3,5 | Patrulha |
| TurnRate | 540 | Giro |
| PatrolTime | 120 | Barra |
| RestTimeBase / ReductionPerKill / Min | 45 / 0,5 / 10 | Descanso |
| ChaseSpeedScale / ChaseCloseIn | 1,5 / 0,75 | Perseguição |
| DetectionRadiusCells | 3 | Alcance |
| bTargetFurthestAlong | False | Mais próximo |
| AimSwitchMargin / AimTolerance | 0,15 / 12 | Troca e tolerância |
| Weapons Damage/FireRate | 10 / 1,5 nas 6 | Arma por nível |
| Weapons KillBonusSeconds | 1,0 / 0,85 / 0,70 / 0,55 / 0,40 / 0,25 | Bônus por abate |
| Weapons AimMode | Dynamic | Re-escolhe a cada tiro |

### UBDPlacementSettings
| Campo | Valor | O que faz |
|---|---|---|
| Palette | Divider, Palanque, Truck, Bleachers, Tower, Character, Palace | Barra |
| Unlocks | Truck 10; Bleachers 25 | Desbloqueio |
| Shooters | [DA_Character] | Lista de personagens |
| RotateAction | IA_Rotate | **Não lido** |

### UBDWaveSettings
| Campo | Valor | O que faz |
|---|---|---|
| WaveEnemy / DebugEnemy / CandidateEnemy | Horda_01 / Horda_01 / Candidato_01 | Quem joga |
| ArrivalDistance | 150 | Chegada |
| BusMoveSeconds / MinBusGap | 2 / 2 | Ônibus |
| CornerSmoothRadius / LateralOffsetMax / LaneWander / CornerRadiusVariance | 0,35 / 0,4 / 0,6 / 0,4 | Movimento |
| SpeedVariance | 0,1 | Ritmo |
| RouteCostVariance / RouteVarianceMode | 1,3 / PerCreep | Variação de rota |

### UBDTowerSettings
| Campo | Valor | O que faz |
|---|---|---|
| HitDistance | 30 | Acerto |
| MaxLifetime | 10 | Vida do projétil |
| ReacquireGrace | 1,0 | Isenção do reconhecimento |

### UBDMinisterSettings
| Campo | Valor | O que faz |
|---|---|---|
| bReleaseOnCandidateKilled / MaxOnBoard | True / 11 | Saída |
| WalkSpeed / WalkCells / IdleChance / IdleTime | 0,4 / 1–8 / 0,35 / 1–3 | Passeio |
| KickReachCells / AgentReachStepCells / KickImpactSeconds | 1 / 0,25 / 0,45 | Chute |
| FlightSeconds / FlightPushCells / DownSeconds / FleeSpeedScale | 1,4 / 2 / 1 / 2,5 | Voo e fuga |
| RefugeRadiusCells / Step / ResumeAfterSeconds | 2 / 0,5 / 5 | Refúgio |
| BlockScale | (4,4,8) | Corpo (entra no alcance do chute) |
| Ministers | [] | DA por assento |

### UBDObjectiveSettings / UBDObstacleSettings / UBDGridSettings / UBDBribeSettings / UBDCastSettings
| Classe.Campo | Valor | Default | O que faz |
|---|---|---|---|
| Objective.ObjectivePlaceable | DA_Objective | | Urna |
| Objective.bRestrictToZone | False | | Zona desligada |
| Objective.MinX/MaxX/MinY/MaxY | 40/47/6/15 | | Sem efeito |
| Obstacle.bGenerateObstacles | False | | Desliga todo o gerador |
| Obstacle.ObstacleCount/Clearance/Path/FreeRatio/MaxAttempts/Fallback | 20/3/25–200/0,6/50/[] | | Gerador (inativo) |
| Grid.GridSizeX × Y | 48 × 22 | 54 × 22 | Tamanho do tabuleiro |
| Grid.CellSize | 700 cm | 200 | Unidade de alcance e velocidade |
| Grid.GridOrigin | (−39100; 327,93; −11) | (0,0,0) | Posição no mundo |
| Bribe.Bag/Count/Register/Transfer | 3 / 1,2 / 0,35 / 0,9 s | | Atraso da propina (≈5,45 s) |
| Cast.Candidatos/Ministros/Personagens/Hordas | 20/11/4/4 | 0 | Só audit, regressão e commandlet |

Sem campo de gameplay: UBDAudioSettings, UBDBloodSettings (sem textura, sem mancha), UBDShotSoundSettings, UBDCreepSoundSettings (só os tetos de fala da §10), UBDCameraSettings, UBDDaySettings (NightShare só muda o céu), UBDUISettings. `UBDSettings` não existe no código.

---

## Regras só no código (não estão no PLANO §1–11)

| # | Regra |
|---|---|
| 1 | Setup dura um frame; a contagem da onda 1 só corre com a urna no chão |
| 2 | Construir, vender, mover e evoluir durante a onda e com o jogo pausado |
| 3 | Peça na mão atravessa ondas; cai em Setup, Victory e Defeat |
| 4 | Ordem fixa das recusas na mão |
| 5 | Chamar a onda antes acumula bônus que nada gasta; sem botão |
| 6 | +1 divisória por onda limpa; +(4+N−1) por candidato agendado morto; cota de plataformas 10/8/5 |
| 7 | Propina em fila, ≈5,45 s até virar dinheiro; vitória e derrota zeram o dinheiro; endless começa com 0 |
| 8 | Fórmula de preço fixa (400 × BaseCost/100); palácio 3000; divisória e urna grátis |
| 9 | Desbloqueios: Caminhão onda 10, Arquibancada 25 |
| 10 | Evolução em bloco na plataforma; reembolso de 60% da evolução; personagens caem com a plataforma sem reembolso de compra |
| 11 | Voto nulo inclui dano bloqueado por ministro e usa "um corpo da onda" como unidade |
| 12 | Virada sem caídos antecipa o próximo candidato |
| 13 | Candidato espera os ônibus; a horda espera +5 s |
| 14 | Boca do candidato sorteada com semente entre todas as bocas com rota |
| 15 | Grupo de candidatos 1 + caídos/20 (endless) |
| 16 | Todo candidato morto, inclusive de retorno, solta um ministro |
| 17 | Onda iniciada durante o desfile solta 0 creeps |
| 18 | A onda não espera o candidato para limpar |
| 19 | Total = W × todas as bocas; round-robin global; intervalo max(0,2; 0,35 × 0,97^W) |
| 20 | Coleira de 5 da âncora; gap de 2 entre ônibus; boca bloqueada limitada à coleira |
| 21 | Espera de 2,0 s do ônibus e 1,5 s do motor (vale o maior); buzina no 1º creep |
| 22 | Variação de rota por creep; reroteamento imediato; `WouldCutOffCells` |
| 23 | Movimento orgânico (faixa, curvas, ritmo) |
| 24 | Corpos (máx. 40), grito, queda, passos, chamados com orçamentos próprios; sangue desligado por falta de textura |
| 25 | Mira de torre: mais adiantado no alcance, re-olhar antes do tiro; reconhecimento 0,3 s só após 1 s ocioso; "todos condenados" |
| 26 | Coice só em plataforma; deck sobe 120 cm por nível do bloco (visual) |
| 27 | Palácio inteiro (§6) |
| 28 | Mito inteiro (§6) |
| 29 | Ministro inteiro (§7) |
| 30 | Apuração espera o desfile do retorno; vale a contagem do momento da resolução (pode ser depois da 100); último retornante morto na onda ≥100 empata e vence |
| 31 | Endless só termina com candidato na urna |
| 32 | Save: um ponto de restauração por partida; só em Building, com urna no chão e sem candidato andando ou desfile; slot apagado na vitória e no Play; load só após derrota (ou Continue); testes em slots próprios (BDMatch_Test, BDProgress_Test) |
| 33 | O load tira os ministros e não solta candidato mesmo com vermelho à frente: só o próximo voto vira o placar |
| 34 | Linha "Abandoned" no PostMatch |
| 35 | Noite comprimida (3 de 16 ondas) |
| 36 | Poste bloqueia a própria célula |
| 37 | Música só em WaveActive |
| 38 | Teto de 2 falas em lados opostos; prioridade da fala do jogador; ducking a 0,5 |
| 39 | Nome do candidato no HUD fixo ("Candidate N") |
| 40 | Comício: a multidão não entra no total W × bocas (conta em `CreepsRaised`); procura lugar até 20 passos e o que não acha não brota; o tell usa a buzina do ônibus; o candidato fica parado e vulnerável os 0,6 s do brotar; o comício vem do DA do número e o corpo ainda do 01 |

## Divergências PLANO × código

| Seção do PLANO | O PLANO diz | O código faz |
|---|---|---|
| §2.1 | Urna numa zona restrita | Qualquer célula livre e alcançável (`bRestrictToZone` False), como o §5 |
| §2.2 | Montagem entre ondas, gastando votos | Cobra dinheiro público; constrói também durante a onda |
| §3 | Uma moeda só: votos = placar e dinheiro | Votos (placar) separados de propina + dinheiro público; nada gasta voto |
| §3 | Azul e vermelho valem o HP (K=1) | +1 por corpo (`bVotesByBody`); campos de HP sem efeito; comentário do ini contradiz |
| §3 | Nulo na mesma taxa do HP | Nulo em unidades de "um corpo da onda" (cresce com a onda) |
| §3 | Gastar votos derruba o azul e pode virar | Gasto nunca mexe no azul; não há virada por gasto |
| §3 | A interface mostra o placar resultante | HUD mostra o dinheiro resultante |
| §3 | Construir gasta BuildCost; janela livre da divisória até a onda 3 | Preço por fórmula em dinheiro; divisória 0 e devolve a cota sempre |
| §4 | Candidato é alvo prioritário de todo defensor | Só o Mito prioriza (a qualquer distância); torres e personagens não |
| §4 | Agendado dá +1 torre e +1 personagem de teto | Dá propina (= vida) e divisórias; não há teto de defensores |
| §4 | Os que voltam não dão nada | Não dão voto nem dinheiro, mas soltam ministro |
| §4 | Nivelamento: azul desce até o vermelho | Os dois a min(azul, vermelho); no caso típico o vermelho desce |
| §4 | Virada só dispara o retorno | Sem caídos, antecipa o próximo candidato |
| §5 | Boca desliza 1–5; bloqueada vai à posição aberta mais próxima | −5..+5 com 0, até 6 tentativas, coleira de 5 da âncora |
| §6 | Duas famílias com orçamentos separados | Torre e personagem sem teto; cotas só de divisória e plataforma |
| §6 | Arco de tiro; tipo de dano | Sem arco; Pierce = Single |
| §6 | Multiplicador de alcance por altura | 1,25 fixo nas três plataformas, independente dos andares |
| §7 | Níveis até 10 | 6 |
| §7 | Vida ~1,12–1,15 por onda | 1,022 |
| §7 | Cinco estrelas trazem skin nova | Troca de arma por nível (placeholders); 6 posições de estrela, 5 preenchíveis |
| §8 | HealthScaleGrowth 1,035 | 1,022 |
| §8 | Dificuldades diferem em escalada e obstáculos | Escalada global; obstáculos desligados; só cotas e 1ª contagem mudam |
| §8 | Presos 1/2/3 | 2/2/2 |
| §8 | Saves 3/2/1 | 2/2/2 |
| §8 | Apuração espera o candidato andando | Espera também o candidato no ônibus, o pendente e o desfile do retorno |
| §9 | Ônibus marcam as bocas ativas | Todos seguem sua boca; só as ativas têm motor e buzina |

## Comandos de console (gameplay)

| Comando | Argumentos | O que faz | Onde |
|---|---|---|---|
| BD.Match.Status | — | Fase, onda, contagem, cotas | Match/BDMatchManager.cpp |
| BD.Match.CallWave | — | Solta a onda agora (acumula bônus) | idem |
| BD.Match.ClearWave | — | Dá a onda por limpa | idem |
| BD.Match.SetWave | `<wave>` | Pula o contador | idem |
| BD.Match.SetPhase | `<Building\|WaveActive>` | Força a fase (Building zera a onda) | idem |
| BD.Match.Speed | `<1\|2\|4>` | Velocidade | idem |
| BD.Match.AddMoney | `<n>` | Dinheiro público | idem |
| BD.Match.Endless | — | Endless após vitória | idem |
| BD.Match.FreezeTimer (cvar) | 0/1 | Congela a contagem | idem |
| BD.Save.Write / Load / Status | — | Gasta save e grava / reconstrói do slot (sem trava de fase) / estado | idem |
| BD.Progress.Status / SetWon / Reset | `[dif]` | Progresso entre dificuldades | idem |
| BD.Balance.Report | `[wave]` | Vida da horda × defesa de referência | idem |
| BD.Votes.Status / AddBlue / AddRed | `<n>` | Votos (AddRed testa a virada) | Wave/BDWaveDebug.cpp |
| BD.Wave.Spawn / SpawnAll / SpawnLoop / KillAll / Status | vários | Creeps manuais | idem |
| BD.HUD.Debug (cvar) | 0/1 | Placar de teste | Debug/BDDebugScoreboard.cpp |
| BD.Candidate.Spawn / Return / Status / Rally | `[ordinal]` (Rally) | Candidato agora, retorno, estado (com o comício: estado, próximo em N s, palavra), comício agora | Candidate/BDCandidateDebug.cpp |
| BD.Bribe.Status / Grant / Flush / Report | `[n]` | Propina e Casa da Moeda | Bribe/BDBribeDebug.cpp |
| BD.Economy.Report | — | Preços, desbloqueios, propina | idem |
| BD.Place.Select / Take / SelectTransient / EmptyHand | vários | Mão | Placement/BDPlacementDebug.cpp |
| BD.Place.At / AtSlot / AtEdge / Click | `<x> <y> …` | Colocar | idem |
| BD.Place.Sell / RemoveAt / SellAtEdge / RemoveAtEdge | `<x> <y> …` | Vender (devolve dinheiro, não votos) | idem |
| BD.Place.PickUp / PickUpEdge / CancelMove | `<x> <y> …` | Mover | idem |
| BD.Grid.CellState / SetEdge / SetCells / Audit / SaveLayout | vários | Grid | Grid/* |
| BD.Path.WouldBlock / WouldBlockEdges / Test | vários | Validação de caminho | Path/BDPathfinder.cpp |
| BD.Objective.PlaceAt / Status | `<x> <y>` | Urna | Objective/BDObjectiveDebug.cpp |
| BD.Obstacles.Generate | `<seed>` | Gerador (respeita o flag desligado) | Obstacle/BDObstacleGenerator.cpp |
| BD.Tower.Status / Upgrade / UpgradeCost / UpgradeAll / SetAllLevels | vários | Defensores | Tower/BDTowerDebug.cpp |
| BD.Palace.Status / SetLevel | `<0-5>` | Nível do palácio (troca a arma do Mito) | Palace/BDPalace.cpp |
| BD.Agent.Status / SetTime / Wake / Kick | vários | Mito | Palace/BDAgent.cpp |
| BD.Minister.Spawn / Status / Clear | `[n] [agent]` | Ministros | Palace/BDMinisterSubsystem.cpp |
| BD.Test.Regression | `[quit]` | Regressão | Debug/BDRegressionSubsystem.cpp |
| BD.Sim.Run / BD.Debug.AutoEvolve | vários | Simulação | Debug/BDSimSubsystem.cpp |
| BD.Debug.AutoSetup(.Run, cvars) | vários | Defesa automática | Debug/BDDebugAutoSetup.cpp |
| BD.Debug.PerfScene / ClearAll | vários | Cena de medição / limpar tudo | idem |
| BD.Debug.Later / BD.Delay | `<s> <cmd>` | Comando atrasado (real / jogo) | Debug, UI |
| BD.UI.Play / GamePause / Pause / Menu / Status | vários | Fluxo de telas e pausa | UI/BDUIDebug.cpp |
| BD.Cast.Audit | — | Slots vazios do elenco | Cast/BDCast.cpp |
| BD.WaveLog.Enabled / BD.PostMatch.Enabled (cvars) | 0/1 | Relatórios CSV | Report/* |

---

## Dúvidas que o código não responde

| # | Ponto |
|---|---|
| 2 | **Onda com 0 creeps** (iniciada durante o desfile) só limpa quando algum inimigo sai da lista; não há garantia de que sempre exista esse gatilho. Há outra chamada a `OnWaveCleared` (BDMatchManager.cpp:1283) não lida. |
| 4 | **Venda depois do fim**: `CanRemove`/`SellPiece` não olham a fase; não confirmado se a entrada do jogo segue ativa com o painel de fim. |
| 5 | **Creep sem rota após reroteamento**: o comentário diz que ele para, mas o código acrescenta a urna como waypoint (andaria reto atravessando barreiras). `WouldFenceInCreeps` deve impedir o caso. |
| 7 | **Ordinal das divisórias** = ordem de saída, não de morte; com grupos (endless) a escada pode sair fora de ordem. |
| 8 | **Candidato que morre pela 1ª vez durante um retorno** entra nos caídos mas não no desfile em curso. Intenção não documentada. |
| 9 | **Onda pendente que coincide com onda devida**: sai um grupo só; o candidato devido parece se perder. |
| 10 | **Candidato numa boca não ativa**: pode sair de boca sem ônibus estacionado. Não verificado visualmente. |
| 12 | **Ministro na entrada** (Entering) não protege ninguém: deliberado? |
| 13 | **Espera do motor** só ocorre com áudio ativo: headless e jogo têm cadência inicial diferente. |
| 14 | **Tempo do desfile**: presume-se tempo de jogo (acelera em 2x/4x); não confirmado. |
| 15 | **Mito**: no 1º tiro, anim ainda não carregada pode cair no modo timer; troca de nível no meio de uma rajada pode manter a cadência antiga até a próxima entrada no estado; `StartReturn` durante o chute não interrompe o chute. |
| 16 | **Coice da Nicole** move a malha placeholder oculta; provavelmente não aparece no corpo dela. |
| 17 | **Margem de troca de alvo das torres** é 1 cm de caminho (na prática troca por qualquer creep mais adiantado). Intenção desconhecida (talvez 1 célula). |
| 18 | **Comentários velhos**: "contagem congela" (BDGameBalanceSettings.h:81-85, AddVotesRed); "volta com a vida com que caiu" (BDCandidateSubsystem.h:43 — é a de saída); "palace level visual only" (o nível troca a arma). |
| 19 | **Número de bocas (6)** vem de log antigo e do PLANO; o .umap atual não foi lido. Não confirmado se o Esplanada tem GameMode Override. |
| 20 | **Pontos da curva `CF_HealthScaleByWave`** não foram lidos (está desligada). |
| 21 | **Slots de cada plataforma** (Palanque/Caminhão/Arquibancada) estão nos BPs e não foram extraídos; §6 diz 4/6/8. |
| 22 | **Propina na fila no fim da partida** é descartada: se a última onda mata um candidato e a apuração vem logo, a conversão em andamento se perde. Ordem exata não testada. |
| 23 | **Ordem de tick** entre os subsistemas de candidato, ministro e torres no mesmo frame não é fixa; a cobertura de 0,3 s deve absorver. |
| 24 | **Comício e corpo por número**: Candidato_02..20 já têm comício próprio, mas o corpo de todos ainda é o do `CandidateEnemy` (01). Quando a arte de cada um chegar, o spawn precisa passar a ler o corpo do DA do número. |

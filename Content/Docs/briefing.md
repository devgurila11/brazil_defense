# Briefing atual — Brazil Defense

**Versão: 2026-10-13 21:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Comício do candidato — primeira mecânica nova

Consultar o RAIOX antes de mexer (candidato §4, Mito §6, falas §10,
elenco §11) e atualizá-lo no fim. Vale para agendado e para o grupo do
endless; para o desfile de retorno, ver "Decisões abaixo".

## A sequência (regra fechada)

1. **Parada.** No trajeto, de tempos em tempos (ver números), o
   candidato para onde está. Antes de parar, um **tell** de 1,5 s
   (exclamação sobre ele + som curto) para o jogador entender o que vem.
2. **Discurso.** Enquanto discursa ele é **invencível** (torres,
   personagens e Mito não o miram; qualquer dano que chegue vira nulo,
   inclusive projétil já em voo) e **invisível para o Mito**: o Mito
   perde o alvo e volta a patrulhar como se o candidato não existisse.
   Áudio: só a fala do candidato toca; as demais falas (horda, Mito,
   Nicole) ficam mudas até ele terminar — efeitos (tiros, gritos de
   morte, bipe) continuam. O teto de falas vira 1, dele; volta a 2 no
   fim.
3. **Multidão.** No fim da fala **brota do chão** em volta dele a
   multidão: creeps comuns (sem proteção nenhuma), já roteados para a
   urna, contando voto como qualquer militante (+azul se morre,
   +vermelho se chega), com grito, corpo e placar de abates normais.
   Nascem nas células livres mais próximas dele (livre e alcançável;
   nunca plataforma, palácio, Blocked ou célula ocupada por outro
   nascido neste mesmo comício), espalhando pela rota para frente e
   para trás; se faltar célula, o que sobra brota na onda seguinte do
   anel. Animação de brotar: subir do chão em 0,6 s (placeholder:
   escala Z de 0 a 1).
4. **Retomada.** Ele volta a andar, vulnerável, e o Mito volta a
   persegui-lo pela regra de hoje (qualquer distância).

Repete até ele morrer ou chegar à urna (derrota, como hoje).

## Números (no DA do candidato, com defaults por N = ordinal 1..20)

- `RallyFirstDelay`: 6 s depois de sair do ônibus (não discursa na boca).
- `RallyEvery`: 20 s no N=1 caindo linearmente até 10 s no N=20;
  sorteio ±20 % a cada comício.
- `RallyTellSeconds` 1,5 · `RallyDuration` 4 s (ou a duração da fala,
  se houver cue; mínimo 2,5 s).
- `RallyCount` = 2 × N (N=1 brota 2; N=10 brota 20; N=20 brota 40).
- `RallyComposition`: lista de (DA de horda, peso). Default: tipos
  1..N com peso igual. Hoje só existe arte do tipo 1, então o
  commandlet/scaffold preenche todos com DA_Horda_01 e peso 1 — a
  estrutura fica pronta para os 20 tipos.
- `RallySpeech`: cue da fala do comício (separada de `SpeechSound` de
  andar e de `DeathSound`). Placeholder: exclamação + o som de fala
  que existir.
- `bRallies` true. Tudo isso também lido no spawn, gravado no registro
  do candidato como a vida.

Tempo de comício é tempo de jogo (acelera em 2x/4x, para na pausa).
Vida dos nascidos = vida da horda da onda atual (1,022^(W−1)), pelo DA
do tipo.

## Leitura visual (placeholders até a arte)

- Tell: exclamação sobre a cabeça (igual à de fala), vermelha.
- Discurso: **aura vermelha pulsando** no chão em volta dele (disco
  translúcido, 1,5 célula de raio, pulso 1 Hz) + ícone de megafone
  sobre ele. Tem que ser diferente da cúpula do ministro à primeira
  vista: ministro = cúpula azulada; comício = disco vermelho.
- HUD: aviso "COMÍCIO" 2 s na linha do candidato (mesma do "O
  CANDIDATO SAIU").
- Material novo `M_RallyAura` em Content/Elenco/Candidatos/_Shared.

## Código

- Lógica no `ABDCandidate` + um pequeno `UBDRallyComponent` (estados
  Walking → Tell → Speaking → Spawning → Walking), parâmetros no
  `UBDCandidateData` (criar se o candidato ainda lê só do EnemyData).
- Invencibilidade: reaproveitar o caminho do abrigo do ministro
  (`IsSheltered`/dano vira nulo), com flag própria `bRallyImmune`
  para o RAIOX e os checks distinguirem.
- Mito: ao entrar em Speaking, `AcquireTarget` ignora o candidato e
  `StartReturn`/patrulha retoma; ao sair, reengata pela regra atual.
- Falas: `CanSpeak` recebe um "dono exclusivo" enquanto há comício;
  dois candidatos discursando ao mesmo tempo (endless) → o segundo
  espera o primeiro terminar para iniciar o tell.
- Brotar: criar uma função genérica no WaveSubsystem **"spawn creep do
  DA X na célula C, roteado para a urna"** — a mistura de tipos nos
  ônibus (próximo briefing) vai usar a mesma função. Os nascidos entram
  na lista de inimigos da onda corrente (a onda não limpa com eles
  vivos) e NÃO contam no total "W × bocas".
- Save: já recusado com candidato em campo; nada a fazer.
- Console: `BD.Candidate.Rally` (força o comício agora),
  `BD.Candidate.Status` mostra estado do comício e próximo em N s.

## Decisões abaixo (padrão escolhido; o usuário confirma)

- Desfile de retorno: **não** faz comício (`bRallyOnReturn` false nas
  settings). Quem volta já é castigo; empilhar multidão em cima vira
  derrota garantida.
- Candidato discursando quando a onda limpa: a onda espera os nascidos
  como espera qualquer creep; o candidato segue não contando.
- Chegada à urna durante o tell: a chegada vence (derrota).

## Checks (regressão)

- Tell → Speaking → brota `RallyCount` → Walking, nos tempos do DA.
- Dano em Speaking vira nulo; torre/Nicole/Mito não o escolhem.
- Mito perseguindo → Speaking → Mito em patrulha; fim → persegue de novo.
- Durante Speaking só a fala do candidato pode começar; depois o teto
  volta a 2.
- Nascidos: todos em célula livre e alcançável, roteados, contam voto,
  não contam no total da onda; 40 em corredor estreito brotam sem
  sobrepor.
- `bRallyOnReturn` false: retornante não discursa.
- Regressão inteira antes dos checks novos; compilar os dois alvos;
  commit; §12; RAIOX (§4 candidato, §6 Mito, §10 falas, §11 elenco,
  tabela de settings) e seção nova no PLANO ("Comício") só com a regra.

## Entregável

- Comício funcionando com placeholders. Para ver na tela: onda 5,
  `BD.Candidate.Rally` força um comício; `BD.Candidate.Status` mostra
  o estado.

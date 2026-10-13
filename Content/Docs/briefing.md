# Briefing atual — Brazil Defense

**Versão: 2026-10-13 09:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Fase nova: elenco pronto na UE antes de preencher o jogo

Mecânicas ficam como estão. O trabalho agora é deixar TODOS os campos do
elenco criados e vazios, com nomes provisórios, para o usuário produzir
textura, MI, animação e som de cada um na pasta certa. O preenchimento
no jogo (quem sai em qual onda etc.) é fase posterior, não entra aqui.

Antes de começar: commitar o que está pronto com 102/102 (Mito sem
estamina chuta ministro, ministros passam separadores, console em `'` e
F10, Rodadas 8 e 9), com a entrada no §12. A pendência de GPU da
Rodada 9 fica em espera; não misturar com esta leva.

## Entidades e contagens

- Candidatos: 20 (`DA_Candidato_01..20`).
- Ministros: 11 (`DA_Ministro_01..11`).
- Personagens de plataforma e tipos de horda: contagem ainda não
  definida pelo usuário. Fazer a geração parametrizada (abaixo) e criar
  4 de cada como ponto de partida (`DA_Personagem_01..04`,
  `DA_Horda_01..04`); ele ajusta depois rodando o commandlet de novo.

## Commandlet de geração

`-run=BDScaffoldCast -Candidatos=20 -Ministros=11 -Personagens=4 -Hordas=4 [-DryRun]`

- Idempotente: cria o que falta, nunca sobrescreve asset que já existe
  nem apaga nada. Rodar de novo com número maior só acrescenta.
- Reaproveitar o que existe: `DA_Candidate`, `DA_Enemy_Test` (jumento e
  suas 4 skins), `DA_Shooter_Pistol` (Nicole) viram respectivamente
  `DA_Candidato_01`, `DA_Horda_01` e `DA_Personagem_01` por
  redirecionamento/renomeação da ferramenta do editor, com referências
  conferidas numa sessão nova. Os ministros não têm DA ainda — os 11
  nascem do zero com a classe de dados do ministro (criar `UBDMinisterData`
  se hoje o ministro lê só das Settings; o `ABDMinister` passa a receber
  o DA no spawn).
- Convenção fixa: o NOME DO ASSET nunca muda quando o oficial chegar.
  Só mudam `DisplayName` e as string tables. Anotar isso no PLANO.

## Campos por entidade (todos criados, vazios onde não houver)

Comum a todos: `DisplayName` (FText), `Icon` (abates/painel), malha
esquelética, esqueleto, `MeshMaterial` + `SkinMaterials`, `MeshScale`,
`MeshYaw`.

- **Horda**: anims Run, Death_01..04; sons SpeechSound (falas), CallSound
  (zurro), FootstepSound, DeathSound (grito), BodyFallSound; `KillType`,
  `KillIcon`; números de HP/velocidade como hoje.
- **Candidato**: anims Walk, Idle, Death; sons SpeechSound (falas enquanto
  anda) e **DeathSound (toca ao morrer, banco separado das falas)**;
  `CandidateFace` (já existe `CandidateFaces` nos settings — ligar ao DA).
- **Ministro**: anims Walk, Idle, Read (livro), Kicked (voo/queda),
  Flee; sons SpeechSound (falas) e **FleeSound (toca ao levar o chute,
  banco separado)**; `ScreamSound` atual vira o FleeSound. Ministro não
  tem DeathSound: não morre, foge.
- **Personagem**: como o `UBDShooterData` de hoje (corpo, Idle,
  Shooting, 6 armas, CelebrationSound), sem campo novo.

## Pastas espelhadas em Content/

`Content/Elenco/<Tipo>/<Nome>/{Mesh,Textures,Materials,Anim,Audio}` —
ex.: `Content/Elenco/Ministros/Ministro_01/Audio/`. Criar as pastas
vazias com um `.gitkeep` (ou arquivo marcador que o git aceite) para o
usuário só largar o asset no lugar. Os assets reaproveitados (jumento,
Nicole, candidato) são MOVIDOS para a pasta deles com a ferramenta do
editor, referências conferidas.

## String tables

Chaves `Candidate.01..20`, `Minister.01..11`, `Shooter.01..N`,
`Enemy.01..N` em `BD_pt` e `BD_en`, texto provisório igual ao nome do
asset ("Candidato 01" / "Candidate 01"). `BDLoc` resolve pelo nome do
asset, como já faz para `Enemy.<asset>`.

## `BD.Cast.Audit`

Lista cada DA do elenco e, por linha, quais slots estão VAZIOS (mesh,
material, cada anim, cada som, ícone). Resumo no fim: N DAs, N slots
vazios, por tipo. É o checklist de produção do usuário; precisa rodar
headless e no editor.

## Regras

- Nada de lógica de jogo nova: quem sai, quando, quantos — não mexer.
- Nenhum DA novo é apontado em settings de onda/candidato/ministro
  ainda; o jogo continua rodando com os de hoje (que agora têm nome
  novo). Regressão tem que seguir 102/102 antes de criar qualquer check.
- Checks novos: ELENCO — contagem de DAs por tipo bate com o commandlet;
  Audit acha todos; os 3 reaproveitados têm os slots de hoje
  preenchidos; o candidato tem DeathSound e o ministro tem FleeSound
  como campos distintos de SpeechSound.
- Compilar os dois alvos; commit; entrada no §12 e a convenção de nomes
  numa seção nova do PLANO ("Elenco").

## Entregável

- Commandlet rodado uma vez, pastas e DAs no lugar, `BD.Cast.Audit`
  mostrando a lista de slots vazios.
- Dizer no fim quantos slots vazios ficaram por tipo, para o usuário
  começar a produção.

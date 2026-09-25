# Briefing atual — Brazil Defense

**Versão: 2026-09-25 19:10**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Ajustes de UI/controle + dois diagnósticos

Coisas notadas jogando após o último push (89d375e). Alguns são para
fazer, outros são para o Claude do terminal INVESTIGAR e explicar
antes de mexer — marcados como DIAGNÓSTICO.

---

## 1. Nome do tipo no placar de abates

- Abaixo do ícone do jumento no placar lateral, mostrar o nome do
  tipo: "Militantes".
- Vem de um DisplayName no DataAsset do inimigo (UBDEnemyData), para
  cada tipo futuro trazer o seu nome. Preencher o do jumento com
  "Militantes".
- Abaixo do ícone do candidato (placeholder à direita), mostrar
  "Candidatos".
- Tudo FText (localizável), só na UI — não sobre o inimigo no jogo.

---

## 2. Botão de pause

- Adicionar um botão de PAUSE no HUD (e uma tecla, ex: P ou Esc) para
  o jogador parar o jogo e avaliar a estratégia com tudo congelado.
- Pausa tudo: ondas, movimento, animação, timers. O HUD continua
  visível e legível.
- Despausar retoma de onde parou. Não confundir com o menu de
  derrota/vitória — é pause de gameplay.
- Confirmar que o pause não quebra a colocação/preview (deve dar para
  olhar, e talvez planejar, com o jogo parado).

---

## 3. Rotação da urna não respeita o giro

- Ao posicionar a urna, girar com o clique da roda do mouse deveria
  mudar a direção que ela encara. Mas ao fixar, ela vira para uma
  direção pré-fixada, ignorando o giro do preview.
- É o mesmo tipo de bug de rotação já visto nas peças: o yaw do
  preview não está sendo aplicado ao ator no spawn. A urna
  provavelmente tem uma orientação forçada no código.
- Corrigir: o yaw escolhido no preview (clique da roda) vira o yaw da
  urna ao ser fixada.

---

## 4. DIAGNÓSTICO — ponto do grid vermelho para separador mesmo vazio

Existe um ponto onde, mesmo sem nada posicionado ali, o preview do
separador de fila fica vermelho (recusado).

Investigar e explicar ANTES de mexer:

- Separador é em ARESTA. Aquela aresta, se bloqueada, fecharia o único
  caminho da horda naquele trecho? Se sim, o vermelho é o
  WouldBlockPath recusando corretamente (não é bug).
- Ou é um ponto aberto, com caminho de sobra ao redor, que deveria
  aceitar e não aceita? Aí é bug.
- Logar o motivo da recusa naquele ponto (o EBDPlacementRefusal já
  existe) e dizer qual é.

Não consertar até saber se é a validação funcionando ou bug real.

---

## 5. DIAGNÓSTICO — votos azuis não batem com jumentos mortos

Números observados: votos AZUIS 772.000, VERMELHOS 18.000, jumentos
mortos 21.491. A conta "mortos menos passados" não bate com o placar.

772.000 / 21.491 = ~36 votos por jumento morto. Suspeita: o voto azul
é por HP (bBlueVotesByHealth), não por contagem de corpos — então o
placar de abates conta CORPOS e o placar de votos soma HP eliminado.
São métricas diferentes de propósito, e não batem por subtração.

Investigar e confirmar:

- O voto azul é por HP do inimigo morto, ou por contagem?
- O voto vermelho é por HP de quem passa? Candidato que passa dá o HP
  dele (enorme) ao vermelho?
- Matar candidato dá voto? (deve ser NÃO.) Conta como "morte" em
  algum contador?
- Se for tudo por HP: NÃO é bug, os números estão certos medindo
  coisas diferentes. Nesse caso, AVALIAR se vale deixar isso claro na
  UI — ex: o placar de abates rotulado como corpos, o de votos como
  "influência"/votos, para o jogador não estranhar a discrepância.
- Se NÃO for por HP e os números realmente não fecham, aí é bug de
  contagem — reportar onde.

Explicar a lógica atual antes de propor qualquer mudança.

---

## 6. DIAGNÓSTICO — consumo de memória crescente

Jogando, o consumo de memória parece crescer conforme as ondas
avançam (observado até a onda 150). Investigar antes de mexer:

1. A memória sobe durante a onda e VOLTA a cair quando os creeps
   morrem / o tabuleiro esvazia entre ondas? Ou só sobe e nunca desce?
   - Sobe e desce = normal (pico da onda, mais creeps vivos).
   - Só sobe, nunca desce = LEAK, sério numa partida de 150 ondas.
2. Se for leak: checar se o creep (jumento skeletal) libera TUDO ao
   morrer — a instância de animação, o material dinâmico, o physics
   asset (foi criado na importação, um corpo por creep), o ator em si.
3. O leak começou com o jumento skeletal ou já existia com os
   cilindros? (o skeletal animado é bem mais pesado; suspeito
   principal.)
4. Rodar simulação longa (ex: 50-100 ondas) medindo memória por onda.
   Se a curva só cresce sem platô, confirma o leak. Se estabiliza
   entre ondas, é normal.

Reportar: a memória estabiliza entre ondas ou cresce sem parar? E onde
vaza, se for o caso. Não consertar até saber.

Nota de contexto: o teto de onda sem evoluir nada está consistente em
~150 no Easy (dois testes seguidos). É a linha de base do
posicionamento puro — útil como referência, e também relevante aqui:
se a memória cresce a ponto de travar antes disso, pode estar
limitando o teto por vazamento, não por dificuldade.

## 7. Escalada de candidatos no endless (saem em grupo)

Hoje sai 1 candidato por onda de candidato (a cada 5 ondas). No
endless (depois dos 20 padrão), isso fica esparso e o fim de jogo
perde intensidade. Escalar quantos saem JUNTOS conforme o total de
candidatos MORTOS cresce:

- Até 20 candidatos mortos: 1 por vez (padrão atual).
- Passou de 20 mortos: 2 candidatos saem juntos.
- Passou de 40 mortos: 3 juntos.
- Passou de 60: 4 juntos. E assim por diante (+1 a cada 20 mortes).

Regra: NumCandidatosJuntos = 1 + floor(CandidatosMortos / 20).

- Eles saem JUNTOS (vários candidatos-chefe andando ao mesmo tempo),
  não é mudar a frequência — continua nas ondas de candidato, mas com
  mais de um por vez.
- Cada candidato do grupo sai de uma boca DIFERENTE (não amontoar no
  mesmo respawn). Sortear bocas distintas para eles — atacam por
  frentes diferentes ao mesmo tempo, obrigando a dividir a defesa.
  Se houver menos bocas ativas que candidatos, distribuir o mais
  espalhado possível (no máximo repetir boca quando não houver
  alternativa).
- Isso torna o endless progressivamente brutal, de propósito — é o
  que faz o endless ser "até onde você aguenta", nunca vencível.
- Interage com o retorno dos caídos (quando o vermelho vira, os
  mortos voltam): aceito que isso possa virar uma parede de
  candidatos no endless profundo. É intencional.
- Expor o passo (20 mortes por +1 candidato) em settings.

Confirmar que a barra de candidatos no HUD (a lista) aguenta vários
vivos ao mesmo tempo — ela já foi feita para isso, mas com 4-5 juntos
mais o retorno, testar o "+N a caminho".

## Entregável

- "Militantes" e "Candidatos" abaixo dos ícones no placar.
- Botão de pause funcional (+ tecla).
- Rotação da urna respeitando o giro do preview.
- Diagnóstico do ponto vermelho: dizer se é validação correta ou bug.
- Diagnóstico dos votos vs abates: explicar a lógica (HP vs contagem)
  e dizer se é comportamento correto ou bug.
- Diagnóstico de memória: dizer se estabiliza entre ondas ou vaza, e
  onde vaza se for o caso.
- Escalada de candidatos: +1 candidato junto a cada 20 mortos, saindo
  em grupo no endless.
- Os itens 1-3 são para fazer; os 4-6 são para investigar e explicar
  primeiro (só corrigir se for bug de verdade).
- BD.Test.Regression passa; compilar os dois alvos.

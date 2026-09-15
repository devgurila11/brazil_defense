# Briefing atual — Brazil Defense

**Versão: 2026-09-15 15:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# AutoSetup precisa JOGAR, não despejar

Descoberta que invalida as métricas anteriores: o AutoSetup espalha
peças aleatoriamente, sem estratégia. Não faz labirinto, não alonga
a rota, não isola a urna, não concentra defesa em gargalo. Toda a
calibração de hoje mediu uma defesa burra — os números não descrevem
o jogo real.

A estratégia central do jogo é a do Clash of Clans: o LABIRINTO
(organizadores de fila / cercas) é a defesa primária — alonga a rota
para a horda sofrer dano o caminho todo. As torres e personagens são
a secundária, posicionados ao longo do corredor que o labirinto cria.

O AutoSetup precisa jogar assim, para as métricas valerem.

---

## O que o AutoSetup estratégico deve fazer, em ordem

### 1. Construir o labirinto PRIMEIRO

Antes de qualquer torre, gastar cercas para alongar a rota das bocas
até a urna.

- Isolar a urna: cercar em volta dela deixando só uma entrada
  estreita (como o muro protege o CV no Clash), forçando a horda a
  contornar.
- Criar serpentina: em vez de deixar a rota curta, construir cercas
  que forcem zigue-zague, maximizando o comprimento do caminho.
- Respeitar a validação que já existe: NUNCA fechar completamente
  (WouldBlockPathEdges recusa). Sempre deixar caminho.
- Medir o ganho: a rota depois do labirinto deve ser
  significativamente mais longa que a rota direta. Logar o
  comprimento antes/depois.

### 2. Posicionar defesa NO CORREDOR, não espalhada

Com o labirinto feito, o caminho da horda é conhecido e longo.

- Concentrar torres e plataformas ao LONGO desse corredor, onde os
  creeps passam — não em células aleatórias longe da rota.
- Priorizar os pontos por onde a horda passa MAIS de uma vez (curvas
  da serpentina, onde a rota dobra sobre si).
- Cobrir todas as bocas ativas: nenhuma rota de spawn pode ficar sem
  defensor ao alcance.
- Concentrar perto da urna: a última linha de defesa, para o candidato
  que chega longe na rota.

### 3. Só então gastar o resto

Sobrou orçamento/votos depois do labirinto e da cobertura do corredor?
Reforçar os gargalos e a zona da urna.

---

## Parâmetros de estratégia (não hardcodar)

Expor para ajuste, para simular jogadores de níveis diferentes:

- Fração do orçamento gasta em cercas antes das torres (ALVO: labirinto
  primeiro, ex: 40% em cercas).
- Densidade do labirinto (quão sinuosa a serpentina).
- Se concentra na urna, distribui no corredor, ou mistura.

Isso também dá o "perfil de jogador" que faltava: um AutoSetup que
faz labirinto denso e concentra bem = jogador bom; um que faz pouco
labirinto = jogador médio. Assim dá para medir a faixa real de
dificuldade.

---

## Depois: remedir tudo

Com o AutoSetup jogando de verdade, rodar a bateria de novo e comparar
com os números de hoje. Provavelmente muda:

- A onda de quebra (labirinto = horda sofre mais = defesa aguenta mais,
  ou horda tão atrasada que nem chega).
- O candidato (rota longa = mais tempo sob fogo = mais fácil de matar
  antes da urna). Isso pode resolver sozinho o gargalo do candidato.
- O overkill (defesa concentrada no corredor mira melhor).

Reportar as 5 métricas de novo, defesa estratégica, e comparar lado a
lado com a defesa-despejo de hoje. NÃO recalibrar curvas ainda — medir
primeiro com a defesa que joga certo, porque tudo pode mudar.

---

## Nota sobre os DA\_ de dificuldade

O usuário observou que faltam organizadores de fila para proteger a
urna. O orçamento de cercas por dificuldade é ajustável nos
DA_Difficulty (DividerBudget). Se depois de o AutoSetup fazer
labirinto ainda faltar cerca para isolar a urna, aumentar o
DividerBudget — mas medir primeiro.

# Briefing atual — Brazil Defense

**Versão: 2026-09-25 15:45**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Ônibus (SM_Bus) acompanha a boca de spawn

Estado atual (confirmado): a âncora e o wander das bocas funcionam
(cada boca desliza até 5 células/35m por onda, sem acumular). Mas os
6 SM_Bus são StaticMeshActor parados sobre as âncoras — o código não
os conhece. Resultado: a boca desliza e o ônibus fica parado, então a
horda sai a até 35m do ônibus, quebrando a leitura de "vêm da
caravana".

Decisão: o ônibus SEGUE a boca. Manter o wander (a variação de saída
é desejada — dá imprevisibilidade). Só fazer o ônibus acompanhar.

## Ligar cada ônibus à sua boca

Os 6 ônibus já estão exatamente sobre as âncoras (confirmado):

- SM_Bus6 → âncora (6,0)
- SM_Bus5 → âncora (22,0)
- SM_Bus → âncora (0,5)
- SM_Bus2 → âncora (0,17)
- SM_Bus3 → âncora (10,21)
- SM_Bus4 → âncora (26,21)

Ligar cada ônibus à sua boca por referência (não por proximidade em
runtime — resolver a associação uma vez, pela âncora, e guardar).
Pode ser: converter os StaticMeshActor num ator do jogo (ABDBus) com
a boca que ele serve, OU um registro no código que mapeia âncora →
ônibus. O que for mais limpo.

## Movimento

- Quando a boca desliza (WanderSpawnPoints, entre ondas), o ônibus
  correspondente se move JUNTO, para a nova posição da boca.
- Movimento SUAVE entre ondas (interpolar em alguns segundos), não
  teleporte. O jogador vê o ônibus se reposicionando antes da onda —
  vira aviso visual de onde a horda vem desta vez.
- O ônibus translada no PRÓPRIO EIXO da borda (frente/ré), NUNCA gira.
  Aponta sempre para dentro da arena (como já está colocado).
- Respeita a mesma âncora e limite de 5 células que a boca já respeita
  — ônibus e boca nunca se separam.
- A boca continua sendo o ponto lógico de spawn; o ônibus só
  acompanha visualmente, ficando com a saída da horda sempre nele.

## Anti-colisão entre ônibus da mesma borda

Cada lado do terreno retangular tem DOIS ônibus (3 lados usados, 6
ônibus no total). Dois ônibus da mesma borda não podem se sobrepor.

Pela geometria atual não colidem (âncoras a 16, 16 e 12 células;
movimento máx 5 de cada lado = 10 células de alcance somado). Mas a
borda esquerda (âncoras 0,5 e 0,17, distância 12) tem só 2 células de
folga — apertado.

Adicionar trava de segurança: um ônibus não desliza a ponto de chegar
a menos de MinBusGap células (ex: 2) do ônibus vizinho da mesma borda.
Se o wander da boca fosse levar além disso, o ônibus (e a boca) param
antes desse limite. Assim nunca há sobreposição, mesmo que as âncoras
sejam reposicionadas no futuro.

Expor MinBusGap em settings.

## Manter

- O wander das bocas (MouthWanderChance 0.75, MaxCells 5) fica como
  está — a variação é a mecânica desejada.
- Tudo mais: spawn, rota, candidato, etc.

## Sons — NÃO nesta leva

Motor, buzina e som de saída da horda ficam para depois. Só deixar
claro no código onde esses eventos entrariam (ônibus começa a mover /
onda começa a sair), para facilitar plugar os sons na próxima leva.

## Entregável

- Cada SM_Bus ligado à sua boca; ao deslizar a boca, o ônibus vai
  junto, suave, sem girar.
- A horda sempre sai de dentro/ao lado do ônibus, nunca a 35m dele.
- Wander mantido (variação preservada).
- Dois ônibus da mesma borda nunca se sobrepõem (trava MinBusGap).
- BD.Test.Regression passa; compilar os dois alvos.

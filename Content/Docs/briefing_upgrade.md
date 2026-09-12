# Briefing — sistema de upgrade de defensor

## Contexto

Brazil Defense, UE 5.8.1, C++, prefixo BD.
Defensores atirando e matando, votos azuis e vermelhos contando,
autossetup montando cenário completo.

Com 20 defensores de nível 1 a defesa quebra entre a onda 5 e a 10 —
porque o dano não escala e o HP sim. Este briefing resolve isso.

## Curvas

```
Custo do nível N = UpgradeCostBase * 1.55^(N-1)
Dano do nível N  = Damage * (1 + 0.40 * (N-1))
Máximo 10 níveis.
```

Expor `UpgradeCostGrowth` (1.55) e `DamageGrowthPerLevel` (0.40) em
`UBDGameBalanceSettings`. O resto vem do `FBDTowerLevel` que já existe
no `UBDTowerData`.

Motivo das duas curvas: custo exponencial contra dano linear faz
empilhar a mesma torre ficar caro por conta própria, e diversificar
vira a jogada certa sem proibir nada.

## Moeda — modelo cruel

O upgrade sai dos **votos azuis**, derrubando o placar. Não há carteira
separada: gastar enfraquece a posição eleitoral e fortalece a defesa.
É a decisão central do jogo.

Recusar o upgrade se o jogador não tiver votos suficientes.

## Aviso antes de confirmar

Expor, para o HUD futuro e para o log agora:

```cpp
int32 GetUpgradeCost(ABDTowerBase*) const;
bool WouldInvertScoreboard(int32 Cost) const;
```

Se o gasto for fazer o vermelho ultrapassar o azul, sinalizar.
O jogador ainda pode fazer — mas fazendo sabendo. Sem isso ele clica,
o candidato surge, e parece bug.

## Interface

Selecionar um defensor já posicionado (clique esquerdo sobre ele fora
do modo de colocação) e confirmar o upgrade.

Console:
- `BD.Tower.Upgrade <x> <y>`
- `BD.Tower.UpgradeAll` (debug)

## Autossetup

Parâmetro de nível inicial dos defensores:

```
BD.Debug.AutoSetup.Level <N>
```

Monta a defesa toda no nível N. Assim dá para comparar "defesa nível 1"
contra "nível 3" e ver onde cada uma quebra.

## Entregável

Compilar limpo nos dois targets. Rodar o teste comparativo de ondas
1/5/10/20/30 com defesa nível 1, 3 e 5, e reportar em que onda cada
uma quebra.

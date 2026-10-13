# Briefing atual — Brazil Defense

**Versão: 2026-10-14 18:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Leva de ajustes + suíte de equilíbrio

Quatro itens, independentes; um commit por item. Consultar o RAIOX
antes de cada um e corrigir as linhas no fim. Regressão inteira antes e
depois de cada item; dois alvos; §12.

## 1. Chegada só pela frente da urna (bug)

Sintoma em jogo: separadores no entorno da urna deixando um corredor
só, e hordas e candidato chegam por trás e votam. Pista do RAIOX: a
chegada vale a ≤150 cm do ator, e a "perna final" vai da última célula
da rota até o ator, não até o centro da célula — um creep na célula de
trás fecha a distância sem atravessar a aresta onde está o separador.

Regra: a urna tem uma **face de entrada** (a do "teclado", definida no
BP/DA pela rotação do ator; o jogador pode girar ao colocar, se já
houver giro de peça, senão fixa). A perna final só é válida pela aresta
dessa face: o pathfinding trata a célula da urna como alcançável
apenas por essa aresta, e a chegada exige ter cruzado ela. Aresta de
entrada com separador = urna sem rota (recusa de colocação como já
acontece quando se fecha o caminho).

Checks: urna cercada com um corredor → nenhum creep chega por outra
aresta; separador na face de entrada é recusado; `WouldBlockPath`
continua válido; candidato segue a mesma regra.

## 2. Falas −25 % e sem repetição

- Horda: alvo 0,2 → 0,15 fala/s; vocalização por creep 8–20 s →
  11–27 s. Mito e Nicole: comemoração 9 % → 7 %, intervalo mínimo por
  falante 15 → 20 s. Teto global de 2 simultâneas fica.
- **Sorteio sem reposição** em todo banco de falas (horda, Mito,
  Nicole, candidato): a mesma fala não volta até todas as outras do
  banco terem tocado; embaralha de novo ao esvaziar, sem repetir a
  última. Banco com 1 fala toca sempre ela.
- Números em settings (já existem), não hardcoded.

Checks: 1000 sorteios num banco de 5 → nenhuma fala repete antes das
outras 4; taxas novas lidas das settings.

## 3. Mito: dano ×2 contra horda

`HordeDamageMultiplier` 2,0 no `UBDPalaceData`, aplicado ao dano da
arma contra militante (qualquer tipo de horda). Contra candidato fica
1,0. Vale para as 6 armas (placeholders herdam). Bônus de patrulha por
abate não muda.

Check: tiro em militante = 20 no nível 0; em candidato = 10.

## 4. Suíte de equilíbrio (`BD.Balance.Suite`)

Objetivo: nunca mais calcular na mão se uma peça, ator, poder ou magia
nova desequilibra. Nada é calibrado nesta leva — só medir e relatar.

### Telemetria nova
- WaveLog ganha, por onda e por tipo de peça (torre, Nicole/personagem,
  palanque, caminhão, arquibancada, palácio/Mito): quantidade no grid,
  dinheiro acumulado gasto (compra + evolução), dano causado, abates,
  dano desperdiçado, tempo ocioso (sem alvo no alcance) em %.
- PostMatch ganha, por tipo: dano por 100 de dinheiro, % dos abates,
  nível médio; lista de peças desbloqueadas e nunca compradas; votos
  vermelhos por candidato (quantos passaram enquanto cada um esteve
  vivo); comícios e multidão total por candidato.

### Perfis de simulação
`BD.Balance.Suite [seed]` roda, na mesma seed, com o AutoSetup/Sim
atual restrito a cada perfil, até a 100 ou derrota, em 4x headless:
- `tower` — só torres.
- `stage` — só palanque + personagem.
- `truck` — só caminhão + personagem (a partir da 10).
- `mixed` — misto, como o AutoSetup faz hoje.
- cada um com e sem palácio (`+palace`).
Divisórias liberadas em todos. Os perfis são uma lista em settings para
crescer depois (magias, novas peças).

### Saída
Uma tabela por rodada, gravada em `Saved/Balance/suite_<data>.md` e
colada no PLANO §13 pelo terminal:

| Perfil | Onda alcançada | Azul × Vermelho | Dano/100$ por tipo | % abates por tipo | Nunca comprado | Vermelhos por candidato |

Mais uma linha de **limiares** (ficam no PLANO, seção nova "Equilíbrio",
valores iniciais abaixo — o usuário ajusta):
- L1: nenhum tipo rende mais que 1,5× o dano/100$ de outro do mesmo
  papel (torre × personagem; palanque × caminhão × arquibancada).
- L2: nenhum perfil puro vence a 100 com vermelho < 10 % do azul.
- L3: todo tipo desbloqueado aparece em pelo menos um perfil vencedor.
- L4: nenhum tipo com ociosidade média > 50 % no perfil misto.
O relatório marca cada limiar como OK ou ESTOURADO com os números.

### Regra de processo (escrever no PLANO "Equilíbrio")
Todo briefing que cria ou altera peça, ator, poder, magia ou número de
economia termina com `BD.Balance.Suite` rodada, tabela no §13 e os
limiares estourados listados na resposta do terminal. Não corrigir
sozinho: só apontar.

### Entregável do item 4
A primeira rodada da suíte no estado atual, com a tabela e os limiares.
Expectativa: L1 estoura entre Nicole e torre (≈6,3 contra 3,0).

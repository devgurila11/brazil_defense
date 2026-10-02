# Briefing atual — Brazil Defense

**Versão: 2026-10-02 10:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Ajustes do Agente e economia + debug de tiro intermitente

## 1. Preço do palácio: 3000

- PalaceCost de 1800 para 3000. Assim não dá para comprar antes da
  onda 1 (fundos iniciais 2000) — o Agente vira conquista de começo,
  o jogador junta alguns candidatos antes. Preço continua FIXO (regra
  de ouro), só esse valor muda.

## 2. Taxa de mover peça: continua subindo com a onda (confirmado)

- Mover NÃO é construir — é refazer o que foi mal planejado, e deve
  custar. Manter a taxa de movimentação crescente como está. Isso é
  exceção consciente à regra de ouro: construir é fixo, mas MOVER
  (corrigir posição) fica mais caro com a onda, para premiar boa
  estratégia desde o início. Deixar como está.

## 3. Bônus por abate e armas do Agente (confirmado)

- Bônus por abate caindo 0,15s por nível (1,00 / 0,85 / 0,70 / 0,55 /
  0,40 / 0,25) e a lista de 6 armas (níveis 0-5): confirmados, manter.

## 4. Debug de tiro do Agente — intermitente, cor própria

O tiro do Agente deve ter line trace de DEBUG para ver a mecânica da
ação, mas INTERMITENTE, não contínuo:

- Cada disparo da pistola desenha um risco que APARECE E SOME (pisca
  no instante do tiro, dura uma fração de segundo), NÃO uma linha fixa
  ligando Agente e alvo o tempo todo.
- Motivo: é DISPARO, não laser/rajada. Linha fixa pareceria arma
  contínua. O pisca-pisca mostra tiros individuais, no ritmo da
  cadência (Glock, 1,5/s).
- COR PRÓPRIA, diferente do debug das torres (que mostram alcance/alvo
  em outra cor), para distinguir o tiro do Agente.
- Ligável por cvar (ex: BD.Agent.ShowShots), como os outros debugs.
- Nota para o futuro: quando entrarem armas CONTÍNUAS (laser, rajada),
  o debug delas SIM será uma linha contínua — a diferença de debug
  (pisca vs contínuo) ajuda a distinguir disparo de feixe. Por ora
  todas as defesas são disparo, então todos os debugs de tiro piscam.

## 5. Som de disparo — estrutura pronta, placeholder por enquanto

Deixar o sistema de som de tiro PRONTO, mas sem arquivo ainda (os sons
reais vêm depois do teste do Agente).

- Estrutura reutilizável: um Cue de disparo que QUALQUER defensor que
  atira possa usar (Agente agora; torres e atiradores de plataforma
  depois), trocando só o conjunto de clipes por arma.
- Disparado no instante de cada tiro, sincronizado com a cadência
  (Glock) e com a animação Shooting / o debug intermitente.
- 3D na posição do atirador, atenuação por câmera, SC_Effects.
- Concurrency (teto cortando o mais distante), como os outros sons —
  numa cena com várias defesas atirando, não virar parede.
- SLOT vazio por enquanto (sem som). Quando o usuário trouxer 2-3
  variações de disparo de pistola, é só apontar no Cue. Deixar o
  caminho/campo pronto e documentado onde plugar.
- NÃO é partícula — partículas (muzzle flash, projétil, impacto) ficam
  para o futuro, depois do teste. Agora só a estrutura de som +
  o debug intermitente que já existe.

## NÃO fazer agora
- Ministros, garrafa (fatias 3 e 4).

## Entregável
- Palácio 3000 (fixo).
- Taxa de mover mantida (crescente).
- Bônus/armas confirmados.
- Debug de tiro do Agente: piscadas por disparo, cor própria, por cvar.
- Sistema de som de disparo pronto e reutilizável, com slot vazio
  (placeholder) para plugar o som depois.
- BD.Test.Regression passa; compilar os dois alvos; commit.

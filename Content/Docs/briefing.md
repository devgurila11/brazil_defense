# Briefing atual — Brazil Defense

**Versão: 2026-09-26 15:15**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Ligar o som de tombo + renomear os assets do jumento

Duas coisas, ambas de higiene/acabamento do jumento.

---

## 1. Som de tombo (baque do corpo no chão)

Os 4 sons S_Jumento_Tombo_01 a \_04 já existem mas não estão ligados a
nada. Ligar como o BAQUE do corpo caindo no chão na animação de morte.

- Disparar quando o corpo morto TOCA o chão na animação de queda (o
  momento do impacto — via Anim Notify na animação de morte, ou no
  momento em que o corpo começa a afundar, o que for mais simples).
- Sortear entre os 4 (Random), leve variação de pitch.
- 3D na posição do corpo, atenuação por câmera, SC_Effects.
- Concurrency próprio (ex: 5, cortando o mais distante) — numa onda
  que limpa 189 de uma vez, não deixar 189 baques soarem juntos.
- Fecha a morte: o militante zurra ao morrer (som de morte), cai
  (animação), e faz o baque no chão (tombo). Três camadas dando peso
  ao abate.

---

## 2. Renomear esqueleto / malha / physics do jumento

Vieram do Mixamo com nome ruim (Run*Forward\_\_1*\*) e ficaram assim.
Confunde — parece animação, não o personagem. Renomear no padrão do
projeto:

- Skeletal Mesh: SKM_Jumento (ou SK_Jumento — o padrão que o projeto
  já usa para skeletal).
- Skeleton: SKEL_Jumento (ou o padrão do projeto).
- Physics Asset: PHYS_Jumento.
- Qualquer outro asset residual do Mixamo com nome ruim na pasta do
  jumento: renomear no padrão.

CUIDADO: esqueleto e malha têm referências (o DataAsset aponta eles,
as animações usam o esqueleto). Renomear pelo editor/ferramenta que
ATUALIZA as referências, não quebrando os apontamentos. Confirmar
depois que:

- O DA_Enemy_Test ainda aponta o skeletal certo.
- As animações (A_Jumento_Run, A_Jumento_Death_01..04) ainda usam o
  esqueleto.
- A regressão passa (a checagem do jumento continua achando o mesh).

---

## Entregável

- Tombo ligado ao impacto do corpo no chão, sorteado, com concurrency.
- Esqueleto/malha/physics renomeados no padrão, sem quebrar
  referências.
- DA_Enemy_Test e animações ainda apontando certo.
- BD.Test.Regression passa; compilar os dois alvos.
- Commit + push (inclui a renomeação e o tombo).

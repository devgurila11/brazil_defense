# Briefing atual — Brazil Defense

**Versão: 2026-09-23 23:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Fechamento do dia: confirmar regra e commitar

A leva de infraestrutura (build visível, BD.Test.Regression, separador
inicial, rotação no botão do meio) está feita e passou 21/21 no
regression. Faltam duas coisas para fechar o dia.

---

## 1. Construção durante a onda: manter BLOQUEADO (decisão confirmada)

Você alinhou para nenhuma peça nova entrar durante a onda (torre e
personagem também bloqueados, não só divisória e plataforma). Está
CONFIRMADO como a regra certa:

- Construir qualquer peça = só na fase de montagem / entre ondas.
- Evoluir = livre durante a onda (reação tática).
- Não voltar atrás. É coerente: construir é decisão de montagem,
  evoluir é reação.

Garantir que o BD.Test.Regression tenha uma invariante que trave isso
("nenhuma construção aceita durante onda ativa; evolução aceita"),
para não regredir no futuro.

---

## 2. Commit e push

Fechar tudo que está pendente desde o último commit (06078e4):

- Separador com cota própria
- Barra do candidato só após dano
- Evolução livre durante a onda
- Personagem recusado sem slot livre
- PostMatch.csv (relatório de fim de partida)
- Build visível (menu/HUD/log/CSV)
- BD.Test.Regression
- Separador inicial dobrado (DA_Difficulty)
- Rotação no botão do meio

Mensagem de commit que resuma. Atualizar PLANO §12 com a entrada
desta leva (data, o que mudou, o que afeta) e trocar o "COMMIT" pelo
hash, como nos pushes anteriores. Confirmar o hash no fim.

---

## Entregável

- Invariante de "sem construção durante a onda" no regression.
- Commit + push com tudo pendente; PLANO §12 atualizado com o hash.
- Confirmar: regression ainda passa tudo depois do commit.

# Briefing atual — Brazil Defense

**Versão: 2026-10-02 17:15**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Liberar uso de recursos a qualquer momento

Mudança de filosofia (confirmada): os recursos são do jogador, ele usa
QUANDO QUISER, a qualquer momento. O único limite é TER o recurso
(dinheiro público / cota de separador / slot livre). Nunca a fase.

Motivo: travar construção durante a onda frustra o estrategista que
tem recurso na mão e perde por estar TRAVADO, não por estratégia ruim
(ex: voltou do banheiro, onda começou, tem dinheiro mas não pode
agir). Perder por não planejar é justo; perder por o jogo não deixar
agir com recurso na mão é raiva.

## O que liberar durante a onda (e em qualquer fase)
- Construir qualquer peça: torre, plataforma, personagem, cerca,
  palácio.
- Posicionar personagem em slot.
- Evoluir (já era livre — confirmar que segue).
- Mover/reposicionar peça (com a taxa de movimentação que já existe).
- Vender.

Tudo isso passa a ser permitido SEMPRE — montagem ou onda ativa — desde
que haja recurso. Se não há recurso, recusa com o motivo que já existe
(NoVotes / sem dinheiro / sem cota / sem slot), não "fase errada".

## O que NÃO muda
- A URNA continua travada depois que a onda 1 começa (a urna fixa é
  regra de design, não é "recurso" — fica como está).
- Os CUSTOS não mudam (regra de ouro: construir é fixo, evoluir sobe).
- Validações de posição continuam (WouldBlockPath, slot ocupado, etc.).

## Ajustar o regression
- Havia uma invariante "nenhuma construção aceita durante onda ativa".
  Ela agora é INVÁLIDA. Trocar por "construção aceita durante a onda se
  houver recurso" (o oposto), para o termômetro refletir a regra nova.
- As outras invariantes de colocação seguem (recusa sem recurso,
  sem slot, urna travada após onda 1).

## Entregável
- Construir/posicionar/evoluir/mover/vender liberados em qualquer fase,
  limitados só por recurso.
- Urna segue travada após onda 1.
- Invariante do regression trocada para a regra nova; passa tudo.
- Compilar os dois alvos; commit.
